#ifndef RAST_IMGUI_H_
#define RAST_IMGUI_H_

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include <cimgui.h>

#include <stdbool.h>

// from rasterizer.h
struct framebuffer;

void imgui_set_allocators();

void imgui_init_renderer();
void imgui_shutdown_renderer();

void imgui_render(ImDrawData* data, struct framebuffer* fb);

#endif
