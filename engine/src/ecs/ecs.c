#include <stdlib.h>
#include <string.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>

#include "ecs/ecs.h"
#include <math.h>
#include "math/matrix.h"
#include "geometry/g_common.h"
#include "material/m_common.h"

static uint32_t next_entity_id = 0;

typedef struct {
    void* data;
    uint32_t* entity_to_index;
    uint32_t* index_to_entity;
    uint32_t count;
    uint32_t data_capacity;
    uint32_t entity_capacity;
} GenericPool;

static GenericPool transform_pool = {0};
static GenericPool mesh_pool = {0};
static GenericPool material_pool = {0};
static GenericPool camera_pool = {0};
static GenericPool fps_controller_pool = {0};
static GenericPool billboard_pool = {0}; // no data, just presence
static GenericPool ambient_light_pool = {0};
static GenericPool point_light_pool = {0};

// Helper to grow entity_to_index (sparse map)
static void grow_entity_map(GenericPool* pool, uint32_t min_entity) {
    uint32_t new_cap = pool->entity_capacity ? pool->entity_capacity * 2 : 1024;
    if (new_cap <= min_entity) new_cap = min_entity + 1;
    uint32_t* new_map = (uint32_t*)realloc(pool->entity_to_index, new_cap * sizeof(uint32_t));
    if (!new_map) {
        SDL_Log("Failed to realloc entity map");
        return;
    }
    for (uint32_t i = pool->entity_capacity; i < new_cap; i++) {
        new_map[i] = ~0u;
    }
    pool->entity_to_index = new_map;
    pool->entity_capacity = new_cap;
}

// Helper to grow data and index_to_entity (dense)
static void grow_data(GenericPool* pool, size_t component_size) {
    uint32_t new_cap = pool->data_capacity ? pool->data_capacity * 2 : 64;
    void* new_data = realloc(pool->data, new_cap * component_size);
    uint32_t* new_idx_ent = (uint32_t*)realloc(pool->index_to_entity, new_cap * sizeof(uint32_t));
    if (!new_data || !new_idx_ent) {
        SDL_Log("Failed to realloc data pool");
        return;
    }
    pool->data = new_data;
    pool->index_to_entity = new_idx_ent;
    pool->data_capacity = new_cap;
}

// Generic has
static bool pool_has(const GenericPool* pool, Entity e) {
    return e < pool->entity_capacity && pool->entity_to_index[e] != ~0u;
}

// Generic remove (swap and pop)
static void pool_remove(GenericPool* pool, Entity e, size_t component_size) {
    if (!pool_has(pool, e)) return;
    uint32_t idx = pool->entity_to_index[e];
    uint32_t last = --pool->count;
    // Copy last to idx (if data exists)
    if (pool->data) {
        memcpy((char*)pool->data + idx * component_size, (char*)pool->data + last * component_size, component_size);
    }
    uint32_t swapped_e = pool->index_to_entity[last];
    pool->index_to_entity[idx] = swapped_e;
    pool->entity_to_index[swapped_e] = idx;
    pool->entity_to_index[e] = ~0u;
}

// Generic add/overwrite (with data copy)
static void pool_add(GenericPool* pool, Entity e, const void* comp_data, size_t component_size) {
    if (e >= pool->entity_capacity) {
        grow_entity_map(pool, e);
    }
    uint32_t idx;
    if (pool->entity_to_index[e] != ~0u) {
        // Overwrite
        idx = pool->entity_to_index[e];
        if (pool->data && comp_data) {
            memcpy((char*)pool->data + idx * component_size, comp_data, component_size);
        }
        return;
    }
    // Add new
    if (pool->count == pool->data_capacity) {
        grow_data(pool, component_size);
    }
    idx = pool->count++;
    if (pool->data && comp_data) {
        memcpy((char*)pool->data + idx * component_size, comp_data, component_size);
    }
    pool->index_to_entity[idx] = e;
    pool->entity_to_index[e] = idx;
}

// Generic get
static void* pool_get(const GenericPool* pool, Entity e, size_t component_size) {
    if (!pool_has(pool, e)) return NULL;
    uint32_t idx = pool->entity_to_index[e];
    return (char*)pool->data + idx * component_size;
}

Entity create_entity(void) {
    return next_entity_id++;
}

void destroy_entity(AppState* state, Entity e) {
    remove_transform(e);
    remove_mesh(state, e);
    remove_material(state, e);
    remove_camera(e);
    remove_fps_controller(e);
    remove_billboard(e);
    remove_ambient_light(e);
    remove_point_light(e);
}

// Transforms
void add_transform(Entity e, vec3 pos, vec3 rot, vec3 scale) {
    TransformComponent comp = { .position = pos, .rotation = quat_from_euler(rot), .scale = scale };
    pool_add(&transform_pool, e, &comp, sizeof(TransformComponent));
}
TransformComponent* get_transform(Entity e) {
    return (TransformComponent*)pool_get(&transform_pool, e, sizeof(TransformComponent));
}
bool has_transform(Entity e) {
    return pool_has(&transform_pool, e);
}
void remove_transform(Entity e) {
    pool_remove(&transform_pool, e, sizeof(TransformComponent));
}

// Meshes
void add_mesh(Entity e, MeshComponent mesh) {
    pool_add(&mesh_pool, e, &mesh, sizeof(MeshComponent));
}
MeshComponent* get_mesh(Entity e) {
    return (MeshComponent*)pool_get(&mesh_pool, e, sizeof(MeshComponent));
}
bool has_mesh(Entity e) {
    return pool_has(&mesh_pool, e);
}
void remove_mesh(AppState* state, Entity e) {
    MeshComponent* mesh = get_mesh(e);
    if (mesh) {
        if (mesh->vertex_buffer) SDL_ReleaseGPUBuffer(state->device, mesh->vertex_buffer);
        if (mesh->index_buffer) SDL_ReleaseGPUBuffer(state->device, mesh->index_buffer);
    }
    pool_remove(&mesh_pool, e, sizeof(MeshComponent));
}

// Materials
void add_material(Entity e, MaterialComponent material) {
    pool_add(&material_pool, e, &material, sizeof(MaterialComponent));
}
MaterialComponent* get_material(Entity e) {
    return (MaterialComponent*)pool_get(&material_pool, e, sizeof(MaterialComponent));
}
bool has_material(Entity e) {
    return pool_has(&material_pool, e);
}
void remove_material(AppState* state, Entity e) {
    MaterialComponent* mat = get_material(e);
    if (mat) {
        if (mat->texture) SDL_ReleaseGPUTexture(state->device, mat->texture);
        if (mat->pipeline) SDL_ReleaseGPUGraphicsPipeline(state->device, mat->pipeline);
        if (mat->vertex_shader) SDL_ReleaseGPUShader(state->device, mat->vertex_shader);
        if (mat->fragment_shader) SDL_ReleaseGPUShader(state->device, mat->fragment_shader);
    }
    pool_remove(&material_pool, e, sizeof(MaterialComponent));
}

// Cameras
void add_camera(Entity e, float fov, float near_clip, float far_clip) {
    CameraComponent comp = { .fov = fov, .near_clip = near_clip, .far_clip = far_clip };
    pool_add(&camera_pool, e, &comp, sizeof(CameraComponent));
}
CameraComponent* get_camera(Entity e) {
    return (CameraComponent*)pool_get(&camera_pool, e, sizeof(CameraComponent));
}
bool has_camera(Entity e) {
    return pool_has(&camera_pool, e);
}
void remove_camera(Entity e) {
    pool_remove(&camera_pool, e, sizeof(CameraComponent));
}

// FPS Controllers
void add_fps_controller(Entity e, float sense, float speed) {
    FpsCameraControllerComponent comp = { .mouse_sense = sense, .move_speed = speed };
    pool_add(&fps_controller_pool, e, &comp, sizeof(FpsCameraControllerComponent));
}
FpsCameraControllerComponent* get_fps_controller(Entity e) {
    return (FpsCameraControllerComponent*)pool_get(&fps_controller_pool, e, sizeof(FpsCameraControllerComponent));
}
bool has_fps_controller(Entity e) {
    return pool_has(&fps_controller_pool, e);
}
void remove_fps_controller(Entity e) {
    pool_remove(&fps_controller_pool, e, sizeof(FpsCameraControllerComponent));
}

// Billboards (flag, no data)
void add_billboard(Entity e) {
    pool_add(&billboard_pool, e, NULL, 0); // no data copy
}
bool has_billboard(Entity e) {
    return pool_has(&billboard_pool, e);
}
void remove_billboard(Entity e) {
    pool_remove(&billboard_pool, e, 0);
}

// Ambient Lights
void add_ambient_light(Entity e, vec3 rgb, float brightness) {
    AmbientLightComponent comp = { rgb.x, rgb.y, rgb.z, brightness };
    pool_add(&ambient_light_pool, e, &comp, sizeof(AmbientLightComponent));
}
AmbientLightComponent* get_ambient_light(Entity e) {
    return (AmbientLightComponent*)pool_get(&ambient_light_pool, e, sizeof(AmbientLightComponent));
}
bool has_ambient_light(Entity e) {
    return pool_has(&ambient_light_pool, e);
}
void remove_ambient_light(Entity e) {
    pool_remove(&ambient_light_pool, e, sizeof(AmbientLightComponent));
}

// Point Lights
void add_point_light(Entity e, vec3 rgb, float brightness) {
    PointLightComponent comp = { rgb.x, rgb.y, rgb.z, brightness };
    pool_add(&point_light_pool, e, &comp, sizeof(PointLightComponent));
}
PointLightComponent* get_point_light(Entity e) {
    return (PointLightComponent*)pool_get(&point_light_pool, e, sizeof(PointLightComponent));
}
bool has_point_light(Entity e) {
    return pool_has(&point_light_pool, e);
}
void remove_point_light(Entity e) {
    pool_remove(&point_light_pool, e, sizeof(PointLightComponent));
}

void fps_controller_event_system(AppState* state, SDL_Event* event) {
    for (uint32_t i = 0; i < fps_controller_pool.count; i++) {
        Entity e = fps_controller_pool.index_to_entity[i];
        FpsCameraControllerComponent* ctrl = &((FpsCameraControllerComponent*)fps_controller_pool.data)[i];
        TransformComponent* trans = get_transform(e);
        if (!trans) continue;

        switch (event->type) {
            case SDL_EVENT_MOUSE_MOTION: {
                float delta_yaw = event->motion.xrel * ctrl->mouse_sense;
                float delta_pitch = event->motion.yrel * ctrl->mouse_sense;

                vec4 dq_yaw = quat_from_axis_angle((vec3){0.0f, 1.0f, 0.0f}, delta_yaw);
                trans->rotation = quat_multiply(dq_yaw, trans->rotation);

                vec3 forward = vec3_rotate(trans->rotation, (vec3){0.0f, 0.0f, -1.0f});
                vec3 right = vec3_normalize(vec3_cross(forward, (vec3){0.0f, 1.0f, 0.0f}));
                vec4 dq_pitch = quat_from_axis_angle(right, delta_pitch);
                trans->rotation = quat_multiply(dq_pitch, trans->rotation);

                trans->rotation = quat_normalize(trans->rotation);

                forward = vec3_rotate(trans->rotation, (vec3){0.0f, 0.0f, -1.0f});
                float curr_pitch = asinf(forward.y);
                if (curr_pitch > (float)M_PI * 0.49f || curr_pitch < -(float)M_PI * 0.49f) {
                    float clamped_pitch = curr_pitch > (float)M_PI * 0.49f ? (float)M_PI * 0.49f : -(float)M_PI * 0.49f;
                    float curr_yaw = atan2f(forward.x, forward.z) + (float)M_PI;
                    trans->rotation = quat_from_euler((vec3){clamped_pitch, curr_yaw, 0.0f});
                }
                break;
            }
            case SDL_EVENT_KEY_DOWN:
                if (event->key.key == SDLK_ESCAPE) state->quit = true;
                break;
        }
    }
}

void fps_controller_update_system(AppState* state, float dt) {
    for (uint32_t i = 0; i < fps_controller_pool.count; i++) {
        Entity e = fps_controller_pool.index_to_entity[i];
        FpsCameraControllerComponent* ctrl = &((FpsCameraControllerComponent*)fps_controller_pool.data)[i];
        TransformComponent* trans = get_transform(e);
        if (!trans) continue;

        vec3 forward = vec3_rotate(trans->rotation, (vec3){0.0f, 0.0f, 1.0f});
        vec3 right = vec3_rotate(trans->rotation, (vec3){1.0f, 0.0f, 0.0f});
        vec3 up = vec3_rotate(trans->rotation, (vec3){0.0f, 1.0f, 0.0f});

        int numkeys;
        const bool* key_state = SDL_GetKeyboardState(&numkeys);
        vec3 motion = {0.0f, 0.0f, 0.0f};
        if (key_state[SDL_SCANCODE_W]) motion = vec3_add(motion, forward);
        if (key_state[SDL_SCANCODE_A]) motion = vec3_sub(motion, right);
        if (key_state[SDL_SCANCODE_S]) motion = vec3_sub(motion, forward);
        if (key_state[SDL_SCANCODE_D]) motion = vec3_add(motion, right);
        if (key_state[SDL_SCANCODE_SPACE]) motion = vec3_add(motion, up);

        motion = vec3_normalize(motion);
        motion = vec3_scale(motion, dt * ctrl->move_speed);
        trans->position = vec3_add(trans->position, motion);
    }
}

SDL_AppResult render_system(AppState* state) {
    SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(state->device);
    SDL_GPUTexture* swapchain;
    if (!SDL_WaitAndAcquireGPUSwapchainTexture(cmd, state->window, &swapchain, &state->width, &state->height)) {
        SDL_Log("Failed to get swapchain texture: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    if (swapchain == NULL) {
        SDL_Log("Failed to get swapchain texture: %s", SDL_GetError());
        SDL_SubmitGPUCommandBuffer(cmd);
        return SDL_APP_FAILURE;
    }

    if (state->dwidth != state->width || state->dheight != state->height) {
        // Recreate depth texture (always needed)
        if (state->depth_texture) SDL_ReleaseGPUTexture(state->device, state->depth_texture);
        SDL_GPUTextureCreateInfo depth_info = {
            .type = SDL_GPU_TEXTURETYPE_2D,
            .format = SDL_GPU_TEXTUREFORMAT_D24_UNORM,
            .width = state->width,
            .height = state->height,
            .layer_count_or_depth = 1,
            .num_levels = 1,
            .usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET
        };
        state->depth_texture = SDL_CreateGPUTexture(state->device, &depth_info);
        if (!state->depth_texture) {
            SDL_Log("Failed to recreate depth texture: %s", SDL_GetError());
            SDL_SubmitGPUCommandBuffer(cmd);
            return SDL_APP_FAILURE;
        }

        // Recreate scene texture only if post enabled
        if (state->enable_post) {
            if (state->scene_texture) SDL_ReleaseGPUTexture(state->device, state->scene_texture);
            SDL_GPUTextureCreateInfo scene_info = {
                .type = SDL_GPU_TEXTURETYPE_2D,
                .format = state->swapchain_format,
                .width = state->width,
                .height = state->height,
                .layer_count_or_depth = 1,
                .num_levels = 1,
                .usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER
            };
            state->scene_texture = SDL_CreateGPUTexture(state->device, &scene_info);
            if (!state->scene_texture) {
                SDL_Log("Failed to recreate scene texture; disabling post-processing: %s", SDL_GetError());
                state->enable_post = false;
            }
        }

        state->dwidth = state->width;
        state->dheight = state->height;
    }

    Entity cam = state->camera_entity;
    TransformComponent* cam_trans = get_transform(cam);
    CameraComponent* cam_comp = get_camera(cam);
    if (!cam_trans || !cam_comp) {
        SDL_Log("No active camera entity");
        SDL_SubmitGPUCommandBuffer(cmd);
        return SDL_APP_CONTINUE;
    }

    mat4 view;
    mat4_identity(view);
    vec4 conj_rot = quat_conjugate(cam_trans->rotation);
    mat4_rotate_quat(view, conj_rot);
    mat4_translate(view, vec3_scale(cam_trans->position, -1.0f));

    mat4 proj;
    float aspect = (float)state->width / (float)state->height;
    mat4_perspective(proj, cam_comp->fov * (float)M_PI / 180.0f, aspect, cam_comp->near_clip, cam_comp->far_clip);

    // Prepare lights (shared for both paths)
    int ambient_idx = 0;
    vec4 ambient_colors[MAX_LIGHTS] = {0};
    for (uint32_t i = 0; i < ambient_light_pool.count; i++) {
        if (ambient_idx >= MAX_LIGHTS) break;
        AmbientLightComponent light = ((AmbientLightComponent*)ambient_light_pool.data)[i];
        if (light.w <= 0.0f) continue;
        ambient_colors[ambient_idx++] = light;
    }

    int point_idx = 0;
    vec4 light_positions[MAX_LIGHTS] = {0};
    vec4 light_colors[MAX_LIGHTS] = {0};
    for (uint32_t i = 0; i < point_light_pool.count; i++) {
        if (point_idx >= MAX_LIGHTS) break;
        Entity e = point_light_pool.index_to_entity[i];
        PointLightComponent light = ((PointLightComponent*)point_light_pool.data)[i];
        if (light.w <= 0.0f) continue;
        TransformComponent* trans = get_transform(e);
        if (!trans) continue;
        light_positions[point_idx] = (vec4){trans->position.x, trans->position.y, trans->position.z, 0.0f};
        light_colors[point_idx] = light;
        point_idx++;
    }

    // Viewport (shared)
    SDL_GPUViewport viewport = {0.0f, 0.0f, (float)state->width, (float)state->height, 0.0f, 1.0f};

    // Depth target (shared)
    SDL_GPUDepthStencilTargetInfo depth_target_info = {
        .texture = state->depth_texture,
        .load_op = SDL_GPU_LOADOP_CLEAR,
        .store_op = SDL_GPU_STOREOP_STORE,
        .cycle = false,
        .clear_depth = 1.0f
    };

    SDL_GPURenderPass* scene_pass = NULL;
    if (state->enable_post) {
        // Render scene to off-screen texture
        SDL_GPUColorTargetInfo scene_color_info = {
            .texture = state->scene_texture,
            .clear_color = {0.0f, 0.0f, 0.0f, 1.0f},
            .load_op = SDL_GPU_LOADOP_CLEAR,
            .store_op = SDL_GPU_STOREOP_STORE
        };
        scene_pass = SDL_BeginGPURenderPass(cmd, &scene_color_info, 1, &depth_target_info);
    } else {
        // Render scene directly to swapchain
        SDL_GPUColorTargetInfo swap_color_info = {
            .texture = swapchain,
            .clear_color = {0.0f, 0.0f, 0.0f, 1.0f},
            .load_op = SDL_GPU_LOADOP_CLEAR,
            .store_op = SDL_GPU_STOREOP_STORE
        };
        scene_pass = SDL_BeginGPURenderPass(cmd, &swap_color_info, 1, &depth_target_info);
    }
    if (!scene_pass) {
        SDL_Log("Failed to begin scene render pass: %s", SDL_GetError());
        SDL_SubmitGPUCommandBuffer(cmd);
        return SDL_APP_FAILURE;
    }
    SDL_SetGPUViewport(scene_pass, &viewport);

    // Render entities (same for both paths)
    for (uint32_t i = 0; i < mesh_pool.count; i++) {
        Entity e = mesh_pool.index_to_entity[i];
        MeshComponent* mesh = &((MeshComponent*)mesh_pool.data)[i];
        if (!mesh->vertex_buffer) continue;
        MaterialComponent* mat = get_material(e);
        TransformComponent* trans = get_transform(e);
        if (!mat || !mat->pipeline || !trans) continue;

        mat4 model;
        mat4_identity(model);
        if (has_billboard(e)) {
            mat4_translate(model, trans->position);
            mat4_rotate_quat(model, cam_trans->rotation);
            mat4_rotate_y(model, (float)M_PI);
            mat4_scale(model, trans->scale);
        } else {
            mat4_translate(model, trans->position);
            mat4_rotate_quat(model, trans->rotation);
            mat4_scale(model, trans->scale);
        }

        UBOData ubo = {0};
        memcpy(ubo.model, model, sizeof(mat4));
        memcpy(ubo.view, view, sizeof(mat4));
        memcpy(ubo.proj, proj, sizeof(mat4));
        memcpy(ubo.point_light_pos, light_positions, point_idx * sizeof(vec4));
        memcpy(ubo.point_light_color, light_colors, point_idx * sizeof(vec4));
        memcpy(ubo.ambient_color, ambient_colors, ambient_idx * sizeof(vec4));

        ubo.color = (vec4){mat->color.x, mat->color.y, mat->color.z, 1.0f};
        ubo.camera_pos = (vec4){cam_trans->position.x, cam_trans->position.y, cam_trans->position.z, 0.0f};

        SDL_BindGPUGraphicsPipeline(scene_pass, mat->pipeline);
        SDL_PushGPUVertexUniformData(cmd, 0, &ubo, sizeof(UBOData));

        SDL_GPUTextureSamplerBinding tex_bind = {
            .texture = mat->texture ? mat->texture : state->white_texture,
            .sampler = state->sampler
        };
        SDL_BindGPUFragmentSamplers(scene_pass, 0, &tex_bind, 1);

        SDL_GPUBufferBinding vbo_binding = { .buffer = mesh->vertex_buffer, .offset = 0 };
        SDL_BindGPUVertexBuffers(scene_pass, 0, &vbo_binding, 1);

        if (mesh->index_buffer) {
            SDL_GPUBufferBinding ibo_binding = { .buffer = mesh->index_buffer, .offset = 0 };
            SDL_BindGPUIndexBuffer(scene_pass, &ibo_binding, mesh->index_size);
            SDL_DrawGPUIndexedPrimitives(scene_pass, mesh->num_indices, 1, 0, 0, 0);
        } else {
            SDL_DrawGPUPrimitives(scene_pass, mesh->num_vertices, 1, 0, 0);
        }
    }

    SDL_EndGPURenderPass(scene_pass);

    if (state->enable_post) {
        // Post-processing pass: Apply Sobel to swapchain
        SDL_GPUColorTargetInfo swap_color_info = {
            .texture = swapchain,
            .clear_color = {0.0f, 0.0f, 0.0f, 1.0f},
            .load_op = SDL_GPU_LOADOP_CLEAR,
            .store_op = SDL_GPU_STOREOP_STORE
        };

        SDL_GPURenderPass* post_pass = SDL_BeginGPURenderPass(cmd, &swap_color_info, 1, NULL);  // No depth
        if (!post_pass) {
            SDL_Log("Failed to begin post render pass: %s", SDL_GetError());
            SDL_SubmitGPUCommandBuffer(cmd);
            return SDL_APP_FAILURE;
        }
        SDL_SetGPUViewport(post_pass, &viewport);

        SDL_BindGPUGraphicsPipeline(post_pass, state->post_pipeline);

        // Push uniform (texel size for sampling offsets)
        typedef struct {
            float texelSize[2];
        } PostUBO;
        PostUBO pubo = { {1.0f / (float)state->width, 1.0f / (float)state->height} };
        SDL_PushGPUFragmentUniformData(cmd, 0, &pubo, sizeof(PostUBO));

        // Bind scene texture as sampler
        SDL_GPUTextureSamplerBinding post_tex_bind = {
            .texture = state->scene_texture,
            .sampler = state->post_sampler
        };
        SDL_BindGPUFragmentSamplers(post_pass, 0, &post_tex_bind, 1);

        // Bind quad buffers and draw
        SDL_GPUBufferBinding vbo_binding = { .buffer = state->post_vbo, .offset = 0 };
        SDL_BindGPUVertexBuffers(post_pass, 0, &vbo_binding, 1);

        SDL_GPUBufferBinding ibo_binding = { .buffer = state->post_ibo, .offset = 0 };
        SDL_BindGPUIndexBuffer(post_pass, &ibo_binding, SDL_GPU_INDEXELEMENTSIZE_16BIT);

        SDL_DrawGPUIndexedPrimitives(post_pass, 6, 1, 0, 0, 0);

        SDL_EndGPURenderPass(post_pass);
    }

    SDL_SubmitGPUCommandBuffer(cmd);
    return SDL_APP_CONTINUE;
}

void init_appstate(AppState* state) {
    // Assume state->device, state->swapchain_format, state->width, state->height, state->sampler are already set

    state->scene_texture = NULL;  // Default to NULL
    state->post_vbo = NULL;
    state->post_ibo = NULL;
    state->post_vert_shader = NULL;
    state->post_frag_shader = NULL;
    state->post_pipeline = NULL;

    if (!state->enable_post) return;

    // Create scene texture (initial)
    SDL_GPUTextureCreateInfo scene_info = {
        .type = SDL_GPU_TEXTURETYPE_2D,
        .format = state->swapchain_format,
        .width = state->width,
        .height = state->height,
        .layer_count_or_depth = 1,
        .num_levels = 1,
        .usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER
    };
    state->scene_texture = SDL_CreateGPUTexture(state->device, &scene_info);
    if (!state->scene_texture) {
        SDL_Log("Failed to create scene texture: %s", SDL_GetError());
        state->enable_post = false;  // Disable if creation fails
        return;
    }

    // Create full-screen quad (clip space positions, dummy normals, UVs)
    float post_vertices[4 * 8] = {
        -1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f,  // Bottom-left: uv 0,1
        1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,  // Bottom-right: uv 1,1
        1.0f,  1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f,  // Top-right: uv 1,0
        -1.0f,  1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f   // Top-left: uv 0,0
    };
    uint16_t post_indices[6] = {0, 1, 2, 2, 3, 0};

    upload_vertices(state->device, post_vertices, sizeof(post_vertices), &state->post_vbo);
    upload_indices(state->device, post_indices, sizeof(post_indices), &state->post_ibo);

    // Load post shaders (use your renamed files)
    state->post_vert_shader = load_shader(state->device, "shaders/post.vert.spv", SDL_GPU_SHADERSTAGE_VERTEX, 0, 0, 0, 0);
    state->post_frag_shader = load_shader(state->device, "shaders/post_sobel.frag.spv", SDL_GPU_SHADERSTAGE_FRAGMENT, 1, 1, 0, 0);  // 1 sampler, 1 uniform buffer

    if (!state->post_vert_shader || !state->post_frag_shader) {
        SDL_Log("Failed to load post shaders; disabling post-processing");
        state->enable_post = false;
        return;
    }

    // Build post pipeline (no depth, no cull, etc.)
    SDL_GPUGraphicsPipelineCreateInfo post_pipe_info = {
        .target_info = {
            .num_color_targets = 1,
            .color_target_descriptions = (SDL_GPUColorTargetDescription[]){{.format = state->swapchain_format}},
            .has_depth_stencil_target = false
        },
        .primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
        .vertex_shader = state->post_vert_shader,
        .fragment_shader = state->post_frag_shader,
        .vertex_input_state = {
            .num_vertex_buffers = 1,
            .vertex_buffer_descriptions = (SDL_GPUVertexBufferDescription[]){{
                .slot = 0,
                .pitch = 8 * sizeof(float),
                .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
                .instance_step_rate = 0
            }},
            .num_vertex_attributes = 3,
            .vertex_attributes = (SDL_GPUVertexAttribute[]){
                {.location = 0, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, .offset = 0},  // pos
                {.location = 1, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, .offset = 3 * sizeof(float)},  // norm (unused)
                {.location = 2, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, .offset = 6 * sizeof(float)}   // uv
            }
        },
        .rasterizer_state = {.fill_mode = SDL_GPU_FILLMODE_FILL, .cull_mode = SDL_GPU_CULLMODE_NONE, .front_face = SDL_GPU_FRONTFACE_CLOCKWISE},
        .depth_stencil_state = {.enable_depth_test = false, .enable_depth_write = false, .enable_stencil_test = false}
    };
    state->post_pipeline = SDL_CreateGPUGraphicsPipeline(state->device, &post_pipe_info);
    if (!state->post_pipeline) {
        SDL_Log("Failed to create post pipeline; disabling post-processing: %s", SDL_GetError());
        state->enable_post = false;
    }

    SDL_GPUSamplerCreateInfo post_sampler_info = {
        .min_filter = SDL_GPU_FILTER_LINEAR,
        .mag_filter = SDL_GPU_FILTER_LINEAR,
        .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR,
        .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        .max_anisotropy = 1.0f,
        .enable_anisotropy = false
    };
    state->post_sampler = SDL_CreateGPUSampler(state->device, &post_sampler_info);
    if (!state->post_sampler) {
        SDL_Log("Failed to create post sampler: %s", SDL_GetError());
        state->enable_post = false;
        return;
    }
}

void free_pools(AppState* state) {    if (state->scene_texture) SDL_ReleaseGPUTexture(state->device, state->scene_texture);
    if (state->scene_texture) SDL_ReleaseGPUTexture(state->device, state->scene_texture);
    if (state->post_vbo) SDL_ReleaseGPUBuffer(state->device, state->post_vbo);
    if (state->post_ibo) SDL_ReleaseGPUBuffer(state->device, state->post_ibo);
    if (state->post_vert_shader) SDL_ReleaseGPUShader(state->device, state->post_vert_shader);
    if (state->post_frag_shader) SDL_ReleaseGPUShader(state->device, state->post_frag_shader);
    if (state->post_pipeline) SDL_ReleaseGPUGraphicsPipeline(state->device, state->post_pipeline);
    if (state->post_sampler) SDL_ReleaseGPUSampler(state->device, state->post_sampler);

    // Destroy all entities to release resources (e.g., GPU buffers)
    for (uint32_t i = 0; i < next_entity_id; i++) {
        destroy_entity(state, i);
    }

    // Free pool allocations
    free(transform_pool.data);
    free(transform_pool.entity_to_index);
    free(transform_pool.index_to_entity);

    free(mesh_pool.data);
    free(mesh_pool.entity_to_index);
    free(mesh_pool.index_to_entity);

    free(material_pool.data);
    free(material_pool.entity_to_index);
    free(material_pool.index_to_entity);

    free(camera_pool.data);
    free(camera_pool.entity_to_index);
    free(camera_pool.index_to_entity);

    free(fps_controller_pool.data);
    free(fps_controller_pool.entity_to_index);
    free(fps_controller_pool.index_to_entity);

    free(billboard_pool.data); // Likely NULL, but safe
    free(billboard_pool.entity_to_index);
    free(billboard_pool.index_to_entity);

    free(ambient_light_pool.data);
    free(ambient_light_pool.entity_to_index);
    free(ambient_light_pool.index_to_entity);

    free(point_light_pool.data);
    free(point_light_pool.entity_to_index);
    free(point_light_pool.index_to_entity);
}