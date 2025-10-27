#include "imgui.h"

#include "mem.h"
#include "rasterizer.h"

#include <string.h>

struct imgui {
    ImGuiContext* context;
    ImGuiIO* io;
    ImGuiPlatformIO* platform_io;

    struct pipeline pipeline;
};

static void imgui_vertex_shader(const void* const* bindings, const struct shader_context* context,
                                float* position) {
    //
}

static uint32_t imgui_fragment_shader(const struct shader_context* context) {
    //
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
