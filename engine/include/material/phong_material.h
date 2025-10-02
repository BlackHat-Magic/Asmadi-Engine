#pragma once

#include "core/appstate.h"
#include "ecs/ecs.h"

MaterialComponent
create_phong_material (vec3 color, MaterialSide side, AppState* state);