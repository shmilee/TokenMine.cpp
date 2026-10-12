// Data-driven block definitions.
//
// A Block is pure data: properties (physics/rendering behavior), a model
// (which texture on which face, shape) and a stable identity. Core systems
// (world, renderer, player) read these fields — they must never switch on
// block identity. Adding a new block = adding a JSON file + textures, no
// code changes.
//
// BlockState is intentionally not a separate type yet: no block in the
// current set carries per-instance state (facing, attachment, ...). The
// concept is introduced when the first such block appears, together with a
// real consumer (chunk storage).
#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace mc {

// Right-click interaction kinds (Phase 17). A block whose
// BlockProperties.interact names one of these opens the corresponding GUI
// instead of placing a block. The set is validated at data load (see
// BlockRegistry) — a block can only declare an interaction the game
// implements.
inline constexpr std::string_view kInteractCraftingTable = "crafting_table";
// Phase 40: opens the furnace panel (the smelting GUI).
inline constexpr std::string_view kInteractFurnace = "furnace";

// Phase 41: the dye color of a dye-family base block. A dyed variant is
// named "<dye_base>_<color>" — EXCEPT white, which IS the base block
// (no suffix). The naming convention is a single pure function (below),
// shared by the recolor interaction and the game-init cross-check.
inline constexpr std::string_view kDyeColorWhite = "white";

// The block name of a dye-family variant for a base family and a dye
// color: "<base>_<color>", white mapping to the base name itself.
inline std::string dyeVariantName(std::string_view base,
                                  std::string_view color) {
    if (color == kDyeColorWhite) {
        return std::string(base);
    }
    return std::string(base) + "_" + std::string(color);
}

// Stable handle into a BlockRegistry. Invalid until assigned.
//
// Convention: BlockID{0} is the reserved "air" sentinel — the absence of a
// block. It is built into every BlockRegistry (not a data-driven block: no
// JSON, no textures). Chunk storage and meshing treat it as empty space;
// data blocks are assigned ids starting at 1.
struct BlockID {
    int value = -1;

    constexpr bool valid() const { return value >= 0; }
    constexpr bool operator==(BlockID o) const { return value == o.value; }
    constexpr bool operator!=(BlockID o) const { return value != o.value; }
    constexpr bool operator<(BlockID o) const { return value < o.value; }
    static constexpr BlockID invalid() { return {-1}; }
    static constexpr BlockID air() { return {0}; }
};

// Behavioral properties. All fields have sensible defaults; JSON files only
// need to override what differs.
struct BlockProperties {
    bool solid = true;           // blocks movement (AABB collision)
    bool opaque = true;          // fully occludes adjacent faces + blocks light
    bool transparent = false;    // rendered in the blending pass
    bool destroyable = true;     // can be broken by the player
    bool fluid = false;          // simulated as a fluid (Phase 9)
    float hardness = 1.0f;       // base break time in seconds (tools refine it)
    int lightEmission = 0;       // block light 0..15 (Phase 7)

    // Item dropped on break, addressed by item NAME (validated against the
    // ItemRegistry at game init — block items share the block's name, so
    // grass -> "dirt" drops the dirt block item). Empty = drops nothing.
    std::string drops;
    // How many of the drop item one break yields (1..64, must fit in one
    // stack of the drop item — cross-checked at game init).
    int dropsCount = 1;
    // Right-click interaction (Phase 17): kInteractCraftingTable opens the
    // crafting-table GUI instead of placing a block. Empty (the default) =
    // no special interaction. Validated against the known set at data load.
    // Mutually exclusive with dyeBase (a block either opens a GUI or is
    // dyed, never both).
    std::string interact;
    // Dye family (Phase 41): when non-empty, this block is a dyeable
    // variant of the named family (e.g. "wool"). The family's base block
    // is the one with dyeColor "white" (the base NAME, no suffix); dyed
    // variants are named dyeVariantName(dyeBase, dyeColor).
    std::string dyeBase;
    // Dye color (Phase 41): only with a non-empty dyeBase. Must name an
    // existing dye item's color (game-init cross-check).
    std::string dyeColor;
    // Crop growth (Phase 48): when non-empty, this block is a growing crop
    // step — it names the NEXT stage block (the mature stage has an empty
    // growsTo). Requires growthTicks. The growth engine (world/growth)
    // rolls a deterministic per-tick hash (seed + frame + position) and
    // advances the cell one step when it passes (probability 1/growthTicks
    // per 1/60 frame). Validated at data load (pairing + range) and at
    // game init (the target must name an existing block).
    std::string growsTo;
    // Mean ticks (1/60 s frames) between growth steps (1..1'000'000); only
    // meaningful with a non-empty growsTo (a lone growthTicks is rejected).
    int growthTicks = 0;
};

// Which texture name is applied to each face group (full_cube shapes) — or
// the single cross texture (cross shapes; Phase 45).
struct BlockTexture {
    std::string top;
    std::string side;
    std::string bottom;
    // Cross plants (shape "cross", Phase 45): the single texture drawn on
    // both crossing quads. Only used when model.shape == Shape::Cross.
    std::string cross;
};

// Rendering shape + face textures.
struct BlockModel {
    enum class Shape {
        FullCube, // 1x1x1
        Cross,    // Phase 45: two crossing vertical quads (cross plants —
                  // flowers). Rendered in the transparent (blended,
                  // unculled) pass with the cell's own light; never
                  // greedy-merged (no cube faces are emitted).
    };

    Shape shape = Shape::FullCube;
    BlockTexture textures;
};

struct Block {
    BlockID id = BlockID::invalid();
    std::string name;
    BlockProperties properties;
    BlockModel model;
};

// Hash for BlockID in unordered containers (identity hash).
struct BlockIDHash {
    std::size_t operator()(BlockID id) const noexcept {
        return std::hash<int>{}(id.value);
    }
};

} // namespace mc
