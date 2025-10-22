#pragma once

#include <ecs/ecs.h>

MaterialComponent
create_phong_material (gpu_renderer* renderer, SDL_FColor color, SDL_FColor emissive, SDL_GPUCullMode cullmode, SDL_GPUTexture* texture, SDL_GPUSampler* sampler);