// Crop growth (Phase 48): deterministic per-tick rolls for the cells that
// hold a block with a non-empty `grows_to` property (e.g. wheat_1..wheat_6
// -> the next stage; the mature stage has no growsTo and never rolls).
//
// Design (mirrors FluidSim: a pure function over the world; the game keeps
// the bookkeeping set — see Game::m_crops):
//  - The WORLD holds the growth state: a crop cell's block id IS the stage
//    (wheat_N). There is no per-instance state and no save-format field.
//  - Once per frame (fixed 1/60 step, day-cycle class — a no-input run
//    stays byte-reproducible), each tracked cell rolls
//        roll = splitmix64(seed + frame * GOLDEN + hash3(x, y, z, salt))
//      and advances to its `grows_to` block when roll % growthTicks == 0
//      (probability 1/growthTicks per tick, mean one step every
//      growthTicks frames).
//  - The roll input ADVANCES with the frame (PITS #4: splitmix64 is a pure
//    finalizer — a constant input yields a constant stream). The per-cell
//    position hash decorrelates crops from each other (two plants do not
//    advance in lockstep); the world seed decorrelates across worlds.
//  - Environment gates (Phase 50, all data-driven per crop block): a
//    non-empty `grows_on` requires the cell below to hold that block (the
//    soil — hard gate); `growth_light` (0..15, 0 = none) requires
//    max(sky, block) light at the crop cell; the roll modulus is
//    growthTicks x drySlowdown (1 = none) when no water is within
//    Chebyshev radius 4 of the soil cell at the soil's y (dry farmland
//    grows slower — the Minecraft value is 2).
//  - The tracked set is position-ordered (std::map), so iteration and the
//    returned step list are deterministic.
#pragma once

#include <cstdint>
#include <map>
#include <vector>

#include "block/block_registry.hpp"
#include "world/coordinates.hpp"
#include "world/world.hpp"

namespace mc {

// One cell advancing one growth stage this frame.
struct GrowthStep {
    BlockPos pos;
    BlockID to;
};

// The subset of `cropCells` whose current world block is a growth stage
// (non-empty growsTo, matching the tracked id) and whose roll passed this
// frame. A cell whose world block no longer matches the tracked id (stale
// entry — defensive; the game maintains the set through its setBlock
// choke point and rescans after load) yields no step. Pure function of
// its arguments: same world + set + seed + frame -> same steps.
std::vector<GrowthStep> cropGrowthSteps(const World& world,
                                        const BlockRegistry& blocks,
                                        const std::map<BlockPos, BlockID>& cropCells,
                                        std::uint64_t seed,
                                        std::uint64_t frame);

} // namespace mc
