// Fire spread (Phase 66, Fire v2): deterministic per-roll ignitions of the
// FLAMABLE neighbor cells of live fire cells (Minecraft: fire spreads to
// the wood around it; the shipped data flags wood/leaves/grass/crop
// stages — Phase 58/63).
//
// Design (mirrors cropGrowthSteps: a pure function over the world; the
// game keeps the bookkeeping set — see Game::m_states.fires):
//  - The WORLD holds the burn state: a burning cell's block id IS the fire
//    block. There is no per-instance state and no save-format field
//    (the remaining life lives in Game::m_states.fires — Phase 64).
//  - Once every spread_interval_frames (the game gates on the frame
//    counter, fixed 1/60 step, day-cycle class — a no-input run stays
//    byte-reproducible), each live fire cell rolls once per SPREAD
//    neighbor:
//        u = splitmix64(seed + frame * GOLDEN
//                       + hash3(x, y, z, salt) + i * GOLDEN2)
//      and ignites the neighbor when u < chance (probability `chance` per
//    neighbor per roll frame). The roll input ADVANCES with the frame
//    (PITS #4: splitmix64 is a pure finalizer — a constant input yields a
//    constant stream). The per-cell position hash decorrelates fires from
//    each other; the world seed decorrelates across worlds.
//  - Spread neighbors (Minecraft): the 4 horizontal + ABOVE. Fire never
//    spreads downward (a fire on the floor does not burn the floor).
//  - A flammable cell adjacent to several fire cells rolls once per fire
//    neighbor (any pass ignites it) and is returned ONCE.
//  - The firePositions input is position-ordered (the game passes the
//    m_states.fires map keys), so the returned list — sorted — is deterministic.
#pragma once

#include <cstdint>
#include <vector>

#include "block/block_registry.hpp"
#include "world/coordinates.hpp"
#include "world/world.hpp"

namespace mc {

// The spread neighbor offsets, in roll order (Phase 66): +X, -X, +Z, -Z,
// +Y. NO -Y (fire does not spread downward — Minecraft).
inline constexpr BlockPos kFireSpreadNeighbors[5] = {{1, 0, 0},
                                                     {-1, 0, 0},
                                                     {0, 0, 1},
                                                     {0, 0, -1},
                                                     {0, 1, 0}};

// The flammable 5-neighbor cells of `firePositions` whose spread roll
// passed this frame. A cell whose roll passed but that no longer holds a
// flamable block (defensive — the game's world is the authority) is
// dropped. Pure function of its arguments: same world + positions + seed
// + frame + chance -> same ignitions.
std::vector<BlockPos> fireSpreadIgnitions(const World& world,
                                          const BlockRegistry& blocks,
                                          const std::vector<BlockPos>& firePositions,
                                          std::uint64_t seed,
                                          std::uint64_t frame,
                                          double chance);

} // namespace mc
