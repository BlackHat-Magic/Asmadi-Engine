#pragma once

#include "ecs/ecs.h"
#include "core/appstate.h"

MaterialComponent create_phong_material(vec3 color, MaterialSide side, AppState* state);