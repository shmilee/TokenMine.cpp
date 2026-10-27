#include "world/beam_light.hpp"

#include "world/lighting.hpp" // kMaxLight
#include "world/world.hpp"

namespace mc {

std::pair<float, float> beamLightSample(const World& world, int x, int y,
                                        int z) {
    // (x, y+1, z): the air cell directly above the beacon block — the
    // beam's lowest cell (drawBeaconBeams positions the column on the
    // block's top face). The beacon's own cell is opaque and reads
    // (0, 0), which would darken the beam to the ambient floor in every
    // environment (the gate 119 E2E defect, 10-27).
    const float sky =
        static_cast<float>(world.getSkyLight(x, y + 1, z)) /
        static_cast<float>(kMaxLight);
    const float block =
        static_cast<float>(world.getBlockLight(x, y + 1, z)) /
        static_cast<float>(kMaxLight);
    return {sky, block};
}

} // namespace mc
