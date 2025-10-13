#include <SDL3/SDL.h>

#include <ecs/ecs.h>
#include "math/matrix.h"

MaterialComponent
create_basic_material (vec3 color, MaterialSide side, gpu_renderer* renderer);