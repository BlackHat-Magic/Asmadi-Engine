#include "spline.h"
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <SDL3/SDL.h>

CatmullRomSpline* create_catmull_rom_spline(vec3* points, int num_points, bool closed, float tension) {
    if (num_points < 2) return NULL;
    CatmullRomSpline* spline = (CatmullRomSpline*)malloc(sizeof(CatmullRomSpline));
    spline->points = (vec3*)malloc(num_points * sizeof(vec3));
    memcpy(spline->points, points, num_points * sizeof(vec3));
    spline->num_points = num_points;
    spline->closed = closed;
    spline->tension = tension;
    return spline;
}

void destroy_spline(CatmullRomSpline* spline) {
    if (spline) {
        free(spline->points);
        free(spline);
    }
}

static vec3 catmull_rom_interpolate(vec3 p0, vec3 p1, vec3 p2, vec3 p3, float t, float tension) {
    float t2 = t * t;
    float t3 = t2 * t;
    vec3 a = vec3_scale(vec3_add(vec3_scale(p0, -1.0f), vec3_add(vec3_scale(p3, -1.0f), vec3_add(vec3_scale(p1, 2.0f + tension), vec3_scale(p2, tension)))), 0.5f);
    vec3 b = vec3_scale(vec3_add(vec3_scale(p0, 5.0f + 3.0f * tension), vec3_add(vec3_scale(p3, tension), vec3_add(vec3_scale(p1, -4.0f - 4.0f * tension), vec3_scale(p2, -tension - 2.0f)))), 0.5f);
    vec3 c = vec3_scale(vec3_add(vec3_scale(p0, -tension), vec3_add(vec3_scale(p2, 2.0f + tension), vec3_scale(p1, -2.0f - 2.0f * tension))), 0.5f);
    vec3 d = vec3_scale(vec3_add(vec3_scale(p0, tension), vec3_add(vec3_scale(p2, tension), vec3_scale(p1, 2.0f))), 0.5f);
    return vec3_add(vec3_add(vec3_scale(a, t3), vec3_scale(b, t2)), vec3_add(vec3_scale(c, t), d));
}

vec3 get_point_on_spline(CatmullRomSpline* spline, float t) {
    if (spline->closed && spline->num_points < 3) return (vec3){0.0f, 0.0f, 0.0f};  // Need at least 3 for closed
    int segments = spline->closed ? spline->num_points : spline->num_points - 1;
    t = fmodf(t, 1.0f);  // Wrap t
    if (t < 0.0f) t += 1.0f;
    float scaled_t = t * (float)segments;
    int idx = (int)floorf(scaled_t);
    float frac = scaled_t - (float)idx;

    int i0 = (idx - 1 + segments) % segments;
    int i1 = idx % segments;
    int i2 = (idx + 1) % segments;
    int i3 = (idx + 2) % segments;

    if (!spline->closed) {
        i0 = SDL_clamp(i0, 0, spline->num_points - 1);
        i1 = SDL_clamp(i1, 0, spline->num_points - 1);
        i2 = SDL_clamp(i2, 0, spline->num_points - 1);
        i3 = SDL_clamp(i3, 0, spline->num_points - 1);
    }

    return catmull_rom_interpolate(spline->points[i0], spline->points[i1], spline->points[i2], spline->points[i3], frac, spline->tension);
}

vec3* get_points_on_spline(CatmullRomSpline* spline, int divisions, int* out_num_points) {
    if (divisions < 2) divisions = 2;
    *out_num_points = divisions + (spline->closed ? 1 : 0);  // Close the loop
    vec3* points = (vec3*)malloc(*out_num_points * sizeof(vec3));
    for (int i = 0; i < divisions; i++) {
        float t = (float)i / (float)(divisions - (spline->closed ? 0 : 1));
        points[i] = get_point_on_spline(spline, t);
    }
    if (spline->closed) points[divisions] = points[0];  // Close
    return points;
}