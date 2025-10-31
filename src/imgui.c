#include "imgui.h"

#include "mem.h"
#include "rasterizer.h"
#include "mat.h"

#include <string.h>

struct imgui_renderer_data {
    rasterizer_t* rast;

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

static uint32_t convert_imgui_color(uint32_t src) {
    // imgui colors are stored in reverse of ours
    
    uint32_t dst = 0;
    for (uint32_t dst_index = 0; dst_index < 4; dst_index++) {
        uint32_t src_index = 4 - (dst_index + 1);

        uint8_t byte = (src >> (src_index * 8)) & 0xFF;
        dst |= byte << (dst_index * 8);
    }

    return dst;
}

static void imgui_vertex_shader(const void* const* bindings, const struct shader_context* context,
                                float* position) {
    const struct ImDrawVert* vertex = bindings[0];
    const struct imgui_uniform_data* render_data = context->uniform_data;

    float input_pos[4];
    input_pos[0] = vertex->pos.x;
    input_pos[1] = vertex->pos.y;
    input_pos[2] = 0.f;
    input_pos[3] = 1.f;

    mat_dot(render_data->projection, input_pos, 4, 4, 1, position);

    struct imgui_fragment_data* frag_data = context->working_data;
    memcpy(frag_data->uv, &vertex->uv, 2 * sizeof(float));
    frag_data->color = convert_imgui_color(vertex->col);
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

void imgui_init_renderer(rasterizer_t* rast) {
    struct imgui_renderer_data* data = mem_alloc(sizeof(struct imgui_renderer_data));
    data->rast = rast;

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
    io->BackendRendererUserData = NULL;
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

    memset(&call, 0, sizeof(struct indexed_render_call));
    call.pipeline = &renderer_data->pipeline;
    call.framebuffer = fb;
    call.first_instance = 0;
    call.instance_count = 1;
    call.uniform_data = &uniforms;
    call.scissor_rect = &scissor;

    ImVec2 scissor_offset = data->DisplayPos;
    ImVec2 scissor_scale = data->FramebufferScale;

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
            call.index_count = cmd->ElemCount;

            float scissor_min[2], scissor_max[2];
            scissor_min[0] = (cmd->ClipRect.x - scissor_offset.x) * scissor_scale.x;
            scissor_min[1] = (cmd->ClipRect.y - scissor_offset.y) * scissor_scale.y;
            scissor_max[0] = (cmd->ClipRect.z - scissor_offset.x) * scissor_scale.x;
            scissor_max[1] = (cmd->ClipRect.w - scissor_offset.y) * scissor_scale.y;

            scissor.x = (int32_t)scissor_min[0];
            scissor.y = (int32_t)scissor_min[1];
            scissor.width = (uint32_t)(scissor_max[0] - scissor_min[0]);
            scissor.height = (uint32_t)(scissor_max[1] - scissor_min[1]);

            // todo: pass texture data!
            
            render_indexed(renderer_data->rast, &call);
        }
    }
}
