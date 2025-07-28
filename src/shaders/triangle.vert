#version 450

layout (location = 0) out vec3 fragColor;

layout (set = 1, binding = 0) uniform ColorsUniformBuffer {
    vec3 colors[3];
} ubo_colors;

vec2 positions[3] = vec2[] (
    vec2(0.0, 0.5),
    vec2(-0.5, -0.5),
    vec2(0.5, -0.5)
);

void main() {
    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
    fragColor = ubo_colors.colors[gl_VertexIndex]; // read color from uniform buffer
}