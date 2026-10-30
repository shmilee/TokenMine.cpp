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

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
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

// GEN-2: the data-driven tree a SAPLING grows into (the block's `tree`
// object — a strict six-key nested object, the chance_drops-entry
// precedent). All fields are validated at data load (pairing + range +
// vocabulary, named `tree.<key>`); the trunk / leaves NAMES are
// cross-checked against the registry at directory load (both must exist),
// and a block with a `tree` object must be STATEFUL with at least two
// states (state_count >= 2 — the growth stage IS the per-instance state,
// ENT-2; rejected at registration). The game resolves the names to
// BlockIDs into a world/tree_shape TreeSpec at init.
struct TreeGrowData {
    std::string trunk;      // the trunk block name (non-empty once validated)
    std::string leaves;     // the canopy block name (non-empty once validated)
    int minHeight = 0;      // [1, 16] once validated
    int heightRange = 0;    // [0, 8] once validated
    int leafRadius = 0;     // [1, 2] once validated (the Oak table reads it)
    std::string shape;      // "oak" / "spruce" / "birch" once validated
};

// Behavioral properties. All fields have sensible defaults; JSON files only
// need to override what differs.
struct BlockProperties {
    bool solid = true;           // blocks movement (AABB collision)
    bool opaque = true;          // fully occludes adjacent faces + blocks light
    bool transparent = false;    // rendered in the blending pass
    // PRE-5 alpha cutout: true = the block's geometry belongs to the
    // CUTOUT render pass (between the opaque and the translucent-blend
    // pass): it is drawn with alpha discard active (the fragment shader's
    // `alpha < 0.5 -> discard`, gated on render.alpha_test) AND with depth
    // WRITE enabled, so the holes of its texture (alpha-0 pixels) do not
    // occlude geometry behind them (the leaves / cross-plant case).
    // Requires `transparent` (rejected otherwise at data load — the cutout
    // pass is a sub-pass of the transparent routing). Default false = the
    // block stays in the translucent-blend pass (depth write off when
    // render.alpha_test is on; when it is off the whole pipeline is
    // bit-identical to pre-PRE-5). The assignment is content data (the
    // texture alpha census: a block's cutout eligibility is its texture's
    // alpha set being a subset of {0, 255} — no intermediate values; a
    // texture with a translucent core — glass 220, water 200, pane 90 —
    // keeps the blend pass).
    bool cutout = false;
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
    // Light opacity (LGT-2, the leaf light attenuation): the light lost
    // when ENTERING this block's cell, 0..15 (Minecraft's light scale:
    // 15 = fully blocks light, 0 = passes like air, 2 = the leaf value,
    // 1 = the glass value). JSON key `light_opacity` is OPTIONAL: when
    // ABSENT the parser DERIVES it from the final opaque flag (opaque ->
    // 15, non-opaque -> 0 — a bit-for-bit reproduction of the pre-LGT-2
    // lighting for content without the field). A FLUID cell never reads
    // this field (it costs the fixed kFluidLightAttenuation — see
    // world/lighting). Struct default 0: the built-in air sentinel keeps
    // it (air is non-opaque, the derived value anyway).
    int lightOpacity = 0;
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
    // Stair rotation (Phase 92): when non-empty, a SNEAKING right-click on
    // this block rotates it IN PLACE — setBlock to the named variant (the
    // "next facing" of the stair family). The orientation IS the block id
    // (the 4 stair variants from Phase 89, `stair_facing` 0-3) — the project
    // keeps NO per-block rotation state (the "state = block id" philosophy,
    // like the crop stages), so there is NO format change. The named variant
    // is init-cross-checked to exist (the grows_to / ignites_to discipline);
    // an explicit empty string is rejected at data load (the ammo discipline).
    std::string stairRotatesTo;
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
    // Beacon pyramid base (Phase 83): when true, a cell of THIS block
    // counts as one unit of the mineral base a beacon (the block with a
    // non-empty beaconEffect) scans below it to determine its power level
    // (a full 3x3 / 5x5 / 7x7 / 9x9 layer = levels 1..4, Minecraft). The
    // shipped data flags iron_block (the canonical Minecraft beacon
    // material). A pure data flag — the core reads the property, it never
    // switches on a block id (hard rule 3).
    bool beaconBase = false;
    // Beacon effect (Phase 83, a LIST in Phase 91): when non-empty, this
    // block is a BEACON — each frame, if its power level (see beaconBase)
    // is >= 1, it applies its effect to every player within the beacon
    // range (the duration is refreshed while the player stays in range, the
    // fire-source pattern). Existence cross-checked at game init (the
    // grows_to / ignites_to discipline); an explicit empty name is rejected
    // at data load (the ammo discipline). Empty (the default) = not a
    // beacon.
    // Phase 83 shipped "absorption" (the beacon's Minecraft effect, the
    // project's second absorption source after the golden apple).
    std::string beaconEffect;
    // Phase 91: the beacon's EFFECT LIST — the "enchanting table" (multiple
    // effects on the same beacon, like Minecraft's primary + secondary
    // effect). This is the authoritative field the core reads. `beacon_
    // effects` (a non-empty array of effect names) takes precedence;
    // `beacon_effect` (a single name) is the backward-compat 1-element
    // list. The two are mutually exclusive (both present = rejected at
    // data load, the grants_effect / grants_effects discipline). Merged
    // into this list at parse time.
    std::vector<std::string> beaconEffects;
    // GEN-2: the tree this sapling grows into (the data `tree` object; see
    // TreeGrowData). nullopt = not a sapling (the default). Requires
    // growth_ticks (the per-stage roll pace — a `tree` block without it
    // never completes) and state_count >= 2 (the stage IS the state,
    // rejected at registration).
    std::optional<TreeGrowData> tree;
    // Sound material (CRAFT-3): when non-empty, the block's dig / place /
    // break sounds play the MATERIAL's timbre ("<material>_<event>" —
    // e.g. "stone_break") instead of the generic event name. The material
    // vocabulary is the wav prefixes on disk (assets/sounds/): a material
    // exists iff its three event files <material>_dig.wav /
    // <material>_place.wav / <material>_break.wav all exist — cross-checked
    // at game init (the grows_to discipline, naming the missing piece).
    // An explicit empty string is rejected at data load (the ammo
    // discipline). Empty (the default) = the generic event names (dig /
    // place / break) — the bit-level invariant for field-less content
    // (the structural-immunity red line: such blocks play exactly the
    // samples pre-CRAFT-3). Pure data — the core reads the property, it
    // never switches on a block id (hard rule 3).
    std::string soundMaterial;

    // CRAFT-6: the block item's furnace fields. A block item is
    // SYNTHESIZED from its block (buildBlockItems — an explicit
    // same-named item file is rejected), so the block property is the
    // only data-driven route for a block to be fuel or smeltable.
    // 'fuel_time' (seconds, (0, 1000] — the item-side discipline, the
    // coal 80 s precedent; ABSENT / 0 = not a fuel): inherited by the
    // synthesized item (wood 20 s shipped).
    float fuelTime = 0.0f;
    // 'smeltable_to' (the smelting product's ITEM name; ABSENT = not
    // smeltable — the ammo discipline, explicit empty string rejected;
    // the product's existence is cross-checked at game init over ALL
    // items, block items included): sand -> glass shipped.
    std::string smeltableTo;
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
        Slab,     // Phase 85: a bottom half-block (y 0..0.5 of the cell) —
                  // a flat step you can stand on top of (feet at y + 0.5);
                  // the cell is solid in its lower half only (height-aware
                  // collision). Uses the cube face textures.
        Stairs,   // Phase 87 (4-directional in Phase 89): a step you climb —
                  // a wedge from full height at one end to zero at the other,
                  // oriented by `stairFacing`. The solid height varies across
                  // the cell along the facing axis: the collision
                  // (Entity::solidTopHeight) uses the AABB's low end on that
                  // axis so the feet rest on the slope at that position.
                  // Uses the cube face textures.
        Cross,    // Phase 45: two crossing vertical quads (cross plants —
                  // flowers). Rendered in the transparent (blended,
                  // unculled) pass with the cell's own light; never
                  // greedy-merged (no cube faces are emitted).
        Wall,     // Phase 95: a 1.5-block-tall pillar (Minecraft's wall) —
                  // the cell is solid in y 0..1.5 (a full block + a half
                  // cap on top). A barrier: the collision (height-aware,
                  // solidTopHeight = 1.5) blocks the player, who cannot
                  // jump over it (the jump is ~1.25 < 1.5). Uses the cube
                  // face textures. The third new block model type (after the
                  // slab Phase 85 + the stairs Phase 87).
        Fence,    // Phase 98: Minecraft's fence — two crossed 1/8-thick
                  // vertical posts (y 0..1.5, like the wall) + a connecting
                  // BAR toward each horizontal fence / full opaque block
                  // neighbor. A barrier (solidTopHeight = 1.5). Uses a
                  // single "side" texture (the top/bottom cube keys are
                  // rejected, the cross discipline). The fourth new block
                  // model type.
        GlassPane, // Phase 99: Minecraft's glass pane — a 1/16-thick 1x1
                  // glass SHEET (full cell height, the z-centered plane) +
                  // thin rim slivers on the four edges (culled against a
                  // pane neighbor — the seam where two rows of panes meet).
                  // A JUMPABLE solid (solidTopHeight = 1.0, the jump is
                  // ~1.125 > 1.0). Blended in the transparent pass, light
                  // passes through (non-opaque). Uses a single "side"
                  // texture (the pane). The fifth new block model type.
    };

    Shape shape = Shape::FullCube;
    BlockTexture textures;
    // Phase 89: the stair's facing (only meaningful when shape == Stairs).
    // 0 = north (high at -Z, low at +Z), 1 = east (high at +X), 2 = south
    // (high at +Z), 3 = west (high at -X). Data-driven: each stair block
    // variant (stone_stairs / _east / _south / _west) sets its own facing —
    // the project keeps NO per-block facing state (4 block variants instead,
    // like the dye/wool family). Default 0 (north).
    int stairFacing = 0;
    // ENT-2: the explicit per-instance state pair (JSON `model.state_count`
    // / `model.state_default`), strictly paired (both present or both
    // absent — one without the other is rejected). `stateCount` is the
    // number of states [1, 255] (the chunk's state byte holds 0..count-1;
    // 255 is the unset sentinel, so the count never reaches it);
    // `stateDefault` is the default state [0, count) — what the game's
    // setBlock choke point writes on placement and what the collision /
    // mesh / save derivation fall back to when a cell's byte is unset.
    // The Stairs shape does NOT use these fields: it carries an IMPLICIT
    // state (count 4, default = stairFacing — the shipped content stays
    // field-free), and an explicit state_count on a stair is a
    // contradiction (rejected at data load). See Block::hasState /
    // stateCount / stateDefault for the dual gate.
    int stateCount = 0;
    int stateDefault = 0;
    // GEN-9: the MOISTURE top-face variant (full-cube only, empty = the
    // feature is off for this block). When set, the +Y face is meshed with
    // this texture while the soil cell is WET (water within Chebyshev
    // radius kWetWaterRadius at the same y — world/growth's soilIsWet, the
    // crop dry_slowdown rule) and with `textures.top` when dry. The wetness
    // is DERIVED at mesh time from the live fluid field (no per-instance
    // state, no save field): the mesher re-evaluates the top face on every
    // re-mesh, and the game marks tracked cells' chunks dirty when a fluid
    // change flips their wetness (the Game's m_states.moistureBlocks pass). The
    // dropped-item / held-hand cube keep the dry texture (no world context
    // there — the Minecraft inventory icon is static too).
    std::string moistureTop;
};

// Phase 97: the stair's PER-INSTANCE facing when a player PLACES it, from
// the camera yaw (Minecraft: the stair's low side faces where the player
// looks). yaw 0 = facing -Z, positive turns left (the Camera/Player
// convention). facing = the HIGH side, so a player facing -Z (yaw 0) places
// a stair high at +Z (facing 2 / south), facing -X (yaw +pi/2) places it
// high at +X (facing 1 / east), facing +Z (yaw pi) high at -Z (facing 0 /
// north), facing +X (yaw 3pi/2) high at -X (facing 3 / west). A 45-degree
// tie resolves to the X axis (deterministic). Pure function (unit-tested).
inline int stairFacingFromYaw(float yaw) {
    const float fx = -std::sin(yaw); // the camera's forward X component
    const float fz = -std::cos(yaw); // the camera's forward Z component
    if (std::fabs(fx) >= std::fabs(fz)) {
        return fx >= 0.0f ? 3 : 1; // facing +X -> west; facing -X -> east
    }
    return fz >= 0.0f ? 0 : 2; // facing +Z -> north; facing -Z -> south
}

struct Block {
    BlockID id = BlockID::invalid();
    std::string name;
    BlockProperties properties;
    BlockModel model;

    // ENT-2: the per-instance state accessors (the DUAL gate). A block is
    // stateful either through the Stairs shape (IMPLICIT: count 4, default
    // = stair_facing — the shipped stair content carries no state fields)
    // or through the explicit model.state_count/state_default pair (any
    // shape). Pure data reads — the core switches on NONE of these (hard
    // rule 3): the game's choke point, the rotation, the save and the
    // consumers read these accessors.
    bool hasState() const {
        return model.shape == BlockModel::Shape::Stairs ||
               model.stateCount > 0;
    }
    // GEN-2: true when the block carries a `tree` object (a SAPLING — the
    // growth engine tracks it, the game resolves its spec at init).
    bool hasTree() const { return properties.tree.has_value(); }
    int stateCount() const {
        return model.shape == BlockModel::Shape::Stairs ? 4
                                                        : model.stateCount;
    }
    int stateDefault() const {
        return model.shape == BlockModel::Shape::Stairs ? model.stairFacing
                                                        : model.stateDefault;
    }
};

// Hash for BlockID in unordered containers (identity hash).
struct BlockIDHash {
    std::size_t operator()(BlockID id) const noexcept {
        return std::hash<int>{}(id.value);
    }
};

} // namespace mc
