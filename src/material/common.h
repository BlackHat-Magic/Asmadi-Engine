#pragma once

#include <SDL3/SDL.h>

#include <ecs/ecs.h>

SDL_GPUShader* load_shader (SDL_GPUDevice* device, const char* filename, SDL_GPUShaderStage state, Uint32 sampler_count, Uint32 uniform_buffer_count, Uint32 storage_buffer_count, Uint32 storage_texture_count);

SDL_GPUTexture* load_texture(SDL_GPUDevice* device, const char* bmp_file_path);

void set_vertex_shader (SDL_GPUDevice* device, MaterialComponent* mat, const char* filepath, SDL_GPUTextureFormat swapchain_format);

void set_fragment_shader (SDL_GPUDevice* device, MaterialComponent* mat, const char* filepath, SDL_GPUTextureFormat swapchain_format);

static void build_pipeline (SDL_GPUDevice* device, MaterialComponent* mat, SDL_GPUTextureFormat swapchain_format);