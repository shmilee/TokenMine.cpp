#version 330 core

in vec2 v_uv;
in vec2 v_uvLocal;
in float v_tiled;
in float v_skyLight;
in float v_blockLight;
in float v_faceShade;
// PRE-2 distance fog: the vertex's view-space depth (independent
// expression tree in basic.vert — gl_Position is untouched).
in float v_viewDepth;
// PRE-3 water ripples: the vertex's sample-window wander (tile units,
// basic.vert). (0,0) for every vertex except a ripples-on water top face
// — fract(x + (0,0)) is x bit-for-bit, so this changes nothing for the
// tiled greedy quads or any non-water geometry.
in vec2 v_rippleOff;
// V3-15: the portal's UV scroll (the flowing read). u_portalTile = the
// nether portal's tile index (tile units, the tile's TOP-LEFT corner);
// u_portalAnim = the config gate (0 = the phase term is never added =
// the pre-V3-15 pipeline bit-for-bit); u_portalPhase = the frame-locked
// scroll in [0, 1) tile units (the texture is 2-periodic horizontally,
// so the in-tile x scroll wraps seamlessly).
uniform vec2 u_portalTile;
uniform int u_portalAnim;
uniform float u_portalPhase;

uniform sampler2D u_texture;
uniform int u_useTexture;
uniform vec3 u_color;
// Solid-color (u_useTexture == 0) alpha: 1.0 for the opaque draws (arrows,
// the Phase 1+ world), lowered for the translucent beacon beam (Phase 83).
// The textured path always uses the texture's per-pixel alpha.
uniform float u_alpha;
// Day/night factor for sky light (1 = noon, 0 = deep night). LGT-3: the
// sky term retreats AFFINELY with it (see the lightmap line in main())
// instead of scaling; block light is intentionally unaffected —
// glowstone shines at night.
uniform float u_dayFactor;
// UX-1 (the configurable deep-night sky floor): the retreat distance in
// the 0..1-normalized sky units, (15 - L) / 15 with L = the configured
// floor (render.night_sky_level, code default 7 = the LGT-3 moonlight
// floor — at the default the host uploads 8/15, the pre-UX-1 named
// constant bit-for-bit). u_ambient is the ambient floor of the light
// formula (the host uploads 0.03 at any nonzero floor; 0.0 for L = 0 =
// the true all-black night, no residual glow). Both are PER-RUN
// constants (config-driven, no per-draw variation): set once at init
// (bind -> set -> unbind).
uniform float u_nightRetreat;
uniform float u_ambient;
// UX-9c (the configurable fully-occluded floor): the ambient value used
// when the effective light is ~0 (caves / deep water / sealed rooms —
// the baked light is exactly 0 at every vertex there). The host uploads
// render.cave_ambient (code default kAmbientFloor — at the default
// night floor both uploaded values are 0.03f, so the presentation
// formula is bit-identical to the pre-UX-9c pipeline). Like
// u_nightRetreat / u_ambient it is a PER-RUN constant (config-driven,
// no per-draw variation): set once at init (bind -> set -> unbind).
uniform float u_caveAmbient;
// One tile in UV space (1/columns, 1/rows of the atlas). Only read on the
// tiled (greedy-merged chunk quad) path.
uniform vec2 u_tileSize;
// PRE-2 distance fog: the streaming disc edge dissolves into the horizon.
// u_fogEnabled is per-draw uniform state (1 = on, 0 = off — no fragment
// divergence). u_fogColor is the same-frame sky color. The
// u_fogEnd > u_fogStart guard defends against a degenerate range (the
// caller turns the fog off first; this is belt and braces).
uniform int u_fogEnabled;
uniform float u_fogStart;
uniform float u_fogEnd;
uniform vec3 u_fogColor;
// PRE-5 alpha cutout: the config gate (render.alpha_test, set once per
// frame on the basic program). 1 = the alpha-0 holes of cutout textures
// (leaves, cross plants, the glass frame, the item-icon backgrounds)
// DISCARD their fragment — no color write AND no depth write (with the
// depth mask on in the cutout pass / off in the blend pass). 0 = the
// pre-PRE-5 pipeline bit-for-bit (the branch never executes).
uniform int u_discardEnabled;

out vec4 fragColor;

void main() {
    // GLSL requires a boolean condition for ?: (no implicit int conversion).
    // Textured blocks (water, glass, leaves) carry their per-pixel alpha in
    // the texture; everything else is opaque.
    // Tiled path (Phase 18 greedy meshing): v_uv is the atlas TILE INDEX
    // and v_uvLocal is the position in tile units — a merged W x H quad
    // spans W x H repetitions of that one tile. The fractional part wraps
    // each cell back onto the tile; the legacy path samples v_uv (atlas
    // space) untouched, so non-chunk geometry is pixel-identical.
    vec2 sampleUv = v_uv;
    if (v_tiled > 0.5) {
        // The wander joins the in-tile coordinate BEFORE the fract wrap
        // (PRE-3): the window moves inside this tile only — the water top
        // face is emitted tiled by the mesher, and the wrap seam is
        // water-on-water (the tile is generated periodic). With
        // v_rippleOff = (0,0) this line is bit-identical to the pre-PRE-3
        // (v_uv + fract(v_uvLocal)) * u_tileSize.
        // V3-15: the portal scroll joins the in-tile coordinate before
        // the fract wrap (the ripple's twin — the wander is (0,0) for
        // every non-portal tile and the phase branch is gated, so the
        // pre-V3-15 pipeline stays bit-identical).
        vec2 inTile = v_uvLocal + v_rippleOff;
        if (u_portalAnim != 0 &&
            abs(v_uv.x - u_portalTile.x) < 0.5 &&
            abs(v_uv.y - u_portalTile.y) < 0.5) {
            inTile.x += u_portalPhase;
        }
        sampleUv = (v_uv + fract(inTile)) * u_tileSize;
    }
    vec4 texel = texture(u_texture, sampleUv);
    vec3 base = (u_useTexture != 0) ? texel.rgb : u_color;
    float alpha = (u_useTexture != 0) ? texel.a : u_alpha;
    // PRE-5 alpha cutout: discard the pure holes (alpha 0) — the
    // translucent values (water 200/255 = 0.78, the glass core 220/255 =
    // 0.86, the solid-color u_alpha draws >= 0.5) stay, so the fix is
    // bucket-independent and the water / glass core / beam / cloud solid
    // colors are untouched. With u_discardEnabled = 0 the branch is never
    // taken and the pipeline is bit-identical to pre-PRE-5.
    if (u_discardEnabled != 0 && alpha < 0.5) { discard; }
    // Lightmap lighting: the brighter of the EFFECTIVE sky light and the
    // block light (each 0..1), with per-face shading and a small ambient
    // floor so fully dark areas stay distinguishable from the void
    // background.
    // LGT-3 (the night light retreat) + UX-1 (the configurable floor):
    // the sky term is an affine retreat of the stored noon field, not a
    // scale. v_skyLight is the noon (source level 15) field normalized to
    // 0..1; the source level behind u_dayFactor is S = L + (15 - L) *
    // u_dayFactor with L = the configured deep-night floor
    // (render.night_sky_level, code default 7 — the LGT-3 Minecraft
    // moonlight floor, the 10-27 user verdict) — and the sky field is
    // affine in its source level (world/lighting.hpp — every sky source
    // shares one level, the per-cell costs are source-independent), so
    // the effective level is max(stored - (15 - S), 0): in these
    // normalized values max(v_skyLight - u_nightRetreat *
    // (1.0 - u_dayFactor), 0.0) with u_nightRetreat = (15 - L) / 15
    // (default 8/15). The lit region shrinks toward the cave mouths /
    // canopy gaps along the stored = 15 - S contour as night falls — the
    // Minecraft-style retreat, with zero re-propagation. v_blockLight is
    // always >= 0, so the outer max also supplies the 0 clamp. Fixed
    // points: at u_dayFactor 1 the term is v_skyLight - 0.0 = v_skyLight
    // bit-for-bit (the noon identity — the gate S1 root, for ANY floor);
    // at 0 the open sky (1.0) reads 1.0 - u_nightRetreat = L / 15
    // (default 7/15 — the moonlight floor, never all black; L = 0 IS the
    // all-black night, and u_ambient goes to 0.0 with it — deep night at
    // the default floor is the moonlit 7/15 of the stored level, that IS
    // the feature vs the pre-LGT-3 scale's all-0 night).
    float light = max(v_skyLight - u_nightRetreat * (1.0 - u_dayFactor),
                      v_blockLight);
    // UX-9c: the fully-occluded floor (light <= 0.001 = caves / deep
    // water / sealed rooms, where the baked light is exactly 0 at every
    // vertex): u_caveAmbient replaces u_ambient as the floor. At the
    // code default both are 0.03f, so `amb` is 0.03f either way = the
    // expression keeps identical operands and order = bit-identical
    // (the gate's S1 root). A fragment whose interpolated light
    // straddles the 0.001 boundary selects per pixel — a pure ALU
    // select, no correctness issue.
    float amb = light <= 0.001f ? u_caveAmbient : u_ambient;
    vec3 rgb = base * (amb + (1.0 - amb) * light * v_faceShade);
    // PRE-2 distance fog: a linear ramp toward the same-frame sky color
    // from u_fogStart to u_fogEnd. Near geometry clamps to fogT = 0 and
    // mix(x, y, 0.0) is x bit-exactly for non-negative colors, so
    // enabling fog leaves near pixels bit-identical. With
    // u_fogEnabled = 0 the branch is never taken and the pipeline is
    // bit-identical to the pre-fog shader.
    if (u_fogEnabled != 0 && u_fogEnd > u_fogStart) {
        float fogT = clamp((v_viewDepth - u_fogStart) /
                           (u_fogEnd - u_fogStart), 0.0, 1.0);
        rgb = mix(rgb, u_fogColor, fogT);
    }
    fragColor = vec4(rgb, alpha);
}
