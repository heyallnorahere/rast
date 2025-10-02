#include <glib.h>

#include "window.h"

int main(int argc, const char** argv) {
    window_t* window = NULL;
    bool success = true;

    window = window_create("rast", 1600, 900);
    while (!window_is_close_requested(window)) {
        backbuffer_t* backbuffer = window_get_backbuffer(window);
        if (!backbuffer) {
            success = false;
            break;
        }

        uint32_t* pixels = (uint32_t*)backbuffer->data;
        for (uint32_t i = 0; i < backbuffer->height; i++) {
            for (uint32_t j = 0; j < backbuffer->width; j++) {
                uint32_t offset = j + i * backbuffer->width;
                pixels[offset] = 0x00FF00FF;
            }
        }

        if (!window_swap_buffers(window)) {
            success = false;
            break;
        }

        window_poll();
    }

    window_destroy(window);
    return success ? 0 : 1;
}
