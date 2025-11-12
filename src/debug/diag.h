#ifndef DIAG_H_
#define DIAG_H_

#include <stdbool.h>

// from capture.h
typedef struct capture capture_t;

void diag_init();
void diag_shutdown();

void diag_update();

capture_t* diag_current_capture();

#endif
