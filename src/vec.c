#include "vec.h"

#include <math.h>

void vec_add(const float* lhs, const float* rhs, size_t count, float* result) {
    for (size_t i = 0; i < count; i++) {
        result[i] = lhs[i] + rhs[i];
    }
}

void vec_sub(const float* lhs, const float* rhs, size_t count, float* result) {
    float neg_rhs[count];

    vec_mult(rhs, -1.f, count, neg_rhs);
    vec_add(lhs, neg_rhs, count, result);
}

void vec_mult(const float* lhs, float rhs, size_t count, float* result) {
    for (size_t i = 0; i < count; i++) {
        result[i] = lhs[i] * rhs;
    }
}

void vec_rot90(const float* src, float* dst, bool negative) {
    dst[0] = src[1] * (negative ? 1.f : -1.f);
    dst[1] = src[0] * (negative ? -1.f : 1.f);
}

float vec_dot(const float* lhs, const float* rhs, size_t count) {
    float result = 0.f;

    for (size_t i = 0; i < count; i++) {
        result += lhs[i] * rhs[i];
    }

    return result;
}

float vec_length(const float* vec, size_t count) {
    float length2 = vec_dot(vec, vec, count);
    return sqrt(length2);
}
