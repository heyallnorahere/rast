#include <glib.h>
#include <math.h>

#include "window.h"
#include "image.h"
#include "rasterizer.h"
#include "vec.h"

struct vertex {
    float x, y;
};

struct instance {
    float scale;
    float theta, radius;
    uint32_t color;
};

struct shader_working_data {
    uint32_t color;
};

static const struct vertex s_vertices[] = {
    { 0.f, -0.5f },
    { 0.5f, 0.5f },
    { -0.5f, 0.5f },
};

static const uint16_t s_indices[] = { 0, 1, 2 };

static void vertex_shader(const void* const* vertex_data, const struct shader_context* context,
                          float* position) {
    const struct vertex* vertex = vertex_data[0];
    const struct instance* instance = vertex_data[1];

    float model_space[2], unit_radial[2], radial[2];

    unit_radial[0] = cosf(instance->theta);
    unit_radial[1] = sinf(instance->theta);

    vec_mult(unit_radial, instance->radius, 2, radial);
    vec_mult(&vertex->x, instance->scale, 2, model_space);
    vec_add(model_space, radial, 2, position);

    position[2] = 0.5f;

    struct shader_working_data* result = context->working_data;
    result->color = instance->color;
}

static uint32_t fragment_shader(const struct shader_context* context) {
    const struct shader_working_data* data = context->working_data;
    return data->color;
}

int main(int argc, const char** argv) {
    window_t* window = NULL;
    bool success = true;

    struct vertex_binding bindings[2];
    bindings[0].stride = sizeof(struct vertex);
    bindings[0].input_rate = VERTEX_INPUT_RATE_VERTEX;
    bindings[1].stride = sizeof(struct instance);
    bindings[1].input_rate = VERTEX_INPUT_RATE_INSTANCE;

    struct blended_parameter color_parameter;
    color_parameter.count = 4;
    color_parameter.type = ELEMENT_TYPE_BYTE;
    color_parameter.offset = 0;

    struct pipeline pipeline;
    memset(&pipeline, 0, sizeof(struct pipeline));

    pipeline.shader.working_size = sizeof(struct shader_working_data);
    pipeline.shader.vertex_stage = vertex_shader;
    pipeline.shader.fragment_stage = fragment_shader;
    pipeline.shader.inter_stage_parameter_count = 1;
    pipeline.shader.inter_stage_parameters = &color_parameter;
    pipeline.binding_count = 2;
    pipeline.bindings = bindings;
    pipeline.winding = WINDING_ORDER_CCW;
    pipeline.topology = TOPOLOGY_TYPE_TRIANGLES;

    struct framebuffer fb;
    fb.attachment_count = 1;

    struct indexed_render_call call;
    memset(&call, 0, sizeof(struct indexed_render_call));

    struct instance instances[6];
    for (size_t i = 0; i < sizeof(instances) / sizeof(struct instance); i++) {
        struct instance* instance = &instances[i];

        instance->color = (rand() & 0xFFFFFF00) | 0xFF;
        instance->scale = 0.25f;
        instance->radius = 0.5f;
        instance->theta = ((float)M_PI * 2.f * (float)i) /
                          ((float)sizeof(instances) / (float)sizeof(struct instance));
    }

    const void* vertices[] = { s_vertices, instances };
    call.vertices = vertices;

    call.indices = s_indices;
    call.index_count = 3;
    call.instance_count = sizeof(instances) / sizeof(struct instance);
    call.pipeline = &pipeline;
    call.framebuffer = &fb;

    image_pixel clear_color;
    clear_color.color = 0x787878FF;

    window = window_create("rast", 1600, 900);
    while (!window_is_close_requested(window)) {
        image_t* backbuffer = window_get_backbuffer(window);
        if (!backbuffer) {
            success = false;
            break;
        }

        fb.attachments = &backbuffer;
        fb.width = backbuffer->width;
        fb.height = backbuffer->height;

        framebuffer_clear(&fb, &clear_color);
        render_indexed(&call);

        if (!window_swap_buffers(window)) {
            success = false;
            break;
        }

        window_poll();
    }

    window_destroy(window);
    return success ? 0 : 1;
}
