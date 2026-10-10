#include "world/raycast.hpp"

#include <cmath>
#include <limits>

#include "block/block.hpp"
#include "block/block_registry.hpp"
#include "world/coordinates.hpp"

namespace mc {

namespace {
constexpr float kInf = std::numeric_limits<float>::infinity();
} // namespace

std::optional<BlockHit> raycastBlock(const World& world, Vec3 origin,
                                     Vec3 direction, float maxDistance) {
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

        if (world.getBlock(x, y, z) != BlockID::air()) {
            Vec3 normal = Vec3::zero();
            // The hit face points back at the ray: opposite to the travel
            // direction on the crossed axis.
            if (axis == 0) {
                normal.x = static_cast<float>(-stepX);
            } else if (axis == 1) {
                normal.y = static_cast<float>(-stepY);
            } else {
                normal.z = static_cast<float>(-stepZ);
            }
            return BlockHit {{x, y, z}, normal, t};
        }
    }
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

bool hasLineOfSight(const World& world, Vec3 from, Vec3 to) {
    const Vec3 d = to - from;
    const float dist = std::sqrt(dot(d, d));
    if (dist < 1e-4f) {
        return true;  // co-located: nothing in between
    }
    const std::optional<BlockHit> hit =
        raycastBlock(world, from, d * (1.0f / dist), dist);
    // The player's eye is always in an air cell, so any hit short of the
    // target is a real obstruction; a hit in the last epsilon would mean
    // the target stands inside a block, which cannot happen, so treat it
    // as clear either way.
    return !hit.has_value() || hit->distance >= dist - 1e-3f;
}

} // namespace mc
