// World save (Phase 13): seed + modifications + full game state.
//
// A save file stores the world seed plus only what DIFFERS from the world
// that seed generates (block/fluid cell edits), plus the non-world state:
// player position/orientation, the day-cycle time, the 27-slot inventory
// and the dropped-item entities (with their lifetime timers). Loading a
// save = generate the world from the seed (the normal startup path) and
// apply the saved state on top — no voxel data is duplicated in the file.
//
// File layout (data/saves/<slot>.json, pretty-printed):
//
// {
//   "format": 2,
//   "seed": 123456789,
//   "time": 0.2541666,                       // timeOfDay, [0, 1)
//   "player": { "pos": [0.5, 53.0, 0.5], "yaw": 0.0, "pitch": -0.25,
//               "health": 20.0,              // format 2+ (format 1: full)
//               "hunger": 20.0,              // format 4+ (older: full)
//               "saturation": 6.0,           // format 5+ (older: 0)
//               "effects": [                 // format 8+ (older: none)
//                 { "name": "poison", "remaining": 12.5 } ] },
//   "inventory": {
//     "selected": 0,
//     "slots": [ { "item": "stone", "count": 64 }, { "item": "", "count": 0 } ]
//   },                                        // exactly Inventory::kTotalSize
//   "inv_grid": [ ... 9 entries ... ],        // format 3+: the inventory
//   "table_grid": [ ... 9 entries ... ],      // panel's grid + the table's
//   "entities": [ { "type": "item", "item": "dirt", "count": 1,
//                   "pos": [0.5, 52.5, 0.5], "age": 12.5,
//                   "pickup_delay": 0.25 } ], // grid (same slot entries)
//   "blocks": [ [0, 53, 0, "air"] ],          // cell edits vs generation
//   "fluids": [ [4, 50, 4, 5, "water"] ]      // (name "air" = block
// }                                            // removed; format 9: the
//                                              // fluid kind is named)
//
// Validation on load is STRICT (the file is machine-generated, so an
// unexpected field means corruption or a version mismatch — unlike
// user-authored data files, unknown fields are an ERROR, not a warning):
//   - format must be 1..5 (format 1 files predate the player health field
//     and load with full health; format 2 predates the crafting grids and
//     loads with empty grids; format 1-3 predates the hunger field and
//     loads with full hunger; format 1-4 predates the saturation field
//     and loads it at 0); the seed must equal the seed the world was
//     generated with; every item/block name must be registered; counts
//     respect the item data (1..maxStack, entity cap 64); y in [0, 128);
//     no duplicate edit position, and a block edit and a fluid edit share
//     a cell only when the implied live state is a single occupant (an
//     "air" block edit + positive fluid = the fluid flowed into the
//     burned/broken cell; a non-air block edit + level-0 fluid = the
//     block displaced the generated fluid). A solid block and a positive
//     fluid in one cell is rejected.
//
// diffWorld() compares every existing chunk against the reference
// generation (the same pure generator the World itself used) and emits
// the differing cells; applyWorldEdits() replays them (blocks first,
// then fluids) and fails loudly if an edit cannot be applied. Both are
// free functions over World + registries — no game or GL dependency — so
// the whole save/load core is unit-testable headless.
#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "block/block_registry.hpp"
#include "core/error.hpp"
#include "core/json.hpp"
#include "core/math/vec3.hpp"
#include "core/status_effect.hpp"
#include "item/campfire.hpp"
#include "item/furnace.hpp"
#include "item/inventory.hpp"
#include "world/chunk_streaming.hpp"
#include "world/coordinates.hpp"
#include "world/world.hpp"

namespace mc {

// Save format version (bump when the schema changes).
//   1: original (Phase 13) — no player health field.
//   2: player health (Phase 16 survival). Format-1 files still load
//      (health defaults to full).
//   3: the crafting grids (Phase 17) — "inv_grid" + "table_grid" (9 slot
//      entries each). Format 1/2 files still load (grids default to
//      empty); writes are always format 3.
//   4: the player's hunger (Phase 23). Format 1-3 files still load
//      (hunger defaults to full).
//   5: the player's saturation (Phase 28, the hidden satiety buffer).
//      Format 1-4 files still load (saturation defaults to 0).
//   6: the furnace state (Phase 40) — "furnace": the single smelting
//      state (the single-container v1 rule, see item/furnace.hpp) as
//      three slot entries + "progress_substeps" + "fuel_substeps"
//      (non-negative integer 1/60-substep counters). Format 1-5 files
//      still load (the furnace defaults to empty); writes were always
//      format 6.
//   7: the per-block furnace states (Phase 44) — "furnaces": one entry
//      per FURNACE BLOCK in the world (position-sorted), each with its
//      position + the same state fields as the format-6 "furnace"
//      object. Format-6 files still load: the ONE global state is
//      migrated to the sole furnace block of the world, or discarded
//      (all entries empty) when the world holds zero or several
//      furnace blocks (which one owned the shared state is unknowable);
//      writes were always format 7.
//   8: the player's active status effects (Phase 46) — "player":
//      { ..., "effects": [ { "name": ..., "remaining": ... } ] },
//      name-sorted, `remaining` in (0, ...] seconds (the real-dt
//      countdown; the tick phase is NOT saved — a reload restarts it at
//      the full interval). Format-7 and older files still load (no
//      effects); format-8 files REQUIRE the "effects" key (an empty
//      array is valid — the furnaces-key discipline); writes are always
//      format 8.
//   9: the fluid KIND on every fluid edit (Phase 51, lava) — "fluids":
//      [ [x, y, z, level, "water"|"lava"] ] (five elements; the kind is a
//      fluid NAME from the two-kind vocabulary). Format-8 and older files
//      still load: their four-element edits migrate to water (the only
//      simulated fluid before Phase 51); writes are always format 9.
//   10: the burning fire blocks (Phase 64, the fire visuals) — "fires":
//      [ { "pos": [x, y, z], "remaining": ... } ] (position-sorted), one
//      entry per FIRE BLOCK in the world, `remaining` in (0, ...] seconds
//      (the real-dt burn-out countdown; a reload continues burning —
//      like the player's fire effect). Format-9 and older files still
//      load (no fires — the fire block did not exist); format-10 files
//      REQUIRE the "fires" key (an empty array is valid — the furnaces-
//      key discipline); writes are always format 10.
//   11: the campfire cooking states (Phase 71) — "campfires":
//      [ { "pos": [x, y, z], "item": ..., "count": ...,
//      "progress_substeps": ... } ] (position-sorted), one entry per
//      CAMPFIRE BLOCK in the world (an empty slot is an entry with
//      "item": "" / "count": 0 / "progress_substeps": 0). The cross-checks
//      run in BOTH directions (the furnaces-key discipline) plus the
//      progress invariant: progress_substeps > 0 requires the entry to
//      hold exactly ONE smeltable item (the item/campfire.hpp tick
//      maintains it). Format-10 and older files still load (no campfires
//      — the campfire block did not exist); a format-11 file REQUIREs the
//      "campfires" key (an empty array is valid); writes are always
//      format 11.
//   12: the two-component absorption (Phase 72) — "player" gains
//      "absorption" (a number in [0, 64], the pool's hard cap; the value
//      may sit BELOW the saved effects' total `absorb_hp` — damage
//      drains the pool between effect-change sync points — but may never
//      exceed it). A format-11 file carrying the key is rejected (the
//      new-key discipline); format-11 and older files load via the
//      SHARED-HEALTH MIGRATION in Game::applySave (the excess above the
//      base max becomes the pool, the health keeps the base part);
//      writes are always format 12 (the field is always written — the
//      pool is 0 when no layer is active).
//   13: the campfire's FOUR slots + the extinguished block (Phase 75) —
//      "campfires" entries gain "slots": an array of EXACTLY 4 objects
//      [ { "item", "count", "progress_substeps" } ] (the per-slot rules
//      are the format-11 entry's: count 0 with "item" "" or count 1 with
//      a registered item; the progress invariant holds PER SLOT —
//      progress > 0 requires that slot to hold exactly one smeltable
//      item). The bidirectional cross-check runs against the CAMPFIRE
//      FAMILY: a "campfire" (lit) OR "campfire_extinguished" (water put
//      it out, Phase 75) block edit requires an entry, and an entry
//      requires a family block. Format-11/12 files carry the OLD single-
//      slot entry shape (no "slots" key — carrying it is a version lie,
//      rejected by the per-format field whitelist) and load via the
//      SINGLE-SLOT MIGRATION (the old item/count/progress land in slot 0,
//      the other three slots are empty); their cross-check is the
//      lit-only one (the extinguished block did not exist). A
//      format-13 file requires the new shape (an old-shape entry is
//      rejected); writes are always format 13.
//  14: the per-instance stair facings (Phase 94) — "stairs": an array of
//      {x, y, z, facing} (position-sorted), one entry per stair block in
//      the world (the project's first per-block state; the facing is the
//      World's per-cell byte). A format-14 file REQUIREs the "stairs" key
//      (an empty array is valid) and rejects a "states" key (a version
//      lie); a format-13 load migrates into this empty (the per-instance
//      facing did not exist; the stair block's stair_facing default is
//      used on load). The entries are cross-checked against the block
//      edits in both directions (every stair cell has a facing, every
//      facing is a stair cell). A format-14 file LOADS via the
//      VALUE-PRESERVING MIGRATION into the format-15 states (facing ->
//      state); a format-15 file never carries "stairs".
//  15: the per-instance block states (ENT-2, the Phase 94 stair facing
//      generalized to any stateful block — the Stairs shape OR an explicit
//      model.state_count) — "states": an array of {x, y, z, state}
//      (position-sorted), one entry per stateful block in the world. The
//      state is validated per entry against the block at the entry's
//      position (a block edit's name -> registry -> [0, stateCount())) and
//      cross-checked against the block edits in both directions (every
//      stateful cell has a state, every state is a stateful cell — the
//      furnaces-key discipline, generalized). A format-15 file REQUIREs
//      the "states" key (an empty array is valid) and rejects a "stairs"
//      key (restricted to format 14, a version lie); a format-14 file
//      loads via the value-preserving migration above, and a format-13
//      load migrates into this empty (the per-instance state did not
//      exist; the block's state default is used on load). Writes are
//      always format 15.
inline constexpr int kSaveFormat = 15;
inline constexpr int kSaveFormatMin = 1;

// Phase 40: the furnace save-field bounds — the max smelt/fuel times the
// item data allows (smelt_time ≤ 100 s, fuel_time ≤ 1000 s; see
// ItemRegistry::parseFurnace) in 1/60 substeps.
inline constexpr int kFurnaceMaxProgressSubsteps = 100 * 60;
inline constexpr int kFurnaceMaxFuelSubsteps = 1000 * 60;

// Phase 37: the world diff (P33 diagnosis: ~0.14 ms/chunk single-threaded —
// a ~100k-chunk world took ~14 s to save at exit) runs on a throwaway pool
// of this many workers when the world holds more than
// kSaveDiffParallelMinChunks chunks.
inline constexpr int kSaveDiffWorkers = 4;
// Thread spawning costs more than the diff below this many chunks
// (~0.14 ms/chunk over 4 workers vs a few ms of spawn) — small worlds keep
// the serial path.
inline constexpr std::size_t kSaveDiffParallelMinChunks = 256;

// PIPE-2: the save journal's soft cap on the TOTAL recorded cell count
// (both tables). A normal session holds a few thousand entries (player
// edits + the fluid/burn/growth paths they trigger); beyond the cap an
// extreme session (a massive lava flood, a scripted world rebuild) marks
// the journal invalid and the exit save falls back to the legacy full
// diffWorld (the INFO log names it) — the journal must never grow
// unbounded. 262144 = 256 x 1024 (the two round numbers).
inline constexpr std::size_t kSaveJournalMaxEntries = 262144;

// PIPE-2: the session's cell-edit JOURNAL as position sets. The exit save
// is {existing cell c : c is in the journal AND live(c) != fresh(c)} —
// byte-identical to the full diffWorld (see journalDiff) — because every
// post-generation cell write goes through World::setBlock / setFluid (the
// observer choke point), so a cell that differs from generation was
// written this session and is recorded. VALUES are not stored (the value
// is the live world at exit); the displacement trap (a setBlock into a
// fluid cell drains it via the internal setFluidAt) is covered by the
// observer's double report, which lands the cell in BOTH tables. The
// column maps are position-sorted (deterministic iteration).
struct SaveJournal {
    // Per column: the cells whose BLOCK was written.
    std::map<ColumnPos, std::set<BlockPos>> blocks;
    // Per column: the cells whose FLUID was written.
    std::map<ColumnPos, std::set<BlockPos>> fluids;
    std::size_t entries = 0; // the total, both tables
    bool valid = true;

    // Records a written cell (idempotent per table). Returns true
    // EXACTLY ONCE — when the total first exceeds kSaveJournalMaxEntries
    // and the journal falls back to invalid (further records are no-ops;
    // the exit save then uses the legacy diffWorld).
    bool recordBlock(const BlockPos& pos);
    bool recordFluid(const BlockPos& pos);
    // Unload eviction (the Game's unloadFarColumns): a column that leaves
    // the world re-enters only as FRESH streamed terrain (any edit it
    // held is gone with it), so its entries are dropped and the total
    // count stays bounded over a long wandering session.
    void evictColumn(const ColumnPos& col);
};

// One cell whose block differs from generation ("air" = removed).
struct SavedBlockEdit {
    BlockPos pos;
    std::string blockName; // block NAME (registry-resolved on load)
};

// One cell whose fluid differs from generation (level 0 = drained). The
// KIND is the fluid's name-bearing enum (Phase 51, format 9: format-8 and
// older files migrate to Water — the only simulated fluid then).
struct SavedFluidEdit {
    BlockPos pos;
    FluidKind kind;
    std::uint8_t level;
};

// One furnace block's smelting state (format 7; Phase 44). The game keeps
// an entry for EVERY furnace block in the world (empty states included) —
// the save cross-checks the entries against the block edits in both
// directions.
struct SavedFurnace {
    BlockPos pos;
    FurnaceState state;
};

// One burning fire block's remaining life (format 10; Phase 64). The game
// keeps an entry for EVERY fire block in the world (the map is the single
// source of truth) — the save cross-checks the entries against the block
// edits in both directions, exactly like the furnace states.
struct SavedFire {
    BlockPos pos;
    double remainingSeconds = 0.0; // real-dt seconds left before burn-out
};

// One campfire block's cooking state (format 11; Phase 71). The game
// keeps an entry for EVERY campfire block in the world (empty states
// included) — the save cross-checks the entries against the block edits
// in both directions (the furnaces-key discipline).
struct SavedCampfire {
    BlockPos pos;
    CampfireCookState state;
};

// One stateful block's per-instance state (format 15; Phase 94 stair
// facing, generalized in ENT-2) — the project's first per-block STATE (a
// breakthrough of the "no per-instance state" philosophy). A "stateful"
// block is one whose Block::hasState() (the Stairs shape — count 4, the
// facing 0-3 north/east/south/west — or an explicit model.state_count).
// The game keeps an entry for EVERY stateful block in the world (the state
// is the World's per-cell byte), cross-checked against the block edits in
// both directions (the furnaces-key discipline). A format-14 load migrates
// its "stairs" entries in VALUE-PRESERVING (facing -> state); a format-13
// load migrates into this empty (the per-instance state did not exist; the
// block's state default is used on load).
struct SavedState {
    BlockPos pos;
    std::uint8_t state = 0; // [0, block.stateCount()); a stair: 0-3
};

// One active player status effect (format 8; Phase 46): the effect NAME
// (registry-resolved on load) + the real-dt seconds remaining. The tick
// phase is deliberately not saved (a reload restarts the next tick at
// the full interval).
struct SavedEffect {
    std::string name;
    double remaining = 0.0;
};

// One dropped-item entity (the only entity kind the game spawns).
struct SavedItemEntity {
    std::string item; // item NAME (registry-resolved on load)
    int count = 1;
    Vec3 pos;         // bottom-center
    double age = 0.0; // seconds alive (< ItemEntity::kDespawnSeconds)
    // Remaining pickup delay (0..ItemEntity::kPickupDelaySeconds).
    double pickupDelay = 0.0;
};

// Everything one save file holds (state already resolved to names, so
// serialization does not need the registries — parsing does).
struct SaveGame {
    std::int64_t seed = 0;
    double time = 0.0; // timeOfDay, [0, 1)
    Vec3 playerPos;
    float playerYaw = 0.0f;
    float playerPitch = 0.0f;
    // Health (Phase 16). -1 = "unspecified" (format-1 files): the game
    // applies the player's max health. A saved 0 means the player was
    // dead when the file was written. Phase 72: this is the BASE-pool
    // value only (<= the base max); the absorption pool is separate
    // (playerAbsorption).
    float playerHealth = -1.0f;
    // Absorption (Phase 72, the two-component model). -1 = "unspecified"
    // (format 1-11 files): the game migrates the shared P56 health in
    // applySave (the excess above the base max becomes the pool). A
    // format-12 file REQUIREs the field (parse-validated: [0, 64] and
    // <= the saved effects' total `absorb_hp`).
    float playerAbsorption = -1.0f;
    // Hunger (Phase 23). -1 = "unspecified" (format 1-3 files): the game
    // applies the player's max hunger on load.
    float playerHunger = -1.0f;
    // Saturation (Phase 28, the hidden satiety buffer, [0, max hunger]).
    // -1 = "unspecified" (format 1-4 files): the game loads it at 0
    // (no buffer — the pre-Phase-28 behavior).
    float playerSaturation = -1.0f;
    int selectedSlot = 0;
    std::vector<ItemStack> slots; // exactly Inventory::kTotalSize
    // The crafting grids (format 3; Phase 17): the inventory panel's grid
    // and the crafting table's grid, 9 cells each (CraftingGrid::kSize).
    // Empty for format 1/2 saves (the game leaves the grids empty).
    std::vector<ItemStack> invGrid;
    std::vector<ItemStack> tableGrid;
    // The per-block furnace states (format 7; Phase 44): one entry per
    // furnace block in the world (position-sorted; the game's map order).
    // Empty when the world holds no furnace blocks; a format-6 load is
    // migrated into this (see kSaveFormat).
    std::vector<SavedFurnace> furnaces;
    // The burning fire blocks (format 10; Phase 64): one entry per fire
    // block in the world (position-sorted; the game's map order), each with
    // its real-dt remaining life. Empty when the world holds no fire; a
    // format-9 load migrates into this empty (fires did not exist).
    std::vector<SavedFire> fires;
    // The campfire cooking states (format 11; Phase 71): one entry per
    // campfire block in the world (position-sorted; the game's map order),
    // empty states included. Empty when the world holds no campfire; a
    // format-10 load migrates into this empty (the campfire block did not
    // exist).
    std::vector<SavedCampfire> campfires;
    // The per-instance block states (format 15; Phase 94 stair facing,
    // generalized in ENT-2): one entry per stateful block in the world
    // (position-sorted; the World's per-cell byte order). Empty when the
    // world holds no stateful block; a format-14 load migrates its stairs
    // in value-preserving, a format-13 load into this empty (the
    // per-instance state did not exist).
    std::vector<SavedState> states;
    // The player's active status effects (format 8; Phase 46), name-
    // sorted. Empty for format 1-7 loads (the effects did not exist).
    std::vector<SavedEffect> playerEffects;
    std::vector<SavedItemEntity> entities;
    std::vector<SavedBlockEdit> blockEdits; // sorted by pos
    std::vector<SavedFluidEdit> fluidEdits; // sorted by pos
};

// Compares every existing chunk of `world` against `reference` (called
// with a fresh scratch Chunk per ChunkPos — the same pure generator the
// World used at creation) and emits the cells that differ, sorted by
// position. `blocks` resolves block ids to their data names.
// `workers` (Phase 37): above kSaveDiffParallelMinChunks chunks the
// per-chunk comparison runs one chunk per job on a throwaway pool of
// `workers` threads; below it (or with workers <= 1) the serial loop runs.
// The position-sorted output is a total order (one edit per cell), so the
// result is byte-identical in both paths.
Result<void> diffWorld(const World& world, const World::ChunkGenerator& reference,
                       const BlockRegistry& blocks,
                       std::vector<SavedBlockEdit>& outBlockEdits,
                       std::vector<SavedFluidEdit>& outFluidEdits,
                       int workers = 1);

// PIPE-2: the INCREMENTAL variant of diffWorld — byte-identical output
// (same per-cell comparison, same encoded-fluid byte with the
// fluidKindOf(0)=Water level-0 special case, same position total sort),
// restricted to the journal: for each chunk that EXISTS and that the
// journal touches, the reference is regenerated (the same pure
// generator) and only the journal cells are compared; a journal cell
// whose live value equals the fresh value emits nothing (a same-value
// revert, a burned-then-regrown cell, a drained-then-refilled cell).
// `workers` mirrors diffWorld (P37: above kSaveDiffParallelMinChunks
// touched chunks the per-chunk comparison runs one chunk per job on a
// throwaway pool — the workers read the world and run the pure
// generator, no World mutation; the position-sorted output is a total
// order, so both paths are byte-identical).
Result<void> journalDiff(const World& world,
                         const World::ChunkGenerator& reference,
                         const BlockRegistry& blocks,
                         const std::map<ColumnPos, std::set<BlockPos>>&
                             journalBlocks,
                         const std::map<ColumnPos, std::set<BlockPos>>&
                             journalFluids,
                         std::vector<SavedBlockEdit>& outBlockEdits,
                         std::vector<SavedFluidEdit>& outFluidEdits,
                         int workers = 1);

// Replays the saved cell edits onto a freshly generated `world`: block
// edits first (a placement displaces any fluid in the cell), then fluid
// edits. Fails (naming the position) when an edit cannot be applied —
// e.g. a fluid edit in a cell the generated world fills with a block, or
// in a chunk that does not exist and no block edit created.
Result<void> applyWorldEdits(const SaveGame& save, const BlockRegistry& blocks,
                             World& world);

// Serializes a save to JSON (format kSaveFormat). `items` resolves the
// slot ItemIDs back to names.
Result<Json> serializeSave(const SaveGame& save, const ItemRegistry& items);

// Parses and strictly validates a save file, cross-checking item/block/
// effect names and stack bounds against the registries. `expectedSeed`
// is the seed the world was generated with (a mismatch is an error — the
// edits would apply to the wrong terrain).
Result<SaveGame> parseSave(const Json& def, const ItemRegistry& items,
                           const BlockRegistry& blocks,
                           const EffectRegistry& effects,
                           std::int64_t expectedSeed);

} // namespace mc
