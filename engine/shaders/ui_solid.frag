#version 450
layout(location = 1) in vec4 vColor;
layout(location = 0) out vec4 outColor;
void main() {
    outColor = vColor;
}