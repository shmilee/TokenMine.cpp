// Beacon power level (Phase 83): a pure function that reads the world
// directly below a beacon block and returns its pyramid level 0..4
// (Minecraft). A beacon's level is the number of CONSECUTIVE full layers
// of `beacon_base` blocks stacked directly beneath it:
//
//   layer 1 (just under the beacon): a full 3x3 of base blocks
//   layer 2:                        a full 5x5 of base blocks
//   layer 3:                        a full 7x7 of base blocks
//   layer 4:                        a full 9x9 of base blocks
//
// The first layer that is NOT full stops the count (a level-2 beacon does
// not need its level-3 layer — the pyramid is built bottom-up and each
// level requires the one below to be complete). No per-instance state and
// no save-format field: the level is re-derived from the world every frame
// (like a crop's stage is its block id — the growth P48 pattern).
#pragma once

#include "block/block_registry.hpp"
#include "world/world.hpp"

namespace mc {

// The power level (0..4) of the beacon at `pos`, read from the base blocks
// in the layers directly below it. Pure function of its arguments: same
// world + blocks + pos -> same level. A world without a beacon base block
// (the property never set) yields level 0 for every position.
int beaconLevel(const World& world, const BlockRegistry& blocks,
                int x, int y, int z);

} // namespace mc
