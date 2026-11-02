#version 330 core

// V2-4: the rain drop field — the cloud program's fourth precedent (a
// dedicated shader program; the basic/sky/ui/cloud programs are
// untouched). The host draws ONE static kRainDropCount-drop mesh (300
// bar quads, uploaded once at init — zero per-frame upload, the cloud
// quad's discipline): the per-drop SPAWN OFFSET (a_position, the same
// for the drop's four corners) + the bar-quad CORNER offset (a_normal)
// + the fall PHASE (a_uv.x, in [0, 1)). The per-frame state is TWO
// uniforms: u_boxOrigin (the 16^3 box's bottom corner, player-anchored)
// and u_fallOffset ((frame % 16) * fallSpeed, bounded in [0, 16) — the
// magnitude stays float32-exact over a long session; fallSpeed * period
// = boxSize makes the wrap seamless). The vertical fall is re-derived
// PER VERTEX here (the CPU oracle is rainDropPosition in core/weather —
// same frame, same seed, same position; the GLSL float same-driver
// premise, the cloud shader's).
//
// The fixed 15-float MeshVertex layout: only locations 0/1/2 are
// consumed (the ui.vert / sky.vert unlit-pass convention — the remaining
// VAO attributes are harmless).

layout(location = 0) in vec3 a_position; // the drop's spawn offset in the box
layout(location = 1) in vec3 a_normal;   // the bar-quad corner offset
layout(location = 2) in vec2 a_uv;       // uv.x = the fall phase [0, 1)

// These three names are required — Renderer::draw sets u_model / u_view
// / u_proj by name (src/renderer/renderer.cpp). u_model is the identity
// (the host passes it; the geometry is world-anchored by u_boxOrigin).
uniform mat4 u_model;
uniform mat4 u_view;
uniform mat4 u_proj;
// The box's bottom corner (the player's cell centered, kRainBoxHalf = 8
// blocks out on each axis).
uniform vec3 u_boxOrigin;
// (frame % kRainFallPeriodFrames) * kRainDropFallSpeed — bounded in
// [0, kRainBoxSize), the seamless-wrap term (see the file head).
uniform float u_fallOffset;

// kRainBoxSize (core/weather) — the box's side length in blocks.
const float kRainBoxSize = 16.0;

void main() {
    // The vertical fall: offset.y + the frame advance + the per-drop
    // phase (a boxSize-unit vertical offset), wrapped in the box (x/z
    // are constant — the drops fall straight down). CPU oracle:
    // rainDropPosition (std::fmod == GLSL mod for the positive values
    // here).
    float y = mod(a_position.y + u_fallOffset + a_uv.x * kRainBoxSize,
                  kRainBoxSize);
    vec3 world = u_boxOrigin + vec3(a_position.x, y, a_position.z) + a_normal;
    gl_Position = u_proj * u_view * u_model * vec4(world, 1.0);
}
