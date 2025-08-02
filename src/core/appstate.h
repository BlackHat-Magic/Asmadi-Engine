#pragma once

#include <SDL3/SDL.h>

#include "math/matrix.h"

typedef struct {
    SDL_Window* window;
    Uint32 width;
    Uint32 height;
    Uint32 dwidth;
    Uint32 dheight;
    float fov;

    // GPU stuff
    SDL_GPUDevice* device;
    SDL_GPUTexture* texture;
    SDL_GPUSampler* sampler;
    SDL_GPUTexture* depth_texture;
    SDL_GPUTextureFormat swapchain_format;

    // view matrix
    mat4* view_matrix;

    // camera
    mat4* proj_matrix;

    // camera stuff
    vec3* camera_pos;
    float camera_yaw;
    float camera_pitch;
    float camera_roll;

    // time stuff
    Uint64 last_time;
    Uint64 current_time;
    bool quit;
} AppState;