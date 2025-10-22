#include <SDL3/SDL.h>
#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_gpu.h>
#include <SDL3_image/SDL_image.h>

#include <material/m_common.h>

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

    Uint64 code_size;
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
        .stage = stage,
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

// texture loader helper function
SDL_GPUTexture*
load_texture (SDL_GPUDevice* device, const char* bmp_file_path) {
    // note to self: don't forget to look at texture wrapping, texture
    // filtering, mipmaps https://learnopengl.com/Getting-started/Textures load
    // texture
    // does the file exist
    if (!SDL_GetPathInfo (bmp_file_path, NULL)) {
        SDL_Log ("Couldn't read file: %s", SDL_GetError ());
        return NULL;
    }

    // surface
    SDL_Surface* surface = IMG_Load (bmp_file_path);
    if (surface == NULL) {
        SDL_Log ("Failed to load texture: %s", SDL_GetError ());
        return NULL;
    }
    SDL_Surface* abgr_surface =
        SDL_ConvertSurface (surface, SDL_PIXELFORMAT_ABGR8888);
    SDL_DestroySurface (surface);
    if (abgr_surface == NULL) {
        SDL_Log ("Failed to convert surface format: %s", SDL_GetError ());
        return NULL;
    }
    SDL_GPUTextureCreateInfo tex_create_info = {
        .type = SDL_GPU_TEXTURETYPE_2D,
        .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, // RGBA,
        .width = (Uint32) abgr_surface->w,
        .height = (Uint32) abgr_surface->h,
        .layer_count_or_depth = 1,
        .num_levels = 1,
        .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER
    };
    SDL_GPUTexture* texture = SDL_CreateGPUTexture (device, &tex_create_info);
    if (texture == NULL) {
        SDL_Log ("Failed to create texture: %s", SDL_GetError ());
        return NULL;
    }

    // create transfer buffer
    SDL_GPUTransferBufferCreateInfo transfer_info = {
        .size = (Uint32) (abgr_surface->pitch * abgr_surface->h),
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD
    };
    SDL_GPUTransferBuffer* transfer_buf =
        SDL_CreateGPUTransferBuffer (device, &transfer_info);
    if (transfer_buf == NULL) {
        SDL_Log ("Failed to create transfer buffer: %s", SDL_GetError ());
        return NULL;
    }
    void* data_ptr = SDL_MapGPUTransferBuffer (device, transfer_buf, false);
    if (data_ptr == NULL) {
        SDL_Log ("Failed to map transfer buffer: %s", SDL_GetError ());
        SDL_ReleaseGPUTransferBuffer (device, transfer_buf);
        SDL_DestroySurface (abgr_surface);
        return NULL;
    }
    SDL_memcpy (data_ptr, abgr_surface->pixels, transfer_info.size);
    SDL_UnmapGPUTransferBuffer (device, transfer_buf);

    // upload with a command buffer
    SDL_GPUCommandBuffer* upload_cmd = SDL_AcquireGPUCommandBuffer (device);
    if (upload_cmd == NULL) {
        SDL_Log ("Failed to acquire GPU command buffer: %s", SDL_GetError ());
        return NULL;
    }
    SDL_GPUCopyPass* copy_pass = SDL_BeginGPUCopyPass (upload_cmd);
    if (copy_pass == NULL) {
        SDL_Log ("Failed to begin GPU copy pass: %s", SDL_GetError ());
        return NULL;
    }
    SDL_GPUTextureTransferInfo src_info = {
        .transfer_buffer = transfer_buf,
        .offset = 0,
        .pixels_per_row = (Uint32) abgr_surface->w,
        .rows_per_layer = (Uint32) abgr_surface->h,
    };
    SDL_GPUTextureRegion dst_region = {
        .texture = texture,
        .w = (Uint32) abgr_surface->w,
        .h = (Uint32) abgr_surface->h,
        .d = 1,
    };
    SDL_UploadToGPUTexture (copy_pass, &src_info, &dst_region, false);
    SDL_EndGPUCopyPass (copy_pass);
    SDL_SubmitGPUCommandBuffer (upload_cmd);
    SDL_ReleaseGPUTransferBuffer (device, transfer_buf);
    SDL_DestroySurface (abgr_surface);

    return texture;
}

// used for solid-color objects
SDL_GPUTexture* create_white_texture (SDL_GPUDevice* device) {
    SDL_GPUTextureCreateInfo tex_info = {
        .type = SDL_GPU_TEXTURETYPE_2D,
        .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
        .width = 1,
        .height = 1,
        .layer_count_or_depth = 1,
        .num_levels = 1,
        .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER
    };
    SDL_GPUTexture* tex = SDL_CreateGPUTexture (device, &tex_info);
    if (!tex) {
        SDL_Log ("Failed to create white texture: %s", SDL_GetError ());
        return NULL;
    }

    Uint8 pixel[4] = {255, 255, 255, 255}; // White pixel

    SDL_GPUTransferBufferCreateInfo trans_info = {
        .size = 4,
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD
    };
    SDL_GPUTransferBuffer* trans =
        SDL_CreateGPUTransferBuffer (device, &trans_info);
    if (!trans) {
        SDL_Log (
            "Failed to create transfer buffer for white texture: %s",
            SDL_GetError ()
        );
        SDL_ReleaseGPUTexture (device, tex);
        return NULL;
    }

    void* data = SDL_MapGPUTransferBuffer (device, trans, false);
    if (!data) {
        SDL_Log (
            "Failed to map transfer buffer for white texture: %s",
            SDL_GetError ()
        );
        SDL_ReleaseGPUTransferBuffer (device, trans);
        SDL_ReleaseGPUTexture (device, tex);
        return NULL;
    }
    memcpy (data, pixel, 4);
    SDL_UnmapGPUTransferBuffer (device, trans);

    SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer (device);
    if (!cmd) {
        SDL_Log (
            "Failed to acquire command buffer for white texture: %s",
            SDL_GetError ()
        );
        SDL_ReleaseGPUTransferBuffer (device, trans);
        SDL_ReleaseGPUTexture (device, tex);
        return NULL;
    }

    SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass (cmd);
    if (!copy) {
        SDL_Log (
            "Failed to begin copy pass for white texture: %s", SDL_GetError ()
        );
        SDL_SubmitGPUCommandBuffer (cmd);
        SDL_ReleaseGPUTransferBuffer (device, trans);
        SDL_ReleaseGPUTexture (device, tex);
        return NULL;
    }

    SDL_GPUTextureTransferInfo src = {
        .transfer_buffer = trans,
        .offset = 0,
        .pixels_per_row = 1,
        .rows_per_layer = 1
    };
    SDL_GPUTextureRegion dst = {.texture = tex, .w = 1, .h = 1, .d = 1};
    SDL_UploadToGPUTexture (copy, &src, &dst, false);
    SDL_EndGPUCopyPass (copy);
    SDL_SubmitGPUCommandBuffer (cmd);

    SDL_ReleaseGPUTransferBuffer (device, trans);
    return tex;
}