// generic postprocessing vertex sahader

#version 450

layout(location = 0) in vec3 aPos;
layout(location = 2) in vec2 aTexCoord;  // Skip location 1 (normals unused)

layout(location = 0) out vec2 TexCoord;

void main() {
    gl_Position = vec4(aPos.xy, 0.0, 1.0);
    TexCoord = aTexCoord;
}