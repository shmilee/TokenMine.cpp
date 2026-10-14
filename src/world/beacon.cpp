// Beacon power level (Phase 83) — see the header for the contract.
#include "world/beacon.hpp"

#include <cstddef>

namespace mc {

namespace {
// The per-layer footprint of the pyramid (Minecraft): level 1 is a 3x3 just
// under the beacon, each higher level a 2-block wider square.
constexpr int kBeaconLayerSize[4] = {3, 5, 7, 9};
} // namespace

int beaconLevel(const World& world, const BlockRegistry& blocks, int x, int y,
                int z) {
    int level = 0;
    for (std::size_t layer = 0; layer < 4; ++layer) {
        const int baseY = static_cast<int>(y) - 1 - static_cast<int>(layer);
        const int half = kBeaconLayerSize[layer] / 2;
        bool full = true;
        for (int dx = -half; dx <= half && full; ++dx) {
            for (int dz = -half; dz <= half && full; ++dz) {
                const BlockID id = world.getBlock(x + dx, baseY, z + dz);
                if (!id.valid() || !blocks.at(id).properties.beaconBase) {
                    full = false;
                }
            }
        }
        if (!full) {
            break;
        }
        ++level;
    }
    return level;
}

} // namespace mc
