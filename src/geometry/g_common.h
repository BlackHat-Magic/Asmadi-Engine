#include <stdint.h>

#include <SDL3/SDL_gpu.h>

int upload_indices(SDL_GPUDevice* device, void* indices, size_t size, SDL_GPUBuffer** out_buffer);

int upload_vertices(SDL_GPUDevice* device, float* vertices, size_t size, SDL_GPUBuffer** out_buffer);