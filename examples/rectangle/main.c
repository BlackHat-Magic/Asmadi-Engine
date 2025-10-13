#define SDL_MAIN_USE_CALLBACKS 1

#include <math.h>
#include <stdlib.h>
#include <stdio.h>

#include <SDL3/SDL_main.h>

#include <microui.h>

#include <geometry/torus.h>
#include <material/phong_material.h>
#include <ui/ui.h>

#define STARTING_WIDTH 1280
#define STARTING_HEIGHT 720
#define STARTING_FOV 70.0
#define MOUSE_SENSE 1.0f / 100.0f
#define MOVEMENT_SPEED 3.0f


typedef struct {
    bool quit;
    gpu_renderer renderer;
    Entity player;
    Entity torus;
    Uint64 last_time;
    Uint64 prerender;
    Uint64 preui;
    Uint64 postrender;
    float frame_rate;
    Uint64 frame_count;
    bool relative_mouse;
} AppState;

SDL_AppResult SDL_AppEvent (void* appstate, SDL_Event* event) {
    AppState* state = (AppState*) appstate;

    Entity cam = state->player;
    if (cam == (Entity) -1 || !has_transform (cam)) return SDL_APP_CONTINUE;
    TransformComponent* cam_trans = get_transform (cam);

    switch (event->type) {
    case SDL_EVENT_QUIT:
        state->quit = true;
        break;
    case SDL_EVENT_KEY_DOWN:
        if (event->key.key == SDLK_ESCAPE) {
            state->relative_mouse = !state->relative_mouse;
            SDL_SetWindowRelativeMouseMode (state->renderer.window, state->relative_mouse);
        }
        break;
    }

    if (state->relative_mouse) fps_controller_event_system(event);

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppInit (void** appstate, int argc, char** argv) {
    SDL_SetAppMetadata (
        "Asmadi Engine Box Geometry", "0.1.0", "xyz.lukeh.Asmadi-Engine"
    );

    // create appstate
    AppState* state = (AppState*) calloc (1, sizeof (AppState));
    if (state == NULL) {
        SDL_Log ("Failed to allocate app state.");
        return SDL_APP_FAILURE;
    }
    state->renderer.width = STARTING_WIDTH;
    state->renderer.height = STARTING_HEIGHT;
    state->player = (Entity) -1;

    // initialize SDL
    if (!SDL_Init (SDL_INIT_VIDEO)) {
        SDL_Log ("Couldn't initialize SDL: %s", SDL_GetError ());
        return SDL_APP_FAILURE;
    }

    // Create window
    state->renderer.window = SDL_CreateWindow (
        "C Vulkan", state->renderer.width, state->renderer.height,
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_VULKAN
    );
    if (!state->renderer.window) {
        SDL_Log ("Couldn't create window/renderer: %s", SDL_GetError ());
        return SDL_APP_FAILURE;
    }

    // Initialize SDL_ttf
    if (!TTF_Init ()) {
        SDL_Log ("Couldn't initialize SDL_ttf: %s", SDL_GetError ());
        return SDL_APP_FAILURE;
    }

    // create GPU device
    state->renderer.device =
        SDL_CreateGPUDevice (SDL_GPU_SHADERFORMAT_SPIRV, false, NULL);
    if (!state->renderer.device) {
        SDL_Log ("Couldn't create SDL_GPU_DEVICE");
        return SDL_APP_FAILURE;
    }
    if (!SDL_ClaimWindowForGPUDevice (state->renderer.device, state->renderer.window)) {
        SDL_Log ("Couldn't claim window for GPU device: %s", SDL_GetError ());
        return SDL_APP_FAILURE;
    }
    state->renderer.format =
        SDL_GetGPUSwapchainTextureFormat (state->renderer.device, state->renderer.window);
    if (state->renderer.format == SDL_GPU_TEXTUREFORMAT_INVALID) {
        SDL_Log ("Failed to get swapchain texture format: %s", SDL_GetError ());
        return SDL_APP_FAILURE;
    }

    // depth texture
    SDL_GPUTextureCreateInfo depth_info = {
        .type = SDL_GPU_TEXTURETYPE_2D,
        .format = SDL_GPU_TEXTUREFORMAT_D24_UNORM,
        .width = state->renderer.width,
        .height = state->renderer.height,
        .layer_count_or_depth = 1,
        .num_levels = 1,
        .usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET
    };
    state->renderer.depth_texture = SDL_CreateGPUTexture (state->renderer.device, &depth_info);
    if (state->renderer.depth_texture == NULL) {
        SDL_Log ("Failed to create depth texture: %s", SDL_GetError ());
        return SDL_APP_FAILURE;
    }
    state->renderer.dwidth = state->renderer.width;
    state->renderer.dheight = state->renderer.height;

    // load texture
    state->renderer.white_texture = create_white_texture (state->renderer.device);
    if (!state->renderer.white_texture)
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
    state->renderer.sampler = SDL_CreateGPUSampler (state->renderer.device, &sampler_info);
    if (!state->renderer.sampler) {
        SDL_Log ("Failed to create sampler: %s", SDL_GetError ());
        return SDL_APP_FAILURE;
    }

    // player
    state->player = create_entity ();
    add_transform (
        state->player, (vec3) {0.0f, 0.0f, -2.0f}, (vec3) {0.0f, 0.0f, 0.0f},
        (vec3) {1.0f, 1.0f, 1.0f}
    );
    add_camera (state->player, STARTING_FOV, 0.01f, 1000.0f);
    add_fps_controller (state->player, MOUSE_SENSE, MOVEMENT_SPEED);
    state->relative_mouse = true;
    SDL_SetWindowRelativeMouseMode (state->renderer.window, state->relative_mouse);
    UIComponent* ui = create_ui_component (
        &state->renderer, 255, 255, "./assets/NotoSans-Regular.ttf", 12.0f
    );
    if (ui == NULL) {
        // logging handled inside function
        return SDL_APP_FAILURE;
    }
    add_ui (state->player, ui);
    // ui->context.style->title_height = 30;
    // ui->context.style->padding = 6;

    // torus
    state->torus = create_entity ();
    MeshComponent torus_mesh = create_torus_mesh (
        0.5f, 0.2f, 16, 32, (float) M_PI * 2.0f, state->renderer.device
    );
    if (torus_mesh.vertex_buffer == NULL) return SDL_APP_FAILURE;
    add_mesh (state->torus, torus_mesh);
    // torus material
    MaterialComponent torus_material =
        create_phong_material ((vec3) {0.0f, 1.0f, 0.0f}, SIDE_FRONT, &state->renderer);
    add_material (state->torus, torus_material);
    // torus transform
    add_transform (
        state->torus, (vec3) {0.0f, 0.0f, 0.0f}, (vec3) {0.0f, 0.0f, 0.0f},
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

    state->last_time = SDL_GetPerformanceCounter ();

    *appstate = state;
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate (void* appstate) {
    AppState* state = (AppState*) appstate;
    if (state->quit) return SDL_APP_SUCCESS;

    // TODO: handle no camera
    Entity cam = state->player;
    if (cam == (Entity) -1) return SDL_APP_CONTINUE;
    TransformComponent* cam_trans = get_transform (cam);

    // dt
    const Uint64 now = SDL_GetPerformanceCounter ();
    float dt = (float) (now - state->last_time) /
               (float) (SDL_GetPerformanceFrequency ());
    state->last_time = now;

    float render_time_ms = (float) (state->postrender - state->prerender) / 1e6;
    float mesh_time_ms = (float) (state->preui - state->prerender) / 1e6;
    float ui_time_ms = (float) (state->postrender - state->preui) / 1e6;
    state->frame_count++;

    // draw ui
    UIComponent* ui = get_ui (state->player);
    mu_begin (&ui->context);
    if (mu_begin_window (&ui->context, "Test Window", mu_rect (250, 250, 300, 240))) {
        mu_layout_row (&ui->context, 1, (int[]){80}, 0);
        mu_label (&ui->context, "Test label");
        mu_end_window (&ui->context);
    }
    mu_end (&ui->context);
    // ui_render (state, ui);

    char buffer[64];
    sprintf (buffer, "Mesh render: %.1f", mesh_time_ms);
    draw_text (ui, state->renderer.device, buffer, 5.0f, 5.0f, 1.0f, 1.0f, 1.0f, 1.0f);
    sprintf (buffer, "UI render: %.1f", ui_time_ms);
    draw_text (ui, state->renderer.device, buffer, 5.0f, 17.0f, 1.0f, 1.0f, 1.0f, 1.0f);
    sprintf (buffer, "Total render: %.1f", render_time_ms);
    draw_text (ui, state->renderer.device, buffer, 5.0f, 29.0f, 1.0f, 1.0f, 1.0f, 1.0f);
    if (state->frame_count % 60 == 0) {
        state->frame_rate = 1000.0f / render_time_ms;
    }
    sprintf (buffer, "Framerate: %.3f", state->frame_rate);
    draw_text (ui, state->renderer.device, buffer, 5.0f, 41.0f, 1.0f, 1.0f, 1.0f, 1.0f);

    TransformComponent transform = *get_transform (state->torus);
    vec3 rotation = euler_from_quat (transform.rotation);
    rotation.x += 0.005f;
    rotation.z += 0.01f;
    transform.rotation = quat_from_euler (rotation);
    add_transform (state->torus, transform.position, rotation, transform.scale);

    // camera forward vector
    fps_controller_update_system (dt);

    render_system (&state->renderer, cam, &state->prerender, &state->preui, &state->postrender);

    return SDL_APP_CONTINUE;
}

void SDL_AppQuit (void* appstate, SDL_AppResult result) {
    AppState* state = (AppState*) appstate;

    UIComponent ui = *(get_ui (state->player));

    if (ui.rects) free (ui.rects);
    if (ui.pipeline)
        SDL_ReleaseGPUGraphicsPipeline (state->renderer.device, ui.pipeline);
    if (ui.fragment) SDL_ReleaseGPUShader (state->renderer.device, ui.fragment);
    if (ui.vertex) SDL_ReleaseGPUShader (state->renderer.device, ui.vertex);

    free_pools (state->renderer.device);
    if (state->renderer.white_texture) {
        SDL_ReleaseGPUTexture (state->renderer.device, state->renderer.white_texture);
    }
    if (state->renderer.sampler) {
        SDL_ReleaseGPUSampler (state->renderer.device, state->renderer.sampler);
    }
    if (state->renderer.depth_texture) {
        SDL_ReleaseGPUTexture (state->renderer.device, state->renderer.depth_texture);
    }
}