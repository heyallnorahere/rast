#include "mat.h"

void mat_add(const float* lhs, const float* rhs, uint32_t m, uint32_t n, float* result) {
    for (uint32_t r = 0; r < m; r++) {
        for (uint32_t c = 0; c < n; c++) {
            uint32_t offset = mat_offset(n, r, c);

            result[offset] = lhs[offset] + rhs[offset];
        }
    }
}

void mat_mult(const float* mat, float scalar, uint32_t m, uint32_t n, float* result) {
    for (uint32_t r = 0; r < m; r++) {
        for (uint32_t c = 0; c < n; c++) {
            uint32_t offset = mat_offset(n, r, c);

            result[offset] = mat[offset] * scalar;
        }
    }
}

uint32_t mat_offset(uint32_t columns, uint32_t r, uint32_t c) { return r * columns + c; }

void mat_dot(const float* lhs, const float* rhs, uint32_t m, uint32_t x, uint32_t n,
             float* result) {
    for (uint32_t r = 0; r < m; r++) {
        for (uint32_t c = 0; c < n; c++) {
            float value = 0.f;

            for (uint32_t i = 0; i < x; i++) {
                uint32_t lhs_offset = mat_offset(x, r, i);
                uint32_t rhs_offset = mat_offset(n, i, c);

                value += lhs[lhs_offset] * rhs[rhs_offset];
            }

            uint32_t result_offset = mat_offset(n, r, c);
            result[result_offset] = value;
        }
    }
}
