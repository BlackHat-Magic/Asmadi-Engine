#pragma once

#include "ecs/ecs.h"

MeshComponent create_plane_mesh(float width, float height, int width_segments, int height_segments, SDL_GPUDevice* device);