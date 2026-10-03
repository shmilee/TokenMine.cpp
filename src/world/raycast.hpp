// Voxel raycast (DDA) + placement validation.
//
// The raycast is purely geometric: it returns the first NON-AIR cell the
// ray enters. It never reads block properties — interaction rules (can this
// block be broken? can a block be placed here?) live with the caller and are
// data-driven via BlockProperties.
//
// Out-of-range Y and missing chunks read as air through World::getBlock, so
// a ray simply flies out of the world without a hit.
#pragma once

#include <optional>

#include "core/math/vec3.hpp"
#include "world/coordinates.hpp"
#include "world/world.hpp"

namespace mc {

class BlockRegistry;

// First non-air block hit by the ray.
struct BlockHit {
    BlockPos block;    // the hit cell (world coordinates)
    Vec3 normal;       // outward normal of the face the ray hit (+/- axis)
    float distance;    // ray parameter t at the hit
};

// DDA voxel traversal (Amanatides & Woo, 1987). `origin` is the ray start
// (e.g. the player's eye); `direction` must be a unit vector.
//
// Semantics:
//   - The cell containing the origin is NOT tested: a ray does not hit the
//     block it starts inside (Minecraft behaves the same).
//   - The first cell entered after crossing a grid plane is tested; the
//     first non-air one wins.
//   - Hits at exactly `maxDistance` count; beyond it, no hit.
//   - A zero direction or non-positive maxDistance never hits.
std::optional<BlockHit> raycastBlock(const World& world, Vec3 origin,
                                     Vec3 direction, float maxDistance);

// Can a block be placed at `cell`?
//   - the cell must be inside the world's Y range [0, kWorldHeight);
//   - the cell must not already contain a solid block (air and non-solid
//     blocks such as water may be replaced);
//   - the cell must not overlap the player's AABB. The overlap test uses the
//     same strict convention as the collision code (`min < c+1 && max > c`
//     per axis), so a player standing flush on a face does not block the
//     cell below, but a player whose feet are inside the cell does.
bool canPlaceAt(const World& world, const BlockRegistry& blocks, BlockPos cell,
                const Vec3& playerMin, const Vec3& playerMax);

} // namespace mc
