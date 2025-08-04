#version 450

layout (location = 0) in vec3 aPos;

layout (set = 1, binding = 0) uniform UBO {
    vec3 colors[3];
    mat4 model;
    mat4 view;
    mat4 projection;
} ubo;

layout (location = 0) out vec3 fragColor;

void main() {
    gl_Position = ubo.projection * ubo.view * ubo.model * vec4(aPos, 1.0);
    fragColor = ubo.colors[0];  // Use first color for all vertices
}