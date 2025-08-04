#include "geometry/cone.h"

#include "geometry/cylinder.h"

MeshComponent* create_cone_mesh(
    float radius, float height, int radial_segments, int height_segments,
    bool open_ended, float theta_start, float theta_length,
    SDL_GPUDevice* device
) {
    return create_cylinder_mesh(
        0.0f, radius, height, radial_segments, height_segments, open_ended,
        theta_start, theta_length, device
    );
}