#include "geometry/dodecahedron.h"

#include <math.h>
#include <stdlib.h>

#include "geometry/g_common.h"
#include "math/matrix.h"

MeshComponent* create_dodecahedron_mesh(float radius, SDL_GPUDevice* device) {
    const int num_vertices = 20;
    float vertices[20 * 5] = {0};
    float phi = (1.0f + sqrtf(5.0f)) / 2.0f;
    float inv_phi = 1.0f / phi;
    vec3 pos[20] = {
        {0.0f, inv_phi, phi},     // 0
        {0.0f, -inv_phi, phi},    // 1
        {0.0f, -inv_phi, -phi},   // 2
        {0.0f, inv_phi, -phi},    // 3
        {phi, 0.0f, inv_phi},     // 4
        {-phi, 0.0f, inv_phi},    // 5
        {-phi, 0.0f, -inv_phi},   // 6
        {phi, 0.0f, -inv_phi},    // 7
        {inv_phi, phi, 0.0f},     // 8
        {-inv_phi, phi, 0.0f},    // 9
        {-inv_phi, -phi, 0.0f},   // 10
        {inv_phi, -phi, 0.0f},    // 11
        {1.0f, 1.0f, 1.0f},       // 12
        {-1.0f, 1.0f, 1.0f},      // 13
        {-1.0f, -1.0f, 1.0f},     // 14
        {1.0f, -1.0f, 1.0f},      // 15
        {1.0f, -1.0f, -1.0f},     // 16
        {1.0f, 1.0f, -1.0f},      // 17
        {-1.0f, 1.0f, -1.0f},     // 18
        {-1.0f, -1.0f, -1.0f}     // 19
    };

    for (int i = 0; i < 20; i++) {
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

    // 12 pentagons (clockwise winding for outwards normals, matching engine config)
    uint16_t pentagons[12 * 5] = {
        0, 1, 15, 4, 12,    // Face 0 (CW)
        0, 13, 9, 8, 12,    // Face 1
        0, 1, 14, 5, 13,    // Face 2
        1, 15, 11, 10, 14,  // Face 3
        2, 16, 7, 17, 3,    // Face 4
        2, 19, 10, 11, 16,  // Face 5
        2, 3, 18, 6, 19,    // Face 6
        3, 17, 8, 9, 18,    // Face 7
        15, 4, 7, 16, 11,   // Face 8
        4, 12, 8, 17, 7,    // Face 9
        13, 5, 6, 18, 9,    // Face 10
        5, 14, 10, 19, 6    // Face 11
    };

    // 108 indices: 12 pentagons * 3 triangles * 3 verts (fan triangulation)
    uint16_t indices[108] = {0};
    int idx = 0;
    for (int p = 0; p < 12; p++) {
        uint16_t v0 = pentagons[p * 5 + 0];
        uint16_t v1 = pentagons[p * 5 + 1];
        uint16_t v2 = pentagons[p * 5 + 2];
        uint16_t v3 = pentagons[p * 5 + 3];
        uint16_t v4 = pentagons[p * 5 + 4];
        indices[idx++] = v0;
        indices[idx++] = v1;
        indices[idx++] = v2;
        indices[idx++] = v0;
        indices[idx++] = v2;
        indices[idx++] = v3;
        indices[idx++] = v0;
        indices[idx++] = v3;
        indices[idx++] = v4;
    }

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
        .num_indices = 108,
        .index_size = SDL_GPU_INDEXELEMENTSIZE_16BIT
    };

    return out_mesh;
}