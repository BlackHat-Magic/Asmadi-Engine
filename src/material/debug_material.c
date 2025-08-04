#include "debug_material.h"
#include "material/m_common.h"

MaterialComponent create_debug_material(vec3 color, SDL_GPUDevice* device) {
    MaterialComponent mat = { .color = color, .texture = NULL };

    // Use new debug shaders (constant color, no texture)
    int vert_failed = set_vertex_shader(device, &mat, "shaders/debug.vert.spv", SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM);  // Assume swapchain format for simplicity
    if (vert_failed) return mat;  // Invalid on failure
    int frag_failed = set_fragment_shader(device, &mat, "shaders/debug.frag.spv", SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM);
    if (frag_failed) return mat;

    return mat;
}