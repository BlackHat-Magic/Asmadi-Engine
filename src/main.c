#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_oldnames.h>
#include <SDL3/SDL_timer.h>
#define SDL_MAIN_USE_CALLBACKS 1

#include <SDL3/SDL.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_video.h>
#include <SDL3_image/SDL_image.h>
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

    // GPU stuff
    SDL_GPUDevice* device;

    SDL_GPUTexture* texture;
    SDL_GPUSampler* sampler;

    // time stuff
    Uint64 last_time;
    Uint64 current_time;
    bool quit;
} AppState;

// shader loader helper function
SDL_GPUShader* load_shader(
    SDL_GPUDevice* device, const char* filename, SDL_GPUShaderStage stage,
    Uint32 sampler_count, Uint32 uniform_buffer_count,
    Uint32 storage_buffer_count, Uint32 storage_texture_count
) {
    if (!SDL_GetPathInfo(filename, NULL)) {
        SDL_Log("Couldn't read file %s: %s", filename, SDL_GetError());
        return NULL;
    }

    const char* entrypoint     = "main";
    SDL_GPUShaderFormat format = SDL_GPU_SHADERFORMAT_SPIRV;

    size_t code_size;
    void* code = SDL_LoadFile(filename, &code_size);
    if (code == NULL) {
        SDL_Log("Could't read file %s: %s", filename, SDL_GetError());
        return NULL;
    }

    SDL_GPUShaderCreateInfo shader_info = {
        .code                 = code,
        .code_size            = code_size,
        .entrypoint           = entrypoint,
        .format               = format,
        .stage                = stage,
        .num_samplers         = sampler_count,
        .num_uniform_buffers  = uniform_buffer_count,
        .num_storage_buffers  = storage_buffer_count,
        .num_storage_textures = storage_texture_count,
    };

    SDL_GPUShader* shader = SDL_CreateGPUShader(device, &shader_info);
    SDL_free(code);
    if (shader == NULL) {
        SDL_Log("Couldn't create GPU Shader: %s", SDL_GetError());
        return NULL;
    }
    return shader;
}

SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event) {
    AppState* state = (AppState*)appstate;
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
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_VULKAN
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
        SDL_Log("Couldn't claim window for GPU device: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    // load texture
    SDL_Surface* surface = IMG_Load("assets/test.bmp");
    if (!surface) {
        SDL_Log("Failed to load texture: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    SDL_Surface* rgba_surface =
        SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA8888);
    SDL_DestroySurface(surface);
    if (rgba_surface == NULL) {
        SDL_Log("Failed to convert surface format: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    SDL_GPUTextureCreateInfo tex_create_info = {
        .type                 = SDL_GPU_TEXTURETYPE_2D,
        .format               = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,  // RGBA,
        .width                = (Uint32)rgba_surface->w,
        .height               = (Uint32)rgba_surface->h,
        .layer_count_or_depth = 1,
        .num_levels           = 1,
        .usage                = SDL_GPU_TEXTUREUSAGE_SAMPLER
    };
    state->texture = SDL_CreateGPUTexture(state->device, &tex_create_info);
    if (!state->texture) {
        SDL_Log("Failed to create texture: %s", SDL_GetError());
        SDL_DestroySurface(rgba_surface);
        return SDL_APP_FAILURE;
    }

    // create transfer buffer
    SDL_GPUTransferBufferCreateInfo transfer_info = {
        .size  = (Uint32)(rgba_surface->pitch * rgba_surface->h),
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD
    };
    SDL_GPUTransferBuffer* transfer_buf =
        SDL_CreateGPUTransferBuffer(state->device, &transfer_info);
    void* data_ptr =
        SDL_MapGPUTransferBuffer(state->device, transfer_buf, false);
    if (data_ptr == NULL) {
        SDL_Log("Failed to map transfer buffer: %s", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(state->device, transfer_buf);
        SDL_DestroySurface(rgba_surface);
        return SDL_APP_FAILURE;
    }
    SDL_memcpy(data_ptr, rgba_surface->pixels, transfer_info.size);
    SDL_UnmapGPUTransferBuffer(state->device, transfer_buf);

    // upload with a command buffer
    SDL_GPUCommandBuffer* upload_cmd =
        SDL_AcquireGPUCommandBuffer(state->device);
    SDL_GPUCopyPass* copy_pass          = SDL_BeginGPUCopyPass(upload_cmd);
    SDL_GPUTextureTransferInfo src_info = {
        .transfer_buffer = transfer_buf,
        .offset          = 0,
        .pixels_per_row  = (Uint32)rgba_surface->w,
        .rows_per_layer  = (Uint32)rgba_surface->h,
    };
    SDL_GPUTextureRegion dst_region = {
        .texture = state->texture,
        .w       = (Uint32)rgba_surface->w,
        .h       = (Uint32)rgba_surface->h,
        .d       = 1,
    };
    SDL_UploadToGPUTexture(copy_pass, &src_info, &dst_region, false);
    SDL_EndGPUCopyPass(copy_pass);
    SDL_SubmitGPUCommandBuffer(upload_cmd);
    SDL_ReleaseGPUTransferBuffer(state->device, transfer_buf);
    SDL_DestroySurface(rgba_surface);

    // create sampler
    SDL_GPUSamplerCreateInfo sampler_info = {
        .min_filter        = SDL_GPU_FILTER_LINEAR,
        .mag_filter        = SDL_GPU_FILTER_LINEAR,
        .mipmap_mode       = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR,
        .address_mode_u    = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
        .address_mode_v    = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
        .address_mode_w    = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
        .max_anisotropy    = 1.0f,
        .enable_anisotropy = false
    };
    state->sampler = SDL_CreateGPUSampler(state->device, &sampler_info);
    if (!state->sampler) {
        SDL_Log("Failed to create sampler: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    // load triangle vertex shader
    SDL_GPUShader* triangle_vert = load_shader(
        state->device, "shaders/triangle.vert.spv", SDL_GPU_SHADERSTAGE_VERTEX,
        0, 1, 0, 0
    );
    if (triangle_vert == NULL) {
        SDL_Log("Failed to load vertex shader: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    // load triangle fragment shader
    SDL_GPUShader* triangle_frag = load_shader(
        state->device, "shaders/triangle.frag.spv",
        SDL_GPU_SHADERSTAGE_FRAGMENT, 1, 0, 0, 0
    );
    if (triangle_frag == NULL) {
        SDL_Log("Failed to load fragment shader: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    // create shader pipeline
    SDL_GPUGraphicsPipelineCreateInfo pipe_info = {
        .target_info =
            {
                          .num_color_targets = 1,
                          .color_target_descriptions =
                    (SDL_GPUColorTargetDescription[]){
                        {.format = SDL_GetGPUSwapchainTextureFormat(
                             state->device, state->window
                         )}
                    }, },
        .primitive_type  = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
        .vertex_shader   = triangle_vert,
        .fragment_shader = triangle_frag,
    };
    pipe_info.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
    SDL_GPUGraphicsPipeline* pipeline =
        SDL_CreateGPUGraphicsPipeline(state->device, &pipe_info);
    if (pipeline == NULL) {
        SDL_Log(
            "Could not create triangle graphics pipeline: %s", SDL_GetError()
        );
        return SDL_APP_FAILURE;
    }
    SDL_ReleaseGPUShader(state->device, triangle_vert);
    SDL_ReleaseGPUShader(state->device, triangle_frag);
    state->triangle_pipeline = pipeline;

    *appstate = state;
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void* appstate) {
    AppState* state = (AppState*)appstate;

    // dt
    const Uint64 now = SDL_GetPerformanceCounter();
    float dt         = (float)(now - state->last_time) /
               (float)(SDL_GetPerformanceFrequency());
    state->last_time = now;

    // color
    const Uint64 ticks_now = SDL_GetTicks();
    const float progress   = (float)((ticks_now % 5000) / 5000.0 * 2 * M_PI);

    float c1 = sinf(progress) / 2 + 0.5f;
    float c2 = sinf(progress + M_PI / 3 * 2) / 2 + 0.5f;
    float c3 = sinf(progress + M_PI / 3 * 4) / 2 + 0.5f;

    // reuse color values to avoid redundant calls to expensive trig functions
    // gcc -O2 would probably optimize this away, but it doesn't hurt
    float colors[3][4] = {
        {c1, c2, c3},
        {c2, c3, c1},
        {c3, c1, c2}
    };

    // acquire command buffer, swapchain
    SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(state->device);
    SDL_GPUTexture* swapchain;
    if (!SDL_WaitAndAcquireGPUSwapchainTexture(
            cmd, state->window, &swapchain, &state->width, &state->height
        )) {
        SDL_Log("Failed to get swapchain texture: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    if (swapchain == NULL) {
        SDL_Log("Failed to get swapchain texture: %s", SDL_GetError());
        SDL_SubmitGPUCommandBuffer(cmd);
        return SDL_APP_FAILURE;
    }

    // Begin render pass and bind pipeline
    SDL_GPUColorTargetInfo color_target_info = {0};
    color_target_info.texture                = swapchain;
    color_target_info.clear_color =
        (SDL_FColor){0.0f, 0.0f, 0.0f, 1.0f};  // black
    color_target_info.load_op  = SDL_GPU_LOADOP_CLEAR;
    color_target_info.store_op = SDL_GPU_STOREOP_STORE;
    SDL_GPURenderPass* pass =
        SDL_BeginGPURenderPass(cmd, &color_target_info, 1, NULL);
    SDL_BindGPUGraphicsPipeline(pass, state->triangle_pipeline);
    SDL_PushGPUVertexUniformData(cmd, 0, colors, sizeof(colors));

    SDL_GPUTextureSamplerBinding tex_bind = {
        .texture = state->texture, .sampler = state->sampler
    };
    SDL_BindGPUFragmentSamplers(pass, 0, &tex_bind, 1);

    // draw triangle
    SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0);

    // end render pass and submit
    SDL_EndGPURenderPass(pass);
    SDL_SubmitGPUCommandBuffer(cmd);

    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void* appstate, SDL_AppResult result) {
    AppState* state = (AppState*)appstate;
    if (state->texture) {
        SDL_ReleaseGPUTexture(state->device, state->texture);
    }
    if (state->sampler) {
        SDL_ReleaseGPUSampler(state->device, state->sampler);
    }
    if (state->triangle_pipeline) {
        SDL_ReleaseGPUGraphicsPipeline(state->device, state->triangle_pipeline);
    }
}