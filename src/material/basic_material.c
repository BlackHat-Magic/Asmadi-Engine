#include "material/basic_material.h"

MaterialComponent create_basic_material(vec3 color, SDL_GPUDevice* device) {
    return (MaterialComponent){
        .color           = color,
        .texture         = NULL,
        .vertex_shader   = NULL,
        .fragment_shader = NULL,
    };
}