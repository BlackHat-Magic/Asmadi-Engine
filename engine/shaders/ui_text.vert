#version 450
layout(location=0) in vec2 aPos;   // pixel coords
layout(location=1) in vec2 aUV;
layout(location=2) in vec4 aColor;

layout(location=0) out vec2 vUV;
layout(location=1) out vec4 vColor;

// Push constants unsupported in current SDL_gpu; encode screen size via vertex transform.
// We’ll normalize in shader using built-in viewport: do manual pixel->NDC here with uniforms.
// For simplicity, derive NDC by reading gl_FragCoord? Not available. We pass via special constants.
// Use an implicit contract: we will set viewport to window size already; convert to NDC in shader
// by pulling from a uniform is ideal, but SDL_gpu fragment/vertex uniform push is already used by 3D.
// So here we put screen size in a separate uniform buffer bound at set=0, binding=0.

layout(std140, set=0, binding=0) uniform ScreenUBO {
    vec2 screenSize;
} ubo;

void main() {
    vec2 ndc = (aPos / ubo.screenSize) * 2.0 - 1.0;
    ndc.y = -ndc.y;
    gl_Position = vec4(ndc, 0.0, 1.0);
    vUV = aUV;
    vColor = aColor;
}