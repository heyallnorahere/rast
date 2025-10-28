#include "imgui.h"

#include "mem.h"
#include "rasterizer.h"
#include "mat.h"

#include <string.h>

struct imgui_renderer_data {
    struct pipeline pipeline;
    struct vertex_binding binding;
    struct blended_parameter blended_params[2];
};

struct imgui_fragment_data {
    float uv[2];
    uint32_t color;
};

struct imgui_uniform_data {
    float projection[4 * 4];
};

static void imgui_vertex_shader(const void* const* bindings, const struct shader_context* context,
                                float* position) {
    const struct ImDrawVert* vertex = bindings[0];
    const struct imgui_uniform_data* render_data = context->uniform_data;

    float input_pos[4];
    memcpy(input_pos, &vertex->pos, 2 * sizeof(float));
    input_pos[2] = 0.f;
    input_pos[3] = 1.f;

    mat_dot(render_data->projection, input_pos, 4, 4, 1, position);

    struct imgui_fragment_data* frag_data = context->working_data;
    memcpy(frag_data->uv, &vertex->uv, 2 * sizeof(float));
    frag_data->color = vertex->col;
}

static uint32_t imgui_fragment_shader(const struct shader_context* context) {
    const struct imgui_fragment_data* frag_data = context->working_data;
    const struct imgui_uniform_data* render_data = context->uniform_data;

    // todo: texture sampling
    return frag_data->color;
}

static void* imgui_mem_alloc(size_t size, void* user_data) { return mem_alloc(size); }
static void imgui_mem_free(void* block, void* user_data) { return mem_free(block); }

static void imgui_set_allocators() {
    igSetAllocatorFunctions(imgui_mem_alloc, imgui_mem_free, NULL);
}

void imgui_init_platform() {
    ImGuiIO* io = igGetIO_Nil();
    io->BackendPlatformName = "SDL via rast";

    // viewports? maybe
}

void imgui_init_renderer() {
    struct imgui_renderer_data* data = mem_alloc(sizeof(struct imgui_renderer_data));

    ImGuiIO* io = igGetIO_Nil();
    io->BackendRendererName = "rast";
    io->BackendRendererUserData = data;
    io->BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;
    io->BackendFlags |= ImGuiBackendFlags_RendererHasTextures;

    memset(&data->pipeline, 0, sizeof(struct pipeline));
    data->pipeline.depth.test = false;
    data->pipeline.depth.write = false;
    data->pipeline.shader.vertex_stage = imgui_vertex_shader;
    data->pipeline.shader.fragment_stage = imgui_fragment_shader;
    data->pipeline.shader.inter_stage_parameter_count = 2;
    data->pipeline.shader.inter_stage_parameters = data->blended_params;
    data->pipeline.binding_count = 1;
    data->pipeline.bindings = &data->binding;
    data->pipeline.topology = TOPOLOGY_TYPE_TRIANGLES;

    data->blended_params[0].count = 2;
    data->blended_params[0].type = ELEMENT_TYPE_FLOAT;
    data->blended_params[0].offset = (size_t)(&((struct imgui_fragment_data*)NULL)->uv);

    data->blended_params[1].count = 4;
    data->blended_params[1].type = ELEMENT_TYPE_BYTE;
    data->blended_params[1].offset = (size_t)(&((struct imgui_fragment_data*)NULL)->color);

    data->binding.input_rate = VERTEX_INPUT_RATE_VERTEX;
    data->binding.stride = sizeof(struct ImDrawVert);

    // viewports? maybe
}
