#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_keycode.h>
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
#define MOUSE_SENSE 1.0f / 100.0f
#define MOVEMENT_SPEED 3.0f

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
    switch (event->type) {
        case SDL_EVENT_QUIT:
            state->quit = true;
            break;
        case SDL_EVENT_MOUSE_MOTION:
            state->camera_yaw -= event->motion.xrel * MOUSE_SENSE;
            state->camera_pitch += event->motion.yrel * MOUSE_SENSE;
            // prevent gimbal lock
            if (state->camera_pitch > (float)M_PI * 0.49f) {
                state->camera_pitch = (float)M_PI * 0.49f;
            } else if (state->camera_pitch < (float)M_PI * -0.49f) {
                state->camera_pitch = (float)M_PI * -0.49f;
            }
            break;
        case SDL_EVENT_KEY_DOWN:
            if (event->key.key == SDLK_ESCAPE) state->quit = true;
            break;
    }

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

    // depth texture
    SDL_GPUTextureCreateInfo depth_info = {
        .type                 = SDL_GPU_TEXTURETYPE_2D,
        .format               = SDL_GPU_TEXTUREFORMAT_D32_FLOAT,
        .width                = state->width,
        .height               = state->height,
        .layer_count_or_depth = 1,
        .num_levels           = 1,
        .usage                = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET
    };
    state->depth_texture = SDL_CreateGPUTexture(state->device, &depth_info);
    if (state->depth_texture == NULL) {
        SDL_Log("Failed to create depth texture: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    // load texture
    state->texture = load_texture("assets/test.png", state->device);
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
        .vertex_input_state =
            {.vertex_buffer_descriptions =
                 (SDL_GPUVertexBufferDescription[]){
                     {.slot               = 0,
                      .pitch              = 5 * sizeof(float),
                      .input_rate         = SDL_GPU_VERTEXINPUTRATE_VERTEX,
                      .instance_step_rate = 0}
                 }, .num_vertex_buffers = 1,
                          .vertex_attributes =
                 (SDL_GPUVertexAttribute[]){
                     {.location    = 0,
                      .buffer_slot = 0,
                      .format      = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
                      .offset      = 0},  // pos (vec3)
                     {.location    = 1,
                      .buffer_slot = 0,
                      .format      = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
                      .offset      = 3 * sizeof(float)}  // texcoord (vec2)
                 }, .num_vertex_attributes = 2},
        .rasterizer_state =
            {.fill_mode  = SDL_GPU_FILLMODE_FILL,
                          .cull_mode  = SDL_GPU_CULLMODE_BACK,
                          .front_face = SDL_GPU_FRONTFACE_CLOCKWISE},
        .depth_stencil_state = {
                          .enable_depth_test   = true,
                          .enable_depth_write  = true,
                          .compare_op          = SDL_GPU_COMPAREOP_LESS,
                          .enable_stencil_test = false
        }
    };
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

    // create quad
    float vertices[] = {
        // Face 0 (z = -0.5, reordered for consistent winding)
        -0.5f, -0.5f, -0.5f, 0.0f, 0.0f, 0.5f, 0.5f, -0.5f, 1.0f, 1.0f, 0.5f,
        -0.5f, -0.5f, 1.0f, 0.0f, 0.5f, 0.5f, -0.5f, 1.0f, 1.0f, -0.5f, -0.5f,
        -0.5f, 0.0f, 0.0f, -0.5f, 0.5f, -0.5f, 0.0f, 1.0f,

        // Face 1 (z = 0.5, unchanged - correct winding)
        -0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 0.5f, -0.5f, 0.5f, 1.0f, 0.0f, 0.5f,
        0.5f, 0.5f, 1.0f, 1.0f, 0.5f, 0.5f, 0.5f, 1.0f, 1.0f, -0.5f, 0.5f, 0.5f,
        0.0f, 1.0f, -0.5f, -0.5f, 0.5f, 0.0f, 0.0f,

        // Face 2 (x = -0.5, unchanged - correct winding)
        -0.5f, 0.5f, 0.5f, 1.0f, 0.0f, -0.5f, 0.5f, -0.5f, 1.0f, 1.0f, -0.5f,
        -0.5f, -0.5f, 0.0f, 1.0f, -0.5f, -0.5f, -0.5f, 0.0f, 1.0f, -0.5f, -0.5f,
        0.5f, 0.0f, 0.0f, -0.5f, 0.5f, 0.5f, 1.0f, 0.0f,

        // Face 3 (x = 0.5, reordered for consistent winding)
        0.5f, 0.5f, 0.5f, 1.0f, 0.0f, 0.5f, -0.5f, -0.5f, 0.0f, 1.0f, 0.5f,
        0.5f, -0.5f, 1.0f, 1.0f, 0.5f, -0.5f, -0.5f, 0.0f, 1.0f, 0.5f, 0.5f,
        0.5f, 1.0f, 0.0f, 0.5f, -0.5f, 0.5f, 0.0f, 0.0f,

        // Face 4 (y = -0.5, unchanged - correct winding)
        -0.5f, -0.5f, -0.5f, 0.0f, 1.0f, 0.5f, -0.5f, -0.5f, 1.0f, 1.0f, 0.5f,
        -0.5f, 0.5f, 1.0f, 0.0f, 0.5f, -0.5f, 0.5f, 1.0f, 0.0f, -0.5f, -0.5f,
        0.5f, 0.0f, 0.0f, -0.5f, -0.5f, -0.5f, 0.0f, 1.0f,

        // Face 5 (y = 0.5, reordered for consistent winding)
        -0.5f, 0.5f, -0.5f, 0.0f, 1.0f, 0.5f, 0.5f, 0.5f, 1.0f, 0.0f, 0.5f,
        0.5f, -0.5f, 1.0f, 1.0f, 0.5f, 0.5f, 0.5f, 1.0f, 0.0f, -0.5f, 0.5f,
        -0.5f, 0.0f, 1.0f, -0.5f, 0.5f, 0.5f, 0.0f, 0.0f
    };

    // create vertex buffer
    SDL_GPUBufferCreateInfo vbo_info = {
        .usage = SDL_GPU_BUFFERUSAGE_VERTEX, .size = sizeof(vertices)
    };
    state->vertex_buffer = SDL_CreateGPUBuffer(state->device, &vbo_info);
    if (!state->vertex_buffer) {
        SDL_Log("Failed to create vertex buffer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    // Upload vertex data
    SDL_GPUTransferBufferCreateInfo transfer_info_v = {
        .size = sizeof(vertices), .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD
    };
    SDL_GPUTransferBuffer* transfer_v =
        SDL_CreateGPUTransferBuffer(state->device, &transfer_info_v);
    void* data_v = SDL_MapGPUTransferBuffer(state->device, transfer_v, false);
    SDL_memcpy(data_v, vertices, sizeof(vertices));
    SDL_UnmapGPUTransferBuffer(state->device, transfer_v);

    // Command buffer for upload
    SDL_GPUCommandBuffer* upload_cmd =
        SDL_AcquireGPUCommandBuffer(state->device);
    SDL_GPUCopyPass* copy_pass = SDL_BeginGPUCopyPass(upload_cmd);

    // Upload vertices
    SDL_GPUTransferBufferLocation src_v = {
        .transfer_buffer = transfer_v, .offset = 0
    };
    SDL_GPUBufferRegion dst_v = {
        .buffer = state->vertex_buffer, .offset = 0, .size = sizeof(vertices)
    };
    SDL_UploadToGPUBuffer(copy_pass, &src_v, &dst_v, false);

    SDL_EndGPUCopyPass(copy_pass);
    SDL_SubmitGPUCommandBuffer(upload_cmd);
    SDL_ReleaseGPUTransferBuffer(state->device, transfer_v);
    // SDL_ReleaseGPUTransferBuffer(state->device, transfer_i);

    // view matrix
    mat4* view = (mat4*)malloc(sizeof(mat4));
    mat4_identity(*view);
    mat4_translate(*view, (vec3){0.0f, 0.0f, 100.0f});
    state->view_matrix = view;

    // model matrix
    mat4* model_matrix = (mat4*)malloc(sizeof(mat4));
    mat4_identity(*model_matrix);
    state->model_matrix = model_matrix;

    // perspective matrix
    mat4* proj = (mat4*)malloc(sizeof(mat4));
    mat4_perspective(
        *proj, STARTING_FOV * (float)M_PI / 180.0f,
        (float)STARTING_WIDTH / (float)STARTING_HEIGHT, 0.01f, 1000.0f
    );
    state->proj_matrix = proj;

    // camera
    vec3* camera_pos    = (vec3*)malloc(sizeof(vec3));
    *camera_pos         = (vec3){0.0f, 0.0f, -2.0f};
    state->camera_pos   = camera_pos;
    state->camera_yaw   = M_PI;
    state->camera_pitch = 0.0f;
    state->camera_roll  = 0.0f;
    SDL_SetWindowRelativeMouseMode(state->window, true);

    state->last_time = SDL_GetPerformanceCounter();

    *appstate = state;
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void* appstate) {
    AppState* state = (AppState*)appstate;
    if (state->quit) return SDL_APP_SUCCESS;

    // dt
    const Uint64 now = SDL_GetPerformanceCounter();

    float dt = (float)(now - state->last_time) /
               (float)(SDL_GetPerformanceFrequency());
    state->last_time = now;

    // rotation
    // mat4_rotate_x(*state->model_matrix, dt / 5.0f);
    // mat4_rotate_y(*state->model_matrix, dt / 3.0f);

    // camera forward vector
    vec3 world_up = {0.0f, 1.0f, 0.0f};

    vec3 camera_forward = vec3_normalize((vec3){
        -sinf(state->camera_yaw) * cosf(state->camera_pitch),
        sinf(state->camera_pitch),
        -cosf(state->camera_yaw) * cosf(state->camera_pitch)
    });

    vec3 camera_right = vec3_normalize(vec3_cross(camera_forward, world_up));
    vec3 camera_up    = vec3_cross(camera_forward, camera_right);  /* now guaranteed orthogonal */

    // movement
    int numkeys;
    const bool* key_state = SDL_GetKeyboardState(&numkeys);
    vec3 motion           = {0.0f, 0.0f, 0.0f};
    if (key_state[SDL_SCANCODE_W]) motion = vec3_add(motion, camera_forward);
    if (key_state[SDL_SCANCODE_A]) motion = vec3_sub(motion, camera_right);
    if (key_state[SDL_SCANCODE_S]) motion = vec3_sub(motion, camera_forward);
    if (key_state[SDL_SCANCODE_D]) motion = vec3_add(motion, camera_right);
    if (key_state[SDL_SCANCODE_SPACE]) motion = vec3_add(motion, camera_up);
    motion             = vec3_normalize(motion);
    motion             = vec3_scale(motion, dt * MOVEMENT_SPEED);
    *state->camera_pos = vec3_add(*state->camera_pos, motion);

    // look
    vec3 target = vec3_add(*state->camera_pos, camera_forward);mat4_identity(*state->view_matrix);
    mat4_rotate_x(*state->view_matrix, -state->camera_pitch);
    mat4_rotate_y(*state->view_matrix, -state->camera_yaw);
    mat4_translate(*state->view_matrix, vec3_scale(*state->camera_pos, -1.0f));

    // uniform buffer
    UBOData ubo = {0};
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

    // color target info
    SDL_GPUColorTargetInfo color_target_info = {
        .texture     = swapchain,
        .clear_color = {0.0f, 0.0f, 0.0f, 1.0f},
        .load_op     = SDL_GPU_LOADOP_CLEAR,
        .store_op    = SDL_GPU_STOREOP_STORE
    };

    // depth target info
    SDL_GPUDepthStencilTargetInfo depth_target_info = {
        .texture     = state->depth_texture,
        .load_op     = SDL_GPU_LOADOP_CLEAR,
        .store_op    = SDL_GPU_STOREOP_STORE,
        .cycle       = false,
        .clear_depth = 1.0f
    };

    // begin render pass
    SDL_GPURenderPass* pass =
        SDL_BeginGPURenderPass(cmd, &color_target_info, 1, &depth_target_info);
    SDL_BindGPUGraphicsPipeline(pass, state->triangle_pipeline);

    // bind UBO
    SDL_PushGPUVertexUniformData(cmd, 0, &ubo, sizeof(UBOData));
    SDL_GPUTextureSamplerBinding tex_bind = {
        .texture = state->texture, .sampler = state->sampler
    };
    SDL_BindGPUFragmentSamplers(pass, 0, &tex_bind, 1);

    SDL_GPUBufferBinding vbo_binding = {
        .buffer = state->vertex_buffer, .offset = 0
    };
    SDL_BindGPUVertexBuffers(pass, 0, &vbo_binding, 1);

    // draw triangle
    SDL_DrawGPUPrimitives(pass, 36, 1, 0, 0);

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
    if (state->vertex_buffer) {
        SDL_ReleaseGPUBuffer(state->device, state->vertex_buffer);
    }
    if (state->index_buffer) {
        SDL_ReleaseGPUBuffer(state->device, state->index_buffer);
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
    if (state->depth_texture) {
        SDL_ReleaseGPUTexture(state->device, state->depth_texture);
    }
}