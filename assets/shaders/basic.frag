#version 330 core

in vec2 v_uv;
in float v_skyLight;
in float v_blockLight;
in float v_faceShade;

uniform sampler2D u_texture;
uniform int u_useTexture;
uniform vec3 u_color;

out vec4 fragColor;

void main() {
    // GLSL requires a boolean condition for ?: (no implicit int conversion).
    vec3 base = (u_useTexture != 0) ? texture(u_texture, v_uv).rgb : u_color;
    // Lightmap lighting: the brighter of sky and block light (each 0..1),
    // with per-face shading and a small ambient floor so fully dark areas
    // stay distinguishable from the void background.
    const float kAmbient = 0.03;
    float light = max(v_skyLight, v_blockLight);
    fragColor = vec4(base * (kAmbient + (1.0 - kAmbient) * light * v_faceShade), 1.0);
}
