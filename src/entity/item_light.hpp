// PRE-6: the world light a dropped item is drawn with.
//
// Drops used to render at full sky light no matter where they fell
// (the geometry bakes skyLight = 1.0 / blockLight = 0.0): a stone cube
// dropped in an unlit cave glowed like daylight. This is the pure
// function behind the fix — it samples the light of the cell the drop
// sits in (the AABB CENTER cell, the Minecraft "entity cell" convention)
// and returns it normalized to 0..1, ready for the vertex shader's
// per-draw override (u_useDropLight / u_dropSky / u_dropBlock in
// basic.vert). The caller samples it once per drop per frame (no cache:
// light changes — a torch placed, day turning — are picked up
// automatically; the cost is two World reads per drop per frame,
// negligible at player-scale drop counts).
//
// The values are the RAW light (0..15 / 15), NOT day-scaled: the
// fragment's existing formula (basic.frag: max(v_skyLight * u_dayFactor,
// v_blockLight)) applies the day/night scale to sky light and keeps
// block light un-scaled (the glowstone-shines-at-night discipline), so a
// drop behaves exactly like the world around it.
#pragma once

#include <utility>

namespace mc {

class World;
class ItemEntity;

// The (sky, block) light of the drop's AABB-center cell, each normalized
// to 0..1 (light level / kMaxLight). center = position + height/2
// (position is the AABB BOTTOM center). The cell is the per-axis floor of
// the center (hard rule 1: floor semantics for negative coordinates —
// std::floor, not truncation). Out-of-range / never-created cells take
// the World getters' documented fallback (above/around the world: sky
// 15, block 0; below the world: both 0).
std::pair<float, float> itemDropLight(const World& world,
                                      const ItemEntity& item);

} // namespace mc
