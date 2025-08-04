// src/geometry/ring.h

#pragma once

#include "ecs/ecs.h"

MeshComponent* create_ring_mesh(
    float inner_radius, float outer_radius, int theta_segments, int phi_segments,
    float theta_start, float theta_length, SDL_GPUDevice* device
);