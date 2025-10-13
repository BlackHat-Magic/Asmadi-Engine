#include <stdlib.h>

#include <geometry/lathe.h>
#include <geometry/cylinder.h>

MeshComponent create_cylinder_mesh (
    float radius_top,
    float radius_bottom,
    float height,
    int radial_segments,
    int height_segments,
    bool open_ended,
    float theta_start,
    float theta_length,
    SDL_GPUDevice* device
) {
    MeshComponent out_mesh = {0};
    if (radial_segments < 3 || height_segments < 1) {
        SDL_Log (
            "Cylinder must have at least 3 radial segments and 1 height segment"
        );
        return (MeshComponent) {0};
    }

    // Calculate num_points: sides (height_segments + 1) + optional 2 centers
    int num_points = height_segments + 1;
    if (!open_ended) num_points += 2;

    vec2* points = (vec2*) malloc (num_points * sizeof (vec2));
    if (!points) {
        SDL_Log ("Failed to allocate points for cylinder path");
        return (MeshComponent) {0};
    }

    int idx = 0;
    float half_height = height / 2.0f;

    // Bottom center (if closed)
    if (!open_ended) {
        points[idx++] = (vec2) {0.0f, -half_height};
    }

    // Bottom edge
    points[idx++] = (vec2) {radius_bottom, -half_height};

    // Intermediate side points (if height_segments > 1)
    for (int i = 1; i < height_segments; i++) {
        float frac = (float) i / (float) height_segments;
        points[idx++] =
            (vec2) {radius_bottom + frac * (radius_top - radius_bottom),
                    -half_height + frac * height};
    }

    // Top edge
    points[idx++] = (vec2) {radius_top, half_height};

    // Top center (if closed)
    if (!open_ended) {
        points[idx++] = (vec2) {0.0f, half_height};
    }

    out_mesh = create_lathe_mesh (
        points, idx, radial_segments, theta_start, theta_length, device
    );
    free (points);
    return out_mesh;
}