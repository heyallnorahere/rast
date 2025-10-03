#include "rasterizer.h"

#include "image.h"
#include "mem.h"
#include "vec.h"

#include <glib.h>

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

void framebuffer_clear(struct framebuffer* fb, const image_pixel* clear_values) {
    for (uint32_t i = 0; i < fb->attachment_count; i++) {
        image_t* attachment = fb->attachments[i];
        framebuffer_fill_attachment(attachment, &clear_values[i]);
    }
}

struct vertex_output {
    void* working_data;
    float position[4];
};

static void run_vertex_stage(const struct shader* shader, const void* const* vertex_data,
                             struct shader_context* context, struct vertex_output* output) {
    g_assert(!context->working_data);

    memset(output->position, 0, 4 * sizeof(float));
    output->position[3] = 1.f;

    if (shader->working_size > 0) {
        context->working_data = mem_alloc(shader->working_size);
    }

    shader->vertex_stage(vertex_data, context, output->position);
    output->working_data = context->working_data;

    context->working_data = NULL;
}

static void process_face_vertices(const struct indexed_render_call* data, uint32_t instance,
                                  uint32_t face, uint8_t indices, struct shader_context* context,
                                  struct vertex_output* outputs) {
    const void* vertex_data[data->pipeline->binding_count];
    for (uint8_t i = 0; i < indices; i++) {
        uint32_t index_id = data->first_index + face * indices + i;
        uint32_t vertex_id = data->vertex_offset + data->indices[index_id];
        context->vertex_index = vertex_id;

        for (uint32_t j = 0; j < data->pipeline->binding_count; j++) {
            const struct vertex_binding* binding = &data->pipeline->bindings[j];

            uint32_t buffer_index;
            switch (binding->input_rate) {
            case VERTEX_INPUT_RATE_VERTEX:
                buffer_index = vertex_id;
                break;
            case VERTEX_INPUT_RATE_INSTANCE:
                buffer_index = instance;
                break;
            default:
                g_assert_not_reached();
            }

            size_t offset = buffer_index * binding->stride;
            vertex_data[j] = data->vertices[j] + offset;
        }

        run_vertex_stage(&data->pipeline->shader, vertex_data, context, &outputs[i]);
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

static bool face_contains_point(bool cw, const struct vertex_output* outputs, uint8_t vertices,
                                const float* point, float* weights) {
    float area_sum = 0.f;
    float areas[vertices];

    for (uint8_t i = 0; i < vertices; i++) {
        const float* a = outputs[i].position;

        uint8_t next_vertex_id = (i + 1) % vertices;
        const float* b = outputs[next_vertex_id].position;

        float area = signed_quad_area(a, b, point, cw);
        if (area <= 0.f) {
            return false;
        }

        area_sum += area;
        areas[(i + 2) % vertices] = area;
    }

    if (area_sum <= 0) {
        return false;
    }

    if (weights) {
        for (uint8_t i = 0; i < vertices; i++) {
            weights[i] = areas[i] / area_sum;
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
    default:
        g_assert_not_reached();
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
                default:
                    g_assert_not_reached();
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
            default:
                g_assert_not_reached();
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

struct pixel_context {
    const struct pipeline* pipeline;
    struct framebuffer* fb;

    const struct vertex_output* outputs;
    uint8_t vertices;

    uint32_t instance_id;
    void* uniform_data;
};

struct pixel_data {
    uint32_t x, y;
    float point[2];
};

static void render_pixel(const struct pixel_data* data, const struct pixel_context* pixel_context) {
    float weights[pixel_context->vertices];

    if (!face_contains_point(pixel_context->pipeline->winding == WINDING_ORDER_CW,
                             pixel_context->outputs, pixel_context->vertices, data->point,
                             weights)) {
        return;
    }

    float inverse_depth = 0.f;
    for (uint8_t i = 0; i < pixel_context->vertices; i++) {
        inverse_depth += weights[i] / pixel_context->outputs[i].position[2];
    }

    float depth = 1.f / inverse_depth;
    if (!pre_fragment_tests(pixel_context->pipeline, pixel_context->fb, data->x, data->y, depth)) {
        return;
    }

    struct shader_context context;
    context.instance_index = pixel_context->instance_id;
    context.uniform_data = pixel_context->uniform_data;

    if (pixel_context->pipeline->shader.working_size > 0) {
        context.working_data = mem_alloc(pixel_context->pipeline->shader.working_size);
    } else {
        context.working_data = NULL;
    }

    shader_blend_parameters(&pixel_context->pipeline->shader, pixel_context->outputs,
                            pixel_context->vertices, weights, depth, context.working_data);

    uint32_t color = pixel_context->pipeline->shader.fragment_stage(&context);
    for (uint32_t i = 0; i < pixel_context->fb->attachment_count; i++) {
        image_t* attachment = pixel_context->fb->attachments[i];

        bool discard = false;
        image_pixel value;

        switch (attachment->format) {
        case IMAGE_FORMAT_COLOR:
            value.color = color;
            break;
        case IMAGE_FORMAT_DEPTH:
            if (pixel_context->pipeline->depth.write) {
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

        image_set_pixel(attachment, data->x, data->y, &value);
    }

    mem_free(context.working_data);
}

static void render_face(const struct indexed_render_call* data, uint32_t instance, uint32_t face,
                        uint8_t indices) {
    struct shader_context context;
    context.instance_index = instance;
    context.uniform_data = data->uniform_data;
    context.working_data = NULL;

    struct vertex_output outputs[indices];
    process_face_vertices(data, instance, face, indices, &context, outputs);

    // todo: support geometry shaders?

    struct pixel_context pixel_context;
    pixel_context.pipeline = data->pipeline;
    pixel_context.fb = data->framebuffer;
    pixel_context.instance_id = instance;
    pixel_context.outputs = outputs;
    pixel_context.vertices = indices;
    pixel_context.uniform_data = data->uniform_data;

    float point[2];
    for (uint32_t y = 0; y < data->framebuffer->height; y++) {
        point[1] = (float)y / (float)data->framebuffer->height * 2.f - 1.f;

        for (uint32_t x = 0; x < data->framebuffer->width; x++) {
            point[0] = (float)x / (float)data->framebuffer->width * 2.f - 1.f;

            struct pixel_data pixel_data;
            pixel_data.x = x;
            pixel_data.y = y;
            memcpy(pixel_data.point, point, 2 * sizeof(float));

            render_pixel(&pixel_data, &pixel_context);
        }
    }

    mem_free(context.working_data);
    for (uint8_t i = 0; i < indices; i++) {
        mem_free(outputs[i].working_data);
    }
}

static guint index_hasher(gconstpointer key) {
    size_t index = (size_t)key;
    return (guint)index;
}

static gboolean indices_equal(gconstpointer lhs, gconstpointer rhs) { return lhs == rhs; }

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

static void render_instance(const struct indexed_render_call* data, uint32_t instance) {
    // todo: add support for strips! only lists are supported
    uint8_t vertices_per_face = topology_get_vertex_count(data->pipeline->topology);
    uint32_t face_count = data->index_count / vertices_per_face;

    // do we care if there are unused indices?

    for (uint32_t i = 0; i < face_count; i++) {
        render_face(data, instance, i, vertices_per_face);
    }
}

void render_indexed(const struct indexed_render_call* data) {
    for (uint32_t i = 0; i < data->instance_count; i++) {
        uint32_t instance = data->first_instance + i;
        render_instance(data, instance);
    }
}
