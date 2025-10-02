#ifndef WINDOW_H_
#define WINDOW_H_

#include <stdbool.h>
#include <stdint.h>

typedef struct window window_t;

window_t* window_create(const char* title, uint32_t width, uint32_t height);
void window_destroy(window_t* window);

void window_poll();
bool window_is_close_requested(window_t* window);

#endif
