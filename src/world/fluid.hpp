// FluidSim: tick-based simulation of the project's two fluids (water and
// lava) over all loaded chunks.
//
// Storage: each chunk owns a per-cell ENCODED fluid array (see Chunk:
// 0 = dry, water level 1..8, lava level 9..16). A cell holds fluid XOR a
// block (World::setBlock displaces fluid; World::setFluid only fills air
// cells and replaces the other fluid kind). Level 8 (kMaxFluidLevel) is
// the full/source state for both kinds.
//
// Model (simplified Minecraft, deterministic, snapshot-based — all changes
// are computed from the start-of-tick state and applied at the end):
//  - Per KIND (water and lava are simulated independently, same rules):
//  - A cell is FALLING when the cell below it is empty, or the cell below
//    is itself falling. A falling chain shifts down one cell per tick:
//    each cell takes the fluid level of the cell above it (the chain's top
//    empties); a chain reaching y < 0 drains out of the world.
//  - A cell resting on a block spreads to its four horizontal neighbors:
//    an empty neighbor gets level L-1, a fluid neighbor below L-1 is raised
//    to L-1. Level 1 is the terminal film and does not spread.
//  - A cell resting on fluid: full cells (L == kMaxFluidLevel) are stable
//    on any fluid and never merge; a shallower flowing film (L < L2) is
//    absorbed into the deeper pool below (removed); an equal or deeper
//    level (L >= L2) stays and raises the cell below to L (a landed flow
//    keeps filling the cell below).
//  - Deliberate deviation from Minecraft: full cells fall when unsupported
//    (a MC source floats), so water drains into a broken lake floor.
//  - Conflicting targets (two spreads into one cell) resolve to the max
//    level. Fluid volume is not conserved (sources create fluid), like
//    Minecraft's basic behavior.
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
// The tick is O(loaded cells): the per-chunk scan reads raw arrays; the six
// neighbor chunks are cached per chunk so cross-boundary cell reads cost no
// map lookups.
#pragma once

#include <vector>

#include "block/block_registry.hpp"
#include "world/coordinates.hpp"
#include "world/world.hpp"

namespace mc {

class FluidSim {
public:
    // Advance one tick over all loaded chunks: first the water-lava
    // reaction pass, then the flamable burn pass (Phase 58), then the
    // per-kind simulation (water, then lava).
    // Returns the cells whose state changed (world coordinates, sorted and
    // de-duplicated; reaction-converted blocks, burned-to-air blocks and
    // fluid-level changes); empty when nothing moved (a settled world).
    // Changed cells
    // are marked dirty through World::setBlock/World::setFluid, so their
    // chunks (and face-sharing neighbors) are re-meshed; the caller must
    // also re-light the touched region (fluid levels change the light
    // attenuation and emission — see Lighting::updateRegion).
    static std::vector<BlockPos> tick(World& world,
                                      const BlockRegistry& blocks);
};

} // namespace mc
