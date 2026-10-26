#version 330 core

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_uv;
layout(location = 3) in float a_skyLight;
layout(location = 4) in float a_blockLight;
layout(location = 5) in vec2 a_uvLocal;
layout(location = 6) in float a_tiled;
layout(location = 7) in float a_water;

uniform mat4 u_model;
uniform mat4 u_view;
uniform mat4 u_proj;
// PRE-3 water ripples: the frame-locked wander of the water-top sample
// window. The fragment stage applies it inside the TILED branch (the
// fract wrap there keeps it tile-local); u_tileSize stays fragment-side.
uniform int u_ripplesEnabled;
uniform float u_ripplePhase;

out vec2 v_uv;
out vec2 v_uvLocal;
out float v_tiled;
out float v_skyLight;
out float v_blockLight;
out float v_faceShade;
// PRE-2 distance fog: the fragment's depth in view space (see the
// independent expression at the bottom of main()).
out float v_viewDepth;
// PRE-3 water ripples: the sample-window wander (tile units) the
// fragment applies INSIDE its tiled branch. (0,0) for every vertex that
// is not a ripples-on water top face — fract(x + (0,0)) is x bit-for-bit,
// so the tiled path (greedy quads) and the whole non-water pipeline stay
// bit-identical.
out vec2 v_rippleOff;

void main() {
    // Model is rigid (translation + rotation only) in Phase 1, so the
    // model rotation matrix is the correct normal transform.
    vec3 n = mat3(u_model) * a_normal;
    v_uv = a_uv;
    v_uvLocal = a_uvLocal;
    // PRE-3 water ripples: a deterministic sub-texel wander of the
    // water TOP faces' sample window (the white-noise water tile shimmers
    // when its window moves; NEAREST filtering = crisp texel steps, no
    // blending). Computed here (per vertex, world position), applied in
    // the fragment's tiled branch (where the fract wrap keeps it
    // tile-local). Water top faces are emitted tiled (the mesher), so the
    // wander can never leak into a neighbouring atlas tile. (0,0) for
    // every other vertex — the ripples-off pipeline bit-identical.
    const float kRippleWaveNumber = 0.8;   // rad/block, spatial frequency
    const float kRippleAmplitude  = 0.06;  // tile units (~1 texel of 16)
    v_rippleOff = vec2(0.0);
    if (u_ripplesEnabled != 0 && a_water > 0.5) {
        vec2 wp = (u_model * vec4(a_position, 1.0)).xz;
        v_rippleOff =
            vec2(sin(wp.x * kRippleWaveNumber + u_ripplePhase),
                 cos(wp.y * kRippleWaveNumber - u_ripplePhase)) *
            kRippleAmplitude;
    }
    v_tiled = a_tiled;
    v_skyLight = a_skyLight;
    v_blockLight = a_blockLight;
    // Classic per-face shading (the Minecraft lightmap look): a lightmap
    // value alone renders flat, so each face orientation gets a fixed
    // multiplier. Chunk models are pure translations, so the world normal
    // equals the local normal.
    if (n.y > 0.5) {
        v_faceShade = 1.0;
    } else if (n.y < -0.5) {
        v_faceShade = 0.5;
    } else if (abs(n.x) > 0.5) {
        v_faceShade = 0.8;
    } else {
        v_faceShade = 0.6;
    }
    gl_Position = u_proj * u_view * u_model * vec4(a_position, 1.0);
    // PRE-2 distance fog: the view-space depth on an INDEPENDENT
    // expression tree. The gl_Position line above is byte-identical to the
    // pre-fog shader — regrouping its float multiply order would shift
    // ULPs and could flip coplanar-culling edges and depth-tie pixels, so
    // the fog never touches it. The camera looks down -Z (right-handed
    // OpenGL, hard rule 5), so the forward depth is -z. The 1e-7-class
    // difference between the two trees is invisible: it only feeds the
    // aesthetic fog ramp, and it is deterministic across runs.
    v_viewDepth = -(u_view * u_model * vec4(a_position, 1.0)).z;
}
