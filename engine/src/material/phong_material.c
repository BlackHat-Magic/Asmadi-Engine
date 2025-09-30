#include "material/m_common.h"
#include "material/phong_material.h"

MaterialComponent create_phong_material (vec3 color, MaterialSide side, AppState* state) {
    MaterialComponent mat = {
        .color = color,
        .texture = NULL,
        .vertex_shader = NULL,
        .fragment_shader = NULL,
        .side = side
    };

    // TODO: communicate failure to caller
    int vert_failed = set_vertex_shader(state->device, &mat, "shaders/phong_material.vert.spv", state->swapchain_format);
    if (vert_failed) {
        mat.vertex_shader = NULL;
        return mat;
    }

    int frag_failed = set_fragment_shader(state->device, &mat, "shaders/phong_material.frag.spv", state->swapchain_format, 1, 1);
    if (frag_failed) mat.fragment_shader = NULL;

    return mat;
}