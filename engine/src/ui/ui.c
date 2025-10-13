#include <stdlib.h>

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_keycode.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <microui.h>

#include <ui/ui.h>

// 0 length for null terminated string
static int text_width (mu_Font font, const char* string, int len) {
    if (font == NULL) return 1;

    int w = 0, h = 0;
    if (!TTF_GetStringSize (font, string, len, &w, &h)) {
        return (float) w;
    }
    SDL_Log ("TTF_GetStringSize failed: %s", SDL_GetError ());
    return 1;
}

static int text_height (mu_Font font) {
    if (font == NULL) return 1;

    return TTF_GetFontLineSkip (font);
}

// translate SDL mouse buttons to MicroUI buttons
static const char button_map[256] = {
    [SDL_BUTTON_LEFT & 0xff] = MU_MOUSE_LEFT,
    [SDL_BUTTON_RIGHT & 0xff] = MU_MOUSE_RIGHT,
    [SDL_BUTTON_MIDDLE & 0xff] = MU_MOUSE_MIDDLE
};

static const char key_map[256] = {
    [SDLK_LSHIFT & 0xff] = MU_KEY_SHIFT,
    [SDLK_RSHIFT & 0xff] = MU_KEY_SHIFT,
    [SDLK_LCTRL & 0xff] = MU_KEY_CTRL,
    [SDLK_RCTRL & 0xff] = MU_KEY_CTRL,
    [SDLK_LALT & 0xff] = MU_KEY_ALT,
    [SDLK_RALT & 0xff] = MU_KEY_ALT,
    [SDLK_RETURN & 0xff] = MU_KEY_RETURN,
    [SDLK_BACKSPACE & 0xff] = MU_KEY_BACKSPACE,
};

void ui_handle_event (SDL_Event* event, UIComponent* ui) {
    switch (event->type) {
    case SDL_EVENT_MOUSE_MOTION:
        mu_input_mousemove (&ui->context, event->motion.x, event->motion.y);
        break;
    case SDL_EVENT_MOUSE_WHEEL:
        mu_input_scroll (&ui->context, event->wheel.x, event->wheel.y);
        break;
    case SDL_EVENT_TEXT_INPUT:
        mu_input_text (&ui->context, event->text.text);
        break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP:
        Uint32 button = button_map[event->button.button & 0xff];
        if (button && event->type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
            mu_input_mousedown (
                &ui->context, event->button.x, event->button.y, button
            );
        }
        if (button && event->type == SDL_EVENT_MOUSE_BUTTON_UP) {
            mu_input_mouseup (
                &ui->context, event->button.x, event->button.y, button
            );
        }
        break;
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP:
        Uint32 key = key_map[event->key.key & 0xff];
        if (key && event->type == SDL_EVENT_KEY_DOWN) {
            mu_input_keydown (&ui->context, key);
        }
        if (key && event->type == SDL_EVENT_KEY_UP) {
            mu_input_keyup (&ui->context, key);
        }
        break;
    }
}

UIComponent* create_ui_component (
    const gpu_renderer* renderer,
    const Uint32 max_rects,
    const Uint32 max_texts,
    const char* font_path,
    const float ptsize
) {
    UIComponent* ui = malloc (sizeof (UIComponent));
    if (ui == NULL) {
        SDL_Log ("Failed to allocate UI component");
        return NULL;
    }

    // microui
    mu_init (&ui->context);
    ui->context.text_width = text_width;
    ui->context.text_height = text_height;

    ui->rects = malloc (sizeof (UIRect) * max_rects);
    if (ui->rects == NULL) {
        free (ui);
        SDL_Log ("Failed to allocate UI rects.");
        return NULL;
    }
    ui->rect_count = 0;
    ui->max_rects = max_rects;

    // white texture
    ui->white_texture = create_white_texture (renderer->device);
    if (ui->white_texture == NULL) {
        free (ui->rects);
        free (ui);
        return NULL;
    }

    // sampler
    SDL_GPUSamplerCreateInfo sampler_info = {
        .min_filter = SDL_GPU_FILTER_LINEAR,
        .mag_filter = SDL_GPU_FILTER_LINEAR,
        .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR,
        .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
        .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
        .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
        .max_anisotropy = 1.0f,
        .enable_anisotropy = false
    };
    ui->sampler = SDL_CreateGPUSampler (renderer->device, &sampler_info);
    if (ui->sampler == NULL) {
        free (ui->rects);
        SDL_ReleaseGPUTexture (renderer->device, ui->white_texture);
        free (ui);
        SDL_Log ("Failed to create sampler: %s", SDL_GetError ());
        return NULL;
    }

    ui->font = TTF_OpenFont (font_path, ptsize);
    if (ui->font == NULL) {
        free (ui->rects);
        SDL_ReleaseGPUTexture (renderer->device, ui->white_texture);
        SDL_Log ("Failed to create UI font: %s", SDL_GetError ());
        return NULL;
    }

    // max rects * 4 vertices per rect * 10 floats per vertex * 4(?) bytes per
    // float minimum 4KiB
    Uint32 vsize = max_rects * 4 * 10 * sizeof (float);
    vsize = vsize < 4096 ? 4096 : vsize;
    SDL_GPUBufferCreateInfo vinfo = {
        .usage = SDL_GPU_BUFFERUSAGE_VERTEX,
        .size = vsize,
    };
    ui->vbo = SDL_CreateGPUBuffer (renderer->device, &vinfo);
    if (ui->vbo == NULL) {
        free (ui->rects);
        TTF_CloseFont (ui->font);
        SDL_ReleaseGPUTexture (renderer->device, ui->white_texture);
        free (ui);
        SDL_Log ("Failed to create UI vertex buffer: %s", SDL_GetError ());
        return NULL;
    }
    ui->vbo_size = vsize;

    Uint32 isize = max_rects * 6 * sizeof (Uint32);
    isize = isize < 4096 ? 4096 : isize;
    SDL_GPUBufferCreateInfo iinfo = {
        .usage = SDL_GPU_BUFFERUSAGE_INDEX,
        .size = isize
    };
    ui->ibo = SDL_CreateGPUBuffer (renderer->device, &iinfo);
    if (ui->ibo == NULL) {
        free (ui->rects);
        TTF_CloseFont (ui->font);
        SDL_ReleaseGPUTexture (renderer->device, ui->white_texture);
        SDL_ReleaseGPUBuffer (renderer->device, ui->vbo);
        free (ui);
        SDL_Log ("Failed to create UI index buffer: %s", SDL_GetError ());
        return NULL;
    }
    ui->ibo_size = isize;

    ui->vertex = load_shader (
        renderer->device, "shaders/ui.vert.spv", SDL_GPU_SHADERSTAGE_VERTEX, 0,
        0, 0, 0
    );
    if (ui->vertex == NULL) {
        free (ui->rects);
        TTF_CloseFont (ui->font);
        SDL_ReleaseGPUTexture (renderer->device, ui->white_texture);
        SDL_ReleaseGPUBuffer (renderer->device, ui->vbo);
        SDL_ReleaseGPUBuffer (renderer->device, ui->ibo);
        free (ui);
        return NULL;
    }
    ui->fragment = load_shader (
        renderer->device, "shaders/ui.frag.spv", SDL_GPU_SHADERSTAGE_FRAGMENT,
        1, 0, 0, 0
    );
    if (ui->fragment == NULL) {
        free (ui->rects);
        TTF_CloseFont (ui->font);
        SDL_ReleaseGPUTexture (renderer->device, ui->white_texture);
        SDL_ReleaseGPUBuffer (renderer->device, ui->vbo);
        SDL_ReleaseGPUBuffer (renderer->device, ui->ibo);
        SDL_ReleaseGPUShader (renderer->device, ui->vertex);
        free (ui);
        return NULL;
    }

    SDL_GPUGraphicsPipelineCreateInfo info = {
        .target_info =
            {
                .num_color_targets = 1,
                .color_target_descriptions =
                    (SDL_GPUColorTargetDescription[]) {
                        {.format = renderer->format,
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
        .vertex_shader = ui->vertex,
        .fragment_shader = ui->fragment,
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
    ui->pipeline = SDL_CreateGPUGraphicsPipeline (renderer->device, &info);
    if (ui->pipeline == NULL) {
        free (ui->rects);
        TTF_CloseFont (ui->font);
        SDL_ReleaseGPUTexture (renderer->device, ui->white_texture);
        SDL_ReleaseGPUBuffer (renderer->device, ui->vbo);
        SDL_ReleaseGPUBuffer (renderer->device, ui->ibo);
        SDL_ReleaseGPUShader (renderer->device, ui->vertex);
        SDL_ReleaseGPUShader (renderer->device, ui->fragment);
        free (ui);
        SDL_Log ("Unable to create UI graphics pipeline: %s", SDL_GetError ());
        return NULL;
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
    ui->rects[ui->rect_count].texture = ui->white_texture;
    ui->rect_count++;
}

static SDL_GPUTexture*
ui_create_text_texture (SDL_GPUDevice* device, SDL_Surface* abgr) {
    SDL_GPUTextureCreateInfo texinfo = {
        .type = SDL_GPU_TEXTURETYPE_2D,
        .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
        .width = (Uint32) abgr->w,
        .height = (Uint32) abgr->h,
        .layer_count_or_depth = 1,
        .num_levels = 1,
        .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER
    };
    SDL_GPUTexture* tex = SDL_CreateGPUTexture (device, &texinfo);
    if (!tex) {
        SDL_Log ("UI text texture create failed: %s", SDL_GetError ());
        return NULL;
    }
    SDL_GPUTransferBufferCreateInfo tbinfo = {
        .size = (Uint32) (abgr->pitch * abgr->h),
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD
    };
    SDL_GPUTransferBuffer* tbuf = SDL_CreateGPUTransferBuffer (device, &tbinfo);
    if (!tbuf) {
        SDL_ReleaseGPUTexture (device, tex);
        SDL_Log ("UI text transfer buffer create failed: %s", SDL_GetError ());
        return NULL;
    }
    void* map = SDL_MapGPUTransferBuffer (device, tbuf, false);
    if (!map) {
        SDL_ReleaseGPUTransferBuffer (device, tbuf);
        SDL_ReleaseGPUTexture (device, tex);
        SDL_Log ("UI text transfer buffer map failed: %s", SDL_GetError ());
        return NULL;
    }
    memcpy (map, abgr->pixels, tbinfo.size);
    SDL_UnmapGPUTransferBuffer (device, tbuf);

    SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer (device);
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
    SDL_ReleaseGPUTransferBuffer (device, tbuf);
    return tex;
}

int draw_text (
    UIComponent* ui,
    SDL_GPUDevice* device,
    const char* utf8,
    float x,
    float y,
    float r,
    float g,
    float b,
    float a
) {
    if (!ui || !ui->font || !utf8) return 0;
    if (ui->rect_count >= ui->max_rects) return 0;

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

    SDL_GPUTexture* tex = ui_create_text_texture (device, abgr);
    int w = abgr->w;
    int h = abgr->h;
    SDL_DestroySurface (abgr);
    if (!tex) return 0;

    ui->rects[ui->rect_count++] = (UIRect) {
        .rect =
            (SDL_FRect) {x, y + (float) TTF_GetFontDescent (ui->font) * 2.0f,
                         (float) w, (float) h},
        .color = (SDL_FColor) {r, g, b, a},
        .texture = tex,
    };
    return w;
}