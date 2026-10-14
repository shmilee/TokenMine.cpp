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
// BlockProperties.interact names one of these gets the corresponding
// right-click behavior (a GUI for the first two) instead of placing a
// block. The set is validated at data load (see BlockRegistry) — a block
// can only declare an interaction the game implements.
inline constexpr std::string_view kInteractCraftingTable = "crafting_table";
// Phase 40: opens the furnace panel (the smelting GUI).
inline constexpr std::string_view kInteractFurnace = "furnace";
// Phase 71: the campfire cooking interaction (place/take the single
// smelting slot — no panel; see item/campfire.hpp).
inline constexpr std::string_view kInteractCampfire = "campfire";

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

// One entry of a `chance_drops` list (Phase 52): the strict triple
// {item, count, chance} — the same shape as the single `chance_drop`
// (Phase 49). Defaults are the "unfilled" values; the parser validates the
// triple is complete and in range before the entry is used.
struct ChanceDrop {
    std::string item; // the extra item name (non-empty once validated)
    int count = 0;    // how many (1..64, fits one stack once validated)
    double chance = 0.0; // per-break probability in (0, 1] once validated
};

// Behavioral properties. All fields have sensible defaults; JSON files only
// need to override what differs.
struct BlockProperties {
    bool solid = true;           // blocks movement (AABB collision)
    bool opaque = true;          // fully occludes adjacent faces + blocks light
    bool transparent = false;    // rendered in the blending pass
    bool destroyable = true;     // can be broken by the player
    // Burns to air when 6-adjacent to a LAVA cell (Phase 58, the fire
    // system v2 — Minecraft's lava burns crops). The fluid sim's burn pass
    // (world/fluid) checks this every tick; the shipped data flags the
    // wheat crop stages only (wood/leaves/wool do not burn in v1 — see
    // KNOWN_ISSUES). No drop on burn (a burned crop is gone, like
    // Minecraft's char).
    bool flamable = false;
    // Simulated as a fluid (Phase 9; KINDED in Phase 51): the fluid's name
    // (vocabulary: "water", "lava" — validated at data load). Empty = not
    // a fluid. A fluid block is stored as a fluid level in the chunk's
    // fluid array (never as a placed block); it is the texture/lighting
    // reference for cells of that kind (see BlockRegistry::fluidBlock).
    std::string fluidName;
    float hardness = 1.0f;       // base break time in seconds (tools refine it)
    int lightEmission = 0;       // block light 0..15 (Phase 7)
    // Water reaction (Phase 51), only on a fluid block: when a cell of
    // THIS fluid sits 6-adjacent to a water cell, the fluid cell is
    // converted to a block (the fluid is consumed, the water is not —
    // see world/fluid). `waterReactionSource` is the block for a SOURCE
    // (level 8) water neighbor, `waterReactionFlowing` for any flowing
    // water neighbor; a source neighbor wins. Strictly paired (both set or
    // both empty) and the names are cross-checked at game init (the ammo
    // pattern). Empty pair = the fluid is inert toward water (the default
    // for any fluid without the field).
    std::string waterReactionSource;
    std::string waterReactionFlowing;

    // Item dropped on break, addressed by item NAME (validated against the
    // ItemRegistry at game init — block items share the block's name, so
    // grass -> "dirt" drops the dirt block item). Empty = drops nothing.
    std::string drops;
    // How many of the drop item one break yields (1..64, must fit in one
    // stack of the drop item — cross-checked at game init).
    int dropsCount = 1;
    // Chance secondary drop (Phase 49): an OPTIONAL extra drop rolled per
    // break — INDEPENDENT of (and in addition to) the primary `drops`.
    // `chanceDropItem` names the extra item (empty = no chance drop, the
    // default); `chanceDropCount` how many (1..64, fits one stack);
    // `chanceDropChance` the per-break probability in (0, 1] (a break spawns
    // the extra item when a deterministic hash of world-seed + position
    // passes — see block/chance_drop; headless byte-reproducible). Validated
    // at data load (pairing + range) and at game init (the item must exist
    // and its max stack fit the count — the drops discipline).
    std::string chanceDropItem;
    int chanceDropCount = 0;
    double chanceDropChance = 0.0;
    // Chance secondary drop LIST (Phase 52): an OPTIONAL array of extra
    // drops, each rolled INDEPENDENTLY per break (the entry-indexed salt
    // keeps the entries decorrelated — see block/chance_drop). Each entry is
    // the same strict triple as the single `chance_drop`. MUTUALLY EXCLUSIVE
    // with the single `chance_drop` (a block has either 0 or one of the two
    // systems, never both — validated at data load). Empty (the default) =
    // no list. Every entry is validated at data load (triple + range) and at
    // game init (the item must exist and its max stack fit the count — the
    // drops discipline).
    std::vector<ChanceDrop> chanceDrops;
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
    // Crop environment (Phase 50), all optional growth-condition fields
    // (empty/0/1 defaults = no requirement — pre-Phase-50 crops keep their
    // old behavior): `growsOn` names the block the cell BELOW the crop must
    // hold (the soil; the growth engine skips the roll otherwise — the
    // hard gate). Validated at game init (the name must reference an
    // existing block, the grows_to discipline).
    std::string growsOn;
    // Igniter target (Phase 76): when non-empty, a right-click with an
    // `ignites_flamable` item (the flint and steel) sets this block IN
    // PLACE (no drops, the item is not consumed, the "ignite" sound —
    // the Phase 60 igniter semantics). The relight use: the extinguished
    // campfire names "campfire" (Minecraft: flint and steel relights an
    // unlit campfire). Takes precedence over `flamable` (deterministic
    // order). Existence cross-checked at game init (the grows_to
    // discipline); an explicit empty string is rejected at data load (the
    // ammo discipline).
    std::string ignitesTo;
    // Minimum light level (0..15) at the CROP cell, max(sky, block), for
    // the roll to run (0 = no requirement; 9 = the Minecraft crop value).
    int growthLight = 0;
    // Growth slowdown factor when NO water is near the soil cell (any fluid
    // level within Chebyshev radius 4, same y — see world/growth): 1 = no
    // slowdown, 2 = half pace (the Minecraft dry-farmland value). The roll
    // modulus is growthTicks × drySlowdown, so dry passes are a strict
    // subset of wet passes (deterministic, exactly 1/drySlowdown as often).
    int drySlowdown = 1;
    // Planting placement restriction (Phase 53): an OPTIONAL list of block
    // names. When non-empty, a placement of this block is allowed only when
    // the cell DIRECTLY BELOW the target holds one of the listed blocks (the
    // soil) — a placement-time gate, INDEPENDENT of the growth-time `growsOn`
    // gate (Phase 50): a block can be placeable on soil X yet require soil Y
    // to grow. Empty (the default) = unrestricted (the pre-Phase-53 behavior,
    // the generic placeBlock rule). An explicit empty array is rejected at
    // data load (the ammo discipline); each name is cross-checked at game
    // init (it must reference an existing block, the grows_on discipline).
    std::vector<std::string> plantOn;
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
