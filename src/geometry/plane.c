#include "plane.h"

#include <stdlib.h>

#include "geometry/g_common.h"

MeshComponent* create_plane_mesh(float width, float height, int width_segments, int height_segments, SDL_GPUDevice* device) {
    if (width_segments < 1) width_segments = 1;
    if (height_segments < 1) height_segments = 1;

    int num_vertices = (width_segments + 1) * (height_segments + 1);
    int num_indices = width_segments * height_segments * 6;

    // Check for uint16_t overflow (max 65535 verts); fallback to uint32_t if needed
    // For now, assume small segments; add uint32_t support later if required
    if (num_vertices > 65535) {
        SDL_Log("Plane mesh too large for uint16_t indices");
        return NULL;
    }

    float* vertices = (float*)malloc(num_vertices * 5 * sizeof(float));  // pos.x,y,z + uv.u,v
    if (!vertices) {
        SDL_Log("Failed to allocate vertices for plane mesh");
        return NULL;
    }

    uint16_t* indices = (uint16_t*)malloc(num_indices * sizeof(uint16_t));
    if (!indices) {
        SDL_Log("Failed to allocate indices for plane mesh");
        free(vertices);
        return NULL;
    }

    // Generate vertices (grid in XY, Z=0)
    float half_width = width / 2.0f;
    float half_height = height / 2.0f;
    int vertex_idx = 0;
    for (int iy = 0; iy <= height_segments; iy++) {
        float v = (float)iy / (float)height_segments;
        float y_pos = -half_height + (height * v);  // Y increases upward
        for (int ix = 0; ix <= width_segments; ix++) {
            float u = (float)ix / (float)width_segments;
            float x_pos = -half_width + (width * u);

            vertices[vertex_idx++] = x_pos;
            vertices[vertex_idx++] = y_pos;
            vertices[vertex_idx++] = 0.0f;
            vertices[vertex_idx++] = u;
            vertices[vertex_idx++] = 1.0f - v;  // Flip V for standard texture coord (0 at bottom)
        }
    }

    // Generate indices (two triangles per grid cell, clockwise)
    int index_idx = 0;
    for (int iy = 0; iy < height_segments; iy++) {
        for (int ix = 0; ix < width_segments; ix++) {
            uint16_t a = (uint16_t)(iy * (width_segments + 1) + ix);
            uint16_t b = (uint16_t)(iy * (width_segments + 1) + ix + 1);
            uint16_t c = (uint16_t)((iy + 1) * (width_segments + 1) + ix + 1);
            uint16_t d = (uint16_t)((iy + 1) * (width_segments + 1) + ix);

            // Triangle 1: a -> b -> c (clockwise)
            indices[index_idx++] = a;
            indices[index_idx++] = b;
            indices[index_idx++] = c;

            // Triangle 2: a -> c -> d (clockwise)
            indices[index_idx++] = a;
            indices[index_idx++] = c;
            indices[index_idx++] = d;
        }
    }

    SDL_GPUBuffer* vbo = NULL;
    size_t vertices_size = num_vertices * 5 * sizeof(float);
    int vbo_failed = upload_vertices(device, vertices, vertices_size, &vbo);
    free(vertices);
    if (vbo_failed) {
        free(indices);
        return NULL;  // logging handled in upload_vertices()
    }

    SDL_GPUBuffer* ibo = NULL;
    size_t indices_size = num_indices * sizeof(uint16_t);
    int ibo_failed = upload_indices(device, indices, indices_size, &ibo);
    free(indices);
    if (ibo_failed) {
        return NULL;  // logging handled in upload_indices()
    }

    MeshComponent* out_mesh = (MeshComponent*)malloc(sizeof(MeshComponent));
    if (!out_mesh) {
        SDL_Log("Failed to allocate MeshComponent for plane");
        SDL_ReleaseGPUBuffer(device, vbo);
        SDL_ReleaseGPUBuffer(device, ibo);
        return NULL;
    }
    *out_mesh = (MeshComponent){
        .vertex_buffer = vbo,
        .num_vertices = (uint32_t)num_vertices,
        .index_buffer = ibo,
        .num_indices = (uint32_t)num_indices,
        .index_size = SDL_GPU_INDEXELEMENTSIZE_16BIT
    };

    return out_mesh;
}