#include "imgui.h"

#include "mem.h"
#include "rasterizer.h"
#include "mat.h"

#include <string.h>

struct imgui {
    ImGuiContext* context;
    ImGuiIO* io;
    ImGuiPlatformIO* platform_io;

    struct pipeline pipeline;
};

struct imgui_fragment_data {
    float uv[2];
    uint32_t color;
};

struct imgui_vertex {
    float position[2];
    struct imgui_fragment_data frag_data;
};

struct imgui_render_data {
    float projection[4 * 4];
};

static void imgui_vertex_shader(const void* const* bindings, const struct shader_context* context,
                                float* position) {
    const struct imgui_vertex* vertex = bindings[0];
    const struct imgui_render_data* render_data = context->uniform_data;

    float input_pos[4];
    memcpy(input_pos, vertex->position, 2 * sizeof(float));
    input_pos[2] = 0.f;
    input_pos[3] = 1.f;

    mat_dot(render_data->projection, input_pos, 4, 4, 1, position);

    memcpy(context->working_data, &vertex->frag_data, sizeof(struct imgui_fragment_data));
}

static uint32_t imgui_fragment_shader(const struct shader_context* context) {
    //

    return 0;
}

static void* imgui_mem_alloc(size_t size, void* user_data) { return mem_alloc(size); }
static void imgui_mem_free(void* block, void* user_data) { return mem_free(block); }

static void imgui_set_allocators() {
    igSetAllocatorFunctions(imgui_mem_alloc, imgui_mem_free, NULL);
}

static void imgui_init_platform(imgui_t* instance) {
    instance->io->BackendPlatformName = "SDL via rast";

    // viewports? maybe
}

static void imgui_init_renderer(imgui_t* instance) {
    instance->io->BackendRendererName = "rast";
    instance->io->BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;
    instance->io->BackendFlags |= ImGuiBackendFlags_RendererHasTextures;

    memset(&instance->pipeline, 0, sizeof(struct pipeline));
    instance->pipeline.depth.test = false;
    instance->pipeline.depth.write = false;
    instance->pipeline.shader.vertex_stage = imgui_vertex_shader;
    instance->pipeline.shader.fragment_stage = imgui_fragment_shader;

    // viewports? maybe
}

imgui_t* imgui_init() {
    imgui_set_allocators();

    imgui_t* instance = mem_alloc(sizeof(imgui_t));
    instance->context = igCreateContext(NULL);

    instance->io = igGetIO_ContextPtr(instance->context);
    instance->platform_io = igGetPlatformIO_ContextPtr(instance->context);

    imgui_init_platform(instance);
    imgui_init_renderer(instance);

    return instance;
}

void imgui_destroy(imgui_t* instance) {
    imgui_set_allocators();

    // todo: destroy viewports?
}
