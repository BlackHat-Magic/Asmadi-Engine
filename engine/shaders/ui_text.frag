#version 450
layout(location=0) in vec2 vUV;
layout(location=1) in vec4 vColor;

layout(set=1, binding=0) uniform sampler2D uFont;

layout(location=0) out vec4 outColor;

void main() {
    // If UV is zero (for rectangles), treat alpha as 1
    float alpha = 1.0;
    if (vUV != vec2(0.0)) {
        float a = texture(uFont, vUV).r;
        alpha = a;
    }
    outColor = vec4(vColor.rgb, vColor.a * alpha);
    if (outColor.a < 0.01) discard;
}