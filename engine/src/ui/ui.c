#include <stdlib.h>

#include <ui/ui.h>

UIComponent create_ui_component (AppState* state, Uint32 max_rects) {
    UIComponent ui;

    ui.rects = malloc (sizeof (SDL_FRect) * max_rects);
    ui.colors = malloc (sizeof (SDL_FColor) * max_rects);
    ui.rect_count = 0;
    ui.max_rects = max_rects;

    // max rects * 4 vertices per rect * 8 floats per vertex * 4(?) bytes per float
    // minimum 4KiB
    Uint32 vsize = max_rects * 4 * 8 * sizeof (float);
    vsize = vsize < 4096 ? 4096 : vsize;
    SDL_GPUBufferCreateInfo vinfo = {
        .usage = SDL_GPU_BUFFERUSAGE_VERTEX,
        .size = vsize,
    };
    ui.vbo = SDL_CreateGPUBuffer (state->device, &vinfo);
    if (ui.vbo == NULL) {
        free (ui.rects);
        free (ui.colors);
        SDL_Log ("Failed to create UI vertex buffer: %s", SDL_GetError ());
        return (UIComponent) {0};
    }
    ui.vbo_size = vsize;

    Uint32 isize = max_rects * 6 * sizeof (Uint32);
    isize = isize < 4096 ? 4096 : isize;
    SDL_GPUBufferCreateInfo iinfo = {
        .usage = SDL_GPU_BUFFERUSAGE_INDEX,
        .size = isize
    };
    ui.ibo = SDL_CreateGPUBuffer (state->device, &iinfo);
    if (ui.ibo == NULL) {
        free (ui.rects);
        free (ui.colors);
        SDL_ReleaseGPUBuffer (state->device, ui.vbo);
        SDL_Log ("Failed to create UI index buffer: %s", SDL_GetError ());
        return (UIComponent) {0};
    }
    ui.ibo_size = isize;

    ui.vertex = load_shader (state->device, "shaders/ui.vert.spv", SDL_GPU_SHADERSTAGE_VERTEX, 0, 0, 0, 0);
    if (ui.vertex == NULL) {
        free (ui.rects);
        free (ui.colors);
        return (UIComponent) {0};
    }
    ui.fragment = load_shader (state->device, "shaders/ui_solid.frag.spv", SDL_GPU_SHADERSTAGE_FRAGMENT, 0, 0, 0, 0);
    if (ui.fragment == NULL) {
        free (ui.rects);
        free (ui.colors);
        SDL_ReleaseGPUShader (state->device, ui.vertex);
        return (UIComponent) {0};
    }

    SDL_GPUGraphicsPipelineCreateInfo info = {
        .target_info =
            {
                .num_color_targets = 1,
                .color_target_descriptions =
                    (SDL_GPUColorTargetDescription[]) {
                        {.format = state->swapchain_format,
                         .blend_state =
                             {
                                 .enable_blend = true,
                                 .src_color_blendfactor =
                                     SDL_GPU_BLENDFACTOR_SRC_ALPHA,
                                 .dst_color_blendfactor =
                                     SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
                                 .color_blend_op = SDL_GPU_BLENDOP_ADD,
                                 .src_alpha_blendfactor =
                                     SDL_GPU_BLENDFACTOR_ONE,
                                 .dst_alpha_blendfactor =
                                     SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
                                 .alpha_blend_op = SDL_GPU_BLENDOP_ADD,
                             }}
                    },
                .has_depth_stencil_target = true,
                .depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D24_UNORM,
            },
        .primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
        .vertex_shader = ui.vertex,
        .fragment_shader = ui.fragment,
        .vertex_input_state =
            {.num_vertex_buffers = 1,
             .vertex_buffer_descriptions =
                 (SDL_GPUVertexBufferDescription[]) {
                     {.slot = 0,
                      .pitch = sizeof (float) * 8,
                      .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
                      .instance_step_rate = 0}
                 },
             .num_vertex_attributes = 3,
             .vertex_attributes =
                 (SDL_GPUVertexAttribute[]) {
                     {.location = 0,
                      .buffer_slot = 0,
                      .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
                      .offset = 0}, // pos
                     {.location = 1,
                      .buffer_slot = 0,
                      .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
                      .offset = 2 * sizeof (float)}, // res
                      {.location = 2,
                        .buffer_slot = 0,
                        .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,
                        .offset = 4 * sizeof (float)}, // color
                 }},
        .rasterizer_state =
            {.fill_mode = SDL_GPU_FILLMODE_FILL,
             .cull_mode = SDL_GPU_CULLMODE_NONE,
             .front_face = SDL_GPU_FRONTFACE_CLOCKWISE},
        .depth_stencil_state = {
            .enable_depth_test = false,
            .enable_depth_write = false,
            .compare_op = SDL_GPU_COMPAREOP_ALWAYS,
        }
    };
    ui.pipeline = SDL_CreateGPUGraphicsPipeline (state->device, &info);
    if (ui.pipeline == NULL) {
        free (ui.rects);
        free (ui.colors);
        SDL_ReleaseGPUShader (state->device, ui.vertex);
        SDL_ReleaseGPUShader (state->device, ui.fragment);
        SDL_Log ("Unable to create UI graphics pipeline: %s", SDL_GetError ());
        return (UIComponent) {0};
    }

    return ui;
}

void draw_rectangle (UIComponent* ui, float x, float y, float w, float h, float r, float g, float b, float a) {
    // skip drawing too many rects
    if (ui->rect_count >= ui->max_rects) return;

    ui->rects[ui->rect_count] = (SDL_FRect) {x, y, w, h};
    ui->colors[ui->rect_count] = (SDL_FColor) {r, g, b, a};
    ui->rect_count++;
}