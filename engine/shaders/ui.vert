#version 450
layout(location = 0) in vec2 aPos;   // screen-space pixels
layout(location = 1) in vec2 aUV;
layout(location = 2) in vec4 aColor; // premultiplied RGBA

layout(push_constant) uniform PC {
    vec2 targetSize; // framebuffer size in pixels
} pc;

layout(location = 0) out vec2 vUV;
layout(location = 1) out vec4 vColor;

void main() {
    // Convert pixels to NDC
    float x = (aPos.x / pc.targetSize.x) * 2.0 - 1.0;
    float y = (aPos.y / pc.targetSize.y) * 2.0 - 1.0;
    // UI y usually top-left; if your coordinate is top-left, flip here:
    y = -y;
    gl_Position = vec4(x, y, 0.0, 1.0);
    vUV = aUV;
    vColor = aColor;
}