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

#include "matrix.h"

#define STARTING_WIDTH 640
#define STARTING_HEIGHT 480
#define STARTING_FOV 70.0

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
    SDL_GPUBuffer* ubo;

    // view matrix
    mat4* view_matrix;

    // objects
    mat4* model_matrix;

    // camera
    mat4* proj_matrix;

    // time stuff
    Uint64 last_time;
    Uint64 current_time;
    bool quit;
} AppState;

typedef struct {
    float colors[3][4];
    float model[16];
    float view[16];
    float proj[16];
} UBOData;

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

SDL_GPUTexture* load_texture(const char* bmp_file_path, SDL_GPUDevice* device) {
    // note to self: don't forget to look at texture wrapping, texture
    // filtering, mipmaps https://learnopengl.com/Getting-started/Textures load
    // texture
    SDL_Surface* surface = IMG_Load(bmp_file_path);
    if (surface == NULL) {
        SDL_Log("Failed to load texture: %s", SDL_GetError());
        return NULL;
    }
    SDL_Surface* abgr_surface =
        SDL_ConvertSurface(surface, SDL_PIXELFORMAT_ABGR8888);
    SDL_DestroySurface(surface);
    if (abgr_surface == NULL) {
        SDL_Log("Failed to convert surface format: %s", SDL_GetError());
        return NULL;
    }
    SDL_GPUTextureCreateInfo tex_create_info = {
        .type                 = SDL_GPU_TEXTURETYPE_2D,
        .format               = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,  // RGBA,
        .width                = (Uint32)abgr_surface->w,
        .height               = (Uint32)abgr_surface->h,
        .layer_count_or_depth = 1,
        .num_levels           = 1,
        .usage                = SDL_GPU_TEXTUREUSAGE_SAMPLER
    };
    SDL_GPUTexture* texture = SDL_CreateGPUTexture(device, &tex_create_info);
    if (texture == NULL) {
        SDL_Log("Failed to create texture: %s", SDL_GetError());
        return NULL;
    }

    // create transfer buffer
    SDL_GPUTransferBufferCreateInfo transfer_info = {
        .size  = (Uint32)(abgr_surface->pitch * abgr_surface->h),
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD
    };
    SDL_GPUTransferBuffer* transfer_buf =
        SDL_CreateGPUTransferBuffer(device, &transfer_info);
    void* data_ptr = SDL_MapGPUTransferBuffer(device, transfer_buf, false);
    if (data_ptr == NULL) {
        SDL_Log("Failed to map transfer buffer: %s", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(device, transfer_buf);
        SDL_DestroySurface(abgr_surface);
        return NULL;
    }
    SDL_memcpy(data_ptr, abgr_surface->pixels, transfer_info.size);
    SDL_UnmapGPUTransferBuffer(device, transfer_buf);

    // upload with a command buffer
    SDL_GPUCommandBuffer* upload_cmd    = SDL_AcquireGPUCommandBuffer(device);
    SDL_GPUCopyPass* copy_pass          = SDL_BeginGPUCopyPass(upload_cmd);
    SDL_GPUTextureTransferInfo src_info = {
        .transfer_buffer = transfer_buf,
        .offset          = 0,
        .pixels_per_row  = (Uint32)abgr_surface->w,
        .rows_per_layer  = (Uint32)abgr_surface->h,
    };
    SDL_GPUTextureRegion dst_region = {
        .texture = texture,
        .w       = (Uint32)abgr_surface->w,
        .h       = (Uint32)abgr_surface->h,
        .d       = 1,
    };
    SDL_UploadToGPUTexture(copy_pass, &src_info, &dst_region, false);
    SDL_EndGPUCopyPass(copy_pass);
    SDL_SubmitGPUCommandBuffer(upload_cmd);
    SDL_ReleaseGPUTransferBuffer(device, transfer_buf);
    SDL_DestroySurface(abgr_surface);

    return texture;
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
    state->width    = STARTING_WIDTH;
    state->height   = STARTING_HEIGHT;

    // initialize SDL
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("Couldn't initialize SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    // Create window
    state->window = SDL_CreateWindow(
        "C Vulkan", state->width, state->height,
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
    state->texture = load_texture("assets/test.bmp", state->device);
    if (state->texture == NULL)
        return SDL_APP_FAILURE;  // errors logged inside function

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
        0, 0, 1, 0
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

    // create ubo
    SDL_GPUBufferCreateInfo ubo_info = {
        .usage = SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ,
        .size  = sizeof(UBOData)
    };
    state->ubo = SDL_CreateGPUBuffer(state->device, &ubo_info);
    if (state->ubo == NULL) {
        SDL_Log("Failed to create Uniform Buffer Object: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    // view matrix
    mat4* view = (mat4*)malloc(sizeof(mat4));
    mat4_identity(*view);
    mat4_translate(*view, (vec3){0.0f, 0.0f, -3.0f});
    state->view_matrix = view;

    // model matrix
    mat4* model_matrix = (mat4*)malloc(sizeof(mat4));
    mat4_identity(*model_matrix);
    mat4_rotate_x(*model_matrix, -55.0f * (float)M_PI / 180.0f);
    state->model_matrix = model_matrix;

    // perspective matrix
    mat4* proj = (mat4*)malloc(sizeof(mat4));
    mat4_identity(*proj);
    mat4_perspective(
        *proj, STARTING_FOV * (float)M_PI / 180.0f,
        (float)STARTING_WIDTH / (float)STARTING_HEIGHT, 0.01f, 1000.0f
    );
    state->proj_matrix = proj;

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
    UBOData ubo      = {0};
    ubo.colors[0][0] = c1;
    ubo.colors[0][1] = c2;
    ubo.colors[0][2] = c3;
    ubo.colors[0][3] = 0.0f;
    ubo.colors[1][0] = c2;
    ubo.colors[1][1] = c3;
    ubo.colors[1][2] = c1;
    ubo.colors[1][3] = 0.0f;
    ubo.colors[2][0] = c3;
    ubo.colors[2][1] = c1;
    ubo.colors[2][2] = c2;
    ubo.colors[2][3] = 0.0f;
    SDL_memcpy(ubo.model, *state->model_matrix, sizeof(mat4));
    SDL_memcpy(ubo.view, *state->view_matrix, sizeof(mat4));
    SDL_memcpy(ubo.proj, *state->proj_matrix, sizeof(mat4));

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

    // Create transfer buffer
    SDL_GPUTransferBufferCreateInfo transfer_info = {
        .size = sizeof(UBOData), .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD
    };
    SDL_GPUTransferBuffer* transfer_buf =
        SDL_CreateGPUTransferBuffer(state->device, &transfer_info);
    if (transfer_buf == NULL) {
        SDL_Log("Failed to create transfer buffer: %s", SDL_GetError());
        SDL_SubmitGPUCommandBuffer(cmd);
        return SDL_APP_FAILURE;
    }
    void* data_ptr =
        SDL_MapGPUTransferBuffer(state->device, transfer_buf, false);
    if (data_ptr == NULL) {
        SDL_Log("Failed to map transfer buffer: %s", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(state->device, transfer_buf);
        SDL_SubmitGPUCommandBuffer(cmd);
        return SDL_APP_FAILURE;
    }
    SDL_memcpy(data_ptr, &ubo, sizeof(UBOData));
    SDL_UnmapGPUTransferBuffer(state->device, transfer_buf);

    // copy transfer
    SDL_GPUCopyPass* copy_pass             = SDL_BeginGPUCopyPass(cmd);
    SDL_GPUTransferBufferLocation src_info = {
        .transfer_buffer = transfer_buf,
        .offset          = 0,
    };
    SDL_GPUBufferRegion dst_region = {
        .buffer = state->ubo, .offset = 0, .size = sizeof(UBOData)
    };
    SDL_UploadToGPUBuffer(copy_pass, &src_info, &dst_region, false);
    SDL_EndGPUCopyPass(copy_pass);
    SDL_ReleaseGPUTransferBuffer(state->device, transfer_buf);

    // begin render pass
    SDL_GPUColorTargetInfo color_target_info = {0};
    color_target_info.texture                = swapchain;
    color_target_info.clear_color =
        (SDL_FColor){0.0f, 0.0f, 0.0f, 1.0f};  // black
    color_target_info.load_op  = SDL_GPU_LOADOP_CLEAR;
    color_target_info.store_op = SDL_GPU_STOREOP_STORE;
    SDL_GPURenderPass* pass =
        SDL_BeginGPURenderPass(cmd, &color_target_info, 1, NULL);
    SDL_BindGPUGraphicsPipeline(pass, state->triangle_pipeline);

    // bind UBO
    SDL_GPUBufferBinding ubo_binding = {.buffer = state->ubo, .offset = 0};
    SDL_BindGPUVertexStorageBuffers(pass, 0, &state->ubo, 1);
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
    if (state->ubo) {
        SDL_ReleaseGPUBuffer(state->device, state->ubo);
    }
    if (state->view_matrix) {
        free(state->view_matrix);
    }
    if (state->model_matrix) {
        free(state->model_matrix);
    }
    if (state->proj_matrix) {
        free(state->proj_matrix);
    }
}