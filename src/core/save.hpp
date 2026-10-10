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
//               "saturation": 6.0 },         // format 5+ (older: 0)
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
//   "fluids": [ [4, 50, 4, 5] ]               // (name "air" = block removed)
// }
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
//     no duplicate edit position, and a block edit and a fluid edit can
//     never share a cell (a block displaces fluid).
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
#include <string>
#include <vector>

#include "block/block_registry.hpp"
#include "core/error.hpp"
#include "core/json.hpp"
#include "core/math/vec3.hpp"
#include "item/inventory.hpp"
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
inline constexpr int kSaveFormat = 5;
inline constexpr int kSaveFormatMin = 1;

// Phase 37: the world diff (P33 diagnosis: ~0.14 ms/chunk single-threaded —
// a ~100k-chunk world took ~14 s to save at exit) runs on a throwaway pool
// of this many workers when the world holds more than
// kSaveDiffParallelMinChunks chunks.
inline constexpr int kSaveDiffWorkers = 4;
// Thread spawning costs more than the diff below this many chunks
// (~0.14 ms/chunk over 4 workers vs a few ms of spawn) — small worlds keep
// the serial path.
inline constexpr std::size_t kSaveDiffParallelMinChunks = 256;

// One cell whose block differs from generation ("air" = removed).
struct SavedBlockEdit {
    BlockPos pos;
    std::string blockName; // block NAME (registry-resolved on load)
};

// One cell whose fluid level differs from generation (0 = drained).
struct SavedFluidEdit {
    BlockPos pos;
    std::uint8_t level;
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
    // dead when the file was written.
    float playerHealth = -1.0f;
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

// Parses and strictly validates a save file, cross-checking item/block
// names and stack bounds against the registries. `expectedSeed` is the
// seed the world was generated with (a mismatch is an error — the edits
// would apply to the wrong terrain).
Result<SaveGame> parseSave(const Json& def, const ItemRegistry& items,
                           const BlockRegistry& blocks,
                           std::int64_t expectedSeed);

} // namespace mc
