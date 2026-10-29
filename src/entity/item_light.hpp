// PRE-6: the world light a dropped item is drawn with.
//
// Drops used to render at full sky light no matter where they fell
// (the geometry bakes skyLight = 1.0 / blockLight = 0.0): a stone cube
// dropped in an unlit cave glowed like daylight. This is the pure
// function behind the fix — it samples the light of the cell the drop
// sits in (the AABB CENTER cell, the Minecraft "entity cell" convention)
// and returns it normalized to 0..1, ready for the vertex shader's
// per-draw override (u_useLightOverride / u_lightOverrideSky /
// u_lightOverrideBlock in basic.vert — the PRE-6 drop names, renamed
// neutral when the beacon beams joined the pattern, LGT-4). The caller
// samples it once per drop per frame (no cache:
// light changes — a torch placed, day turning — are picked up
// automatically; the cost is two World reads per drop per frame,
// negligible at player-scale drop counts).
//
// The values are the RAW light (0..15 / 15), NOT day-reduced: the
// fragment's existing formula (basic.frag, LGT-3: max(v_skyLight -
// (8.0/15.0) * (1.0 - u_dayFactor), v_blockLight)) applies the
// day/night affine retreat WITH the kNightSkyLevel 7 moonlight floor to
// sky light and keeps block light un-reduced (the
// glowstone-shines-at-night discipline), so a drop behaves exactly like
// the world around it (moonlit at deep night, never all black).
#pragma once

#include <utility>

#include "core/math/vec3.hpp"
#include "world/coordinates.hpp"

namespace mc {

class World;
class ItemEntity;

// The AABB center cell of an entity (the PRE-6 "entity cell" convention,
// shared by the drop light and the PRE-8 burning-mob flames): `position`
// is the AABB BOTTOM center (the Entity convention), the center is
// height/2 above it, and the cell is the per-axis FLOOR of the center
// (hard rule 1 — floor semantics for negative coordinates: an entity at
// x = -0.2 takes cell -1, not 0).
BlockPos entityCenterCell(const Vec3& position, float height);

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
