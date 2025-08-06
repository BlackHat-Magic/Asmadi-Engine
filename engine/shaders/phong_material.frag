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
    vec4 ambient_color[64];
    vec4 pointLightPos[64];
    vec4 pointLightColor[64];
    vec4 viewPos;
} ubo;

void main() {
    vec4 texColor = texture(texture1, TexCoord);
    vec3 objectColor = texColor.rgb * fragColor;
    vec3 view_xyz = vec3(ubo.viewPos.x, ubo.viewPos.y, ubo.viewPos.z);
    vec3 norm = normalize(Normal);

    vec3 ambient_sum = vec3(0.0);
    vec3 diffuse_sum = vec3(0.0);
    vec3 specular_sum = vec3(0.0);

    // ambient lights
    for (int i = 0; i < 64; i++) {
        float brightness = ubo.ambient_color[i].w;
        if (brightness <= 0.0) {
            break;
        }
        vec3 ambient_rgb = ubo.ambient_color[i].rgb;
        ambient_sum += brightness * ambient_rgb * objectColor;
    }

    // point light diffuse
    for (int i = 0; i < 64; i++) {
        float brightness = ubo.pointLightColor[i].w;
        if (brightness <= 0.0) {
            break;
        }
        vec3 point_xyz = ubo.pointLightPos[i].xyz;
        vec3 light_dir = normalize(point_xyz - FragPos);
        vec3 point_rgb = ubo.pointLightColor[i].rgb;

        // diffuse
        float diff = max(dot(norm, light_dir), 0.0);
        diffuse_sum += brightness * point_rgb * diff * objectColor;

        // specular
        vec3 view_dir = normalize(view_xyz - FragPos);
        vec3 reflection_dir = reflect(-light_dir, norm);
        float spec = pow(max(dot(view_dir, reflection_dir), 0.0), 256);
        specular_sum += 0.5 * spec * point_rgb;
    }

    // Combine (for now, ambient + diffuse; add specular next)
    vec3 result = ambient_sum + diffuse_sum + specular_sum;
    outColor = vec4(result, texColor.a);
}