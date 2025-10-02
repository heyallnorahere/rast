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

        size_t data_size = backbuffer->width * backbuffer->height * 4;
        memset(backbuffer->data, 0xFF, data_size);

        if (!window_swap_buffers(window)) {
            success = false;
            break;
        }

        window_poll();
    }

    window_destroy(window);
    return success ? 0 : 1;
}
