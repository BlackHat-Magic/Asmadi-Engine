#include "geometry/cylinder.h"

#include <math.h>
#include <stdlib.h>

#include "geometry/g_common.h"

MeshComponent* create_cylinder_mesh(
    float radius_top, float radius_bottom, float height,
    int radial_segments, int height_segments, bool open_ended,
    float theta_start, float theta_length, SDL_GPUDevice* device
) {
    if (radial_segments < 3) {
        SDL_Log("Cylinder must have at least 3 radial segments");
        return NULL;
    }
    if (height_segments < 1) {
        SDL_Log("Cylinder must have at least 1 height segment");
        return NULL;
    }

    bool closed = (fabsf(theta_length - 2.0f * (float)M_PI) < 0.0001f);

    int num_radials = radial_segments + 1;
    int num_side_vertices = num_radials * (height_segments + 1);

    int num_bottom_cap_vertices = 0;
    int num_top_cap_vertices = 0;
    if (!open_ended) {
        if (radius_bottom > 0.0f) {
            num_bottom_cap_vertices = radial_segments + 1;
        }
        if (radius_top > 0.0f) {
            num_top_cap_vertices = radial_segments + 1;
        }
    }

    int num_vertices = num_side_vertices + num_bottom_cap_vertices + num_top_cap_vertices;

    if (num_vertices > 65535) {
        SDL_Log("Cylinder mesh too large for uint16_t indices");
        return NULL;
    }

    float* vertices = (float*)malloc(num_vertices * 5 * sizeof(float));  // pos.x,y,z + uv.u,v
    if (!vertices) {
        SDL_Log("Failed to allocate vertices for cylinder mesh");
        return NULL;
    }

    // Generate side vertices
    int vertex_idx = 0;
    for (int i = 0; i < num_radials; i++) {
        float u = (float)i / (float)radial_segments;
        float phi = theta_start + u * theta_length;

        float cos_phi = cosf(phi);
        float sin_phi = sinf(phi);

        for (int j = 0; j <= height_segments; j++) {
            float v = (float)j / (float)height_segments;
            float current_radius = radius_bottom + (radius_top - radius_bottom) * v;
            float y = -height * 0.5f + height * v;

            float x = current_radius * sin_phi;
            float z = current_radius * -cos_phi;  // Flipped for clockwise winding consistency

            vertices[vertex_idx++] = x;
            vertices[vertex_idx++] = y;
            vertices[vertex_idx++] = z;
            vertices[vertex_idx++] = u;
            vertices[vertex_idx++] = 1.0f - v;  // Flip v to match standard (0 at top, 1 at bottom)
        }
    }

    // Generate bottom cap vertices if needed
    int bottom_cap_start = num_side_vertices;
    if (num_bottom_cap_vertices > 0) {
        // Rim vertices with polar UVs
        for (int i = 0; i < radial_segments; i++) {  // < radial_segments (no seam duplicate for cap)
            float u = (float)i / (float)radial_segments;
            float phi = theta_start + u * theta_length;

            float sin_phi = sinf(phi);
            float cos_phi = cosf(phi);

            float x = radius_bottom * sin_phi;
            float y = -height * 0.5f;
            float z = radius_bottom * -cos_phi;

            float uv_x = (sin_phi + 1.0f) * 0.5f;
            float uv_y = (-cos_phi + 1.0f) * 0.5f;

            vertices[vertex_idx++] = x;
            vertices[vertex_idx++] = y;
            vertices[vertex_idx++] = z;
            vertices[vertex_idx++] = uv_x;
            vertices[vertex_idx++] = uv_y;
        }

        // Center vertex
        vertices[vertex_idx++] = 0.0f;
        vertices[vertex_idx++] = -height * 0.5f;
        vertices[vertex_idx++] = 0.0f;
        vertices[vertex_idx++] = 0.5f;
        vertices[vertex_idx++] = 0.5f;
    }

    // Generate top cap vertices if needed
    int top_cap_start = bottom_cap_start + num_bottom_cap_vertices;
    if (num_top_cap_vertices > 0) {
        // Rim vertices with polar UVs
        for (int i = 0; i < radial_segments; i++) {
            float u = (float)i / (float)radial_segments;
            float phi = theta_start + u * theta_length;

            float sin_phi = sinf(phi);
            float cos_phi = cosf(phi);

            float x = radius_top * sin_phi;
            float y = height * 0.5f;
            float z = radius_top * -cos_phi;

            float uv_x = (sin_phi + 1.0f) * 0.5f;
            float uv_y = (-cos_phi + 1.0f) * 0.5f;  // Same as bottom; adjust if texture flipping needed

            vertices[vertex_idx++] = x;
            vertices[vertex_idx++] = y;
            vertices[vertex_idx++] = z;
            vertices[vertex_idx++] = uv_x;
            vertices[vertex_idx++] = uv_y;
        }

        // Center vertex
        vertices[vertex_idx++] = 0.0f;
        vertices[vertex_idx++] = height * 0.5f;
        vertices[vertex_idx++] = 0.0f;
        vertices[vertex_idx++] = 0.5f;
        vertices[vertex_idx++] = 0.5f;
    }

    // Indices for sides
    int num_side_indices = radial_segments * height_segments * 6;

    int num_bottom_cap_indices = 0;
    int num_top_cap_indices = 0;
    if (!open_ended) {
        if (radius_bottom > 0.0f) {
            num_bottom_cap_indices = (radial_segments - 1) * 3;
            if (closed) num_bottom_cap_indices += 3;
        }
        if (radius_top > 0.0f) {
            num_top_cap_indices = (radial_segments - 1) * 3;
            if (closed) num_top_cap_indices += 3;
        }
    }

    int num_indices = num_side_indices + num_bottom_cap_indices + num_top_cap_indices;

    uint16_t* indices = (uint16_t*)malloc(num_indices * sizeof(uint16_t));
    if (!indices) {
        SDL_Log("Failed to allocate indices for cylinder mesh");
        free(vertices);
        return NULL;
    }

    // Generate side indices (clockwise)
    int index_idx = 0;
    int num_points = height_segments + 1;
    for (int i = 0; i < radial_segments; i++) {
        for (int j = 0; j < height_segments; j++) {
            uint16_t a = (uint16_t)(i * num_points + j);
            uint16_t b = (uint16_t)(i * num_points + (j + 1));
            uint16_t c = (uint16_t)((i + 1) * num_points + (j + 1));
            uint16_t d = (uint16_t)((i + 1) * num_points + j);

            // Triangle 1: a b c
            indices[index_idx++] = a;
            indices[index_idx++] = b;
            indices[index_idx++] = c;

            // Triangle 2: a c d
            indices[index_idx++] = a;
            indices[index_idx++] = c;
            indices[index_idx++] = d;
        }
    }

    // Generate bottom cap indices if needed
    if (num_bottom_cap_indices > 0) {
        uint16_t center = (uint16_t)(bottom_cap_start + radial_segments);
        // Forward winding for bottom
        for (int i = 0; i < radial_segments - 1; i++) {
            indices[index_idx++] = center;
            indices[index_idx++] = (uint16_t)(bottom_cap_start + i);
            indices[index_idx++] = (uint16_t)(bottom_cap_start + i + 1);
        }
        if (closed) {
            indices[index_idx++] = center;
            indices[index_idx++] = (uint16_t)(bottom_cap_start + radial_segments - 1);
            indices[index_idx++] = (uint16_t)(bottom_cap_start + 0);
        }
    }

    // Generate top cap indices if needed
    if (num_top_cap_indices > 0) {
        uint16_t center = (uint16_t)(top_cap_start + radial_segments);
        // Reversed winding for top
        for (int i = 0; i < radial_segments - 1; i++) {
            indices[index_idx++] = center;
            indices[index_idx++] = (uint16_t)(top_cap_start + i + 1);
            indices[index_idx++] = (uint16_t)(top_cap_start + i);
        }
        if (closed) {
            indices[index_idx++] = center;
            indices[index_idx++] = (uint16_t)(top_cap_start + 0);
            indices[index_idx++] = (uint16_t)(top_cap_start + radial_segments - 1);
        }
    }

    // Upload to GPU
    SDL_GPUBuffer* vbo = NULL;
    size_t vertices_size = num_vertices * 5 * sizeof(float);
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