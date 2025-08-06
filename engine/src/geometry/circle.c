#include "geometry/circle.h"

#include <math.h>
#include <stdlib.h>

#include "geometry/g_common.h"

MeshComponent create_circle_mesh(float radius, int segments, SDL_GPUDevice* device) {
    MeshComponent null_mesh = (MeshComponent) {0};
    if (segments < 3) {
        SDL_Log("Circle must have at least 3 segments");
        return null_mesh;
    }

    int num_vertices = segments + 1;  // Center + ring
    int num_indices = segments * 3;   // One triangle per segment

    // Check for uint16_t overflow (max 65535 verts); extend to uint32_t if needed later
    if (num_vertices > 65535) {
        SDL_Log("Circle mesh too large for uint16_t indices");
        return null_mesh;
    }

    float* vertices = (float*)malloc(num_vertices * 8 * sizeof(float));  // pos.x,y,z + normal.x,y,z + uv.u,v
    if (!vertices) {
        SDL_Log("Failed to allocate vertices for circle mesh");
        return null_mesh;
    }

    uint16_t* indices = (uint16_t*)malloc(num_indices * sizeof(uint16_t));
    if (!indices) {
        SDL_Log("Failed to allocate indices for circle mesh");
        free(vertices);
        return null_mesh;
    }

    // Center vertex
    int vertex_idx = 0;
    vertices[vertex_idx++] = 0.0f;  // x
    vertices[vertex_idx++] = 0.0f;  // y
    vertices[vertex_idx++] = 0.0f;  // z
    vertices[vertex_idx++] = 0.0f;  // nx
    vertices[vertex_idx++] = 0.0f;  // ny
    vertices[vertex_idx++] = 1.0f;  // nz
    vertices[vertex_idx++] = 0.5f;  // u
    vertices[vertex_idx++] = 0.5f;  // v

    // Ring vertices
    for (int i = 0; i < segments; i++) {
        float theta = (float)i / (float)segments * 2.0f * (float)M_PI;
        float cos_theta = cosf(theta);
        float sin_theta = sinf(theta);

        vertices[vertex_idx++] = radius * cos_theta;
        vertices[vertex_idx++] = radius * sin_theta;
        vertices[vertex_idx++] = 0.0f;
        vertices[vertex_idx++] = 0.0f;  // nx
        vertices[vertex_idx++] = 0.0f;  // ny
        vertices[vertex_idx++] = 1.0f;  // nz
        vertices[vertex_idx++] = 0.5f + 0.5f * cos_theta;
        vertices[vertex_idx++] = 0.5f + 0.5f * sin_theta;  // Note: may need to flip v (1.0f - ...) based on texture orientation
    }

    // Indices (clockwise winding for consistency with other geometries)
    int index_idx = 0;
    for (int i = 0; i < segments; i++) {
        indices[index_idx++] = 0;                     // Center
        indices[index_idx++] = (uint16_t)(i + 1);     // Current ring vertex
        indices[index_idx++] = (uint16_t)((i + 1) % segments + 1);  // Next ring vertex (wrap around)
    }

    // Upload to GPU
    SDL_GPUBuffer* vbo = NULL;
    size_t vertices_size = num_vertices * 8 * sizeof(float);
    int vbo_failed = upload_vertices(device, vertices, vertices_size, &vbo);
    free(vertices);
    if (vbo_failed) {
        free(indices);
        return null_mesh;  // Logging handled in upload_vertices
    }

    SDL_GPUBuffer* ibo = NULL;
    size_t indices_size = num_indices * sizeof(uint16_t);
    int ibo_failed = upload_indices(device, indices, indices_size, &ibo);
    free(indices);
    if (ibo_failed) {
        SDL_ReleaseGPUBuffer(device, vbo);
        return null_mesh;  // Logging handled in upload_indices
    }

    MeshComponent out_mesh = (MeshComponent){
        .vertex_buffer = vbo,
        .num_vertices = (uint32_t)num_vertices,
        .index_buffer = ibo,
        .num_indices = (uint32_t)num_indices,
        .index_size = SDL_GPU_INDEXELEMENTSIZE_16BIT
    };

    return out_mesh;
}