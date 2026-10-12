// Chance secondary drop (Phase 49): the deterministic per-break roll that
// decides whether a block's OPTIONAL `chance_drop` extra item is spawned
// (in addition to the primary drop).
//
// Determinism is a first-class requirement: the roll is a PURE function of
// (world seed, block position, chance) — the same cell broken in the same
// world always gives the same result, so headless runs stay byte-reproducible
// (no wall-clock, no per-instance RNG state). The hash advances on the
// position (a dedicated salt keeps this roll decorrelated from the crop-growth
// and worldgen rolls), and splitmix64 finalizes (PITS #4: a pure finalizer —
// the input must vary, which the position hash guarantees per cell).
#pragma once

#include <cstdint>

namespace mc {

// True when the break of the cell at (x,y,z) in a world with the given seed
// ALSO spawns the chance-drop item (probability `chance` in (0,1]).
// chance <= 0 is always false, chance >= 1 is always true (boundary no-ops so
// callers can pass raw data without re-validating).
bool rollChanceDrop(std::uint64_t seed, int x, int y, int z, double chance);

// Phase 52: the same contract for entry `entryIndex` (0-based) of a
// `chance_drops` LIST. Each entry rolls INDEPENDENTLY of the others: the
// per-entry salt is derived from a list-dedicated base salt finalized with
// the entry index, so two entries at the same cell (and the single-entry
// chance_drop roll) never track each other. The roll is still a pure
// function of (seed, position, entryIndex, chance): the same cell broken in
// the same world always gives the same result per entry.
bool rollChanceDropEntry(std::uint64_t seed, int x, int y, int z,
                         int entryIndex, double chance);

} // namespace mc
