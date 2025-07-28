#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_timer.h>
#define SDL_MAIN_USE_CALLBACKS 1

#include <SDL3/SDL.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_video.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    SDL_Window* window;
    Uint32 width;
    Uint32 height;

    // the triangle \[T]/
    SDL_GPUGraphicsPipeline* triangle_pipeline;
    SDL_GPUBuffer* triangle_vertex_buffer;
    SDL_GPUSampler* triangle_sampler;

    // GPU stuff
    SDL_GPUDevice* device;

    // time stuff
    Uint64 last_time;
    Uint64 current_time;
    bool quit;
} AppState;

// shader loader helper function
SDL_GPUShader* load_shader (
    SDL_GPUDevice* device,
    const char* filename,
    SDL_GPUShaderStage stage,
    Uint32 sampler_count,
    Uint32 uniform_buffer_count,
    Uint32 storage_buffer_count,
    Uint32 storage_texture_count
) {
    if (!SDL_GetPathInfo (filename, NULL)) {
        SDL_Log ("Couldn't read file %s: %s", filename, SDL_GetError ());
        return NULL;
    }

    const char* entrypoint = "main";
    SDL_GPUShaderFormat format = SDL_GPU_SHADERFORMAT_SPIRV;

    size_t code_size;
    void* code = SDL_LoadFile (filename, &code_size);
    if (code == NULL) {
        SDL_Log ("Could't read file %s: %s", filename, SDL_GetError ());
        return NULL;
    }
    
    SDL_GPUShaderCreateInfo shader_info = {
        .code = code,
        .code_size = code_size,
        .entrypoint = entrypoint,
        .format = format,
        .stage= stage,
        .num_samplers = sampler_count,
        .num_uniform_buffers = uniform_buffer_count,
        .num_storage_buffers = storage_buffer_count,
        .num_storage_textures = storage_texture_count,
    };

    SDL_GPUShader* shader = SDL_CreateGPUShader (device, &shader_info);
    SDL_free (code);
    if (shader == NULL) {
        SDL_Log ("Couldn't create GPU Shader: %s", SDL_GetError ());
        return NULL;
    }
    return shader;
}

SDL_AppResult SDL_AppEvent (void* appstate, SDL_Event* event) {
    AppState* state = (AppState*) appstate;
    if (event->type == SDL_EVENT_QUIT) return SDL_APP_SUCCESS;

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppInit(void** appstate, int argc, char** argv) {
    SDL_SetAppMetadata(
        "GMTK GameJam 2025 SDL", "0.1.0", "xyz.lukeh.GMTK-GameJam-2025-SDL"
    );

    // create appstate
    AppState* state = (AppState*)calloc(1, sizeof(AppState));
    state->width    = 640;
    state->height   = 480;

    // initialize SDL
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("Couldn't initialize SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    // Create window
    state->window = SDL_CreateWindow(
        "C OpenGL", state->width, state->height,
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL
    );
    if (!state->window) {
        SDL_Log("Couldn't create window/renderer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    // create GPU device
    state->device =
        SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, false, NULL);
    if (!state->device) {
        SDL_Log("Couldn't create SDL_GPU_DEVICE");
        return SDL_APP_FAILURE;
    }
    if (!SDL_ClaimWindowForGPUDevice(state->device, state->window)) {
        SDL_Log ("Couldn't claim window for GPU device: %s", SDL_GetError ());
        return SDL_APP_FAILURE;
    }

    // load triangle vertex shader
    SDL_GPUShader* triangle_vert = load_shader (state->device, "shaders/triangle.vert.spv", SDL_GPU_SHADERSTAGE_VERTEX, 0, 0, 0, 0);
    if (triangle_vert == NULL) {
        SDL_Log ("Failed to load vertex shader: %s", SDL_GetError ());
        return SDL_APP_FAILURE;
    }

    // load triangle fragment shader
    SDL_GPUShader* triangle_frag = load_shader (state->device, "shaders/triangle.frag.spv", SDL_GPU_SHADERSTAGE_FRAGMENT, 0, 0, 0, 0);
    if (triangle_frag == NULL) {
        SDL_Log ("Failed to load fragment shader: %s", SDL_GetError ());
        return SDL_APP_FAILURE;
    }
    
    // create shader pipeline
    SDL_GPUGraphicsPipelineCreateInfo pipe_info = {
        .target_info = {
            .num_color_targets = 1,
            .color_target_descriptions = (SDL_GPUColorTargetDescription[]) {{
                .format = SDL_GetGPUSwapchainTextureFormat (state->device, state->window)
            }},
        },
        .primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
        .vertex_shader = triangle_vert,
        .fragment_shader = triangle_frag
    };
    pipe_info.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
    SDL_GPUGraphicsPipeline* pipeline = SDL_CreateGPUGraphicsPipeline (state->device, &pipe_info);
    if (pipeline == NULL) {
        SDL_Log ("Could not create triangle graphics pipeline: %s", SDL_GetError ());
        return SDL_APP_FAILURE;
    }
    SDL_ReleaseGPUShader (state->device, triangle_vert);
    SDL_ReleaseGPUShader (state->device, triangle_frag);
    state->triangle_pipeline = pipeline;

    *appstate = state;
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate (void* appstate) {
    AppState* state = (AppState*) appstate;

    // dt
    const Uint64 now = SDL_GetPerformanceCounter ();
    float dt = (float) (now - state->last_time) / (float) (SDL_GetPerformanceFrequency());
    state->last_time = now;

    // color
    // ...

    // acquire command buffer, swapchain
    SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(state->device);
    SDL_GPUTexture* swapchain;
    if (!SDL_WaitAndAcquireGPUSwapchainTexture(cmd, state->window, &swapchain, &state->width, &state->height)) {
        SDL_Log ("Failed to get swapchain texture: %s", SDL_GetError ());
        return SDL_APP_FAILURE;
    }
    if (swapchain == NULL) {
        SDL_Log ("Failed to get swapchain texture: %s", SDL_GetError ());
        SDL_SubmitGPUCommandBuffer(cmd);
        return SDL_APP_FAILURE;
    }

    // Begin render pass and bind pipeline
    SDL_GPUColorTargetInfo color_target_info = { 0 };
    color_target_info.texture = swapchain;
    color_target_info.clear_color = (SDL_FColor) {0.0f, 0.0f, 0.0f, 1.0f}; // black
    color_target_info.load_op = SDL_GPU_LOADOP_CLEAR;
    color_target_info.store_op = SDL_GPU_STOREOP_STORE;
    SDL_GPURenderPass* pass = SDL_BeginGPURenderPass (
        cmd,
        &color_target_info,
        1,
        NULL
    );
    SDL_BindGPUGraphicsPipeline (pass, state->triangle_pipeline);


    // draw triangle
    SDL_DrawGPUPrimitives (pass, 3, 1, 0, 0);

    // end render pass and submit
    SDL_EndGPURenderPass (pass);
    SDL_SubmitGPUCommandBuffer (cmd);

    return SDL_APP_CONTINUE;
}

void SDL_AppQuit (void* appstate, SDL_AppResult result) {
    // ...
}