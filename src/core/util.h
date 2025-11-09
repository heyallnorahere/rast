#ifndef UTIL_H_
#define UTIL_H_

#include <stdint.h>

uint32_t util_float4_to_u32(const float* data);
void util_u32_to_float4(uint32_t value, float* data);

#endif
