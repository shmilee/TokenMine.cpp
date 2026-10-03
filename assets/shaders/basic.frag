#version 330 core

in vec2 v_uv;
in float v_skyLight;
in float v_blockLight;
in float v_faceShade;

uniform sampler2D u_texture;
uniform int u_useTexture;
uniform vec3 u_color;
// Day/night scale for sky light (1 = day, 0 = deep night). Block light is
// intentionally unaffected — glowstone shines at night.
uniform float u_dayFactor;

out vec4 fragColor;

void main() {
    // GLSL requires a boolean condition for ?: (no implicit int conversion).
    // Textured blocks (water, glass, leaves) carry their per-pixel alpha in
    // the texture; everything else is opaque.
    vec4 texel = texture(u_texture, v_uv);
    vec3 base = (u_useTexture != 0) ? texel.rgb : u_color;
    float alpha = (u_useTexture != 0) ? texel.a : 1.0;
    // Lightmap lighting: the brighter of day-scaled sky light and block
    // light (each 0..1), with per-face shading and a small ambient floor so
    // fully dark areas stay distinguishable from the void background.
    const float kAmbient = 0.03;
    float light = max(v_skyLight * u_dayFactor, v_blockLight);
    fragColor = vec4(base * (kAmbient + (1.0 - kAmbient) * light * v_faceShade), alpha);
}
