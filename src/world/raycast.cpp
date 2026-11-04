#include "world/raycast.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>

#include "block/block.hpp"
#include "block/block_registry.hpp"
#include "world/coordinates.hpp"

namespace mc {

namespace {
constexpr float kInf = std::numeric_limits<float>::infinity();

// A DDA walk result: the first hit cell, the outward normal of the hit
// face (it points back at the ray — the crossed grid-plane face for a
// whole-cell hit, the slab's interior surface for the V2-17 gap-fill
// partial-solid hit), and the ray parameter t at the hit.
struct DdaHit {
    BlockPos cell;
    Vec3 normal;
    float t;
};

// The hit-face normal for a grid-plane crossing on `axis`: opposite the
// travel direction on the crossed axis (the walk only crosses that axis
// when the direction component is non-zero, so the sign is defined).
Vec3 crossingNormal(int axis, const Vec3& direction) {
    if (axis == 0) {
        return {direction.x > 0.0f ? -1.0f : 1.0f, 0.0f, 0.0f};
    }
    if (axis == 1) {
        return {0.0f, direction.y > 0.0f ? -1.0f : 1.0f, 0.0f};
    }
    return {0.0f, 0.0f, direction.z > 0.0f ? -1.0f : 1.0f};
}

// V2-17 gap fill: the cell-local solid Y interval [bottom, top) of a
// PARTIAL-solid slab cell (the Minecraft partial-solid raycast semantics,
// wiki-verified 11-04): a stateful slab (model.state_count — the shipped
// stone_slab) by its per-cell state (the World's per-cell byte — unset ->
// the block's stateDefault, the solidRange / emitSlab twin): state 0 =
// [0, kSlabHalf) the bottom half, state 1 = [kSlabHalf, 1) the top half;
// a stateless slab (the bed family) = [0, kSlabHalf). nullopt = not a
// partial slab (a double slab is a FULL cell, a non-solid slab has no
// solid region to pass through, every other shape is a full cell) — the
// DDA treats nullopt cells as whole cells (the exact pre-gap-fill
// behavior, the bit-level red line).
std::optional<std::pair<float, float>> slabSolidRangeAt(
        const World& world, const BlockRegistry& blocks, BlockPos c) {
    const BlockID id = world.getBlock(c.x, c.y, c.z);
    if (id == BlockID::air() || !id.valid()) {
        return std::nullopt;
    }
    const Block& b = blocks.at(id);
    if (b.model.shape != BlockModel::Shape::Slab || !b.properties.solid) {
        return std::nullopt;
    }
    if (b.hasState()) {
        const int st = world.getBlockState(c.x, c.y, c.z);
        const int s = (st != Chunk::kBlockStateUnset) ? st : b.stateDefault();
        if (s == 1) {
            return std::pair {kSlabHalf, 1.0f}; // the top slab: solid in its top half
        }
        if (s >= 2) {
            return std::nullopt; // the double slab: a full cell
        }
    }
    return std::pair {0.0f, kSlabHalf}; // state 0 / the stateless slab: the bottom half
}

// Shared DDA walk (Amanatides & Woo, 1987): the first cell, after the
// one containing the origin, where `test(cell)` is true within
// maxDistance. Out-of-range Y and missing chunks read as air/no-fluid
// through World, so the walk simply flies out of the world without a
// hit. `direction` must be a unit vector.
//
// V2-17 gap fill: `slabRange` (nullptr = the whole-cell walk, the
// pre-gap-fill behavior) makes the walk PARTIAL-SOLID-AWARE for slab
// cells (the Minecraft raycast semantics): a ray that ENTERS a slab cell
// through its AIR half does not hit the cell's boundary face — it
// passes through, hitting either the slab's INTERIOR surface (the
// y = kSlabHalf plane — the bottom slab's top face for a descending ray,
// the top slab's bottom face for an ascending one; the hit cell is the
// slab cell itself, the normal points back into the air half the ray
// came from) or, when the ray leaves the cell laterally before reaching
// the surface (or travels away from the slab), the cell BEHIND. A ray
// that enters the SOLID half hits the boundary face exactly as before
// (and a double slab / every non-slab cell is a whole cell, unchanged).
// The shared path keeps the block ray + the fluid ray consistent (no
// per-caller special-casing).
std::optional<DdaHit> ddaFirstCell(
        Vec3 origin, Vec3 direction, float maxDistance,
        const std::function<bool(BlockPos)>& test,
        const std::function<std::optional<std::pair<float, float>>(BlockPos)>&
                slabRange = nullptr) {
    if (maxDistance <= 0.0f) {
        return std::nullopt;
    }
    if (direction == Vec3::zero()) {
        return std::nullopt;
    }

    // Integer cell of the origin (floor is exact for the negative-side
    // boundaries we meet in traversal).
    int x = static_cast<int>(std::floorf(origin.x));
    int y = static_cast<int>(std::floorf(origin.y));
    int z = static_cast<int>(std::floorf(origin.z));

    const int stepX = direction.x > 0.0f ? 1 : (direction.x < 0.0f ? -1 : 0);
    const int stepY = direction.y > 0.0f ? 1 : (direction.y < 0.0f ? -1 : 0);
    const int stepZ = direction.z > 0.0f ? 1 : (direction.z < 0.0f ? -1 : 0);

    // Ray parameter step per crossed grid plane; +inf when the ray is
    // parallel to that axis (the plane is never crossed).
    const float tDeltaX = stepX != 0 ? std::abs(1.0f / direction.x) : kInf;
    const float tDeltaY = stepY != 0 ? std::abs(1.0f / direction.y) : kInf;
    const float tDeltaZ = stepZ != 0 ? std::abs(1.0f / direction.z) : kInf;

    // Ray parameter of the first grid plane ahead on each axis. When the
    // origin sits exactly on a plane, the t is 0 and that neighbor cell is
    // tested first — the correct semantics for "just crossed the boundary".
    const float tMaxX0 =
        stepX > 0 ? (static_cast<float>(x + 1) - origin.x) / direction.x
        : stepX < 0 ? (static_cast<float>(x) - origin.x) / direction.x
                    : kInf;
    const float tMaxY0 =
        stepY > 0 ? (static_cast<float>(y + 1) - origin.y) / direction.y
        : stepY < 0 ? (static_cast<float>(y) - origin.y) / direction.y
                    : kInf;
    const float tMaxZ0 =
        stepZ > 0 ? (static_cast<float>(z + 1) - origin.z) / direction.z
        : stepZ < 0 ? (static_cast<float>(z) - origin.z) / direction.z
                    : kInf;
    float tMaxX = tMaxX0;
    float tMaxY = tMaxY0;
    float tMaxZ = tMaxZ0;

    // Walk cell by cell, always advancing the axis whose plane is closest.
    // At least one step is non-zero (direction != 0), so one tMax is finite
    // and grows each iteration: the loop always terminates via the reach
    // check.
    float t = 0.0f;
    while (true) {
        int axis;
        if (tMaxX <= tMaxY && tMaxX <= tMaxZ) {
            axis = 0;
        } else if (tMaxY <= tMaxZ) {
            axis = 1;
        } else {
            axis = 2;
        }
        switch (axis) {
            case 0:
                x += stepX;
                t = tMaxX;
                tMaxX += tDeltaX;
                break;
            case 1:
                y += stepY;
                t = tMaxY;
                tMaxY += tDeltaY;
                break;
            default:
                z += stepZ;
                t = tMaxZ;
                tMaxZ += tDeltaZ;
                break;
        }

        if (t > maxDistance) {
            return std::nullopt;
        }

        const BlockPos c {x, y, z};
        if (!test(c)) {
            continue;
        }
        if (slabRange) {
            const std::optional<std::pair<float, float>> sr = slabRange(c);
            if (sr) {
                const float bottom = sr->first;
                const float top = sr->second;
                // The entry point's cell-local Y (the crossing point on
                // the grid plane that led into this cell; the clamp kills
                // the ±ulp overshoot when the entry IS a y-plane crossing,
                // where it is exactly 0.0 / 1.0).
                const float entryY =
                    std::clamp(origin.y + t * direction.y -
                                  static_cast<float>(y),
                               0.0f, 1.0f);
                // Is the ray ENTERING THE SOLID at the boundary? (a
                // boundary hit on the crossed face) vs sitting in the air
                // half (fall through to the interior-surface / pass-through
                // test below). The solid is [bottom, top):
                //   - strictly inside (bottom < entryY < top): a side
                //     entry into the slab's body, any travel direction;
                //   - on the top edge (entryY == top): a hit only when
                //     descending (dir.y < 0, into the solid) — a top-slab
                //     top-face entry (entryY 1.0 == top 1.0) is the case
                //     this disambiguates from the air half;
                //   - on the bottom edge (entryY == bottom): a hit only
                //     when ascending (dir.y > 0, into the solid) — a
                //     bottom-slab bottom-face entry (entryY 0.0 == bottom
                //     0.0);
                //   - otherwise (outside, or grazing a surface edge with
                //     no travel into the solid): the air half.
                // The edge equalities are reliable: face entries clamp to
                // exactly 0.0f / 1.0f and bottom/top are 0.0 / 0.5 / 1.0.
                const bool boundaryHit =
                    (entryY > bottom && entryY < top) ||
                    (entryY == top && direction.y < 0.0f) ||
                    (entryY == bottom && direction.y > 0.0f);
                if (boundaryHit) {
                    // Entered the SOLID half: the whole-cell boundary hit
                    // (the exact pre-gap-fill behavior).
                    return DdaHit {c, crossingNormal(axis, direction), t};
                }
                // Entered the AIR half: the slab's interior surface (the
                // y = kSlabHalf plane) is the only face the ray can hit
                // inside this cell — and only when it travels TOWARD the
                // solid half (a bottom slab, solid below: descending; a
                // top slab, solid above: ascending). A ray running
                // parallel (no vertical component) never reaches it from
                // the air half.
                const bool towardSolid =
                    (bottom == 0.0f) ? direction.y < 0.0f
                                     : direction.y > 0.0f;
                if (towardSolid) {
                    const float tHalf =
                        (static_cast<float>(y) + kSlabHalf - origin.y) /
                        direction.y;
                    // Q must land INSIDE the cell: at or after the entry
                    // (tHalf >= t — equality = the entry grazes the
                    // surface, a hit) and at or before the next lateral
                    // grid plane (the "hits exactly at the boundary
                    // count" convention, the maxDistance twin). The y
                    // exit is never the limit: for the toward-solid
                    // direction the surface precedes the far y boundary
                    // by construction (kSlabHalf sits between the air-half
                    // entry and it).
                    if (tHalf >= t && tHalf <= std::min(tMaxX, tMaxZ) &&
                        tHalf <= maxDistance) {
                        // The interior-surface hit: the slab cell itself,
                        // the normal pointing back into the air half the
                        // ray came from (bottom slab: +Y its top face; top
                        // slab: -Y its bottom face).
                        return DdaHit {
                            c,
                            bottom == 0.0f ? Vec3 {0.0f, 1.0f, 0.0f}
                                           : Vec3 {0.0f, -1.0f, 0.0f},
                            tHalf};
                    }
                }
                // The ray misses the interior surface (it leaves the cell
                // laterally first, or travels away from the slab): pass
                // through the air half and keep walking.
                continue;
            }
        }
        return DdaHit {c, crossingNormal(axis, direction), t};
    }
}

} // namespace

std::optional<BlockHit> raycastBlock(const World& world,
                                     const BlockRegistry& blocks,
                                     Vec3 origin, Vec3 direction,
                                     float maxDistance) {
    // The first NON-AIR block (the original semantics: fluids are
    // transparent to the block ray). V2-17 gap fill: partial-solid-aware
    // for slab cells (slabSolidRangeAt — the shared ddaFirstCell sub-cell
    // test).
    const std::optional<DdaHit> hit = ddaFirstCell(
        origin, direction, maxDistance,
        [&](BlockPos c) {
            return world.getBlock(c.x, c.y, c.z) != BlockID::air();
        },
        [&](BlockPos c) {
            return slabSolidRangeAt(world, blocks, c);
        });
    if (!hit) {
        return std::nullopt;
    }
    return BlockHit {hit->cell, hit->normal, hit->t};
}

std::optional<BlockPos> raycastFluid(const World& world,
                                     const BlockRegistry& blocks,
                                     Vec3 origin, Vec3 direction,
                                     float maxDistance) {
    // The bucket's target ray (Phase 47): the first cell that is a
    // non-air BLOCK or carries a fluid level — a wall occludes the ray
    // (Minecraft: the bucket uses block targeting, you cannot scoop
    // through one), while the plain block ray passes through fluids. The
    // caller reads the returned cell's fluid level: kMaxFluidLevel (8)
    // is the scoopable source, anything else is "no action" (a block
    // face or flowing water). V2-17 gap fill: the shared ddaFirstCell
    // sub-cell test keeps the bucket ray consistent with the block ray
    // (a slab's air half is pass-through here too).
    const std::optional<DdaHit> hit = ddaFirstCell(
        origin, direction, maxDistance,
        [&](BlockPos c) {
            return world.getBlock(c.x, c.y, c.z) != BlockID::air() ||
                   world.getFluid(c.x, c.y, c.z) > 0;
        },
        [&](BlockPos c) {
            return slabSolidRangeAt(world, blocks, c);
        });
    if (!hit) {
        return std::nullopt;
    }
    return hit->cell;
}

bool canPlaceAt(const World& world, const BlockRegistry& blocks, BlockPos cell,
                const Vec3& playerMin, const Vec3& playerMax) {
    if (cell.y < 0 || cell.y >= kWorldHeight) {
        return false;
    }
    const BlockID id = world.getBlock(cell.x, cell.y, cell.z);
    if (id != BlockID::air() && blocks.at(id).properties.solid) {
        return false;
    }

    // Strict per-axis overlap test (same convention as the player's
    // collision code): cell [c, c+1) overlaps [min, max] iff
    // min < c+1 && max > c on every axis.
    const float cx = static_cast<float>(cell.x);
    const float cy = static_cast<float>(cell.y);
    const float cz = static_cast<float>(cell.z);
    if (playerMin.x < cx + 1.0f && playerMax.x > cx &&
        playerMin.y < cy + 1.0f && playerMax.y > cy &&
        playerMin.z < cz + 1.0f && playerMax.z > cz) {
        return false;
    }
    return true;
}

bool canPlantAt(const World& world, const BlockRegistry& blocks, BlockPos cell,
                const std::vector<std::string>& plantOn) {
    if (plantOn.empty()) {
        return true; // unrestricted (the generic placeBlock rule)
    }
    // The cell directly below the placement target must hold one of the
    // listed soil blocks. Out-of-range Y (cell.y == 0 -> below is -1) reads
    // as air through World::getBlock, so a bottom-row placement is rejected
    // (air is never in the list).
    const BlockID below = world.getBlock(cell.x, cell.y - 1, cell.z);
    for (const std::string& soil : plantOn) {
        if (below == blocks.idForName(soil)) {
            return true;
        }
    }
    return false;
}

std::optional<BlockPos> bucketPourTarget(const World& world,
                                         const BlockRegistry& blocks,
                                         const std::optional<BlockHit>& blockHit,
                                         const std::optional<BlockPos>& fluidHit,
                                         FluidKind kind,
                                         const Vec3& playerMin,
                                         const Vec3& playerMax) {
    // Candidate: the block-ray face neighbor (the phase-47 placement
    // convention — the block ray passes through fluids, so a floor under
    // a 1-deep pool makes the pool's top water cell the candidate), or,
    // when the block ray hit nothing (open water beyond block reach), the
    // fluid ray's own hit cell. No cross-fallback: a block hit always
    // decides the candidate (Minecraft aims the bucket at the block face,
    // never at a farther water cell behind it).
    std::optional<BlockPos> candidate;
    if (blockHit) {
        candidate = BlockPos {
            blockHit->block.x + static_cast<int>(blockHit->normal.x),
            blockHit->block.y + static_cast<int>(blockHit->normal.y),
            blockHit->block.z + static_cast<int>(blockHit->normal.z)};
    } else {
        candidate = fluidHit;
    }
    if (!candidate) {
        return std::nullopt;
    }
    if (!canPlaceAt(world, blocks, *candidate, playerMin, playerMax)) {
        return std::nullopt; // out of range / solid / player overlap
    }
    const int level = world.getFluidOfKind(candidate->x, candidate->y,
                                           candidate->z, kind);
    if (level == 0) {
        if (world.getFluid(candidate->x, candidate->y, candidate->z) == 0) {
            // (a) the legacy placement: air without fluid.
            return candidate;
        }
        // (c) FLUID-5: a fluid cell of the OTHER kind (a water bucket
        // against lava, a lava bucket against water). getFluid returns
        // the LEVEL of whichever kind is present (0..8) — the kind
        // itself comes from getFluidKind.
        const std::optional<FluidKind> cellKind =
            world.getFluidKind(candidate->x, candidate->y, candidate->z);
        if (cellKind && *cellKind == FluidKind::Lava &&
            kind == FluidKind::Water) {
            // The REACTION POUR: the water source goes one cell further
            // along the hit direction — the cell the ray came from
            // (directly ABOVE the lava for a straight top-down pool aim,
            // the open side for a wall-occluded aim); with no block hit
            // (open lava beyond reach) the +Y convention. The EXISTING
            // reaction pass then converts the lava to the lava block's
            // water_reaction product on the next tick (the poured water
            // is a level-8 source — one rule source, no new reaction
            // rule). No dry-air placement cell -> no-op, the bucket is
            // kept (Minecraft: water that cannot be placed does
            // nothing).
            const Vec3 dir = blockHit
                                 ? Vec3 {blockHit->normal.x,
                                         blockHit->normal.y,
                                         blockHit->normal.z}
                                 : Vec3 {0.0f, 1.0f, 0.0f};
            const BlockPos place {
                candidate->x + static_cast<int>(dir.x),
                candidate->y + static_cast<int>(dir.y),
                candidate->z + static_cast<int>(dir.z)};
            if (!canPlaceAt(world, blocks, place, playerMin, playerMax) ||
                world.getBlock(place.x, place.y, place.z) != BlockID::air() ||
                world.getFluid(place.x, place.y, place.z) != 0) {
                return std::nullopt;
            }
            return place;
        }
        // A LAVA bucket against a WATER cell: the REPLACEMENT POUR —
        // setFluid's existing "replace the other kind" semantics swap
        // the water for a lava source (the caller writes level 8); the
        // reaction pass then converts it on the next tick according to
        // the remaining water neighbors (a lone cell with none keeps
        // the lava — Minecraft: lava replaces water, then reacts).
        return candidate;
    }
    // (b) the pour onto a water surface: a flowing cell of the bucket's
    // own kind becomes the source. An already-source cell (level 8) is a
    // no-op (Minecraft: the bucket is kept — pouring source on source
    // changes nothing).
    if (level == kMaxFluidLevel) {
        return std::nullopt;
    }
    return candidate;
}

float raycastAabb(Vec3 origin, Vec3 dir, Vec3 boxMin, Vec3 boxMax) {
    // Standard slab method: each axis contributes an interval of ray
    // parameters that pierce its slab; the intersection of the three is
    // the hit. tmin starts at 0, so an origin inside the box reports 0.
    float tmin = 0.0f;
    float tmax = std::numeric_limits<float>::max();
    const float o[3] = {origin.x, origin.y, origin.z};
    const float d[3] = {dir.x, dir.y, dir.z};
    const float lo[3] = {boxMin.x, boxMin.y, boxMin.z};
    const float hi[3] = {boxMax.x, boxMax.y, boxMax.z};
    for (int a = 0; a < 3; ++a) {
        if (std::abs(d[a]) < 1e-8f) {
            // Parallel to the slab: a hit requires the origin to be
            // within the slab's span.
            if (o[a] < lo[a] || o[a] > hi[a]) {
                return -1.0f;
            }
            continue;
        }
        float t1 = (lo[a] - o[a]) / d[a];
        float t2 = (hi[a] - o[a]) / d[a];
        if (t1 > t2) {
            std::swap(t1, t2);
        }
        tmin = std::max(tmin, t1);
        tmax = std::min(tmax, t2);
        if (tmin > tmax) {
            return -1.0f;
        }
    }
    return tmin;
}

bool hasLineOfSight(const World& world, const BlockRegistry& blocks,
                    Vec3 from, Vec3 to) {
    const Vec3 d = to - from;
    const float dist = std::sqrt(dot(d, d));
    if (dist < 1e-4f) {
        return true;  // co-located: nothing in between
    }
    const std::optional<BlockHit> hit =
        raycastBlock(world, blocks, from, d * (1.0f / dist), dist);
    // The player's eye is always in an air cell, so any hit short of the
    // target is a real obstruction; a hit in the last epsilon would mean
    // the target stands inside a block, which cannot happen, so treat it
    // as clear either way.
    return !hit.has_value() || hit->distance >= dist - 1e-3f;
}

} // namespace mc
