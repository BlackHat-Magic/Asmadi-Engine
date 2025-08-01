#include "material/common.h"

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>

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

// texture loader helper function
SDL_GPUTexture* load_texture(SDL_GPUDevice* device, const char* bmp_file_path) {
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
    if (transfer_buf == NULL) {
        SDL_Log ("Failed to create transfer buffer: %s", SDL_GetError ());
        return NULL;
    }
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
    if (upload_cmd == NULL) {
        SDL_Log ("Failed to acquire GPU command buffer: %s", SDL_GetError ());
        return NULL;
    }
    SDL_GPUCopyPass* copy_pass          = SDL_BeginGPUCopyPass(upload_cmd);
    if (copy_pass == NULL) {
        SDL_Log ("Failed to begin GPU copy pass: %s", SDL_GetError ());
        return NULL;
    }
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

// returns 0 on success 1 on failure
int set_vertex_shader(
    SDL_GPUDevice* device, MaterialComponent* mat, const char* filepath,
    SDL_GPUTextureFormat swapchain_format
) {
    mat->vertex_shader =
        load_shader(device, filepath, SDL_GPU_SHADERSTAGE_VERTEX, 0, 1, 0, 0);
    if (mat->vertex_shader == NULL) return 1; // logging handled in load_shader()
    if (mat->vertex_shader && mat->fragment_shader) {
        int pipe_failed = build_pipeline(device, mat, swapchain_format);
        if (pipe_failed) return 1; // logging handled in build_pipeline()
    }
    return 0;
}

// returns 0 on success 1 on failure
int set_fragment_shader(
    SDL_GPUDevice* device, MaterialComponent* mat, const char* filepath,
    SDL_GPUTextureFormat swapchain_format
) {
    mat->fragment_shader =
        load_shader(device, filepath, SDL_GPU_SHADERSTAGE_FRAGMENT, 1, 0, 0, 0);
    if (mat->fragment_shader == NULL) return 1; // logging handled in load_shader()
    if (mat->vertex_shader && mat->fragment_shader) {
        int pipe_failed = build_pipeline(device, mat, swapchain_format);
        if (pipe_failed) return 1; // logging handled in build_pipeline()
    }
    return 0;
}

// returns 0 on success 1 on failure
static int build_pipeline(
    SDL_GPUDevice* device, MaterialComponent* mat,
    SDL_GPUTextureFormat swapchain_format
) {
    SDL_GPUGraphicsPipelineCreateInfo pipe_info = {
        .target_info =
            {
                          .num_color_targets = 1,
                          .color_target_descriptions =
                    (SDL_GPUColorTargetDescription[]){
                        {.format = swapchain_format}
                    }, },
        .primitive_type  = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
        .vertex_shader   = mat->vertex_shader,
        .fragment_shader = mat->fragment_shader,
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
                      .offset      = 0},  // pos
                     {.location    = 1,
                      .buffer_slot = 0,
                      .format      = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
                      .offset      = 3 * sizeof(float)}  // texcoord
                 }, .num_vertex_attributes = 2},
        .rasterizer_state =
            {.fill_mode  = SDL_GPU_FILLMODE_FILL,
                          .cull_mode  = SDL_GPU_CULLMODE_BACK,
                          .front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE},
        .depth_stencil_state = {
                          .enable_depth_test   = true,
                          .enable_depth_write  = true,
                          .compare_op          = SDL_GPU_COMPAREOP_LESS,
                          .enable_stencil_test = false
        }
    };
    mat->pipeline = SDL_CreateGPUGraphicsPipeline(device, &pipe_info);
    if (!mat->pipeline) {
        SDL_Log("Failed to create material pipeline: %s", SDL_GetError());
        return 1;
    }

    return 0;
}