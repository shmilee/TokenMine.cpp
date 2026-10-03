#version 330 core

in vec3 v_normal;
in vec2 v_uv;
in float v_light;

uniform sampler2D u_texture;
uniform int u_useTexture;
uniform vec3 u_color;
// Direction toward the light (normalized).
uniform vec3 u_lightDir;

out vec4 fragColor;

void main() {
    // GLSL requires a boolean condition for ?: (no implicit int conversion).
    vec3 base = (u_useTexture != 0) ? texture(u_texture, v_uv).rgb : u_color;
    float ndl = max(dot(normalize(v_normal), normalize(u_lightDir)), 0.0);
    // Placeholder lighting: simple directional + ambient. Replaced by the
    // real lighting system in Phase 7.
    float lit = 0.3 + 0.7 * ndl;
    fragColor = vec4(base * lit * v_light, 1.0);
}
