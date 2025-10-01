#include "geometry/dodecahedron.h"

#include <math.h>
#include <stdlib.h>

#include "ecs/ecs.h"
#include "geometry/g_common.h"
#include "math/matrix.h"

MeshComponent create_dodecahedron_mesh(float radius, SDL_GPUDevice* device) {
    float phi = (1.0f + sqrtf(5.0f)) / 2.0f;
    float phi_inv = 1.0f / phi;

    float raw_verts[20 * 3] = {
        1.0f, 1.0f, 1.0f,
        1.0f, 1.0f, -1.0f,
        1.0f, -1.0f, 1.0f,
        1.0f, -1.0f, -1.0f,
        -1.0f, 1.0f, 1.0f,
        -1.0f, 1.0f, -1.0f,
        -1.0f, -1.0f, 1.0f,
        -1.0f, -1.0f, -1.0f,
        phi_inv, phi, 0.0f,
        -phi_inv, phi, 0.0f,
        phi_inv, -phi, 0.0f,
        -phi_inv, -phi, 0.0f,
        phi, 0.0f, phi_inv,
        phi, 0.0f, -phi_inv,
        -phi, 0.0f, phi_inv,
        -phi, 0.0f, -phi_inv,
        0.0f, phi_inv, phi,
        0.0f, -phi_inv, phi,
        0.0f, phi_inv, -phi,
        0.0f, -phi_inv, -phi
    };

    int num_vertices = 20;
    float* vertices = (float*)malloc(num_vertices * 8 * sizeof(float));
    if (!vertices) {
        SDL_Log("Failed to allocate vertices for dodecahedron mesh");
        return (MeshComponent) {0};
    }

    int vertex_idx = 0;
    for (int i = 0; i < num_vertices; i++) {
        vec3 pos = {raw_verts[i * 3], raw_verts[i * 3 + 1], raw_verts[i * 3 + 2]};
        float len = sqrtf(pos.x * pos.x + pos.y * pos.y + pos.z * pos.z);
        pos = vec3_scale(pos, radius / len);

        vertices[vertex_idx++] = pos.x;
        vertices[vertex_idx++] = pos.y;
        vertices[vertex_idx++] = pos.z;
        vertices[vertex_idx++] = 0.0f;  // nx (placeholder)
        vertices[vertex_idx++] = 0.0f;  // ny
        vertices[vertex_idx++] = 0.0f;  // nz

        // Spherical UV mapping using normalized position
        vec3 norm_pos = vec3_scale(pos, 1.0f / radius);
        float u = 0.5f + atan2f(norm_pos.z, norm_pos.x) / (2.0f * (float)M_PI);
        float v = acosf(norm_pos.y) / (float)M_PI;
        vertices[vertex_idx++] = u;
        vertices[vertex_idx++] = v;
    }

    int num_indices = 108;
    Uint16 indices[108] = {
        1, 8, 0, 0, 12, 13, 13, 1, 0,
        4, 9, 5, 5, 15, 14, 14, 4, 5,
        2, 10, 3, 3, 13, 12, 12, 2, 3,
        7, 11, 6, 6, 14, 15, 15, 7, 6,
        2, 12, 0, 0, 16, 17, 17, 2, 0,
        1, 13, 3, 3, 19, 18, 18, 1, 3,
        4, 14, 6, 6, 17, 16, 16, 4, 6,
        7, 15, 5, 5, 18, 19, 19, 7, 5,
        4, 16, 0, 0, 8, 9, 9, 4, 0,
        2, 17, 6, 6, 11, 10, 10, 2, 6,
        1, 18, 5, 5, 9, 8, 8, 1, 5,
        7, 19, 3, 3, 10, 11, 11, 7, 3
    };

    // Compute normals
    compute_vertex_normals(vertices, num_vertices, indices, num_indices, 8, 0, 3);

    SDL_GPUBuffer* vbo = NULL;
    Uint64 vertices_size = num_vertices * 8 * sizeof(float);
    int vbo_failed = upload_vertices(device, vertices, vertices_size, &vbo);
    free(vertices);
    if (vbo_failed) return (MeshComponent) {0};

    SDL_GPUBuffer* ibo = NULL;
    Uint64 indices_size = num_indices * sizeof(Uint16);
    int ibo_failed = upload_indices(device, indices, indices_size, &ibo);
    if (ibo_failed) {
        SDL_ReleaseGPUBuffer(device, vbo);
        return (MeshComponent) {0};
    }
    
    MeshComponent out_mesh = (MeshComponent){
        .vertex_buffer = vbo,
        .num_vertices = (Uint32)num_vertices,
        .index_buffer = ibo,
        .num_indices = (Uint32)num_indices,
        .index_size = SDL_GPU_INDEXELEMENTSIZE_16BIT
    };

    return out_mesh;
}