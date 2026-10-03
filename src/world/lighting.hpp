// Lighting: Minecraft-style 0..15 sky light and block light, stored per
// chunk (see Chunk) and queried through World::getSkyLight/getBlockLight.
//
// Propagation model (simplified Minecraft):
//  - A cell blocks light iff its block is properties.opaque (binary
//    opacity: opaque = 15, otherwise 0 — water/leaves pass light through).
//  - Sky light: every column's topmost contiguous run of non-opaque cells
//    reads 15 (open to the sky); light then propagates through
//    non-opaque cells, free vertically, -1 per horizontal step.
//  - Block light: every block with properties.lightEmission > 0 is a
//    source (its own cell holds the emission value, even if opaque);
//    light propagates through non-opaque cells, -1 per step in all
//    directions.
//  - The void around the world is open sky (sky 15, block 0).
//
// Two entry points share one BFS core:
//  - propagateAll: recompute everything (startup, new chunk created).
//  - updateLocal: after a single block change, re-light only the region
//    that can have changed — see the method docs for the region shapes
//    (sky: full columns in a ±radius footprint, because sky light travels
//    vertically without attenuation; block: a ±radius box, because block
//    light decays away within 15 steps). Cells outside the region keep
//    their previous values, which remain correct, and serve as read-only
//    boundary sources for the propagation.
#pragma once

#include "block/block_registry.hpp"
#include "world/coordinates.hpp"
#include "world/world.hpp"

namespace mc {

// Highest light value (Minecraft's light scale).
inline constexpr int kMaxLight = 15;
// Re-light radius after a single block change: max light range (15) plus a
// one-cell boundary margin.
inline constexpr int kLightUpdateRadius = 16;

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
};

} // namespace mc
