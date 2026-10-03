// FluidSim: tick-based water simulation over all loaded chunks.
//
// Storage: each chunk owns a per-cell fluid level array (0..kMaxFluidLevel,
// see Chunk). A cell holds fluid XOR a block (World::setBlock displaces
// fluid; World::setFluid only fills air cells). The game simulates one
// fluid (water); the level 8 (kMaxFluidLevel) is the full/source state.
//
// Model (simplified Minecraft, deterministic, snapshot-based — all changes
// are computed from the start-of-tick state and applied at the end):
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
//    level. Water volume is not conserved (sources create water), like
//    Minecraft's basic behavior.
//
// The tick is O(loaded cells): the per-chunk scan reads raw arrays; the six
// neighbor chunks are cached per chunk so cross-boundary cell reads cost no
// map lookups.
#pragma once

#include <vector>

#include "world/coordinates.hpp"
#include "world/world.hpp"

namespace mc {

class FluidSim {
public:
    // Advance one tick over all loaded chunks. Returns the cells whose
    // fluid level changed (world coordinates, sorted); empty when nothing
    // moved (a settled world). Changed cells are marked dirty through
    // World::setFluid, so their chunks (and face-sharing neighbors) are
    // re-meshed; the caller must also re-light the touched region (fluid
    // levels change the light attenuation — see Lighting::updateRegion).
    static std::vector<BlockPos> tick(World& world);
};

} // namespace mc
