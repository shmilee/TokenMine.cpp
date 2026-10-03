#version 330 core

in vec2 v_corner;

uniform vec4 u_color;

out vec4 fragColor;

void main() {
    // Round sprite: keep the inscribed circle of the quad.
    if (dot(v_corner, v_corner) > 0.25) {
        discard;
    }
    fragColor = u_color;
}
