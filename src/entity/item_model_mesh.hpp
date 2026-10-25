// CRAFT-2: item-model geometry — the CPU side of the per-item 3D model.
//
// makeItemModelGeometry builds the vertex/index data for one item model:
// each element box -> the six face quads (the kCubeFaces corner
// convention generalized to an arbitrary from/to box — the winding and
// normals are the block-cube's, so the shader's faceShade convention
// top 1.0 / ±X 0.8 / ±Z 0.6 / bottom 0.5 applies exactly like the
// block-cube hand and the dropped cube). The geometry is centered on the
// origin in the unit-cube model space (the from/to corners in [0, 1]^3
// are shifted by -0.5, like the icon-quad hand), with full sky light
// baked in (the dropped-item-cube convention: the shader's day/night
// scale then shades it like the world).
//
// The mesh is DOUBLE-SIDED by construction: it is one quad per face with
// the geometric face normal, and the caller draws it with culling OFF
// (every face renders from any hand-spin / drop-rotation state — the
// beacon-beam / cross-particle convention). There is no per-face culling
// (the boxes may overlap — an overlapping element's interior faces are
// depth-occluded by the nearer surface). Content authoring convention
// (the v1 models): element slabs on each axis are NESTED (strict
// overlap) or DISJOINT, never sharing a face plane with an overlapping
// area — face-coincident quads z-fight, and the parser does not police
// it (the shipped data is the only author).
//
// The caller scales the geometry with the model matrix (the hand: the
// icon-hand scale; the world drop: ItemEntity::kSize), so one canonical
// geometry serves all three item contexts.
#pragma once

#include <cstdint>
#include <vector>

#include "block/cube_face_table.hpp"
#include "entity/item_mesh.hpp"
#include "item/item.hpp"

namespace mc {

ItemMeshGeometry makeItemModelGeometry(const Item::ItemModel& model);

} // namespace mc
