#pragma once

#include "ecs/ecs.h"
#include "math/matrix.h"

MeshComponent create_lathe_mesh(
    vec2* path, int path_length, int segments, float phi_start, float phi_length,
    SDL_GPUDevice* device
);