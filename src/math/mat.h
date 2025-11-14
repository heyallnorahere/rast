#ifndef MAT_H_
#define MAT_H_

#include <stdint.h>

void mat_identity(float* mat, uint32_t size);

void mat_add(const float* lhs, const float* rhs, uint32_t m, uint32_t n, float* result);
void mat_mult(const float* mat, float scalar, uint32_t m, uint32_t n, float* result);

uint32_t mat_offset(uint32_t columns, uint32_t r, uint32_t c);

void mat_dot(const float* lhs, const float* rhs, uint32_t m, uint32_t x, uint32_t n, float* result);

void mat_transpose(const float* mat, uint32_t src_rows, uint32_t src_columns, float* result);

// 4x4 matrices
void mat_perspective(float* mat, float vfov, float aspect, float near, float far);
void mat_look_at(float* mat, const float* eye, const float* center, const float* up);

#endif
