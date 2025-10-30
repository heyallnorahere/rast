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

void imgui_set_allocators() { igSetAllocatorFunctions(imgui_mem_alloc, imgui_mem_free, NULL); }

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
    data->pipeline.shader.working_size = sizeof(struct imgui_fragment_data);
    data->pipeline.shader.vertex_stage = imgui_vertex_shader;
    data->pipeline.shader.fragment_stage = imgui_fragment_shader;
    data->pipeline.shader.inter_stage_parameter_count = 2;
    data->pipeline.shader.inter_stage_parameters = data->blended_params;
    data->pipeline.binding_count = 1;
    data->pipeline.bindings = &data->binding;
    data->pipeline.topology = TOPOLOGY_TYPE_TRIANGLES;
    data->pipeline.winding = WINDING_ORDER_CCW;

    data->blended_params[0].count = 2;
    data->blended_params[0].type = ELEMENT_TYPE_FLOAT;
    data->blended_params[0].offset = (size_t)&((struct imgui_fragment_data*)NULL)->uv;

    data->blended_params[1].count = 4;
    data->blended_params[1].type = ELEMENT_TYPE_BYTE;
    data->blended_params[1].offset = (size_t)&((struct imgui_fragment_data*)NULL)->color;

    data->binding.input_rate = VERTEX_INPUT_RATE_VERTEX;
    data->binding.stride = sizeof(struct ImDrawVert);

    // viewports? maybe
}

void imgui_shutdown_renderer() {
    ImGuiIO* io = igGetIO_Nil();
    if (!io) {
        return;
    }

    struct imgui_renderer_data* data = io->BackendRendererUserData;
    if (!data) {
        return;
    }

    // todo: if we use viewports, destroy resources? idk

    mem_free(data);
}

void imgui_render(ImDrawData* data, struct framebuffer* fb) {
    ImGuiIO* io = igGetIO_Nil();
    struct imgui_renderer_data* renderer_data = io->BackendRendererUserData;

    float scale[2];
    scale[0] = 2.f / data->DisplaySize.x;
    scale[1] = 2.f / data->DisplaySize.y;

    struct imgui_uniform_data uniforms;
    mat_identity(uniforms.projection, 4);

    // projection[0, 0]
    uniforms.projection[0] = scale[0];

    // projection[1, 1]
    uniforms.projection[5] = scale[1];

    // projection[0, 3]
    uniforms.projection[3] = -1.f - data->DisplayPos.x * scale[0];

    // projection[1, 3]
    uniforms.projection[7] = -1.f - data->DisplayPos.y * scale[1];

    // projection[2, 3]
    // bug: rasterizer doesn't like it when we use z=0 for vertices
    uniforms.projection[11] = 0.1f;

    struct indexed_render_call call;
    struct rect scissor;

    call.pipeline = &renderer_data->pipeline;
    call.framebuffer = fb;
    call.multithread = false;
    call.first_instance = 0;
    call.instance_count = 1;
    call.uniform_data = &uniforms;
    call.scissor_rect = &scissor;

    for (int i = 0; i < data->CmdListsCount; i++) {
        ImDrawList* draw_list = data->CmdLists.Data[i];

        const void* vertices = draw_list->VtxBuffer.Data;
        call.vertices = &vertices;
        call.indices = draw_list->IdxBuffer.Data;

        for (int j = 0; j < draw_list->CmdBuffer.Size; j++) {
            ImDrawCmd* cmd = &draw_list->CmdBuffer.Data[j];

            if (cmd->UserCallback) {
                cmd->UserCallback(draw_list, cmd);
                continue;
            }

            call.vertex_offset = cmd->VtxOffset;
            call.first_index = cmd->IdxOffset;
            call.instance_count = cmd->ElemCount;

            // todo: pass texture data!
            
            render_indexed(&call);
        }
    }
}
