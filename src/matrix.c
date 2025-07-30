#include "matrix.h"

// Helper macro for mat4 indexing (column-major: m[col*4 + row])
#define MAT4_IDX(row, col) ((col) * 4 + (row))

// VEC2
vec2 vec2_add(vec2 a, vec2 b) { return (vec2){a.x + b.x, a.y + b.y}; }
vec2 vec2_sub(vec2 a, vec2 b) { return (vec2){a.x - b.x, a.y - b.y}; }
vec2 vec2_scale(vec2 v, float s) { return (vec2){v.x * s, v.y * s}; }
vec2 vec2_normalize(vec2 v) {
    float len = sqrtf(v.x * v.x + v.y * v.y);
    if (len > 0.0f) {
        return vec2_scale(v, 1.0f / len);
    }
    return v;  // zero vector case
}
float vec2_dot(vec2 a, vec2 b) { return a.x * b.x + a.y * b.y; }
float vec2_cross(vec2 a, vec2 b) { return a.x * b.y - a.y * b.x; }

// VEC3
vec3 vec3_add(vec3 a, vec3 b) {
    return (vec3){a.x + b.x, a.y + b.y, a.z + b.z};
}
vec3 vec3_sub(vec3 a, vec3 b) {
    return (vec3){a.x - b.x, a.y - b.y, a.z - b.z};
}
vec3 vec3_scale(vec3 v, float s) { return (vec3){v.x * s, v.y * s, v.z * s}; }
vec3 vec3_normalize(vec3 v) {
    float len = sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
    if (len > 0.0f) {
        return vec3_scale(v, 1.0f / len);
    }
    return v;  // zero vector case
}
float vec3_dot(vec3 a, vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
vec3 vec3_cross(vec3 a, vec3 b) {
    return (vec3){a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z,
                  a.x * b.y - a.y * b.x};
}

// MAT4
void mat4_identity(mat4 m) {
    for (int i = 0; i < 16; i++) m[i] = 0.0f;
    m[MAT4_IDX(0, 0)] = m[MAT4_IDX(1, 1)] = m[MAT4_IDX(2, 2)] =
        m[MAT4_IDX(3, 3)]                 = 1.0f;
}
void mat4_translate(mat4 m, vec3 v) {
    mat4 trans;
    mat4_identity(trans);
    trans[MAT4_IDX(0, 3)] = v.x;
    trans[MAT4_IDX(1, 3)] = v.y;
    trans[MAT4_IDX(2, 3)] = v.z;
    mat4_multiply(m, m, trans);  // post-multiply
}
void mat4_rotate_x(mat4 m, float angle_rad) {
    float c = cosf(angle_rad), s = sinf(angle_rad);
    mat4 rot;
    mat4_identity(rot);
    rot[MAT4_IDX(1, 1)] = c;
    rot[MAT4_IDX(1, 2)] = -s;
    rot[MAT4_IDX(2, 1)] = s;
    rot[MAT4_IDX(2, 2)] = c;
    mat4_multiply(m, m, rot);
}
void mat4_rotate_y(mat4 m, float angle_rad) {
    float c = cosf(angle_rad), s = sinf(angle_rad);
    mat4 rot;
    mat4_identity(rot);
    rot[MAT4_IDX(0, 0)] = c;
    rot[MAT4_IDX(0, 2)] = s;
    rot[MAT4_IDX(2, 0)] = -s;
    rot[MAT4_IDX(2, 2)] = c;
    mat4_multiply(m, m, rot);
}
void mat4_rotate_z(mat4 m, float angle_rad) {
    float c = cosf(angle_rad), s = sinf(angle_rad);
    mat4 rot;
    mat4_identity(rot);
    rot[MAT4_IDX(0, 0)] = c;
    rot[MAT4_IDX(0, 1)] = -s;
    rot[MAT4_IDX(1, 0)] = s;
    rot[MAT4_IDX(1, 1)] = c;
    mat4_multiply(m, m, rot);
}
void mat4_scale(mat4 m, vec3 v) {
    mat4 scale;
    mat4_identity(scale);
    scale[MAT4_IDX(0, 0)] = v.x;
    scale[MAT4_IDX(1, 1)] = v.y;
    scale[MAT4_IDX(2, 2)] = v.z;
    mat4_multiply(m, m, scale);
}
void mat4_multiply(mat4 out, mat4 a, mat4 b) {
    mat4 temp;
    for (int row = 0; row < 4; row++) {
        for (int col = 0; col < 4; col++) {
            temp[MAT4_IDX(row, col)] = 0.0f;
            for (int k = 0; k < 4; k++) {
                temp[MAT4_IDX(row, col)] +=
                    a[MAT4_IDX(row, k)] * b[MAT4_IDX(k, col)];
            }
        }
    }
    for (int i = 0; i < 16; i++) out[i] = temp[i];
}
void mat4_perspective(
    mat4 m, float fov_rad, float aspect, float near, float far
) {
    float tan_half_fov = tanf(fov_rad / 2.0f);
    float focal        = 1.0f / tan_half_fov;
    mat4_identity(m);
    m[MAT4_IDX(0, 0)] = focal / aspect;
    m[MAT4_IDX(1, 1)] = -focal;
    m[MAT4_IDX(2, 2)] = -far / (far - near);
    m[MAT4_IDX(2, 3)] = -(far * near) / (far - near);
    m[MAT4_IDX(3, 2)] = -1.0f;
    m[MAT4_IDX(3, 3)] = 0.0f;
}
void mat4_look_at(mat4 m, vec3 eye, vec3 center, vec3 up) {
    vec3 f = vec3_normalize(vec3_sub(center, eye));
    vec3 s = vec3_normalize(vec3_cross(f, up));
    vec3 u = vec3_cross(s, f);
    mat4_identity(m);
    m[MAT4_IDX(0, 0)] = s.x;
    m[MAT4_IDX(0, 1)] = u.x;
    m[MAT4_IDX(0, 2)] = -f.x;
    m[MAT4_IDX(1, 0)] = s.y;
    m[MAT4_IDX(1, 1)] = u.y;
    m[MAT4_IDX(1, 2)] = -f.y;
    m[MAT4_IDX(2, 0)] = s.z;
    m[MAT4_IDX(2, 1)] = u.z;
    m[MAT4_IDX(2, 2)] = -f.z;
    m[MAT4_IDX(0, 3)] = -vec3_dot(s, eye);
    m[MAT4_IDX(1, 3)] = -vec3_dot(u, eye);
    m[MAT4_IDX(2, 3)] = vec3_dot(f, eye);  // note the sign flip
}