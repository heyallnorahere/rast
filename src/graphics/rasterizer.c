#include "rasterizer.h"

#include "core/mem.h"
#include "core/thread_worker.h"
#include "core/semaphore.h"
#include "math/vec.h"
#include "math/geo.h"
#include "graphics/image.h"
#include "debug/capture.h"

#include <math.h>
#include <string.h>

struct rasterizer {
    thread_worker_t* worker;
    uint32_t num_scanlines;
    capture_t* current_capture;
};

struct vertex_output {
    void* working_data;
    float position[4];
};

struct render_context {
    const struct pipeline* pipeline;
    struct framebuffer* fb;

    struct vertex_output* outputs;
    uint8_t vertices;

    uint32_t instance_id;
    void* uniform_data;

    semaphore_t* semaphore;
};

struct blend_context {
    float src_alpha, dst_alpha;
};

static image_pixel image_get_pixel(const image_t* image, uint32_t x, uint32_t y) {
    uint32_t index = image_get_pixel_index(image, x, y);

    size_t offset = image->pixel_stride * index;
    void* pixel_address = image->data + offset;

    image_pixel pixel;
    memcpy(&pixel, pixel_address, image->pixel_stride);

    return pixel;
}

static void image_set_pixel(image_t* image, uint32_t x, uint32_t y, const image_pixel* value) {
    uint32_t index = image_get_pixel_index(image, x, y);

    size_t offset = image->pixel_stride * index;
    void* pixel_address = image->data + offset;

    memcpy(pixel_address, value, image->pixel_stride);
}

static void framebuffer_fill_attachment(image_t* attachment, const image_pixel* value) {
    for (uint32_t y = 0; y < attachment->height; y++) {
        for (uint32_t x = 0; x < attachment->width; x++) {
            image_set_pixel(attachment, x, y, value);
        }
    }
}

void framebuffer_clear(rasterizer_t* rast, struct framebuffer* fb,
                       const image_pixel* clear_values) {
    for (uint32_t i = 0; i < fb->attachment_count; i++) {
        image_t* attachment = fb->attachments[i];
        framebuffer_fill_attachment(attachment, &clear_values[i]);
    }

    if (rast->current_capture) {
        capture_add_framebuffer_clear(rast->current_capture, fb, clear_values);
    }
}

static void process_face_vertices(const struct indexed_render_call* data, uint32_t instance,
                                  uint32_t face, uint8_t indices, struct vertex_output* outputs,
                                  struct captured_primitive* captured) {
    struct shader_context context;
    context.instance_index = instance;
    context.uniform_data = data->uniform_data;
    context.working_data = NULL;

    size_t working_size = data->pipeline->shader.working_size;
    if (captured) {
        captured->indices = mem_alloc(sizeof(uint32_t) * indices);
        captured->working_data = mem_alloc(working_size * indices);
        captured->vertex_positions = mem_alloc(sizeof(float) * 4 * indices);
        captured->instance_index = instance;
    }

    const void* vertex_data[data->pipeline->binding_count];
    for (uint8_t i = 0; i < indices; i++) {
        uint32_t index_id = data->first_index + face * indices + i;
        context.vertex_index = data->vertex_offset + data->indices[index_id];

        for (uint32_t j = 0; j < data->pipeline->binding_count; j++) {
            const struct vertex_binding* binding = &data->pipeline->bindings[j];
            const struct vertex_buffer* vbuf = &data->vertices[j];

            uint32_t buffer_index;
            switch (binding->input_rate) {
            case VERTEX_INPUT_RATE_VERTEX:
                buffer_index = context.vertex_index;
                break;
            case VERTEX_INPUT_RATE_INSTANCE:
                buffer_index = instance;
                break;
            }

            size_t offset = buffer_index * binding->stride;
            vertex_data[j] = vbuf->data + offset;
        }

        struct vertex_output* current_output = &outputs[i];
        memset(current_output->position, 0, 4 * sizeof(float));
        current_output->position[3] = 1.f;

        context.working_data = current_output->working_data;
        data->pipeline->shader.vertex_stage(vertex_data, &context, current_output->position);

        if (captured) {
            captured->indices[i] = context.vertex_index;

            memcpy(captured->working_data + i * working_size, context.working_data, working_size);
            memcpy(captured->vertex_positions + i * 4, current_output->position, 4 * sizeof(float));
        }
    }
}

static float signed_quad_area(const float* a, const float* b, const float* c, bool cw) {
    float ab[2], ab_normal[2];
    vec_sub(b, a, 2, ab);
    vec_rot90(ab, ab_normal, cw);

    float ac[2];
    vec_sub(c, a, 2, ac);

    return vec_dot(ac, ab_normal, 2);
}

static bool face_contains_point(bool cw, bool cull_back, const struct vertex_output* outputs,
                                uint8_t vertices, const float* point, float* weights) {
    float area_sum = 0.f;
    float areas[vertices];

    for (uint8_t i = 0; i < vertices; i++) {
        const float* a = outputs[i].position;

        uint8_t next_vertex_id = (i + 1) % vertices;
        const float* b = outputs[next_vertex_id].position;

        float area = signed_quad_area(a, b, point, cw);
        if (cull_back && area <= 0.f) {
            return false;
        }

        area_sum += area;
        areas[(i + 2) % vertices] = area;
    }

    if (area_sum <= 0) {
        area_sum *= -1.f;
    }

    bool first_out = areas[0] <= 0.f;
    for (uint8_t i = 0; i < vertices; i++) {
        if (weights) {
            weights[i] = areas[i] / area_sum;
        }

        if (i > 0 && !cull_back) {
            bool current_out = areas[i] <= 0.f;
            if (first_out != current_out) {
                return false;
            }
        }
    }

    return true;
}

static size_t parameter_element_stride(element_type type) {
    switch (type) {
    case ELEMENT_TYPE_BYTE:
        return 1;
        break;
    case ELEMENT_TYPE_FLOAT:
        return sizeof(float);
        break;
    }
}

static void shader_blend_parameters(const struct shader* shader,
                                    const struct vertex_output* outputs, uint8_t indices,
                                    const float* weights, float current_depth, void* result) {
    for (uint32_t i = 0; i < shader->inter_stage_parameter_count; i++) {
        const struct blended_parameter* parameter = &shader->inter_stage_parameters[i];

        size_t stride = parameter_element_stride(parameter->type);
        for (uint32_t j = 0; j < parameter->count; j++) {
            size_t buffer_offset = parameter->offset + j * stride;

            float result_value = 0.f;
            for (uint8_t k = 0; k < indices; k++) {
                const void* source_data = outputs[k].working_data + buffer_offset;

                float vertex_value;
                switch (parameter->type) {
                case ELEMENT_TYPE_BYTE:
                    vertex_value = (float)*(uint8_t*)source_data;
                    break;
                case ELEMENT_TYPE_FLOAT:
                    vertex_value = *(float*)source_data;
                    break;
                }

                float weight = weights[k];
                float depth = outputs[k].position[2];

                result_value += vertex_value * weight / depth;
            }

            result_value *= current_depth;

            void* destination_data = result + buffer_offset;
            switch (parameter->type) {
            case ELEMENT_TYPE_BYTE:
                *(uint8_t*)destination_data = (uint8_t)result_value;
                break;
            case ELEMENT_TYPE_FLOAT:
                *(float*)destination_data = result_value;
                break;
            }
        }
    }
}

static bool pre_fragment_tests(const struct pipeline* pipeline, struct framebuffer* fb, uint32_t x,
                               uint32_t y, float current_depth) {
    for (uint32_t i = 0; i < fb->attachment_count; i++) {
        image_t* attachment = fb->attachments[i];

        switch (attachment->format) {
        case IMAGE_FORMAT_DEPTH:
            if (pipeline->depth.test) {
                const float* depth_data = attachment->data;

                uint32_t index = image_get_pixel_index(attachment, x, y);
                float closest_depth = depth_data[index];

                if (current_depth > closest_depth) {
                    return false;
                }
            }

            break;
        default:
            // nothing
            break;
        }
    }

    return true;
}

static float get_blending_factor(blend_factor factor, const struct blend_context* bc) {
    switch (factor) {
    case BLEND_FACTOR_SRC_ALPHA:
        return bc->src_alpha;
    case BLEND_FACTOR_ONE_MINUS_SRC_ALPHA:
        return 1.f - bc->src_alpha;
    case BLEND_FACTOR_ONE:
        return 1.f;
    case BLEND_FACTOR_ZERO:
    default:
        return 0.f;
    }
}

static uint32_t blend_channel(uint8_t src, uint8_t dst, const struct blend_context* bc,
                              const struct component_blend_op* op) {
    float src_factor = get_blending_factor(op->src_factor, bc);
    float dst_factor = get_blending_factor(op->dst_factor, bc);

    float src_val = (float)src / (float)0xFF;
    float dst_val = (float)dst / (float)0xFF;

    float src_operand = src_val * src_factor;
    float dst_operand = dst_val * dst_factor;

    float result;
    switch (op->op) {
    case BLEND_OP_ADD:
        result = src_operand + dst_operand;
        break;
    case BLEND_OP_SRC_SUB_DST:
        result = src_operand - dst_operand;
        break;
    case BLEND_OP_DST_SUB_SRC:
        result = dst_operand - src_operand;
        break;
    default:
        result = 0.f;
    }

    result = result > 0.f ? result : 0.f;
    return result > 1.f ? 0xFF : (uint8_t)(result * (float)0xFF);
}

static uint32_t blend_pixel(uint32_t src, uint32_t dst, const struct blend_attachment* attachment) {
    if (!attachment->enabled) {
        return src;
    }

    uint8_t src_alpha = src & 0xFF;
    uint8_t dst_alpha = dst & 0xFF;

    struct blend_context bc;
    bc.src_alpha = (float)src_alpha / (float)0xFF;
    bc.dst_alpha = (float)dst_alpha / (float)0xFF;

    uint32_t result = 0;
    for (uint32_t i = 0; i < 4; i++) {
        uint8_t src_channel = (src >> (i * 8)) & 0xFF;
        uint8_t dst_channel = (dst >> (i * 8)) & 0xFF;

        const struct component_blend_op* op = i > 0 ? &attachment->color : &attachment->alpha;
        uint8_t channel = blend_channel(src_channel, dst_channel, &bc, op);

        result |= (uint32_t)channel << (i * 8);
    }

    return result;
}

static void render_pixel(uint32_t x, uint32_t y, const struct render_context* rc) {
    float weights[rc->vertices];
    float point[2];

    point[0] = ((float)x + 0.5f) / (float)rc->fb->width * 2.f - 1.f;
    point[1] = ((float)y + 0.5f) / (float)rc->fb->height * 2.f - 1.f;

    if (!face_contains_point(rc->pipeline->winding == WINDING_ORDER_CW, rc->pipeline->cull_back,
                             rc->outputs, rc->vertices, point, weights)) {
        return;
    }

    float inverse_depth = 0.f;
    for (uint8_t i = 0; i < rc->vertices; i++) {
        inverse_depth += weights[i] / rc->outputs[i].position[2];
    }

    float depth = 1.f / inverse_depth;
    if (!pre_fragment_tests(rc->pipeline, rc->fb, x, y, depth)) {
        return;
    }

    struct shader_context context;
    context.instance_index = rc->instance_id;
    context.uniform_data = rc->uniform_data;

    if (rc->pipeline->shader.working_size > 0) {
        context.working_data = mem_alloc(rc->pipeline->shader.working_size);
    } else {
        context.working_data = NULL;
    }

    shader_blend_parameters(&rc->pipeline->shader, rc->outputs, rc->vertices, weights, depth,
                            context.working_data);

    uint32_t src_color = rc->pipeline->shader.fragment_stage(&context);
    uint32_t blending_index = 0;

    for (uint32_t i = 0; i < rc->fb->attachment_count; i++) {
        image_t* attachment = rc->fb->attachments[i];

        bool discard = false;
        image_pixel value;

        image_pixel dst = image_get_pixel(attachment, x, y);
        switch (attachment->format) {
        case IMAGE_FORMAT_COLOR:
            if (blending_index >= rc->pipeline->blend_attachment_count) {
                value.color = src_color;
            } else {
                const struct blend_attachment* attachment =
                    &rc->pipeline->blend_attachments[blending_index++];

                value.color = blend_pixel(src_color, dst.color, attachment);
            }

            break;
        case IMAGE_FORMAT_DEPTH:
            if (rc->pipeline->depth.write) {
                value.depth = depth;
            } else {
                discard = true;
            }

            break;
        default:
            memset(&value, 0, sizeof(image_pixel));
            break;
        }

        if (discard) {
            continue;
        }

        image_set_pixel(attachment, x, y, &value);
    }

    mem_free(context.working_data);
}

struct scanline {
    const struct render_context* rc;
    const struct rect* scissor;

    uint32_t index;
};

void render_scanline(void* user_data, void* job) {
    rasterizer_t* rast = user_data;
    struct scanline* sl = job;

    for (uint32_t y_offset = sl->index; y_offset < sl->scissor->height;
         y_offset += rast->num_scanlines) {
        uint32_t y = sl->scissor->y + y_offset;

        for (uint32_t x_offset = 0; x_offset < sl->scissor->width; x_offset++) {
            uint32_t x = sl->scissor->x + x_offset;

            render_pixel(x, y, sl->rc);
        }
    }

    if (sl->rc->semaphore) {
        semaphore_signal(sl->rc->semaphore);
    }
}

rasterizer_t* rasterizer_create(bool multithread) {
    rasterizer_t* rast = mem_alloc(sizeof(rasterizer_t));

    if (multithread) {
        rast->worker = thread_worker_start(render_scanline, rast);
        rast->num_scanlines = thread_worker_get_thread_count(rast->worker);
    } else {
        rast->worker = NULL;
        rast->num_scanlines = 1;
    }

    rast->current_capture = NULL;
    return rast;
}

void rasterizer_destroy(rasterizer_t* rast) {
    if (!rast) {
        return;
    }

    thread_worker_stop(rast->worker);
    mem_free(rast);
}

void rasterizer_set_current_capture(rasterizer_t* rast, capture_t* cap) {
    rast->current_capture = cap;
}

static float map_dimension(float value, uint32_t size) {
    if (value < -1.f) {
        return 0;
    }

    if (value > 1.f) {
        return size;
    }

    float screen_space = (value + 1.f) / 2.f;
    return screen_space * size;
}

static bool gen_scissor_rect(const struct render_context* rc, struct rect* scissor,
                             const struct rect* existing_scissor) {
    uint32_t x0 = UINT32_MAX;
    uint32_t y0 = UINT32_MAX;

    uint32_t x1 = 0;
    uint32_t y1 = 0;

    for (uint8_t i = 0; i < rc->vertices; i++) {
        const float* point = rc->outputs[i].position;

        float x = map_dimension(point[0], rc->fb->width);
        float y = map_dimension(point[1], rc->fb->height);

        if (x < x0) {
            x0 = floor(x);
        }

        if (y < y0) {
            y0 = floor(y);
        }

        if (x > x1) {
            x1 = ceil(x);
        }

        if (y > y1) {
            y1 = ceil(y);
        }
    }

    if (existing_scissor) {
        uint32_t x1_not = existing_scissor->x + existing_scissor->width;
        uint32_t y1_not = existing_scissor->y + existing_scissor->height;

        if (existing_scissor->x > x0) {
            x0 = existing_scissor->x;
        }

        if (existing_scissor->y > y0) {
            y0 = existing_scissor->y;
        }

        if (x1_not < x1) {
            x1 = x1_not;
        }

        if (y1_not < y1) {
            y1 = y1_not;
        }
    }

    scissor->x = x0;
    scissor->y = y0;
    scissor->width = x1 - x0;
    scissor->height = y1 - y0;

    return x1 > x0 && y1 > y0;
}

static void render_face(rasterizer_t* rast, const struct indexed_render_call* data, uint32_t face,
                        struct render_context* rc, struct captured_primitive* captured) {
    process_face_vertices(data, rc->instance_id, face, rc->vertices, rc->outputs, captured);

    // todo: support geometry shaders?

    struct rect scissor;
    if (!gen_scissor_rect(rc, &scissor, data->scissor_rect)) {
        return;
    }

    if (captured) {
        memcpy(&captured->scissor, &scissor, sizeof(struct rect));
    }

    uint32_t total_jobs =
        scissor.height < rast->num_scanlines ? scissor.height : rast->num_scanlines;

    struct scanline scanlines[total_jobs];
    for (uint32_t i = 0; i < total_jobs; i++) {
        struct scanline* sl = &scanlines[i];
        sl->index = i;
        sl->scissor = &scissor;
        sl->rc = rc;

        // if multithreading is supported, we want to take advantage of it
        if (rast->worker) {
            thread_worker_push_job(rast->worker, sl);
        } else {
            render_scanline(rast, sl);
        }
    }

    // if we have a semaphore, its probably being signaled
    if (rc->semaphore) {
        semaphore_wait_for_value(rc->semaphore, total_jobs);
    }
}

static uint8_t topology_get_vertex_count(topology_type topology) {
    switch (topology) {
    case TOPOLOGY_TYPE_TRIANGLES:
        return 3;
    case TOPOLOGY_TYPE_QUADS:
        return 4;
    default:
        return 0;
    }
}

void render_indexed(rasterizer_t* rast, struct indexed_render_call* data) {
    // todo: add support for strips! only lists are supported
    uint8_t vertices_per_face = topology_get_vertex_count(data->pipeline->topology);
    uint32_t face_count = data->index_count / vertices_per_face;

    // do we care if there are unused indices?

    struct vertex_output outputs[vertices_per_face];
    void* working_data_block = mem_alloc(data->pipeline->shader.working_size * vertices_per_face);

    for (uint8_t i = 0; i < vertices_per_face; i++) {
        outputs[i].working_data = working_data_block + data->pipeline->shader.working_size * i;
    }

    struct render_context rc;
    rc.pipeline = data->pipeline;
    rc.fb = data->framebuffer;
    rc.outputs = outputs;
    rc.vertices = vertices_per_face;
    rc.uniform_data = data->uniform_data;

    if (rast->worker) {
        rc.semaphore = semaphore_create();
    } else {
        rc.semaphore = NULL;
    }

    struct captured_render_call* captured = NULL;
    if (rast->current_capture) {
        captured = mem_alloc(sizeof(struct captured_render_call));
        captured->first_instance = data->first_instance;
        captured->instance_count = data->instance_count;
        captured->vertices_per_primitive = vertices_per_face;
        captured->primitive_count = face_count;
        captured->vertex_buffer_count = data->pipeline->binding_count;
        captured->working_data_stride = data->pipeline->shader.working_size;

        captured->vertex_buffers =
            mem_alloc(sizeof(struct captured_vertex_buffer) * captured->vertex_buffer_count);

        captured->instances = mem_alloc(sizeof(struct captured_instance) * data->instance_count);

        for (uint32_t i = 0; i < captured->vertex_buffer_count; i++) {
            struct captured_vertex_buffer* captured_vbuf = &captured->vertex_buffers[i];
            const struct vertex_binding* binding = &data->pipeline->bindings[i];
            const struct vertex_buffer* vbuf = &data->vertices[i];

            captured_vbuf->instance_data = binding->input_rate == VERTEX_INPUT_RATE_INSTANCE;
            captured_vbuf->vertex_stride = binding->stride;

            captured_vbuf->size = vbuf->size;
            captured_vbuf->data = mem_alloc(vbuf->size);
            memcpy(captured_vbuf->data, vbuf->data, vbuf->size);
        }
    }

    for (uint32_t i = 0; i < data->instance_count; i++) {
        rc.instance_id = data->first_instance + i;

        struct captured_instance* captured_instance = NULL;
        if (captured) {
            captured_instance = &captured->instances[i];
            captured_instance->primitives =
                mem_alloc(sizeof(struct captured_primitive) * face_count);
        }

        for (uint32_t j = 0; j < face_count; j++) {
            struct captured_primitive* captured_primitive = NULL;
            if (captured_instance) {
                captured_primitive = &captured_instance->primitives[j];
            }

            render_face(rast, data, j, &rc, captured_primitive);
        }
    }

    if (rast->current_capture && captured) {
        capture_add_render_call(rast->current_capture, data->framebuffer, captured);
    }

    semaphore_destroy(rc.semaphore);
    mem_free(working_data_block);
}
