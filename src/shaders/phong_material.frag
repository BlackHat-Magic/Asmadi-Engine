#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 TexCoord;
layout(location = 2) in vec3 Normal;
layout(location = 3) in vec3 FragPos;

layout(set = 2, binding = 0) uniform sampler2D texture1;

layout(location = 0) out vec4 outColor;

layout(std140, set = 1, binding = 0) uniform UBO {
    vec4 color;
    mat4 model;
    mat4 view;
    mat4 projection;
    vec4 ambient_color;
    vec4 pointLightPos;
    vec4 pointLightColor;
} ubo;

void main() {
    vec4 texColor = texture(texture1, TexCoord);
    vec3 objectColor = texColor.rgb * fragColor;

    // Ambient (unchanged)
    vec3 ambient_rgb = vec3(ubo.ambient_color.x, ubo.ambient_color.y, ubo.ambient_color.z);
    vec3 ambient = ubo.ambient_color.w * ambient_rgb * objectColor;

    // Diffuse (new)
    vec3 point_rgb = vec3(ubo.pointLightColor.x, ubo.pointLightColor.y, ubo.pointLightColor.z);
    vec3 point_pos_xyz = vec3(ubo.pointLightPos.x, ubo.pointLightPos.y, ubo.pointLightPos.z);
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(point_pos_xyz - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = ubo.pointLightColor.w * point_rgb * diff * objectColor;

    // Combine (for now, ambient + diffuse; add specular next)
    vec3 result = ambient + diffuse;
    outColor = vec4(result, texColor.a);
}