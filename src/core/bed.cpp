// V2-13 (v2.14.0): the bed sleep system's implementation (see the
// header for the model). The gate predicates are pure functions of
// their arguments (no game, no clock — the unit tests own the
// boundaries); the respawn validation reads the world through the
// registry's properties (hard rule 3 — the bed family is the
// dye_base property, never a color id).
#include "core/bed.hpp"

#include <algorithm>

#include "block/block.hpp"
#include "player/player.hpp" // Player::hasRoomAt (the strict-inequality scan)

namespace mc {

namespace {

// The strict-inequality cell overlap (the Player::hasRoomAt /
// Entity::hasRoomAt convention — a face resting exactly on a cell
// boundary is not an overlap): the AABB's closed axis range [lo, hi]
// meets the cell's closed range [c, c+1).
bool axisOverlaps(float lo, float hi, int c) {
    return lo < c + 1.0f && hi > c;
}

} // namespace

bool sleepDistanceOk(Vec3 feet, Vec3 bedCenter) {
    const float dx = feet.x - bedCenter.x;
    const float dy = feet.y - bedCenter.y;
    const float dz = feet.z - bedCenter.z;
    // The squared comparison (the no-sqrt boundary discipline — the
    // inclusive edge: exactly kSleepMaxDistance passes).
    return dx * dx + dy * dy + dz * dz <=
           kSleepMaxDistance * kSleepMaxDistance;
}

bool inSleepHostileBox(Vec3 bedCenter, Vec3 mobPos) {
    // The CLOSED box (a mob exactly on the face still prevents the
    // sleep — the unit tests pin the corners).
    const float dx = std::fabs(mobPos.x - bedCenter.x);
    const float dz = std::fabs(mobPos.z - bedCenter.z);
    const float dy = mobPos.y - bedCenter.y;
    return dx <= kSleepHostileRadius && dz <= kSleepHostileRadius &&
           dy >= -kSleepHostileBelow && dy <= kSleepHostileAbove;
}

bool bedRespawnValid(const World& world, const BlockRegistry& blocks,
                     Vec3 pos, float width, float height) {
    const BlockPos cell = bedSpawnCell(pos);
    const BlockID id = world.getBlock(cell.x, cell.y, cell.z);
    if (id == BlockID::air() || !id.valid()) {
        return false;
    }
    const Block& b = blocks.at(id);
    if (b.properties.dyeBase != kDyeBaseBed) {
        return false; // not a bed family member (any color is a bed)
    }
    // The room scan: the player's AABB (bottom-center pos) must fit —
    // the Player::hasRoomAt strict-inequality discipline, with the bed
    // cell itself EXEMPT (its slab's solid region ends at the feet —
    // the Entity::hasRoomAt height-aware overlap precedent: standing
    // on the slab's top is room, the support is not an obstruction).
    const float half = width * 0.5f;
    const float mnX = pos.x - half;
    const float mxX = pos.x + half;
    const float mnY = pos.y;
    const float mxY = pos.y + height;
    const float mnZ = pos.z - half;
    const float mxZ = pos.z + half;
    const int x0 = static_cast<int>(std::floorf(mnX));
    const int x1 = static_cast<int>(std::floorf(mxX));
    const int y0 = static_cast<int>(std::floorf(mnY));
    const int y1 = static_cast<int>(std::floorf(mxY));
    const int z0 = static_cast<int>(std::floorf(mnZ));
    const int z1 = static_cast<int>(std::floorf(mxZ));
    for (int x = x0; x <= x1; ++x) {
        for (int y = y0; y <= y1; ++y) {
            for (int z = z0; z <= z1; ++z) {
                if (x == cell.x && y == cell.y && z == cell.z) {
                    continue; // the bed's own cell: the support
                }
                if (!axisOverlaps(mnX, mxX, x) ||
                    !axisOverlaps(mnY, mxY, y) ||
                    !axisOverlaps(mnZ, mxZ, z)) {
                    continue;
                }
                const BlockID n = world.getBlock(x, y, z);
                if (n == BlockID::air() || !n.valid()) {
                    continue;
                }
                if (blocks.at(n).properties.solid) {
                    return false;
                }
            }
        }
    }
    return true;
}

} // namespace mc
