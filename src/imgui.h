#ifndef RAST_IMGUI_H_
#define RAST_IMGUI_H_

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include <cimgui.h>

typedef struct imgui imgui_t;

imgui_t* imgui_init();
void imgui_destroy(imgui_t* instance);

void imgui_make_current(const imgui_t* instance);

#endif
