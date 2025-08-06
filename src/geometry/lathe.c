#include "geometry/lathe.h"

#include <math.h>
#include <stdlib.h>

#include "geometry/g_common.h"
#include "math/matrix.h"

MeshComponent* create_lathe_mesh(
    vec2* points, int num_points, int phi_segments,
    float phi_start, float phi_length, SDL_GPUDevice* device
) {
    if (num_points < 2) {
        SDL_Log("Lathe requires at least 2 points");
        return NULL;
    }
    if (phi_segments < 3) {
        SDL_Log("Lathe requires at least 3 phi segments");
        return NULL;
    }

    int num_phi = phi_segments + 1;  // Rings closed
    int num_vertices = num_points * num_phi;

    // Check for uint16_t overflow (max 65535 verts); extend to uint32_t if needed later
    if (num_vertices > 65535) {
        SDL_Log("Lathe mesh too large for uint16_t indices");
        return NULL;
    }

    float* vertices = (float*)malloc(num_vertices * 8 * sizeof(float));  // pos.x,y,z + normal.x,y,z + uv.u,v
    if (!vertices) {
        SDL_Log("Failed to allocate vertices for lathe mesh");
        return NULL;
    }

    // Generate vertices
    int vertex_idx = 0;
    for (int i = 0; i < num_points; i++) {
        float u = (float)i / (float)(num_points - 1);  // Axial UV

        for (int j = 0; j < num_phi; j++) {
            float phi_frac = (float)j / (float)phi_segments;
            float phi = phi_start + phi_frac * phi_length;
            float cos_phi = cosf(phi);
            float sin_phi = sinf(phi);

            // Position: rotate point around Y (points.x is radius, points.y is height)
            float x = points[i].x * cos_phi;
            float y = points[i].y;
            float z = points[i].x * sin_phi;

            // UV: u along path, v around phi (0-1)
            float v = phi_frac;

            vertices[vertex_idx++] = x;
            vertices[vertex_idx++] = y;
            vertices[vertex_idx++] = z;
            vertices[vertex_idx++] = 0.0f;  // nx (placeholder)
            vertices[vertex_idx++] = 0.0f;  // ny
            vertices[vertex_idx++] = 0.0f;  // nz
            vertices[vertex_idx++] = v;     // Note: may need to flip u/v based on convention
            vertices[vertex_idx++] = u;
        }
    }

    // Generate indices (quads between rings, flipped winding for outward faces)
    int num_indices = (num_points - 1) * phi_segments * 6;
    uint16_t* indices = (uint16_t*)malloc(num_indices * sizeof(uint16_t));
    if (!indices) {
        SDL_Log("Failed to allocate indices for lathe mesh");
        free(vertices);
        return NULL;
    }

    int index_idx = 0;
    for (int i = 0; i < num_points - 1; i++) {
        for (int j = 0; j < phi_segments; j++) {
            uint16_t a = (uint16_t)(i * num_phi + j);
            uint16_t b = (uint16_t)(i * num_phi + (j + 1) % phi_segments);  // Wrap phi
            uint16_t c = (uint16_t)((i + 1) * num_phi + (j + 1) % phi_segments);
            uint16_t d = (uint16_t)((i + 1) * num_phi + j);

            // Flipped winding: a -> c -> b and a -> d -> c (counterclockwise if original was clockwise)
            indices[index_idx++] = a;
            indices[index_idx++] = c;
            indices[index_idx++] = b;

            indices[index_idx++] = a;
            indices[index_idx++] = d;
            indices[index_idx++] = c;
        }
    }

    // Compute normals
    compute_vertex_normals(vertices, num_vertices, indices, num_indices, 8, 0, 3);

    // Upload to GPU
    SDL_GPUBuffer* vbo = NULL;
    size_t vertices_size = num_vertices * 8 * sizeof(float);
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