#pragma once

#include <SDL3/SDL.h>

#include "math/matrix.h"
#include "core/appstate.h"

// TODO: More robust max entities
#define MAX_ENTITIES 1024

typedef struct {
    vec3 colors[3];
    float model[16];
    float view[16];
    float proj[16];
} UBOData;

typedef uint32_t Entity;

typedef struct {
    mat4 model;
} TransformComponent;

typedef struct {
    SDL_GPUBuffer* vertex_buffer;
    uint32_t num_vertices;
    SDL_GPUBuffer* index_buffer;
    uint32_t num_indices;
    SDL_GPUIndexElementSize index_size;
} MeshComponent;

typedef struct {
    vec3 color;
    SDL_GPUTexture* texture;
    SDL_GPUShader* vertex_shader;
    SDL_GPUShader* fragment_shader;
    SDL_GPUGraphicsPipeline* pipeline;
} MaterialComponent;

// TODO: colliders

// ECS storage arrays per component type (sparse; use entity as index)
extern TransformComponent transforms[MAX_ENTITIES];
extern MeshComponent meshes[MAX_ENTITIES];
extern MaterialComponent materials[MAX_ENTITIES];
extern uint8_t entity_active[MAX_ENTITIES];

// API
Entity create_entity ();
void destroy_entity (AppState* state, Entity e);

// add components
void add_transform (Entity e, vec3 pos, vec3 rot, vec3 scale);
void add_mesh (Entity e, MeshComponent mesh);
void add_material (Entity e, MaterialComponent material);

SDL_AppResult render_system (AppState* state);