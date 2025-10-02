#pragma once

#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <microui.h>

typedef struct {
  SDL_GPUShader* ui_vertex;
  SDL_GPUShader* ui_fragment_solid;
  SDL_GPUShader* ui_fragment_textured;
  SDL_GPUGraphicsPipeline* pipe_solid;
  SDL_GPUGraphicsPipeline* pipe_textured;
  SDL_GPUBuffer* vertex_buffer;
  SDL_GPUBuffer* index_buffer;
  Uint32 vb_capacity;  // in bytes
  Uint32 ib_capacity;  // in bytes
  Uint32 num_vertices; // current frame
  Uint32 num_indices;  // current frame
  SDL_GPUTexture* glyph_atlas; // not used in naive path
  SDL_GPUSampler* ui_sampler;  // reuse engine sampler if you want
  int fb_w, fb_h;              // framebuffer size for this frame
  mu_Context* m_context;
  TTF_Font* font;
  // Current scissor
  SDL_Rect scissor;
} UIRenderer;

typedef struct {
  float x, y;      // px
  float u, v;      // uv
  float r, g, b, a; // color
} UiVertex;

extern const char button_map[256];
extern const char key_map[256];

void ui_handleInput(mu_Context* context, SDL_Event* event);

int getTextWidth(void* font, const char* str, int len);
int getTextHeight(void* font);

// Init/shutdown
UIRenderer* ui_init(SDL_GPUDevice* device,
                    SDL_GPUTextureFormat swapchain_format,
                    const char* font_path,
                    int font_px);
void ui_shutdown(SDL_GPUDevice* device, UIRenderer* ui);

// Per-frame
void ui_begin(UIRenderer* ui, int fb_w, int fb_h);
void ui_set_clip(UIRenderer* ui, SDL_Rect clip);
void ui_add_rect(UIRenderer* ui, SDL_FRect r, SDL_Color c);

// Naive text path: creates a temporary texture for the string and draws one quad
// Good enough to get you on-screen; replace with an atlas later.
void ui_add_text_naive(SDL_GPUDevice* dev,
                       SDL_GPURenderPass* pass,
                       UIRenderer* ui,
                       int x, int y,
                       const char* s,
                       SDL_Color c);

// Upload and draw (batched rects only; text-naive draws immediately)
void ui_upload_buffers(SDL_GPUDevice* dev);
void ui_draw(UIRenderer* ui,
             SDL_GPUDevice* dev,
             SDL_GPURenderPass* pass,
             SDL_GPUSampler* sampler,
             SDL_GPUTextureFormat swapchain_format);