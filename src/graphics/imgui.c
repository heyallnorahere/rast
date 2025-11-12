#include "imgui.h"

#include "core/mem.h"
#include "core/util.h"
#include "graphics/rasterizer.h"
#include "graphics/image.h"
#include "graphics/texture.h"
#include "math/mat.h"
#include "math/geo.h"

#include <string.h>

struct imgui_renderer_data {
    rasterizer_t* rast;

    struct pipeline pipeline;
    struct vertex_binding binding;
    struct blended_parameter blended_params[2];
    struct blend_attachment color_blending;

    struct sampler sampler;
};

struct imgui_fragment_data {
    float uv[2];
    float color[4];
};

struct imgui_uniform_data {
    float projection[4 * 4];
    struct texture tex;
};

static void convert_imgui_color(uint32_t src, float* color) {
    for (uint32_t i = 0; i < 4; i++) {
        uint8_t byte = (src >> (i * 8)) & 0xFF;
        color[i] = (float)byte / (float)0xFF;
    }
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
    convert_imgui_color(vertex->col, frag_data->color);
}

static uint32_t imgui_fragment_shader(const struct shader_context* context) {
    const struct imgui_fragment_data* frag_data = context->working_data;
    const struct imgui_uniform_data* uniforms = context->uniform_data;

    float sample[4];
    texture_sample(&uniforms->tex, frag_data->uv, sample);

    float color[4];
    for (uint32_t i = 0; i < 4; i++) {
        color[i] = sample[i] * frag_data->color[i];
    }

    return util_float4_to_u32(color);
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
    data->pipeline.blend_attachment_count = 1;
    data->pipeline.blend_attachments = &data->color_blending;
    data->pipeline.cull_back = false;

    data->blended_params[0].count = 2;
    data->blended_params[0].type = ELEMENT_TYPE_FLOAT;
    data->blended_params[0].offset = (size_t)&((struct imgui_fragment_data*)NULL)->uv;

    data->blended_params[1].count = 4;
    data->blended_params[1].type = ELEMENT_TYPE_FLOAT;
    data->blended_params[1].offset = (size_t)&((struct imgui_fragment_data*)NULL)->color;

    data->binding.input_rate = VERTEX_INPUT_RATE_VERTEX;
    data->binding.stride = sizeof(struct ImDrawVert);

    data->color_blending.enabled = true;
    data->color_blending.color.op = BLEND_OP_ADD;
    data->color_blending.color.src_factor = BLEND_FACTOR_SRC_ALPHA;
    data->color_blending.color.dst_factor = BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    data->color_blending.alpha.op = BLEND_OP_ADD;
    data->color_blending.alpha.src_factor = BLEND_FACTOR_ONE;
    data->color_blending.alpha.dst_factor = BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;

    data->sampler.filter = SAMPLER_FILTER_NEAREST;
    data->sampler.wrapping = SAMPLER_WRAPPING_REPEAT;

    // viewports? maybe
}

static void imgui_destroy_texture(ImTextureData* tex) {
    image_free(tex->BackendUserData);

    tex->BackendUserData = NULL;
    tex->TexID = 0;

    tex->Status = ImTextureStatus_Destroyed;
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

    ImGuiPlatformIO* platform_io = igGetPlatformIO_Nil();
    for (int i = 0; i < platform_io->Textures.Size; i++) {
        imgui_destroy_texture(platform_io->Textures.Data[i]);
    }

    // todo: if we use viewports, destroy resources? idk

    mem_free(data);
    io->BackendRendererUserData = NULL;
}

static void imgui_update_texture(ImTextureData* tex) {
    if (tex->Status == ImTextureStatus_OK) {
        return;
    }

    image_t* image;
    bool new_texture = tex->Status == ImTextureStatus_WantCreate;
    if (new_texture) {
        image = image_allocate(tex->Width, tex->Height, IMAGE_FORMAT_COLOR);
        tex->BackendUserData = image;
        tex->TexID = (ImTextureID)image;
    } else {
        image = tex->BackendUserData;
    }

    if (new_texture || tex->Status == ImTextureStatus_WantUpdates) {
        struct rect upload_scissor;
        upload_scissor.x = new_texture ? 0 : tex->UpdateRect.x;
        upload_scissor.y = new_texture ? 0 : tex->UpdateRect.y;
        upload_scissor.width = new_texture ? tex->Width : tex->UpdateRect.w;
        upload_scissor.height = new_texture ? tex->Height : tex->UpdateRect.h;

        uint32_t* dst = image->data;
        for (uint32_t y_src = 0; y_src < upload_scissor.height; y_src++) {
            uint32_t y_dst = upload_scissor.y + y_src;

            void* src_row = ImTextureData_GetPixelsAt(tex, upload_scissor.x, y_dst);
            for (uint32_t x_src = 0; x_src < upload_scissor.width; x_src++) {
                uint32_t x_dst = upload_scissor.x + x_src;

                size_t src_offset = x_src * tex->BytesPerPixel;
                const uint8_t* src_pixel = src_row + src_offset;

                uint32_t dst_pixel = 0;
                for (uint32_t i = 0; i < tex->BytesPerPixel; i++) {
                    uint8_t channel = src_pixel[i];
                    dst_pixel |= (uint32_t)channel << ((3 - i) * 8);
                }

                uint32_t dst_index = (y_dst * image->width) + x_dst;
                dst[dst_index] = dst_pixel;
            }
        }

        tex->Status = ImTextureStatus_OK;
    }

    if (tex->Status == ImTextureStatus_WantDestroy) {
        imgui_destroy_texture(tex);
    }
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

    uniforms.tex.sampler = &renderer_data->sampler;

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

    if (data->Textures) {
        for (int i = 0; i < data->Textures->Size; i++) {
            ImTextureData* tex = data->Textures->Data[i];
            if (tex->Status != ImTextureStatus_OK) {
                imgui_update_texture(tex);
            }
        }
    }

    for (int i = 0; i < data->CmdListsCount; i++) {
        ImDrawList* draw_list = data->CmdLists.Data[i];

        struct vertex_buffer vbuf;
        vbuf.data = draw_list->VtxBuffer.Data;
        vbuf.size = draw_list->VtxBuffer.Size * sizeof(struct ImDrawVert);

        call.vertices = &vbuf;
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

            ImTextureID tex_id = ImDrawCmd_GetTexID(cmd);
            uniforms.tex.image = (image_t*)tex_id;

            float scissor_min[2], scissor_max[2];
            scissor_min[0] = (cmd->ClipRect.x - scissor_offset.x) * scissor_scale.x;
            scissor_min[1] = (cmd->ClipRect.y - scissor_offset.y) * scissor_scale.y;
            scissor_max[0] = (cmd->ClipRect.z - scissor_offset.x) * scissor_scale.x;
            scissor_max[1] = (cmd->ClipRect.w - scissor_offset.y) * scissor_scale.y;

            scissor.x = (int32_t)scissor_min[0];
            scissor.y = (int32_t)scissor_min[1];
            scissor.width = (uint32_t)(scissor_max[0] - scissor_min[0]);
            scissor.height = (uint32_t)(scissor_max[1] - scissor_min[1]);

            render_indexed(renderer_data->rast, &call);
        }
    }
}
