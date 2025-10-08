#pragma once

#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>
#include <core/appstate.h>

int UI_Init (AppState* state);

int UI_DrawRect (AppState* state, SDL_GPUCommandBuffer* cmd, SDL_GPURenderPass* pass, float x, float y, float w, float h, float r, float g, float b, float a);

void UI_Shutdown (AppState* state);