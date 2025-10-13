#include <material/phong_material.h>
#include <material/m_common.h>

MaterialComponent
create_phong_material (vec3 color, MaterialSide side, gpu_renderer* renderer) {
    MaterialComponent mat = {
        .color = color,
        .texture = NULL,
        .vertex_shader = NULL,
        .fragment_shader = NULL,
        .side = side
    };

    // TODO: communicate failure to caller
    int vert_failed = set_vertex_shader (
        renderer, &mat, "shaders/phong_material.vert.spv"
    );
    if (vert_failed) {
        mat.vertex_shader = NULL;
        return mat;
    }

    int frag_failed = set_fragment_shader (
        renderer, &mat, "shaders/phong_material.frag.spv",
        1, 1
    );
    if (frag_failed) mat.fragment_shader = NULL;

    return mat;
}