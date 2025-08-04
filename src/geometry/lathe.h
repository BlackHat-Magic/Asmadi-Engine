#pragma once

#include "ecs/ecs.h"
#include "math/matrix.h"

MeshComponent* create_lathe_mesh(
    vec2* points, int num_points, int radial_segments,
    float phi_start, float phi_length, SDL_GPUDevice* device
);