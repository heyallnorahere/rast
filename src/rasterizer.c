#include "rasterizer.h"

#include "image.h"
#include "mem.h"
#include "vec.h"

#include <glib.h>

static void image_set_pixel(image_t* image, uint32_t x, uint32_t y, const image_pixel* value) {
    size_t stride = image_get_pixel_stride(image->format);
    uint32_t index = image_get_pixel_index(image, x, y);

    size_t offset = stride * index;
    void* pixel_address = image->data + offset;

    memcpy(pixel_address, value, stride);
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

static bool face_contains_point(bool cw, bool cull, const struct vertex_output* outputs,
                                uint8_t vertices, const float* point) {
    float current_to_next[2];
    float current_to_point[2];
    float normal[2];

    int8_t common_direction = 0;
    for (uint8_t i = 0; i < vertices; i++) {
        uint8_t next_vertex_id = (i + 1) % vertices;

        vec_sub(outputs[next_vertex_id].position, outputs[i].position, 2, current_to_next);
        vec_sub(point, outputs[i].position, 2, current_to_point);

        vec_rot90(current_to_next, normal, cw);
        float coefficient = vec_dot(normal, current_to_point, 2);

        if (cull && coefficient < 0.f) {
            return false;
        }

        int8_t current_direction = coefficient > 0.f ? 1 : (coefficient < 0.f ? -1 : 0);
        if (common_direction == 0) {
            common_direction = current_direction;
        } else if (common_direction != current_direction) {
            return false;
        }
    }

    return true;
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

    float point[2];
    for (uint32_t y = 0; y < data->framebuffer->height; y++) {
        point[1] = -((float)y / (float)data->framebuffer->height * 2.f - 1.f);

        for (uint32_t x = 0; x < data->framebuffer->width; x++) {
            point[0] = (float)x / (float)data->framebuffer->width * 2.f - 1.f;

            if (!face_contains_point(data->pipeline->winding == WINDING_ORDER_CW,
                                     data->pipeline->cull_back, outputs, indices, point)) {
                continue;
            }

            // todo: blend?

            uint32_t color = data->pipeline->shader.fragment_stage(&context);
            for (uint32_t i = 0; i < data->framebuffer->attachment_count; i++) {
                image_t* attachment = data->framebuffer->attachments[i];

                bool discard = false;
                image_pixel value;

                switch (attachment->format) {
                case IMAGE_FORMAT_COLOR:
                    value.color = color;
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
        }
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
