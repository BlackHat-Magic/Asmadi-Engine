#include "geometry/g_common.h"

#include <stdlib.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>

#include <math/matrix.h>

// Returns 0 on success, 1 on failure
int upload_vertices(
    SDL_GPUDevice* device, const void* vertices, Uint64 vertices_size,
    SDL_GPUBuffer** vbo_out
) {
    SDL_GPUBufferCreateInfo vbo_info = {
        .size  = (Uint32)vertices_size,
        .usage = SDL_GPU_BUFFERUSAGE_VERTEX
    };
    SDL_GPUBuffer* vbo = SDL_CreateGPUBuffer(device, &vbo_info);
    if (!vbo) {
        SDL_Log("Failed to create vertex buffer: %s", SDL_GetError());
        return 1;
    }

    SDL_GPUTransferBufferCreateInfo trans_info = {
        .size  = (Uint32)vertices_size,
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD
    };
    SDL_GPUTransferBuffer* trans_buf =
        SDL_CreateGPUTransferBuffer(device, &trans_info);
    if (!trans_buf) {
        SDL_Log("Failed to create transfer buffer: %s", SDL_GetError());
        SDL_ReleaseGPUBuffer(device, vbo);
        return 1;
    }

    void* data = SDL_MapGPUTransferBuffer(device, trans_buf, false);
    if (!data) {
        SDL_Log("Failed to map transfer buffer: %s", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(device, trans_buf);
        SDL_ReleaseGPUBuffer(device, vbo);
        return 1;
    }
    memcpy(data, vertices, vertices_size);
    SDL_UnmapGPUTransferBuffer(device, trans_buf);

    SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(device);
    if (!cmd) {
        SDL_Log("Failed to acquire command buffer: %s", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(device, trans_buf);
        SDL_ReleaseGPUBuffer(device, vbo);
        return 1;
    }

    SDL_GPUCopyPass* copy_pass = SDL_BeginGPUCopyPass(cmd);
    if (!copy_pass) {
        SDL_Log("Failed to begin copy pass: %s", SDL_GetError());
        SDL_SubmitGPUCommandBuffer(cmd);
        SDL_ReleaseGPUTransferBuffer(device, trans_buf);
        SDL_ReleaseGPUBuffer(device, vbo);
        return 1;
    }

    SDL_GPUTransferBufferLocation src_loc = {
        .transfer_buffer = trans_buf,
        .offset          = 0
    };
    SDL_GPUBufferRegion dst_reg = {
        .buffer = vbo,
        .offset = 0,
        .size   = (Uint32)vertices_size
    };
    SDL_UploadToGPUBuffer(copy_pass, &src_loc, &dst_reg, false);
    SDL_EndGPUCopyPass(copy_pass);
    SDL_SubmitGPUCommandBuffer(cmd);

    SDL_ReleaseGPUTransferBuffer(device, trans_buf);
    *vbo_out = vbo;
    return 0;
}

// Returns 0 on success, 1 on failure
int upload_indices(
    SDL_GPUDevice* device, const void* indices, Uint64 indices_size,
    SDL_GPUBuffer** ibo_out
) {
    SDL_GPUBufferCreateInfo ibo_info = {
        .size  = (Uint32)indices_size,
        .usage = SDL_GPU_BUFFERUSAGE_INDEX
    };
    SDL_GPUBuffer* ibo = SDL_CreateGPUBuffer(device, &ibo_info);
    if (!ibo) {
        SDL_Log("Failed to create index buffer: %s", SDL_GetError());
        return 1;
    }

    SDL_GPUTransferBufferCreateInfo trans_info = {
        .size  = (Uint32)indices_size,
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD
    };
    SDL_GPUTransferBuffer* trans_buf =
        SDL_CreateGPUTransferBuffer(device, &trans_info);
    if (!trans_buf) {
        SDL_Log("Failed to create transfer buffer: %s", SDL_GetError());
        SDL_ReleaseGPUBuffer(device, ibo);
        return 1;
    }

    void* data = SDL_MapGPUTransferBuffer(device, trans_buf, false);
    if (!data) {
        SDL_Log("Failed to map transfer buffer: %s", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(device, trans_buf);
        SDL_ReleaseGPUBuffer(device, ibo);
        return 1;
    }
    memcpy(data, indices, indices_size);
    SDL_UnmapGPUTransferBuffer(device, trans_buf);

    SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(device);
    if (!cmd) {
        SDL_Log("Failed to acquire command buffer: %s", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(device, trans_buf);
        SDL_ReleaseGPUBuffer(device, ibo);
        return 1;
    }

    SDL_GPUCopyPass* copy_pass = SDL_BeginGPUCopyPass(cmd);
    if (!copy_pass) {
        SDL_Log("Failed to begin copy pass: %s", SDL_GetError());
        SDL_SubmitGPUCommandBuffer(cmd);
        SDL_ReleaseGPUTransferBuffer(device, trans_buf);
        SDL_ReleaseGPUBuffer(device, ibo);
        return 1;
    }

    SDL_GPUTransferBufferLocation src_loc = {
        .transfer_buffer = trans_buf,
        .offset          = 0
    };
    SDL_GPUBufferRegion dst_reg = {
        .buffer = ibo,
        .offset = 0,
        .size   = (Uint32)indices_size
    };
    SDL_UploadToGPUBuffer(copy_pass, &src_loc, &dst_reg, false);
    SDL_EndGPUCopyPass(copy_pass);
    SDL_SubmitGPUCommandBuffer(cmd);

    SDL_ReleaseGPUTransferBuffer(device, trans_buf);
    *ibo_out = ibo;
    return 0;
}

void compute_vertex_normals(
    float* vertices, int num_vertices, const Uint16* indices,
    int num_indices, int stride, int pos_offset, int norm_offset
) {
    // Accumulate normals per vertex
    vec3* accum_norms = (vec3*)calloc(num_vertices, sizeof(vec3));
    if (!accum_norms) {
        SDL_Log("Failed to allocate accum_norms for normals computation");
        return;
    }

    for (int i = 0; i < num_indices; i += 3) {
        int ia = indices[i];
        int ib = indices[i + 1];
        int ic = indices[i + 2];

        vec3 a = {
            vertices[ia * stride + pos_offset],
            vertices[ia * stride + pos_offset + 1],
            vertices[ia * stride + pos_offset + 2]
        };
        vec3 b = {
            vertices[ib * stride + pos_offset],
            vertices[ib * stride + pos_offset + 1],
            vertices[ib * stride + pos_offset + 2]
        };
        vec3 c = {
            vertices[ic * stride + pos_offset],
            vertices[ic * stride + pos_offset + 1],
            vertices[ic * stride + pos_offset + 2]
        };

        vec3 ab = vec3_sub(b, a);
        vec3 ac = vec3_sub(c, a);
        vec3 face_norm = vec3_normalize(vec3_cross(ab, ac));

        accum_norms[ia] = vec3_add(accum_norms[ia], face_norm);
        accum_norms[ib] = vec3_add(accum_norms[ib], face_norm);
        accum_norms[ic] = vec3_add(accum_norms[ic], face_norm);
    }

    // Normalize and assign
    for (int i = 0; i < num_vertices; i++) {
        vec3 norm = vec3_normalize(accum_norms[i]);
        vertices[i * stride + norm_offset] = norm.x;
        vertices[i * stride + norm_offset + 1] = norm.y;
        vertices[i * stride + norm_offset + 2] = norm.z;
    }

    free(accum_norms);
}