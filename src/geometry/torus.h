#pragma once

#include "ecs/ecs.h"

MeshComponent* create_torus_mesh(
    float radius, float tube_radius, int radial_segments, int tubular_segments,
    float arc, SDL_GPUDevice* device
);