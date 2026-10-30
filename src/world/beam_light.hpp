// LGT-4: the world light a beacon beam is drawn with.
//
// The beam used to render as a constant-white unlit column (the geometry
// bakes skyLight = 0.0 / blockLight = 1.0): bright at deep night, and
// untouched by nearby glowstone or torches. This is the pure function
// behind the fix — it samples the light of the beam's LOWEST AIR CELL,
// (x, y+1, z) — the cell directly above the beacon block, where the beam
// model begins (the draw positions it on the block's top face). The
// beacon's OWN cell is an opaque solid and always reads (0, 0): sampling
// it would render the beam at the ambient floor in every environment
// (10-27 gate 119 E2E: on/off beam diff 0 px in a scene whose camera
// frames the column — the cell selection was the defect). Returns the
// normalized 0..1 light, ready for the vertex shader's per-draw override
// (u_useLightOverride / u_lightOverrideSky / u_lightOverrideBlock in
// basic.vert — the PRE-6 drop-light uniforms, renamed neutral for the two
// consumers). The caller samples it once per active beacon per frame (no
// cache: light changes — a glowstone placed, day turning — are picked up
// automatically; the cost is two World reads per beacon per frame,
// negligible at player-scale beacon counts).
//
// The values are the RAW light (0..15 / 15), NOT day-reduced: the
// fragment's existing formula (basic.frag, LGT-3 + UX-1: max(v_skyLight -
// u_nightRetreat * (1.0 - u_dayFactor), v_blockLight)) applies the
// day/night affine retreat WITH the configured moonlight floor
// (u_nightRetreat = (15 - L) / 15, L = render.night_sky_level, default
// kNightSkyLevel 7 — 8/15 at the default) to sky light and keeps block
// light un-reduced (the glowstone-shines-at-night discipline), so a beam
// behaves exactly like the world around it (moonlit at deep night at the
// default floor, bright next to glowstone, full white at open-sky noon =
// the legacy constant look there).
#pragma once

#include <utility>

namespace mc {

class World;

// The (sky, block) light of the air cell directly above the beacon block
// at (x, y, z) — i.e. (x, y+1, z), the beam's lowest cell — each
// normalized to 0..1 (light level / kMaxLight). Out-of-range /
// never-created cells take the World getters' documented fallback (above
// / around the world: sky 15, block 0; below the world: both 0). Pure
// function of its arguments — no dayFactor (the fragment applies the
// day/night reduction), same world + beacon -> same values.
std::pair<float, float> beamLightSample(const World& world, int x, int y,
                                        int z);

} // namespace mc
