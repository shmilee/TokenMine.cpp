#include "core/save.hpp"
#include "core/thread_pool.hpp"

#include <algorithm>
#include <iterator>
#include <set>
#include <string>
#include <utility>

#include "entity/item_entity.hpp"
#include "item/crafting_grid.hpp"

namespace mc {

namespace {

std::string posStr(const BlockPos& p) {
    return "(" + std::to_string(p.x) + ", " + std::to_string(p.y) + ", " +
           std::to_string(p.z) + ")";
}

// The JSON parser (RFC 8259) cannot produce non-finite numbers, so for a
// parsed document "is a number" already implies finite.
bool isInt(const Json& v, double* out) {
    if (!v.isNumber()) {
        return false;
    }
    const double d = v.asDouble();
    if (d != std::floor(d)) {
        return false;
    }
    *out = d;
    return true;
}

// Rejects every member of `obj` whose key is not in `allowed`.
Result<void> rejectUnknownFields(const Json& obj,
                                 const std::set<std::string>& allowed,
                                 const std::string& what) {
    for (const auto& [key, value] : obj.members()) {
        if (allowed.count(key) == 0) {
            return MC_FAIL(what + " has unknown field '" + key +
                           "' (save files are machine-generated; unknown "
                           "fields mean corruption or a version mismatch)");
        }
    }
    return {};
}

// One { "item", "count } entry, shared by inventory slots and the crafting
// grids (format 3): "" / 0 = empty, otherwise a registered item name with a
// count in [1, maxStack].
Result<void> parseSlotEntry(const Json& e, const ItemRegistry& items,
                            const std::string& what, ItemStack& out) {
    if (!e.isObject()) {
        return MC_FAIL(what + " must be an object");
    }
    const std::set<std::string> slotFields {"item", "count"};
    MC_TRY(rejectUnknownFields(e, slotFields, what));
    const Json* name = e.find("item");
    const Json* count = e.find("count");
    if (name == nullptr || !name->isString() || count == nullptr ||
        !count->isNumber()) {
        return MC_FAIL(what + " requires string 'item' and numeric 'count'");
    }
    const int n = count->asInt();
    if (name->asString().empty()) {
        if (n != 0) {
            return MC_FAIL(what + " is empty ('item' = \"\") but has count " +
                           std::to_string(n));
        }
        out = ItemStack {};
        return {};
    }
    const ItemID id = items.idForName(name->asString());
    if (!id.valid()) {
        return MC_FAIL(what + " holds unknown item '" + name->asString() +
                       "'");
    }
    if (n < 1 || n > items.at(id).maxStack) {
        return MC_FAIL(what + " count " + std::to_string(n) +
                       " is out of range for '" + name->asString() +
                       "' (max stack " +
                       std::to_string(items.at(id).maxStack) + ")");
    }
    out = ItemStack {id, n};
    return {};
}

// One crafting grid (format 3): an array of exactly CraftingGrid::kSize
// slot entries.
Result<void> parseGrid(const Json& arr, const ItemRegistry& items,
                       const std::string& what, std::vector<ItemStack>& out) {
    if (!arr.isArray() ||
        arr.size() != static_cast<std::size_t>(CraftingGrid::kSize)) {
        return MC_FAIL("save '" + what + "' must be an array of exactly " +
                       std::to_string(CraftingGrid::kSize) + " entries");
    }
    out.resize(static_cast<std::size_t>(CraftingGrid::kSize));
    for (std::size_t i = 0; i < arr.size(); ++i) {
        auto r = parseSlotEntry(arr.at(i), items, what + " #" +
                                        std::to_string(i), out[i]);
        MC_TRY(r);
    }
    return {};
}

// One furnace STATE object — the shared shape of the format-6 "furnace"
// field and each format-7 "furnaces" entry (minus the format-7 "pos"):
// three slot entries + the two integer 1/60-substep counters, plus the
// progress invariant (furnace.hpp: progress > 0 implies a smeltable
// input — a save violating it would resurrect progress the tick/UI
// rules say was lost).
Result<void> parseFurnaceState(const Json& fz, const ItemRegistry& items,
                               const std::string& what,
                               FurnaceState& out) {
    const std::set<std::string> fzFields {"input", "fuel", "output",
                                          "progress_substeps",
                                          "fuel_substeps"};
    MC_TRY(rejectUnknownFields(fz, fzFields, what));
    const char* slotNames[] = {"input", "fuel", "output"};
    ItemStack* slotPtrs[] = {&out.input, &out.fuel, &out.output};
    for (int i = 0; i < 3; ++i) {
        const Json* slot = fz.find(slotNames[i]);
        if (slot == nullptr) {
            return MC_FAIL(what + " requires a '" +
                           std::string(slotNames[i]) + "' slot entry");
        }
        MC_TRY(parseSlotEntry(*slot, items,
                              what + " '" + slotNames[i] + "'", *slotPtrs[i]));
    }
    // The two integer 1/60-substep counters (bounds: the max smelt /
    // fuel times the item data allows — see save.hpp).
    const Json* prog = fz.find("progress_substeps");
    if (prog == nullptr || !prog->isNumber() ||
        prog->asDouble() != std::floor(prog->asDouble()) ||
        prog->asDouble() < 0.0 ||
        prog->asDouble() > kFurnaceMaxProgressSubsteps) {
        return MC_FAIL(what + " 'progress_substeps' must be an integer in "
                           "[0, " + std::to_string(kFurnaceMaxProgressSubsteps) +
                           "]");
    }
    out.progressSubsteps = static_cast<int>(prog->asDouble());
    const Json* fuel = fz.find("fuel_substeps");
    if (fuel == nullptr || !fuel->isNumber() ||
        fuel->asDouble() != std::floor(fuel->asDouble()) ||
        fuel->asDouble() < 0.0 || fuel->asDouble() > kFurnaceMaxFuelSubsteps) {
        return MC_FAIL(what + " 'fuel_substeps' must be an integer in "
                           "[0, " + std::to_string(kFurnaceMaxFuelSubsteps) +
                           "]");
    }
    out.fuelSubsteps = static_cast<int>(fuel->asDouble());
    if (out.progressSubsteps > 0 &&
        (out.input.empty() ||
         items.at(out.input.item).smeltableTo.empty())) {
        return MC_FAIL(what + " has smelt progress but its input is empty "
                           "or not smeltable");
    }
    out.deriveProgressFor();
    return {};
}

} // namespace

// -- world diff / apply -------------------------------------------------------

// Compares one live chunk against a freshly generated reference chunk and
// appends the differing cells. A fresh chunk is all-air — exactly what the
// World's generator callback receives at creation, so the comparison is
// apples to apples (the generator writes every cell).
void diffChunk(const World& world, const World::ChunkGenerator& reference,
               const BlockRegistry& blocks, const ChunkPos& cp,
               std::vector<SavedBlockEdit>& outBlockEdits,
               std::vector<SavedFluidEdit>& outFluidEdits) {
    const Chunk* live = world.findChunk(cp);
    if (live == nullptr) {
        return; // cannot happen: positions come from the world
    }
    Chunk generated;
    reference(generated, cp);
    const BlockPos origin = chunkOrigin(cp);
    for (int ly = 0; ly < Chunk::kSize; ++ly) {
        for (int lz = 0; lz < Chunk::kSize; ++lz) {
            for (int lx = 0; lx < Chunk::kSize; ++lx) {
                const BlockPos pos {origin.x + lx, origin.y + ly,
                                    origin.z + lz};
                if (live->get(lx, ly, lz) != generated.get(lx, ly, lz)) {
                    outBlockEdits.push_back(
                        {pos, blocks.at(live->get(lx, ly, lz)).name});
                }
                const std::uint8_t liveFl = live->fluidAt(lx, ly, lz);
                if (liveFl != generated.fluidAt(lx, ly, lz)) {
                    // Encoded comparison (kind + level in one byte); the
                    // edit carries the LIVE cell's kind and level (0 =
                    // drained, either kind).
                    outFluidEdits.push_back(
                        {pos, fluidKindOf(liveFl), fluidLevelOf(liveFl)});
                }
            }
        }
    }
}

Result<void> diffWorld(const World& world,
                       const World::ChunkGenerator& reference,
                       const BlockRegistry& blocks,
                       std::vector<SavedBlockEdit>& outBlockEdits,
                       std::vector<SavedFluidEdit>& outFluidEdits,
                       int workers) {
    const std::vector<ChunkPos> positions = world.chunkPositions();
    if (workers <= 1 ||
        positions.size() <= kSaveDiffParallelMinChunks) {
        for (const ChunkPos& cp : positions) {
            diffChunk(world, reference, blocks, cp, outBlockEdits,
                      outFluidEdits);
        }
    } else {
        // Phase 37: one job per chunk on a throwaway pool. The workers
        // only read the world (findChunk) and call the pure generator —
        // the streaming pool already runs it concurrently — so no World
        // mutation. Each job appends to its own slice; the position sort
        // below is a total order (one edit per cell), so the result is
        // byte-identical to the serial path regardless of scheduling.
        ThreadPool pool(workers);
        std::vector<std::vector<SavedBlockEdit>> blockParts(positions.size());
        std::vector<std::vector<SavedFluidEdit>> fluidParts(positions.size());
        for (std::size_t i = 0; i < positions.size(); ++i) {
            const ChunkPos cp = positions[i];
            pool.post([&, i, cp] {
                diffChunk(world, reference, blocks, cp, blockParts[i],
                          fluidParts[i]);
            });
        }
        pool.flush();
        for (std::size_t i = 0; i < positions.size(); ++i) {
            outBlockEdits.insert(
                outBlockEdits.end(),
                std::make_move_iterator(blockParts[i].begin()),
                std::make_move_iterator(blockParts[i].end()));
            outFluidEdits.insert(
                outFluidEdits.end(),
                std::make_move_iterator(fluidParts[i].begin()),
                std::make_move_iterator(fluidParts[i].end()));
        }
    }
    std::sort(outBlockEdits.begin(), outBlockEdits.end(),
              [](const SavedBlockEdit& a, const SavedBlockEdit& b) {
                  return a.pos < b.pos;
              });
    std::sort(outFluidEdits.begin(), outFluidEdits.end(),
              [](const SavedFluidEdit& a, const SavedFluidEdit& b) {
                  return a.pos < b.pos;
              });
    return {};
}

Result<void> applyWorldEdits(const SaveGame& save, const BlockRegistry& blocks,
                             World& world) {
    // Blocks first: a placement displaces any fluid in its cell, so the
    // fluid edits (validated to never share a cell with a block edit)
    // apply to air cells only.
    for (const SavedBlockEdit& e : save.blockEdits) {
        const BlockID id = blocks.idForName(e.blockName);
        if (!id.valid()) {
            return MC_FAIL("save block edit at " + posStr(e.pos) +
                           " names unknown block '" + e.blockName + "'");
        }
        if (!world.setBlock(e.pos.x, e.pos.y, e.pos.z, id)) {
            return MC_FAIL("cannot apply save block edit at " +
                           posStr(e.pos));
        }
    }
    for (const SavedFluidEdit& e : save.fluidEdits) {
        if (!world.setFluid(e.pos.x, e.pos.y, e.pos.z, e.kind, e.level)) {
            return MC_FAIL("cannot apply save fluid edit at " +
                           posStr(e.pos) + " (no chunk, cell occupied, or "
                           "level unchanged)");
        }
    }
    return {};
}

// -- serialization --------------------------------------------------------------

Result<Json> serializeSave(const SaveGame& save, const ItemRegistry& items) {
    Json root = Json::object();
    root["format"] = Json(kSaveFormat);
    root["seed"] = Json(static_cast<double>(save.seed));
    root["time"] = Json(save.time);

    Json pos = Json::array();
    pos.push_back(Json(save.playerPos.x));
    pos.push_back(Json(save.playerPos.y));
    pos.push_back(Json(save.playerPos.z));
    Json player = Json::object();
    player["pos"] = pos;
    player["yaw"] = Json(save.playerYaw);
    player["pitch"] = Json(save.playerPitch);
    // Format 2: the player's health. -1 (unspecified) omits the field —
    // a missing health loads as full, so this stays a valid file either
    // way. The game always writes the real value (0..max).
    if (save.playerHealth >= 0.0f) {
        player["health"] = Json(save.playerHealth);
    }
    // Format 4: the player's hunger (same omit-when-unspecified pattern).
    if (save.playerHunger >= 0.0f) {
        player["hunger"] = Json(save.playerHunger);
    }
    // Format 5: the player's saturation (the hidden satiety buffer, Phase
    // 28; the game always writes the real value 0..max).
    if (save.playerSaturation >= 0.0f) {
        player["saturation"] = Json(save.playerSaturation);
    }
    // Format 8: the player's active status effects (Phase 46), name-
    // sorted. The key is ALWAYS written (an empty array is valid — the
    // format-8 loader requires it, the furnaces-key discipline).
    {
        std::vector<SavedEffect> effSorted = save.playerEffects;
        std::sort(effSorted.begin(), effSorted.end(),
                  [](const SavedEffect& a, const SavedEffect& b) {
                      return a.name < b.name;
                  });
        Json effectsArr = Json::array();
        for (const SavedEffect& e : effSorted) {
            Json eObj = Json::object();
            eObj["name"] = Json(e.name);
            eObj["remaining"] = Json(e.remaining);
            effectsArr.push_back(std::move(eObj));
        }
        player["effects"] = std::move(effectsArr);
    }
    root["player"] = player;

    Json slots = Json::array();
    for (const ItemStack& s : save.slots) {
        Json e = Json::object();
        if (s.empty()) {
            e["item"] = Json(std::string(""));
            e["count"] = Json(0);
        } else {
            const Item* item = items.find(s.item);
            if (item == nullptr) {
                return MC_FAIL("inventory holds an unregistered item id " +
                               std::to_string(s.item.value));
            }
            e["item"] = Json(item->name);
            e["count"] = Json(s.count);
        }
        slots.push_back(std::move(e));
    }
    Json inventory = Json::object();
    inventory["selected"] = Json(save.selectedSlot);
    inventory["slots"] = slots;
    root["inventory"] = inventory;

    // Format 3: the crafting grids (9 slot entries each; the inventory
    // panel's 2x2 lives in its grid's top-left 4 cells). Always written —
    // format-3 files must carry both, even when empty.
    auto serializeGrid = [&](const std::vector<ItemStack>& grid) -> Result<Json> {
        Json arr = Json::array();
        const ItemStack emptyStack;
        for (int i = 0; i < CraftingGrid::kSize; ++i) {
            const ItemStack& s =
                i < static_cast<int>(grid.size()) ? grid[static_cast<std::size_t>(i)]
                                                  : emptyStack;
            Json e = Json::object();
            if (s.empty()) {
                e["item"] = Json(std::string(""));
                e["count"] = Json(0);
            } else {
                const Item* item = items.find(s.item);
                if (item == nullptr) {
                    return MC_FAIL("a crafting grid holds an unregistered "
                                   "item id " + std::to_string(s.item.value));
                }
                e["item"] = Json(item->name);
                e["count"] = Json(s.count);
            }
            arr.push_back(std::move(e));
        }
        return arr;
    };
    {
        auto invGrid = serializeGrid(save.invGrid);
        MC_TRY(invGrid);
        root["inv_grid"] = std::move(*invGrid);
    }
    {
        auto tableGrid = serializeGrid(save.tableGrid);
        MC_TRY(tableGrid);
        root["table_grid"] = std::move(*tableGrid);
    }

    // Format 7: the per-block furnace states (Phase 44): one entry per
    // furnace block (position-sorted — the game's map order), each an
    // object with the block position + the three slot entries + the two
    // integer 1/60-substep counters. Always written (possibly empty — a
    // world without furnace blocks).
    {
        auto slotJson = [&](const ItemStack& s, const char* what)
            -> Result<Json> {
            Json e = Json::object();
            if (s.empty()) {
                e["item"] = Json(std::string(""));
                e["count"] = Json(0);
            } else {
                const Item* item = items.find(s.item);
                if (item == nullptr) {
                    return MC_FAIL(std::string(what) +
                                   " holds an unregistered item id " +
                                   std::to_string(s.item.value));
                }
                e["item"] = Json(item->name);
                e["count"] = Json(s.count);
            }
            return e;
        };
        Json furnaces = Json::array();
        for (const SavedFurnace& sf : save.furnaces) {
            Json fz = Json::object();
            Json pos = Json::array();
            pos.push_back(Json(sf.pos.x));
            pos.push_back(Json(sf.pos.y));
            pos.push_back(Json(sf.pos.z));
            fz["pos"] = pos;
            auto inSlot = slotJson(sf.state.input, "the furnace input");
            MC_TRY(inSlot);
            fz["input"] = std::move(*inSlot);
            auto fuelSlot = slotJson(sf.state.fuel, "the furnace fuel");
            MC_TRY(fuelSlot);
            fz["fuel"] = std::move(*fuelSlot);
            auto outSlot = slotJson(sf.state.output, "the furnace output");
            MC_TRY(outSlot);
            fz["output"] = std::move(*outSlot);
            fz["progress_substeps"] = Json(sf.state.progressSubsteps);
            fz["fuel_substeps"] = Json(sf.state.fuelSubsteps);
            furnaces.push_back(std::move(fz));
        }
        root["furnaces"] = std::move(furnaces);
    }

    // The burning fire blocks (format 10; Phase 64): one entry per fire
    // block (position-sorted — the game's map order), each an object with
    // the block position + the real-dt seconds remaining (the burn-out
    // countdown; a reload continues burning). Always written (possibly
    // empty — a world without fire).
    {
        Json fires = Json::array();
        for (const SavedFire& sf : save.fires) {
            Json fe = Json::object();
            Json pos = Json::array();
            pos.push_back(Json(sf.pos.x));
            pos.push_back(Json(sf.pos.y));
            pos.push_back(Json(sf.pos.z));
            fe["pos"] = pos;
            fe["remaining"] = Json(sf.remainingSeconds);
            fires.push_back(std::move(fe));
        }
        root["fires"] = std::move(fires);
    }

    Json entities = Json::array();
    for (const SavedItemEntity& e : save.entities) {
        Json jpos = Json::array();
        jpos.push_back(Json(e.pos.x));
        jpos.push_back(Json(e.pos.y));
        jpos.push_back(Json(e.pos.z));
        Json je = Json::object();
        je["type"] = Json(std::string("item"));
        je["item"] = Json(e.item);
        je["count"] = Json(e.count);
        je["pos"] = jpos;
        je["age"] = Json(e.age);
        je["pickup_delay"] = Json(e.pickupDelay);
        entities.push_back(std::move(je));
    }
    root["entities"] = entities;

    Json blockEdits = Json::array();
    for (const SavedBlockEdit& e : save.blockEdits) {
        Json a = Json::array();
        a.push_back(Json(e.pos.x));
        a.push_back(Json(e.pos.y));
        a.push_back(Json(e.pos.z));
        a.push_back(Json(e.blockName));
        blockEdits.push_back(std::move(a));
    }
    root["blocks"] = blockEdits;

    Json fluidEdits = Json::array();
    for (const SavedFluidEdit& e : save.fluidEdits) {
        Json a = Json::array();
        a.push_back(Json(e.pos.x));
        a.push_back(Json(e.pos.y));
        a.push_back(Json(e.pos.z));
        a.push_back(Json(static_cast<int>(e.level)));
        // Format 9: the fluid kind as its data name (five elements).
        a.push_back(Json(fluidKindName(e.kind)));
        fluidEdits.push_back(std::move(a));
    }
    root["fluids"] = fluidEdits;
    return root;
}

// -- parsing --------------------------------------------------------------------

Result<SaveGame> parseSave(const Json& def, const ItemRegistry& items,
                           const BlockRegistry& blocks,
                           const EffectRegistry& effects,
                           std::int64_t expectedSeed) {
    if (!def.isObject()) {
        return MC_FAIL("save file must be a JSON object");
    }
    // "furnace" (format 6) and "furnaces" (format 7) are BOTH known keys —
    // which one a file may carry is enforced per format below (a format-6
    // file must still parse for the Phase 44 migration).
    const std::set<std::string> topLevel {"format", "seed", "time", "player",
                                          "inventory", "inv_grid",
                                          "table_grid", "furnace",
                                          "furnaces", "fires", "entities",
                                          "blocks", "fluids"};
    MC_TRY(rejectUnknownFields(def, topLevel, "save file"));

    const Json* fmt = def.find("format");
    if (fmt == nullptr || !fmt->isNumber()) {
        return MC_FAIL("save file requires 'format' (a number)");
    }
    // Format 1 (Phase 13, no health field) and 2 (Phase 16, player
    // health) both load; anything else is a version mismatch.
    if (fmt->asInt() < kSaveFormatMin || fmt->asInt() > kSaveFormat) {
        return MC_FAIL("unsupported save format " +
                       std::to_string(fmt->asInt()) +
                       " (this build reads formats " +
                       std::to_string(kSaveFormatMin) + "-" +
                       std::to_string(kSaveFormat) + ")");
    }

    const Json* seed = def.find("seed");
    if (seed == nullptr || !seed->isNumber()) {
        return MC_FAIL("save file requires 'seed' (a number)");
    }
    const std::int64_t saveSeed = seed->asInt64();
    if (saveSeed != expectedSeed) {
        return MC_FAIL("save seed " + std::to_string(saveSeed) +
                       " does not match the world seed " +
                       std::to_string(expectedSeed) +
                       " (the edits would apply to the wrong terrain)");
    }

    SaveGame save;
    save.seed = saveSeed;

    const Json* time = def.find("time");
    if (time == nullptr || !time->isNumber() ||
        !(time->asDouble() >= 0.0 && time->asDouble() < 1.0)) {
        return MC_FAIL("save 'time' must be a number in [0, 1)");
    }
    save.time = time->asDouble();

    // -- player ----------------------------------------------------------------
    const Json* player = def.find("player");
    if (player == nullptr || !player->isObject()) {
        return MC_FAIL("save file requires a 'player' object");
    }
    const std::set<std::string> playerFields {"pos", "yaw", "pitch",
                                              "health", "hunger",
                                              "saturation", "effects"};
    MC_TRY(rejectUnknownFields(*player, playerFields, "save 'player'"));
    const Json* ppos = player->find("pos");
    if (ppos == nullptr || !ppos->isArray() || ppos->size() != 3) {
        return MC_FAIL("save 'player.pos' must be an array of 3 numbers");
    }
    save.playerPos = {static_cast<float>(ppos->at(0).asDouble()),
                      static_cast<float>(ppos->at(1).asDouble()),
                      static_cast<float>(ppos->at(2).asDouble())};
    for (std::size_t i = 0; i < 3; ++i) {
        if (!ppos->at(i).isNumber()) {
            return MC_FAIL("save 'player.pos'[" + std::to_string(i) +
                           "] must be a number");
        }
    }
    const Json* yaw = player->find("yaw");
    const Json* pitch = player->find("pitch");
    if (yaw == nullptr || !yaw->isNumber() || pitch == nullptr ||
        !pitch->isNumber()) {
        return MC_FAIL("save 'player' requires numeric 'yaw' and 'pitch'");
    }
    save.playerYaw = static_cast<float>(yaw->asDouble());
    save.playerPitch = static_cast<float>(pitch->asDouble());
    // Optional (absent in format 1): -1 stays "unspecified" and the game
    // applies the player's max health. The upper bound is the player's max
    // (unknown here); the game clamps on load.
    const Json* health = player->find("health");
    if (health != nullptr) {
        if (!health->isNumber() || health->asDouble() < 0.0) {
            return MC_FAIL("save 'player.health' must be a number >= 0");
        }
        save.playerHealth = static_cast<float>(health->asDouble());
    }
    // Optional (absent in format 1-3): -1 stays "unspecified" and the
    // game applies the player's max hunger. Upper bound is the player's
    // max (unknown here); the game clamps on load.
    const Json* hunger = player->find("hunger");
    if (hunger != nullptr) {
        if (!hunger->isNumber() || hunger->asDouble() < 0.0) {
            return MC_FAIL("save 'player.hunger' must be a number >= 0");
        }
        save.playerHunger = static_cast<float>(hunger->asDouble());
    }
    // Optional (absent in format 1-4): -1 stays "unspecified" and the
    // game loads it at 0 (no buffer). Upper bound is the player's max
    // (unknown here); the game clamps on load.
    const Json* saturation = player->find("saturation");
    if (saturation != nullptr) {
        if (!saturation->isNumber() || saturation->asDouble() < 0.0) {
            return MC_FAIL("save 'player.saturation' must be a number >= 0");
        }
        save.playerSaturation = static_cast<float>(saturation->asDouble());
    }
    // Status effects (format 8; Phase 46): absent in format 1-7 files
    // (the effects did not exist — they load empty). Format 8 REQUIRES
    // the "effects" array (an empty array is valid — the furnaces-key
    // discipline): entries {name, remaining}, each name registered,
    // remaining > 0, no duplicate name. The entries are normalized to
    // name-sorted order (the serializer's write order).
    if (def.find("format")->asInt() >= 8) {
        const Json* effArr = player->find("effects");
        if (effArr == nullptr || !effArr->isArray()) {
            return MC_FAIL("save format 8 requires a 'player.effects' "
                           "array (empty when the player had no active "
                           "effects)");
        }
        std::set<std::string> seen;
        for (std::size_t i = 0; i < effArr->size(); ++i) {
            const Json& e = effArr->at(i);
            const std::string what = "save 'player.effects'[" +
                                     std::to_string(i) + "]";
            if (!e.isObject()) {
                return MC_FAIL(what + " must be an object "
                               "{\"name\": ..., \"remaining\": ...}");
            }
            const std::set<std::string> effFields {"name", "remaining"};
            MC_TRY(rejectUnknownFields(e, effFields, what));
            const Json* nm = e.find("name");
            const Json* rem = e.find("remaining");
            if (nm == nullptr || !nm->isString() || nm->asString().empty()) {
                return MC_FAIL(what + " requires a non-empty string "
                               "'name' (effect id)");
            }
            if (rem == nullptr || !rem->isNumber() ||
                rem->asDouble() <= 0.0) {
                return MC_FAIL(what + " requires 'remaining' (seconds) > 0");
            }
            if (effects.findByName(nm->asString()) == nullptr) {
                return MC_FAIL(what + " holds the unregistered effect '" +
                               nm->asString() + "'");
            }
            if (!seen.insert(nm->asString()).second) {
                return MC_FAIL(what + " holds the effect '" +
                               nm->asString() + "' twice");
            }
            save.playerEffects.push_back(
                SavedEffect {nm->asString(), rem->asDouble()});
        }
        std::sort(save.playerEffects.begin(), save.playerEffects.end(),
                  [](const SavedEffect& a, const SavedEffect& b) {
                      return a.name < b.name;
                  });
    }

    // -- inventory ---------------------------------------------------------------
    const Json* inv = def.find("inventory");
    if (inv == nullptr || !inv->isObject()) {
        return MC_FAIL("save file requires an 'inventory' object");
    }
    const std::set<std::string> invFields {"selected", "slots"};
    MC_TRY(rejectUnknownFields(*inv, invFields, "save 'inventory'"));
    const Json* selected = inv->find("selected");
    if (selected == nullptr || !selected->isNumber()) {
        return MC_FAIL("save 'inventory' requires numeric 'selected'");
    }
    save.selectedSlot = selected->asInt();
    if (save.selectedSlot < 0 ||
        save.selectedSlot >= Inventory::kHotbarSize) {
        return MC_FAIL("save 'inventory.selected' must be in [0, " +
                       std::to_string(Inventory::kHotbarSize) +
                       "], got " + std::to_string(save.selectedSlot));
    }
    const Json* slots = inv->find("slots");
    if (slots == nullptr || !slots->isArray() ||
        slots->size() != static_cast<std::size_t>(Inventory::kTotalSize)) {
        return MC_FAIL("save 'inventory.slots' must be an array of exactly " +
                       std::to_string(Inventory::kTotalSize) + " entries");
    }
    save.slots.resize(static_cast<std::size_t>(Inventory::kTotalSize));
    for (std::size_t i = 0; i < slots->size(); ++i) {
        auto r = parseSlotEntry(slots->at(i), items, "save slot #" +
                                       std::to_string(i), save.slots[i]);
        MC_TRY(r);
    }

    // -- crafting grids (format 3; Phase 17) --------------------------------------
    // Absent in format 1/2 files: the grids load empty (the old saves
    // predate them). Required and strictly validated in format 3.
    const int fmtVersion = def.find("format")->asInt();
    if (fmtVersion >= 3) {
        const Json* invGrid = def.find("inv_grid");
        if (invGrid == nullptr) {
            return MC_FAIL("save format 3 requires an 'inv_grid' array");
        }
        MC_TRY(parseGrid(*invGrid, items, "inv_grid", save.invGrid));
        const Json* tableGrid = def.find("table_grid");
        if (tableGrid == nullptr) {
            return MC_FAIL("save format 3 requires a 'table_grid' array");
        }
        MC_TRY(parseGrid(*tableGrid, items, "table_grid", save.tableGrid));
    } else if (def.find("inv_grid") != nullptr ||
               def.find("table_grid") != nullptr) {
        return MC_FAIL("save grids ('inv_grid'/'table_grid') require "
                       "format 3 (got " + std::to_string(fmtVersion) + ")");
    }

    // -- furnace states (format 6/7) -----------------------------------------
    // Parsed AFTER the block edits (below): the format-7 entries are
    // cross-checked against the world's furnace blocks, and the format-6
    // migration (the single global state) needs the same set.

    // -- entities ------------------------------------------------------------------
    const Json* ents = def.find("entities");
    if (ents == nullptr || !ents->isArray()) {
        return MC_FAIL("save file requires an 'entities' array");
    }
    for (std::size_t i = 0; i < ents->size(); ++i) {
        const Json& e = ents->at(i);
        if (!e.isObject()) {
            return MC_FAIL("save entity #" + std::to_string(i) +
                           " must be an object");
        }
        const std::set<std::string> entFields {"type", "item", "count", "pos",
                                               "age", "pickup_delay"};
        MC_TRY(rejectUnknownFields(e, entFields, "save entity #" +
                                                std::to_string(i)));
        const Json* type = e.find("type");
        if (type == nullptr || !type->isString() ||
            type->asString() != "item") {
            return MC_FAIL("save entity #" + std::to_string(i) +
                           ": 'type' must be \"item\" (the only entity "
                           "kind this build saves)");
        }
        SavedItemEntity se;
        const Json* name = e.find("item");
        if (name == nullptr || !name->isString() || name->asString().empty()) {
            return MC_FAIL("save entity #" + std::to_string(i) +
                           " requires a non-empty string 'item'");
        }
        const ItemID id = items.idForName(name->asString());
        if (!id.valid()) {
            return MC_FAIL("save entity #" + std::to_string(i) +
                           " holds unknown item '" + name->asString() + "'");
        }
        const int maxStack =
            std::min(items.at(id).maxStack, ItemEntity::kMaxStack);
        const Json* count = e.find("count");
        if (count == nullptr || !count->isNumber()) {
            return MC_FAIL("save entity #" + std::to_string(i) +
                           " requires numeric 'count'");
        }
        se.count = count->asInt();
        if (se.count < 1 || se.count > maxStack) {
            return MC_FAIL("save entity #" + std::to_string(i) +
                           " count " + std::to_string(se.count) +
                           " is out of range for '" + name->asString() +
                           "' (max " + std::to_string(maxStack) + ")");
        }
        const Json* epos = e.find("pos");
        if (epos == nullptr || !epos->isArray() || epos->size() != 3) {
            return MC_FAIL("save entity #" + std::to_string(i) +
                           " requires 'pos' (an array of 3 numbers)");
        }
        for (std::size_t k = 0; k < 3; ++k) {
            if (!epos->at(k).isNumber()) {
                return MC_FAIL("save entity #" + std::to_string(i) +
                               " 'pos'[" + std::to_string(k) +
                               "] must be a number");
            }
        }
        se.pos = {static_cast<float>(epos->at(0).asDouble()),
                  static_cast<float>(epos->at(1).asDouble()),
                  static_cast<float>(epos->at(2).asDouble())};
        const Json* age = e.find("age");
        if (age == nullptr || !age->isNumber() ||
            !(age->asDouble() >= 0.0 && age->asDouble() <
              ItemEntity::kDespawnSeconds)) {
            return MC_FAIL("save entity #" + std::to_string(i) +
                           " 'age' must be a number in [0, " +
                           std::to_string(ItemEntity::kDespawnSeconds) + ")");
        }
        se.age = age->asDouble();
        const Json* delay = e.find("pickup_delay");
        if (delay == nullptr || !delay->isNumber() ||
            !(delay->asDouble() >= 0.0 && delay->asDouble() <=
              ItemEntity::kPickupDelaySeconds)) {
            return MC_FAIL("save entity #" + std::to_string(i) +
                           " 'pickup_delay' must be a number in [0, " +
                           std::to_string(ItemEntity::kPickupDelaySeconds) +
                           "]");
        }
        se.pickupDelay = delay->asDouble();
        se.item = name->asString();
        save.entities.push_back(std::move(se));
    }

    // -- cell edits ------------------------------------------------------------------
    std::set<BlockPos> blockCells;
    const Json* blks = def.find("blocks");
    if (blks == nullptr || !blks->isArray()) {
        return MC_FAIL("save file requires a 'blocks' array");
    }
    for (std::size_t i = 0; i < blks->size(); ++i) {
        const Json& e = blks->at(i);
        if (!e.isArray() || e.size() != 4) {
            return MC_FAIL("save block edit #" + std::to_string(i) +
                           " must be [x, y, z, block name]");
        }
        double x, y, z;
        if (!isInt(e.at(0), &x) || !isInt(e.at(1), &y) || !isInt(e.at(2), &z)) {
            return MC_FAIL("save block edit #" + std::to_string(i) +
                           ": x, y, z must be integers");
        }
        const int yi = static_cast<int>(y);
        if (!isWorldYValid(yi)) {
            return MC_FAIL("save block edit #" + std::to_string(i) +
                           " has y=" + std::to_string(yi) +
                           " outside the world height [0, " +
                           std::to_string(kWorldHeight) + ")");
        }
        if (!e.at(3).isString() || e.at(3).asString().empty()) {
            return MC_FAIL("save block edit #" + std::to_string(i) +
                           " requires a non-empty block name");
        }
        const std::string name = e.at(3).asString();
        if (!blocks.idForName(name).valid()) {
            return MC_FAIL("save block edit #" + std::to_string(i) +
                           " names unknown block '" + name + "'");
        }
        const BlockPos pos {static_cast<int>(x), yi, static_cast<int>(z)};
        if (blockCells.count(pos) != 0) {
            return MC_FAIL("save has duplicate block edits at " + posStr(pos));
        }
        blockCells.insert(pos);
        save.blockEdits.push_back({pos, name});
    }

    std::set<BlockPos> fluidCells;
    const Json* fls = def.find("fluids");
    if (fls == nullptr || !fls->isArray()) {
        return MC_FAIL("save file requires a 'fluids' array");
    }
    for (std::size_t i = 0; i < fls->size(); ++i) {
        const Json& e = fls->at(i);
        // Format 9 names the fluid kind (five elements); format 8 and
        // earlier carry four — those edits were water (the only simulated
        // fluid before Phase 51), the migration.
        const bool named = fmt->asInt() >= 9;
        if (!e.isArray() || e.size() != (named ? 5u : 4u)) {
            return MC_FAIL("save fluid edit #" + std::to_string(i) +
                           (named ? " must be [x, y, z, level, fluid]"
                                  : " must be [x, y, z, level]"));
        }
        double x, y, z, level;
        if (!isInt(e.at(0), &x) || !isInt(e.at(1), &y) || !isInt(e.at(2), &z) ||
            !isInt(e.at(3), &level)) {
            return MC_FAIL("save fluid edit #" + std::to_string(i) +
                           ": x, y, z, level must be integers");
        }
        const int yi = static_cast<int>(y);
        const int li = static_cast<int>(level);
        if (!isWorldYValid(yi)) {
            return MC_FAIL("save fluid edit #" + std::to_string(i) +
                           " has y=" + std::to_string(yi) +
                           " outside the world height [0, " +
                           std::to_string(kWorldHeight) + ")");
        }
        if (li < 0 || li > kMaxFluidLevel) {
            return MC_FAIL("save fluid edit #" + std::to_string(i) +
                           " level " + std::to_string(li) +
                           " is out of range [0, " +
                           std::to_string(kMaxFluidLevel) + "]");
        }
        // The kind: named in format 9 (vocabulary-checked), water in
        // format 8 and earlier (the migration).
        FluidKind kind = FluidKind::Water;
        if (named) {
            const Json& f = e.at(4);
            if (!f.isString()) {
                return MC_FAIL("save fluid edit #" + std::to_string(i) +
                               ": the fluid kind must be a name string");
            }
            const std::optional<FluidKind> k = fluidKindForName(f.asString());
            if (!k) {
                return MC_FAIL("save fluid edit #" + std::to_string(i) +
                               " has unknown fluid '" + f.asString() +
                               "' (vocabulary: \"water\", \"lava\")");
            }
            kind = *k;
        }
        const BlockPos pos {static_cast<int>(x), yi, static_cast<int>(z)};
        if (blockCells.count(pos) != 0) {
            return MC_FAIL("save has a block edit and a fluid edit at the "
                           "same cell " +
                           posStr(pos) + " (a block displaces fluid)");
        }
        if (fluidCells.count(pos) != 0) {
            return MC_FAIL("save has duplicate fluid edits at " + posStr(pos));
        }
        fluidCells.insert(pos);
        save.fluidEdits.push_back(
            {pos, kind, static_cast<std::uint8_t>(li)});
    }

    // -- furnace states (format 6/7; Phase 40/44) ------------------------------
    // A "furnace block" = a block whose `interact` property names the
    // furnace GUI (data-driven — no block-id switch). World generation
    // never emits furnaces, so the block edits are the complete source of
    // truth for which cells hold one.
    auto isFurnaceBlock = [&](const std::string& name) {
        const BlockID id = blocks.idForName(name);
        return id.valid() &&
               blocks.at(id).properties.interact == kInteractFurnace;
    };
    std::set<BlockPos> furnaceCells;
    for (const SavedBlockEdit& e : save.blockEdits) {
        if (isFurnaceBlock(e.blockName)) {
            furnaceCells.insert(e.pos);
        }
    }
    const Json* fzKey = def.find("furnace");
    const Json* fzArr = def.find("furnaces");
    if (fmtVersion >= 7) {
        // Format 7: per-block states, cross-checked against the world's
        // furnace blocks in BOTH directions (an entry without a block or a
        // block without an entry would desync the game's invariant: every
        // furnace block owns a state).
        if (fzKey != nullptr) {
            return MC_FAIL("save format 7 carries the format-6 'furnace' "
                           "field (replaced by the per-block 'furnaces' "
                           "array)");
        }
        if (fzArr == nullptr || !fzArr->isArray()) {
            return MC_FAIL("save format 7 requires a 'furnaces' array");
        }
        std::set<BlockPos> entryPos;
        for (std::size_t i = 0; i < fzArr->size(); ++i) {
            const Json& e = fzArr->at(i);
            const std::string what =
                "save 'furnaces' entry #" + std::to_string(i);
            if (!e.isObject()) {
                return MC_FAIL(what + " must be an object");
            }
            const std::set<std::string> eFields {
                "pos", "input", "fuel", "output", "progress_substeps",
                "fuel_substeps"};
            MC_TRY(rejectUnknownFields(e, eFields, what));
            const Json* pos = e.find("pos");
            if (pos == nullptr || !pos->isArray() || pos->size() != 3) {
                return MC_FAIL(what + " requires a 'pos' [x, y, z] array");
            }
            double x, y, z;
            if (!isInt(pos->at(0), &x) || !isInt(pos->at(1), &y) ||
                !isInt(pos->at(2), &z)) {
                return MC_FAIL(what + " 'pos' entries must be integers");
            }
            const int yi = static_cast<int>(y);
            if (!isWorldYValid(yi)) {
                return MC_FAIL(what + " has y=" + std::to_string(yi) +
                               " outside the world height [0, " +
                               std::to_string(kWorldHeight) + ")");
            }
            const BlockPos bpos {static_cast<int>(x), yi,
                                 static_cast<int>(z)};
            if (entryPos.count(bpos) != 0) {
                return MC_FAIL(what + " duplicates the furnace position " +
                               posStr(bpos));
            }
            entryPos.insert(bpos);
            SavedFurnace sf {bpos, FurnaceState {}};
            // The state fields (the entry minus its "pos").
            Json stateObj = Json::object();
            for (const auto& [k, v] : e.members()) {
                if (k != "pos") {
                    stateObj[k] = v;
                }
            }
            MC_TRY(parseFurnaceState(stateObj, items, what, sf.state));
            save.furnaces.push_back(std::move(sf));
        }
        if (entryPos != furnaceCells) {
            for (const BlockPos& p : entryPos) {
                if (furnaceCells.count(p) == 0) {
                    return MC_FAIL("save 'furnaces' has an entry at " +
                                   posStr(p) +
                                   " which is not a furnace block in the "
                                   "world's block edits");
                }
            }
            for (const BlockPos& p : furnaceCells) {
                if (entryPos.count(p) == 0) {
                    return MC_FAIL("the furnace block at " + posStr(p) +
                                   " has no 'furnaces' entry (every "
                                   "furnace block owns a state, empty "
                                   "included)");
                }
            }
            return MC_FAIL("save 'furnaces' does not match the world's "
                           "furnace blocks");
        }
    } else if (fmtVersion >= 6) {
        // Format 6: the ONE global state (the Phase 40 single-container
        // v1 rule). Phase 44 migration: it belonged to the world's
        // (single) furnace — with exactly one furnace block it is
        // assigned there; with zero or several blocks the owner is
        // unknowable, so the state is DISCARDED (all states load empty).
        if (fzArr != nullptr) {
            return MC_FAIL("save format 6 carries the format-7 'furnaces' "
                           "field");
        }
        if (fzKey == nullptr || !fzKey->isObject()) {
            return MC_FAIL("save format 6 requires a 'furnace' object");
        }
        FurnaceState global;
        MC_TRY(parseFurnaceState(*fzKey, items, "save 'furnace'", global));
        for (const BlockPos& p : furnaceCells) {
            save.furnaces.push_back(
                furnaceCells.size() == 1 ? SavedFurnace {p, global}
                                         : SavedFurnace {p, FurnaceState {}});
        }
        // furnaceCells is a set: already position-sorted.
    } else if (fzKey != nullptr || fzArr != nullptr) {
        return MC_FAIL("save 'furnace'/'furnaces' requires format 6 or "
                       "newer (got " + std::to_string(fmtVersion) + ")");
    }
    // The game's invariant after ANY load: every furnace block in the
    // world owns a state entry. Format 7 guarantees it (the cross-check),
    // format 6 adds it (the migration), formats 1-5 carry no entries at
    // all — add the empty ones.
    for (const BlockPos& p : furnaceCells) {
        bool have = false;
        for (const SavedFurnace& sf : save.furnaces) {
            if (sf.pos == p) {
                have = true;
                break;
            }
        }
        if (!have) {
            save.furnaces.push_back({p, FurnaceState {}});
        }
    }

    // -- burning fires (format 10; Phase 64) ------------------------------
    // A "fire block" = the block named "fire" — the burn target of the
    // Phase 64 burn semantics (the single named reference the init
    // cross-check enforces: a flamable block in the registry requires the
    // fire block to exist). World generation never emits fire, so the
    // block edits are the complete source of truth for which cells hold
    // one — exactly like the furnace cells above.
    std::set<BlockPos> fireCells;
    for (const SavedBlockEdit& e : save.blockEdits) {
        if (e.blockName == "fire") {
            fireCells.insert(e.pos);
        }
    }
    const Json* fireArr = def.find("fires");
    if (fmtVersion >= 10) {
        // Format 10: cross-checked against the world's fire blocks in BOTH
        // directions (an entry without a block or a block without an entry
        // would desync the game's invariant: every fire block owns a
        // remaining-life entry).
        if (fireArr == nullptr || !fireArr->isArray()) {
            return MC_FAIL("save format 10 requires a 'fires' array");
        }
        std::set<BlockPos> entryPos;
        for (std::size_t i = 0; i < fireArr->size(); ++i) {
            const Json& e = fireArr->at(i);
            const std::string what =
                "save 'fires' entry #" + std::to_string(i);
            if (!e.isObject()) {
                return MC_FAIL(what + " must be an object");
            }
            const std::set<std::string> eFields {"pos", "remaining"};
            MC_TRY(rejectUnknownFields(e, eFields, what));
            const Json* pos = e.find("pos");
            if (pos == nullptr || !pos->isArray() || pos->size() != 3) {
                return MC_FAIL(what + " requires a 'pos' [x, y, z] array");
            }
            double x, y, z;
            if (!isInt(pos->at(0), &x) || !isInt(pos->at(1), &y) ||
                !isInt(pos->at(2), &z)) {
                return MC_FAIL(what + " 'pos' entries must be integers");
            }
            const int yi = static_cast<int>(y);
            if (!isWorldYValid(yi)) {
                return MC_FAIL(what + " has y=" + std::to_string(yi) +
                               " outside the world height [0, " +
                               std::to_string(kWorldHeight) + ")");
            }
            const BlockPos bpos {static_cast<int>(x), yi,
                                 static_cast<int>(z)};
            if (entryPos.count(bpos) != 0) {
                return MC_FAIL(what + " duplicates the fire position " +
                               posStr(bpos));
            }
            entryPos.insert(bpos);
            const Json* rem = e.find("remaining");
            if (rem == nullptr || !rem->isNumber() ||
                rem->asDouble() <= 0.0) {
                return MC_FAIL(what + " requires 'remaining' (seconds) > 0");
            }
            save.fires.push_back(SavedFire {bpos, rem->asDouble()});
        }
        if (entryPos != fireCells) {
            for (const BlockPos& p : entryPos) {
                if (fireCells.count(p) == 0) {
                    return MC_FAIL("save 'fires' has an entry at " +
                                   posStr(p) +
                                   " which is not a fire block in the "
                                   "world's block edits");
                }
            }
            for (const BlockPos& p : fireCells) {
                if (entryPos.count(p) == 0) {
                    return MC_FAIL("the fire block at " + posStr(p) +
                                   " has no 'fires' entry (every fire "
                                   "block owns a remaining-life entry)");
                }
            }
            return MC_FAIL("save 'fires' does not match the world's fire "
                           "blocks");
        }
    } else if (fireArr != nullptr) {
        return MC_FAIL("save 'fires' requires format 10 or newer (got " +
                       std::to_string(fmtVersion) + ")");
    }
    // The game's invariant after ANY load: every fire block in the world
    // owns a remaining-life entry. Format 10 guarantees it (the
    // cross-check); format 9 and earlier carry no entries at all (the fire
    // block did not exist — a hand-edited fire edit in an old file is
    // possible) — the game adds the full-lifetime entries on load (like
    // the empty furnace states).
    return save;
}

} // namespace mc
