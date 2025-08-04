#include "geometry/icosahedron.h"

#include <math.h>
#include <stdlib.h>

#include "geometry/g_common.h"
#include "math/matrix.h"

MeshComponent* create_icosahedron_mesh(float radius, SDL_GPUDevice* device) {
    const int num_vertices = 12;
    float vertices[12 * 5] = {0};
    float t = (1.0f + sqrtf(5.0f)) / 2.0f;
    vec3 pos[12] = {
        {-1.0f, t, 0.0f},    // 0
        {1.0f, t, 0.0f},     // 1
        {-1.0f, -t, 0.0f},   // 2
        {1.0f, -t, 0.0f},    // 3
        {0.0f, -1.0f, t},    // 4
        {0.0f, 1.0f, t},     // 5
        {0.0f, -1.0f, -t},   // 6
        {0.0f, 1.0f, -t},    // 7
        {t, 0.0f, -1.0f},    // 8
        {t, 0.0f, 1.0f},     // 9
        {-t, 0.0f, -1.0f},   // 10
        {-t, 0.0f, 1.0f}     // 11
    };

    for (int i = 0; i < 12; i++) {
        pos[i] = vec3_normalize(pos[i]);
        pos[i] = vec3_scale(pos[i], radius);
        vertices[i * 5 + 0] = pos[i].x;
        vertices[i * 5 + 1] = pos[i].y;
        vertices[i * 5 + 2] = pos[i].z;

        // Spherical UV mapping
        float u = 0.5f + atan2f(pos[i].z, pos[i].x) / (2.0f * (float)M_PI);
        float v = acosf(pos[i].y) / (float)M_PI;
        vertices[i * 5 + 3] = u;
        vertices[i * 5 + 4] = v;
    }
    
    uint16_t indices[] = {
        11, 5, 0,
        5, 1, 0,
        1, 7, 0,
        7, 10, 0,
        10, 11, 0,
        5, 9, 1,
        11, 4, 5,
        10, 2, 11,
        7, 6, 10,
        1, 8, 7,
        9, 4, 3,
        4, 2, 3,
        2, 6, 3,
        6, 8, 3,
        8, 9, 3,
        9, 5, 4,
        4, 11, 2,
        2, 10, 6,
        6, 7, 8,
        8, 1, 9
    };

    SDL_GPUBuffer* vbo = NULL;
    size_t vertices_size = sizeof(vertices);
    int vbo_failed = upload_vertices(device, vertices, vertices_size, &vbo);
    if (vbo_failed) return NULL;

    SDL_GPUBuffer* ibo = NULL;
    size_t indices_size = sizeof(indices);
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
        .num_indices = sizeof(indices) / sizeof(uint16_t),
        .index_size = SDL_GPU_INDEXELEMENTSIZE_16BIT
    };

    return out_mesh;
}