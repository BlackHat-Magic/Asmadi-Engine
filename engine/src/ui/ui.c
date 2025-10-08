#include <SDL3/SDL_gpu.h>
#include <string.h>

#include <ui/ui.h>

typedef struct {
    SDL_GPUGraphicsPipeline* pipeline;
    SDL_GPUSampler* sampler;
    SDL_GPUBuffer* vbo;
    Uint32 vbo_size;
} UIRenderer;

static UIRenderer renderer = {0};

static int ensure_vbo (AppState* state, Uint32 needed_bytes) {
    if (renderer.vbo && renderer.vbo_size >= needed_bytes) return 0;
    if (renderer.vbo) {
        SDL_ReleaseGPUBuffer (state->device, renderer.vbo);
        renderer.vbo = NULL;
        renderer.vbo_size = 0;
    }

    // allocate 4KiB minimum
    Uint32 size = needed_bytes < 4096 ? 4096 : needed_bytes;
    SDL_GPUBufferCreateInfo info = {
        .size = size,
        .usage = SDL_GPU_BUFFERUSAGE_VERTEX,
    };
    renderer.vbo = SDL_CreateGPUBuffer (state->device, &info);
    if (renderer.vbo == NULL) {
        SDL_Log ("Failed to create VBO for UI renderer: %s", SDL_GetError ());
        return 1;
    }
    renderer.vbo_size = size;
    return 0;
}

int UI_Init (AppState* state) {
    if (renderer.pipeline) return 0;

    SDL_GPUGraphicsPipelineCreateInfo info = {0};
    info.target_info.num_color_targets = 1;
    SDL_GPUColorTargetDescription color_desc = {
        .format = state->swapchain_format,
    };
    info.target_info.color_target_descriptions = &color_desc;
    info.target_info.has_depth_stencil_target = false;

    extern SDL_GPUShader* load_shader (
        SDL_GPUDevice*, const char*, SDL_GPUShaderStage, Uint32, Uint32, Uint32,
        Uint32
    );

    SDL_GPUShader* vertex = load_shader (
        state->device, "shaders/ui.vert.spv", SDL_GPU_SHADERSTAGE_VERTEX, 0, 1,
        0, 0
    );
    if (vertex == NULL) {
        SDL_Log ("Failed to load UI vertex shader: %s", SDL_GetError ());
        return 1;
    }
    info.vertex_shader = vertex;
    info.vertex_input_state.num_vertex_buffers = 1;
    SDL_GPUVertexBufferDescription vbd = {
        .slot = 0,
        .pitch = sizeof (float) * 6,
        .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
    };
    info.vertex_input_state.vertex_buffer_descriptions = &vbd;
    static SDL_GPUVertexAttribute attrs[2] = {
        {.location = 0,
         .buffer_slot = 0,
         .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
         .offset = 0},
        {.location = 1,
         .buffer_slot = 0,
         .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,
         .offset = sizeof (float) * 2}
    };
    info.vertex_input_state.vertex_attributes = attrs;
    info.vertex_input_state.num_vertex_attributes = 2;

    SDL_GPUShader* fragment = load_shader (
        state->device, "shaders/ui_solid.frag.spv", SDL_GPU_SHADERSTAGE_FRAGMENT,
        0, 1, 0, 0
    );
    if (fragment == NULL) {
        SDL_Log ("Failed to load UI fragment shader: %s", SDL_GetError ());
        return 1;
    }
    info.fragment_shader = fragment;

    info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    info.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
    info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
    info.rasterizer_state.front_face = SDL_GPU_FRONTFACE_CLOCKWISE;
    info.depth_stencil_state.enable_depth_test = false;
    info.depth_stencil_state.enable_depth_write = false;

    renderer.pipeline = SDL_CreateGPUGraphicsPipeline (state->device, &info);
    SDL_ReleaseGPUShader (state->device, vertex);
    SDL_ReleaseGPUShader (state->device, fragment);
    if (renderer.pipeline == NULL) {
        SDL_Log ("Failed to create UI pipeline: %s", SDL_GetError ());
        return 1;
    }

    SDL_GPUSamplerCreateInfo sampler = {
        .min_filter = SDL_GPU_FILTER_LINEAR,
        .mag_filter = SDL_GPU_FILTER_LINEAR,
        .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR,
        .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE
    };
    renderer.sampler = SDL_CreateGPUSampler (state->device, &sampler);

    if (ensure_vbo (state, 4096)) return 1;

    return 0;
}

typedef struct {
    float x, y;
    float r, g, b, a;
} UIVertex;

typedef struct {
    float screen[2]; // width, height
    float pad[2];
} ScreenUBO;

static int upload_to_vbo (
    SDL_GPUDevice* device,
    SDL_GPUBuffer* dest,
    const void* data,
    Uint32 bytes
) {
    SDL_GPUTransferBufferCreateInfo info = {
        .size = bytes,
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
    };
    SDL_GPUTransferBuffer* buffer = SDL_CreateGPUTransferBuffer (device, &info);
    if (buffer == NULL) {
        SDL_Log ("Failed to create UI transfer buffer: %s", SDL_GetError ());
        return 1;
    }

    void* map = SDL_MapGPUTransferBuffer (device, buffer, false);
    if (map == NULL) {
        SDL_Log ("Failed to map UI transfer buffer: %s", SDL_GetError ());
        return 1;
    }
    memcpy (map, data, bytes);
    SDL_UnmapGPUTransferBuffer (device, buffer);

    SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer (device);
    if (cmd == NULL) {
        SDL_ReleaseGPUTransferBuffer (device, buffer);
        SDL_Log ("Failed to acquire UI command buffer: %s", SDL_GetError ());
        return 1;
    }

    SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass (cmd);
    if (copy == NULL) {
        SDL_SubmitGPUCommandBuffer (cmd);
        SDL_ReleaseGPUTransferBuffer (device, buffer);
        SDL_Log ("Failed to begin UI copy pass: %s", SDL_GetError ());
        return 1;
    }

    SDL_GPUTransferBufferLocation src = {
        .transfer_buffer = buffer,
        .offset = 0,
    };
    SDL_GPUBufferRegion dest_region =
        {.buffer = dest, .offset = 0, .size = bytes};
    SDL_UploadToGPUBuffer (copy, &src, &dest_region, false);
    SDL_EndGPUCopyPass (copy);
    SDL_SubmitGPUCommandBuffer (cmd);
    SDL_ReleaseGPUTransferBuffer (device, buffer);
    return 0;
}

int UI_DrawRect (
    AppState* state,
    SDL_GPUCommandBuffer* cmd,
    SDL_GPURenderPass* pass,
    float x,
    float y,
    float w,
    float h,
    float r,
    float g,
    float b,
    float a
) {
    if (renderer.pipeline == NULL) return 1;
    if (w <= 0 || h <= 0) return 0;

    UIVertex verts[6] = {
        {x, y, r, g, b, a},         {x + w, y, r, g, b, a},
        {x + w, y + h, r, g, b, a}, {x, y, r, g, b, a},
        {x + w, y + h, r, g, b, a}, {x, y + h, r, g, b, a},
    };
    Uint32 bytes = sizeof (verts);
    if (ensure_vbo(state, bytes)) return 1;
    if (upload_to_vbo (state->device, renderer.vbo, verts, bytes)) return 1;

    SDL_BindGPUGraphicsPipeline (pass, renderer.pipeline);
    ScreenUBO ubo = {{(float) state->width, (float) state->height}, {0, 0}};
    SDL_PushGPUVertexUniformData (cmd, 0, &ubo, sizeof (ubo));

    SDL_GPUBufferBinding binding = {
        .buffer = renderer.vbo,
        .offset = 0,
    };
    SDL_BindGPUVertexBuffers (pass, 0, &binding, 1);
    SDL_DrawGPUPrimitives (pass, 6, 1, 0, 0);
    return 0;
}

void UI_Shutdown (AppState* state) {
    if (renderer.vbo) {
        SDL_ReleaseGPUBuffer (state->device, renderer.vbo);
        renderer.vbo = NULL;
    }
    if (renderer.pipeline) {
        SDL_ReleaseGPUGraphicsPipeline (state->device, renderer.pipeline);
        renderer.pipeline = NULL;
    }
    if (renderer.sampler) {
        SDL_ReleaseGPUSampler (state->device, renderer.sampler);
        renderer.sampler = NULL;
    }
    renderer.vbo_size = 0;
}