#include <math.h>
#include <stdlib.h>

#include "ecs/ecs.h"
#include "geometry/lathe.h"

MeshComponent create_capsule_mesh (
    float radius,
    float height,
    int cap_segments,
    int radial_segments,
    SDL_GPUDevice* device
) {
    MeshComponent out_mesh = {0};
    if (cap_segments < 1) cap_segments = 1;

    int num_points = (cap_segments + 1) * 2;
    if (height <= 0.0f) {
        num_points -= 1;
    }
    vec2* points = (vec2*) malloc (num_points * sizeof (vec2));
    if (!points) {
        SDL_Log ("Failed to allocate points for capsule path");
        return (MeshComponent) {0};
    }

    float half_height = height * 0.5f;
    int idx = 0;

    for (int i = 0; i <= cap_segments; i++) {
        float theta = (float) i / (float) cap_segments * (float) M_PI * 0.5f;
        points[idx].x = radius * sinf (theta);
        points[idx].y = -half_height - radius * cosf (theta);
        idx++;
    }

    int top_start = (height <= 0.0f) ? cap_segments - 1 : cap_segments;
    for (int i = top_start; i >= 0; i--) {
        float theta = (float) i / (float) cap_segments * (float) M_PI * 0.5f;
        points[idx].x = radius * sinf (theta);
        points[idx].y = half_height + radius * cosf (theta);
        if (i < top_start || height > 0.0f) {
            idx++;
        }
    }

    out_mesh = create_lathe_mesh (
        points, num_points, radial_segments, 0.0f, (float) M_PI * 2.0f, device
    );
    free (points);
    return out_mesh;
}