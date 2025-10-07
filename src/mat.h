#ifndef MAT_H_
#define MAT_H_

#include <stdint.h>

void mat_add(const float* lhs, const float* rhs, uint32_t m, uint32_t n, float* result);
void mat_mult(const float* mat, float scalar, uint32_t m, uint32_t n, float* result);

uint32_t mat_offset(uint32_t columns, uint32_t r, uint32_t c);

void mat_dot(const float* lhs, const float* rhs, uint32_t m, uint32_t x, uint32_t n, float* result);

#endif
