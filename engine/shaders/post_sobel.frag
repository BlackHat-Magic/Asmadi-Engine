#version 450

layout(location = 0) in vec2 TexCoord;

layout(location = 0) out vec4 outColor;

layout(set = 2, binding = 0) uniform sampler2D sceneTexture;  // Set 2 for fragment sampler

layout(std140, set = 3, binding = 0) uniform PostUBO {  // Set 3 for fragment UBO
    vec2 texelSize;
} ubo;

float luminance(vec3 color) {
    return dot(color, vec3(0.2126, 0.7152, 0.0722));
}

void main() {
    // Horizontal Sobel
    float gx = 
        -1.0 * luminance(texture(sceneTexture, TexCoord + vec2(-ubo.texelSize.x, -ubo.texelSize.y)).rgb) +
        -2.0 * luminance(texture(sceneTexture, TexCoord + vec2(-ubo.texelSize.x, 0.0)).rgb) +
        -1.0 * luminance(texture(sceneTexture, TexCoord + vec2(-ubo.texelSize.x, ubo.texelSize.y)).rgb) +
         1.0 * luminance(texture(sceneTexture, TexCoord + vec2(ubo.texelSize.x, -ubo.texelSize.y)).rgb) +
         2.0 * luminance(texture(sceneTexture, TexCoord + vec2(ubo.texelSize.x, 0.0)).rgb) +
         1.0 * luminance(texture(sceneTexture, TexCoord + vec2(ubo.texelSize.x, ubo.texelSize.y)).rgb);

    // Vertical Sobel
    float gy = 
        -1.0 * luminance(texture(sceneTexture, TexCoord + vec2(-ubo.texelSize.x, -ubo.texelSize.y)).rgb) +
        -2.0 * luminance(texture(sceneTexture, TexCoord + vec2(0.0, -ubo.texelSize.y)).rgb) +
        -1.0 * luminance(texture(sceneTexture, TexCoord + vec2(ubo.texelSize.x, -ubo.texelSize.y)).rgb) +
         1.0 * luminance(texture(sceneTexture, TexCoord + vec2(-ubo.texelSize.x, ubo.texelSize.y)).rgb) +
         2.0 * luminance(texture(sceneTexture, TexCoord + vec2(0.0, ubo.texelSize.y)).rgb) +
         1.0 * luminance(texture(sceneTexture, TexCoord + vec2(ubo.texelSize.x, ubo.texelSize.y)).rgb);

    float edge = sqrt(gx * gx + gy * gy);
    outColor = vec4(vec3(edge), 1.0);  // Grayscale edges; adjust threshold or invert as needed
}