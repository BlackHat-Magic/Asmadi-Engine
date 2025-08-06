#include "geometry/g_common.h"

#include <stdint.h>
#include <stdlib.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>

#include "math/matrix.h"

void compute_vertex_normals(
    float* vertices, uint32_t num_vertices,
    uint16_t* indices, uint32_t num_indices,
    uint32_t stride, uint32_t pos_offset, uint32_t normal_offset
) {
    vec3* accumulators = (vec3*)calloc(num_vertices, sizeof(vec3));
    if (!accumulators) {
        SDL_Log("Failed to allocate normal accumulators");
        return;
    }

    for (uint32_t i = 0; i < num_indices; i += 3) {
        uint16_t i0 = indices[i];
        uint16_t i1 = indices[i + 1];
        uint16_t i2 = indices[i + 2];

        // Extract positions
        float* p0_ptr = vertices + i0 * stride + pos_offset;
        float* p1_ptr = vertices + i1 * stride + pos_offset;
        float* p2_ptr = vertices + i2 * stride + pos_offset;

        vec3 p0 = {p0_ptr[0], p0_ptr[1], p0_ptr[2]};
        vec3 p1 = {p1_ptr[0], p1_ptr[1], p1_ptr[2]};
        vec3 p2 = {p2_ptr[0], p2_ptr[1], p2_ptr[2]};

        vec3 e1 = vec3_sub(p1, p0);
        vec3 e2 = vec3_sub(p2, p0);

        vec3 face_normal = vec3_cross(e1, e2);  // Un-normalized for area weighting

        // Accumulate
        accumulators[i0] = vec3_add(accumulators[i0], face_normal);
        accumulators[i1] = vec3_add(accumulators[i1], face_normal);
        accumulators[i2] = vec3_add(accumulators[i2], face_normal);
    }

    // Normalize and set into vertices
    for (uint32_t v = 0; v < num_vertices; v++) {
        vec3 n = vec3_normalize(accumulators[v]);
        if (n.x == 0.0f && n.y == 0.0f && n.z == 0.0f) {  // Zero vector case
            n = (vec3){0.0f, 0.0f, 1.0f};  // Default normal
        }

        float* normal_ptr = vertices + v * stride + normal_offset;
        normal_ptr[0] = n.x;
        normal_ptr[1] = n.y;
        normal_ptr[2] = n.z;
    }

    free(accumulators);
}

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