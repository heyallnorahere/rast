#include "util.h"

uint32_t util_float4_to_u32(const float *data) {
    uint32_t result = 0;
    for (uint32_t i = 0; i < 4; i++) {
        float element = data[i];
        if (element < 0.f) {
            element = 0.f;
        }

        if (element > 1.f) {
            element = 1.f;
        }

        uint8_t byte = (uint8_t)(element * 0xFF);
        result |= (uint32_t)byte << ((3 - i) * 8);
    }

    return result;
}

void util_u32_to_float4(uint32_t value, float *data) {
    for (uint32_t i = 0; i < 4; i++) {
        uint8_t byte = (value >> ((3 - i) * 8)) & 0xFF;
        data[i] = (float)byte / (float)0xFF;
    }
}
