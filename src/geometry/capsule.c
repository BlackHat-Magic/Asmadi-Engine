#include <stdlib.h>
#include <math.h>

#include "ecs/ecs.h"
#include "geometry/lathe.h"

MeshComponent* create_capsule_mesh(
    float radius, float height, int cap_segments, int radial_segments,
    SDL_GPUDevice* device
) {
    if (cap_segments < 1) cap_segments = 1;

    // Total points: bottom cap (cap_segments + 1) + top cap (cap_segments + 1)
    // The cylindrical body is formed by the connection between the last bottom point (bottom equator)
    // and the first top point (top equator) - no extra points needed unless height_segments > 1 (not implemented here)
    int num_points = (cap_segments + 1) * 2;
    if (height <= 0.0f) {
        num_points -= 1;  // Avoid duplicate equator point for sphere case (degenerate but harmless if not subtracted)
    }
    vec2* points = (vec2*)malloc(num_points * sizeof(vec2));
    if (!points) {
        SDL_Log("Failed to allocate points for capsule path");
        return NULL;
    }

    float half_height = height * 0.5f;
    int idx = 0;

    // Bottom cap: from bottom pole to bottom equator
    for (int i = 0; i <= cap_segments; i++) {
        float theta = (float)i / (float)cap_segments * (float)M_PI * 0.5f;
        points[idx].x = radius * sinf(theta);
        points[idx].y = -half_height - radius * cosf(theta);
        idx++;
    }

    // Top cap: from top equator to top pole
    // Note: if height <= 0, skip the first top point (equator) to avoid duplicate
    int top_start = (height <= 0.0f) ? cap_segments - 1 : cap_segments;
    for (int i = top_start; i >= 0; i--) {
        float theta = (float)i / (float)cap_segments * (float)M_PI * 0.5f;
        points[idx].x = radius * sinf(theta);
        points[idx].y = half_height + radius * cosf(theta);
        if (i < top_start || height > 0.0f) {  // Only increment if added
            idx++;
        }
    }

    MeshComponent* mesh = create_lathe_mesh(points, num_points, radial_segments, 0.0f, 2.0f * (float)M_PI, device);
    free(points);
    return mesh;
}