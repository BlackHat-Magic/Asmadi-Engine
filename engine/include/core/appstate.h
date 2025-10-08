#pragma once

#include <SDL3/SDL.h>

typedef Uint32 Entity;

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

    SDL_FRect* rects;
    SDL_FColor* rect_colors;
    Uint32 rect_count;
    SDL_GPUBuffer* rect_vbo;
    Uint32 rect_vbo_size;
    SDL_GPUBuffer* rect_ibo;
    Uint32 rect_ibo_size;
    SDL_GPUGraphicsPipeline* rect_pipeline;
    SDL_GPUShader* rect_vshader;
    SDL_GPUShader* rect_fshader;

    Entity camera_entity;
} AppState;