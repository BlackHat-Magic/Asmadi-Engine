#include "geometry/octahedron.h"

#include <math.h>
#include <stdlib.h>

#include "math/matrix.h"
#include "ecs/ecs.h"
#include "geometry/g_common.h"

MeshComponent create_octahedron_mesh(float radius, SDL_GPUDevice* device) {
    MeshComponent null_mesh = (MeshComponent) {0};
    const int num_vertices = 6;
    float vertices[6 * 8] = {0};
    vec3 pos[6] = {
        {0.0f, 1.0f, 0.0f},   // 0: north pole
        {1.0f, 0.0f, 0.0f},   // 1
        {0.0f, 0.0f, 1.0f},   // 2
        {-1.0f, 0.0f, 0.0f},  // 3
        {0.0f, 0.0f, -1.0f},  // 4
        {0.0f, -1.0f, 0.0f}   // 5: south pole
    };

    for (int i = 0; i < 6; i++) {
        pos[i] = vec3_normalize(pos[i]);
        pos[i] = vec3_scale(pos[i], radius);
        vertices[i * 8 + 0] = pos[i].x;
        vertices[i * 8 + 1] = pos[i].y;
        vertices[i * 8 + 2] = pos[i].z;
        vertices[i * 8 + 3] = 0.0f;  // nx (placeholder)
        vertices[i * 8 + 4] = 0.0f;  // ny
        vertices[i * 8 + 5] = 0.0f;  // nz

        // Spherical UV mapping
        float u = 0.5f + atan2f(pos[i].z, pos[i].x) / (2.0f * (float)M_PI);
        float v = acosf(pos[i].y) / (float)M_PI;
        vertices[i * 8 + 6] = u;
        vertices[i * 8 + 7] = v;
    }

    Uint16 indices[] = {
        0, 2, 1,  // Clockwise for outwards normal
        0, 3, 2,
        0, 4, 3,
        0, 1, 4,
        5, 1, 2,
        5, 2, 3,
        5, 3, 4,
        5, 4, 1
    };

    // Compute normals
    compute_vertex_normals(vertices, num_vertices, indices, sizeof(indices) / sizeof(Uint16), 8, 0, 3);

    SDL_GPUBuffer* vbo = NULL;
    Uint64 vertices_size = sizeof(vertices);
    int vbo_failed = upload_vertices(device, vertices, vertices_size, &vbo);
    if (vbo_failed) return null_mesh;

    SDL_GPUBuffer* ibo = NULL;
    Uint64 indices_size = sizeof(indices);
    int ibo_failed = upload_indices(device, indices, indices_size, &ibo);
    if (ibo_failed) {
        SDL_ReleaseGPUBuffer(device, vbo);
        return null_mesh;
    }

    MeshComponent out_mesh = (MeshComponent){
        .vertex_buffer = vbo,
        .num_vertices = (Uint32)num_vertices,
        .index_buffer = ibo,
        .num_indices = sizeof(indices) / sizeof(Uint16),
        .index_size = SDL_GPU_INDEXELEMENTSIZE_16BIT
    };

    return out_mesh;
}