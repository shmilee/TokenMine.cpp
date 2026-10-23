// Lighting: Minecraft-style 0..15 sky light and block light, stored per
// chunk (see Chunk) and queried through World::getSkyLight/getBlockLight.
//
// Propagation model (Minecraft light opacity, LGT-2 — the leaf light
// attenuation):
//  - Each DRY cell attenuates light by its block's `light_opacity` (0..15,
//    Minecraft's scale: 15 = fully blocks light, 2 = leaves, 1 = glass,
//    0 = air / non-opaque plants). The property is DERIVED from `opaque`
//    at data load when the JSON key is absent (opaque -> 15, non-opaque ->
//    0), so field-less content reproduces the pre-LGT-2 lighting bit-for-
//    bit; an explicit `light_opacity` overrides the derivation. A FLUID
//    cell of either kind ALWAYS costs kFluidLightAttenuation (Minecraft's
//    water value) in every direction — the field is not read for fluids
//    (a documented simplification) — a lake bottom is dim in proportion
//    to the water depth.
//  - Sky light: every column's topmost contiguous run of non-full-opacity
//    cells reads 15 (open to the sky), attenuated per cell while scanning
//    down; light then propagates through non-full-opacity cells — free
//    vertically in a zero-opacity cell (air), otherwise at least 1 per
//    step (the cell's opacity).
//  - Block light: every block with properties.lightEmission > 0 is a
//    source (its own cell holds the emission value, even if fully
//    blocking — the glowstone pattern); light propagates with the same
//    per-cell costs in all directions.
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

#include <set>

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
    //
    // `dirtyOut` (99.10): when non-null, the net-changed chunk positions
    // are collected here INSTEAD of marking the World's shared dirty set
    // (markChunkDirty). The streaming lighting jobs run two such regions
    // in parallel on two workers — concurrent markChunkDirty calls would
    // race on the shared set — and the main thread applies the collected
    // positions after the pool flush (Game::collectLitColumns). Null
    // (the default: fluid relights, block edits, tree growth) marks the
    // World exactly as before.
    static void updateRegion(World& world, const BlockRegistry& blocks,
                             const BlockPos& min, const BlockPos& max,
                             bool newTerrain = false,
                             std::set<ChunkPos>* dirtyOut = nullptr);
};

} // namespace mc
