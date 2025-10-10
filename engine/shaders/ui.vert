#version 450

layout(location = 0) in vec2 position;
layout(location = 1) in vec2 res;
layout(location = 2) in vec4 color;
layout (location = 3) in vec2 uv;

layout(location = 0) out vec4 vColor;
layout(location = 1) out vec2 vUV;

void main() {
    float x = (position.x / res.x) * 2.0 - 1.0;
    float y = (position.y / res.y) * 2.0 - 1.0;
    y = -y;
    gl_Position = vec4(x, y, 0.0, 1.0);
    vColor = color;
    vUV = uv;
}