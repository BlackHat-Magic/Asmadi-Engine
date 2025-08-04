#include "geometry/lathe.h"

#include <math.h>
#include <stdlib.h>

#include "geometry/g_common.h"

MeshComponent* create_lathe_mesh(
    vec2* points, int num_points, int radial_segments,
    float phi_start, float phi_length, SDL_GPUDevice* device
) {
    if (num_points < 2 || radial_segments < 1) {
        SDL_Log("Invalid lathe parameters: need at least 2 points and 1 radial segment");
        return NULL;
    }

    // Vertex count: (radial_segments + 1) radials x num_points axials
    int num_vertices = (radial_segments + 1) * num_points;
    if (num_vertices > 65535) {  // uint16_t index limit; extend to uint32_t if needed
        SDL_Log("Lathe mesh too large for uint16_t indices");
        return NULL;
    }

    float* vertices = (float*)malloc(num_vertices * 5 * sizeof(float));  // pos.x,y,z + uv.u,v
    if (!vertices) {
        SDL_Log("Failed to allocate vertices for lathe mesh");
        return NULL;
    }

    // Generate vertices
    int vertex_idx = 0;
    for (int i = 0; i <= radial_segments; i++) {
        float u = (float)i / (float)radial_segments;
        float phi = phi_start + u * phi_length;

        float cos_phi = cosf(phi);
        float sin_phi = sinf(phi);

        for (int j = 0; j < num_points; j++) {
            float v = (float)j / (float)(num_points - 1);

            // 3D position: rotate around Y (x,z plane)
            float x = points[j].x * sin_phi;
            float y = points[j].y;
            float z = points[j].x * -cos_phi;  // Flipped to reverse rotation direction for clockwise winding

            vertices[vertex_idx++] = x;
            vertices[vertex_idx++] = y;
            vertices[vertex_idx++] = z;
            vertices[vertex_idx++] = u;  // UV u: around
            vertices[vertex_idx++] = v;  // UV v: along path (flip if textures invert)
        }
    }

    // Index count: radial_segments x (num_points - 1) x 6 (two tris per quad)
    int num_indices = radial_segments * (num_points - 1) * 6;
    uint16_t* indices = (uint16_t*)malloc(num_indices * sizeof(uint16_t));
    if (!indices) {
        SDL_Log("Failed to allocate indices for lathe mesh");
        free(vertices);
        return NULL;
    }

    // Generate indices (clockwise winding, like your box/plane)
    int index_idx = 0;
    for (int i = 0; i < radial_segments; i++) {
        for (int j = 0; j < num_points - 1; j++) {
            uint16_t a = (uint16_t)(i * num_points + j);
            uint16_t b = (uint16_t)(i * num_points + (j + 1));
            uint16_t c = (uint16_t)((i + 1) * num_points + (j + 1));
            uint16_t d = (uint16_t)((i + 1) * num_points + j);

            // Triangle 1: a -> b -> c (clockwise)
            indices[index_idx++] = a;
            indices[index_idx++] = b;
            indices[index_idx++] = c;

            // Triangle 2: a -> c -> d (clockwise)
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
        return NULL;  // Logging in upload_vertices
    }

    SDL_GPUBuffer* ibo = NULL;
    size_t indices_size = num_indices * sizeof(uint16_t);
    int ibo_failed = upload_indices(device, indices, indices_size, &ibo);
    free(indices);
    if (ibo_failed) {
        SDL_ReleaseGPUBuffer(device, vbo);
        return NULL;  // Logging in upload_indices
    }

    MeshComponent* out_mesh = (MeshComponent*)malloc(sizeof(MeshComponent));
    if (!out_mesh) {
        SDL_Log("Failed to allocate MeshComponent for lathe");
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