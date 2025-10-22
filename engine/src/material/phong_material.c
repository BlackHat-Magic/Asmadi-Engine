#include <SDL3/SDL_gpu.h>
#include <material/m_common.h>
#include <material/phong_material.h>

MaterialComponent
create_phong_material (gpu_renderer* renderer, SDL_FColor color, SDL_FColor emissive, SDL_GPUCullMode cullmode, SDL_GPUTexture* texture, SDL_GPUSampler* sampler) {
    // TODO: return pointer
    MaterialComponent mat = {
        .color = color,
        .emissive = emissive,
        .texture = texture,
        .sampler = sampler,
        .vertex_shader = NULL,
        .fragment_shader = NULL,
    };

    // TODO: communicate failure to caller
    mat.vertex_shader = load_shader (renderer->device, "shaders/phong_material.vert.spv", SDL_GPU_SHADERSTAGE_VERTEX, 0, 2, 0, 0);
    if (mat.vertex_shader == NULL) {
        return mat;
    }

    mat.fragment_shader = load_shader (renderer->device, "shaders/phong_material.frag.spv", SDL_GPU_SHADERSTAGE_FRAGMENT, 1, 1, 2, 0);
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
        SDL_Log ("Failed to create phong material pipeline: %s", SDL_GetError ());
        return mat;
    }

    SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer (renderer->device);
    if (cmd == NULL) {
        SDL_ReleaseGPUShader (renderer->device, mat.vertex_shader);
        SDL_ReleaseGPUShader (renderer->device, mat.fragment_shader);
        SDL_ReleaseGPUGraphicsPipeline(renderer->device, mat.pipeline);
        mat.vertex_shader = NULL;
        mat.fragment_shader = NULL;
        return mat;
    }

    // material UBO (descriptor set 1)
    Uint32 ubo_size = 0;
    ubo_size += sizeof (color);
    ubo_size += sizeof (emissive);
    // PBR params (metallic, roughness, specular, sheen, clearcoat, IOR, etc.)
    // opacity/alpha cutoff for alpha test
    // UV transform/scale/offset (prob not)
    // flags/modes
    // indices/scales for texture channels
    Uint8 fragment_ubo[ubo_size > 0 ? ubo_size : 1];
    memcpy (fragment_ubo, &color, sizeof (color));
    memcpy (fragment_ubo + sizeof (color), &emissive, sizeof (emissive));
    SDL_PushGPUFragmentUniformData (cmd, 0, fragment_ubo, ubo_size);

    return mat;
}