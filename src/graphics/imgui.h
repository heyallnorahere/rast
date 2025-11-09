#ifndef RAST_IMGUI_H_
#define RAST_IMGUI_H_

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include <cimgui.h>

#include <stdbool.h>

// from rasterizer.h
struct framebuffer;

// from rasterizer.h
typedef struct rasterizer rasterizer_t;

void imgui_set_allocators();

void imgui_init_renderer(rasterizer_t* rast);
void imgui_shutdown_renderer();

void imgui_render(ImDrawData* data, struct framebuffer* fb);

#endif
