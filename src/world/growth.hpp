// Crop growth (Phase 48): deterministic per-tick rolls for the cells that
// hold a block with a non-empty `grows_to` property (e.g. wheat_1..wheat_6
// -> the next stage; the mature stage has no growsTo and never rolls).
//
// Design (mirrors FluidSim: a pure function over the world; the game keeps
// the bookkeeping set — see Game::m_states.crops):
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

// GEN-9: the water-proximity radius for soil wetness (the Phase 50
// Minecraft farmland value: any WATER level within this CHEBYSHEV radius
// of the soil cell, at the soil cell's y, wets it; lava never does —
// Phase 54). Shared by the crop growth pace (cropIsWet) and the farmland
// moisture visual (BlockModel::moistureTop, derived at mesh time).
inline constexpr int kWetWaterRadius = 4;

// True when water wets the soil cell (see kWetWaterRadius).
bool soilIsWet(const World& world, const BlockPos& soilPos);

// True when water is near the CROP's soil cell (the cell below the crop)
// — the dry_slowdown gate (Phase 50).
bool cropIsWet(const World& world, const BlockPos& cropPos);

// One cell advancing one growth stage this frame.
struct GrowthStep {
    BlockPos pos;
    BlockID to;
};

// GEN-2: the action a sapling cell takes on a passed roll (the growth
// stage IS the block's per-instance state — ENT-2; unlike a crop there is
// no next block id to write, only the state byte / the tree).
enum class SaplingAction {
    Advance,  // stage < stateCount-1: bump the state byte to `stage`
    Complete, // the final stage + the space fits: grow the tree of trunk
              // height `height` (the game places the planTree cells)
};

// One sapling cell acting this frame (the cropGrowthSteps twin).
struct SaplingStep {
    BlockPos pos;
    SaplingAction action;
    int stage = 0;  // Advance: the NEW state (current + 1)
    int height = 0; // Complete: the rolled trunk height (treeHeightRoll)
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

// GEN-2: the SAPLING growth steps (the cropGrowthSteps twin over the
// `m_states.saplings` tracked set — blocks with a `tree` object). Once per frame
// (the same fixed 1/60 step the day cycle uses — a no-input run stays
// byte-reproducible), each tracked cell, in position order:
//   1. trust the world (the cell still holds the tracked sapling id — a
//      stale entry yields no step, the crop guard);
//   2. the soil gate: the cell BELOW holds one of the block's `plant_on`
//      soils (the placement gate doubles as the growth gate — the design's
//      one-growth-condition contract; absent list = unrestricted);
//   3. the light gate: max(sky, block) at the sapling cell reaches the
//      block's `growth_light` (0 = none);
//   4. the roll: splitmix over (world seed + frame x golden step + the
//      per-cell position hash, a dedicated salt — PITS #4's advancing
//      state) passes when roll % growth_ticks == 0 (one stage every
//      growth_ticks frames on average, the crop pace);
//   5. the stage: the cell's per-instance state (the ENT-2 byte, the
//      sentinel falling back to the block's state default) — below the
//      final stage the step is an Advance (state + 1); at the final stage
//      the rolled height (world/tree_shape treeHeightRoll — frame-
//      INDEPENDENT, so a blocked retry re-checks the same tree) must FIT
//      (every planTree cell air + dry + in-world, the root cell excepted
//      — it holds the sapling) for the step to be a Complete; otherwise
//      no step (the sapling waits, Minecraft's blocked-canopy rule).
// Pure function of its arguments: same world + set + seed + frame ->
// same steps.
std::vector<SaplingStep> saplingGrowthSteps(const World& world,
                                            const BlockRegistry& blocks,
                                            const std::map<BlockPos, BlockID>& saplingCells,
                                            std::uint64_t seed,
                                            std::uint64_t frame);

} // namespace mc
