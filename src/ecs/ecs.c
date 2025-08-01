#include "ecs/ecs.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>
#include <string.h>

TransformComponent transforms[MAX_ENTITIES];
MeshComponent meshes[MAX_ENTITIES];
MaterialComponent materials[MAX_ENTITIES];
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

    // TODO: is NULL okay here?
    if (meshes[e].vertex_buffer)
        SDL_ReleaseGPUBuffer(state->device, meshes[e].vertex_buffer);
    if (meshes[e].index_buffer)
        SDL_ReleaseGPUBuffer(state->device, meshes[e].index_buffer);
    if (materials[e].texture)
        SDL_ReleaseGPUTexture(state->device, materials[e].texture);

    // cleanup resources
    memset(&transforms[e], 0, sizeof(TransformComponent));
    memset(&meshes[e], 0, sizeof(MeshComponent));
    memset(&materials[e], 0, sizeof(MaterialComponent));
}

void add_transform(Entity e, vec3 pos, vec3 rot, vec3 scale) {
    if (e >= MAX_ENTITIES || !entity_active[e]) return;
    mat4_identity(transforms[e].model);
    mat4_translate(transforms[e].model, pos);
    mat4_rotate_x(transforms[e].model, rot.x);
    mat4_rotate_y(transforms[e].model, rot.y);
    mat4_rotate_z(transforms[e].model, rot.z);
    mat4_scale(transforms[e].model, scale);
}

void add_mesh(Entity e, MeshComponent mesh) {
    if (e >= MAX_ENTITIES || !entity_active[e]) return;
    meshes[e] = mesh;
}

void add_material(Entity e, vec3 color, SDL_GPUTexture* texture) {
    if (e >= MAX_ENTITIES || !entity_active[e]) return;
    materials[e].color   = color;
    materials[e].texture = texture;
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
    SDL_BindGPUGraphicsPipeline(pass, state->triangle_pipeline);

    for (Entity e = 0; e < MAX_ENTITIES; e++) {
        if (!entity_active[e] || !meshes[e].vertex_buffer)
            continue;  // skip if no mesh/inactive

        UBOData ubo = {0};
        memcpy(ubo.model, transforms[e].model, sizeof(mat4));
        memcpy(ubo.view, *state->view_matrix, sizeof(mat4));
        memcpy(ubo.proj, *state->proj_matrix, sizeof(mat4));
        // Colors: use material.color (shader uses colors[3])
        for (int i = 0; i < 3; i++)
            memcpy((void*)&ubo.colors[i], &materials[e].color, sizeof(vec3));

        SDL_PushGPUVertexUniformData(cmd, 0, &ubo, sizeof(UBOData));

        SDL_GPUTextureSamplerBinding tex_bind = {
            .texture =
                materials[e].texture ? materials[e].texture : state->texture,
            .sampler = state->sampler
        };
        SDL_BindGPUFragmentSamplers(pass, 0, &tex_bind, 1);

        SDL_GPUBufferBinding vbo_binding = {
            .buffer = meshes[e].vertex_buffer, .offset = 0
        };
        SDL_BindGPUVertexBuffers(pass, 0, &vbo_binding, 1);

        if (meshes[e].index_buffer) {
            SDL_GPUBufferBinding ibo_binding = {
                .buffer = meshes[e].index_buffer, .offset = 0
            };
            SDL_BindGPUIndexBuffer(pass, &ibo_binding, meshes[e].index_size);
            SDL_DrawGPUIndexedPrimitives(
                pass, meshes[e].num_indices, 1, 0, 0, 0
            );
        } else {
            SDL_DrawGPUPrimitives(pass, meshes[e].num_vertices, 1, 0, 0);
        }
    }

    SDL_EndGPURenderPass(pass);
    SDL_SubmitGPUCommandBuffer(cmd);

    return SDL_APP_CONTINUE;
}