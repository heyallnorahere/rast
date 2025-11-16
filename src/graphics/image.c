#include "image.h"

#include "core/mem.h"

image_t* image_allocate(uint32_t width, uint32_t height, image_format format) {
    size_t pixel_stride = 0;

    switch (format) {
    case IMAGE_FORMAT_COLOR:
        pixel_stride = sizeof(uint32_t);
        break;
    case IMAGE_FORMAT_DEPTH:
        pixel_stride = sizeof(float);
        break;
    }

    size_t data_size = width * height * pixel_stride;
    void* pixels = mem_alloc(data_size);

    image_t* image = mem_alloc(sizeof(image_t));
    image->data = pixels;
    image->width = width;
    image->height = height;
    image->format = format;
    image->pixel_stride = pixel_stride;

    return image;
}

void image_free(image_t* image) {
    if (!image) {
        return;
    }

    mem_free(image->data);
    mem_free(image);
}

uint32_t image_get_pixel_index(const image_t* image, uint32_t x, uint32_t y) {
    return y * image->width + x;
}
