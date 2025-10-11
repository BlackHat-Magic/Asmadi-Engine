#include <stdlib.h>

#include <SDL3_ttf/SDL_ttf.h>

#include <ui/ui.h>

UIComponent create_ui_component (
    const AppState* state,
    const Uint32 max_rects,
    const Uint32 max_texts,
    const char* font_path,
    const float ptsize
) {
    UIComponent ui;

    ui.rects = malloc (sizeof (UIRectItem) * max_rects);
    if (ui.rects == NULL) {
        SDL_Log ("Failed to allocate UI rects.");
        return (UIComponent) {0};
    }
    ui.rect_count = 0;
    ui.max_rects = max_rects;

    ui.font = TTF_OpenFont (font_path, ptsize);
    if (ui.font == NULL) {
        free (ui.rects);
        SDL_Log ("Failed to create UI font: %s", SDL_GetError ());
        return (UIComponent) {0};
    }
    ui.texts = malloc (sizeof (UITextItem) * max_texts);
    if (ui.texts == NULL) {
        free (ui.rects);
        TTF_CloseFont (ui.font);
        SDL_Log ("Failed to allocate UI text buffer.");
        return (UIComponent) {0};
    }
    ui.text_count = 0;
    ui.max_texts = max_texts;

    // max rects * 4 vertices per rect * 10 floats per vertex * 4(?) bytes per
    // float minimum 4KiB
    Uint32 vsize = max_rects * 4 * 10 * sizeof (float);
    vsize = vsize < 4096 ? 4096 : vsize;
    SDL_GPUBufferCreateInfo vinfo = {
        .usage = SDL_GPU_BUFFERUSAGE_VERTEX,
        .size = vsize,
    };
    ui.vbo = SDL_CreateGPUBuffer (state->device, &vinfo);
    if (ui.vbo == NULL) {
        free (ui.rects);
        free (ui.texts);
        TTF_CloseFont (ui.font);
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
        free (ui.texts);
        TTF_CloseFont (ui.font);
        SDL_ReleaseGPUBuffer (state->device, ui.vbo);
        SDL_Log ("Failed to create UI index buffer: %s", SDL_GetError ());
        return (UIComponent) {0};
    }
    ui.ibo_size = isize;

    ui.vertex = load_shader (
        state->device, "shaders/ui.vert.spv", SDL_GPU_SHADERSTAGE_VERTEX, 0, 0,
        0, 0
    );
    if (ui.vertex == NULL) {
        free (ui.rects);
        free (ui.texts);
        TTF_CloseFont (ui.font);
        SDL_ReleaseGPUBuffer (state->device, ui.vbo);
        SDL_ReleaseGPUBuffer (state->device, ui.ibo);
        return (UIComponent) {0};
    }
    ui.fragment = load_shader (
        state->device, "shaders/ui.frag.spv", SDL_GPU_SHADERSTAGE_FRAGMENT, 1,
        0, 0, 0
    );
    if (ui.fragment == NULL) {
        free (ui.rects);
        free (ui.texts);
        TTF_CloseFont (ui.font);
        SDL_ReleaseGPUBuffer (state->device, ui.vbo);
        SDL_ReleaseGPUBuffer (state->device, ui.ibo);
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
                      .pitch = sizeof (float) * 10,
                      .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
                      .instance_step_rate = 0}
                 },
             .num_vertex_attributes = 4,
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
                     {.location = 3,
                      .buffer_slot = 0,
                      .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
                      .offset = 8 * sizeof (float)} // uv
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
        free (ui.texts);
        TTF_CloseFont (ui.font);
        SDL_ReleaseGPUBuffer (state->device, ui.vbo);
        SDL_ReleaseGPUBuffer (state->device, ui.ibo);
        SDL_ReleaseGPUShader (state->device, ui.vertex);
        SDL_ReleaseGPUShader (state->device, ui.fragment);
        SDL_Log ("Unable to create UI graphics pipeline: %s", SDL_GetError ());
        return (UIComponent) {0};
    }

    return ui;
}

void draw_rectangle (
    UIComponent* ui,
    const float x,
    const float y,
    const float w,
    const float h,
    const float r,
    const float g,
    const float b,
    const float a
) {
    // skip drawing too many rects
    if (ui->rect_count >= ui->max_rects) return;

    ui->rects[ui->rect_count].rect = (SDL_FRect) {x, y, w, h};
    ui->rects[ui->rect_count].color = (SDL_FColor) {r, g, b, a};
    ui->rect_count++;
}

float measure_text_width (const UIComponent* ui, const char* utf8) {
    if (!ui || !ui->font || !utf8) return 0.0f;
    int w = 0, h = 0;
    if (!TTF_GetStringSize (ui->font, utf8, 0, &w, &h)) {
        return (float) w;
    }
    SDL_Log ("TTF_GetStringSize failed: %s", SDL_GetError ());
    return 0.0f;
}

void measure_text (const UIComponent* ui, const char* utf8, int* w, int* h) {
    if (w) *w = 0;
    if (h) *h = 0;
    if (!ui || !ui->font || !utf8) return;
    int tw = 0, th = 0;
    if (!TTF_GetStringSize (ui->font, utf8, 0, &tw, &th)) {
        if (w) *w = tw;
        if (h) *h = th;
    } else {
        SDL_Log ("TTF_GetStringSize failed: %s", SDL_GetError ());
    }
}

static SDL_GPUTexture*
ui_create_text_texture (const AppState* state, SDL_Surface* abgr) {
    SDL_GPUTextureCreateInfo texinfo = {
        .type = SDL_GPU_TEXTURETYPE_2D,
        .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
        .width = (Uint32) abgr->w,
        .height = (Uint32) abgr->h,
        .layer_count_or_depth = 1,
        .num_levels = 1,
        .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER
    };
    SDL_GPUTexture* tex = SDL_CreateGPUTexture (state->device, &texinfo);
    if (!tex) {
        SDL_Log ("UI text texture create failed: %s", SDL_GetError ());
        return NULL;
    }
    SDL_GPUTransferBufferCreateInfo tbinfo = {
        .size = (Uint32) (abgr->pitch * abgr->h),
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD
    };
    SDL_GPUTransferBuffer* tbuf =
        SDL_CreateGPUTransferBuffer (state->device, &tbinfo);
    if (!tbuf) {
        SDL_ReleaseGPUTexture (state->device, tex);
        SDL_Log ("UI text transfer buffer create failed: %s", SDL_GetError ());
        return NULL;
    }
    void* map = SDL_MapGPUTransferBuffer (state->device, tbuf, false);
    if (!map) {
        SDL_ReleaseGPUTransferBuffer (state->device, tbuf);
        SDL_ReleaseGPUTexture (state->device, tex);
        SDL_Log ("UI text transfer buffer map failed: %s", SDL_GetError ());
        return NULL;
    }
    memcpy (map, abgr->pixels, tbinfo.size);
    SDL_UnmapGPUTransferBuffer (state->device, tbuf);

    SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer (state->device);
    SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass (cmd);
    SDL_GPUTextureTransferInfo src = {
        .transfer_buffer = tbuf,
        .offset = 0,
        .pixels_per_row = (Uint32) abgr->w,
        .rows_per_layer = (Uint32) abgr->h
    };
    SDL_GPUTextureRegion dst =
        {.texture = tex, .w = (Uint32) abgr->w, .h = (Uint32) abgr->h, .d = 1};
    SDL_UploadToGPUTexture (copy, &src, &dst, false);
    SDL_EndGPUCopyPass (copy);
    SDL_SubmitGPUCommandBuffer (cmd);
    SDL_ReleaseGPUTransferBuffer (state->device, tbuf);
    return tex;
}

int draw_text (
    UIComponent* ui,
    const AppState* state,
    const char* utf8,
    float x,
    float y,
    float r,
    float g,
    float b,
    float a
) {
    if (!ui || !ui->font || !utf8) return 0;
    if (ui->text_count >= ui->max_texts) return 0;

    SDL_Color color = {
        (Uint8) (r * 255.0f), (Uint8) (g * 255.0f), (Uint8) (b * 255.0f),
        (Uint8) (a * 255.0f)
    };
    SDL_Surface* surf = TTF_RenderText_Blended (ui->font, utf8, 0, color);
    if (!surf) {
        SDL_Log ("TTF_RenderText_Blended failed: %s", SDL_GetError ());
        return 0;
    }
    SDL_Surface* abgr = SDL_ConvertSurface (surf, SDL_PIXELFORMAT_ABGR8888);
    SDL_DestroySurface (surf);
    if (!abgr) {
        SDL_Log ("Convert surface failed: %s", SDL_GetError ());
        return 0;
    }

    SDL_GPUTexture* tex = ui_create_text_texture (state, abgr);
    int w = abgr->w;
    int h = abgr->h;
    SDL_DestroySurface (abgr);
    if (!tex) return 0;

    ui->texts[ui->text_count++] = (UITextItem) {
        .dst = (SDL_FRect) {x, y, (float) w, (float) h},
        .color = (SDL_FColor) {r, g, b, a},
        .texture = tex,
    };
    return w;
}