#pragma once

#include <SDL3/SDL_gpu.h>

// Returns 0 on success, 1 on failure
int upload_vertices(
    SDL_GPUDevice* device, const void* vertices, Uint64 vertices_size,
    SDL_GPUBuffer** vbo_out
);

// Returns 0 on success, 1 on failure
int upload_indices(
    SDL_GPUDevice* device, const void* indices, Uint64 indices_size,
    SDL_GPUBuffer** ibo_out
);

void compute_vertex_normals(
    float* vertices, int num_vertices, const Uint16* indices,
    int num_indices, int stride, int pos_offset, int norm_offset
);