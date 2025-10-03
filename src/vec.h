#ifndef VEC_H_
#define VEC_H_

#include <stddef.h>
#include <stdbool.h>

void vec_add(const float* lhs, const float* rhs, size_t count, float* result);
void vec_sub(const float* lhs, const float* rhs, size_t count, float* result);
void vec_mult(const float* lhs, float rhs, size_t count, float* result);

// assumes 2d vector
void vec_rot90(const float* src, float* dst, bool negative);

float vec_dot(const float* lhs, const float* rhs, size_t count);
float vec_length(const float* vec, size_t count);

#endif
