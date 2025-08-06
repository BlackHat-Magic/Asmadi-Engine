#pragma once

#include <SDL3/SDL.h>

#include "math/matrix.h"
#include "core/appstate.h"

// TODO: More robust max lights
#define MAX_LIGHTS 64

typedef enum {
    SIDE_FRONT,
    SIDE_BACK,
    SIDE_DOUBLE,
} MaterialSide;

typedef struct {
    vec4 color;
    float model[16];
    float view[16];
    float proj[16];
    vec4 ambient_color[MAX_LIGHTS]; // RGB + Strength
    vec4 point_light_pos[MAX_LIGHTS]; // xyz + padding (16-byte aligned)
    vec4 point_light_color[MAX_LIGHTS]; // RGB + Strength
    vec4 camera_pos;
} UBOData;

typedef uint32_t Entity;

typedef struct {
    vec3 position;
    vec4 rotation; // quat
    vec3 scale;
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
    MaterialSide side;
} MaterialComponent;

typedef struct {
    float fov;
    float near_clip;
    float far_clip;
} CameraComponent;

typedef struct {
    float mouse_sense;
    float move_speed;
} FpsCameraControllerComponent;

// Billboard is a flag (no data)

typedef vec4 AmbientLightComponent;

typedef vec4 PointLightComponent; // position is another component

// ECS API
Entity create_entity(void);
void destroy_entity(AppState* state, Entity e);

// Transforms
void add_transform(Entity e, vec3 pos, vec3 rot, vec3 scale);
TransformComponent* get_transform(Entity e);
bool has_transform(Entity e);
void remove_transform(Entity e);

// Meshes
void add_mesh(Entity e, MeshComponent mesh);
MeshComponent* get_mesh(Entity e);
bool has_mesh(Entity e);
void remove_mesh(AppState* state, Entity e); // state for device release

// Materials
void add_material(Entity e, MaterialComponent material);
MaterialComponent* get_material(Entity e);
bool has_material(Entity e);
void remove_material(AppState* state, Entity e); // state for device release

// Cameras
void add_camera(Entity e, float fov, float near_clip, float far_clip);
CameraComponent* get_camera(Entity e);
bool has_camera(Entity e);
void remove_camera(Entity e);

// FPS Controllers
void add_fps_controller(Entity e, float sense, float speed);
FpsCameraControllerComponent* get_fps_controller(Entity e);
bool has_fps_controller(Entity e);
void remove_fps_controller(Entity e);

// Billboards (flag)
void add_billboard(Entity e);
bool has_billboard(Entity e);
void remove_billboard(Entity e);

// Ambient Lights
void add_ambient_light(Entity e, vec3 rgb, float brightness);
AmbientLightComponent* get_ambient_light(Entity e);
bool has_ambient_light(Entity e);
void remove_ambient_light(Entity e);

// Point Lights
void add_point_light(Entity e, vec3 rgb, float brightness);
PointLightComponent* get_point_light(Entity e);
bool has_point_light(Entity e);
void remove_point_light(Entity e);

// Systems
void fps_controller_event_system(AppState* state, SDL_Event* event);
void fps_controller_update_system(AppState* state, float dt);
SDL_AppResult render_system(AppState* state);

void free_pools(AppState* state);