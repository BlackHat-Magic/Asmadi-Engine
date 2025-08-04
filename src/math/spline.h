#pragma once

#include "matrix.h"

typedef struct {
    vec3* points;
    int num_points;
    bool closed;
    float tension;  // 0.5 default for centripetal
} CatmullRomSpline;

CatmullRomSpline* create_catmull_rom_spline(vec3* points, int num_points, bool closed, float tension);
void destroy_spline(CatmullRomSpline* spline);
vec3 get_point_on_spline(CatmullRomSpline* spline, float t);  // t in [0,1], wraps if closed
vec3* get_points_on_spline(CatmullRomSpline* spline, int divisions, int* out_num_points);  // Allocates array, caller frees