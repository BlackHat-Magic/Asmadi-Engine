#version 450

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;

layout (location = 0) out vec3 fragColor;
layout (location = 1) out vec2 TexCoord;

layout(std140, set = 1, binding = 0) uniform UBO {
    vec4 color;
    mat4 model;
    mat4 view;
    mat4 projection;
} ubo;

void main() {
    gl_Position = ubo.projection * ubo.view * ubo.model * vec4(aPos, 1.0);
    fragColor = ubo.color.rgb;  // Reuse colors across quad vertices (or update to per-vertex if needed)
    TexCoord = aTexCoord;
}