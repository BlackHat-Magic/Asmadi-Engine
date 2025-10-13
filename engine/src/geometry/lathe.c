#include <math.h>
#include <stdlib.h>

#include <geometry/g_common.h>
#include <geometry/lathe.h>
#include <math/matrix.h>

MeshComponent create_lathe_mesh (
    vec2* points,
    int num_points,
    int phi_segments,
    float phi_start,
    float phi_length,
    SDL_GPUDevice* device
) {
    MeshComponent null_mesh = (MeshComponent) {0};
    if (num_points < 2) {
        SDL_Log ("Lathe requires at least 2 points");
        return null_mesh;
    }
    if (phi_segments < 3) {
        SDL_Log ("Lathe requires at least 3 phi segments");
        return null_mesh;
    }

    int num_phi = phi_segments + 1; // Rings closed
    int num_vertices = num_points * num_phi;

    // Check for Uint16 overflow (max 65535 verts); extend to Uint32 if needed
    // later
    if (num_vertices > 65535) {
        SDL_Log ("Lathe mesh too large for Uint16 indices");
        return null_mesh;
    }

    float* vertices = (float*) malloc (
        num_vertices * 8 * sizeof (float)
    ); // pos.x,y,z + normal.x,y,z + uv.u,v
    if (!vertices) {
        SDL_Log ("Failed to allocate vertices for lathe mesh");
        return null_mesh;
    }

    // Generate vertices
    int vertex_idx = 0;
    for (int i = 0; i < num_points; i++) {
        float u = (float) i / (float) (num_points - 1); // Axial UV

        for (int j = 0; j < num_phi; j++) {
            float phi_frac = (float) j / (float) phi_segments;
            float phi = phi_start + phi_frac * phi_length;
            float cos_phi = cosf (phi);
            float sin_phi = sinf (phi);

            // Position: rotate point around Y (points.x is radius, points.y is
            // height)
            float x = points[i].x * cos_phi;
            float y = points[i].y;
            float z = points[i].x * sin_phi;

            // UV: u along path, v around phi (0-1)
            float v = phi_frac;

            vertices[vertex_idx++] = x;
            vertices[vertex_idx++] = y;
            vertices[vertex_idx++] = z;
            vertices[vertex_idx++] = 0.0f; // nx (placeholder)
            vertices[vertex_idx++] = 0.0f; // ny
            vertices[vertex_idx++] = 0.0f; // nz
            vertices[vertex_idx++] =
                v; // Note: may need to flip u/v based on convention
            vertices[vertex_idx++] = u;
        }
    }

    // Generate indices (quads between rings, flipped winding for outward faces)
    int num_indices = (num_points - 1) * phi_segments * 6;
    Uint16* indices = (Uint16*) malloc (num_indices * sizeof (Uint16));
    if (!indices) {
        SDL_Log ("Failed to allocate indices for lathe mesh");
        free (vertices);
        return null_mesh;
    }

    int index_idx = 0;
    for (int i = 0; i < num_points - 1; i++) {
        for (int j = 0; j < phi_segments; j++) {
            Uint16 a = (Uint16) (i * num_phi + j);
            Uint16 b =
                (Uint16) (i * num_phi + (j + 1) % phi_segments); // Wrap phi
            Uint16 c = (Uint16) ((i + 1) * num_phi + (j + 1) % phi_segments);
            Uint16 d = (Uint16) ((i + 1) * num_phi + j);

            // Flipped winding: a -> c -> b and a -> d -> c (counterclockwise if
            // original was clockwise)
            indices[index_idx++] = a;
            indices[index_idx++] = c;
            indices[index_idx++] = b;

            indices[index_idx++] = a;
            indices[index_idx++] = d;
            indices[index_idx++] = c;
        }
    }

    // Compute normals
    compute_vertex_normals (
        vertices, num_vertices, indices, num_indices, 8, 0, 3
    );

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