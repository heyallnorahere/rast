#include <glib.h>

#include "window.h"
#include "image.h"
#include "rasterizer.h"

struct vertex {
    float x, y;
    uint32_t color;
};

struct shader_working_data {
    uint32_t color;
};

static const struct vertex s_vertices[] = {
    { 0.f, -0.5f, 0xFF0000FF },
    { 0.5f, 0.5f, 0x0000FFFF },
    { -0.5f, 0.5f, 0x00FF00FF },
};

static const uint16_t s_indices[] = { 0, 1, 2 };

static void vertex_shader(const void* const* vertex_data, const struct shader_context* context,
                          float* position) {
    const struct vertex* data = vertex_data[0];
    struct shader_working_data* result = context->working_data;

    position[0] = data->x;
    position[1] = data->y;
    position[2] = 0.5f;

    result->color = data->color;
}

static uint32_t fragment_shader(const struct shader_context* context) {
    const struct shader_working_data* data = context->working_data;
    return data->color;
}

int main(int argc, const char** argv) {
    window_t* window = NULL;
    bool success = true;

    struct vertex_binding binding;
    binding.stride = sizeof(struct vertex);
    binding.input_rate = VERTEX_INPUT_RATE_VERTEX;

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
    pipeline.binding_count = 1;
    pipeline.bindings = &binding;
    pipeline.winding = WINDING_ORDER_CCW;
    pipeline.topology = TOPOLOGY_TYPE_TRIANGLES;

    struct framebuffer fb;
    fb.attachment_count = 1;

    struct indexed_render_call call;
    memset(&call, 0, sizeof(struct indexed_render_call));

    const void* vertices = s_vertices;
    call.vertices = &vertices;
    call.indices = s_indices;
    call.index_count = 3;
    call.instance_count = 1;
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
