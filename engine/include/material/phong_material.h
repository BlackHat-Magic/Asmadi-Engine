#pragma once

#include <ecs/ecs.h>

MaterialComponent
create_phong_material (vec3 color, MaterialSide side, gpu_renderer* renderer);