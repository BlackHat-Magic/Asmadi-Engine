#include "geometry/torus.h"

#include <math.h>
#include <stdlib.h>

#include "geometry/lathe.h"

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

    // Generate the tube circle path in the XZ plane (to be rotated around Y)
    int num_points = radial_segments + 1;
    vec2* points = (vec2*)malloc(num_points * sizeof(vec2));
    if (!points) {
        SDL_Log("Failed to allocate points for torus path");
        return null_mesh;
    }

    for (int i = 0; i < num_points; i++) {
        float phi = (float)i / (float)radial_segments * 2.0f * (float)M_PI;
        points[i].x = radius + tube_radius * cosf(phi);  // Offset from rotation axis
        points[i].y = tube_radius * sinf(phi);           // Height along tube
    }

    // Use lathe to rotate the path around Y, generating the torus
    // Note: This orients the torus with its "hole" along the Y axis (standing up).
    // To lay it flat (hole along Z), add a 90-degree X rotation
    // to the entity transform after creation, e.g., add_transform(e, pos, (vec3){M_PI/2, 0, 0}, scale);
    // lathe returns normals
    MeshComponent mesh = create_lathe_mesh(
        points, num_points, tubular_segments, 0.0f, arc, device
    );

    free(points);
    if (mesh.vertex_buffer == NULL) {
        SDL_Log("Failed to create lathed mesh for torus");
    }
    return mesh;
}