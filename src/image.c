#include "image.h"

#include "mem.h"

#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>

static SDL_PixelFormat image_get_pixel_format(image_format format) {
    switch (format) {
    case IMAGE_FORMAT_COLOR:
        return SDL_PIXELFORMAT_RGBA8888;
    default:
        return SDL_PIXELFORMAT_UNKNOWN;
    }
}

image_t* image_allocate(uint32_t width, uint32_t height, image_format format) {
    SDL_PixelFormat pixel_format = image_get_pixel_format(format);
    SDL_Surface* surface = SDL_CreateSurface((int)width, (int)height, pixel_format);

    if (!surface) {
        return NULL;
    }

    if (!surface->pixels) {
        SDL_DestroySurface(surface);
        return NULL;
    }

    image_t* image = mem_alloc(sizeof(image_t));
    image->surface = surface;

    image->data = surface->pixels;
    image->width = width;
    image->height = height;
    image->format = format;

    return image;
}

void image_free(image_t* image) {
    if (!image) {
        return;
    }

    SDL_DestroySurface(image->surface);
    mem_free(image);
}

size_t image_get_pixel_stride(image_format format) {
    SDL_PixelFormat pixel_format = image_get_pixel_format(format);
    if (pixel_format == SDL_PIXELFORMAT_UNKNOWN) {
        return 0;
    }

    const SDL_PixelFormatDetails* details = SDL_GetPixelFormatDetails(pixel_format);
    if (!details) {
        return 0;
    }

    return details->bytes_per_pixel;
}
