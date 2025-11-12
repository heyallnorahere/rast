#ifndef TEXTURE_H_
#define TEXTURE_H_

#include <stdint.h>

typedef enum {
    SAMPLER_FILTER_NEAREST,
    SAMPLER_FILTER_LINEAR,
} sampler_filter;

typedef enum {
    SAMPLER_WRAPPING_REPEAT,
    SAMPLER_WRAPPING_CLAMP_TO_EDGE,
} sampler_wrapping;

struct sampler {
    sampler_filter filter;
    sampler_wrapping wrapping;
};

// from image.h
typedef struct image image_t;

struct texture {
    const image_t* image;
    const struct sampler* sampler;
};

uint32_t texture_get_channels(const struct texture* texture);

// 2d uv coordinates.
// outputs into sample, number of channels dependent on image format (retrieve with
// texture_get_channels)
void texture_sample(const struct texture* texture, const float* uv, float* sample);

#endif
