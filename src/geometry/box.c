#include "geometry/box.h"

#include <stdlib.h>

#include "ecs/ecs.h"
#include "geometry/common.h"

MeshComponent* create_box_mesh(
    float l, float w, float h, SDL_GPUDevice* device
) {
    float wx = w / 2.0f;
    float hy = h / 2.0f;
    float lz = l / 2.0f;

    float vertices[] = {
        // front (-z)
        -wx, -hy, -lz, 0.0f, 0.0f, wx, -hy, -lz, 1.0f, 0.0f, wx, hy, -lz, 1.0f,
        1.0f, -wx, hy, -lz, 0.0f, 1.0f,

        // back
        -wx, -hy, lz, 0.0f, 0.0f, wx, -hy, lz, 1.0f, 0.0f, wx, hy, lz, 1.0f,
        1.0f, -wx, hy, lz, 0.0f, 1.0f,

        // Left (x = -wx)
        -wx, hy, -lz, 1.0f, 1.0f, -wx, hy, lz, 1.0f, 0.0f, -wx, -hy, lz, 0.0f,
        0.0f, -wx, -hy, -lz, 0.0f, 1.0f,

        // Right (x = wx)
        wx, hy, -lz, 1.0f, 0.0f, wx, -hy, -lz, 0.0f, 0.0f, wx, -hy, lz, 0.0f,
        1.0f, wx, hy, lz, 1.0f, 1.0f,

        // Top (y = hy)
        -wx, hy, -lz, 0.0f, 1.0f, wx, hy, -lz, 1.0f, 1.0f, wx, hy, lz, 1.0f,
        0.0f, -wx, hy, lz, 0.0f, 0.0f,

        // Bottom (y = -hy)
        -wx, -hy, -lz, 0.0f, 0.0f, -wx, -hy, lz, 0.0f, 1.0f, wx, -hy, lz, 1.0f,
        1.0f, wx, -hy, -lz, 1.0f, 0.0f
    };

    // 36 indices: 6 per face (2 triangles each), clockwise winding
    uint16_t indices[] = {
        // Front
        0, 2, 1, 2, 0, 3,
        // Back (unchanged)
        4, 5, 6, 6, 7, 4,
        // Left
        9, 8, 11, 11, 10, 9,
        // Right
        15, 13, 12, 13, 15, 14,
        // Top
        16, 18, 17, 18, 16, 19,
        // Bottom (unchanged)
        20, 23, 22, 22, 21, 20
    };

    SDL_GPUBuffer* vbo = NULL;
    int vbo_failed = upload_vertices(device, vertices, sizeof(vertices), &vbo);
    if (vbo_failed) return NULL;  // logging handled in upload_vertices()

    SDL_GPUBuffer* ibo = NULL;
    int ibo_failed     = upload_indices(device, indices, sizeof(indices), &ibo);
    if (ibo_failed) return NULL;  // logging handled in upload_indices()

    MeshComponent* out_mesh = (MeshComponent*)malloc(sizeof(MeshComponent));
    *out_mesh               = (MeshComponent){.vertex_buffer = vbo,
                                              .num_vertices  = 24,
                                              .index_buffer  = ibo,
                                              .num_indices   = 36,
                                              .index_size = SDL_GPU_INDEXELEMENTSIZE_16BIT};

    return out_mesh;
}