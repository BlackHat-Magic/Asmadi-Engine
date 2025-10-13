#version 450

layout(location = 0) in vec4 vColor;
layout(location = 1) in vec2 vUV;

// TODO: figure out why this needs to be 2.
layout(set = 2, binding = 0) uniform sampler2D uTex;

layout(location = 0) out vec4 outColor;

void main() {
    vec4 texel = texture(uTex, vUV);
    outColor = vColor * texel;
}