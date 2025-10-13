#include <math/matrix.h>
#define SDL_MAIN_USE_CALLBACKS 1

#include <stdlib.h>

#include <SDL3/SDL_main.h>

#include <ecs/ecs.h>
#include <geometry/tetrahedron.h>
#include <material/m_common.h>
#include <material/phong_material.h>

#define STARTING_WIDTH 640
#define STARTING_HEIGHT 480
#define STARTING_FOV 70.0
#define MOUSE_SENSE 1.0f / 100.0f
#define MOVEMENT_SPEED 3.0f

Entity tetrahedron;

SDL_AppResult SDL_AppEvent (void* appstate, SDL_Event* event) {
    AppState* state = (AppState*) appstate;

    Entity cam = state->camera_entity;
    if (cam == (Entity) -1 || !has_transform (cam)) return SDL_APP_CONTINUE;
    TransformComponent* cam_trans = get_transform (cam);

    switch (event->type) {
    case SDL_EVENT_QUIT:
        state->quit = true;
        break;
    case SDL_EVENT_KEY_DOWN:
        if (event->key.key == SDLK_ESCAPE) state->quit = true;
        break;
    }

    fps_controller_event_system (state, event);

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppInit (void** appstate, int argc, char** argv) {
    SDL_SetAppMetadata (
        "Asmadi Engine Box Geometry", "0.1.0", "xyz.lukeh.Asmadi-Engine"
    );

    // create appstate
    AppState* state = (AppState*) calloc (1, sizeof (AppState));
    state->width = STARTING_WIDTH;
    state->height = STARTING_HEIGHT;
    state->camera_entity = (Entity) -1;

    // initialize SDL
    if (!SDL_Init (SDL_INIT_VIDEO)) {
        SDL_Log ("Couldn't initialize SDL: %s", SDL_GetError ());
        return SDL_APP_FAILURE;
    }

    // Create window
    state->window = SDL_CreateWindow (
        "C Vulkan", state->width, state->height,
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_VULKAN
    );
    if (!state->window) {
        SDL_Log ("Couldn't create window/renderer: %s", SDL_GetError ());
        return SDL_APP_FAILURE;
    }

    // create GPU device
    state->device =
        SDL_CreateGPUDevice (SDL_GPU_SHADERFORMAT_SPIRV, false, NULL);
    if (!state->device) {
        SDL_Log ("Couldn't create SDL_GPU_DEVICE");
        return SDL_APP_FAILURE;
    }
    if (!SDL_ClaimWindowForGPUDevice (state->device, state->window)) {
        SDL_Log ("Couldn't claim window for GPU device: %s", SDL_GetError ());
        return SDL_APP_FAILURE;
    }
    state->swapchain_format =
        SDL_GetGPUSwapchainTextureFormat (state->device, state->window);
    if (state->swapchain_format == SDL_GPU_TEXTUREFORMAT_INVALID) {
        SDL_Log ("Failed to get swapchain texture format: %s", SDL_GetError ());
        return SDL_APP_FAILURE;
    }

    // depth texture
    SDL_GPUTextureCreateInfo depth_info = {
        .type = SDL_GPU_TEXTURETYPE_2D,
        .format = SDL_GPU_TEXTUREFORMAT_D24_UNORM,
        .width = state->width,
        .height = state->height,
        .layer_count_or_depth = 1,
        .num_levels = 1,
        .usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET
    };
    state->depth_texture = SDL_CreateGPUTexture (state->device, &depth_info);
    if (state->depth_texture == NULL) {
        SDL_Log ("Failed to create depth texture: %s", SDL_GetError ());
        return SDL_APP_FAILURE;
    }
    state->dwidth = state->width;
    state->dheight = state->height;

    // load texture
    state->white_texture = create_white_texture (state->device);
    if (!state->white_texture)
        return SDL_APP_FAILURE; // logging handled inside load_texture()

    // create sampler
    SDL_GPUSamplerCreateInfo sampler_info = {
        .min_filter = SDL_GPU_FILTER_LINEAR,
        .mag_filter = SDL_GPU_FILTER_LINEAR,
        .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR,
        .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
        .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
        .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
        .max_anisotropy = 1.0f,
        .enable_anisotropy = false
    };
    state->sampler = SDL_CreateGPUSampler (state->device, &sampler_info);
    if (!state->sampler) {
        SDL_Log ("Failed to create sampler: %s", SDL_GetError ());
        return SDL_APP_FAILURE;
    }

    // tetrahedron
    tetrahedron = create_entity ();
    MeshComponent tetrahedron_mesh =
        create_tetrahedron_mesh (0.5f, state->device);
    if (tetrahedron_mesh.vertex_buffer == NULL) return SDL_APP_FAILURE;
    add_mesh (tetrahedron, tetrahedron_mesh);
    // tetrahedron material
    MaterialComponent tetrahedron_material =
        create_phong_material ((vec3) {1.0f, 1.0f, 1.0f}, SIDE_FRONT, state);
    add_material (tetrahedron, tetrahedron_material);
    // tetrahedron transform
    add_transform (
        tetrahedron, (vec3) {0.0f, 0.0f, 0.0f}, (vec3) {0.0f, 0.0f, 0.0f},
        (vec3) {1.0f, 1.0f, 1.0f}
    );

    // ambient light
    Entity ambient_light = create_entity ();
    add_ambient_light (ambient_light, (vec3) {1.0f, 1.0f, 1.0f}, 0.1f);

    // point light
    Entity point_light = create_entity ();
    add_point_light (point_light, (vec3) {1.0f, 1.0f, 1.0f}, 1.0f);
    add_transform (
        point_light, (vec3) {2.0f, 2.0f, -2.0f}, (vec3) {0.0f, 0.0f, 0.0f},
        (vec3) {1.0f, 1.0f, 1.0f}
    );

    // camera
    Entity camera = create_entity ();
    add_transform (
        camera, (vec3) {0.0f, 0.0f, -2.0f}, (vec3) {0.0f, 0.0f, 0.0f},
        (vec3) {1.0f, 1.0f, 1.0f}
    );
    add_camera (camera, STARTING_FOV, 0.01f, 1000.0f);
    add_fps_controller (camera, MOUSE_SENSE, MOVEMENT_SPEED);
    state->camera_entity = camera;
    SDL_SetWindowRelativeMouseMode (state->window, true);

    state->last_time = SDL_GetPerformanceCounter ();

    *appstate = state;
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate (void* appstate) {
    AppState* state = (AppState*) appstate;
    if (state->quit) return SDL_APP_SUCCESS;

    // TODO: handle no camera
    Entity cam = state->camera_entity;
    if (cam == (Entity) -1) return SDL_APP_CONTINUE;
    TransformComponent* cam_trans = get_transform (cam);

    // dt
    const Uint64 now = SDL_GetPerformanceCounter ();
    float dt = (float) (now - state->last_time) /
               (float) (SDL_GetPerformanceFrequency ());
    state->last_time = now;

    TransformComponent transform = *get_transform (tetrahedron);
    vec3 rotation = euler_from_quat (transform.rotation);
    rotation.x += 0.005f;
    rotation.z += 0.01f;
    transform.rotation = quat_from_euler (rotation);
    add_transform (tetrahedron, transform.position, rotation, transform.scale);

    // camera forward vector
    fps_controller_update_system (state, dt);

    render_system (state);

    return SDL_APP_CONTINUE;
}

void SDL_AppQuit (void* appstate, SDL_AppResult result) {
    AppState* state = (AppState*) appstate;
    free_pools (state);
    if (state->white_texture) {
        SDL_ReleaseGPUTexture (state->device, state->white_texture);
    }
    if (state->sampler) {
        SDL_ReleaseGPUSampler (state->device, state->sampler);
    }
    if (state->depth_texture) {
        SDL_ReleaseGPUTexture (state->device, state->depth_texture);
    }
}