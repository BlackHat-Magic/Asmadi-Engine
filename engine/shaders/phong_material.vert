#version 450

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 TexCoord;
layout(location = 2) out vec3 Normal;  // Pass transformed normal
layout(location = 3) out vec3 FragPos;  // Pass world-space position for light calc

// Frame UBO
layout(std140, set = 1, binding = 0) uniform FrameUBO {
    mat4 view;
    mat4 projection;
} frame_ubo;

// object ubo
layout(std140, set = 1, binding = 1) uniform ObjectUBO {
    mat4 model;
    vec4 color;
} object_ubo;

void main() {
    gl_Position = frame_ubo.projection * frame_ubo.view * object_ubo.model * vec4(aPos, 1.0);
    fragColor = object_ubo.color.rgb;
    TexCoord = aTexCoord;
    FragPos = vec3(object_ubo.model * vec4(aPos, 1.0));  // World pos
    Normal = mat3(transpose(inverse(object_ubo.model))) * aNormal;  // Transform normal (normal matrix)
}