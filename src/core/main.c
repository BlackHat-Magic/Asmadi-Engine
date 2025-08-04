#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_keycode.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_oldnames.h>
#include <SDL3/SDL_timer.h>
#include "geometry/cylinder.h"
#include "geometry/tetrahedron.h"
#define SDL_MAIN_USE_CALLBACKS 1

#include <SDL3/SDL.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_video.h>
#include <stdlib.h>
#include <math.h>

#include "core/appstate.h"
#include "ecs/ecs.h"
#include "geometry/box.h"
#include "geometry/capsule.h"
#include "geometry/circle.h"
#include "geometry/cone.h"
// #include "geometry/dodecahedron.h"
#include "geometry/icosahedron.h"
#include "geometry/octahedron.h"
#include "geometry/plane.h"
#include "geometry/ring.h"
#include "geometry/sphere.h"
#include "geometry/torus.h"
#include "geometry/tetrahedron.h"
#include "material/m_common.h"
#include "material/basic_material.h"
#include "math/matrix.h"

#define STARTING_WIDTH 640
#define STARTING_HEIGHT 480
#define STARTING_FOV 70.0
#define MOUSE_SENSE 1.0f / 100.0f
#define MOVEMENT_SPEED 3.0f

SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event) {
    AppState* state = (AppState*)appstate;

    Entity cam = state->camera_entity;
    if (cam == (Entity)-1 || !entity_active[cam]) return SDL_APP_CONTINUE;
    TransformComponent* cam_trans = &transforms[cam];

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

SDL_AppResult SDL_AppInit(void** appstate, int argc, char** argv) {
    SDL_SetAppMetadata(
        "GMTK GameJam 2025 SDL", "0.1.0", "xyz.lukeh.GMTK-GameJam-2025-SDL"
    );

    // create appstate
    AppState* state = (AppState*)calloc(1, sizeof(AppState));
    state->width    = STARTING_WIDTH;
    state->height   = STARTING_HEIGHT;
    state->camera_entity      = (Entity)-1;

    // initialize SDL
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("Couldn't initialize SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    // Create window
    state->window = SDL_CreateWindow(
        "C Vulkan", state->width, state->height,
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_VULKAN
    );
    if (!state->window) {
        SDL_Log("Couldn't create window/renderer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    // create GPU device
    state->device =
        SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, false, NULL);
    if (!state->device) {
        SDL_Log("Couldn't create SDL_GPU_DEVICE");
        return SDL_APP_FAILURE;
    }
    if (!SDL_ClaimWindowForGPUDevice(state->device, state->window)) {
        SDL_Log("Couldn't claim window for GPU device: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    state->swapchain_format = SDL_GetGPUSwapchainTextureFormat(state->device, state->window);
    if (state->swapchain_format == SDL_GPU_TEXTUREFORMAT_INVALID) {
        SDL_Log ("Failed to get swapchain texture format: %s", SDL_GetError ());
        return SDL_APP_FAILURE;
    }

    // depth texture
    SDL_GPUTextureCreateInfo depth_info = {
        .type                 = SDL_GPU_TEXTURETYPE_2D,
        .format               = SDL_GPU_TEXTUREFORMAT_D24_UNORM,
        .width                = state->width,
        .height               = state->height,
        .layer_count_or_depth = 1,
        .num_levels           = 1,
        .usage                = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET
    };
    state->depth_texture = SDL_CreateGPUTexture(state->device, &depth_info);
    if (state->depth_texture == NULL) {
        SDL_Log("Failed to create depth texture: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    state->dwidth = state->width;
    state->dheight = state->height;

     // load texture
    state->white_texture = create_white_texture (state->device);
    if (!state->white_texture) return SDL_APP_FAILURE; // logging handled inside load_texture()

    // create sampler
    SDL_GPUSamplerCreateInfo sampler_info = {
        .min_filter        = SDL_GPU_FILTER_LINEAR,
        .mag_filter        = SDL_GPU_FILTER_LINEAR,
        .mipmap_mode       = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR,
        .address_mode_u    = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
        .address_mode_v    = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
        .address_mode_w    = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
        .max_anisotropy    = 1.0f,
        .enable_anisotropy = false
    };
    state->sampler = SDL_CreateGPUSampler(state->device, &sampler_info);
    if (!state->sampler) {
        SDL_Log("Failed to create sampler: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    // create box entity
    Entity box = create_entity();
    // create box mesh
    MeshComponent* box_mesh = create_box_mesh(1.0f, 1.0f, 1.0f, state->device);
    if (box_mesh == NULL)
        return SDL_APP_FAILURE;  // logging handled inside create_box_mesh()
    add_mesh(box, box_mesh);
    // create box material
    MaterialComponent box_material =
        create_basic_material((vec3){1.0f, 0.0f, 0.0f}, SIDE_FRONT, state);
    add_material(box, box_material);
    // box transform
    add_transform(
        box, (vec3) {0.0f, 0.0f, 0.0f}, (vec3){0.0f, 0.0f, 0.0f},
        (vec3){1.0f, 1.0f, 1.0f}
    );

    // textured box
    Entity tbox = create_entity();
    // texutred box mesh
    MeshComponent* tbox_mesh = create_box_mesh(1.0f, 1.0f, 1.0f, state->device);
    if (box_mesh == NULL)
        return SDL_APP_FAILURE;  // logging handled inside create_box_mesh()
    add_mesh(tbox, tbox_mesh);
    // textured box material
    MaterialComponent tbox_material =
        create_basic_material((vec3){1.0f, 1.0f, 1.0f}, SIDE_FRONT, state);
    tbox_material.texture = load_texture(state->device, "assets/test.png");
    if (tbox_material.texture == NULL) return SDL_APP_FAILURE;
    add_material(tbox, tbox_material);
    // textured box transform
    add_transform(
        tbox, (vec3) {2.0f, 0.0f, 0.0f}, (vec3){0.0f, 0.0f, 0.0f},
        (vec3){1.0f, 1.0f, 1.0f}
    );

    // billboard
    Entity billboard = create_entity();
    add_billboard(billboard);
    // billboard mesh
    MeshComponent* billboard_mesh = create_plane_mesh(1.0f, 1.0f, 1, 1, state->device);
    if (billboard_mesh == NULL) return SDL_APP_FAILURE; // logging handled inside function
    add_mesh(billboard, billboard_mesh);
    // billboard material
    MaterialComponent billboard_material = create_basic_material ((vec3) {1.0f, 1.0f, 1.0f}, SIDE_FRONT, state);
    billboard_material.texture = load_texture(state->device, "assets/test.bmp");
    if (billboard_material.texture == NULL) return SDL_APP_FAILURE;
    add_material(billboard, billboard_material);
    // billboard transform
    add_transform(billboard, (vec3){4.0f, 0.0f, 0.0f}, (vec3){0.0f, 0.0f, 0.0f}, (vec3){1.0f, 1.0f, 1.0f});

    // capsule
    Entity capsule = create_entity();
    MeshComponent* capsule_mesh = create_capsule_mesh (0.5f, 1.0f, 8, 16, state->device);
    if (!capsule_mesh) return SDL_APP_FAILURE;
    add_mesh(capsule, capsule_mesh);
    // capsule material
    MaterialComponent capsule_material = create_basic_material ((vec3) {0.0f, 1.0f, 0.0f}, SIDE_FRONT, state);
    add_material (capsule, capsule_material);
    // capsule transform
    add_transform(capsule, (vec3) {6.0f, 0.0f, 0.0f}, (vec3){0.0f, 0.0f, 0.0f}, (vec3){1.0f, 1.0f, 1.0f});

    // circle
    Entity circle = create_entity();
    MeshComponent* circle_mesh = create_circle_mesh (0.5f, 16, state->device);
    add_mesh(circle, circle_mesh);
    // circle material
    MaterialComponent circle_material = create_basic_material ((vec3) {1.0f, 1.0f, 0.0f}, SIDE_DOUBLE, state);
    add_material(circle, circle_material);
    // circle transform
    add_transform (circle, (vec3) {8.0f, 0.0f, 0.0f}, (vec3) {0.0f, 0.0f, 0.0f}, (vec3) {1.0f, 1.0f, 1.0f});

    // cone
    Entity cone = create_entity();
    // cone mesh
    MeshComponent* cone_mesh = create_cone_mesh (0.5f, 1.0f, 16, 1, false, 0.0f, (float)M_PI * 2.0f, state->device);
    add_mesh(cone, cone_mesh);
    // cone material
    MaterialComponent cone_material = create_basic_material ((vec3) {0.0f, 0.0f, 1.0f}, SIDE_FRONT, state);
    add_material (cone, cone_material);
    // cone transform
    add_transform (cone, (vec3) {0.0f, 2.0f, 0.0f}, (vec3){0.0f, 0.0f, 0.0f}, (vec3){1.0f, 1.0f, 1.0f});

    // cylinder
    Entity cylinder = create_entity();
    // cylinder mesh
    MeshComponent* cylinder_mesh = create_cylinder_mesh(0.5f, 0.5f, 1.0f, 16, 1, false, 0.0f, (float)M_PI * 2.0f, state->device);
    add_mesh(cylinder, cylinder_mesh);
    // cylinder material
    MaterialComponent cylinder_material = create_basic_material((vec3) {1.0f, 0.0f, 1.0f}, SIDE_FRONT, state);
    add_material(cylinder, cylinder_material);
    // cylinder transform
    add_transform (cylinder, (vec3) {2.0f, 2.0f, 0.0f}, (vec3) {0.0f, 0.0f, 0.0f}, (vec3){1.0f, 1.0f, 1.0f});

    // ring
    Entity ring = create_entity();
    // mesh
    MeshComponent* ring_mesh = create_ring_mesh(0.25f, 0.5f, 32, 1, 0.0f, (float)M_PI * 2.0f, state->device);
    if (ring_mesh == NULL) return SDL_APP_FAILURE;
    add_mesh(ring, ring_mesh);
    // material
    MaterialComponent ring_material = create_basic_material((vec3){0.0f, 1.0f, 1.0f}, SIDE_DOUBLE, state);
    add_material(ring, ring_material);
    // transform
    add_transform(ring, (vec3){4.0f, 2.0f, 0.0f}, (vec3){0.0f, 0.0f, 0.0f}, (vec3){1.0f, 1.0f, 1.0f});

    // sphere
    Entity sphere = create_entity();
    // mesh
    MeshComponent* sphere_mesh = create_sphere_mesh(0.5f, 32, 16, 0.0f, (float)M_PI * 2.0f, 0.0f, (float)M_PI, state->device);
    if (sphere_mesh == NULL) return SDL_APP_FAILURE;
    add_mesh(sphere, sphere_mesh);
    // material
    MaterialComponent sphere_material = create_basic_material((vec3){1.0f, 1.0f, 1.0f}, SIDE_FRONT, state);
    add_material(sphere, sphere_material);
    // transform
    add_transform(sphere, (vec3){6.0f, 2.0f, 0.0f}, (vec3){0.0f, 0.0f, 0.0f}, (vec3){1.0f, 1.0f, 1.0f});

    // torus
    Entity torus = create_entity();
    // mesh
    MeshComponent* torus_mesh = create_torus_mesh(0.5f, 0.2f, 16, 32, (float)M_PI * 2.0f, state->device);
    if (torus_mesh == NULL) return SDL_APP_FAILURE;
    add_mesh(torus, torus_mesh);
    // material
    MaterialComponent torus_material = create_basic_material((vec3){0.5f, 0.0f, 0.0f}, SIDE_FRONT, state);
    add_material(torus, torus_material);
    // transform
    add_transform(torus, (vec3){8.0f, 2.0f, 0.0f}, (vec3){0.0f, 0.0f, 0.0f}, (vec3){1.0f, 1.0f, 1.0f});

    // tetrahedron
    Entity tetrahedron = create_entity();
    // mesh
    MeshComponent* tetrahedron_mesh = create_tetrahedron_mesh(0.5f, state->device);
    if (tetrahedron_mesh == NULL) return SDL_APP_FAILURE;
    add_mesh(tetrahedron, tetrahedron_mesh);
    // material
    MaterialComponent tetrahedron_material = create_basic_material((vec3){0.0f, 0.5f, 0.0f}, SIDE_FRONT, state);
    add_material(tetrahedron, tetrahedron_material);
    // transform
    add_transform(tetrahedron, (vec3){0.0f, 4.0f, 0.0f}, (vec3){0.0f, 0.0f, 0.0f}, (vec3){1.0f, 1.0f, 1.0f});

    // octahedron
    Entity octahedron = create_entity();
    // mesh
    MeshComponent* octahedron_mesh = create_octahedron_mesh(0.5f, state->device);
    if (octahedron_mesh == NULL) return SDL_APP_FAILURE;
    add_mesh (octahedron, octahedron_mesh);
    // material
    MaterialComponent octahedron_material = create_basic_material ((vec3){0.5f, 0.5f, 0.0f}, SIDE_FRONT, state);
    add_material(octahedron, octahedron_material);
    // transform
    add_transform(octahedron, (vec3){2.0f, 4.0f, 0.0f}, (vec3){0.0f, 0.0f, 0.0f}, (vec3){1.0f, 1.0f, 1.0f});

    /*
    // dodecahedron
    Entity dodecahedron = create_entity();
    // mesh
    MeshComponent* dodecahedron_mesh = create_dodecahedron_mesh(0.5f, state->device);
    if (dodecahedron_mesh == NULL) return SDL_APP_FAILURE;
    add_mesh (dodecahedron, dodecahedron_mesh);
    // material
    MaterialComponent dodecahedron_material = create_basic_material ((vec3){0.0f, 0.0f, 0.5f}, SIDE_FRONT, state);
    add_material(dodecahedron, dodecahedron_material);
    // transform
    add_transform(dodecahedron, (vec3){4.0f, 4.0f, 0.0f}, (vec3){0.0f, 0.0f, 0.0f}, (vec3){1.0f, 1.0f, 1.0f});
    */

    // octahedron
    Entity icosahedron = create_entity();
    // mesh
    MeshComponent* icosahedron_mesh = create_icosahedron_mesh(0.5f, state->device);
    if (icosahedron_mesh == NULL) return SDL_APP_FAILURE;
    add_mesh (icosahedron, icosahedron_mesh);
    // material
    MaterialComponent icosahedron_material = create_basic_material ((vec3){0.5f, 0.0f, 0.5f}, SIDE_FRONT, state);
    add_material(icosahedron, icosahedron_material);
    // transform
    add_transform(icosahedron, (vec3){6.0f, 4.0f, 0.0f}, (vec3){0.0f, 0.0f, 0.0f}, (vec3){1.0f, 1.0f, 1.0f});

    // camera
    Entity camera = create_entity();
    add_transform(camera, (vec3){0.0f, 0.0f, -2.0f}, (vec3){0.0f, 0.0f, 0.0f}, (vec3){1.0f, 1.0f, 1.0f});
    add_camera(camera, STARTING_FOV, 0.01f, 1000.0f);
    add_fps_controller (camera, MOUSE_SENSE, MOVEMENT_SPEED);
    state->camera_entity = camera;
    SDL_SetWindowRelativeMouseMode(state->window, true);

    state->last_time = SDL_GetPerformanceCounter();

    *appstate = state;
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void* appstate) {
    AppState* state = (AppState*)appstate;
    if (state->quit) return SDL_APP_SUCCESS;

    // TODO: handle no camera
    Entity cam = state->camera_entity;
    if (cam == (Entity)-1) return SDL_APP_CONTINUE;
    TransformComponent* cam_trans = &transforms[cam];

    // dt
    const Uint64 now = SDL_GetPerformanceCounter();
    float dt = (float)(now - state->last_time) /
               (float)(SDL_GetPerformanceFrequency());
    state->last_time = now;

    // camera forward vector
    fps_controller_update_system(state, dt);

    render_system(state);

    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void* appstate, SDL_AppResult result) {
    AppState* state = (AppState*)appstate;
    if (state->white_texture) {
        SDL_ReleaseGPUTexture(state->device, state->white_texture);
    }
    if (state->sampler) {
        SDL_ReleaseGPUSampler(state->device, state->sampler);
    }
    if (state->depth_texture) {
        SDL_ReleaseGPUTexture(state->device, state->depth_texture);
    }
}