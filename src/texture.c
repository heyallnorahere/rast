#include "texture.h"

#include "image.h"
#include "util.h"

#include <string.h>
#include <math.h>

#include <glib.h>

uint32_t texture_get_channels(const struct texture* texture) {
    switch (texture->image->format) {
    case IMAGE_FORMAT_COLOR:
        return 4;
    case IMAGE_FORMAT_DEPTH:
        return 1;
    default:
        return 0;
    }
}

static void texture_get_pixel(const image_t* image, uint32_t x, uint32_t y, float* data) {
    uint32_t index = image_get_pixel_index(image, x, y);
    size_t pixel_offset = index * image->pixel_stride;

    image_pixel pixel_data;
    memcpy(&pixel_data, image->data + pixel_offset, image->pixel_stride);

    switch (image->format) {
    case IMAGE_FORMAT_COLOR:
        util_u32_to_float4(pixel_data.color, data);
        break;
    case IMAGE_FORMAT_DEPTH:
        data[0] = pixel_data.depth;
        break;
    }
}

static void texture_sample_linear(const struct texture* texture, const float* uv, float* sample) {
    float x_f = uv[0] * (texture->image->width - 1);
    float y_f = uv[1] * (texture->image->height - 1);

    uint32_t x0 = (uint32_t)floor(x_f);
    uint32_t y0 = (uint32_t)floor(y_f);

    uint32_t x1 = (uint32_t)ceil(x_f);
    uint32_t y1 = (uint32_t)ceil(y_f);

    uint32_t channels = texture_get_channels(texture);
    memset(sample, 0, channels * sizeof(float));

    for (uint32_t i = 0; i < 4; i++) {
        uint32_t x = ((i & 0x1) != 0) ? x0 : x1;
        uint32_t y = (((i >> 1) & 0x1) != 0) ? y0 : y1;

        float delta_x = x_f - x;
        float delta_y = y_f - y;

        float current_sample[4];
        texture_get_pixel(texture->image, x, y, current_sample);

        float weight = (1.f - fabsf(delta_x)) * (1.f - fabsf(delta_y));
        for (uint32_t j = 0; j < channels; j++) {
            sample[j] += current_sample[j] * weight;
        }
    }
}

static void texture_sample_nearest(const struct texture* texture, const float* uv, float* sample) {
    float x_f = uv[0] * (texture->image->width - 1);
    float y_f = uv[1] * (texture->image->height - 1);

    uint32_t x = (uint32_t)round(x_f);
    uint32_t y = (uint32_t)round(y_f);

    texture_get_pixel(texture->image, x, y, sample);
}

void texture_sample(const struct texture* texture, const float* uv, float* sample) {
    float corrected_uv[2];

    // todo: if wrapping, correct uv
    memcpy(corrected_uv, uv, 2 * sizeof(float));

    switch (texture->sampler->filter) {
    case SAMPLER_FILTER_LINEAR:
        texture_sample_linear(texture, uv, sample);
        break;
    case SAMPLER_FILTER_NEAREST:
        texture_sample_nearest(texture, uv, sample);
        break;
    }
}
