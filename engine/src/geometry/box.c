#include <ecs/ecs.h>
#include <geometry/g_common.h>
#include <geometry/box.h>

MeshComponent
create_box_mesh (float l, float w, float h, SDL_GPUDevice* device) {
    MeshComponent out_mesh = {0};

    float wx = w / 2.0f;
    float hy = h / 2.0f;
    float lz = l / 2.0f;

    float vertices[24 * 8] = {
        // front (-z)
        -wx, -hy, -lz, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, wx, -hy, -lz, 0.0f, 0.0f,
        0.0f, 1.0f, 1.0f, wx, hy, -lz, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, -wx, hy,
        -lz, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,

        // back
        -wx, -hy, lz, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, wx, -hy, lz, 0.0f, 0.0f,
        0.0f, 1.0f, 1.0f, wx, hy, lz, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, -wx, hy, lz,
        0.0f, 0.0f, 0.0f, 0.0f, 0.0f,

        // Left (x = -wx)
        -wx, hy, -lz, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, -wx, hy, lz, 0.0f, 0.0f,
        0.0f, 1.0f, 1.0f, -wx, -hy, lz, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, -wx, -hy,
        -lz, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,

        // Right (x = wx)
        wx, hy, -lz, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, wx, -hy, -lz, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, wx, -hy, lz, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, wx, hy, lz,
        0.0f, 0.0f, 0.0f, 1.0f, 0.0f,

        // Top (y = hy)
        -wx, hy, -lz, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, wx, hy, -lz, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, wx, hy, lz, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, -wx, hy, lz,
        0.0f, 0.0f, 0.0f, 0.0f, 1.0f,

        // Bottom (y = -hy)
        -wx, -hy, -lz, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, -wx, -hy, lz, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f, wx, -hy, lz, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, wx, -hy,
        -lz, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f
    };

    // 36 indices: 6 per face (2 triangles each), clockwise winding
    Uint16 indices[] = {
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

    compute_vertex_normals (vertices, 24, indices, 36, 8, 0, 3);

    SDL_GPUBuffer* vbo = NULL;
    Uint64 vertices_size = sizeof (vertices);
    int vbo_failed = upload_vertices (device, vertices, vertices_size, &vbo);
    if (vbo_failed)
        return (MeshComponent) {0}; // logging handled in upload_vertices()

    SDL_GPUBuffer* ibo = NULL;
    Uint64 indices_size = sizeof (indices);
    int ibo_failed = upload_indices (device, indices, indices_size, &ibo);
    if (ibo_failed) {
        SDL_ReleaseGPUBuffer (device, vbo);
        return (MeshComponent) {0}; // logging handled in upload_indices()
    }

    out_mesh.vertex_buffer = vbo;
    out_mesh.num_vertices = 24;
    out_mesh.index_buffer = ibo;
    out_mesh.num_indices = 36;
    out_mesh.index_size = SDL_GPU_INDEXELEMENTSIZE_16BIT;

    return out_mesh;
}