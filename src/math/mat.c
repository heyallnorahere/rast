#include "mat.h"

#include "math/vec.h"

#include <string.h>

#include <math.h>

void mat_identity(float* mat, uint32_t size) {
    memset(mat, 0, size * size * sizeof(float));

    for (uint32_t i = 0; i < size; i++) {
        // mat[i, i]
        uint32_t index = i * (size + 1);

        mat[index] = 1.f;
    }
}

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

// right-handed, zero-to-one depth
// however, GLM IS COLUMN MAJOR
// our matrices are ROW MAJOR

// https://github.com/g-truc/glm/blob/master/glm/ext/matrix_clip_space.inl#L233
void mat_perspective(float* mat, float vfov, float aspect, float near, float far) {
    memset(mat, 0, 4 * 4 * sizeof(float));

    /*
        mat<4, 4, T, defaultp> Result(static_cast<T>(0));
        Result[0][0] = static_cast<T>(1) / (aspect * tanHalfFovy);
        Result[1][1] = static_cast<T>(1) / (tanHalfFovy);
        Result[2][2] = zFar / (zNear - zFar);
        Result[2][3] = - static_cast<T>(1);
        Result[3][2] = -(zFar * zNear) / (zFar - zNear);
    */

    float tan_half_vfov = tan(vfov / 2.f);

    // mat[0, 0]
    mat[0] = 1.f / (aspect * tan_half_vfov);

    // mat[1, 1]
    mat[5] = 1.f / tan_half_vfov;

    // mat[2, 2]
    mat[10] = far / (near - far);

    // mat[2, 3]
    mat[11] = -(far * near) / (far - near);

    // mat[3, 2]
    mat[14] = -1.f;
}

// https://github.com/g-truc/glm/blob/master/glm/ext/matrix_transform.inl#L153
void mat_look_at(float* mat, const float* eye, const float* center, const float* up) {
    mat_identity(mat, 4);

    float f_u[3];
    vec_sub(center, eye, 3, f_u);

    float f[3];
    vec_normalize(f_u, f, 3);

    float s_u[3];
    vec_cross(f, up, s_u);

    float s[3];
    vec_normalize(s_u, s, 3);

    float u[3];
    vec_cross(s, f, u);

    // row 1 is side
    memcpy(mat, s, 3 * sizeof(float));

    // row 2 is up
    memcpy(mat + 4, u, 3 * sizeof(float));

    // row 3 is inverted front
    vec_mult(f, -1.f, 3, mat + 8);

    // mat[0, 3]
    mat[3] = -vec_dot(s, eye, 3);

    // mat[1, 3]
    mat[7] = -vec_dot(u, eye, 3);

    // mat[2, 3]
    mat[11] = vec_dot(f, eye, 3);
}
