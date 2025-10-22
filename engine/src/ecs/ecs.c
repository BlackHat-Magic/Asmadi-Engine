#include <math.h>
#include <stdlib.h>

#include <ecs/ecs.h>
#include <ui/ui.h>

static Uint32 next_entity_id = 0;

typedef struct {
    void* data;
    Uint32* entity_to_index;
    Uint32* index_to_entity;
    Uint32 count;
    Uint32 data_capacity;
    Uint32 entity_capacity;
} GenericPool;

static GenericPool transform_pool = {0};
static GenericPool mesh_pool = {0};
static GenericPool material_pool = {0};
static GenericPool camera_pool = {0};
static GenericPool fps_controller_pool = {0};
static GenericPool billboard_pool = {0}; // no data, just presence
static GenericPool ambient_light_pool = {0};
static GenericPool point_light_pool = {0};
static GenericPool ui_pool = {0};

// Helper to grow entity_to_index (sparse map)
static void grow_entity_map (GenericPool* pool, Uint32 min_entity) {
    Uint32 new_cap = pool->entity_capacity ? pool->entity_capacity * 2 : 1024;
    if (new_cap <= min_entity) new_cap = min_entity + 1;
    Uint32* new_map =
        (Uint32*) realloc (pool->entity_to_index, new_cap * sizeof (Uint32));
    if (!new_map) {
        SDL_Log ("Failed to realloc entity map");
        return;
    }
    for (Uint32 i = pool->entity_capacity; i < new_cap; i++) {
        new_map[i] = ~0u;
    }
    pool->entity_to_index = new_map;
    pool->entity_capacity = new_cap;
}

// Helper to grow data and index_to_entity (dense)
static void grow_data (GenericPool* pool, Uint64 component_size) {
    Uint32 new_cap = pool->data_capacity ? pool->data_capacity * 2 : 64;
    void* new_data = realloc (pool->data, new_cap * component_size);
    Uint32* new_idx_ent =
        (Uint32*) realloc (pool->index_to_entity, new_cap * sizeof (Uint32));
    if (!new_data || !new_idx_ent) {
        SDL_Log ("Failed to realloc data pool");
        return;
    }
    pool->data = new_data;
    pool->index_to_entity = new_idx_ent;
    pool->data_capacity = new_cap;
}

// Generic has
static bool pool_has (const GenericPool* pool, Entity e) {
    return e < pool->entity_capacity && pool->entity_to_index[e] != ~0u;
}

// Generic remove (swap and pop)
static void pool_remove (GenericPool* pool, Entity e, Uint64 component_size) {
    if (!pool_has (pool, e)) return;
    Uint32 idx = pool->entity_to_index[e];
    Uint32 last = --pool->count;
    // Copy last to idx (if data exists)
    if (pool->data) {
        memcpy (
            (char*) pool->data + idx * component_size,
            (char*) pool->data + last * component_size, component_size
        );
    }
    Uint32 swapped_e = pool->index_to_entity[last];
    pool->index_to_entity[idx] = swapped_e;
    pool->entity_to_index[swapped_e] = idx;
    pool->entity_to_index[e] = ~0u;
}

// Generic add/overwrite (with data copy)
static void pool_add (
    GenericPool* pool,
    Entity e,
    const void* comp_data,
    Uint64 component_size
) {
    if (e >= pool->entity_capacity) {
        grow_entity_map (pool, e);
    }
    Uint32 idx;
    if (pool->entity_to_index[e] != ~0u) {
        // Overwrite
        idx = pool->entity_to_index[e];
        if (pool->data && comp_data) {
            memcpy (
                (char*) pool->data + idx * component_size, comp_data,
                component_size
            );
        }
        return;
    }
    // Add new
    if (pool->count == pool->data_capacity) {
        grow_data (pool, component_size);
    }
    idx = pool->count++;
    if (pool->data && comp_data) {
        memcpy (
            (char*) pool->data + idx * component_size, comp_data, component_size
        );
    }
    pool->index_to_entity[idx] = e;
    pool->entity_to_index[e] = idx;
}

// Generic get
static void*
pool_get (const GenericPool* pool, Entity e, Uint64 component_size) {
    if (!pool_has (pool, e)) return NULL;
    Uint32 idx = pool->entity_to_index[e];
    return (char*) pool->data + idx * component_size;
}

Entity create_entity (void) {
    return next_entity_id++;
}

void destroy_entity (SDL_GPUDevice* device, Entity e) {
    remove_transform (e);
    remove_mesh (device, e);
    remove_material (device, e);
    remove_camera (e);
    remove_fps_controller (e);
    remove_billboard (e);
    remove_ambient_light (e);
    remove_point_light (e);
}

// Transforms
void add_transform (Entity e, vec3 pos, vec3 rot, vec3 scale) {
    TransformComponent comp =
        {.position = pos, .rotation = quat_from_euler (rot), .scale = scale};
    pool_add (&transform_pool, e, &comp, sizeof (TransformComponent));
}
TransformComponent* get_transform (Entity e) {
    return (TransformComponent*) pool_get (
        &transform_pool, e, sizeof (TransformComponent)
    );
}
bool has_transform (Entity e) {
    return pool_has (&transform_pool, e);
}
void remove_transform (Entity e) {
    pool_remove (&transform_pool, e, sizeof (TransformComponent));
}

// Meshes
void add_mesh (Entity e, MeshComponent mesh) {
    pool_add (&mesh_pool, e, &mesh, sizeof (MeshComponent));
}
MeshComponent* get_mesh (Entity e) {
    return (MeshComponent*) pool_get (&mesh_pool, e, sizeof (MeshComponent));
}
bool has_mesh (Entity e) {
    return pool_has (&mesh_pool, e);
}
void remove_mesh (SDL_GPUDevice* device, Entity e) {
    MeshComponent* mesh = get_mesh (e);
    if (mesh) {
        if (mesh->vertex_buffer)
            SDL_ReleaseGPUBuffer (device, mesh->vertex_buffer);
        if (mesh->index_buffer)
            SDL_ReleaseGPUBuffer (device, mesh->index_buffer);
    }
    pool_remove (&mesh_pool, e, sizeof (MeshComponent));
}

// Materials
void add_material (Entity e, MaterialComponent material) {
    pool_add (&material_pool, e, &material, sizeof (MaterialComponent));
}
MaterialComponent* get_material (Entity e) {
    return (MaterialComponent*) pool_get (
        &material_pool, e, sizeof (MaterialComponent)
    );
}
bool has_material (Entity e) {
    return pool_has (&material_pool, e);
}
void remove_material (SDL_GPUDevice* device, Entity e) {
    MaterialComponent* mat = get_material (e);
    if (mat) {
        if (mat->texture) SDL_ReleaseGPUTexture (device, mat->texture);
        if (mat->pipeline)
            SDL_ReleaseGPUGraphicsPipeline (device, mat->pipeline);
        if (mat->vertex_shader)
            SDL_ReleaseGPUShader (device, mat->vertex_shader);
        if (mat->fragment_shader)
            SDL_ReleaseGPUShader (device, mat->fragment_shader);
    }
    pool_remove (&material_pool, e, sizeof (MaterialComponent));
}

// Cameras
void add_camera (Entity e, float fov, float near_clip, float far_clip) {
    CameraComponent comp =
        {.fov = fov, .near_clip = near_clip, .far_clip = far_clip};
    pool_add (&camera_pool, e, &comp, sizeof (CameraComponent));
}
CameraComponent* get_camera (Entity e) {
    return (CameraComponent*) pool_get (
        &camera_pool, e, sizeof (CameraComponent)
    );
}
bool has_camera (Entity e) {
    return pool_has (&camera_pool, e);
}
void remove_camera (Entity e) {
    pool_remove (&camera_pool, e, sizeof (CameraComponent));
}

// FPS Controllers
void add_fps_controller (Entity e, float sense, float speed) {
    FpsCameraControllerComponent comp = {
        .mouse_sense = sense,
        .move_speed = speed
    };
    pool_add (
        &fps_controller_pool, e, &comp, sizeof (FpsCameraControllerComponent)
    );
}
FpsCameraControllerComponent* get_fps_controller (Entity e) {
    return (FpsCameraControllerComponent*) pool_get (
        &fps_controller_pool, e, sizeof (FpsCameraControllerComponent)
    );
}
bool has_fps_controller (Entity e) {
    return pool_has (&fps_controller_pool, e);
}
void remove_fps_controller (Entity e) {
    pool_remove (
        &fps_controller_pool, e, sizeof (FpsCameraControllerComponent)
    );
}

// Billboards (flag, no data)
void add_billboard (Entity e) {
    pool_add (&billboard_pool, e, NULL, 0); // no data copy
}
bool has_billboard (Entity e) {
    return pool_has (&billboard_pool, e);
}
void remove_billboard (Entity e) {
    pool_remove (&billboard_pool, e, 0);
}

void add_ui (Entity e, UIComponent* ui) {
    pool_add (&ui_pool, e, ui, sizeof (UIComponent));
};
bool has_ui (Entity e) {
    return pool_has (&ui_pool, e);
};
UIComponent* get_ui (Entity e) {
    return (UIComponent*) pool_get (&ui_pool, e, sizeof (UIComponent));
}
void remove_ui (Entity e) {
    pool_remove (&ui_pool, e, sizeof (UIComponent));
};

// Ambient Lights
void add_ambient_light (Entity e, SDL_FColor color) {
    AmbientLightComponent comp = color;
    pool_add (&ambient_light_pool, e, &comp, sizeof (AmbientLightComponent));
}
AmbientLightComponent* get_ambient_light (Entity e) {
    return (AmbientLightComponent*) pool_get (
        &ambient_light_pool, e, sizeof (AmbientLightComponent)
    );
}
bool has_ambient_light (Entity e) {
    return pool_has (&ambient_light_pool, e);
}
void remove_ambient_light (Entity e) {
    pool_remove (&ambient_light_pool, e, sizeof (AmbientLightComponent));
}

// Point Lights
// TODO: bulk initialize point lights
// TODO: communicate failure to caller
// TODO: what if the transform is added after the point light?
void add_point_light (Entity e, SDL_FColor color, gpu_renderer* renderer) {
    PointLightComponent comp = color;
    pool_add (&point_light_pool, e, &comp, sizeof (PointLightComponent));

    // reconstruct point light buffer
    GPUPointLight all_lights[point_light_pool.count];
    for (int i = 0; i < point_light_pool.count; i++) {
        // create gpu light
        Entity light_entity = point_light_pool.index_to_entity[i];
        GPUPointLight gpu_light = {0};

        // copy in the color
        PointLightComponent* light = get_point_light (light_entity);
        // light = NULL;
        gpu_light.color = light ? *light : (SDL_FColor) {1.0f, 1.0f, 1.0f, 1.0f};

        // copy in the position
        TransformComponent* transform = get_transform (light_entity);
        vec4 position;
        if (transform) {
            position.x = transform->position.x;
            position.y = transform->position.y;
            position.z = transform->position.z;
            position.w = 0.0f;
        } else {
            position = (vec4) {0};
        }
        gpu_light.position = position;
        
        all_lights[i] = gpu_light;
        SDL_Log (
            "Light: x=%.1f, y=%.1f, z=%.1f, r=%.1f, g=%.1f, b=%.1f, a=%.1f",
            position.x,
            position.y,
            position.z,
            gpu_light.color.r,
            gpu_light.color.g,
            gpu_light.color.b,
            gpu_light.color.a
        );
    }

    // release existing ssbo if it's too small
    Uint32 ssbo_size = point_light_pool.count * sizeof (GPUPointLight);
    ssbo_size = ssbo_size  > 1024 ? ssbo_size : 1024;
    if (renderer->point_ssbo && renderer->point_size < ssbo_size) {
        SDL_ReleaseGPUBuffer (renderer->device, renderer->point_ssbo);
        renderer->point_ssbo = NULL;
        renderer->point_size = 0;
    }

    // if ssbo is uninitialized (or if it was released because too small), create one
    if (renderer->point_ssbo == NULL) {
        SDL_GPUBufferCreateInfo ssbo_info = {
            .size = ssbo_size,
            .usage = SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ
        };
        renderer->point_ssbo = SDL_CreateGPUBuffer (renderer->device, &ssbo_info);
        if (renderer->point_ssbo == NULL) {
            return;
        }
        renderer->point_size = ssbo_size;
    }

    // create transfer buffer
    SDL_GPUTransferBufferCreateInfo tbuf_info = {
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
        .size = ssbo_size
    };
    SDL_GPUTransferBuffer* tbuf = SDL_CreateGPUTransferBuffer (renderer->device, &tbuf_info);
    void* map = SDL_MapGPUTransferBuffer (renderer->device, tbuf, false);
    if (map == NULL) {
        SDL_ReleaseGPUTransferBuffer (renderer->device, tbuf);
        return;
    }
    memcpy (map, all_lights, sizeof (all_lights));
    GPUPointLight* mapped_light = (GPUPointLight*) map;
    SDL_Log (
        "Light: x=%.1f, y=%.1f, z=%.1f, r=%.1f, g=%.1f, b=%.1f, a=%.1f",
        mapped_light->position.x,
        mapped_light->position.y,
        mapped_light->position.z,
        mapped_light->color.r,
        mapped_light->color.g,
        mapped_light->color.b,
        mapped_light->color.a
    );
    SDL_UnmapGPUTransferBuffer (renderer->device, tbuf);

    // upload data
    SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(renderer->device);
    SDL_GPUCopyPass* pass = SDL_BeginGPUCopyPass(cmd);
    SDL_GPUTransferBufferLocation tbuf_loc = {
        .transfer_buffer = tbuf,
        .offset = 0,
    };
    SDL_GPUBufferRegion tbuf_region = {
        .buffer = renderer->point_ssbo,
        .offset = 0,
        .size = sizeof (all_lights),
    };
    SDL_UploadToGPUBuffer (pass, &tbuf_loc, &tbuf_region, false);
    SDL_EndGPUCopyPass (pass);
    SDL_SubmitGPUCommandBuffer (cmd);
    SDL_ReleaseGPUTransferBuffer(renderer->device, tbuf);
}
PointLightComponent* get_point_light (Entity e) {
    return (PointLightComponent*) pool_get (
        &point_light_pool, e, sizeof (PointLightComponent)
    );
}
bool has_point_light (Entity e) {
    return pool_has (&point_light_pool, e);
}
void remove_point_light (Entity e) {
    pool_remove (&point_light_pool, e, sizeof (PointLightComponent));
}

// TODO: more robust???
gpu_renderer* renderer_init (SDL_GPUDevice* device, SDL_Window* window, const Uint32 width, const Uint32 height) {
    // create renderer
    gpu_renderer* renderer = calloc (1, sizeof (gpu_renderer));
    if (renderer == NULL) {
        SDL_Log ("Failed to allocate GPU renderer.");
        return NULL;
    }
    renderer->device = device;
    renderer->window = window;
    renderer->width = width;
    renderer->dwidth = width;
    renderer->height = height;
    renderer->dheight = height;

    // depth texture
    SDL_GPUTextureCreateInfo depth_info = {
        .type = SDL_GPU_TEXTURETYPE_2D,
        .format = SDL_GPU_TEXTUREFORMAT_D24_UNORM,
        .width = width,
        .height = height,
        .layer_count_or_depth = 1,
        .num_levels = 1,
        .usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET
    };
    renderer->depth_texture = SDL_CreateGPUTexture (device, &depth_info);
    if (renderer->depth_texture == NULL) {
        free (renderer);
        SDL_Log ("Failed to create renderer depth texture: %s", SDL_GetError ());
        return NULL;
    }

    // texture format
    renderer->format = SDL_GetGPUSwapchainTextureFormat (device, window);
    if (renderer->format == SDL_GPU_TEXTUREFORMAT_INVALID) {
        SDL_ReleaseGPUTexture (device, renderer->depth_texture);
        free (renderer);
        SDL_Log ("Failed to get swapchain texture format: %s", SDL_GetError ());
        return NULL;
    }

    SDL_GPUBufferCreateInfo ssbo_info = {
        .size = 1024,
        .usage = SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ
    };

    renderer->point_ssbo = SDL_CreateGPUBuffer (renderer->device, &ssbo_info);
    if (renderer->point_ssbo == NULL) {
        SDL_ReleaseGPUTexture (device, renderer->depth_texture);
        free (renderer);
        return NULL;
    }
    renderer->point_size = 1024;

    renderer->ambient_ssbo = SDL_CreateGPUBuffer (renderer->device, &ssbo_info);
    if (renderer->ambient_ssbo == NULL) {
        SDL_ReleaseGPUBuffer (device, renderer->point_ssbo);
        SDL_ReleaseGPUTexture (device, renderer->depth_texture);
        free (renderer);
        return NULL;
    }
    renderer->ambient_size = 1024;

    return renderer;
}

void fps_controller_event_system (SDL_Event* event) {
    for (Uint32 i = 0; i < fps_controller_pool.count; i++) {
        Entity e = fps_controller_pool.index_to_entity[i];
        FpsCameraControllerComponent* ctrl =
            &((FpsCameraControllerComponent*) fps_controller_pool.data)[i];
        TransformComponent* trans = get_transform (e);
        if (!trans) continue;

        switch (event->type) {
        case SDL_EVENT_MOUSE_MOTION: {
            float delta_yaw = event->motion.xrel * ctrl->mouse_sense;
            float delta_pitch = event->motion.yrel * ctrl->mouse_sense;

            vec4 dq_yaw =
                quat_from_axis_angle ((vec3) {0.0f, 1.0f, 0.0f}, delta_yaw);
            trans->rotation = quat_multiply (dq_yaw, trans->rotation);

            vec3 forward =
                vec3_rotate (trans->rotation, (vec3) {0.0f, 0.0f, -1.0f});
            vec3 right = vec3_normalize (
                vec3_cross (forward, (vec3) {0.0f, 1.0f, 0.0f})
            );
            vec4 dq_pitch = quat_from_axis_angle (right, delta_pitch);
            trans->rotation = quat_multiply (dq_pitch, trans->rotation);

            trans->rotation = quat_normalize (trans->rotation);

            forward = vec3_rotate (trans->rotation, (vec3) {0.0f, 0.0f, -1.0f});
            float curr_pitch = asinf (forward.y);
            if (curr_pitch > (float) M_PI * 0.49f ||
                curr_pitch < -(float) M_PI * 0.49f) {
                float clamped_pitch = curr_pitch > (float) M_PI * 0.49f
                                          ? (float) M_PI * 0.49f
                                          : -(float) M_PI * 0.49f;
                float curr_yaw = atan2f (forward.x, forward.z) + (float) M_PI;
                trans->rotation =
                    quat_from_euler ((vec3) {clamped_pitch, curr_yaw, 0.0f});
            }
            break;
        }
        default:
            return;
        }
    }
}

void fps_controller_update_system (float dt) {
    for (Uint32 i = 0; i < fps_controller_pool.count; i++) {
        Entity e = fps_controller_pool.index_to_entity[i];
        FpsCameraControllerComponent* ctrl =
            &((FpsCameraControllerComponent*) fps_controller_pool.data)[i];
        TransformComponent* trans = get_transform (e);
        if (!trans) continue;

        vec3 forward = vec3_rotate (trans->rotation, (vec3) {0.0f, 0.0f, 1.0f});
        vec3 right = vec3_rotate (trans->rotation, (vec3) {1.0f, 0.0f, 0.0f});
        vec3 up = vec3_rotate (trans->rotation, (vec3) {0.0f, 1.0f, 0.0f});

        int numkeys;
        const bool* key_state = SDL_GetKeyboardState (&numkeys);
        vec3 motion = {0.0f, 0.0f, 0.0f};
        if (key_state[SDL_SCANCODE_W]) motion = vec3_add (motion, forward);
        if (key_state[SDL_SCANCODE_A]) motion = vec3_sub (motion, right);
        if (key_state[SDL_SCANCODE_S]) motion = vec3_sub (motion, forward);
        if (key_state[SDL_SCANCODE_D]) motion = vec3_add (motion, right);
        if (key_state[SDL_SCANCODE_SPACE]) motion = vec3_add (motion, up);

        motion = vec3_normalize (motion);
        motion = vec3_scale (motion, dt * ctrl->move_speed);
        trans->position = vec3_add (trans->position, motion);
    }
}

SDL_AppResult render_system (
    gpu_renderer* renderer,
    Entity cam,
    Uint64* prerender,
    Uint64* preui,
    Uint64* postrender
) {
    SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer (renderer->device);
    SDL_GPUTexture* swapchain;
    if (!SDL_WaitAndAcquireGPUSwapchainTexture (
            cmd, renderer->window, &swapchain, &renderer->width,
            &renderer->height
        )) {
        SDL_Log ("Failed to get swapchain texture: %s", SDL_GetError ());
        return SDL_APP_FAILURE;
    }
    if (swapchain == NULL) {
        SDL_Log ("Failed to get swapchain texture: %s", SDL_GetError ());
        SDL_SubmitGPUCommandBuffer (cmd);
        return SDL_APP_FAILURE;
    }

    TransformComponent* cam_trans = get_transform (cam);
    CameraComponent* cam_comp = get_camera (cam);
    if (!cam_trans || !cam_comp) {
        SDL_Log ("No active camera entity");
        SDL_SubmitGPUCommandBuffer (cmd);
        return SDL_APP_CONTINUE;
    }

    SDL_GPUColorTargetInfo color_target_info = {
        .texture = swapchain,
        .clear_color = {0.0f, 0.0f, 0.0f, 1.0f},
        .load_op = SDL_GPU_LOADOP_CLEAR,
        .store_op = SDL_GPU_STOREOP_STORE
    };

    SDL_GPUDepthStencilTargetInfo depth_target_info = {
        .texture = renderer->depth_texture,
        .load_op = SDL_GPU_LOADOP_CLEAR,
        .store_op = SDL_GPU_STOREOP_STORE,
        .cycle = false,
        .clear_depth = 1.0f
    };

    SDL_GPURenderPass* pass =
        SDL_BeginGPURenderPass (cmd, &color_target_info, 1, &depth_target_info);
    SDL_GPUViewport viewport = {
        0.0f, 0.0f, (float) renderer->width, (float) renderer->height,
        0.0f, 1.0f
    };
    SDL_SetGPUViewport (pass, &viewport);

    // frame UBOs (set 0)
    mat4 view;
    mat4_identity (view);
    vec4 conj_rot = quat_conjugate (cam_trans->rotation);
    mat4_rotate_quat (view, conj_rot);
    mat4_translate (view, vec3_scale (cam_trans->position, -1.0f));

    mat4 proj;
    float aspect = (float) renderer->width / (float) renderer->height;
    mat4_perspective (
        proj, cam_comp->fov * (float) M_PI / 180.0f, aspect,
        cam_comp->near_clip, cam_comp->far_clip
    );

    Uint32 ambient_count = ambient_light_pool.count;
    Uint32 point_count = point_light_pool.count;

    // vertex stage
    //  must-have
    //  - view matrix
    //  - projection matrix
    //  sometimes
    //  - normal or inverse-transpose
    //  - time
    //  - per-instance index
    struct {
        mat4 view;
        mat4 proj;
    } vertex_ubo;
    // because C is annoying and won't let me just copy the array over smh
    memcpy (&vertex_ubo.view, &view, sizeof (view));
    memcpy (&vertex_ubo.proj, &proj, sizeof (proj));
    SDL_PushGPUVertexUniformData (cmd, 0, &vertex_ubo, sizeof (vertex_ubo));

    // fragment stage
    //  must-have
    //  - camera transform
    //  - exposure time for tonemapping
    //  often
    //  - environment parameters
    //  - light counts
    //  shadow/map transforms/handles
    vec4 cam_pos = {
        .x = cam_trans->position.x,
        .y = cam_trans->position.y,
        .z = cam_trans->position.z,
        .w = 0.0f,
    };
    struct {
        vec4 cam_pos;
        vec4 cam_rot;
        Uint32 ambient_count;
        Uint32 point_count;
        Uint32 pad0;
        Uint32 pad1;
    } fragment_ubo = {
        .cam_pos = cam_pos,
        .cam_rot = cam_trans->rotation,
        .ambient_count = ambient_count,
        .point_count = point_count,
        .pad0 = 0, .pad1 = 1,
    };
    SDL_PushGPUFragmentUniformData (cmd, 0, &fragment_ubo, sizeof (fragment_ubo));

    *prerender = SDL_GetTicksNS ();
    for (Uint32 i = 0; i < mesh_pool.count; i++) {
        Entity e = mesh_pool.index_to_entity[i];
        MeshComponent* mesh = get_mesh (e);
        if (mesh == NULL) continue;
        MaterialComponent* mat = get_material (e);
        TransformComponent* trans = get_transform (e);
        if (!mat || !mat->pipeline || !trans) continue;

        // object vertex ubo (set 2)
        // TODO: encode arbitrary number of custom per-object uniforms
        mat4 model;
        mat4_identity (model);
        if (has_billboard (e)) {
            mat4_translate (model, trans->position);
            mat4_rotate_quat (model, cam_trans->rotation);
            mat4_rotate_y (model, (float) M_PI);
            mat4_scale (model, trans->scale);
        } else {
            mat4_translate (model, trans->position);
            mat4_rotate_quat (model, trans->rotation);
            mat4_scale (model, trans->scale);
        }
        struct {
            mat4 model;
            vec4 color;
        } vertex_ubo;
        memcpy (&vertex_ubo.model, &model, sizeof (model));
        memcpy (&vertex_ubo.color, &mat->color, sizeof (SDL_FColor));
        SDL_PushGPUVertexUniformData (cmd, 1, &vertex_ubo, sizeof (vertex_ubo));

        SDL_BindGPUGraphicsPipeline (pass, mat->pipeline);

        SDL_GPUBufferBinding vbo_binding = {
            .buffer = mesh->vertex_buffer,
            .offset = 0
        };
        SDL_BindGPUVertexBuffers (pass, 0, &vbo_binding, 1);

        SDL_GPUTextureSamplerBinding tex_bind = {
            .texture = mat->texture,
            .sampler = mat->sampler
        };
        SDL_BindGPUFragmentSamplers (pass, 0, &tex_bind, 1);
        SDL_GPUBuffer* buffers[] = {renderer->ambient_ssbo, renderer->point_ssbo};
        SDL_BindGPUFragmentStorageBuffers (pass, 1, buffers, 2);

        if (mesh->index_buffer) {
            SDL_GPUBufferBinding ibo_binding = {
                .buffer = mesh->index_buffer,
                .offset = 0
            };
            SDL_BindGPUIndexBuffer (pass, &ibo_binding, mesh->index_size);
            SDL_DrawGPUIndexedPrimitives (pass, mesh->num_indices, 1, 0, 0, 0);
        } else {
            SDL_DrawGPUPrimitives (pass, mesh->num_vertices, 1, 0, 0);
        }
    }

    // draw queued texts
    *preui = SDL_GetTicksNS ();
    for (int i = 0; i < ui_pool.count; i++) {
        UIComponent* ui = &((UIComponent*) ui_pool.data)[i];

        bool scissor_enabled = false;
        mu_Command* mu_command = NULL;
        while (mu_next_command (&ui->context, &mu_command)) {
            switch (mu_command->type) {
            case MU_COMMAND_TEXT:
                draw_text (
                    ui, renderer->device, mu_command->text.str,
                    (float) mu_command->text.pos.x,
                    (float) mu_command->text.pos.y,
                    (float) mu_command->text.color.r / 255.0f,
                    (float) mu_command->text.color.g / 255.0f,
                    (float) mu_command->text.color.b / 255.0f,
                    (float) mu_command->text.color.a / 255.0f
                );
                break;
            case MU_COMMAND_RECT:
                draw_rectangle (
                    ui, (float) mu_command->rect.rect.x,
                    (float) mu_command->rect.rect.y,
                    (float) mu_command->rect.rect.w,
                    (float) mu_command->rect.rect.h,
                    (float) mu_command->rect.color.r / 255.0f,
                    (float) mu_command->rect.color.g / 255.0f,
                    (float) mu_command->rect.color.b / 255.0f,
                    (float) mu_command->rect.color.a / 255.0f
                );
                break;
            case MU_COMMAND_CLIP:
                if (mu_command->clip.rect.w <= 0 ||
                    mu_command->clip.rect.h <= 0) {
                    if (scissor_enabled) {
                        SDL_SetGPUScissor (pass, NULL);
                        scissor_enabled = false;
                    }
                } else {
                    SDL_Rect scissor = {
                        (int) mu_command->clip.rect.x,
                        (int) mu_command->clip.rect.y,
                        (int) mu_command->clip.rect.w,
                        (int) mu_command->clip.rect.h,
                    };
                    SDL_SetGPUScissor (pass, &scissor);
                    scissor_enabled = true;
                }
                break;
            default:
                break;
            }
        }
        if (scissor_enabled) SDL_SetGPUScissor (pass, NULL);

        if (ui->rect_count == 0) continue;
        SDL_BindGPUGraphicsPipeline (pass, ui->pipeline);

        for (int r = 0; r < ui->rect_count; r++) {
            UIRect* rect = &ui->rects[r];

            float rx = (float) renderer->width;
            float ry = (float) renderer->height;
            float x1 = rect->rect.x;
            float y1 = rect->rect.y;
            float x2 = rect->rect.x + rect->rect.w;
            float y2 = rect->rect.y + rect->rect.h;
            SDL_FColor col = rect->color;
            float verts[40] = {
                x1, y2, rx, ry, col.r, col.g, col.b, col.a, 0.0f, 1.0f,
                x2, y2, rx, ry, col.r, col.g, col.b, col.a, 1.0f, 1.0f,
                x1, y1, rx, ry, col.r, col.g, col.b, col.a, 0.0f, 0.0f,
                x2, y1, rx, ry, col.r, col.g, col.b, col.a, 1.0f, 0.0f,
            };
            uint32_t inds[6] = {0, 1, 2, 1, 3, 2};

            uint32_t vsize = sizeof (verts);
            uint32_t isize = sizeof (inds);

            SDL_GPUTransferBufferCreateInfo vtinfo = {
                .size = vsize,
                .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD
            };
            SDL_GPUTransferBuffer* vtbuf =
                SDL_CreateGPUTransferBuffer (renderer->device, &vtinfo);
            void* vmap =
                SDL_MapGPUTransferBuffer (renderer->device, vtbuf, false);
            memcpy (vmap, verts, vsize);
            SDL_UnmapGPUTransferBuffer (renderer->device, vtbuf);

            SDL_GPUTransferBufferCreateInfo itinfo = {
                .size = isize,
                .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD
            };
            SDL_GPUTransferBuffer* itbuf =
                SDL_CreateGPUTransferBuffer (renderer->device, &itinfo);
            void* imap =
                SDL_MapGPUTransferBuffer (renderer->device, itbuf, false);
            memcpy (imap, inds, isize);
            SDL_UnmapGPUTransferBuffer (renderer->device, itbuf);

            SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass (cmd);
            SDL_GPUTransferBufferLocation vsrc = {
                .transfer_buffer = vtbuf,
                .offset = 0
            };
            SDL_GPUBufferRegion vdst =
                {.buffer = ui->vbo, .offset = 0, .size = vsize};
            SDL_UploadToGPUBuffer (copy, &vsrc, &vdst, false);
            SDL_GPUTransferBufferLocation isrc = {
                .transfer_buffer = itbuf,
                .offset = 0
            };
            SDL_GPUBufferRegion idst =
                {.buffer = ui->ibo, .offset = 0, .size = isize};
            SDL_UploadToGPUBuffer (copy, &isrc, &idst, false);
            SDL_EndGPUCopyPass (copy);
            SDL_ReleaseGPUTransferBuffer (renderer->device, vtbuf);
            SDL_ReleaseGPUTransferBuffer (renderer->device, itbuf);

            SDL_GPUTextureSamplerBinding tex_bind = {
                .texture = rect->texture,
                .sampler = ui->sampler
            };
            SDL_BindGPUFragmentSamplers (pass, 0, &tex_bind, 1);

            SDL_GPUBufferBinding vbind = {.buffer = ui->vbo, .offset = 0};
            SDL_BindGPUVertexBuffers (pass, 0, &vbind, 1);
            SDL_GPUBufferBinding ibind = {.buffer = ui->ibo, .offset = 0};
            SDL_BindGPUIndexBuffer (
                pass, &ibind, SDL_GPU_INDEXELEMENTSIZE_32BIT
            );
            SDL_DrawGPUIndexedPrimitives (pass, 6, 1, 0, 0, 0);

            // If this was a text texture, release it now (keep white texture)
            if (rect->texture != ui->white_texture) {
                SDL_ReleaseGPUTexture (renderer->device, rect->texture);
                rect->texture = ui->white_texture;
            }
        }

        ui->rect_count = 0;
    }
    *postrender = SDL_GetTicksNS ();

    SDL_EndGPURenderPass (pass);
    SDL_SubmitGPUCommandBuffer (cmd);
    return SDL_APP_CONTINUE;
}

void free_pools (SDL_GPUDevice* device) {
    // Destroy all entities to release resources (e.g., GPU buffers)
    for (Uint32 i = 0; i < next_entity_id; i++) {
        destroy_entity (device, i);
    }

    // Free pool allocations
    free (transform_pool.data);
    free (transform_pool.entity_to_index);
    free (transform_pool.index_to_entity);

    free (mesh_pool.data);
    free (mesh_pool.entity_to_index);
    free (mesh_pool.index_to_entity);

    free (material_pool.data);
    free (material_pool.entity_to_index);
    free (material_pool.index_to_entity);

    free (camera_pool.data);
    free (camera_pool.entity_to_index);
    free (camera_pool.index_to_entity);

    free (fps_controller_pool.data);
    free (fps_controller_pool.entity_to_index);
    free (fps_controller_pool.index_to_entity);

    free (billboard_pool.data); // Likely NULL, but safe
    free (billboard_pool.entity_to_index);
    free (billboard_pool.index_to_entity);

    free (ambient_light_pool.data);
    free (ambient_light_pool.entity_to_index);
    free (ambient_light_pool.index_to_entity);

    free (point_light_pool.data);
    free (point_light_pool.entity_to_index);
    free (point_light_pool.index_to_entity);
}