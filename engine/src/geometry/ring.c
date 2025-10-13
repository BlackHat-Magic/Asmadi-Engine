#include <math.h>
#include <stdlib.h>

#include <geometry/ring.h>
#include <geometry/g_common.h>

MeshComponent create_ring_mesh (
    float inner_radius,
    float outer_radius,
    int theta_segments,
    int phi_segments,
    float theta_start,
    float theta_length,
    SDL_GPUDevice* device
) {
    MeshComponent null_mesh = (MeshComponent) {0};
    if (theta_segments < 3) {
        SDL_Log ("Ring must have at least 3 theta segments");
        return null_mesh;
    }
    if (phi_segments < 1) {
        SDL_Log ("Ring must have at least 1 phi segment");
        return null_mesh;
    }
    if (inner_radius >= outer_radius) {
        SDL_Log ("Inner radius must be less than outer radius");
        return null_mesh;
    }

    int num_theta = theta_segments + 1;
    int num_phi = phi_segments + 1;
    int num_vertices = num_theta * num_phi;

    if (num_vertices > 65535) {
        SDL_Log ("Ring mesh too large for Uint16 indices");
        return null_mesh;
    }

    float* vertices = (float*) malloc (
        num_vertices * 8 * sizeof (float)
    ); // pos.x,y,z + uv.u,v
    if (!vertices) {
        SDL_Log ("Failed to allocate vertices for ring mesh");
        return null_mesh;
    }

    int vertex_idx = 0;
    for (int i = 0; i < num_theta; i++) {
        float theta_frac = (float) i / (float) theta_segments;
        float theta = theta_start + theta_frac * theta_length;
        float cos_theta = cosf (theta);
        float sin_theta = sinf (theta);

        for (int j = 0; j < num_phi; j++) {
            float phi_frac = (float) j / (float) phi_segments;
            float radius =
                inner_radius + phi_frac * (outer_radius - inner_radius);

            float x = radius * cos_theta;
            float y = radius * sin_theta;
            float z = 0.0f;

            float uv_x = (cos_theta * (radius / outer_radius) + 1.0f) * 0.5f;
            float uv_y = (sin_theta * (radius / outer_radius) + 1.0f) * 0.5f;

            vertices[vertex_idx++] = x;
            vertices[vertex_idx++] = y;
            vertices[vertex_idx++] = z;
            vertices[vertex_idx++] = 0.0f; // nx
            vertices[vertex_idx++] = 0.0f; // ny
            vertices[vertex_idx++] = 1.0f; // nz
            vertices[vertex_idx++] = uv_x;
            vertices[vertex_idx++] = uv_y;
        }
    }

    int num_indices = theta_segments * phi_segments * 6;
    Uint16* indices = (Uint16*) malloc (num_indices * sizeof (Uint16));
    if (!indices) {
        SDL_Log ("Failed to allocate indices for ring mesh");
        free (vertices);
        return null_mesh;
    }

    int index_idx = 0;
    for (int i = 0; i < theta_segments; i++) {
        for (int j = 0; j < phi_segments; j++) {
            Uint16 a = (Uint16) (i * num_phi + j);
            Uint16 b = (Uint16) (i * num_phi + j + 1);
            Uint16 c = (Uint16) ((i + 1) * num_phi + j + 1);
            Uint16 d = (Uint16) ((i + 1) * num_phi + j);

            // Clockwise winding to match circle geometry
            indices[index_idx++] = a;
            indices[index_idx++] = b;
            indices[index_idx++] = c;

            indices[index_idx++] = a;
            indices[index_idx++] = c;
            indices[index_idx++] = d;
        }
    }

    // Upload to GPU
    SDL_GPUBuffer* vbo = NULL;
    Uint64 vertices_size = num_vertices * 8 * sizeof (float);
    int vbo_failed = upload_vertices (device, vertices, vertices_size, &vbo);
    free (vertices);
    if (vbo_failed) {
        free (indices);
        return null_mesh; // Logging handled in upload_vertices
    }

    SDL_GPUBuffer* ibo = NULL;
    Uint64 indices_size = num_indices * sizeof (Uint16);
    int ibo_failed = upload_indices (device, indices, indices_size, &ibo);
    free (indices);
    if (ibo_failed) {
        SDL_ReleaseGPUBuffer (device, vbo);
        return null_mesh; // Logging handled in upload_indices
    }

    MeshComponent out_mesh =
        (MeshComponent) {.vertex_buffer = vbo,
                         .num_vertices = (Uint32) num_vertices,
                         .index_buffer = ibo,
                         .num_indices = (Uint32) num_indices,
                         .index_size = SDL_GPU_INDEXELEMENTSIZE_16BIT};

    return out_mesh;
}