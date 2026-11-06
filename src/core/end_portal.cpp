#include "core/end_portal.hpp"

#include <cmath>

namespace mc {

std::vector<BlockPos> endPortalRingCells(int cx, int level, int cz) {
    // The 5x5 ring (the four corners missing), the (dz, dx) nested
    // order (the scan / rescan / states cross-check's shared
    // deterministic order).
    std::vector<BlockPos> cells;
    cells.reserve(12);
    for (int dz = -2; dz <= 2; ++dz) {
        for (int dx = -2; dx <= 2; ++dx) {
            const bool onEdge = (std::abs(dx) == 2) || (std::abs(dz) == 2);
            const bool corner = (std::abs(dx) == 2) && (std::abs(dz) == 2);
            if (onEdge && !corner) {
                cells.push_back({cx + dx, level, cz + dz});
            }
        }
    }
    return cells;
}

std::vector<BlockPos> endRingCenterCandidates(const BlockPos& ringCell) {
    // The EIGHT ring offsets (the ring's shape: the 5x5 border minus
    // the four corners) reversed to centers — the cell minus each
    // offset. The same level (the ring is horizontal).
    static const int kOffsets[8][2] = {
        {-2, 0}, {2, 0}, {0, -2}, {0, 2},
        {-1, -2}, {1, -2}, {-1, 2}, {1, 2}};
    std::vector<BlockPos> out;
    out.reserve(8);
    for (const int (&o)[2] : kOffsets) {
        out.push_back({ringCell.x - o[0], ringCell.y, ringCell.z - o[1]});
    }
    return out;
}

std::vector<BlockPos> endPortalCenterCells(int cx, int level, int cz) {
    // The 3x3 center, the (dz, dx) nested order.
    std::vector<BlockPos> cells;
    cells.reserve(9);
    for (int dz = -1; dz <= 1; ++dz) {
        for (int dx = -1; dx <= 1; ++dx) {
            cells.push_back({cx + dx, level, cz + dz});
        }
    }
    return cells;
}

EndPortalScan scanEndPortalRing(const World& world,
                                const BlockRegistry& blocks, int cx,
                                int level, int cz) {
    EndPortalScan scan;
    scan.cx = cx;
    scan.level = level;
    scan.cz = cz;
    int frames = 0;
    for (const BlockPos& c : endPortalRingCells(cx, level, cz)) {
        const Block& b = blocks.at(world.getBlock(c.x, c.y, c.z));
        if (b.properties.endFrame) {
            ++frames;
            if (b.properties.endFrameEyed) {
                ++scan.eyedCount;
            }
        }
    }
    scan.ringComplete = (frames == 12);
    if (!scan.ringComplete) {
        return scan;
    }
    int air = 0;
    int portal = 0;
    for (const BlockPos& c : endPortalCenterCells(cx, level, cz)) {
        const Block& b = blocks.at(world.getBlock(c.x, c.y, c.z));
        if (b.id == BlockID::air()) {
            ++air;
        } else if (b.properties.endPortal) {
            ++portal;
        }
    }
    scan.centerAir = (air == 9);
    scan.centerPortal = (portal == 9);
    return scan;
}

bool endPortalCellValid(const World& world, const BlockRegistry& blocks,
                        const BlockPos& cell) {
    // The 3x3 of centers the cell could belong to (the ring is
    // horizontal: same level, |dx|, |dz| <= 1 — the center's cells).
    for (int dz = -1; dz <= 1; ++dz) {
        for (int dx = -1; dx <= 1; ++dx) {
            const EndPortalScan s =
                scanEndPortalRing(world, blocks, cell.x + dx, cell.y,
                                  cell.z + dz);
            if (s.ringComplete && s.eyedCount == 12 && s.centerPortal) {
                return true;
            }
        }
    }
    return false;
}

std::vector<BlockPos> endPortalActivationPlan(int cx, int level, int cz) {
    // The 3x3 center becomes the portal (the deterministic order —
    // the game applies setBlock in this sequence).
    return endPortalCenterCells(cx, level, cz);
}

EndEntryPlatformPlan endEntryPlatformPlan() {
    // The 5x5 obsidian square at the island's top layer, centered on
    // (40, 0) — the (dz, dx) nested order (the write's deterministic
    // sequence) + the 3 layers above each column cleared (the landing
    // safety — the island's open sky is already air, the clear is the
    // idempotent belt over the 10b content's future fills).
    EndEntryPlatformPlan plan;
    plan.setObsidian.reserve(25);
    plan.clearAir.reserve(75);
    for (int dz = -2; dz <= 2; ++dz) {
        for (int dx = -2; dx <= 2; ++dx) {
            const int x = kEndEntryX + dx;
            const int z = kEndEntryZ + dz;
            plan.setObsidian.push_back(
                {x, kEndEntryPlatformLevel, z});
            for (int i = 1; i <= kEndEntryPlatformClearAbove; ++i) {
                plan.clearAir.push_back(
                    {x, kEndEntryPlatformLevel + i, z});
            }
        }
    }
    return plan;
}

Vec3 endEntryPosition() {
    // The platform's top center, the feet (the travel's restorePosition
    // target).
    return Vec3 {static_cast<float>(kEndEntryX) + 0.5f,
                 static_cast<float>(kEndEntryPlatformLevel) + 1.0f,
                 static_cast<float>(kEndEntryZ) + 0.5f};
}

float endEntryYaw() {
    // West (the -X direction): yaw 0 = -Z, positive turns left —
    // -X is a quarter turn left of -Z = +pi/2 (the MC-exact landing).
    return static_cast<float>(std::numbers::pi) / 2.0f;
}

} // namespace mc
