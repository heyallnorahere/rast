#ifndef IMAGE_H_
#define IMAGE_H_

#include <stdint.h>
#include <stddef.h>

typedef union image_pixel {
    uint32_t color;
    float depth;
} image_pixel;

typedef enum {
    IMAGE_FORMAT_COLOR,
    IMAGE_FORMAT_DEPTH,
} image_format;

typedef struct image {
    void* data;

    uint32_t width, height;
    image_format format;
    size_t pixel_stride;
} image_t;

image_t* image_allocate(uint32_t width, uint32_t height, image_format format);
void image_free(image_t* image);

uint32_t image_get_pixel_index(const image_t* image, uint32_t x, uint32_t y);

#endif
