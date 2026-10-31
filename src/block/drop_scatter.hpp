// Drop spawn scatter (UX-8d): the deterministic horizontal pseudo-scatter
// that break drops get around the broken block's center.
//
// Root cause addressed: every Game::breakBlock drop (the primary `drops`,
// the `chance_drop`, each `chance_drops` entry) spawned at the EXACT block
// center with zero velocity — a two-drop block (leaves -> stick + apple)
// produced two perfectly overlapping items, and a column of broken blocks
// produced drops lined up on the block centers (visually stiff, unlike
// Minecraft's scattered look).
//
// Determinism is a first-class requirement (hard rule 8): the offset is a
// PURE function of (world seed, block position, drop index) — the same cell
// broken in the same world always scatters the same way, so headless runs
// stay byte-reproducible (no wall-clock, no per-instance RNG state). Same
// family as block/chance_drop (the per-cell deterministic hash precedent);
// the dedicated salt keeps this hash untracked from the chance-drop rolls
// and from the crop-growth / worldgen rolls.
//
// Decorrelation from the chance-drop ROLLS is constructive: the offset and
// the roll are each pure functions of (seed, position) that share no salt —
// the offset value never depends on whether a chance roll passed, so the
// scattered spawn of a drop that DOES occur is the same it would have been
// had the roll not occurred (no re-scatter on the roll outcome).
#pragma once

#include <cstdint>

#include "core/math/vec2.hpp"

namespace mc {

// Half the horizontal scatter band: each component of the offset lands in
// [-kDropScatterRange, +kDropScatterRange).
//
// Bound argument: ItemEntity::kSize is 0.25 (half 0.125), so with a max
// offset of 0.2 the item CENTER is at most 0.2 from the cell center and
// the item BODY reaches at most 0.2 + 0.125 = 0.325 < 0.5 — a scattered
// item ALWAYS stays inside the broken cell (it never intrudes into a
// neighbor's solid volume, so no new collision/float edge cases). Pinned
// by test_drop_scatter (DropScatterTest.StaysInsideTheBrokenCell).
//
// Only the horizontal components scatter: a vertical offset could push the
// item into the cell above (or bury it in the cell below) where the rest
// position becomes physically ambiguous; Minecraft's scatter is horizontal
// for the same reason.
constexpr float kDropScatterRange = 0.2f;

// The horizontal spawn offset (dx, dz) for `dropIndex` of the break of the
// cell at (x, y, z) in a world with the given seed.
//
// dropIndex convention (Game::breakBlock): 0 = primary `drops`,
// 1 = `chance_drop`, 2 + i = `chance_drops` list entry i (the same 0-based
// list order the entry roll uses). The per-index salt keeps the drops of
// one break decorrelated (the two items of one block scatter to different
// spots).
Vec2 dropSpawnOffset(std::uint64_t seed, int x, int y, int z, int dropIndex);

} // namespace mc
