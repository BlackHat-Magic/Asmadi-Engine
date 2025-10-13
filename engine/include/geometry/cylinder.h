#pragma once

#include <ecs/ecs.h>

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
);