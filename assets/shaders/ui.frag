#version 330 core

in vec2 v_uv;

uniform sampler2D u_texture;
uniform int u_useTexture;
// Textured draws (font glyphs, item icons, crack overlay): the texture's
// rgb is the color and its alpha is modulated by u_color.a. Untextured
// draws (solid quads, the outline) use u_color as-is.
uniform vec4 u_color;

out vec4 fragColor;

void main() {
    vec4 texel = texture(u_texture, v_uv);
    fragColor = (u_useTexture != 0)
        ? vec4(texel.rgb, u_color.a * texel.a)
        : u_color;
}
