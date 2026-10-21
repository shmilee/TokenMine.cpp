// Tree shape (GEN-2): the PURE geometry of a grown tree — what a sapling's
// `tree` object becomes when it completes. A tree is planned relative to
// its ROOT (the sapling cell, which becomes the trunk base):
//
//   trunk:  (0, i, 0) for i in [0, height)          (the trunk block)
//   canopy: the shape's layer table, each layer a square of LEAVES at a
//           fixed offset above the root, with the (0, 0) centre skipped
//           while the trunk still occupies it (offset <= height - 1) and
//           the square's corners rounded when the radius is 2.
//
// The shape table (Minecraft's tree silhouettes; the Oak table is
// cell-for-cell the worldgen's placeTrees canopy — the unit test pins it):
//
//   Oak:    (T-2, r), (T-1, r), (T, 1)              the round two-plus-cap
//   Spruce: (T-2, 2), (T-1, 1), (T, 1), (T+1, 1)    the bottom-wide cone
//   Birch:  (T-1, 1), (T, 1), (T+1, 1)              the slim small crown
//
// where (offset, radius) names a layer and r is the spec's leaf_radius
// (only the Oak table reads it — the spruce bottom layer is a fixed 2 and
// the birch a fixed 1, the shape owns its table).
//
// Determinism: everything here is a pure function of (height, spec,
// seed, root) — the game keeps no per-tree state (the grown tree IS the
// block edits), so a placed tree is byte-reproducible across runs. The
// height roll advances its state before the splitmix64 finalizer (PITS #4:
// the finalizer alone is not a progression).
#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

#include "block/block.hpp"
#include "world/coordinates.hpp"

namespace mc {

// The tree shape vocabulary (the JSON `tree.shape` names).
enum class TreeShape { Oak, Spruce, Birch };

// True when `name` is a tree shape (the JSON vocabulary: "oak", "spruce",
// "birch"). The data parser rejects anything else by name.
bool treeShapeValid(std::string_view name);

// The shape for a name that treeShapeValid already accepted (the parser
// validates before calling; an invalid name here is a logic error).
TreeShape treeShapeFromName(std::string_view name);

// The data-driven tree spec: what a grown tree of this sapling is built
// from (trunk / leaves blocks) and how tall it rolls (the trunk height is
// uniform in [minHeight, minHeight + heightRange]). Parsed from the
// block's `tree` object (strict six keys — see block_registry).
struct TreeSpec {
    BlockID trunk;    // the trunk block (the root cell + the column above)
    BlockID leaves;   // the canopy block
    int minHeight = 0;     // [1, 16] once validated
    int heightRange = 0;   // [0, 8] once validated
    int leafRadius = 0;    // [1, 2] once validated (the Oak table reads it)
    TreeShape shape = TreeShape::Oak;
};

// One cell of a planned tree: the offset from the root (the sapling cell)
// + whether it holds the trunk (true) or the leaves (false).
struct TreeCell {
    BlockPos offset;
    bool trunkCell;
};

// The full cell list of a tree with trunk height `height` (>= 1): the
// trunk column first (i ascending — the placement order: trunk before
// canopy), then the canopy layers (table order, each layer x-then-z
// ascending). The (0, 0) cell appears ONCE, as the trunk base (i = 0).
// Pure geometry: no world access, no randomness.
std::vector<TreeCell> planTree(int height, const TreeSpec& spec);

// The trunk height roll for one placement: a pure function of the world
// seed and the root cell, uniform in
// [spec.minHeight, spec.minHeight + spec.heightRange]. Frame-INDEPENDENT
// (the same root under the same seed rolls the same height every retry —
// a blocked placement re-checks the same planned tree once space opens).
int treeHeightRoll(std::uint64_t seed, const BlockPos& pos,
                   const TreeSpec& spec);

// The geometry of a sapling block's `tree` data: the spec with the trunk /
// leaves ids LEFT UNRESOLVED (invalid — the game's init resolves them into
// its spec cache after every block is registered; the pure growth
// function only needs the geometry, the placement only the ids). The block
// must carry a `tree` object (hasTree() — a logic error otherwise).
TreeSpec treeSpecFromBlock(const Block& block);

} // namespace mc
