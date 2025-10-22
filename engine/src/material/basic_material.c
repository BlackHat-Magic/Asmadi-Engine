#include <SDL3/SDL_gpu.h>
#include <material/basic_material.h>
#include <material/m_common.h>

MaterialComponent
create_basic_material (SDL_FColor color, SDL_GPUCullMode cullmode, gpu_renderer* renderer) {
    MaterialComponent mat = {
        .color = color,
        .texture = NULL,
        .vertex_shader = NULL,
        .fragment_shader = NULL,
    };

    // TODO: communicate failure to caller
    mat.vertex_shader = load_shader (renderer->device, "shaders/basic_material.vert.spv", SDL_GPU_SHADERSTAGE_VERTEX, 0, 2, 0, 0);
    if (mat.vertex_shader == NULL) {
        return mat;
    };

    mat.fragment_shader = load_shader (renderer->device, "shaders/basic_material.frag.spv", SDL_GPU_SHADERSTAGE_FRAGMENT, 1, 1, 2, 0);
    if (mat.fragment_shader == NULL) {
        SDL_ReleaseGPUShader (renderer->device, mat.vertex_shader);
        mat.vertex_shader = NULL;
        return mat;
    }

    SDL_GPUGraphicsPipelineCreateInfo pipe_info = {
        .target_info = {
            .num_color_targets = 1,
            .color_target_descriptions = (SDL_GPUColorTargetDescription[]) {
                {
                    .format = renderer->format
                }
            },
            .has_depth_stencil_target = true,
            .depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D24_UNORM
        },
        .primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
        .vertex_shader = mat.vertex_shader,
        .fragment_shader = mat.fragment_shader,
        .vertex_input_state = {
            .vertex_buffer_descriptions = (SDL_GPUVertexBufferDescription[]) {
                {
                    .slot = 0,
                    .pitch = 8 * sizeof (float),
                    .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
                    .instance_step_rate = 0
                }
            },
            .num_vertex_buffers = 1,
            .num_vertex_attributes = 3,
            .vertex_attributes = (SDL_GPUVertexAttribute[]) {
                {
                    .location = 0,
                    .buffer_slot = 0,
                    .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
                    .offset = 0
                },
                {
                    .location = 1,
                    .buffer_slot = 0,
                    .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
                    .offset = 3 * sizeof (float)
                },
                {
                    .location = 2,
                    .buffer_slot = 0,
                    .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
                    .offset = 6 * sizeof (float)
                }
            }
        },
        .rasterizer_state = {
            .fill_mode = SDL_GPU_FILLMODE_FILL,
            .cull_mode = cullmode,
            .front_face = SDL_GPU_FRONTFACE_CLOCKWISE
        },
        .depth_stencil_state = {
            .enable_depth_test = true,
            .enable_depth_write = true,
            .compare_op = SDL_GPU_COMPAREOP_LESS,
            .enable_stencil_test = false
        }
    };
    mat.pipeline = SDL_CreateGPUGraphicsPipeline(renderer->device, &pipe_info);
    if (mat.pipeline == NULL) {
        SDL_ReleaseGPUShader (renderer->device, mat.vertex_shader);
        SDL_ReleaseGPUShader (renderer->device, mat.fragment_shader);
        mat.vertex_shader = NULL;
        mat.fragment_shader = NULL;
        return mat;
    }

    return mat;
}