#include "image.h"

#include "mem.h"

#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>

#include <glib.h>

static SDL_PixelFormat image_get_pixel_format(image_format format) {
    switch (format) {
    case IMAGE_FORMAT_COLOR:
        return SDL_PIXELFORMAT_RGBA8888;
    default:
        return SDL_PIXELFORMAT_UNKNOWN;
    }
}

static size_t image_get_pixel_format_stride(SDL_PixelFormat format) {
    const SDL_PixelFormatDetails* details = SDL_GetPixelFormatDetails(format);
    if (!details) {
        return 0;
    }

    return details->bytes_per_pixel;
}

image_t* image_allocate(uint32_t width, uint32_t height, image_format format) {
    size_t pixel_stride = 0;
    SDL_PixelFormat pixel_format = SDL_PIXELFORMAT_UNKNOWN;

    switch (format) {
    case IMAGE_FORMAT_DEPTH:
        pixel_stride = 4;
        break;
    default:
        pixel_format = image_get_pixel_format(format);
        g_assert(pixel_format != SDL_PIXELFORMAT_UNKNOWN);

        pixel_stride = image_get_pixel_format_stride(pixel_format);
        break;
    }

    void* pixels;
    SDL_Surface* surface;

    if (pixel_format != SDL_PIXELFORMAT_UNKNOWN) {
        surface = SDL_CreateSurface((int)width, (int)height, pixel_format);
        if (!surface) {
            return NULL;
        }

        if (!surface->pixels) {
            SDL_DestroySurface(surface);
            return NULL;
        }

        pixels = surface->pixels;
    } else {
        surface = NULL;

        size_t data_size = width * height * pixel_stride;
        pixels = mem_alloc(data_size);
    }

    image_t* image = mem_alloc(sizeof(image_t));
    image->data = pixels;
    image->width = width;
    image->height = height;
    image->format = format;
    image->pixel_stride = pixel_stride;
    image->surface = surface;

    return image;
}

void image_free(image_t* image) {
    if (!image) {
        return;
    }

    if (image->surface) {
        SDL_DestroySurface(image->surface);
    } else {
        mem_free(image->data);
    }

    mem_free(image);
}

uint32_t image_get_pixel_index(const image_t* image, uint32_t x, uint32_t y) {
    return y * image->width + x;
}
