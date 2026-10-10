#version 330 core

in vec2 v_uv;

uniform sampler2D u_texture;
uniform int u_useTexture;
// Textured draws (font glyphs, item icons, crack overlay): the final rgb
// is the texture's rgb modulated by u_color.rgb (Phase 36: before, u_color
// modulated ONLY the alpha, so colored text — e.g. the death screen's red
// "You died!" — silently rendered as the texture's white; every textured
// draw passes a white u_color.rgb, so white text, icons and cracks are
// bit-identical to the old behavior), and the alpha is modulated by
// u_color.a. Untextured draws (solid quads, the outline) use u_color
// as-is.
uniform vec4 u_color;

out vec4 fragColor;

void main() {
    vec4 texel = texture(u_texture, v_uv);
    fragColor = (u_useTexture != 0)
        ? vec4(texel.rgb * u_color.rgb, u_color.a * texel.a)
        : u_color;
}
