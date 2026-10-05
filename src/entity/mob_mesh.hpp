// Mob model geometry (Phase 20): the CPU side of a mob's box model.
//
// makeMobModelGeometry builds the vertex/index data for every box of a
// MobModel: the shared six-face table (same corner order and winding as
// blocks and dropped items — see cube_face_table.hpp), one texture tile on
// all six faces, full sky light baked in (the shader's day/night scale
// then shades the mob like the world: bright by day, dark by night — the
// same simplification as dropped items; mobs have no local light).
//
// Local space: the mob's BOTTOM-CENTER is the origin (boxes are positioned
// relative to it, in model units). The game draws the result with
// translate(position) * rotateY(facingYaw) — the camera yaw convention
// (yaw 0 = -Z), so the -Z face of a box is the mob's "front".
#pragma once

#include "entity/item_mesh.hpp"
#include "entity/mob_registry.hpp"

namespace mc {

ItemMeshGeometry makeMobModelGeometry(const MobModel& model,
                                      const AtlasLayout& layout);

} // namespace mc
