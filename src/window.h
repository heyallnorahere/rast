#ifndef WINDOW_H_
#define WINDOW_H_

#include <stdbool.h>
#include <stdint.h>

// in rgba8 format, 4 bytes per pixel
typedef struct backbuffer {
    void* data;
    uint32_t width, height;

    void* surface;
} backbuffer_t;

typedef struct window window_t;

window_t* window_create(const char* title, uint32_t width, uint32_t height);
void window_destroy(window_t* window);

bool window_get_framebuffer_size(window_t* window, uint32_t* width, uint32_t* height);

void window_poll();
bool window_is_close_requested(window_t* window);

backbuffer_t* window_get_backbuffer(window_t* window);
bool window_swap_buffers(window_t* window);

#endif
