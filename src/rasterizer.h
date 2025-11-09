#ifndef RASTERIZER_H_
#define RASTERIZER_H_

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// from image.h
typedef struct image image_t;
typedef union image_pixel image_pixel;

struct framebuffer {
    image_t* const* attachments;
    uint32_t attachment_count;

    uint32_t width, height;
};

typedef enum {
    ELEMENT_TYPE_BYTE,
    ELEMENT_TYPE_FLOAT,
} element_type;

struct blended_parameter {
    size_t offset;

    element_type type;
    uint32_t count;
};

struct shader_context {
    // undefined except in the vertex stage
    uint32_t vertex_index;

    uint32_t instance_index;

    void* working_data;
    void* uniform_data;
};

struct shader {
    size_t working_size;

    // position: 4 float vector, defaults to <0, 0, 0, 1>
    void (*vertex_stage)(const void* const* vertex_data, const struct shader_context* context,
                         float* position);

    uint32_t (*fragment_stage)(const struct shader_context* context);

    const struct blended_parameter* inter_stage_parameters;
    uint32_t inter_stage_parameter_count;
};

typedef enum {
    VERTEX_INPUT_RATE_VERTEX,
    VERTEX_INPUT_RATE_INSTANCE,
} vertex_input_rate;

struct vertex_binding {
    size_t stride;
    vertex_input_rate input_rate;
};

typedef enum {
    WINDING_ORDER_CCW,
    WINDING_ORDER_CW,
} winding_order;

typedef enum {
    TOPOLOGY_TYPE_TRIANGLES,
    TOPOLOGY_TYPE_QUADS,
} topology_type;

typedef enum {
    BLEND_OP_ADD,
    BLEND_OP_SRC_SUB_DST,
    BLEND_OP_DST_SUB_SRC,
} blend_op;

typedef enum {
    BLEND_FACTOR_ZERO,
    BLEND_FACTOR_ONE,
    BLEND_FACTOR_SRC_ALPHA,
    BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
} blend_factor;

struct component_blend_op {
    blend_op op;
    blend_factor src_factor, dst_factor;
};

struct blend_attachment {
    bool enabled;
    struct component_blend_op color, alpha;
};

struct pipeline {
    struct shader shader;

    struct {
        bool test, write;
    } depth;

    const struct vertex_binding* bindings;
    uint32_t binding_count;

    bool cull_back;
    winding_order winding;
    topology_type topology;

    uint32_t blend_attachment_count;
    const struct blend_attachment* blend_attachments;
};

struct rect {
    uint32_t x, y;
    uint32_t width, height;
};

struct indexed_render_call {
    const void* const* vertices;
    const uint16_t* indices;

    uint32_t vertex_offset;
    uint32_t first_index, index_count;
    uint32_t first_instance, instance_count;

    const struct pipeline* pipeline;
    struct framebuffer* framebuffer;

    const struct rect* scissor_rect;

    void* uniform_data;
};

typedef struct rasterizer rasterizer_t;

rasterizer_t* rasterizer_create(uint32_t num_scanlines, bool multithread);
void rasterizer_destroy(rasterizer_t* rast);

void framebuffer_clear(struct framebuffer* fb, const image_pixel* clear_values);

void render_indexed(rasterizer_t* rast, struct indexed_render_call* data);

#endif
