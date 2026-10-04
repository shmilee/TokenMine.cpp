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
//   "format": 1,
//   "seed": 123456789,
//   "time": 0.2541666,                       // timeOfDay, [0, 1)
//   "player": { "pos": [0.5, 53.0, 0.5], "yaw": 0.0, "pitch": -0.25 },
//   "inventory": {
//     "selected": 0,
//     "slots": [ { "item": "stone", "count": 64 }, { "item": "", "count": 0 } ]
//   },                                        // exactly Inventory::kTotalSize
//   "entities": [ { "type": "item", "item": "dirt", "count": 1,
//                   "pos": [0.5, 52.5, 0.5], "age": 12.5,
//                   "pickup_delay": 0.25 } ],
//   "blocks": [ [0, 53, 0, "air"] ],          // cell edits vs generation
//   "fluids": [ [4, 50, 4, 5] ]               // (name "air" = block removed)
// }
//
// Validation on load is STRICT (the file is machine-generated, so an
// unexpected field means corruption or a version mismatch — unlike
// user-authored data files, unknown fields are an ERROR, not a warning):
//   - format must be 1; the seed must equal the seed the world was
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
inline constexpr int kSaveFormat = 1;

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
    int selectedSlot = 0;
    std::vector<ItemStack> slots; // exactly Inventory::kTotalSize
    std::vector<SavedItemEntity> entities;
    std::vector<SavedBlockEdit> blockEdits; // sorted by pos
    std::vector<SavedFluidEdit> fluidEdits; // sorted by pos
};

// Compares every existing chunk of `world` against `reference` (called
// with a fresh scratch Chunk per ChunkPos — the same pure generator the
// World used at creation) and emits the cells that differ, sorted by
// position. `blocks` resolves block ids to their data names.
Result<void> diffWorld(const World& world, const World::ChunkGenerator& reference,
                       const BlockRegistry& blocks,
                       std::vector<SavedBlockEdit>& outBlockEdits,
                       std::vector<SavedFluidEdit>& outFluidEdits);

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
