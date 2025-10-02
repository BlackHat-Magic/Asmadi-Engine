#version 450
layout(location = 0) in vec2 vUV;
layout(location = 1) in vec4 vColor;

layout(set = 0, binding = 0) uniform sampler2D uTex;

layout(location = 0) out vec4 outColor;
void main() {
    vec4 tex = texture(uTex, vUV);
    // If atlas is alpha-only expanded to RGBA in upload, use tex.r as alpha.
    outColor = vec4(vColor.rgb, vColor.a * tex.a) * tex; // Choose your blend: multiply color or tint alpha-only
}