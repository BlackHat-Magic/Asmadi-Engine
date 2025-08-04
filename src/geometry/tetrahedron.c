#include "geometry/tetrahedron.h"

#include <stdlib.h>

#include "geometry/g_common.h"

MeshComponent* create_tetrahedron_mesh(float radius, SDL_GPUDevice* device) {
    // 4 vertices (positions + UVs; simple UV projection for demo)
    const int num_vertices = 4;
    float vertices[] = {
        1.0f, 1.0f, 1.0f, 0.5f, 0.5f,    // Vert 0
        1.0f, -1.0f, -1.0f, 0.5f, 0.0f,  // Vert 1
        -1.0f, 1.0f, -1.0f, 0.0f, 0.5f,  // Vert 2
        -1.0f, -1.0f, 1.0f, 1.0f, 0.5f   // Vert 3
    };

    // Normalize and scale
    for (int i = 0; i < num_vertices; i++) {
        vec3 pos = {vertices[i*5], vertices[i*5+1], vertices[i*5+2]};
        pos = vec3_normalize(pos);
        pos = vec3_scale(pos, radius);
        vertices[i*5] = pos.x;
        vertices[i*5+1] = pos.y;
        vertices[i*5+2] = pos.z;
        // UVs can be improved (e.g., spherical mapping)
    }

    // 12 indices (4 triangles, clockwise)
    const int num_indices = 12;
    uint16_t indices[] = {
        0, 1, 2,  // Face 0
        0, 3, 1,  // Face 1
        0, 2, 3,  // Face 2
        1, 3, 2   // Face 3
    };

    SDL_GPUBuffer* vbo = NULL;
    size_t vertices_size = num_vertices * 5 * sizeof(float);
    int vbo_failed = upload_vertices(device, vertices, vertices_size, &vbo);
    if (vbo_failed) return NULL;

    SDL_GPUBuffer* ibo = NULL;
    size_t indices_size = num_indices * sizeof(uint16_t);
    int ibo_failed = upload_indices(device, indices, indices_size, &ibo);
    if (ibo_failed) {
        SDL_ReleaseGPUBuffer(device, vbo);
        return NULL;
    }

    MeshComponent* out_mesh = (MeshComponent*)malloc(sizeof(MeshComponent));
    if (!out_mesh) {
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