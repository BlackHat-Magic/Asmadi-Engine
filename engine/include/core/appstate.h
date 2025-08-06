#pragma once

#include <SDL3/SDL.h>

typedef uint32_t Entity;

typedef struct {
    SDL_Window* window;
    Uint32 width;
    Uint32 height;
    Uint32 dwidth;
    Uint32 dheight;
    float fov;

    // GPU stuff
    SDL_GPUDevice* device;
    SDL_GPUTexture* white_texture; // for solid-color objects
    SDL_GPUSampler* sampler;
    SDL_GPUTexture* depth_texture;
    SDL_GPUTextureFormat swapchain_format;

    // time stuff
    Uint64 last_time;
    Uint64 current_time;
    bool quit;

    Entity camera_entity;
} AppState;