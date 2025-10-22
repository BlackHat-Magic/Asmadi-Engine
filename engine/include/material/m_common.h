#pragma once

#include <SDL3/SDL.h>

#include <ecs/ecs.h>

SDL_GPUShader* load_shader (
    SDL_GPUDevice* device,
    const char* filename,
    SDL_GPUShaderStage state,
    Uint32 sampler_count,
    Uint32 uniform_buffer_count,
    Uint32 storage_buffer_count,
    Uint32 storage_texture_count
);

SDL_GPUTexture* load_texture (SDL_GPUDevice* device, const char* bmp_file_path);

SDL_GPUTexture* create_white_texture (SDL_GPUDevice* device);