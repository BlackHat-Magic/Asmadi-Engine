#include "geometry/sphere.h"

#include <math.h>
#include <stdlib.h>

#include "geometry/lathe.h"

MeshComponent create_sphere_mesh(
    float radius, int width_segments, int height_segments,
    float phi_start, float phi_length,
    float theta_start, float theta_length,
    SDL_GPUDevice* device
) {
    MeshComponent null_mesh = (MeshComponent) {0};
    if (width_segments < 3 || height_segments < 2) {
        SDL_Log("Sphere must have at least 3 width segments and 2 height segments");
        return null_mesh;
    }

    int num_points = height_segments + 1;
    vec2* points = (vec2*)malloc(num_points * sizeof(vec2));
    if (!points) {
        SDL_Log("Failed to allocate points for sphere path");
        return null_mesh;
    }

    for (int i = 0; i < num_points; i++) {
        float frac = (float)i / (float)height_segments;
        float theta = theta_start + (1.0f - frac) * theta_length;  // Reversed to start from bottom (theta max) to top (theta min)
        points[i].x = radius * sinf(theta);
        points[i].y = radius * cosf(theta);
    }

    // lathe returns normals
    MeshComponent mesh = create_lathe_mesh(points, num_points, width_segments, phi_start, phi_length, device);
    free(points);
    return mesh;
}