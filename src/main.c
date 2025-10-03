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
    float depth;
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

    position[2] = instance->depth;

    struct shader_working_data* result = context->working_data;
    result->color = instance->color;
}

static uint32_t fragment_shader(const struct shader_context* context) {
    const struct shader_working_data* data = context->working_data;
    return data->color;
}

static void time_diff(const struct timespec* t0, const struct timespec* t1,
                      struct timespec* delta) {
    delta->tv_sec = t1->tv_sec - t0->tv_sec;
    delta->tv_nsec = t1->tv_nsec - t0->tv_nsec;

    if (t1->tv_nsec < t0->tv_nsec) {
        delta->tv_sec--;
        delta->tv_nsec += 1e9;
    }
}

static bool is_depth_buffer_valid(image_t* buffer, uint32_t width, uint32_t height) {
    if (!buffer) {
        return false;
    }

    return buffer->width == width && buffer->height == height;
}

static void validate_depth_buffer(window_t* window, image_t** buffer) {
    uint32_t width, height;
    if (!window_get_framebuffer_size(window, &width, &height)) {
        return;
    }

    if (!is_depth_buffer_valid(*buffer, width, height)) {
        image_free(*buffer);
        *buffer = image_allocate(width, height, IMAGE_FORMAT_DEPTH);
    }
}

int main(int argc, const char** argv) {
    srand(time(NULL));

    window_t* window = NULL;
    image_t* depth_buffer = NULL;
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
    pipeline.depth.test = true;
    pipeline.depth.write = true;
    pipeline.binding_count = 2;
    pipeline.bindings = bindings;
    pipeline.winding = WINDING_ORDER_CCW;
    pipeline.topology = TOPOLOGY_TYPE_TRIANGLES;

    image_t* attachments[2];
    memset(attachments, 0, sizeof(attachments));

    struct framebuffer fb;
    fb.attachment_count = sizeof(attachments) / sizeof(image_t*);
    fb.attachments = attachments;

    struct indexed_render_call call;
    memset(&call, 0, sizeof(struct indexed_render_call));

    struct instance instances[6];
    uint32_t instance_count = sizeof(instances) / sizeof(struct instance);

    for (uint32_t i = 0; i < instance_count; i++) {
        struct instance* instance = &instances[i];

        instance->color = 0xFF;
        for (size_t j = 0; j < 3; j++) {
            uint8_t channel = rand() & 0xFF;
            instance->color |= channel << ((j + 1) * 8);
        }

        instance->scale = 0.25f;
        instance->radius = 0.125f;
        instance->theta = (float)M_PI * 2.f * (float)i / (float)instance_count;
    }

    const void* vertices[] = { s_vertices, instances };
    call.vertices = vertices;

    call.indices = s_indices;
    call.index_count = 3;
    call.instance_count = instance_count;
    call.pipeline = &pipeline;
    call.framebuffer = &fb;

    struct timespec t0, t1, delta;
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &t0);

    image_pixel clear[2];
    clear[0].color = 0x787878FF;
    clear[1].depth = 1.f;

    window = window_create("rast", 1600, 900);
    while (!window_is_close_requested(window)) {
        clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &t1);
        time_diff(&t0, &t1, &delta);

        float t = (float)delta.tv_sec + (float)delta.tv_nsec / 1e+9f;
        for (uint32_t i = 0; i < instance_count; i++) {
            struct instance* instance = &instances[i];

            float theta = instance->theta + t * (float)M_PI / 2.f;
            instance->depth = sinf(theta) * 0.5f + 0.5f;
        }

        image_t* backbuffer = window_get_backbuffer(window);
        if (!backbuffer) {
            success = false;
            break;
        }

        attachments[0] = backbuffer;
        fb.width = backbuffer->width;
        fb.height = backbuffer->height;

        validate_depth_buffer(window, &attachments[1]);

        framebuffer_clear(&fb, clear);
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
