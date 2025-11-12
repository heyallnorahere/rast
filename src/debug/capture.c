#include "capture.h"

#include "core/mem.h"
#include "core/list.h"

#include "graphics/image.h"
#include "graphics/rasterizer.h"

#include <string.h>

// capture_t
struct capture {
    // of type struct capture_event
    struct list events;

    // i dont know if we need more
};

capture_t* capture_new() {
    capture_t* cap = mem_alloc(sizeof(capture_t));
    list_init(&cap->events);

    return cap;
}

static void capture_cleanup_render_call(struct captured_render_call* rc) {
    for (uint32_t i = 0; i < rc->instance_count; i++) {
        struct captured_instance* instance = &rc->instances[i];

        for (uint32_t j = 0; j < rc->primitive_count; j++) {
            struct captured_primitive* primitive = &instance->primitives[j];

            mem_free(primitive->working_data);
            mem_free(primitive->indices);
            mem_free(primitive->vertex_positions);
        }

        mem_free(instance->primitives);
    }

    for (uint32_t i = 0; i < rc->vertex_buffer_count; i++) {
        struct captured_vertex_buffer* buffer = &rc->vertex_buffers[i];
        mem_free(buffer->data);
    }

    mem_free(rc->instances);
    mem_free(rc->vertex_buffers);
    mem_free(rc);
}

static void capture_destroy_event(void* data, void* user_data) {
    struct capture_event* event = data;
    struct capture* cap = user_data;

    switch (event->type) {
    case CAPTURE_EVENT_TYPE_RENDER_CALL:
        capture_cleanup_render_call(event->render_call);
        break;
    case CAPTURE_EVENT_TYPE_FRAMEBUFFER_CLEAR:
        mem_free(event->fb_clear.clear_values);
        break;
    }

    for (uint32_t i = 0; i < event->attachment_count; i++) {
        image_free(event->results[i]);
    }

    mem_free(event->results);
    mem_free(event);
}

void capture_destroy(capture_t* cap) {
    if (!cap) {
        return;
    }

    list_free_full(&cap->events, capture_destroy_event, cap);
    mem_free(cap);
}

static void capture_take_snapshot(struct capture_event* ev, const struct framebuffer* fb) {
    ev->attachment_count = fb->attachment_count;
    ev->results = mem_alloc(sizeof(image_t*) * ev->attachment_count);

    for (uint32_t i = 0; i < ev->attachment_count; i++) {
        image_t* attachment = fb->attachments[i];
        image_t* snapshot =
            image_allocate(attachment->width, attachment->height, attachment->format);

        size_t data_size = snapshot->pixel_stride * snapshot->width * snapshot->height;
        memcpy(snapshot->data, attachment->data, data_size);

        ev->results[i] = snapshot;
    }
}

static struct capture_event* capture_create_event(capture_t* cap, capture_event_type type,
                                                  const struct framebuffer* fb) {
    struct capture_event* ev = mem_alloc(sizeof(struct capture_event));
    ev->type = type;

    if (fb) {
        capture_take_snapshot(ev, fb);
    } else {
        ev->attachment_count = 0;
        ev->results = NULL;
    }

    list_append(&cap->events, ev);
    return ev;
}

void capture_add_render_call(capture_t* cap, const struct framebuffer* fb,
                             struct captured_render_call* data) {
    struct capture_event* ev = capture_create_event(cap, CAPTURE_EVENT_TYPE_RENDER_CALL, fb);
    ev->render_call = data;
}

void capture_add_framebuffer_clear(capture_t* cap, const struct framebuffer* fb,
                                   const image_pixel* clear_values) {
    struct capture_event* ev = capture_create_event(cap, CAPTURE_EVENT_TYPE_FRAMEBUFFER_CLEAR, fb);
    
    size_t buf_size = fb->attachment_count * sizeof(image_pixel);
    ev->fb_clear.clear_values = mem_alloc(buf_size);
    memcpy(ev->fb_clear.clear_values, clear_values, buf_size);
}

uint32_t capture_get_events(const capture_t* cap, const struct capture_event** events) {
    uint32_t count = 0;

    for (struct list_node* current = cap->events.head; current != NULL; current = current->next) {
        if (events) {
            events[count] = current->data;
        }

        count++;
    }

    return count;
}
