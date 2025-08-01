#include "common.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>
#include <stdint.h>

// return 0 on success 1 on failure
int upload_indices(
    SDL_GPUDevice* device, void* indices, size_t size,
    SDL_GPUBuffer** out_buffer
) {
    SDL_GPUBufferCreateInfo ibo_info = {
        .usage = SDL_GPU_BUFFERUSAGE_INDEX, .size = size
    };
    *out_buffer = SDL_CreateGPUBuffer(device, &ibo_info);
    if (out_buffer == NULL) {
        SDL_Log("Failed to create index buffer object: %s", SDL_GetError());
        return 1;
    }

    SDL_GPUTransferBufferCreateInfo transfer_info = {
        .size = size, .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD
    };
    SDL_GPUTransferBuffer* transfer =
        SDL_CreateGPUTransferBuffer(device, &transfer_info);
    if (transfer == NULL) {
        SDL_Log("Failed to create transfer buffer: %s", SDL_GetError());
        return 1;
    }
    void* data = SDL_MapGPUTransferBuffer(device, transfer, false);
    if (data == NULL) {
        SDL_Log("Failed to map transfer buffer: %s", SDL_GetError());
        return 1;
    }
    SDL_memcpy(data, indices, size);
    SDL_UnmapGPUTransferBuffer(device, transfer);

    SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(device);
    if (cmd == NULL) {
        SDL_Log("Failed to acquire GPU command buffer: %s", SDL_GetError());
        return 1;
    }
    SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(cmd);
    if (copy == NULL) {
        SDL_Log("Failed to begin GPU copy pass: %s", SDL_GetError());
        return 1;
    }
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

    return 0;
}

// return 0 on success 1 on failure
int upload_vertices(
    SDL_GPUDevice* device, float* vertices, size_t size,
    SDL_GPUBuffer** out_buffer
) {
    SDL_GPUBufferCreateInfo vbo_info = {
        .usage = SDL_GPU_BUFFERUSAGE_VERTEX, .size = size
    };
    *out_buffer = SDL_CreateGPUBuffer(device, &vbo_info);
    if (out_buffer == NULL) {
        SDL_Log("Failed to create vertex buffer object: %s", SDL_GetError());
        return 1;
    }

    SDL_GPUTransferBufferCreateInfo transfer_info = {
        .size = size, .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD
    };
    SDL_GPUTransferBuffer* transfer =
        SDL_CreateGPUTransferBuffer(device, &transfer_info);
    if (transfer == NULL) {
        SDL_Log("Failed to map transfer buffer: %s", SDL_GetError());
        return 1;
    }
    void* data = SDL_MapGPUTransferBuffer(device, transfer, false);
    if (data == NULL) {
        SDL_Log("Failed to map transfer buffer: %s", SDL_GetError());
        return 1;
    }
    memcpy(data, vertices, size);
    SDL_UnmapGPUTransferBuffer(device, transfer);

    SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(device);
    if (cmd == NULL) {
        SDL_Log("Failed to acquire GPU command buffer: %s", SDL_GetError());
        return 1;
    }
    SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(cmd);
    if (copy == NULL) {
        SDL_Log("Failed to begin GPU copy pass: %s", SDL_GetError());
        return 1;
    }
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

    return 0;
}