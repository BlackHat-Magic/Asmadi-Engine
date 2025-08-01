#include "common.h"

#include <SDL3/SDL_gpu.h>
#include <stdint.h>

static void upload_indices(
    SDL_GPUDevice* device, void* indices, size_t size,
    SDL_GPUBuffer** out_buffer
) {
    SDL_GPUBufferCreateInfo ibo_info = {
        .usage = SDL_GPU_BUFFERUSAGE_INDEX, .size = size
    };
    *out_buffer = SDL_CreateGPUBuffer(device, &ibo_info);

    SDL_GPUTransferBufferCreateInfo transfer_info = {
        .size = size, .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD
    };
    SDL_GPUTransferBuffer* transfer =
        SDL_CreateGPUTransferBuffer(device, &transfer_info);
    void* data = SDL_MapGPUTransferBuffer(device, transfer, false);
    SDL_memcpy(data, indices, size);
    SDL_UnmapGPUTransferBuffer(device, transfer);

    SDL_GPUCommandBuffer* cmd         = SDL_AcquireGPUCommandBuffer(device);
    SDL_GPUCopyPass* copy             = SDL_BeginGPUCopyPass(cmd);
    SDL_GPUTransferBufferLocation src = {
        .transfer_buffer = transfer, .offset = 0
    };
    SDL_GPUBufferRegion dst = {
        .buffer = *out_buffer, .offset = 0, .size = size
    };
    SDL_UploadToGPUBuffer(copy, &src, &dst, false);
    SDL_EndGPUCopyPass(copy);
    SDL_SubmitGPUCommandBuffer(cmd);
    SDL_ReleaseGPUTransferBuffer(device, transfer);
}

static void upload_vertices(
    SDL_GPUDevice* device, float* vertices, size_t size,
    SDL_GPUBuffer** out_buffer
) {
    SDL_GPUBufferCreateInfo vbo_info = {
        .usage = SDL_GPU_BUFFERUSAGE_VERTEX, .size = size
    };
    *out_buffer = SDL_CreateGPUBuffer(device, &vbo_info);

    SDL_GPUTransferBufferCreateInfo transfer_info = {
        .size = size, .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD
    };
    SDL_GPUTransferBuffer* transfer =
        SDL_CreateGPUTransferBuffer(device, &transfer_info);
    void* data = SDL_MapGPUTransferBuffer(device, transfer, false);
    memcpy(data, vertices, size);
    SDL_UnmapGPUTransferBuffer(device, transfer);

    SDL_GPUCommandBuffer* cmd         = SDL_AcquireGPUCommandBuffer(device);
    SDL_GPUCopyPass* copy             = SDL_BeginGPUCopyPass(cmd);
    SDL_GPUTransferBufferLocation src = {
        .transfer_buffer = transfer, .offset = 0
    };
    SDL_GPUBufferRegion dst = {
        .buffer = *out_buffer, .offset = 0, .size = size
    };
    SDL_UploadToGPUBuffer(copy, &src, &dst, false);
    SDL_EndGPUCopyPass(copy);
    SDL_SubmitGPUCommandBuffer(cmd);
    SDL_ReleaseGPUTransferBuffer(device, transfer);
}