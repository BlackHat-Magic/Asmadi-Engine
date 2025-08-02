#pragma once

// Helper macro for mat4 indexing (column-major: m[col*4 + row])
#define MAT4_IDX(row, col) ((col) * 4 + (row))

typedef struct {
    float x, y;
} vec2;

typedef struct {
    float x, y, z;
} vec3;

typedef struct {
    float x, y, z, w;
} vec4;

typedef float mat4[16];

vec2 vec2_add (vec2 a, vec2 b);
vec2 vec2_sub (vec2 a, vec2 b);
vec2 vec2_scale (vec2 v, float s);
vec2 vec2_normalize (vec2 v);
float vec2_dot (vec2 a, vec2 b);
float vec2_cross (vec2 a, vec2 b);

vec3 vec3_add (vec3 a, vec3 b);
vec3 vec3_sub (vec3 a, vec3 b);
vec3 vec3_scale (vec3 v, float s);
vec3 vec3_normalize (vec3 v);
float vec3_dot (vec3 a, vec3 b);
vec3 vec3_cross (vec3 a, vec3 b);

vec4 vec4_add (vec4 a, vec4 b);
vec4 vec4_sub (vec4 a, vec4 b);
vec4 vec4_scale (vec4 v, float s);
vec4 vec4_normalize (vec4 v);
float vec4_dot (vec4 a, vec4 b);

vec4 quat_from_euler(vec3 euler);
vec3 euler_from_quat(vec4 q);
vec4 quat_multiply(vec4 a, vec4 b); // compose rotations
vec4 quat_conjugate(vec4 q);
vec4 quat_normalize (vec4 q);

void mat4_identity(mat4 m);
void mat4_translate(mat4 m, vec3 v);
void mat4_rotate_x(mat4 m, float angle_rad);
void mat4_rotate_y(mat4 m, float angle_rad);
void mat4_rotate_z(mat4 m, float angle_rad);
void mat4_rotate_quat(mat4 m, vec4 q);
void mat4_scale(mat4 m, vec3 v);
void mat4_multiply(mat4 out, mat4 a, mat4 b);
void mat4_perspective(mat4 m, float fov_rad, float aspect, float near, float far);
void mat4_look_at(mat4 m, vec3 eye, vec3 center, vec3 up);