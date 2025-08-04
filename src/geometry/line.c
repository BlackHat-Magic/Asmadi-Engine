#include "line.h"
#include "geometry/g_common.h"
#include <stdlib.h>

MeshComponent* create_line_mesh_from_points(vec3* points, int num_points, SDL_GPUDevice* device) {
    if (num_points < 2) return NULL;

    // Vertices: just positions (float3), no texcoords for debug line
    float* vertices = (float*)malloc(num_points * 3 * sizeof(float));
    for (int i = 0; i < num_points; i++) {
        vertices[i * 3 + 0] = points[i].x;
        vertices[i * 3 + 1] = points[i].y;
        vertices[i * 3 + 2] = points[i].z;
    }

    SDL_GPUBuffer* vbo = NULL;
    size_t vertices_size = num_points * 3 * sizeof(float);
    int vbo_failed = upload_vertices(device, vertices, vertices_size, &vbo);
    free(vertices);
    if (vbo_failed) return NULL;

    // No indices for linestrip
    MeshComponent* out_mesh = (MeshComponent*)malloc(sizeof(MeshComponent));
    *out_mesh = (MeshComponent){
        .vertex_buffer = vbo,
        .num_vertices = (uint32_t)num_points,
        .index_buffer = NULL,
        .num_indices = 0,
        .index_size = SDL_GPU_INDEXELEMENTSIZE_16BIT  // Unused
    };
    return out_mesh;
}