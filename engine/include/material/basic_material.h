#include <SDL3/SDL.h>

#include <ecs/ecs.h>
#include <math/matrix.h>

MaterialComponent
create_basic_material (SDL_FColor color, SDL_GPUCullMode cullmode, gpu_renderer* renderer);