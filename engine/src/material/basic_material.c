#include "material/basic_material.h"
#include "material/m_common.h"

MaterialComponent
create_basic_material (vec3 color, MaterialSide side, AppState* state) {
    MaterialComponent mat = {
        .color = color,
        .texture = NULL,
        .vertex_shader = NULL,
        .fragment_shader = NULL,
        .side = side
    };

    // TODO: communicate failure to caller
    int vert_failed = set_vertex_shader (
        state->device, &mat, "shaders/basic_material.vert.spv",
        state->swapchain_format
    );
    if (vert_failed) {
        mat.vertex_shader = NULL;
        return mat;
    };

    int frag_failed = set_fragment_shader (
        state->device, &mat, "shaders/basic_material.frag.spv",
        state->swapchain_format, 1, 0
    );
    if (frag_failed) mat.fragment_shader = NULL;
    return mat;
}