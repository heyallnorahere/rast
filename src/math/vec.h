#ifndef VEC_H_
#define VEC_H_

#include <stdint.h>
#include <stdbool.h>

void vec_add(const float* lhs, const float* rhs, uint32_t count, float* result);
void vec_sub(const float* lhs, const float* rhs, uint32_t count, float* result);
void vec_mult(const float* lhs, float rhs, uint32_t count, float* result);

// assumes 2d vector
void vec_rot90(const float* src, float* dst, bool negative);

float vec_dot(const float* lhs, const float* rhs, uint32_t count);
float vec_length(const float* vec, uint32_t count);

void vec_normalize(const float* vec, float* result, uint32_t count);

// assumes 3d vector
void vec_cross(const float* a, const float* b, float* result);

#endif
