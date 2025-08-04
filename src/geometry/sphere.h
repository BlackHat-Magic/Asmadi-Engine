#pragma once

#include "ecs/ecs.h"

MeshComponent* create_sphere_mesh(
    float radius, int width_segments, int height_segments,
    float phi_start, float phi_length,
    float theta_start, float theta_length,
    SDL_GPUDevice* device
);