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
#include <string>
#include <vector>

#include "core/math/vec3.hpp"
#include "world/chunk.hpp"
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

// DDA traversal for the FLUID-BUCKET target (Phase 47): the first cell
// that is a non-air BLOCK or carries a fluid level. Same convention as
// raycastBlock (the origin cell is not tested, hits at exactly
// maxDistance count, a zero direction never hits) with one difference:
// a solid wall OCCLUDES the ray (Minecraft: the bucket uses block
// targeting — you cannot scoop through a wall), while raycastBlock
// passes through fluids. The caller then reads the returned cell's
// fluid level: kMaxFluidLevel (8, a source) is the scoop, anything else
// (a block, or a flowing cell) means "no action".
std::optional<BlockPos> raycastFluid(const World& world, Vec3 origin,
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

// Planting placement restriction (Phase 53): true when the cell DIRECTLY
// BELOW `cell` holds one of the blocks named in `plantOn` (the soil). An
// empty `plantOn` is UNRESTRICTED (returns true) — the caller only invokes
// it for placeable blocks, but the empty case is safe by construction. This
// is a placement-time gate, independent of the growth-time `grows_on` gate
// (Phase 50). The names are resolved through the registry at call time
// (placement is not a hot path — one per right-click).
bool canPlantAt(const World& world, const BlockRegistry& blocks, BlockPos cell,
                const std::vector<std::string>& plantOn);

// FLUID-4: resolve the POUR target cell for a bucket that carries `kind`
// (the phase-47 pour accepted only an AIR face neighbor — pouring against
// a water surface was a documented no-op; this adds the Minecraft
// pour-onto-water-surface). Candidate selection, in order:
//   - when the BLOCK ray (`blockHit`, the plain raycast that passes
//     through fluids) hit, the candidate is the hit face's NEIGHBOR —
//     the placement convention: a floor under a 1-deep pool makes the
//     top water cell the candidate, so "aim at the pool" works;
//   - when the block ray hit NOTHING (open water beyond block reach),
//     the candidate is the FLUID ray's own hit cell (`fluidHit`, walls
//     already excluded by the empty block hit).
// The candidate must pass `canPlaceAt` (Y range, no solid block, no
// player overlap — shared with the legacy path, which it keeps
// bit-identical) and hold EITHER:
//   (a) air without fluid -> the legacy placement, or
//   (b) fluid of `kind` below `kMaxFluidLevel` -> the new pour onto a
//       water surface (a flowing cell becomes the source).
// An already-source cell (level 8) or a different fluid kind is a no-op
// (Minecraft: the bucket is kept); nullopt = the click falls through.
std::optional<BlockPos> bucketPourTarget(const World& world,
                                         const BlockRegistry& blocks,
                                         const std::optional<BlockHit>& blockHit,
                                         const std::optional<BlockPos>& fluidHit,
                                         FluidKind kind,
                                         const Vec3& playerMin,
                                         const Vec3& playerMax);

// Slab test of a ray against an axis-aligned box (Phase 20, mob targeting).
// Returns the ray parameter t at ENTRY, or -1 for a miss. `dir` does not
// need to be normalized (t is then in dir-units); the game passes the unit
// camera forward, so t is in blocks. t = 0 when the origin is inside the
// box (an "immediate" hit); a box fully behind the origin misses.
float raycastAabb(Vec3 origin, Vec3 dir, Vec3 boxMin, Vec3 boxMax);

// Line of sight (Phase 38, skeleton shooting): true when no non-air cell
// blocks the segment from `from` to `to` — the same first-non-air-cell
// predicate the arrows fly through, so the skeleton never fires a shot a
// wall would stop (previously it shot through walls and the arrow
// vanished on impact). The cell containing `from` is exempt (the
// raycastBlock convention: a ray does not hit the block it starts in).
bool hasLineOfSight(const World& world, Vec3 from, Vec3 to);

} // namespace mc
