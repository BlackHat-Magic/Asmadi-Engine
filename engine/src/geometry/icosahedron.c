#include <math.h>
#include <stdlib.h>

#include <math/matrix.h>
#include <geometry/g_common.h>
#include <geometry/icosahedron.h>

MeshComponent create_icosahedron_mesh (float radius, SDL_GPUDevice* device) {
    MeshComponent null_mesh = (MeshComponent) {0};
    const int num_vertices = 12;
    float* vertices = (float*) malloc (num_vertices * 8 * sizeof (float));
    if (!vertices) {
        SDL_Log ("Failed to allocate vertices for icosahedron mesh");
        return null_mesh;
    }

    float t = (1.0f + sqrtf (5.0f)) / 2.0f;
    vec3 pos[12] = {
        {-1.0f, t, 0.0f},  // 0
        {1.0f, t, 0.0f},   // 1
        {-1.0f, -t, 0.0f}, // 2
        {1.0f, -t, 0.0f},  // 3
        {0.0f, -1.0f, t},  // 4
        {0.0f, 1.0f, t},   // 5
        {0.0f, -1.0f, -t}, // 6
        {0.0f, 1.0f, -t},  // 7
        {t, 0.0f, -1.0f},  // 8
        {t, 0.0f, 1.0f},   // 9
        {-t, 0.0f, -1.0f}, // 10
        {-t, 0.0f, 1.0f}   // 11
    };

    int vertex_idx = 0;
    for (int i = 0; i < 12; i++) {
        pos[i] = vec3_normalize (pos[i]);
        pos[i] = vec3_scale (pos[i], radius);
        vertices[vertex_idx++] = pos[i].x;
        vertices[vertex_idx++] = pos[i].y;
        vertices[vertex_idx++] = pos[i].z;
        vertices[vertex_idx++] = 0.0f; // nx (placeholder)
        vertices[vertex_idx++] = 0.0f; // ny
        vertices[vertex_idx++] = 0.0f; // nz

        // Spherical UV mapping
        float u = 0.5f + atan2f (pos[i].z, pos[i].x) / (2.0f * (float) M_PI);
        float v = acosf (pos[i].y) / (float) M_PI;
        vertices[vertex_idx++] = u;
        vertices[vertex_idx++] = v;
    }

    Uint16 indices[] = {
        11, 5, 0, 5, 1, 0, 1, 7, 0, 7, 10, 0, 10, 11, 0, 5, 9, 1, 11, 4, 5, 10,
        2, 11, 10, 2, 7, // Fixed from original (was 10, 2,
                         // 11 but repeated; assuming
                         // correction based on standard
                         // icosahedron)
        10, 6, 7,        // Adjusted for correct triangles
        1, 8, 7, 9, 4, 3, 4, 2, 3, 2, 6, 3, 6, 8, 3, 8, 9, 3, 9, 5, 4, 4, 11, 2,
        2, 10, 6, 6, 7, 8, 8, 1, 9
    };

    // Note: The indices in the original code seem incomplete or erroneous (only
    // 60 indices but listed less); assuming standard icosahedron indices
    // Standard icosahedron has 20 faces, 60 indices. Here using a corrected
    // list:
    Uint16 standard_indices[60] = {0,  5,  1,  0, 1, 7,  0, 7,  10, 0,  10, 11,
                                   0,  11, 5,  1, 5, 9,  5, 11, 4,  11, 10, 2,
                                   10, 7,  6,  7, 1, 8,  3, 9,  4,  3,  4,  2,
                                   3,  2,  6,  3, 6, 8,  3, 8,  9,  4,  9,  5,
                                   2,  4,  11, 6, 2, 10, 8, 6,  7,  9,  8,  1};

    // Compute normals using standard_indices
    compute_vertex_normals (
        vertices, num_vertices, standard_indices, 60, 8, 0, 3
    );

    SDL_GPUBuffer* vbo = NULL;
    Uint64 vertices_size = num_vertices * 8 * sizeof (float);
    int vbo_failed = upload_vertices (device, vertices, vertices_size, &vbo);
    free (vertices);
    if (vbo_failed) return null_mesh;

    SDL_GPUBuffer* ibo = NULL;
    Uint64 indices_size = 60 * sizeof (Uint16);
    int ibo_failed =
        upload_indices (device, standard_indices, indices_size, &ibo);
    if (ibo_failed) {
        SDL_ReleaseGPUBuffer (device, vbo);
        return null_mesh;
    }

    MeshComponent out_mesh =
        (MeshComponent) {.vertex_buffer = vbo,
                         .num_vertices = (Uint32) num_vertices,
                         .index_buffer = ibo,
                         .num_indices = 60,
                         .index_size = SDL_GPU_INDEXELEMENTSIZE_16BIT};

    return out_mesh;
}