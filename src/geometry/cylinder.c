#include "geometry/cylinder.h"

#include <math.h>
#include <stdlib.h>

#include "geometry/g_common.h"

MeshComponent* create_cylinder_mesh(
    float radius_bottom, float radius_top, float height, int radial_segments,
    int height_segments, bool open_ended, float theta_start, float theta_length,
    SDL_GPUDevice* device
) {
    if (radial_segments < 3) {
        SDL_Log("Cylinder must have at least 3 radial segments");
        return NULL;
    }
    if (height_segments < 1) height_segments = 1;

    // Side vertices
    int num_side_rows = height_segments + 1;
    int num_side_cols = radial_segments + 1;
    int num_side_vertices = num_side_rows * num_side_cols;

    // Cap vertices (centers + rings, separate for flat normals)
    int num_cap_vertices = 0;
    bool add_bottom_cap = !open_ended && (radius_bottom > 0.0f);
    bool add_top_cap = !open_ended && (radius_top > 0.0f);
    if (add_bottom_cap) num_cap_vertices += radial_segments + 1;
    if (add_top_cap) num_cap_vertices += radial_segments + 1;

    int num_vertices = num_side_vertices + num_cap_vertices;

    if (num_vertices > 65535) {
        SDL_Log("Cylinder mesh too large for uint16_t indices");
        return NULL;
    }

    float* vertices = (float*)malloc(num_vertices * 8 * sizeof(float));  // pos.x,y,z + normal.x,y,z + uv.u,v
    if (!vertices) {
        SDL_Log("Failed to allocate vertices for cylinder mesh");
        return NULL;
    }

    // Side indices
    int num_side_indices = height_segments * radial_segments * 6;

    // Cap indices
    int num_cap_indices = 0;
    if (add_bottom_cap) num_cap_indices += radial_segments * 3;
    if (add_top_cap) num_cap_indices += radial_segments * 3;

    int num_indices = num_side_indices + num_cap_indices;

    uint16_t* indices = (uint16_t*)malloc(num_indices * sizeof(uint16_t));
    if (!indices) {
        SDL_Log("Failed to allocate indices for cylinder mesh");
        free(vertices);
        return NULL;
    }

    // Generate side vertices
    int vertex_idx = 0;
    float half_height = height / 2.0f;
    for (int iy = 0; iy < num_side_rows; iy++) {
        float v = (float)iy / (float)height_segments;
        float current_radius = radius_bottom * (1.0f - v) + radius_top * v;
        float y_pos = -half_height + height * v;

        for (int ix = 0; ix < num_side_cols; ix++) {
            float u = (float)ix / (float)radial_segments;
            float theta = theta_start + u * theta_length;
            float sin_theta = sinf(theta);
            float cos_theta = cosf(theta);

            vertices[vertex_idx++] = current_radius * sin_theta;
            vertices[vertex_idx++] = y_pos;
            vertices[vertex_idx++] = current_radius * cos_theta;
            vertices[vertex_idx++] = 0.0f;  // nx (placeholder)
            vertices[vertex_idx++] = 0.0f;  // ny
            vertices[vertex_idx++] = 0.0f;  // nz
            vertices[vertex_idx++] = u;
            vertices[vertex_idx++] = 1.0f - v;
        }
    }

    // Generate side indices (unflipped winding to correct outward normals)
    int index_idx = 0;
    for (int iy = 0; iy < height_segments; iy++) {
        for (int ix = 0; ix < radial_segments; ix++) {
            uint16_t a = (uint16_t)(iy * num_side_cols + ix);
            uint16_t b = (uint16_t)(iy * num_side_cols + ix + 1);
            uint16_t c = (uint16_t)((iy + 1) * num_side_cols + ix + 1);
            uint16_t d = (uint16_t)((iy + 1) * num_side_cols + ix);

            indices[index_idx++] = a;
            indices[index_idx++] = b;
            indices[index_idx++] = c;

            indices[index_idx++] = a;
            indices[index_idx++] = c;
            indices[index_idx++] = d;
        }
    }

    // Bottom cap
    uint16_t bottom_center_index = 0;
    if (add_bottom_cap) {
        bottom_center_index = (uint16_t)(num_side_vertices);  // Current vertex count
        vertices[vertex_idx++] = 0.0f;
        vertices[vertex_idx++] = -half_height;
        vertices[vertex_idx++] = 0.0f;
        vertices[vertex_idx++] = 0.0f;  // nx
        vertices[vertex_idx++] = -1.0f; // ny
        vertices[vertex_idx++] = 0.0f;  // nz
        vertices[vertex_idx++] = 0.5f;  // u
        vertices[vertex_idx++] = 0.5f;  // v

        uint16_t bottom_ring_start = bottom_center_index + 1;
        for (int i = 0; i < radial_segments; i++) {
            float theta = theta_start + ((float)i / (float)radial_segments) * theta_length;
            float sin_theta = sinf(theta);
            float cos_theta = cosf(theta);

            vertices[vertex_idx++] = radius_bottom * sin_theta;
            vertices[vertex_idx++] = -half_height;
            vertices[vertex_idx++] = radius_bottom * cos_theta;
            vertices[vertex_idx++] = 0.0f;  // nx
            vertices[vertex_idx++] = -1.0f; // ny
            vertices[vertex_idx++] = 0.0f;  // nz
            vertices[vertex_idx++] = 0.5f + 0.5f * sin_theta;
            vertices[vertex_idx++] = 0.5f + 0.5f * cos_theta;
        }

        // Bottom indices (order for normal -y)
        for (int i = 0; i < radial_segments; i++) {
            indices[index_idx++] = bottom_center_index;
            indices[index_idx++] = bottom_ring_start + ((i + 1) % radial_segments);
            indices[index_idx++] = bottom_ring_start + i;
        }
    }

    // Top cap
    uint16_t top_center_index = 0;
    if (add_top_cap) {
        top_center_index = (uint16_t)((add_bottom_cap ? num_side_vertices + radial_segments + 1 : num_side_vertices));
        vertices[vertex_idx++] = 0.0f;
        vertices[vertex_idx++] = half_height;
        vertices[vertex_idx++] = 0.0f;
        vertices[vertex_idx++] = 0.0f;  // nx
        vertices[vertex_idx++] = 1.0f;  // ny
        vertices[vertex_idx++] = 0.0f;  // nz
        vertices[vertex_idx++] = 0.5f;  // u
        vertices[vertex_idx++] = 0.5f;  // v

        uint16_t top_ring_start = top_center_index + 1;
        for (int i = 0; i < radial_segments; i++) {
            float theta = theta_start + ((float)i / (float)radial_segments) * theta_length;
            float sin_theta = sinf(theta);
            float cos_theta = cosf(theta);

            vertices[vertex_idx++] = radius_top * sin_theta;
            vertices[vertex_idx++] = half_height;
            vertices[vertex_idx++] = radius_top * cos_theta;
            vertices[vertex_idx++] = 0.0f;  // nx
            vertices[vertex_idx++] = 1.0f;  // ny
            vertices[vertex_idx++] = 0.0f;  // nz
            vertices[vertex_idx++] = 0.5f + 0.5f * sin_theta;
            vertices[vertex_idx++] = 0.5f + 0.5f * cos_theta;
        }

        // Top indices (order for normal +y)
        for (int i = 0; i < radial_segments; i++) {
            indices[index_idx++] = top_center_index;
            indices[index_idx++] = top_ring_start + i;
            indices[index_idx++] = top_ring_start + ((i + 1) % radial_segments);
        }
    }

    // Compute normals for sides (caps already set manually)
    compute_vertex_normals(vertices, num_vertices, indices, num_indices, 8, 0, 3);

    // Upload to GPU
    SDL_GPUBuffer* vbo = NULL;
    size_t vertices_size = num_vertices * 8 * sizeof(float);
    int vbo_failed = upload_vertices(device, vertices, vertices_size, &vbo);
    free(vertices);
    if (vbo_failed) {
        free(indices);
        return NULL;
    }

    SDL_GPUBuffer* ibo = NULL;
    size_t indices_size = num_indices * sizeof(uint16_t);
    int ibo_failed = upload_indices(device, indices, indices_size, &ibo);
    free(indices);
    if (ibo_failed) {
        SDL_ReleaseGPUBuffer(device, vbo);
        return NULL;
    }

    MeshComponent* out_mesh = (MeshComponent*)malloc(sizeof(MeshComponent));
    if (!out_mesh) {
        SDL_Log("Failed to allocate MeshComponent for cylinder");
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