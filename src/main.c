#include <glib.h>

#include <math.h>

#include "image.h"
#include "rasterizer.h"
#include "vec.h"
#include "window.h"
#include "mat.h"

#include "imgui.h"

struct uniforms {
    float view_projection[4 * 4];
};

struct vertex {
    float position[3];
};

struct instance {
    float model[4 * 4];
    uint32_t color;
};

struct shader_working_data {
    uint32_t color;
};

static const struct vertex s_vertices[] = {
    { 0.f, -0.5f, 0.f },
    { 0.5f, 0.5f, 0.f },
    { -0.5f, 0.5f, 0.f },
};

static const uint16_t s_indices[] = { 0, 1, 2 };

static void vertex_shader(const void* const* vertex_data, const struct shader_context* context,
                          float* position) {
    const struct vertex* vertex = vertex_data[0];
    const struct instance* instance = vertex_data[1];
    const struct uniforms* uniforms = context->uniform_data;

    float vertex_pos[4];
    memcpy(vertex_pos, vertex->position, 3 * sizeof(float));
    vertex_pos[3] = 1.f;

    float world_position[4];
    mat_dot(instance->model, vertex_pos, 4, 4, 1, world_position);
    mat_dot(uniforms->view_projection, world_position, 4, 4, 1, position);

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

    if (delta->tv_nsec < 0) {
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
    imgui_set_allocators();

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

    static const uint32_t instance_count = 6;
    struct instance instances[instance_count];

    for (uint32_t i = 0; i < instance_count; i++) {
        struct instance* instance = &instances[i];

        instance->color = 0xFF;
        for (size_t j = 0; j < 3; j++) {
            uint8_t channel = rand() & 0xFF;
            instance->color |= channel << ((j + 1) * 8);
        }
    }

    const void* vertices[] = { s_vertices, instances };
    call.vertices = vertices;

    call.indices = s_indices;
    call.index_count = 3;
    call.instance_count = instance_count;
    call.pipeline = &pipeline;
    call.framebuffer = &fb;
    call.scissor_rect = NULL;
    call.multithread = false;

    struct uniforms uniforms;
    call.uniform_data = &uniforms;

    struct timespec t0, t1, delta;
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &t0);

    float total_seconds = 0.f;

    image_pixel clear[2];
    clear[0].color = 0x787878FF;
    clear[1].depth = 1.f;

    static const float up[3] = { 0.f, 1.f, 0.f };
    static const float center[3] = { 0.f, 0.f, 0.f };

    float camera_position[3];
    float camera_theta = 0.f;
    float camera_distance = 2.f;

    window = window_create("rast", 1600, 900);

    igCreateContext(NULL);
    window_init_imgui(window);
    imgui_init_renderer();

    while (!window_is_close_requested(window)) {
        window_poll();
        igNewFrame();

        static bool draw_demo = true;
        if (draw_demo) {
            igShowDemoWindow(&draw_demo);
        }

        clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &t1);
        time_diff(&t0, &t1, &delta);
        memcpy(&t0, &t1, sizeof(struct timespec));

        float delta_seconds = (float)delta.tv_sec + (float)delta.tv_nsec / 1e+9f;
        total_seconds += delta_seconds;

        for (uint32_t i = 0; i < instance_count; i++) {
            struct instance* instance = &instances[i];
            mat_identity(instance->model, 4);

            // scale
            float scale[4 * 4];
            mat_identity(scale, 4);

            for (uint32_t j = 0; j < 3; j++) {
                // scale[j, j]
                scale[j * 5] *= 0.25f;
            }

            float theta = (float)M_PI * 2.f * (float)i / (float)instance_count;

            // rotation
            float rotation[4 * 4];
            mat_identity(rotation, 4);

            // rotating around y
            // this means that i is now i'cos(theta) - k'sin(theta)
            // accordingly, k is now i'sin(theta) + k'cos(theta)

            float cos_theta = cosf(theta);
            float sin_theta = sinf(theta);

            // rotation[0, 0]
            rotation[0] = cos_theta;

            // rotation[0, 2]
            rotation[2] = -sin_theta;

            // rotation[2, 0]
            rotation[8] = sin_theta;

            // rotation[2, 2]
            rotation[10] = cos_theta;

            // translation
            float translation[4 * 4];
            mat_identity(translation, 4);

            // negative z
            // mat[2, 3]
            translation[11] = -0.5f;

            float displacement[4 * 4];
            mat_dot(rotation, translation, 4, 4, 4, displacement);
            mat_dot(scale, displacement, 4, 4, 4, instance->model);
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

        static const float vfov = M_PI / 4.f;
        float aspect = (float)backbuffer->width / (float)backbuffer->height;

        float projection[4 * 4];
        mat_perspective(projection, vfov, aspect, 0.1f, 100.f);

        float cos_theta = cosf(camera_theta);
        float sin_theta = sinf(camera_theta);

        float phi = cos_theta * (float)M_PI / 4.f;
        float cos_phi = cosf(phi);
        float sin_phi = sinf(phi);

        camera_theta += delta_seconds;
        camera_position[0] = cos_theta * cos_phi * camera_distance;
        camera_position[1] = sin_phi * camera_distance;
        camera_position[2] = sin_theta * cos_phi * camera_distance;

        float view[4 * 4];
        mat_look_at(view, camera_position, center, up);
        mat_dot(projection, view, 4, 4, 4, uniforms.view_projection);

        framebuffer_clear(&fb, clear);
        render_indexed(&call);

        igRender();
        imgui_render(igGetDrawData(), &fb);

        if (!window_swap_buffers(window)) {
            success = false;
            break;
        }
    }

    imgui_shutdown_renderer();
    window_destroy(window);
    igDestroyContext(NULL);

    return success ? 0 : 1;
}
