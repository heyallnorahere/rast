#ifndef RASTERIZER_H_
#define RASTERIZER_H_

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// from image.h
typedef struct image image_t;

// from mt_worker.h
typedef struct mt_worker mt_worker_t;

struct framebuffer {
    image_t* const* attachments;
    uint32_t attachment_count;

    uint32_t width, height;
};

typedef union {
    uint32_t color;
    float depth;
} image_pixel;

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

struct pipeline {
    struct shader shader;

    struct {
        bool test, write;
    } depth;

    const struct vertex_binding* bindings;
    uint32_t binding_count;

    winding_order winding;
    topology_type topology;
};

struct indexed_render_call {
    const void* const* vertices;
    const uint16_t* indices;

    uint32_t vertex_offset;
    uint32_t first_index, index_count;
    uint32_t first_instance, instance_count;

    const struct pipeline* pipeline;
    struct framebuffer* framebuffer;

    void* uniform_data;

    mt_worker_t* worker;
};

void framebuffer_clear(struct framebuffer* fb, const image_pixel* clear_values);

void render_indexed(const struct indexed_render_call* data);

#endif
