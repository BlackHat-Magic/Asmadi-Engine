#pragma once

#include "ecs/ecs.h"
#include "math/matrix.h"

MeshComponent* create_line_mesh_from_points(vec3* points, int num_points, SDL_GPUDevice* device);