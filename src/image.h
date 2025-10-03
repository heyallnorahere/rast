#ifndef IMAGE_H_
#define IMAGE_H_

#include <stdint.h>
#include <stddef.h>

typedef enum {
    IMAGE_FORMAT_COLOR,
} image_format;

typedef struct image {
    void* data;

    uint32_t width, height;
    image_format format;

    void* surface;
} image_t;

image_t* image_allocate(uint32_t width, uint32_t height, image_format format);
void image_free(image_t* image);

size_t image_get_pixel_stride(image_format format);

#endif
