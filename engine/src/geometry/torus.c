#include "geometry/g_common.h"
#include "geometry/torus.h"

#include <math.h>
#include <stdlib.h>

#include "math/matrix.h"

MeshComponent create_torus_mesh(
    float radius, float tube_radius, int radial_segments, int tubular_segments,
    float arc, SDL_GPUDevice* device
) {
    MeshComponent null_mesh = (MeshComponent) {0};
    if (radial_segments < 3 || tubular_segments < 3) {
        SDL_Log("Torus must have at least 3 segments in each direction");
        return null_mesh;
    }
    if (tube_radius <= 0.0f || radius <= 0.0f) {
        SDL_Log("Torus radii must be positive");
        return null_mesh;
    }
    if (arc <= 0.0f || arc > 2.0f * (float)M_PI) {
        SDL_Log("Torus arc must be between 0 and 2*PI");
        return null_mesh;
    }

    bool is_closed = fabsf(arc - 2.0f * (float)M_PI) < 1e-6f;
    int num_tubular = tubular_segments + (is_closed ? 0 : 1);
    int num_radial = radial_segments;  // Always closed in radial direction

    int num_vertices = num_tubular * num_radial;
    if (num_vertices > 65535) {
        SDL_Log("Torus mesh too large for uint16_t indices");
        return null_mesh;
    }

    float* vertices = (float*)malloc(num_vertices * 8 * sizeof(float));  // pos3 + norm3 + uv2
    if (!vertices) {
        SDL_Log("Failed to allocate vertices for torus mesh");
        return null_mesh;
    }

    int vertex_idx = 0;
    for (int tu = 0; tu < num_tubular; tu++) {
        float u = ((float)tu / (float)tubular_segments) * arc;
        float cos_u = cosf(u);
        float sin_u = sinf(u);

        for (int ra = 0; ra < num_radial; ra++) {
            float v = ((float)ra / (float)radial_segments) * 2.0f * (float)M_PI;
            float cos_v = cosf(v);
            float sin_v = sinf(v);

            // Position (hole along Y-axis to match previous orientation)
            float x = (radius + tube_radius * cos_v) * cos_u;
            float y = tube_radius * sin_v;
            float z = (radius + tube_radius * cos_v) * sin_u;

            // Tube center
            vec3 tube_center = {radius * cos_u, 0.0f, radius * sin_u};

            // Normal (direction from tube center to point)
            vec3 pos = {x, y, z};
            vec3 norm = vec3_normalize(vec3_sub(pos, tube_center));

            // UV
            float uv_u = (float)tu / (float)tubular_segments;
            float uv_v = (float)ra / (float)radial_segments;

            vertices[vertex_idx++] = x;
            vertices[vertex_idx++] = y;
            vertices[vertex_idx++] = z;
            vertices[vertex_idx++] = norm.x;
            vertices[vertex_idx++] = norm.y;
            vertices[vertex_idx++] = norm.z;
            vertices[vertex_idx++] = uv_u;
            vertices[vertex_idx++] = uv_v;
        }
    }

    int num_u_loops = tubular_segments;
    int num_r_loops = radial_segments;
    int num_indices = num_u_loops * num_r_loops * 6;
    uint16_t* indices = (uint16_t*)malloc(num_indices * sizeof(uint16_t));
    if (!indices) {
        SDL_Log("Failed to allocate indices for torus mesh");
        free(vertices);
        return null_mesh;
    }

    int index_idx = 0;
    for (int tu = 0; tu < num_u_loops; tu++) {
        int tu1 = tu + 1;
        if (is_closed) {
            tu1 %= tubular_segments;
        }

        for (int ra = 0; ra < num_r_loops; ra++) {
            int ra1 = (ra + 1) % radial_segments;

            uint16_t a = (uint16_t)(tu * num_radial + ra);
            uint16_t b = (uint16_t)(tu1 * num_radial + ra);
            uint16_t c = (uint16_t)(tu1 * num_radial + ra1);
            uint16_t d = (uint16_t)(tu * num_radial + ra1);

            // Clockwise winding for front-face (matches SDL_GPU_FRONTFACE_CLOCKWISE)
            indices[index_idx++] = a;
            indices[index_idx++] = d;
            indices[index_idx++] = b;

            indices[index_idx++] = b;
            indices[index_idx++] = d;
            indices[index_idx++] = c;
        }
    }

    // Upload to GPU
    SDL_GPUBuffer* vbo = NULL;
    size_t vertices_size = num_vertices * 8 * sizeof(float);
    if (upload_vertices(device, vertices, vertices_size, &vbo)) {
        free(vertices);
        free(indices);
        return null_mesh;  // Logging handled in upload_vertices
    }
    free(vertices);

    SDL_GPUBuffer* ibo = NULL;
    size_t indices_size = num_indices * sizeof(uint16_t);
    if (upload_indices(device, indices, indices_size, &ibo)) {
        SDL_ReleaseGPUBuffer(device, vbo);
        free(indices);
        return null_mesh;  // Logging handled in upload_indices
    }
    free(indices);

    MeshComponent out_mesh = (MeshComponent){
        .vertex_buffer = vbo,
        .num_vertices = (uint32_t)num_vertices,
        .index_buffer = ibo,
        .num_indices = (uint32_t)num_indices,
        .index_size = SDL_GPU_INDEXELEMENTSIZE_16BIT
    };

    return out_mesh;
}