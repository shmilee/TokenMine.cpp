// Lighting: Minecraft-style 0..15 sky light and block light, stored per
// chunk (see Chunk) and queried through World::getSkyLight/getBlockLight.
//
// Propagation model (simplified Minecraft):
//  - A cell blocks light iff its block is properties.opaque. Air is free
//    for sky light; a fluid cell (water) attenuates by
//    kFluidLightAttenuation (Minecraft's water value) in every direction —
//    a lake bottom is dim in proportion to the water depth.
//  - Sky light: every column's topmost contiguous run of non-opaque cells
//    reads 15 (open to the sky), attenuated per fluid cell while scanning
//    down; light then propagates through non-opaque cells, free vertically
//    in air, -1 per horizontal air step, -3 per fluid cell.
//  - Block light: every block with properties.lightEmission > 0 is a
//    source (its own cell holds the emission value, even if opaque);
//    light propagates through non-opaque cells, -1 per air step, -3 per
//    fluid cell, in all directions.
//  - The void around the world is open sky (sky 15, block 0) and dry.
//
// Three entry points share one BFS core:
//  - propagateAll: recompute everything (startup, new chunk created).
//  - updateLocal: after a single block change, re-light the ±radius region
//    around it (a one-cell box, see updateRegion).
//  - updateRegion: after a batch of cell changes (a fluid tick), re-light
//    the region around their bounding box.
//    Region shape: sky light travels vertically without air attenuation,
//    so the sky re-scan covers FULL COLUMNS over the box's X/Z footprint
//    (expanded by the radius); block light decays within 15 steps, so it
//    is re-propagated in the box expanded by the radius. Cells outside the
//    region keep their previous values, which remain correct, and serve as
//    read-only boundary sources for the propagation. Only chunks whose
//    light actually changed (net) are marked dirty.
#pragma once

#include "block/block_registry.hpp"
#include "world/coordinates.hpp"
#include "world/world.hpp"

namespace mc {

// Highest light value (Minecraft's light scale).
inline constexpr int kMaxLight = 15;
// Re-light radius after cell changes: max light range (15) plus a one-cell
// boundary margin.
inline constexpr int kLightUpdateRadius = 16;
// Light lost when entering a fluid cell (Minecraft's water value), applied
// in every direction for both sky and block light.
inline constexpr int kFluidLightAttenuation = 3;

class Lighting {
public:
    // Recompute sky and block light for every existing chunk. O(world
    // size); the demo area takes a few tens of milliseconds.
    static void propagateAll(World& world, const BlockRegistry& blocks);

    // Re-light the region whose light can have changed after a single
    // block change at `changed` (break or place). Every chunk whose light
    // actually changed is marked dirty (its mesh carries per-face light);
    // chunks that came out identical are left untouched.
    static void updateLocal(World& world, const BlockRegistry& blocks,
                            const BlockPos& changed);

    // Re-light the region whose light can have changed after a batch of
    // cell changes inside the [min, max] box (a fluid tick passes the
    // bounding box of its changed cells; a single cell is min == max).
    // Same net-change dirty semantics as updateLocal.
    //
    // `newTerrain` (Phase 99.9): true when the box covers a freshly
    // INSERTED chunk (the streaming pipeline's column job). Then the sky
    // propagation always runs, even if the rescan's vertical re-assert
    // wrote nothing (a new column can be dark from the start — under a
    // ceiling — and still need its light FLOWED IN from the lit neighbor
    // columns). False (the default: fluid relights, block edits) enables
    // the no-op skip: rescan wrote nothing => the sky field is at its old
    // fixed point => the seed scan + BFS are pure re-proof.
    static void updateRegion(World& world, const BlockRegistry& blocks,
                             const BlockPos& min, const BlockPos& max,
                             bool newTerrain = false);
};

} // namespace mc
