#include "geometry/ring.h"

#include <math.h>
#include <stdlib.h>

#include "geometry/g_common.h"

MeshComponent* create_ring_mesh(
    float inner_radius, float outer_radius, int theta_segments, int phi_segments,
    float theta_start, float theta_length, SDL_GPUDevice* device
) {
    if (theta_segments < 3) {
        SDL_Log("Ring must have at least 3 theta segments");
        return NULL;
    }
    if (phi_segments < 1) {
        SDL_Log("Ring must have at least 1 phi segment");
        return NULL;
    }
    if (inner_radius >= outer_radius) {
        SDL_Log("Inner radius must be less than outer radius");
        return NULL;
    }

    int num_theta = theta_segments + 1;
    int num_phi = phi_segments + 1;
    int num_vertices = num_theta * num_phi;

    if (num_vertices > 65535) {
        SDL_Log("Ring mesh too large for uint16_t indices");
        return NULL;
    }

    float* vertices = (float*)malloc(num_vertices * 5 * sizeof(float));  // pos.x,y,z + uv.u,v
    if (!vertices) {
        SDL_Log("Failed to allocate vertices for ring mesh");
        return NULL;
    }

    int vertex_idx = 0;
    for (int i = 0; i < num_theta; i++) {
        float theta_frac = (float)i / (float)theta_segments;
        float theta = theta_start + theta_frac * theta_length;
        float cos_theta = cosf(theta);
        float sin_theta = sinf(theta);

        for (int j = 0; j < num_phi; j++) {
            float phi_frac = (float)j / (float)phi_segments;
            float radius = inner_radius + phi_frac * (outer_radius - inner_radius);

            float x = radius * cos_theta;
            float y = radius * sin_theta;
            float z = 0.0f;

            float uv_x = (cos_theta * (radius / outer_radius) + 1.0f) * 0.5f;
            float uv_y = (sin_theta * (radius / outer_radius) + 1.0f) * 0.5f;

            vertices[vertex_idx++] = x;
            vertices[vertex_idx++] = y;
            vertices[vertex_idx++] = z;
            vertices[vertex_idx++] = uv_x;
            vertices[vertex_idx++] = uv_y;
        }
    }

    int num_indices = theta_segments * phi_segments * 6;
    uint16_t* indices = (uint16_t*)malloc(num_indices * sizeof(uint16_t));
    if (!indices) {
        SDL_Log("Failed to allocate indices for ring mesh");
        free(vertices);
        return NULL;
    }

    int index_idx = 0;
    for (int i = 0; i < theta_segments; i++) {
        for (int j = 0; j < phi_segments; j++) {
            uint16_t a = (uint16_t)(i * num_phi + j);
            uint16_t b = (uint16_t)(i * num_phi + j + 1);
            uint16_t c = (uint16_t)((i + 1) * num_phi + j + 1);
            uint16_t d = (uint16_t)((i + 1) * num_phi + j);

            // Clockwise winding to match circle geometry
            indices[index_idx++] = a;
            indices[index_idx++] = b;
            indices[index_idx++] = c;

            indices[index_idx++] = a;
            indices[index_idx++] = c;
            indices[index_idx++] = d;
        }
    }

    // Upload to GPU
    SDL_GPUBuffer* vbo = NULL;
    size_t vertices_size = num_vertices * 5 * sizeof(float);
    int vbo_failed = upload_vertices(device, vertices, vertices_size, &vbo);
    free(vertices);
    if (vbo_failed) {
        free(indices);
        return NULL;  // Logging handled in upload_vertices
    }

    SDL_GPUBuffer* ibo = NULL;
    size_t indices_size = num_indices * sizeof(uint16_t);
    int ibo_failed = upload_indices(device, indices, indices_size, &ibo);
    free(indices);
    if (ibo_failed) {
        SDL_ReleaseGPUBuffer(device, vbo);
        return NULL;  // Logging handled in upload_indices
    }

    MeshComponent* out_mesh = (MeshComponent*)malloc(sizeof(MeshComponent));
    if (!out_mesh) {
        SDL_Log("Failed to allocate MeshComponent for ring");
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