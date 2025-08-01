#include <SDL3/SDL.h>

#include "math/matrix.h"
#include "ecs/ecs.h"

MaterialComponent create_basic_material (vec3 color, SDL_GPUDevice* device);