#version 450

layout (location = 0) out vec3 fragColor;
layout (location = 1) out vec2 TexCoord;

layout (set = 1, binding = 0) uniform UBO {
    vec3 colors[3];
    mat4 model;
    mat4 view;
    mat4 projection;
} ubo;

// hardcoded positions my beloved 😍
vec2 positions[3] = vec2[] (
    vec2(0.0, 0.5),
    vec2(-0.5, -0.5),
    vec2(0.5, -0.5)
);

vec2 texCoords[3] = vec2[] (
    vec2(0.5, 1.0),
    vec2(0.0, 0.0),
    vec2(1.0, 0.0)
);

void main() {
    gl_Position = ubo.projection * ubo.view * ubo.model * vec4(positions[gl_VertexIndex], 0.0, 1.0);
    fragColor = ubo.colors[gl_VertexIndex]; // read color from uniform buffer
    TexCoord = texCoords[gl_VertexIndex];
}