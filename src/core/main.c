#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_keycode.h>
#include <SDL3/SDL_oldnames.h>
#include <SDL3/SDL_timer.h>
#define SDL_MAIN_USE_CALLBACKS 1

#include <SDL3/SDL.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_video.h>
#include <math.h>
#include <stdlib.h>

#include "core/appstate.h"
#include "ecs/ecs.h"
#include "geometry/box.h"
#include "material/basic_material.h"
#include "material/m_common.h"
#include "math/matrix.h"

#define STARTING_WIDTH 640
#define STARTING_HEIGHT 480
#define STARTING_FOV 70.0
#define MOUSE_SENSE 1.0f / 100.0f
#define MOVEMENT_SPEED 3.0f

SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event) {
    AppState* state = (AppState*)appstate;
    switch (event->type) {
        case SDL_EVENT_QUIT:
            state->quit = true;
            break;
        case SDL_EVENT_MOUSE_MOTION:
            float delta_yaw = -event->motion.xrel * MOUSE_SENSE;
            float delta_pitch = event->motion.yrel * MOUSE_SENSE;

            // Global yaw around world up (keeps level)
            vec4 dq_yaw = quat_from_axis_angle((vec3){0.0f, 1.0f, 0.0f}, delta_yaw);
            *state->camera_rotation = quat_multiply(dq_yaw, *state->camera_rotation);

            // Local pitch around horizontal right axis (cross(forward, world_up))
            vec3 forward = vec3_rotate(*state->camera_rotation, (vec3){0.0f, 0.0f, -1.0f});
            vec3 right = vec3_normalize(vec3_cross(forward, (vec3){0.0f, 1.0f, 0.0f}));
            vec4 dq_pitch = quat_from_axis_angle(right, delta_pitch);
            *state->camera_rotation = quat_multiply(dq_pitch, *state->camera_rotation);

            // Normalize total rotation
            *state->camera_rotation = quat_normalize(*state->camera_rotation);

            // Clamp pitch
            forward = vec3_rotate(*state->camera_rotation, (vec3){0.0f, 0.0f, -1.0f});
            float curr_pitch = asinf(forward.y);
            if (curr_pitch > (float)M_PI * 0.49f || curr_pitch < -(float)M_PI * 0.49f) {
                float clamped_pitch = curr_pitch;
                if (clamped_pitch > (float)M_PI * 0.49f) clamped_pitch = (float)M_PI * 0.49f;
                if (clamped_pitch < -(float)M_PI * 0.49f) clamped_pitch = -(float)M_PI * 0.49f;
                float curr_yaw = atan2f(forward.x, forward.z) + (float)M_PI;
                *state->camera_rotation = quat_from_euler((vec3){clamped_pitch, curr_yaw, 0.0f});
            }
            break;
        case SDL_EVENT_KEY_DOWN:
            if (event->key.key == SDLK_ESCAPE) state->quit = true;
            break;
    }

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
    state->fov      = STARTING_FOV;

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
    state->texture = load_texture(state->device, "assets/test.png");
    if (!state->texture) return SDL_APP_FAILURE; // logging handled inside load_texture()

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
    vec3 transforms[] = {
        {0.0f, 0.0f, 0.0f},
        {2.0f, 0.0f, 0.0f},
        {3.0f, 4.0f, 0.0f},
        {0.0f, 0.0f, 5.0f},
        {-2.0f, 0.0f, -3.0f},
        {-4.0f, -5.0f, 1.0f},
        {0.0f, 0.0f, 2.0f},
        {0.0f, 3.0f, 0.0f},
        {0.0f, 4.0f, 5.0f},
        {-2.0f, 0.0f, 0.0f}
    };
    for (int i = 0; i < 10; i++) {
        Entity box              = create_entity();

        // create box mesh
        MeshComponent* box_mesh = create_box_mesh(1.0f, 1.0f, 1.0f, state->device);
        if (box_mesh == NULL)
            return SDL_APP_FAILURE;  // logging handled inside create_box_mesh()
        add_mesh(box, box_mesh);

        // create box material
        MaterialComponent box_material =
            create_basic_material((vec3){0.75f, 0.0f, 0.0f}, state->device);

        // box vertex shader
        int vert_failed = set_vertex_shader(
            state->device, &box_material, "shaders/triangle.vert.spv",
            state->swapchain_format
        );
        if (vert_failed)
            return SDL_APP_FAILURE;  // logging handled in set_vertex_shader

        // box fragment shader
        int frag_failed = set_fragment_shader(
            state->device, &box_material, "shaders/triangle.frag.spv",
            state->swapchain_format
        );
        if (frag_failed)
            return SDL_APP_FAILURE;  // logging handled in set_fragment_shader

        // apply box material
        add_material(box, box_material);
        add_transform(
            box, transforms[i], (vec3){0.0f, 0.0f, 0.0f},
            (vec3){1.0f, 1.0f, 1.0f}
        );
    }

    // view matrix
    mat4* view = (mat4*)malloc(sizeof(mat4));
    mat4_identity(*view);
    state->view_matrix = view;

    // perspective matrix
    mat4* proj = (mat4*)malloc(sizeof(mat4));
    mat4_perspective(
        *proj, STARTING_FOV * (float)M_PI / 180.0f,
        (float)STARTING_WIDTH / (float)STARTING_HEIGHT, 0.01f, 1000.0f
    );
    state->proj_matrix = proj;

    // camera
    vec3* camera_pos    = (vec3*)malloc(sizeof(vec3));
    *camera_pos         = (vec3){0.0f, 0.0f, -2.0f};
    state->camera_pos   = camera_pos;
    vec4* camera_rotation = (vec4*) malloc (sizeof (vec4));
    *camera_rotation = quat_from_euler ((vec3) {0.0f, (float)M_PI, 0.0f});
    state->camera_rotation = camera_rotation;
    SDL_SetWindowRelativeMouseMode(state->window, true);

    state->last_time = SDL_GetPerformanceCounter();

    *appstate = state;
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void* appstate) {
    AppState* state = (AppState*)appstate;
    if (state->quit) return SDL_APP_SUCCESS;

    // dt
    const Uint64 now = SDL_GetPerformanceCounter();

    float dt = (float)(now - state->last_time) /
               (float)(SDL_GetPerformanceFrequency());
    state->last_time = now;

    // camera forward vector
    vec3 camera_forward = quat_rotate(*state->camera_rotation, (vec3){0.0f, 0.0f, -1.0f});
    vec3 camera_right = quat_rotate(*state->camera_rotation, (vec3){1.0f, 0.0f, 0.0f});
    vec3 camera_up = quat_rotate(*state->camera_rotation, (vec3){0.0f, -1.0f, 0.0f});
    // TODO: fix camera_up being wrong

    // movement
    int numkeys;
    const bool* key_state = SDL_GetKeyboardState(&numkeys);
    vec3 motion           = {0.0f, 0.0f, 0.0f};
    if (key_state[SDL_SCANCODE_W]) motion = vec3_add(motion, camera_forward);
    if (key_state[SDL_SCANCODE_A]) motion = vec3_sub(motion, camera_right);
    if (key_state[SDL_SCANCODE_S]) motion = vec3_sub(motion, camera_forward);
    if (key_state[SDL_SCANCODE_D]) motion = vec3_add(motion, camera_right);
    if (key_state[SDL_SCANCODE_SPACE]) motion = vec3_add(motion, camera_up);
    motion             = vec3_normalize(motion);
    motion             = vec3_scale(motion, dt * MOVEMENT_SPEED);
    *state->camera_pos = vec3_add(*state->camera_pos, motion);

    // look
    mat4_identity(*state->view_matrix);
    vec4 conj_rotation = quat_conjugate(*state->camera_rotation);
    mat4_rotate_quat(*state->view_matrix, conj_rotation);
    mat4_translate(*state->view_matrix, vec3_scale(*state->camera_pos, -1.0f));

    render_system(state);

    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void* appstate, SDL_AppResult result) {
    AppState* state = (AppState*)appstate;
    if (state->texture) {
        SDL_ReleaseGPUTexture(state->device, state->texture);
    }
    if (state->sampler) {
        SDL_ReleaseGPUSampler(state->device, state->sampler);
    }
    if (state->view_matrix) {
        free(state->view_matrix);
    }
    if (state->proj_matrix) {
        free(state->proj_matrix);
    }
    if (state->depth_texture) {
        SDL_ReleaseGPUTexture(state->device, state->depth_texture);
    }
    if (state->camera_rotation) {
        free (state->camera_rotation);
    }
}