#include "vec.h"

#include <math.h>

#include "math/mat.h"

void vec_add(const float* lhs, const float* rhs, uint32_t count, float* result) {
    mat_add(lhs, rhs, count, 1, result);
}

void vec_sub(const float* lhs, const float* rhs, uint32_t count, float* result) {
    float neg_rhs[count];

    vec_mult(rhs, -1.f, count, neg_rhs);
    vec_add(lhs, neg_rhs, count, result);
}

void vec_mult(const float* lhs, float rhs, uint32_t count, float* result) {
    mat_mult(lhs, rhs, count, 1, result);
}

void vec_rot90(const float* src, float* dst, bool negative) {
    dst[0] = src[1] * (negative ? 1.f : -1.f);
    dst[1] = src[0] * (negative ? -1.f : 1.f);
}

float vec_dot(const float* lhs, const float* rhs, uint32_t count) {
    float result;
    mat_dot(lhs, rhs, 1, count, 1, &result);

    return result;
}

float vec_length(const float* vec, uint32_t count) {
    float length2 = vec_dot(vec, vec, count);
    return sqrt(length2);
}

void vec_normalize(const float* vec, float* result, uint32_t count) {
    float length = vec_length(vec, count);

    for (uint32_t i = 0; i < count; i++) {
        result[i] = vec[i] / length;
    }
}

void vec_cross(const float* a, const float* b, float* result) {
    result[0] = a[1] * b[2] - a[2] * b[1];
    result[1] = a[2] * b[0] - a[0] * b[2];
    result[2] = a[0] * b[1] - a[1] * b[0];
}
