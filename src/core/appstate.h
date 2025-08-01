#pragma once

#include <SDL3/SDL.h>

#include "math/matrix.h"

typedef struct {
    SDL_Window* window;
    Uint32 width;
    Uint32 height;

    // the triangle \[T]/
    SDL_GPUGraphicsPipeline* triangle_pipeline;
    SDL_GPUBuffer* triangle_vertex_buffer;

    // GPU stuff
    SDL_GPUDevice* device;
    SDL_GPUTexture* texture;
    SDL_GPUSampler* sampler;
    SDL_GPUBuffer* vertex_buffer;
    SDL_GPUBuffer* index_buffer;
    SDL_GPUTexture* depth_texture;

    // view matrix
    mat4* view_matrix;

    // objects
    mat4* model_matrix;

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