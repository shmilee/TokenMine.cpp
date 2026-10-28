// FluidSim: tick-based simulation of the project's three fluids (water,
// lava, and honey — FLUID-6) over all loaded chunks.
//
// Storage: each chunk owns a per-cell ENCODED fluid array (see Chunk:
// 0 = dry, water level 1..8, lava level 9..16, honey level 17..24). A
// cell holds fluid XOR a block (World::setBlock displaces fluid;
// World::setFluid only fills air cells and replaces any other fluid
// kind). Level 8 (kMaxFluidLevel) is the full/source state for all
// kinds.
//
// Model (simplified Minecraft, deterministic, snapshot-based — all changes
// are computed from the start-of-tick state and applied at the end):
//  - Per KIND (water, lava and honey are simulated independently, same
//    rules — honey is additionally tick-GATED: it only simulates on
//    every 3rd fluid tick (FLUID-6, the 1/3-speed high viscosity; the
//    gate is frame-locked, see Game's honeyActive):
//  - A cell is FALLING when the cell below it is empty, or the cell below
//    is itself falling. A falling chain shifts down one cell per tick:
//    each cell takes the fluid level of the cell above it (the chain's top
//    empties); a chain reaching y < 0 drains out of the world. The shift
//    is a paired move — one level-token leaves the top and one arrives at
//    the bottom — net-zero per tick except the boundary drain.
//  - A cell resting on a block (or on the other fluid kind) spreads to
//    its four horizontal neighbors: an empty neighbor gets level L-1, a
//    fluid neighbor below L-1 is raised to L-1. Level 1 is the terminal
//    film and does not spread.
//  - A cell resting on a SAME-KIND fluid of level B (the unified pool
//    rules):
//      B == 8          the cell becomes full — a column backed by a source
//                      below refills to the top (this is what settles a
//                      broken lake floor; without it the refilled column
//                      oscillates level-7 / absorbed / level-7 forever);
//      0 < B < 8, L < B  the cell is ABSORBED into the pool (removed; the
//                      pool keeps its level B);
//      0 < B < 8, L >= B  STATIC DEPOSITION: the cell stays, the pool keeps
//                      B, and nothing is written. The pre-FLUID-1 rule
//                      raised the pool to L here; that raise was the only
//                      way a pool could gain volume with no source — the
//                      one unaccounted volume-gain path. Deleting it makes
//                      pool levels monotonically non-increasing per cell
//                      while keeping the deposition stable: a cliff-fed
//                      film cascade settles into a static film stack
//                      instead of oscillating (the FLUID-1 step-0 probe's
//                      flip-trigger finding, route A).
//  - Conservation (FLUID-1): "total volume ±0" is NOT the target — that
//    is the wrong reading of Minecraft, where a spreading flow spawns
//    level-(L-1) neighbors from nothing. Instead every per-tick cell
//    change is accounted in one of four named event classes: the spread
//    front σ (creating a flow in a dry cell and raising a shallower flow
//    — the front's creations are what MC does), pool absorption α,
//    source-back refill κ, and the y<0 boundary drain δ; conveyor falls
//    are the paired net-zero moves. In a closed region the flow-cell
//    count F (levels 1..7) and the source count S (level 8) obey
//    F' = F + σ_c − α − κ − δ and S' = S + κ (σ_c = the spread
//    creations; the raises do not change F), and a tick with no
//    σ/α/κ/δ event at all conserves the total level sum.
//  - Accepted standing rotation: cliff-fed film cascades and lake
//    overflows run as permanent standing waterfalls — a bounded set of
//    edge cells (sea-cliff films, the P90 surface-lake overflow)
//    re-churns every tick (a period-2 film cascade). It is off-view in
//    the shipped baselines, deterministic (byte-identical ×2 runs), and
//    only those chunks stay fluid-hot (the 99.8 activity gate). See
//    docs/KNOWN_ISSUES (fluid section).
//  - Deliberate deviations from Minecraft: full cells fall when
//    unsupported (a MC source floats), so water drains into a broken lake
//    floor; the water-lava reaction does not consume the water; the burn
//    pass does not consume the lava.
//  - Conflicting targets (two spreads into one cell) resolve to the max
//    level.
//
// Water-lava interaction (Phase 51): BEFORE the per-kind simulation, a
// snapshot pass converts every LAVA cell that is 6-adjacent to a WATER
// cell: the lava cell becomes the block named by the lava fluid block's
// 'water_reaction' data — 'source' when any water neighbor is a level-8
// source, 'flowing' otherwise. The water is NOT consumed (Minecraft's
// steam particles are a v1 deviation). A lava fluid block without
// water_reaction is inert toward water. After the pass no lava cell has a
// water neighbor, so the two kinds are 6-neighborhood-disjoint and each
// kind simulates exactly as if the other did not exist.
//
// Flamable burn (Phase 58, the fire system v2): after the reaction pass a
// second snapshot pass burns every BLOCK cell 6-adjacent to a LAVA cell
// whose data is `flamable` (the shipped data flags the wheat crop stages
// — Minecraft's lava burns crops): the block becomes air, with NO drop
// (a burned crop is gone, like Minecraft's char). The lava is not
// consumed. Inert when no registered block is flamable. Deliberate v1
// simplification: only DIRECT (6-) neighbors burn — Minecraft's fire
// spreads cell by cell over time.
//
// The tick is O(visited chunks' live fluid cells) (PIPE-3: the per-chunk
// 32K full scans are cell-list traversals — Chunk::fluidCells, collected
// per visited chunk; the visited set = the kind's active chunks, the 99.8
// activity flags' set twin, + their face neighbors). The six neighbor
// chunks are cached per chunk so cross-boundary cell reads cost no map
// lookups.
#pragma once

#include <set>
#include <vector>

#include "block/block_registry.hpp"
#include "world/coordinates.hpp"
#include "world/world.hpp"

namespace mc {

class FluidSim {
public:
    // Advance one tick over all loaded chunks: first the water-lava
    // reaction pass (honey is inert in it — no water_reaction data), then
    // the flamable burn pass (Phase 58), then the per-kind simulation
    // (water, then lava, then honey when `honeyActive`).
    //
    // `honeyActive` (FLUID-6): the honey viscosity gate — the honey
    // kind's simulation runs only on ticks where this is true (the
    // caller gates it frame-locked, every 3rd fluid tick = 1/3 speed);
    // water and lava are unaffected by the flag.
    //
    // Returns the cells whose state changed (world coordinates, sorted and
    // de-duplicated; reaction-converted blocks, burned-to-air blocks and
    // fluid-level changes); empty when nothing moved (a settled world).
    // Changed cells
    // are marked dirty through World::setBlock/World::setFluid, so their
    // chunks (and face-sharing neighbors) are re-meshed; the caller must
    // also re-light the touched region (fluid levels change the light
    // attenuation and emission — see Lighting::updateRegion).
    static std::vector<BlockPos> tick(World& world,
                                      const BlockRegistry& blocks,
                                      const std::set<ChunkPos>* flammable =
                                          nullptr,
                                      bool honeyActive = false);
};

} // namespace mc
