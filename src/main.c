#include <glib.h>

#include "window.h"

int main(int argc, const char** argv) {
    window_t* window;

    window = window_create("rast", 1600, 900);
    while (!window_is_close_requested(window)) {
        window_poll();
    }

    window_destroy(window);
    return 0;
}
