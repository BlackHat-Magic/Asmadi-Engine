#include <SDL3/SDL_gpu.h>
#include <stdlib.h>
#include <string.h>

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <microui.h>

#include <core/appstate.h>
#include <ui/ui.h>
#include <material/m_common.h>

const char button_map[256] = {
    [SDL_BUTTON_LEFT & 0xff] = MU_MOUSE_LEFT,
    [SDL_BUTTON_RIGHT & 0xff] = MU_MOUSE_RIGHT,
    [SDL_BUTTON_MIDDLE & 0xff] = MU_MOUSE_MIDDLE,
};
const char key_map[256] = {
    [SDLK_LSHIFT & 0xff] = MU_KEY_SHIFT,
    [SDLK_RSHIFT & 0xff] = MU_KEY_SHIFT,
    [SDLK_LCTRL & 0xff] = MU_KEY_CTRL,
    [SDLK_RCTRL & 0xff] = MU_KEY_CTRL,
    [SDLK_LALT & 0xff] = MU_KEY_ALT,
    [SDLK_RALT & 0xff] = MU_KEY_ALT,
    [SDLK_RETURN & 0xff] = MU_KEY_RETURN,
    [SDLK_BACKSPACE & 0xff] = MU_KEY_BACKSPACE,
};

void ui_handleInput(mu_Context* context, SDL_Event* event) {
  switch (event->type) {
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP: {
      int b = button_map[event->button.button & 0xff];
      if (b && event->type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
        mu_input_mousedown(context, event->button.x, event->button.y, b);
      } else if (b && event->type == SDL_EVENT_MOUSE_BUTTON_UP) {
        mu_input_mouseup(context, event->button.x, event->button.y, b);
      }
      break;
    }
    case SDL_EVENT_MOUSE_MOTION:
      mu_input_mousemove(context, event->motion.x, event->motion.y);
      break;
    case SDL_EVENT_MOUSE_WHEEL:
      mu_input_scroll(context, event->wheel.x * -30, event->wheel.y * -30);
      break;
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP: {
      int c = key_map[event->key.key & 0xff];
      if (c && event->type == SDL_EVENT_KEY_DOWN) mu_input_keydown(context, c);
      if (c && event->type == SDL_EVENT_KEY_UP) mu_input_keyup(context, c);
      break;
    }
    case SDL_EVENT_TEXT_INPUT:
      mu_input_text(context, event->text.text);
      break;
  }
}

int getTextWidth(void* font, const char* str, int len) {
    TTF_Font* ttf = (TTF_Font*)font;
    if (!str) return 0;

    // SDL3_ttf takes length in bytes; 0 means null-terminated.
    size_t length = (len > 0) ? (size_t)len : 0;

    int w = 0, h = 0;
    if (!TTF_GetStringSize(ttf, str, length, &w, &h)) {
    // success -> w,h filled
    return w;
    }
    // failure
    return 0;
}

int getTextHeight(void* font) {
    TTF_Font* ttf = (TTF_Font*)font;
    return TTF_GetFontHeight(ttf);
}

static SDL_GPUGraphicsPipeline* create_ui_pipeline(SDL_GPUDevice* device,
                                                   SDL_GPUShader* vs,
                                                   SDL_GPUShader* fs,
                                                   SDL_GPUTextureFormat fmt,
                                                   bool textured) {
  SDL_GPUColorTargetBlendState blend = {
      .src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA,
      .dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
      .color_blend_op = SDL_GPU_BLENDOP_ADD,
      .src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE,
      .dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
      .alpha_blend_op = SDL_GPU_BLENDOP_ADD,
      .color_write_mask = (SDL_GPU_COLORCOMPONENT_R |
                           SDL_GPU_COLORCOMPONENT_G |
                           SDL_GPU_COLORCOMPONENT_B |
                           SDL_GPU_COLORCOMPONENT_A),
      .enable_blend = true,
      .enable_color_write_mask = true,
  };

  SDL_GPUColorTargetDescription color_target = {
      .format = fmt,
      .blend_state = blend,
  };

  SDL_GPUGraphicsPipelineCreateInfo info = {
      .target_info =
          {
              .num_color_targets = 1,
              .color_target_descriptions = &color_target,
              .has_depth_stencil_target = false,
          },
      .primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
      .vertex_shader = vs,
      .fragment_shader = fs,
      .vertex_input_state =
          {
              .vertex_buffer_descriptions =
                  (SDL_GPUVertexBufferDescription[]){
                      {
                          .slot = 0,
                          .pitch = (Uint32)sizeof(UiVertex),
                          .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
                          .instance_step_rate = 0,
                      },
                  },
              .num_vertex_buffers = 1,
              .num_vertex_attributes = 3,
              .vertex_attributes =
                  (SDL_GPUVertexAttribute[]){
                      { .location = 0, .buffer_slot = 0,
                        .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
                        .offset = 0 },
                      { .location = 1, .buffer_slot = 0,
                        .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
                        .offset = 2 * sizeof(float) },
                      { .location = 2, .buffer_slot = 0,
                        .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,
                        .offset = 4 * sizeof(float) },
                  },
          },
      .rasterizer_state =
          {
              .fill_mode = SDL_GPU_FILLMODE_FILL,
              .cull_mode = SDL_GPU_CULLMODE_NONE,
              .front_face = SDL_GPU_FRONTFACE_CLOCKWISE,
          },
      .depth_stencil_state =
          {
              .enable_depth_test = false,
              .enable_depth_write = false,
          },
  };

  SDL_GPUGraphicsPipeline* pipe = SDL_CreateGPUGraphicsPipeline(device, &info);
  if (!pipe) {
    SDL_Log("Failed to create UI pipeline: %s", SDL_GetError());
    return NULL;
  }
  return pipe;
}

UIRenderer* ui_init(SDL_GPUDevice* device,
                    SDL_GPUTextureFormat swapchain_format,
                    const char* font_path,
                    int font_px) {
  UIRenderer* renderer = (UIRenderer*)calloc(1, sizeof(UIRenderer));
  if (!renderer) {
    SDL_Log("Unable to allocate UIRenderer.");
    return NULL;
  }

  // Load shaders (compiled by CMake to build/shaders/*.spv)
  renderer->ui_vertex =
      load_shader(device, "shaders/ui.vert.spv", SDL_GPU_SHADERSTAGE_VERTEX,
                  /*samplers*/ 0, /*uniform_buffers*/ 0, 0, 0);
  if (!renderer->ui_vertex) {
    free(renderer);
    return NULL;
  }
  renderer->ui_fragment_solid =
      load_shader(device, "shaders/ui_solid.frag.spv",
                  SDL_GPU_SHADERSTAGE_FRAGMENT, 0, 0, 0, 0);
  if (!renderer->ui_fragment_solid) {
    SDL_ReleaseGPUShader(device, renderer->ui_vertex);
    free(renderer);
    return NULL;
  }
  renderer->ui_fragment_textured =
      load_shader(device, "shaders/ui_textured.frag.spv",
                  SDL_GPU_SHADERSTAGE_FRAGMENT, 1, 0, 0, 0);
  if (!renderer->ui_fragment_textured) {
    SDL_ReleaseGPUShader(device, renderer->ui_fragment_solid);
    SDL_ReleaseGPUShader(device, renderer->ui_vertex);
    free(renderer);
    return NULL;
  }

  // Pipelines with blending, depth disabled
  renderer->pipe_solid = create_ui_pipeline(
      device, renderer->ui_vertex, renderer->ui_fragment_solid,
      swapchain_format, false);
  if (!renderer->pipe_solid) {
    SDL_ReleaseGPUShader(device, renderer->ui_fragment_textured);
    SDL_ReleaseGPUShader(device, renderer->ui_fragment_solid);
    SDL_ReleaseGPUShader(device, renderer->ui_vertex);
    free(renderer);
    return NULL;
  }
  renderer->pipe_textured = create_ui_pipeline(
      device, renderer->ui_vertex, renderer->ui_fragment_textured,
      swapchain_format, true);
  if (!renderer->pipe_textured) {
    SDL_ReleaseGPUGraphicsPipeline(device, renderer->pipe_solid);
    SDL_ReleaseGPUShader(device, renderer->ui_fragment_textured);
    SDL_ReleaseGPUShader(device, renderer->ui_fragment_solid);
    SDL_ReleaseGPUShader(device, renderer->ui_vertex);
    free(renderer);
    return NULL;
  }

  // Create dynamic buffers (64k vertices/indices to start)
  renderer->vb_capacity = 64 * 1024; // bytes
  renderer->ib_capacity = 64 * 1024; // bytes

  SDL_GPUBufferCreateInfo vinfo = {
      .size = renderer->vb_capacity,
      .usage = SDL_GPU_BUFFERUSAGE_VERTEX,
  };
  renderer->vertex_buffer = SDL_CreateGPUBuffer(device, &vinfo);
  if (!renderer->vertex_buffer) {
    SDL_Log("UI: failed to create vertex buffer: %s", SDL_GetError());
    ui_shutdown(device, renderer);
    return NULL;
  }
  SDL_GPUBufferCreateInfo iinfo = {
      .size = renderer->ib_capacity,
      .usage = SDL_GPU_BUFFERUSAGE_INDEX,
  };
  renderer->index_buffer = SDL_CreateGPUBuffer(device, &iinfo);
  if (!renderer->index_buffer) {
    SDL_Log("UI: failed to create index buffer: %s", SDL_GetError());
    ui_shutdown(device, renderer);
    return NULL;
  }

  // MicroUI context and SDL_ttf font
  renderer->font = TTF_OpenFont(font_path, font_px);
  if (!renderer->font) {
    SDL_Log("UI: Failed to open font %s: %s", font_path, SDL_GetError());
    ui_shutdown(device, renderer);
    return NULL;
  }
  renderer->m_context = (mu_Context*)malloc(sizeof(mu_Context));
  if (!renderer->m_context) {
    SDL_Log("UI: Failed to allocate microui context");
    ui_shutdown(device, renderer);
    return NULL;
  }
  mu_init(renderer->m_context);
  renderer->m_context->text_width = getTextWidth;
  renderer->m_context->text_height = getTextHeight;

  // Default scissor to full screen
  renderer->scissor = (SDL_Rect){0, 0, 0x7fffffff, 0x7fffffff};

  return renderer;
}

void ui_shutdown(SDL_GPUDevice* device, UIRenderer* ui) {
  if (!ui) return;
  if (ui->m_context) free(ui->m_context);
  if (ui->font) TTF_CloseFont(ui->font);
  if (ui->glyph_atlas) SDL_ReleaseGPUTexture(device, ui->glyph_atlas);
  if (ui->pipe_textured) SDL_ReleaseGPUGraphicsPipeline(device, ui->pipe_textured);
  if (ui->pipe_solid) SDL_ReleaseGPUGraphicsPipeline(device, ui->pipe_solid);
  if (ui->ui_fragment_textured) SDL_ReleaseGPUShader(device, ui->ui_fragment_textured);
  if (ui->ui_fragment_solid) SDL_ReleaseGPUShader(device, ui->ui_fragment_solid);
  if (ui->ui_vertex) SDL_ReleaseGPUShader(device, ui->ui_vertex);
  if (ui->index_buffer) SDL_ReleaseGPUBuffer(device, ui->index_buffer);
  if (ui->vertex_buffer) SDL_ReleaseGPUBuffer(device, ui->vertex_buffer);
  free(ui);
}

void ui_begin(UIRenderer* ui, int fb_w, int fb_h) {
  ui->fb_w = fb_w;
  ui->fb_h = fb_h;
  ui->num_vertices = 0;
  ui->num_indices = 0;
  ui->scissor = (SDL_Rect){0, 0, fb_w, fb_h};
}

// Append a solid-colored rectangle (two triangles) to the CPU-side batch.
// Note: this only writes into a temporary CPU array; we upload later.
static void ensure_capacity(UIRenderer* ui, Uint32 add_vertices, Uint32 add_indices) {
  // We only check sizes; for a minimal path, we’ll just cap to capacity.
  Uint32 v_needed = (ui->num_vertices + add_vertices) * sizeof(UiVertex);
  Uint32 i_needed = (ui->num_indices + add_indices) * sizeof(Uint16);
  if (v_needed > ui->vb_capacity || i_needed > ui->ib_capacity) {
    // You can realloc and recreate buffers; for now, log and clamp
    SDL_Log("UI batch overflow (v:%u i:%u), consider increasing capacities",
            v_needed, i_needed);
  }
}

static void write_vertices_to_staging(SDL_GPUDevice* dev,
                                      SDL_GPUBuffer* gpu_buf,
                                      const void* data,
                                      Uint32 size) {
  SDL_GPUTransferBufferCreateInfo tinfo = {
      .size = size,
      .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
  };
  SDL_GPUTransferBuffer* tb = SDL_CreateGPUTransferBuffer(dev, &tinfo);
  void* ptr = SDL_MapGPUTransferBuffer(dev, tb, false);
  memcpy(ptr, data, size);
  SDL_UnmapGPUTransferBuffer(dev, tb);

  SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(dev);
  SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(cmd);

  SDL_GPUTransferBufferLocation src = {.transfer_buffer = tb, .offset = 0};
  SDL_GPUBufferRegion dst = {.buffer = gpu_buf, .offset = 0, .size = size};
  SDL_UploadToGPUBuffer(copy, &src, &dst, false);

  SDL_EndGPUCopyPass(copy);
  SDL_SubmitGPUCommandBuffer(cmd);
  SDL_ReleaseGPUTransferBuffer(dev, tb);
}

typedef struct {
  // simple linear arrays per frame
  UiVertex* verts;
  Uint16* indices;
  Uint32 verts_cap;
  Uint32 inds_cap;
} UiBatchCPU;

static UiBatchCPU g_batch = {0};

static void ui_batch_reserve(Uint32 vcap, Uint32 icap) {
  if (g_batch.verts_cap < vcap) {
    g_batch.verts = (UiVertex*)realloc(g_batch.verts, vcap * sizeof(UiVertex));
    g_batch.verts_cap = vcap;
  }
  if (g_batch.inds_cap < icap) {
    g_batch.indices = (Uint16*)realloc(g_batch.indices, icap * sizeof(Uint16));
    g_batch.inds_cap = icap;
  }
}

void ui_set_clip(UIRenderer* ui, SDL_Rect clip) {
  ui->scissor = clip;
}

void ui_add_rect(UIRenderer* ui, SDL_FRect r, SDL_Color c) {
  // 4 verts, 6 indices
  ensure_capacity(ui, 4, 6);
  ui_batch_reserve(ui->num_vertices + 4, ui->num_indices + 6);

  Uint16 base = (Uint16)ui->num_vertices;
  float x = r.x, y = r.y, w = r.w, h = r.h;
  float rcpw = 1.0f, rcpv = 1.0f;

  UiVertex v0 = {x, y, 0.0f, 0.0f, c.r / 255.0f, c.g / 255.0f, c.b / 255.0f,
                 c.a / 255.0f};
  UiVertex v1 = {x + w, y, 0.0f, 0.0f, v0.r, v0.g, v0.b, v0.a};
  UiVertex v2 = {x + w, y + h, 0.0f, 0.0f, v0.r, v0.g, v0.b, v0.a};
  UiVertex v3 = {x, y + h, 0.0f, 0.0f, v0.r, v0.g, v0.b, v0.a};

  g_batch.verts[ui->num_vertices + 0] = v0;
  g_batch.verts[ui->num_vertices + 1] = v1;
  g_batch.verts[ui->num_vertices + 2] = v2;
  g_batch.verts[ui->num_vertices + 3] = v3;

  g_batch.indices[ui->num_indices + 0] = base + 0;
  g_batch.indices[ui->num_indices + 1] = base + 1;
  g_batch.indices[ui->num_indices + 2] = base + 2;
  g_batch.indices[ui->num_indices + 3] = base + 0;
  g_batch.indices[ui->num_indices + 4] = base + 2;
  g_batch.indices[ui->num_indices + 5] = base + 3;

  ui->num_vertices += 4;
  ui->num_indices += 6;
}

// Naive text: create an SDL_Surface for the entire string, upload to a temporary
// texture, draw one quad immediately (not batched with shapes), then destroy temp.
void ui_add_text_naive(SDL_GPUDevice* dev,
                       SDL_GPURenderPass* pass,
                       UIRenderer* ui,
                       int x, int y,
                       const char* s,
                       SDL_Color c) {
  if (!ui->font || !s || !*s) return;

  SDL_Surface* surf =
    TTF_RenderText_Blended(ui->font, s, 0 /* 0 = null-terminated */, 
                           (SDL_Color){255, 255, 255, 255});
  if (!surf) return;

  SDL_Surface* rgba = SDL_ConvertSurface(surf, SDL_PIXELFORMAT_ABGR8888);
  if (!rgba) rgba = surf;

  SDL_GPUTextureCreateInfo tinfo = {
      .type = SDL_GPU_TEXTURETYPE_2D,
      .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
      .width = (Uint32)rgba->w,
      .height = (Uint32)rgba->h,
      .layer_count_or_depth = 1,
      .num_levels = 1,
      .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER,
  };
  SDL_GPUTexture* tex = SDL_CreateGPUTexture(dev, &tinfo);
  if (!tex) {
    if (rgba != surf) SDL_DestroySurface(rgba);
    SDL_DestroySurface(surf);
    return;
  }

  // upload pixels
  SDL_GPUTransferBufferCreateInfo tbinfo = {
      .size = (Uint32)(rgba->pitch * rgba->h),
      .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
  };
  SDL_GPUTransferBuffer* tb = SDL_CreateGPUTransferBuffer(dev, &tbinfo);
  void* ptr = SDL_MapGPUTransferBuffer(dev, tb, false);
  memcpy(ptr, rgba->pixels, tbinfo.size);
  SDL_UnmapGPUTransferBuffer(dev, tb);

  SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(dev);
  SDL_GPUCopyPass* cp = SDL_BeginGPUCopyPass(cmd);
  SDL_GPUTextureTransferInfo src = {.transfer_buffer = tb,
                                    .offset = 0,
                                    .pixels_per_row = (Uint32)rgba->w,
                                    .rows_per_layer = (Uint32)rgba->h};
  SDL_GPUTextureRegion dst = {.texture = tex,
                              .w = (Uint32)rgba->w,
                              .h = (Uint32)rgba->h,
                              .d = 1};
  SDL_UploadToGPUTexture(cp, &src, &dst, false);
  SDL_EndGPUCopyPass(cp);
  SDL_SubmitGPUCommandBuffer(cmd);
  SDL_ReleaseGPUTransferBuffer(dev, tb);

  if (rgba != surf) SDL_DestroySurface(rgba);
  SDL_DestroySurface(surf);

  // Draw one quad now (bind textured pipeline and sampler)
  SDL_BindGPUGraphicsPipeline(pass, ui->pipe_textured);

  // Set scissor
  SDL_Rect sc = (SDL_Rect){ ui->scissor.x, ui->scissor.y, ui->scissor.w, ui->scissor.h };
    SDL_SetGPUScissor(pass, &sc);

  // Build a tiny CPU quad
  UiVertex v[4];
  float w = (float)tinfo.width, h = (float)tinfo.height;
  float cr = c.r / 255.0f, cg = c.g / 255.0f, cb = c.b / 255.0f,
        ca = c.a / 255.0f;
  v[0] = (UiVertex){(float)x, (float)y, 0.0f, 0.0f, cr, cg, cb, ca};
  v[1] = (UiVertex){(float)x + w, (float)y, 1.0f, 0.0f, cr, cg, cb, ca};
  v[2] = (UiVertex){(float)x + w, (float)y + h, 1.0f, 1.0f, cr, cg, cb, ca};
  v[3] = (UiVertex){(float)x, (float)y + h, 0.0f, 1.0f, cr, cg, cb, ca};
  Uint16 idx[6] = {0, 1, 2, 0, 2, 3};

  // Upload this quad to the existing VB/IB (overwriting the start)
  write_vertices_to_staging(dev, ui->vertex_buffer, v, sizeof(v));
  write_vertices_to_staging(dev, ui->index_buffer, idx, sizeof(idx));

  // Bind VB/IB
  SDL_GPUBufferBinding vbb = {.buffer = ui->vertex_buffer, .offset = 0};
  SDL_GPUBufferBinding ibb = {.buffer = ui->index_buffer, .offset = 0};
  SDL_BindGPUVertexBuffers(pass, 0, &vbb, 1);
  SDL_BindGPUIndexBuffer(pass, &ibb, SDL_GPU_INDEXELEMENTSIZE_16BIT);

  // Bind sampler+texture at fragment set 0 binding 0
  SDL_GPUTextureSamplerBinding ts = {.texture = tex, .sampler = ui->ui_sampler};
  SDL_BindGPUFragmentSamplers(pass, 0, &ts, 1);

  // Draw
  SDL_DrawGPUIndexedPrimitives(pass, 6, 1, 0, 0, 0);

  // Cleanup temp texture
  SDL_ReleaseGPUTexture(dev, tex);
}

void ui_upload_buffers(SDL_GPUDevice* dev) {
  // Upload g_batch arrays into GPU buffers
  Uint32 vsize = (Uint32)(g_batch.verts ? g_batch.verts_cap : 0) * sizeof(UiVertex);
  Uint32 isize = (Uint32)(g_batch.indices ? g_batch.inds_cap : 0) * sizeof(Uint16);

  // Only upload the used portion
  Uint32 vused = (Uint32)sizeof(UiVertex) * (Uint32)g_batch.verts_cap;
  Uint32 iused = (Uint32)sizeof(Uint16) * (Uint32)g_batch.inds_cap;

  if (g_batch.verts && vused > 0)
    write_vertices_to_staging(dev, ((UIRenderer*)0)->vertex_buffer /* placeholder */, NULL, 0);
  (void)vsize;
  (void)isize;
  (void)vused;
  (void)iused;
  // Note: we will upload exact used lengths in ui_draw to avoid having to know the UIRenderer here.
}

// Draw the batched rectangles (ui_add_rect) and clear batch.
// Text (naive) is drawn immediately via ui_add_text_naive and isn’t part of the batch.
void ui_draw(UIRenderer* ui,
             SDL_GPUDevice* dev,
             SDL_GPURenderPass* pass,
             SDL_GPUSampler* sampler,
             SDL_GPUTextureFormat swapchain_format) {
  if (ui->num_indices == 0) return;

  // Upload batched vertices/indices
  Uint32 vbytes = ui->num_vertices * sizeof(UiVertex);
  Uint32 ibytes = ui->num_indices * sizeof(Uint16);
  write_vertices_to_staging(dev, ui->vertex_buffer, g_batch.verts, vbytes);
  write_vertices_to_staging(dev, ui->index_buffer, g_batch.indices, ibytes);

  SDL_BindGPUGraphicsPipeline(pass, ui->pipe_solid);

  // Set scissor
  SDL_Rect sc = (SDL_Rect){ui->scissor.x, ui->scissor.y,
                           ui->scissor.w, ui->scissor.h};
  SDL_SetGPUScissor(pass, &sc);

  // Bind VB/IB
  SDL_GPUBufferBinding vbb = {.buffer = ui->vertex_buffer, .offset = 0};
  SDL_GPUBufferBinding ibb = {.buffer = ui->index_buffer, .offset = 0};
  SDL_BindGPUVertexBuffers(pass, 0, &vbb, 1);
  SDL_BindGPUIndexBuffer(pass, &ibb, SDL_GPU_INDEXELEMENTSIZE_16BIT);

  SDL_DrawGPUIndexedPrimitives(pass, ui->num_indices, 1, 0, 0, 0);

  // Clear batch
  ui->num_vertices = 0;
  ui->num_indices = 0;
}