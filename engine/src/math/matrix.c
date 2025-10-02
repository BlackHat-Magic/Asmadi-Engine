#include "math/matrix.h"

#include <math.h>
#include <stdlib.h>
#include <time.h>

// VEC2
vec2 vec2_add (vec2 a, vec2 b) {
    return (vec2) {a.x + b.x, a.y + b.y};
}
vec2 vec2_sub (vec2 a, vec2 b) {
    return (vec2) {a.x - b.x, a.y - b.y};
}
vec2 vec2_scale (vec2 v, float s) {
    return (vec2) {v.x * s, v.y * s};
}
vec2 vec2_normalize (vec2 v) {
    float len = sqrtf (v.x * v.x + v.y * v.y);
    if (len > 0.0f) {
        return vec2_scale (v, 1.0f / len);
    }
    return v; // zero vector case
}
float vec2_dot (vec2 a, vec2 b) {
    return a.x * b.x + a.y * b.y;
}
float vec2_cross (vec2 a, vec2 b) {
    return a.x * b.y - a.y * b.x;
}

// VEC3
vec3 vec3_add (vec3 a, vec3 b) {
    return (vec3) {a.x + b.x, a.y + b.y, a.z + b.z};
}
vec3 vec3_sub (vec3 a, vec3 b) {
    return (vec3) {a.x - b.x, a.y - b.y, a.z - b.z};
}
vec3 vec3_scale (vec3 v, float s) {
    return (vec3) {v.x * s, v.y * s, v.z * s};
}
vec3 vec3_normalize (vec3 v) {
    float len = sqrtf (v.x * v.x + v.y * v.y + v.z * v.z);
    if (len > 0.0f) {
        return vec3_scale (v, 1.0f / len);
    }
    return v; // zero vector case
}
float vec3_dot (vec3 a, vec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}
vec3 vec3_cross (vec3 a, vec3 b) {
    return (vec3) {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z,
                   a.x * b.y - a.y * b.x};
}

// VEC4
vec4 vec4_add (vec4 a, vec4 b) {
    return (vec4) {a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w};
}
vec4 vec4_sub (vec4 a, vec4 b) {
    return (vec4) {a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w};
}
vec4 vec4_scale (vec4 v, float s) {
    return (vec4) {v.x * s, v.y * s, v.z * s, v.w * s};
}
vec4 vec4_normalize (vec4 v) {
    float len = sqrtf (v.x * v.x + v.y * v.y + v.z * v.z + v.w * v.w);
    if (len > 0.0f) {
        return vec4_scale (v, 1.0f / len);
    }
    return v;
}
float vec4_dot (vec4 a, vec4 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
}

// Quaternion helpers
vec4 quat_from_euler (vec3 euler) {
    float cx = cosf (euler.x * 0.5f), sx = sinf (euler.x * 0.5f);
    float cy = cosf (euler.y * 0.5f), sy = sinf (euler.y * 0.5f);
    float cz = cosf (euler.z * 0.5f), sz = sinf (euler.z * 0.5f);
    return (vec4) {
        cx * cy * cz + sx * sy * sz, // w
        sx * cy * cz - cx * sy * sz, // x
        cx * sy * cz + sx * cy * sz, // y
        cx * cy * sz - sx * sy * cz  // z
    };
}
vec3 euler_from_quat (vec4 q) {
    // Extract Euler angles (handle singularities)
    float sinr_cosp = 2.0f * (q.w * q.x + q.y * q.z);
    float cosr_cosp = 1.0f - 2.0f * (q.x * q.x + q.y * q.y);
    float pitch = atan2f (sinr_cosp, cosr_cosp);

    float sinp = 2.0f * (q.w * q.y - q.z * q.x);
    float yaw;
    if (fabsf (sinp) >= 1.0f) {
        yaw = copysignf ((float) M_PI / 2.0f, sinp); // Gimbal lock case
    } else {
        yaw = asinf (sinp);
    }

    float siny_cosp = 2.0f * (q.w * q.z + q.x * q.y);
    float cosy_cosp = 1.0f - 2.0f * (q.y * q.y + q.z * q.z);
    float roll = atan2f (siny_cosp, cosy_cosp);

    return (vec3) {pitch, yaw, roll};
}
vec4 quat_multiply (vec4 a, vec4 b) {
    return (vec4) {
        a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z, // w
        a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y, // x
        a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x, // y
        a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w  // z
    };
}
vec4 quat_conjugate (vec4 q) {
    return (vec4) {q.w, -q.x, -q.y, -q.z};
}
vec4 quat_normalize (vec4 q) {
    float len = sqrtf (q.w * q.w + q.x * q.x + q.y * q.y + q.z * q.z);
    if (len > 0.0f) {
        return (vec4) {q.w / len, q.x / len, q.y / len, q.z / len};
    }
    return (vec4) {1.0f, 0.0f, 0.0f, 0.0f}; // Identity quat
}
vec3 quat_rotate (vec4 q, vec3 v) {
    vec4 pv = {0.0f, v.x, v.y, v.z};
    vec4 conj_q = quat_conjugate (q);
    vec4 temp = quat_multiply (q, pv);
    vec4 result = quat_multiply (temp, conj_q);
    return (vec3) {result.x, result.y, result.z};
}
vec4 quat_from_axis_angle (vec3 axis, float angle) {
    float half_angle = angle * 0.5f;
    float c = cosf (half_angle);
    float s = sinf (half_angle);
    axis = vec3_normalize (axis);
    return (vec4) {c, axis.x * s, axis.y * s, axis.z * s};
}
vec3 vec3_rotate (vec4 q, vec3 v) {
    vec4 vq = {0.0f, v.x, v.y, v.z};
    vec4 conj_q = quat_conjugate (q);
    vec4 temp = quat_multiply (q, vq);
    vec4 result = quat_multiply (temp, conj_q);
    return (vec3) {result.x, result.y, result.z};
}

// MAT4
void mat4_identity (mat4 m) {
    for (int i = 0; i < 16; i++)
        m[i] = 0.0f;
    m[MAT4_IDX (0, 0)] = m[MAT4_IDX (1, 1)] = m[MAT4_IDX (2, 2)] =
        m[MAT4_IDX (3, 3)] = 1.0f;
}
void mat4_translate (mat4 m, vec3 v) {
    mat4 trans;
    mat4_identity (trans);
    trans[MAT4_IDX (0, 3)] = v.x;
    trans[MAT4_IDX (1, 3)] = v.y;
    trans[MAT4_IDX (2, 3)] = v.z;
    mat4_multiply (m, m, trans); // post-multiply
}
void mat4_rotate_x (mat4 m, float angle_rad) {
    float c = cosf (angle_rad), s = sinf (angle_rad);
    mat4 rot;
    mat4_identity (rot);
    rot[MAT4_IDX (1, 1)] = c;
    rot[MAT4_IDX (1, 2)] = -s;
    rot[MAT4_IDX (2, 1)] = s;
    rot[MAT4_IDX (2, 2)] = c;
    mat4_multiply (m, m, rot);
}
void mat4_rotate_y (mat4 m, float angle_rad) {
    float c = cosf (angle_rad), s = sinf (angle_rad);
    mat4 rot;
    mat4_identity (rot);
    rot[MAT4_IDX (0, 0)] = c;
    rot[MAT4_IDX (0, 2)] = s;
    rot[MAT4_IDX (2, 0)] = -s;
    rot[MAT4_IDX (2, 2)] = c;
    mat4_multiply (m, m, rot);
}
void mat4_rotate_z (mat4 m, float angle_rad) {
    float c = cosf (angle_rad), s = sinf (angle_rad);
    mat4 rot;
    mat4_identity (rot);
    rot[MAT4_IDX (0, 0)] = c;
    rot[MAT4_IDX (0, 1)] = -s;
    rot[MAT4_IDX (1, 0)] = s;
    rot[MAT4_IDX (1, 1)] = c;
    mat4_multiply (m, m, rot);
}
void mat4_rotate_quat (mat4 m, vec4 q) {
    // Assumes q is normalized; convert quat to rotation matrix and
    // post-multiply
    mat4 rot;
    float xx = q.x * q.x, xy = q.x * q.y, xz = q.x * q.z, xw = q.x * q.w;
    float yy = q.y * q.y, yz = q.y * q.z, yw = q.y * q.w;
    float zz = q.z * q.z, zw = q.z * q.w;

    rot[MAT4_IDX (0, 0)] = 1.0f - 2.0f * (yy + zz);
    rot[MAT4_IDX (0, 1)] = 2.0f * (xy - zw);
    rot[MAT4_IDX (0, 2)] = 2.0f * (xz + yw);
    rot[MAT4_IDX (0, 3)] = 0.0f;

    rot[MAT4_IDX (1, 0)] = 2.0f * (xy + zw);
    rot[MAT4_IDX (1, 1)] = 1.0f - 2.0f * (xx + zz);
    rot[MAT4_IDX (1, 2)] = 2.0f * (yz - xw);
    rot[MAT4_IDX (1, 3)] = 0.0f;

    rot[MAT4_IDX (2, 0)] = 2.0f * (xz - yw);
    rot[MAT4_IDX (2, 1)] = 2.0f * (yz + xw);
    rot[MAT4_IDX (2, 2)] = 1.0f - 2.0f * (xx + yy);
    rot[MAT4_IDX (2, 3)] = 0.0f;

    rot[MAT4_IDX (3, 0)] = 0.0f;
    rot[MAT4_IDX (3, 1)] = 0.0f;
    rot[MAT4_IDX (3, 2)] = 0.0f;
    rot[MAT4_IDX (3, 3)] = 1.0f;

    mat4_multiply (m, m, rot);
}
void mat4_scale (mat4 m, vec3 v) {
    mat4 scale;
    mat4_identity (scale);
    scale[MAT4_IDX (0, 0)] = v.x;
    scale[MAT4_IDX (1, 1)] = v.y;
    scale[MAT4_IDX (2, 2)] = v.z;
    mat4_multiply (m, m, scale);
}
void mat4_multiply (mat4 out, mat4 a, mat4 b) {
    mat4 temp;
    for (int row = 0; row < 4; row++) {
        for (int col = 0; col < 4; col++) {
            temp[MAT4_IDX (row, col)] = 0.0f;
            for (int k = 0; k < 4; k++) {
                temp[MAT4_IDX (row, col)] +=
                    a[MAT4_IDX (row, k)] * b[MAT4_IDX (k, col)];
            }
        }
    }
    for (int i = 0; i < 16; i++)
        out[i] = temp[i];
}
void mat4_perspective (
    mat4 m,
    float fov_rad,
    float aspect,
    float near,
    float far
) {
    float tan_half_fov = tanf (fov_rad / 2.0f);
    float focal = 1.0f / tan_half_fov;
    mat4_identity (m);
    m[MAT4_IDX (0, 0)] = focal / aspect;
    m[MAT4_IDX (1, 1)] = focal;
    m[MAT4_IDX (2, 2)] = far / (far - near);
    m[MAT4_IDX (2, 3)] = -(far * near) / (far - near);
    m[MAT4_IDX (3, 2)] = 1.0f;
    m[MAT4_IDX (3, 3)] = 0.0f;
}
void mat4_look_at (mat4 m, vec3 eye, vec3 center, vec3 up) {
    vec3 f = vec3_normalize (vec3_sub (center, eye));
    vec3 s = vec3_normalize (vec3_cross (f, up)); // Changed order
    vec3 u = vec3_cross (s, f);                   // Changed order

    mat4_identity (m);
    m[MAT4_IDX (0, 0)] = s.x;
    m[MAT4_IDX (0, 1)] = u.x;
    m[MAT4_IDX (0, 2)] = -f.x;
    m[MAT4_IDX (1, 0)] = s.y;
    m[MAT4_IDX (1, 1)] = u.y;
    m[MAT4_IDX (1, 2)] = -f.y;
    m[MAT4_IDX (2, 0)] = s.z;
    m[MAT4_IDX (2, 1)] = u.z;
    m[MAT4_IDX (2, 2)] = -f.z;
    m[MAT4_IDX (0, 3)] = -vec3_dot (s, eye);
    m[MAT4_IDX (1, 3)] = -vec3_dot (u, eye);
    m[MAT4_IDX (2, 3)] = vec3_dot (f, eye);
}

void random_seed (unsigned int seed) {
    srand (seed);
}

float random_float (void) {
    return (float) rand () / (float) RAND_MAX;
}

float random_float_range (float min, float max) {
    return min + random_float () * (max - min);
}

int random_int (int min, int max) {
    if (min > max) {
        int temp = min;
        min = max;
        max = temp;
    }
    return min + rand () / (RAND_MAX / (max - min + 1) + 1);
}

bool random_bool (void) {
    return random_float () < 0.5f;
}

vec2 random_vec2 (void) {
    return (vec2) {random_float (), random_float ()};
}

vec3 random_vec3 (void) {
    return (vec3) {random_float (), random_float (), random_float ()};
}

vec3 random_in_unit_sphere (void) {
    vec3 p;
    do {
        p = vec3_scale (random_vec3 (), 2.0f);
        p = vec3_sub (p, (vec3) {1.0f, 1.0f, 1.0f}); // Map to [-1,1)^3
    } while (vec3_dot (p, p) >= 1.0f); // Rejection sampling until inside sphere
    return p;
}

vec3 random_unit_vector (void) {
    return vec3_normalize (random_in_unit_sphere ());
}