#version 330 core

// V2-4: the rain drop's fragment — a flat unlit white-blue
// (kRainDropColor* / kRainDropAlpha, core/weather: (200, 210, 235) /
// 255 at 0.35 alpha) — the design's no-texture decision. The drop is a
// thin 0.02 x 0.5 bar quad (the vertex stage), double-sided (culling
// off at the draw), blended in the caller's blend section.

uniform vec4 u_color;

out vec4 fragColor;

void main() {
    fragColor = u_color;
}
