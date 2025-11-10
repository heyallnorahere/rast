#ifndef CAPTURE_H_
#define CAPTURE_H_

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "math/geo.h"

// from rasterizer.h
struct framebuffer;

// from rasterizer_internal.h
struct render_context;
struct vertex_output;

// from image.h
typedef struct image image_t;
typedef union image_pixel image_pixel;

struct captured_vertex_buffer {
    size_t size, vertex_stride;
    void* data;

    bool instance_data;
};

struct captured_primitive {
    uint32_t instance_index;
    struct rect scissor;

    uint32_t* indices;
    float* vertex_positions;
    void* working_data;
};

struct captured_instance {
    uint32_t primitive_count;
    struct captured_primitive* primitives;
};

struct captured_render_call {
    uint32_t instance_count;
    struct captured_instance* instances;

    uint8_t vertex_count;
    size_t working_data_stride;

    uint32_t vertex_buffer_count;
    struct captured_vertex_buffer* vertex_buffers;
};

struct captured_framebuffer_clear {
    image_pixel* clear_values;
};

typedef enum capture_event_type {
    CAPTURE_EVENT_TYPE_RENDER_CALL,
    CAPTURE_EVENT_TYPE_FRAMEBUFFER_CLEAR,
} capture_event_type;

struct capture_event {
    capture_event_type type;
    uint32_t attachment_count;
    image_t** results;

    union {
        struct captured_render_call* render_call;
        struct captured_framebuffer_clear fb_clear;
    };
};

typedef struct capture capture_t;

capture_t* capture_new();
void capture_destroy(capture_t* cap);

// capture takes ownership of data! im too lazy to write copying code lmfao
// all pointers should be separate chunks allocated with mem_alloc
void capture_add_render_call(capture_t* cap, const struct framebuffer* fb,
                             struct captured_render_call* data);

void capture_add_framebuffer_clear(capture_t* cap, const struct framebuffer* fb,
                                   const image_pixel* clear_values);

uint32_t capture_get_events(const capture_t* cap, const struct capture_event** events);

#endif
