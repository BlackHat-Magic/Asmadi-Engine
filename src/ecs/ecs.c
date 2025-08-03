#include <stdlib.h>
#include <string.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>

#include "ecs/ecs.h"
#include <math.h>
#include "math/matrix.h"

TransformComponent transforms[MAX_ENTITIES];
MeshComponent* meshes[MAX_ENTITIES] = {NULL};
MaterialComponent materials[MAX_ENTITIES];
CameraComponent cameras[MAX_ENTITIES];
FpsCameraControllerComponent fps_controllers[MAX_ENTITIES] = {0};
BillboardComponent billboards[MAX_ENTITIES] = {0};
uint8_t entity_active[MAX_ENTITIES] = {0};

Entity create_entity() {
    for (Entity e = 0; e < MAX_ENTITIES; e++) {
        if (!entity_active[e]) {
            entity_active[e] = 1;
            return e;
        }
    }
    return (Entity)-1;  // max entities reached
}

void destroy_entity(AppState* state, Entity e) {
    if (e >= MAX_ENTITIES || !entity_active[e]) return;
    entity_active[e] = 0;

    if (meshes[e]->vertex_buffer)
        SDL_ReleaseGPUBuffer(state->device, meshes[e]->vertex_buffer);
    if (meshes[e]->index_buffer)
        SDL_ReleaseGPUBuffer(state->device, meshes[e]->index_buffer);
    if (materials[e].texture)
        SDL_ReleaseGPUTexture(state->device, materials[e].texture);

    // cleanup resources
    memset(&transforms[e], 0, sizeof(TransformComponent));
    if (meshes[e]) free(meshes[e]);
    memset(&materials[e], 0, sizeof(MaterialComponent));
    memset (&cameras[e], 0, sizeof (CameraComponent));
    billboards[e] = 0;

    if (materials[e].pipeline)
        SDL_ReleaseGPUGraphicsPipeline(state->device, materials[e].pipeline);
    if (materials[e].vertex_shader)
        SDL_ReleaseGPUShader(state->device, materials[e].vertex_shader);
    if (materials[e].fragment_shader)
        SDL_ReleaseGPUShader(state->device, materials[e].fragment_shader);
}

void add_transform(Entity e, vec3 pos, vec3 rot, vec3 scale) {
    if (e >= MAX_ENTITIES || !entity_active[e]) return;
    transforms[e].position = pos;
    transforms[e].rotation = quat_from_euler(rot);
    transforms[e].scale = scale;
}

void add_mesh(Entity e, MeshComponent* mesh) {
    if (e >= MAX_ENTITIES || !entity_active[e]) return;
    meshes[e] = mesh;
}

void add_material(Entity e, MaterialComponent material) {
    if (e >= MAX_ENTITIES || !entity_active[e]) return;
    materials[e] = material;
}

void add_camera(Entity e, float fov, float near_clip, float far_clip) {
    if (e >= MAX_ENTITIES || !entity_active[e]) return;
    cameras[e].fov = fov;
    cameras[e].near_clip = near_clip;
    cameras[e].far_clip = far_clip;
}

void add_fps_controller (Entity e, float sense, float speed) {
    if (e >= MAX_ENTITIES || !entity_active[e]) return;
    fps_controllers[e].mouse_sense = sense;
    fps_controllers[e].move_speed = speed;
}

void add_billboard(Entity e) {
    if (e >= MAX_ENTITIES || !entity_active[e]) return;
    billboards[e] = 1;
}

void fps_controller_event_system(AppState* state, SDL_Event* event) {
    for (Entity e = 0; e < MAX_ENTITIES; e++) {
        if (!entity_active[e] || fps_controllers[e].mouse_sense == 0.0f) continue;

        TransformComponent* trans = &transforms[e];

        switch (event->type) {
            case SDL_EVENT_MOUSE_MOTION: {
                float delta_yaw = -event->motion.xrel * fps_controllers[e].mouse_sense;
                float delta_pitch = event->motion.yrel * fps_controllers[e].mouse_sense;

                // Global yaw around world up
                vec4 dq_yaw = quat_from_axis_angle((vec3){0.0f, 1.0f, 0.0f}, delta_yaw);
                trans->rotation = quat_multiply(dq_yaw, trans->rotation);

                // Local pitch around right axis
                vec3 forward = vec3_rotate(trans->rotation, (vec3){0.0f, 0.0f, -1.0f});
                vec3 right = vec3_normalize(vec3_cross(forward, (vec3){0.0f, 1.0f, 0.0f}));
                vec4 dq_pitch = quat_from_axis_angle(right, delta_pitch);
                trans->rotation = quat_multiply(dq_pitch, trans->rotation);

                // Normalize
                trans->rotation = quat_normalize(trans->rotation);

                // Clamp pitch
                forward = vec3_rotate(trans->rotation, (vec3){0.0f, 0.0f, -1.0f});
                float curr_pitch = asinf(forward.y);
                if (curr_pitch > (float)M_PI * 0.49f || curr_pitch < -(float)M_PI * 0.49f) {
                    float clamped_pitch = curr_pitch;
                    if (clamped_pitch > (float)M_PI * 0.49f) clamped_pitch = (float)M_PI * 0.49f;
                    if (clamped_pitch < -(float)M_PI * 0.49f) clamped_pitch = -(float)M_PI * 0.49f;
                    float curr_yaw = atan2f(forward.x, forward.z) + (float)M_PI;
                    trans->rotation = quat_from_euler((vec3){clamped_pitch, curr_yaw, 0.0f});
                }
                break;
            }
            case SDL_EVENT_KEY_DOWN:
                if (event->key.key == SDLK_ESCAPE) {
                    state->quit = true;
                }
                break;
        }
    }
}

void fps_controller_update_system(AppState* state, float dt) {
    for (Entity e = 0; e < MAX_ENTITIES; e++) {
        if (!entity_active[e] || fps_controllers[e].move_speed == 0.0f) continue;

        TransformComponent* trans = &transforms[e];

        // Directions (note: up uses {0.0f, -1.0f, 0.0f} as in original code; fix if needed)
        vec3 forward = vec3_rotate(trans->rotation, (vec3){0.0f, 0.0f, -1.0f});
        vec3 right = vec3_rotate(trans->rotation, (vec3){1.0f, 0.0f, 0.0f});
        vec3 up = vec3_rotate(trans->rotation, (vec3){0.0f, -1.0f, 0.0f});

        int numkeys;
        const bool* key_state = SDL_GetKeyboardState(&numkeys);
        vec3 motion = {0.0f, 0.0f, 0.0f};
        if (key_state[SDL_SCANCODE_W]) motion = vec3_add(motion, forward);
        if (key_state[SDL_SCANCODE_A]) motion = vec3_sub(motion, right);
        if (key_state[SDL_SCANCODE_S]) motion = vec3_sub(motion, forward);
        if (key_state[SDL_SCANCODE_D]) motion = vec3_add(motion, right);
        if (key_state[SDL_SCANCODE_SPACE]) motion = vec3_add(motion, up);

        motion = vec3_normalize(motion);
        motion = vec3_scale(motion, dt * fps_controllers[e].move_speed);
        trans->position = vec3_add(trans->position, motion);
    }
}

SDL_AppResult render_system(AppState* state) {
    // acquire command buffer, swapchain
    SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(state->device);
    SDL_GPUTexture* swapchain;
    if (!SDL_WaitAndAcquireGPUSwapchainTexture(
            cmd, state->window, &swapchain, &state->width, &state->height
        )) {
        SDL_Log("Failed to get swapchain texture: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    if (swapchain == NULL) {
        SDL_Log("Failed to get swapchain texture: %s", SDL_GetError());
        SDL_SubmitGPUCommandBuffer(cmd);
        return SDL_APP_FAILURE;
    }

    // resize check
    if (state->dwidth != state->width || state->dheight != state->height) {
        // Release old depth texture
        if (state->depth_texture) {
            SDL_ReleaseGPUTexture(state->device, state->depth_texture);
        }

        // Recreate depth texture
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
        if (state->depth_texture == NULL) {
            SDL_Log("Failed to recreate depth texture: %s", SDL_GetError());
            SDL_SubmitGPUCommandBuffer(cmd);
            return SDL_APP_FAILURE;
        }

        // Update tracked dimensions
        state->dwidth = state->width;
        state->dheight = state->height;
    }

    Entity cam = state->camera_entity;
    if (cam == (Entity)-1 || !entity_active[cam]) {
        SDL_Log ("No active camera entity");
        SDL_SubmitGPUCommandBuffer (cmd);
        return SDL_APP_CONTINUE;
    }
    TransformComponent* cam_trans = &transforms[cam];
    CameraComponent* cam_comp = &cameras[cam];

    // I don't think we can cache this one...
    // so recomputing is probably fine, I GUESS...
    mat4 view;
    mat4_identity(view);
    vec4 conj_rot = quat_conjugate(cam_trans->rotation);
    mat4_rotate_quat(view, conj_rot);
    mat4_translate(view, vec3_scale(cam_trans->position, -1.0f));

    // TODO: cache projection matrix between frames
    // it only changes on window resize but gets recalculated every frame
    mat4 proj;
    float aspect = (float)state->width / (float)state->height;
    mat4_perspective(proj, cam_comp->fov * (float)M_PI / 180.0f, aspect, cam_comp->near_clip, cam_comp->far_clip);

    // color target info
    SDL_GPUColorTargetInfo color_target_info = {
        .texture     = swapchain,
        .clear_color = {0.0f, 0.0f, 0.0f, 1.0f},
        .load_op     = SDL_GPU_LOADOP_CLEAR,
        .store_op    = SDL_GPU_STOREOP_STORE
    };

    // depth target info
    SDL_GPUDepthStencilTargetInfo depth_target_info = {
        .texture     = state->depth_texture,
        .load_op     = SDL_GPU_LOADOP_CLEAR,
        .store_op    = SDL_GPU_STOREOP_STORE,
        .cycle       = false,
        .clear_depth = 1.0f
    };

    // begin render pass
    SDL_GPURenderPass* pass =
        SDL_BeginGPURenderPass(cmd, &color_target_info, 1, &depth_target_info);
    SDL_GPUViewport viewport = {
        .x = 0.0f,
        .y = 0.0f,
        .w = (float) state->width,
        .h = (float) state->height,
        .min_depth = 0.0f,
        .max_depth = 1.0f
    };
    SDL_SetGPUViewport(pass, &viewport);

    for (Entity e = 0; e < MAX_ENTITIES; e++) {
        if (!entity_active[e] || !meshes[e] || !meshes[e]->vertex_buffer)
            continue;  // skip if no mesh/inactive

        mat4 model;
        mat4_identity(model);
        TransformComponent* trans = &transforms[e];
        TransformComponent* cam_trans = &transforms[cam];
        if (billboards[e]) {
            mat4_translate(model, trans->position);
            mat4_rotate_quat(model, cam_trans->rotation);
            mat4_scale(model, trans->scale);
        } else {
            mat4_translate(model, transforms[e].position);
            mat4_rotate_quat(model, transforms[e].rotation);
            mat4_scale(model, transforms[e].scale);
        }

        UBOData ubo = {0};
        memcpy(ubo.model, model, sizeof(mat4));
        memcpy(ubo.view, view, sizeof(mat4));
        memcpy(ubo.proj, proj, sizeof(mat4));
        // Colors: use material.color (shader uses colors[3])
        for (int i = 0; i < 3; i++) {
            ubo.colors[i][0] = materials[e].color.x;
            ubo.colors[i][1] = materials[e].color.y;
            ubo.colors[i][2] = materials[e].color.z;
            ubo.colors[i][3] = 0.0f; // padding
        }

        if (materials[e].pipeline) {
            SDL_BindGPUGraphicsPipeline(pass, materials[e].pipeline);
        } else {
            continue;
        }

        SDL_PushGPUVertexUniformData(cmd, 0, &ubo, sizeof(UBOData));

        SDL_GPUTextureSamplerBinding tex_bind = {
            .texture =
                materials[e].texture ? materials[e].texture : state->texture,
            .sampler = state->sampler
        };
        SDL_BindGPUFragmentSamplers(pass, 0, &tex_bind, 1);

        SDL_GPUBufferBinding vbo_binding = {
            .buffer = meshes[e]->vertex_buffer, .offset = 0
        };
        SDL_BindGPUVertexBuffers(pass, 0, &vbo_binding, 1);

        if (meshes[e]->index_buffer) {
            SDL_GPUBufferBinding ibo_binding = {
                .buffer = meshes[e]->index_buffer, .offset = 0
            };
            SDL_BindGPUIndexBuffer(pass, &ibo_binding, meshes[e]->index_size);
            SDL_DrawGPUIndexedPrimitives(
                pass, meshes[e]->num_indices, 1, 0, 0, 0
            );
        } else {
            SDL_DrawGPUPrimitives(pass, meshes[e]->num_vertices, 1, 0, 0);
        }
    }

    SDL_EndGPURenderPass(pass);
    SDL_SubmitGPUCommandBuffer(cmd);

    return SDL_APP_CONTINUE;
}