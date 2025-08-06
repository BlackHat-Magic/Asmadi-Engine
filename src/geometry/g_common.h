#include <stdint.h>

#include <SDL3/SDL_gpu.h>

void compute_vertex_normals(
    float* vertices, uint32_t num_vertices,
    uint16_t* indices, uint32_t num_indices,
    uint32_t stride, uint32_t pos_offset, uint32_t normal_offset
);

int upload_indices(SDL_GPUDevice* device, void* indices, size_t size, SDL_GPUBuffer** out_buffer);

int upload_vertices(SDL_GPUDevice* device, float* vertices, size_t size, SDL_GPUBuffer** out_buffer);