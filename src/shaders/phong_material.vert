#version 450

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 TexCoord;
layout(location = 2) out vec3 Normal;  // Pass transformed normal
layout(location = 3) out vec3 FragPos;  // Pass world-space position for light calc

layout(std140, set = 1, binding = 0) uniform UBO {
    vec4 color;
    mat4 model;
    mat4 view;
    mat4 projection;
    vec4 ambient_color;
    vec4 pointLightPos;    // New: Point light position (world space)
    vec4 pointLightColor;  // New: Point light color
} ubo;

void main() {
    gl_Position = ubo.projection * ubo.view * ubo.model * vec4(aPos, 1.0);
    fragColor = ubo.color.rgb;
    TexCoord = aTexCoord;
    FragPos = vec3(ubo.model * vec4(aPos, 1.0));  // World pos
    Normal = mat3(transpose(inverse(ubo.model))) * aNormal;  // Transform normal (normal matrix)
}