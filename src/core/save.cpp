#include "core/save.hpp"
#include "core/thread_pool.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <set>
#include <string>
#include <utility>

#include "entity/item_entity.hpp"
#include "entity/mob_registry.hpp" // SURV-7: validateStats' mob names
#include "item/crafting_grid.hpp"
#include "player/drown.hpp"
#include "world/chunk_streaming.hpp"

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
// count in [1, maxStack]. SURV-3 (format 18): the OPTIONAL "damage" key
// (the accumulated durability, written only when > 0) is accepted only for
// `allowDamage` callers (the inventory slots — the grids / furnace /
// campfire slots do not carry it, the format-18 scope) and is
// cross-checked against the item's data durability.
// SURV-3 (format 18): the per-stack "damage" key's value rules, shared by
// the slot / entity / armor-object entries: a number >= 0, on an item that
// HAS a data durability, and strictly BELOW it (the invariant
// 0 <= damage < item.durability — a broken item is removed, it has no
// save form).
Result<void> checkDamageValue(const Json& dmg, const Item& item,
                              const std::string& what, float& out) {
    if (!dmg.isNumber() || dmg.asDouble() < 0.0) {
        return MC_FAIL(what + " 'damage' must be a number >= 0");
    }
    out = static_cast<float>(dmg.asDouble());
    if (item.durability <= 0.0f) {
        return MC_FAIL(what + " holds damage " + std::to_string(out) +
                       " but the item '" + item.name + "' has no durability");
    }
    if (out >= item.durability) {
        return MC_FAIL(what + " damage " + std::to_string(out) +
                       " is at/above the item '" + item.name + "'s "
                       "durability " + std::to_string(item.durability) +
                       " (a broken item is removed, never saved)");
    }
    return {};
}

Result<void> parseSlotEntry(const Json& e, const ItemRegistry& items,
                            const std::string& what, bool allowDamage,
                            ItemStack& out) {
    if (!e.isObject()) {
        return MC_FAIL(what + " must be an object");
    }
    std::set<std::string> slotFields {"item", "count"};
    if (allowDamage) {
        slotFields.insert("damage");
    }
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
    // SURV-3: the damage cross-check (an item without durability cannot
    // carry one; a damage at/above the durability is the broken state —
    // the stack is removed, never saved). Absent key = 0 (full).
    if (const Json* dmg = e.find("damage")) {
        MC_TRY(checkDamageValue(*dmg, items.at(id), what, out.damage));
    }
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
                                        std::to_string(i),
                                        /*allowDamage=*/false, out[i]);
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
                              what + " '" + slotNames[i] + "'",
                              /*allowDamage=*/false, *slotPtrs[i]));
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

// -- the save journal (PIPE-2) -------------------------------------------------

bool SaveJournal::recordBlock(const BlockPos& pos) {
    if (!valid) {
        return false;
    }
    const ColumnPos col{floorDiv(pos.x, kChunkSize), floorDiv(pos.z, kChunkSize)};
    if (!blocks[col].insert(pos).second) {
        return false; // already recorded (idempotent)
    }
    ++entries;
    if (entries > kSaveJournalMaxEntries) {
        valid = false; // the exit save falls back to the legacy diffWorld
        return true;
    }
    return false;
}

bool SaveJournal::recordFluid(const BlockPos& pos) {
    if (!valid) {
        return false;
    }
    const ColumnPos col{floorDiv(pos.x, kChunkSize), floorDiv(pos.z, kChunkSize)};
    if (!fluids[col].insert(pos).second) {
        return false; // already recorded (idempotent)
    }
    ++entries;
    if (entries > kSaveJournalMaxEntries) {
        valid = false; // the exit save falls back to the legacy diffWorld
        return true;
    }
    return false;
}

void SaveJournal::evictColumn(const ColumnPos& col) {
    const auto b = blocks.find(col);
    if (b != blocks.end()) {
        entries -= b->second.size();
        blocks.erase(b);
    }
    const auto f = fluids.find(col);
    if (f != fluids.end()) {
        entries -= f->second.size();
        fluids.erase(f);
    }
}

// Compares one live chunk against a freshly generated reference chunk,
// restricted to the journal cells (PIPE-2): the SAME per-cell comparison
// as diffChunk (the encoded fluid byte — the fluidKindOf(0)=Water
// level-0 special case included), so a journal cell that matches
// generation emits nothing and the output is byte-identical to
// diffWorld's for the same world.
void journalDiffChunk(const World& world,
                      const World::ChunkGenerator& reference,
                      const BlockRegistry& blocks, const ChunkPos& cp,
                      const std::vector<BlockPos>* blockCells,
                      const std::vector<BlockPos>* fluidCells,
                      std::vector<SavedBlockEdit>& outBlockEdits,
                      std::vector<SavedFluidEdit>& outFluidEdits) {
    const Chunk* live = world.findChunk(cp);
    if (live == nullptr) {
        return; // cannot happen: positions come from the world
    }
    Chunk generated;
    reference(generated, cp);
    const BlockPos origin = chunkOrigin(cp);
    if (blockCells != nullptr) {
        for (const BlockPos& pos : *blockCells) {
            const int lx = pos.x - origin.x;
            const int ly = pos.y - origin.y;
            const int lz = pos.z - origin.z;
            if (live->get(lx, ly, lz) != generated.get(lx, ly, lz)) {
                outBlockEdits.push_back(
                    {pos, blocks.at(live->get(lx, ly, lz)).name});
            }
        }
    }
    if (fluidCells != nullptr) {
        for (const BlockPos& pos : *fluidCells) {
            const int lx = pos.x - origin.x;
            const int ly = pos.y - origin.y;
            const int lz = pos.z - origin.z;
            const std::uint8_t liveFl = live->fluidAt(lx, ly, lz);
            if (liveFl != generated.fluidAt(lx, ly, lz)) {
                outFluidEdits.push_back(
                    {pos, fluidKindOf(liveFl), fluidLevelOf(liveFl)});
            }
        }
    }
}

Result<void> journalDiff(const World& world,
                         const World::ChunkGenerator& reference,
                         const BlockRegistry& blocks,
                         const std::map<ColumnPos, std::set<BlockPos>>&
                             journalBlocks,
                         const std::map<ColumnPos, std::set<BlockPos>>&
                             journalFluids,
                         std::vector<SavedBlockEdit>& outBlockEdits,
                         std::vector<SavedFluidEdit>& outFluidEdits,
                         int workers) {
    // Group the journal cells by the chunk they live in (a journal
    // COLUMN spans up to 8 chunks; the maps are position-sorted -> the
    // grouping is deterministic).
    std::map<ChunkPos, std::vector<BlockPos>> blockCellsByChunk;
    for (const auto& entry : journalBlocks) {
        for (const BlockPos& pos : entry.second) {
            blockCellsByChunk[worldToChunk(pos)].push_back(pos);
        }
    }
    std::map<ChunkPos, std::vector<BlockPos>> fluidCellsByChunk;
    for (const auto& entry : journalFluids) {
        for (const BlockPos& pos : entry.second) {
            fluidCellsByChunk[worldToChunk(pos)].push_back(pos);
        }
    }
    // K = the touched chunks that still EXIST: an unloaded column's
    // chunks are gone (the journal's eviction dropped them too), matching
    // diffWorld's "only existing chunks" semantics.
    std::vector<ChunkPos> k;
    for (const auto& entry : blockCellsByChunk) {
        if (world.hasChunk(entry.first)) {
            k.push_back(entry.first);
        }
    }
    for (const auto& entry : fluidCellsByChunk) {
        if (blockCellsByChunk.count(entry.first) == 0 &&
            world.hasChunk(entry.first)) {
            k.push_back(entry.first);
        }
    }
    std::sort(k.begin(), k.end());
    const auto runChunk = [&world, &reference, &blocks, &blockCellsByChunk,
                           &fluidCellsByChunk](const ChunkPos& cp,
                                               std::vector<SavedBlockEdit>& ob,
                                               std::vector<SavedFluidEdit>& of) {
        const auto bb = blockCellsByChunk.find(cp);
        const auto ff = fluidCellsByChunk.find(cp);
        journalDiffChunk(world, reference, blocks, cp,
                         bb == blockCellsByChunk.end() ? nullptr : &bb->second,
                         ff == fluidCellsByChunk.end() ? nullptr : &ff->second,
                         ob, of);
    };
    if (workers <= 1 || k.size() <= kSaveDiffParallelMinChunks) {
        for (const ChunkPos& cp : k) {
            runChunk(cp, outBlockEdits, outFluidEdits);
        }
    } else {
        // P37 reuse: one job per chunk on a throwaway pool. The workers
        // only read the world (findChunk) + the grouped cell lists (the
        // maps are not mutated after the posts) and call the pure
        // generator — no World mutation. Each job appends to its own
        // slice; the position sort below is a total order (one edit per
        // cell per table), so the result is byte-identical to the serial
        // path regardless of scheduling.
        ThreadPool pool(workers);
        std::vector<std::vector<SavedBlockEdit>> blockParts(k.size());
        std::vector<std::vector<SavedFluidEdit>> fluidParts(k.size());
        for (std::size_t i = 0; i < k.size(); ++i) {
            const ChunkPos cp = k[i];
            pool.post([&, i, cp] {
                runChunk(cp, blockParts[i], fluidParts[i]);
            });
        }
        pool.flush();
        for (std::size_t i = 0; i < k.size(); ++i) {
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

// -- retained column diffs (STRM-2) ------------------------------------------------

RetainedColumnDiff retainColumnDiff(const World& world,
                                    const World::ChunkGenerator& reference,
                                    const BlockRegistry& blocks,
                                    const ColumnPos& col,
                                    const std::set<BlockPos>* blockCells,
                                    const std::set<BlockPos>* fluidCells) {
    // Group the column's journal cells by the chunk they live in (a
    // journal COLUMN spans up to kWorldHeight/kChunkSize chunks;
    // journalDiffChunk reads local coords relative to the chunk origin,
    // so only a chunk's OWN cells may be passed to it). The sets are
    // position-sorted -> the groups are deterministic; a cell outside the
    // column (structurally impossible — the caller passes the column's
    // own journal sets) is dropped.
    std::map<ChunkPos, std::vector<BlockPos>> bByChunk;
    if (blockCells != nullptr) {
        for (const BlockPos& pos : *blockCells) {
            const ChunkPos cp = worldToChunk(pos);
            if (cp.x == col.x && cp.z == col.z) {
                bByChunk[cp].push_back(pos);
            }
        }
    }
    std::map<ChunkPos, std::vector<BlockPos>> fByChunk;
    if (fluidCells != nullptr) {
        for (const BlockPos& pos : *fluidCells) {
            const ChunkPos cp = worldToChunk(pos);
            if (cp.x == col.x && cp.z == col.z) {
                fByChunk[cp].push_back(pos);
            }
        }
    }
    // The column's layers, bottom to top: a layer whose chunk no longer
    // exists (structurally impossible at the Game's call site — the
    // unload pass snapshots BEFORE the erase loop) is skipped by
    // journalDiffChunk itself (its no-op).
    RetainedColumnDiff out;
    for (int cy = 0; cy * kChunkSize < kWorldHeight; ++cy) {
        const ChunkPos cp {col.x, cy, col.z};
        const auto b = bByChunk.find(cp);
        const auto f = fByChunk.find(cp);
        journalDiffChunk(world, reference, blocks, cp,
                         b == bByChunk.end() ? nullptr : &b->second,
                         f == fByChunk.end() ? nullptr : &f->second,
                         out.blocks, out.fluids);
    }
    // The same position total order journalDiff/diffWorld end with (one
    // edit per cell per table -> a byte-identical serialization).
    std::sort(out.blocks.begin(), out.blocks.end(),
              [](const SavedBlockEdit& a, const SavedBlockEdit& b) {
                  return a.pos < b.pos;
              });
    std::sort(out.fluids.begin(), out.fluids.end(),
              [](const SavedFluidEdit& a, const SavedFluidEdit& b) {
                  return a.pos < b.pos;
              });
    return out;
}

void mergeRetained(SaveGame& save,
                   const std::map<ColumnPos, RetainedColumnDiff>& retained) {
    if (retained.empty()) {
        return; // the pre-STRM-2 shape: the lists stay exactly as diffed
    }
    // A duplicate cell is structurally impossible (see the header): a
    // column sits in at most one of the journal and this map, and the
    // diff that filled the lists covered EXISTING chunks only — a
    // retained column is unloaded. The merge is a pure union; the
    // re-sort restores the position total order the journalDiff /
    // diffWorld tail sort left.
    for (const auto& entry : retained) {
        const RetainedColumnDiff& diff = entry.second;
        save.blockEdits.insert(save.blockEdits.end(), diff.blocks.begin(),
                               diff.blocks.end());
        save.fluidEdits.insert(save.fluidEdits.end(), diff.fluids.begin(),
                               diff.fluids.end());
    }
    std::sort(save.blockEdits.begin(), save.blockEdits.end(),
              [](const SavedBlockEdit& a, const SavedBlockEdit& b) {
                  return a.pos < b.pos;
              });
    std::sort(save.fluidEdits.begin(), save.fluidEdits.end(),
              [](const SavedFluidEdit& a, const SavedFluidEdit& b) {
                  return a.pos < b.pos;
              });
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
        // A save can hold a fluid edit in a column with no block edit
        // (lava that overflowed past the demo area into generated
        // terrain, for example): setFluid writes existing chunks only,
        // so ensure the chunk first — the synchronous generation the
        // placement path uses (the generated cells under the edit are
        // exactly the cells the save diffed against).
        world.getOrCreateChunk(worldToChunk(e.pos));
        // A level-0 edit on an already-dry cell is a no-op the diff
        // records when the generated fluid was displaced (a placed block
        // or the water/lava reaction took the cell). The block edits were
        // applied first, so the cell is dry: skip instead of failing.
        if (e.level == 0 &&
            world.getFluidOfKind(e.pos.x, e.pos.y, e.pos.z, e.kind) == 0) {
            continue;
        }
        if (!world.setFluid(e.pos.x, e.pos.y, e.pos.z, e.kind, e.level)) {
            return MC_FAIL("cannot apply save fluid edit at " +
                           posStr(e.pos) + " (cell occupied or level "
                           "unchanged)");
        }
    }
    // Top up every edited column to a COMPLETE column. The edits above
    // create only their target chunks; the streaming loader counts a
    // column as loaded when ANY chunk exists, so a partial column's
    // remaining chunks would stay void forever (a hole around the
    // restored edits).
    std::set<ColumnPos> editedColumns;
    for (const SavedBlockEdit& e : save.blockEdits) {
        const ChunkPos cp = worldToChunk(e.pos);
        editedColumns.insert({cp.x, cp.z});
    }
    for (const SavedFluidEdit& e : save.fluidEdits) {
        const ChunkPos cp = worldToChunk(e.pos);
        editedColumns.insert({cp.x, cp.z});
    }
    for (const ColumnPos& c : editedColumns) {
        world.ensureColumn(c.x, c.z);
    }
    return {};
}

// -- serialization --------------------------------------------------------------

// One {item, count} slot entry of a per-block state (the furnace's
// input/fuel/output; CRAFT-1: the crafting-table grid cells reuse the
// same shape). A free function rather than an in-function lambda: the
// Result<Json> value construction here trips GCC's -O3
// maybe-uninitialized false positive when inlined into serializeSave
// (the variant move ctor; the code is trivially initialized).
static Result<Json> serializeSlotEntry(const ItemStack& s,
                                       const ItemRegistry& items,
                                       const char* what) {
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
}

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
    // way. The game always writes the real value (Phase 72: the
    // BASE-pool value only, 0..base max).
    if (save.playerHealth >= 0.0f) {
        player["health"] = Json(save.playerHealth);
    }
    // Format 12: the player's absorption pool (Phase 72, always written
    // — a format-12 file REQUIREs the key; the game always has the real
    // value, 0 when no layer is active).
    if (save.playerAbsorption >= 0.0f) {
        player["absorption"] = Json(save.playerAbsorption);
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
            // SURV-6 (format 20): the tier ONLY when > 0 (the
            // omit-when-default discipline, the format-18 damage twin) —
            // a tier-0 entry keeps its format-19 byte shape.
            if (e.tier > 0) {
                eObj["tier"] = Json(e.tier);
            }
            effectsArr.push_back(std::move(eObj));
        }
        player["effects"] = std::move(effectsArr);
    }
    // SURV-7 (format 21): the player's death-related statistics — ALWAYS
    // written (an empty object is valid, the furnaces/tables discipline).
    // The map is name-sorted by construction (std::map order). Zero
    // counters are ABSENT from the game's map, so the object holds only
    // non-zero entries.
    {
        Json statsObj = Json::object();
        for (const auto& [name, count] : save.playerStats) {
            // The double cast is exact for every realistic counter (the
            // uint64 value is far below 2^53); Json has no integer
            // factory (the ambiguous-overload trap).
            statsObj[name] = Json(static_cast<double>(count));
        }
        player["stats"] = std::move(statsObj);
    }
    // SURV-1: the player's armor (format 16, ALWAYS written — a
    // format-16 file REQUIRES the key; an empty object = no armor).
    // Only the OCCUPIED slots are written, in the fixed slot order.
    // SURV-3 (format 18): an occupied slot with damage > 0 flips to the
    // object shape {item, damage} (the key discipline: damage is written
    // ONLY when > 0 — a full-durability piece stays the pre-18 string).
    {
        Json armorObj = Json::object();
        for (int slot = 0; slot < kArmorSlotCount; ++slot) {
            const std::string& name =
                save.playerArmor[static_cast<std::size_t>(slot)];
            if (!name.empty()) {
                const float dmg =
                    save.playerArmorDamage[static_cast<std::size_t>(slot)];
                if (dmg > 0.0f) {
                    Json entry = Json::object();
                    entry["item"] = Json(name);
                    entry["damage"] = Json(dmg);
                    armorObj[kArmorSlotNames[slot]] = std::move(entry);
                } else {
                    armorObj[kArmorSlotNames[slot]] = Json(name);
                }
            }
        }
        player["armor"] = std::move(armorObj);
    }
    // SURV-2: the player's oxygen (format 17, ALWAYS written — a
    // format-17 file REQUIRES the key; the game always has the real
    // value, kOxygenMax when breathing).
    if (save.playerOxygen >= 0) {
        player["oxygen"] = Json(save.playerOxygen);
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
            // SURV-3 (format 18): the damage key only when > 0 (0 = full
            // durability = absent — the omit-when-default discipline).
            if (s.damage > 0.0f) {
                e["damage"] = Json(s.damage);
            }
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
    // The E panel's grid (format 3, the table grid's twin — the TABLE
    // grid is per-block since CRAFT-1, the "tables" array below).
    {
        auto invGrid = serializeGrid(save.invGrid);
        MC_TRY(invGrid);
        root["inv_grid"] = std::move(*invGrid);
    }

    // Format 7: the per-block furnace states (Phase 44): one entry per
    // furnace block (position-sorted — the game's map order), each an
    // object with the block position + the three slot entries + the two
    // integer 1/60-substep counters. Always written (possibly empty — a
    // world without furnace blocks).
    {
        Json furnaces = Json::array();
        for (const SavedFurnace& sf : save.furnaces) {
            Json fz = Json::object();
            Json pos = Json::array();
            pos.push_back(Json(sf.pos.x));
            pos.push_back(Json(sf.pos.y));
            pos.push_back(Json(sf.pos.z));
            fz["pos"] = pos;
            auto inSlot = serializeSlotEntry(sf.state.input, items,
                                             "the furnace input");
            MC_TRY(inSlot);
            fz["input"] = std::move(*inSlot);
            auto fuelSlot = serializeSlotEntry(sf.state.fuel, items,
                                               "the furnace fuel");
            MC_TRY(fuelSlot);
            fz["fuel"] = std::move(*fuelSlot);
            auto outSlot = serializeSlotEntry(sf.state.output, items,
                                              "the furnace output");
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

    // The campfire cooking states (format 13; Phase 71, the four-slot
    // family shape in Phase 75): one entry per campfire-FAMILY block
    // (lit or extinguished — position-sorted, the game's map order),
    // each an object with the block position + "slots": EXACTLY 4 slot
    // objects (item/count/progress_substeps — empty slots are ""/0/0).
    // Always written (possibly empty — a world without campfire blocks).
    {
        Json campfires = Json::array();
        for (const SavedCampfire& sc : save.campfires) {
            Json ce = Json::object();
            Json pos = Json::array();
            pos.push_back(Json(sc.pos.x));
            pos.push_back(Json(sc.pos.y));
            pos.push_back(Json(sc.pos.z));
            ce["pos"] = pos;
            Json slotsArr = Json::array();
            for (std::size_t s = 0; s < kCampfireSlotCount; ++s) {
                const ItemStack& slot = sc.state.slots[s];
                Json se = Json::object();
                if (slot.empty()) {
                    se["item"] = Json(std::string(""));
                    se["count"] = Json(0);
                } else {
                    const Item* item = items.find(slot.item);
                    if (item == nullptr) {
                        return MC_FAIL("save campfire at " + posStr(sc.pos) +
                                       " slot " + std::to_string(s) +
                                       " holds an unregistered item id " +
                                       std::to_string(slot.item.value));
                    }
                    se["item"] = Json(item->name);
                    se["count"] = Json(slot.count);
                }
                se["progress_substeps"] = Json(sc.state.progressSubsteps[s]);
                slotsArr.push_back(std::move(se));
            }
            ce["slots"] = slotsArr;
            campfires.push_back(std::move(ce));
        }
        root["campfires"] = std::move(campfires);
    }

    // The per-block crafting-table grids (format 19; CRAFT-1): one entry
    // per crafting_table block (position-sorted — the game's map order),
    // each an object with the block position + "slots": EXACTLY 9 grid
    // slot entries (the format-3 grid discipline — the slots carry no
    // damage, the SURV-3 grid scope). Always written (possibly empty — a
    // world without table blocks).
    {
        Json tables = Json::array();
        for (const SavedTable& st : save.tables) {
            Json te = Json::object();
            Json pos = Json::array();
            pos.push_back(Json(st.pos.x));
            pos.push_back(Json(st.pos.y));
            pos.push_back(Json(st.pos.z));
            te["pos"] = pos;
            Json slotsArr = Json::array();
            for (int c = 0; c < CraftingGrid::kSize; ++c) {
                auto cell = serializeSlotEntry(
                    st.grid.at(c), items,
                    ("save table at " + posStr(st.pos) + " cell " +
                     std::to_string(c)).c_str());
                MC_TRY(cell);
                slotsArr.push_back(std::move(*cell));
            }
            te["slots"] = slotsArr;
            tables.push_back(std::move(te));
        }
        root["tables"] = std::move(tables);
    }

    // Always written (possibly empty — a world without stateful blocks).
    // ENT-2: the per-instance block states (format 15; Phase 94's stair
    // facings generalized — the project's first per-block state).
    {
        Json states = Json::array();
        for (const SavedState& ss : save.states) {
            Json se = Json::object();
            Json pos = Json::array();
            pos.push_back(Json(ss.pos.x));
            pos.push_back(Json(ss.pos.y));
            pos.push_back(Json(ss.pos.z));
            se["pos"] = pos;
            se["state"] = Json(static_cast<int>(ss.state));
            states.push_back(std::move(se));
        }
        root["states"] = std::move(states);
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
        // SURV-3 (format 18): the stack's damage only when > 0 (the omit-
        // when-default discipline, the slots' twin).
        if (e.damage > 0.0f) {
            je["damage"] = Json(e.damage);
        }
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
    // file must still parse for the Phase 44 migration). Likewise "stairs"
    // (format 14) and "states" (format 15, ENT-2's generalization): both
    // are known, the per-format key discipline below enforces which one a
    // file may carry ("stairs" is restricted to 14, "states" to 15).
    // Likewise "table_grid" (format 3-18) and "tables" (format 19,
    // CRAFT-1's generalization): both are known, the per-format key
    // discipline below enforces which one a file may carry ("table_grid"
    // is restricted to 18 and older, "tables" to 19).
    const std::set<std::string> topLevel {"format", "seed", "time", "player",
                                          "inventory", "inv_grid",
                                          "table_grid", "furnace",
                                          "furnaces", "fires", "campfires",
                                          "stairs", "states", "tables",
                                          "entities", "blocks", "fluids"};
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
    // SURV-7: "stats" is format-21-and-newer vocabulary — a pre-21 file
    // carrying it is a version lie (the unknown-field naming below).
    std::set<std::string> playerFields {"pos", "yaw", "pitch",
                                       "health", "absorption",
                                       "hunger", "saturation",
                                       "effects", "armor",
                                       "oxygen"};
    if (fmt->asInt() >= 21) {
        playerFields.insert("stats");
    }
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
        const int fmt = def.find("format")->asInt();
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
            // SURV-6 (format 20): the "tier" key is format-20-and-newer
            // vocabulary — a pre-20 file carrying it is a version lie
            // (the format-dependent whitelist, the table_grid/tables
            // three-way discipline).
            const std::set<std::string> effFields =
                (fmt >= 20)
                    ? std::set<std::string> {"name", "remaining", "tier"}
                    : std::set<std::string> {"name", "remaining"};
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
            const Effect* effDef = effects.findByName(nm->asString());
            if (effDef == nullptr) {
                return MC_FAIL(what + " holds the unregistered effect '" +
                               nm->asString() + "'");
            }
            // SURV-6: the tier (absent = 0 = the format-19 migration),
            // a whole number in [0, tierMax) against the REFERENCED
            // effect's data (the registry is available here — the
            // absorption-pool invariant pattern).
            int tier = 0;
            if (const Json* t = e.find("tier")) {
                if (!t->isNumber() || t->asDouble() != std::floor(t->asDouble()) ||
                    t->asDouble() < 0.0) {
                    return MC_FAIL(what + " 'tier' must be an integer >= 0, "
                                   "got " +
                                   (t->isNumber()
                                        ? std::to_string(t->asDouble())
                                        : "a non-number"));
                }
                tier = static_cast<int>(t->asDouble());
                if (tier >= effDef->tierMax) {
                    return MC_FAIL(what + " holds effect '" + effDef->name +
                                   "' at tier " + std::to_string(tier) +
                                   ", but the effect has tier_max " +
                                   std::to_string(effDef->tierMax) +
                                   " (tiers are 0-based, in [0, " +
                                   std::to_string(effDef->tierMax - 1) +
                                   "])");
                }
            }
            if (!seen.insert(nm->asString()).second) {
                return MC_FAIL(what + " holds the effect '" +
                               nm->asString() + "' twice");
            }
            save.playerEffects.push_back(
                SavedEffect {nm->asString(), rem->asDouble(), tier});
        }
        std::sort(save.playerEffects.begin(), save.playerEffects.end(),
                  [](const SavedEffect& a, const SavedEffect& b) {
                      return a.name < b.name;
                  });
    }
    // Statistics (format 21; SURV-7, the death-related counters):
    // ALWAYS present on format-21 files (an empty object is valid — the
    // furnaces/tables key discipline). Entries are a name (non-empty) +
    // a whole number >= 0. Pre-21 files load with EMPTY stats (the
    // system did not exist) — the "stats" key on a pre-21 file was
    // already rejected above (the version-lie whitelist). The vocabulary
    // check (kill.<name> names a registered mob) runs at GAME load —
    // validateStats; the mob registry is not a parseSave parameter.
    if (fmt->asInt() >= 21) {
        const Json* st = player->find("stats");
        if (st == nullptr || !st->isObject()) {
            return MC_FAIL("save format 21 requires a 'player.stats' "
                           "object (an empty object when no counter "
                           "was non-zero)");
        }
        for (const auto& [key, value] : st->members()) {
            const std::string what = "save 'player.stats'[" + key + "]";
            if (key.empty()) {
                return MC_FAIL("save 'player.stats' holds an empty stat "
                               "name");
            }
            if (!value.isNumber() || value.asDouble() < 0.0 ||
                value.asDouble() != std::floor(value.asDouble())) {
                return MC_FAIL(what + " must be a whole number >= 0, got " +
                               (value.isNumber()
                                    ? std::to_string(value.asDouble())
                                    : "a non-number"));
            }
            save.playerStats[key] =
                static_cast<std::uint64_t>(value.asDouble());
        }
    }
    // Absorption pool (format 12; Phase 72, the two-component model).
    // Absent in format 1-11 files (the SHARED health carried the layer —
    // the game migrates it in applySave; the -1 sentinel stays). A
    // format-11 file carrying the key is rejected (the new-key
    // discipline). Format 12 REQUIREs the field (the effects-key
    // discipline): a number in [0, 64] (the pool's hard cap) and
    // <= the saved effects' total `absorb_hp` (the pool never exceeds
    // the layer — damage only drains it, so it may sit BELOW the total;
    // the registry is available here, so the invariant is enforced at
    // parse time).
    if (def.find("format")->asInt() >= 12) {
        const Json* absorption = player->find("absorption");
        if (absorption == nullptr || !absorption->isNumber()) {
            return MC_FAIL("save format 12 requires 'player.absorption' "
                           "(a number; 0 when no layer is active)");
        }
        const double a = absorption->asDouble();
        if (a < 0.0 || a > 64.0) {
            return MC_FAIL("save 'player.absorption' must be a number in "
                           "[0, 64], got " + std::to_string(a));
        }
        double total = 0.0;
        for (const SavedEffect& se : save.playerEffects) {
            if (const Effect* e = effects.findByName(se.name)) {
                total += e->absorbHp;
            }
        }
        if (a > total + 1e-4) {
            return MC_FAIL("save 'player.absorption' (" + std::to_string(a) +
                           ") exceeds the effects' absorption total (" +
                           std::to_string(total) + ")");
        }
        save.playerAbsorption = static_cast<float>(a);
    } else if (player->find("absorption") != nullptr) {
        return MC_FAIL("save 'player.absorption' requires format 12 or "
                       "newer (got " +
                       std::to_string(def.find("format")->asInt()) + ")");
    }
    // Armor (format 16; SURV-1). Absent in format 1-15 files (the
    // player had no armor — the game loads all four slots empty; the
    // default-empty sentinel). A format-15 file carrying the key is
    // rejected (the version-lie discipline, the absorption twin).
    // Format 16 REQUIRES the field (the effects-key discipline): an
    // object of the OCCUPIED slots only — each key a name from the
    // four-slot vocabulary, each value the NAME of a registered item
    // that IS an armor piece for exactly that slot (the registry is
    // available here, so the item existence + the data-slot match are
    // enforced at parse time, the effects-name discipline).
    if (def.find("format")->asInt() >= 16) {
        const Json* armor = player->find("armor");
        if (armor == nullptr || !armor->isObject()) {
            return MC_FAIL("save format 16 requires a 'player.armor' "
                           "object (an empty object when the player "
                           "wore no armor)");
        }
        for (const auto& [key, value] : armor->members()) {
            const std::string what = "save 'player.armor." + key + "'";
            const int slot = armorSlotIndex(key);
            if (slot < 0) {
                return MC_FAIL(what + " is not a known slot (vocabulary: "
                               "\"head\", \"chest\", \"legs\", \"feet\")");
            }
            // SURV-3 (format 18): the entry shape — a STRING (the
            // pre-18 shape, damage 0 = full durability) or, in a
            // format-18 file, the STRICT object {item, damage} with
            // damage > 0 (the damaged piece; the serializer writes the
            // object exactly then, so an object without damage cannot
            // occur from this build). An object in a format-17 and
            // older file is a version lie (rejected, named).
            std::string name;
            float dmg = 0.0f;
            if (value.isString()) {
                if (value.asString().empty()) {
                    return MC_FAIL(what + " requires a non-empty item name");
                }
                name = value.asString();
            } else if (value.isObject() &&
                       def.find("format")->asInt() >= 18) {
                const std::set<std::string> entryFields {"item", "damage"};
                MC_TRY(rejectUnknownFields(value, entryFields, what));
                const Json* vItem = value.find("item");
                const Json* vDmg = value.find("damage");
                if (vItem == nullptr || !vItem->isString() ||
                    vItem->asString().empty()) {
                    return MC_FAIL(what + " requires a non-empty string "
                                       "'item'");
                }
                name = vItem->asString();
                if (vDmg == nullptr || !vDmg->isNumber() ||
                    vDmg->asDouble() <= 0.0) {
                    return MC_FAIL(what + " requires 'damage' (a number "
                                       "> 0 — the object shape is the "
                                       "damaged piece; a full-durability "
                                       "piece is the string shape)");
                }
                dmg = static_cast<float>(vDmg->asDouble());
            } else {
                return MC_FAIL(what +
                    (value.isObject()
                         ? " is an object: the damaged-piece shape "
                           "requires format 18 or newer (got " +
                           std::to_string(def.find("format")->asInt()) +
                           ")"
                         : " requires an item name string (or the "
                           "format-18 damaged-piece object)"));
            }
            const Item* item = items.findByName(name);
            if (item == nullptr) {
                return MC_FAIL(what + " holds unknown item '" + name +
                               "'");
            }
            if (item->armorSlot != key) {
                return MC_FAIL(what + " holds the item '" +
                               name + "' whose armor slot is "
                               "'" + item->armorSlot +
                               "' (a piece equips the slot named by its "
                               "data)");
            }
            if (dmg > 0.0f) {
                // The damage cross-check (an armor item without
                // durability, or a damage at/above it, is named).
                MC_TRY(checkDamageValue(Json(dmg), *item, what, dmg));
            }
            save.playerArmor[static_cast<std::size_t>(slot)] = name;
            save.playerArmorDamage[static_cast<std::size_t>(slot)] = dmg;
        }
    } else if (player->find("armor") != nullptr) {
        return MC_FAIL("save 'player.armor' requires format 16 or newer "
                       "(got " +
                       std::to_string(def.find("format")->asInt()) + ")");
    }
    // Oxygen (format 17; SURV-2, the drowning system). Absent in format
    // 1-16 files (the player had full air — the sentinel stays -1 for
    // the migration in Game::applySave); a format-16 file carrying the
    // key is rejected (the version-lie discipline, the armor twin).
    // Format 17 REQUIRES the field (the armor-key discipline): an
    // integer in [0, kOxygenMax] (the air cap, player/drown.hpp).
    if (def.find("format")->asInt() >= 17) {
        const Json* oxygen = player->find("oxygen");
        if (oxygen == nullptr) {
            return MC_FAIL("save format 17 requires a 'player.oxygen' "
                           "integer (0.." +
                           std::to_string(kOxygenMax) + ")");
        }
        double d = 0.0;
        if (!isInt(*oxygen, &d)) {
            return MC_FAIL("save 'player.oxygen' must be an integer in "
                           "[0, " + std::to_string(kOxygenMax) + "]");
        }
        if (d < 0.0 || d > static_cast<double>(kOxygenMax)) {
            return MC_FAIL("save 'player.oxygen' must be in [0, " +
                           std::to_string(kOxygenMax) + "], got " +
                           std::to_string(d));
        }
        save.playerOxygen = static_cast<int>(d);
    } else if (player->find("oxygen") != nullptr) {
        return MC_FAIL("save 'player.oxygen' requires format 17 or newer "
                       "(got " +
                       std::to_string(def.find("format")->asInt()) + ")");
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
        // SURV-3 (format 18): the inventory slots carry the per-stack
        // "damage" key (the grids do not — the format-18 scope); a
        // format-17 and older file carrying it is a version lie
        // (rejected by name, the allowDamage=false whitelist).
        auto r = parseSlotEntry(slots->at(i), items, "save slot #" +
                                       std::to_string(i),
                                       def.find("format")->asInt() >= 18,
                                       save.slots[i]);
        MC_TRY(r);
    }

    // -- crafting grids (format 3; Phase 17; CRAFT-1) -------------------
    // Absent in format 1/2 files: the grids load empty (the old saves
    // predate them). Required and strictly validated in format 3.
    // CRAFT-1 (format 19): the E panel's grid ("inv_grid") is the only
    // global grid left — the crafting TABLE's grid is per-block now
    // ("tables", parsed below). "table_grid" is restricted to format 18
    // and older (it is the format-18 migration source: the ONE global
    // grid is attributed to the world's sole table block, or discarded —
    // the Phase 44 precedent).
    const int fmtVersion = def.find("format")->asInt();
    std::vector<ItemStack> legacyTableGrid; // format 3-18; parsed here,
                                            // migrated into save.tables
    if (fmtVersion >= 3) {
        const Json* invGrid = def.find("inv_grid");
        if (invGrid == nullptr) {
            return MC_FAIL("save format 3 requires an 'inv_grid' array");
        }
        MC_TRY(parseGrid(*invGrid, items, "inv_grid", save.invGrid));
    } else if (def.find("inv_grid") != nullptr ||
               def.find("table_grid") != nullptr) {
        return MC_FAIL("save grids ('inv_grid'/'table_grid') require "
                       "format 3 (got " + std::to_string(fmtVersion) + ")");
    }
    if (fmtVersion >= 3 && fmtVersion <= 18) {
        const Json* tableGrid = def.find("table_grid");
        if (tableGrid == nullptr) {
            return MC_FAIL("save format 3 requires a 'table_grid' array");
        }
        MC_TRY(parseGrid(*tableGrid, items, "table_grid", legacyTableGrid));
    } else if (fmtVersion >= 19 && def.find("table_grid") != nullptr) {
        return MC_FAIL("save 'table_grid' is restricted to format 18 or "
                       "older (a format-19 file carries the per-block "
                       "'tables' array instead)");
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
        // SURV-3 (format 18): the stack's "damage" key (the omit-when-
        // 0 discipline — a format-17 and older file carrying it is a
        // version lie, rejected by name via the whitelist below).
        std::set<std::string> entFields {"type", "item", "count",
                                         "pos", "age",
                                         "pickup_delay"};
        if (def.find("format")->asInt() >= 18) {
            entFields.insert("damage");
        }
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
        // SURV-3: the damage cross-check (absent key = 0 = full).
        if (const Json* dmg = e.find("damage")) {
            const std::string what = "save entity #" + std::to_string(i);
            MC_TRY(checkDamageValue(*dmg, items.at(id), what, se.damage));
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
    std::map<BlockPos, std::string> blockCells;
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
        blockCells.emplace(pos, name);
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
        // A cell edit pair is legal when the implied LIVE state is a single
        // occupant: an "air" block edit (the generated block was burned or
        // broken and the fluid then flowed in — the Phase 90 lava-lake
        // burn) co-locating with a positive fluid level, or a non-air block
        // edit co-locating with a level-0 (drained) fluid edit (the block
        // displaced the generated fluid). The one impossible live state is
        // a solid block AND a positive fluid in the same cell.
        const auto bit = blockCells.find(pos);
        if (bit != blockCells.end() && bit->second != "air" && li > 0) {
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

    // -- campfire cooking states (format 11; Phase 71; the four-slot
    // family shape in Phase 75) -------------------
    // A "campfire block" = the block named "campfire" (the Phase 71
    // cooking block — the interact property names the game behavior, the
    // NAME is the save's reference, exactly like the furnace cells).
    // Phase 75 adds the extinguished twin ("campfire_extinguished" —
    // water put the lit campfire out): it owns the same cooking state,
    // so the format-13+ cross-check runs against the whole FAMILY.
    std::set<BlockPos> litCells;    // "campfire" — the format 11-12 check
    std::set<BlockPos> familyCells; // lit + extinguished — the format 13+ check
    for (const SavedBlockEdit& e : save.blockEdits) {
        if (e.blockName == "campfire") {
            litCells.insert(e.pos);
            familyCells.insert(e.pos);
        } else if (e.blockName == "campfire_extinguished") {
            familyCells.insert(e.pos);
        }
    }
    const Json* campfireArr = def.find("campfires");
    // The per-slot rules (Phase 75 — shared by both entry shapes): the
    // strict triple "item" (string, "" = empty) / "count" (numeric, 0
    // when empty, 1..maxStack otherwise) / "progress_substeps" (numeric,
    // [0, the furnace cap]) + the per-slot progress invariant (progress
    // > 0 requires the slot to hold EXACTLY ONE smeltable item — a save
    // violating it would resurrect progress the tick cannot have
    // produced, the Phase 40 furnace invariant).
    const auto parseCampfireSlot =
        [&items](const Json& s, const std::string& what, ItemStack& slot,
                 int& prog) -> Result<void> {
        const std::set<std::string> sFields {"item", "count",
                                             "progress_substeps"};
        MC_TRY(rejectUnknownFields(s, sFields, what));
        const Json* name = s.find("item");
        const Json* count = s.find("count");
        if (name == nullptr || !name->isString() || count == nullptr ||
            !count->isNumber()) {
            return MC_FAIL(what + " requires string 'item' and numeric "
                           "'count'");
        }
        const int n = count->asInt();
        slot = ItemStack {};
        if (name->asString().empty()) {
            if (n != 0) {
                return MC_FAIL(what + " is empty ('item' = "") but has "
                               "count " + std::to_string(n));
            }
        } else {
            const ItemID id = items.idForName(name->asString());
            if (!id.valid()) {
                return MC_FAIL(what + " holds unknown item '" +
                               name->asString() + "'");
            }
            if (n < 1 || n > items.at(id).maxStack) {
                return MC_FAIL(what + " count " + std::to_string(n) +
                               " is out of range for '" + name->asString() +
                               "' (max stack " +
                               std::to_string(items.at(id).maxStack) + ")");
            }
            slot = ItemStack {id, n};
        }
        const Json* pr = s.find("progress_substeps");
        if (pr == nullptr || !pr->isNumber()) {
            return MC_FAIL(what + " requires numeric 'progress_substeps'");
        }
        prog = pr->asInt();
        if (prog < 0 || prog > kFurnaceMaxProgressSubsteps) {
            return MC_FAIL(what + " progress_substeps " + std::to_string(prog) +
                           " is out of range [0, " +
                           std::to_string(kFurnaceMaxProgressSubsteps) +
                           "]");
        }
        if (prog > 0 &&
            (slot.count != 1 ||
             items.at(slot.item).smeltableTo.empty())) {
            return MC_FAIL(what + " has progress_substeps " +
                           std::to_string(prog) +
                           " but its slot holds no smeltable item of "
                           "count 1");
        }
        return {};
    };
    if (fmtVersion >= 13) {
        // Phase 75: the four-slot entry. Cross-checked against the whole
        // CAMPFIRE FAMILY (an entry without a family block, or a family
        // block without an entry, would desync the game's invariant:
        // every campfire-family block owns a cooking-state entry).
        if (campfireArr == nullptr || !campfireArr->isArray()) {
            return MC_FAIL("save format 13 requires a 'campfires' array");
        }
        std::set<BlockPos> entryPos;
        for (std::size_t i = 0; i < campfireArr->size(); ++i) {
            const Json& e = campfireArr->at(i);
            const std::string what =
                "save 'campfires' entry #" + std::to_string(i);
            if (!e.isObject()) {
                return MC_FAIL(what + " must be an object");
            }
            const std::set<std::string> eFields {"pos", "slots"};
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
                return MC_FAIL(what + " duplicates the campfire position " +
                               posStr(bpos));
            }
            entryPos.insert(bpos);
            const Json* slotsArr = e.find("slots");
            if (slotsArr == nullptr || !slotsArr->isArray() ||
                slotsArr->size() != kCampfireSlotCount) {
                return MC_FAIL(what + " requires a 'slots' array of "
                               "exactly " + std::to_string(kCampfireSlotCount) +
                               " slot objects");
            }
            CampfireCookState st;
            for (std::size_t s = 0; s < kCampfireSlotCount; ++s) {
                const std::string sw =
                    what + " slot #" + std::to_string(s);
                MC_TRY(parseCampfireSlot(slotsArr->at(s), sw, st.slots[s],
                                         st.progressSubsteps[s]));
            }
            save.campfires.push_back(SavedCampfire {bpos, st});
        }
        if (entryPos != familyCells) {
            for (const BlockPos& p : entryPos) {
                if (familyCells.count(p) == 0) {
                    return MC_FAIL("save 'campfires' has an entry at " +
                                   posStr(p) +
                                   " which is not a campfire-family "
                                   "block in the world's block edits");
                }
            }
            for (const BlockPos& p : familyCells) {
                if (entryPos.count(p) == 0) {
                    return MC_FAIL("the campfire-family block at " +
                                   posStr(p) +
                                   " has no 'campfires' entry (every "
                                   "campfire block, lit or extinguished, "
                                   "owns a cooking-state entry)");
                }
            }
            return MC_FAIL("save 'campfires' does not match the world's "
                           "campfire blocks");
        }
    } else if (fmtVersion >= 11) {
        // Format 11/12: the OLD single-slot entry shape (no "slots" key —
        // carrying one is a version lie, rejected by the field whitelist
        // below). The entry lands in slot 0 on load (the single-slot
        // migration, Phase 75); the cross-check is lit-only (the
        // extinguished block did not exist).
        if (campfireArr == nullptr || !campfireArr->isArray()) {
            return MC_FAIL("save format 11 requires a 'campfires' array");
        }
        std::set<BlockPos> entryPos;
        for (std::size_t i = 0; i < campfireArr->size(); ++i) {
            const Json& e = campfireArr->at(i);
            const std::string what =
                "save 'campfires' entry #" + std::to_string(i);
            if (!e.isObject()) {
                return MC_FAIL(what + " must be an object");
            }
            const std::set<std::string> eFields {"pos", "item", "count",
                                                 "progress_substeps"};
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
                return MC_FAIL(what + " duplicates the campfire position " +
                               posStr(bpos));
            }
            entryPos.insert(bpos);
            // The slot parser's whitelist is {item, count,
            // progress_substeps} — feed it the entry MINUS "pos" (the
            // whitelist above already rejected any other field).
            Json slotView = Json::object();
            slotView["item"] = *e.find("item");
            slotView["count"] = *e.find("count");
            slotView["progress_substeps"] = *e.find("progress_substeps");
            ItemStack slot;
            int prog = 0;
            MC_TRY(parseCampfireSlot(slotView, what, slot, prog));
            // The single-slot migration (Phase 75): the old shape lands
            // in slot 0; the other three slots stay empty.
            CampfireCookState st;
            st.slots[0] = slot;
            st.progressSubsteps[0] = prog;
            save.campfires.push_back(SavedCampfire {bpos, st});
        }
        if (entryPos != litCells) {
            for (const BlockPos& p : entryPos) {
                if (litCells.count(p) == 0) {
                    return MC_FAIL("save 'campfires' has an entry at " +
                                   posStr(p) +
                                   " which is not a campfire block in the "
                                   "world's block edits");
                }
            }
            for (const BlockPos& p : litCells) {
                if (entryPos.count(p) == 0) {
                    return MC_FAIL("the campfire block at " + posStr(p) +
                                   " has no 'campfires' entry (every "
                                   "campfire block owns a cooking-state "
                                   "entry)");
                }
            }
            return MC_FAIL("save 'campfires' does not match the world's "
                           "campfire blocks");
        }
    } else if (campfireArr != nullptr) {
        return MC_FAIL("save 'campfires' requires format 11 or newer (got " +
                       std::to_string(fmtVersion) + ")");
    }

    // -- per-instance block states (format 14 "stairs" / format 15
    //    "states"; Phase 94 stair facing, generalized in ENT-2) ---------
    // A "state cell" = a block edit whose block has a per-instance state
    // (hasState(): the Stairs shape or an explicit model.state_count — the
    // block registry resolves the name). The save cross-checks the entries
    // against the state cells in both directions (every stateful cell owns
    // a state, every state is a stateful cell — the furnaces-key
    // discipline, generalized).
    //
    // Key discipline (version-lie rejection): format 15 REQUIREs the
    // "states" array (an empty array is valid) and rejects a "stairs" key
    // (restricted to 14); format 14 REQUIREs the "stairs" array (an empty
    // array is valid) and rejects a "states" key (requires 15 or newer);
    // format 13 and earlier carry NEITHER (either key is a version lie).
    //
    // Migration: a format-14 file LOADS via the VALUE-PRESERVING migration
    // ({pos, facing} -> {pos, state = facing}); format 13 and earlier
    // migrate into this empty (the per-instance state did not exist — the
    // consumers fall back to the block's state default on load).
    std::set<BlockPos> stateCells;
    std::map<BlockPos, std::string> stateCellBlock; // pos -> block name
    for (const SavedBlockEdit& e : save.blockEdits) {
        const Block* b = blocks.findByName(e.blockName);
        if (b != nullptr && b->hasState()) {
            stateCells.insert(e.pos);
            stateCellBlock[e.pos] = e.blockName;
        }
    }
    const Json* statesArr = def.find("states");
    const Json* stairArr = def.find("stairs");
    // The entry's position: an [x, y, z] integer triple, y in the world
    // height, no duplicates (shared by the 14/15 entry loops).
    const auto parseEntryPos =
        [&](const Json& e, const std::string& what,
            std::set<BlockPos>& seen) -> Result<BlockPos> {
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
        const BlockPos bpos {static_cast<int>(x), yi, static_cast<int>(z)};
        if (seen.count(bpos) != 0) {
            return MC_FAIL(what + " duplicates the state position " +
                           posStr(bpos));
        }
        seen.insert(bpos);
        return bpos;
    };
    if (fmtVersion >= 15) {
        if (stairArr != nullptr) {
            return MC_FAIL("save 'stairs' is restricted to format 14 (a "
                           "format-15 file carries 'states')");
        }
        if (statesArr == nullptr || !statesArr->isArray()) {
            return MC_FAIL("save format 15 requires a 'states' array");
        }
        std::set<BlockPos> entryPos;
        for (std::size_t i = 0; i < statesArr->size(); ++i) {
            const Json& e = statesArr->at(i);
            const std::string what =
                "save 'states' entry #" + std::to_string(i);
            if (!e.isObject()) {
                return MC_FAIL(what + " must be an object");
            }
            // Strict entry shape: exactly {pos, state}.
            const std::set<std::string> eFields {"pos", "state"};
            MC_TRY(rejectUnknownFields(e, eFields, what));
            Result<BlockPos> pr = parseEntryPos(e, what, entryPos);
            MC_TRY(pr);
            const BlockPos& bpos = pr.value();
            const Json* st = e.find("state");
            if (st == nullptr || !st->isNumber()) {
                return MC_FAIL(what + " requires numeric 'state'");
            }
            const int si = st->asInt();
            if (si < 0) {
                return MC_FAIL(what + " state " + std::to_string(si) +
                               " is out of range (it must be >= 0)");
            }
            // The domain check against the block at the entry's position
            // (block edit name -> registry -> stateCount): a stateful cell
            // names its range; an entry at a NON-stateful position is
            // caught by the cross-check below.
            const auto cit = stateCellBlock.find(bpos);
            if (cit != stateCellBlock.end()) {
                const Block* cb = blocks.findByName(cit->second);
                if (cb != nullptr && si >= cb->stateCount()) {
                    return MC_FAIL(what + " state " + std::to_string(si) +
                                   " is out of range [0, " +
                                   std::to_string(cb->stateCount()) +
                                   ") for the " + cit->second + " at " +
                                   posStr(bpos));
                }
            }
            save.states.push_back(
                SavedState {bpos, static_cast<std::uint8_t>(si)});
        }
        if (entryPos != stateCells) {
            for (const BlockPos& p : entryPos) {
                if (stateCells.count(p) == 0) {
                    return MC_FAIL("save 'states' has an entry at " +
                                   posStr(p) +
                                   " which is not a stateful block in the "
                                   "world's block edits");
                }
            }
            for (const BlockPos& p : stateCells) {
                if (entryPos.count(p) == 0) {
                    return MC_FAIL("the stateful block at " + posStr(p) +
                                   " has no 'states' entry (every "
                                   "stateful block owns a per-instance "
                                   "state)");
                }
            }
            return MC_FAIL("save 'states' does not match the world's "
                           "stateful blocks");
        }
    } else if (fmtVersion == 14) {
        if (statesArr != nullptr) {
            return MC_FAIL("save 'states' requires format 15 or newer (got " +
                           std::to_string(fmtVersion) + ")");
        }
        if (stairArr == nullptr || !stairArr->isArray()) {
            return MC_FAIL("save format 14 requires a 'stairs' array");
        }
        // A format-14 file's state cells are the STAIR cells (the Stairs
        // shape — the model state fields postdate format 14, so a 14 file
        // can only carry stair states).
        std::set<BlockPos> stairCells;
        for (const SavedBlockEdit& e : save.blockEdits) {
            const Block* b = blocks.findByName(e.blockName);
            if (b != nullptr &&
                b->model.shape == BlockModel::Shape::Stairs) {
                stairCells.insert(e.pos);
            }
        }
        std::set<BlockPos> entryPos;
        for (std::size_t i = 0; i < stairArr->size(); ++i) {
            const Json& e = stairArr->at(i);
            const std::string what =
                "save 'stairs' entry #" + std::to_string(i);
            if (!e.isObject()) {
                return MC_FAIL(what + " must be an object");
            }
            const std::set<std::string> eFields {"pos", "facing"};
            MC_TRY(rejectUnknownFields(e, eFields, what));
            Result<BlockPos> pr = parseEntryPos(e, what, entryPos);
            MC_TRY(pr);
            const BlockPos& bpos = pr.value();
            const Json* f = e.find("facing");
            if (f == nullptr || !f->isNumber()) {
                return MC_FAIL(what + " requires numeric 'facing'");
            }
            const int fi = f->asInt();
            if (fi < 0 || fi > 3) {
                return MC_FAIL(what + " facing " + std::to_string(fi) +
                               " is out of range [0, 3]");
            }
            // The VALUE-PRESERVING migration 14 -> 15: the facing becomes
            // the state (a stair's state IS its facing).
            save.states.push_back(
                SavedState {bpos, static_cast<std::uint8_t>(fi)});
        }
        if (entryPos != stairCells) {
            for (const BlockPos& p : entryPos) {
                if (stairCells.count(p) == 0) {
                    return MC_FAIL("save 'stairs' has an entry at " +
                                   posStr(p) +
                                   " which is not a stair block in the "
                                   "world's block edits");
                }
            }
            for (const BlockPos& p : stairCells) {
                if (entryPos.count(p) == 0) {
                    return MC_FAIL("the stair block at " + posStr(p) +
                                   " has no 'stairs' entry (every stair "
                                   "block owns a per-instance facing)");
                }
            }
            return MC_FAIL("save 'stairs' does not match the world's "
                           "stair blocks");
        }
    } else {
        // Format 13 and earlier: neither key may be present (a version
        // lie); the states migrate into this empty (the per-instance state
        // did not exist — the consumers fall back to the block's state
        // default on load).
        if (statesArr != nullptr) {
            return MC_FAIL("save 'states' requires format 15 or newer (got " +
                           std::to_string(fmtVersion) + ")");
        }
        if (stairArr != nullptr) {
            return MC_FAIL("save 'stairs' requires format 14 or newer (got " +
                           std::to_string(fmtVersion) + ")");
        }
    }

    // -- per-block crafting-table grids (format 19; CRAFT-1) -------------
    // A "table block" = a block whose `interact` property names the
    // crafting-table GUI (data-driven — no block-id switch; the furnace's
    // twin). World generation never emits tables, so the block edits are
    // the complete source of truth for which cells hold one.
    auto isTableBlock = [&](const std::string& name) {
        const BlockID id = blocks.idForName(name);
        return id.valid() &&
               blocks.at(id).properties.interact == kInteractCraftingTable;
    };
    std::set<BlockPos> tableCells;
    for (const SavedBlockEdit& e : save.blockEdits) {
        if (isTableBlock(e.blockName)) {
            tableCells.insert(e.pos);
        }
    }
    const Json* tablesArr = def.find("tables");
    if (fmtVersion >= 19) {
        // Format 19: per-block grids, cross-checked against the world's
        // table blocks in BOTH directions (an entry without a block or a
        // block without an entry would desync the game's invariant: every
        // table block owns a grid).
        if (tablesArr == nullptr || !tablesArr->isArray()) {
            return MC_FAIL("save format 19 requires a 'tables' array");
        }
        std::set<BlockPos> entryPos;
        for (std::size_t i = 0; i < tablesArr->size(); ++i) {
            const Json& e = tablesArr->at(i);
            const std::string what =
                "save 'tables' entry #" + std::to_string(i);
            if (!e.isObject()) {
                return MC_FAIL(what + " must be an object");
            }
            const std::set<std::string> eFields {"pos", "slots"};
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
                return MC_FAIL(what + " duplicates the table position " +
                               posStr(bpos));
            }
            entryPos.insert(bpos);
            const Json* slotsArr = e.find("slots");
            if (slotsArr == nullptr || !slotsArr->isArray() ||
                slotsArr->size() !=
                    static_cast<std::size_t>(CraftingGrid::kSize)) {
                return MC_FAIL(what + " requires a 'slots' array of "
                               "exactly " + std::to_string(CraftingGrid::kSize) +
                               " slot entries");
            }
            std::vector<ItemStack> slots;
            MC_TRY(parseGrid(*slotsArr, items, what + " 'slots'", slots));
            CraftingGrid grid;
            for (int c = 0; c < CraftingGrid::kSize; ++c) {
                grid.setSlot(c, slots[static_cast<std::size_t>(c)]);
            }
            save.tables.push_back(SavedTable {bpos, std::move(grid)});
        }
        if (entryPos != tableCells) {
            for (const BlockPos& p : entryPos) {
                if (tableCells.count(p) == 0) {
                    return MC_FAIL("save 'tables' has an entry at " +
                                   posStr(p) +
                                   " which is not a crafting_table block "
                                   "in the world's block edits");
                }
            }
            for (const BlockPos& p : tableCells) {
                if (entryPos.count(p) == 0) {
                    return MC_FAIL("the crafting_table block at " +
                                   posStr(p) +
                                   " has no 'tables' entry (every table "
                                   "block owns a grid, empty included)");
                }
            }
            return MC_FAIL("save 'tables' does not match the world's "
                           "crafting_table blocks");
        }
    } else if (tablesArr != nullptr) {
        return MC_FAIL("save 'tables' requires format 19 or newer (got " +
                       std::to_string(fmtVersion) + ")");
    }
    // The game's invariant after ANY load: every crafting_table block in
    // the world owns a grid entry. Format 19 guarantees it (the
    // cross-check); format 18 and older carry the ONE global
    // "table_grid" — when it is non-empty it belonged to the world's
    // (single) table: with exactly one table block it is ASSIGNED there;
    // with zero or several blocks the owner is unknowable, so the grid is
    // DISCARDED (all grids load empty — the Phase 44 sole-attribution
    // precedent). Formats 1-2 carried no grid at all (the entries stay
    // empty).
    if (fmtVersion < 19) {
        const bool nonEmptyLegacy = std::any_of(
            legacyTableGrid.begin(), legacyTableGrid.end(),
            [](const ItemStack& s) { return !s.empty(); });
        const bool sole = tableCells.size() == 1;
        for (const BlockPos& p : tableCells) {
            CraftingGrid grid;
            if (nonEmptyLegacy && sole) {
                for (int c = 0; c < CraftingGrid::kSize; ++c) {
                    grid.setSlot(
                        c, legacyTableGrid[static_cast<std::size_t>(c)]);
                }
            }
            save.tables.push_back(SavedTable {p, std::move(grid)});
        }
        // tableCells is a set: already position-sorted.
    }
    // The game's invariant after ANY load: every stateful block in the
    // world owns a per-instance state entry. Format 15 guarantees it (the
    // cross-check); format 14 migrates its stairs in value-preserving;
    // format 13 and earlier carry no entries at all (the per-instance
    // state did not exist — a hand-edited stateful block in an old file is
    // possible) — the consumers fall back to the block's state default on
    // load (the cells keep the sentinel).
    return save;
}

// ---------------------------------------------------------------------------
// SAVE-1: the auto-save (periodic + crash-resilient save writes)
// ---------------------------------------------------------------------------

Result<double> autoSaveIntervalSeconds(const Config& config) {
    const Json* j = config.at("save.auto_save_seconds");
    if (j == nullptr) {
        return kAutoSaveDefaultSeconds; // absent: the code default
    }
    if (!j->isNumber()) {
        return MC_FAIL("config save.auto_save_seconds must be a number");
    }
    const double v = j->asDouble();
    if (!std::isfinite(v)) {
        return MC_FAIL("config save.auto_save_seconds must be finite");
    }
    if (v < 0.0) {
        return MC_FAIL("config save.auto_save_seconds must be >= 0 "
                       "(0 = auto-save off), got " + std::to_string(v));
    }
    return v;
}

bool autoSaveStep(double& accum, double dt, double interval) {
    if (interval <= 0.0) {
        return false; // off: the timer is structurally inert
    }
    accum += dt;
    return accum >= interval;
}

Result<void> writeAtomicFile(const std::filesystem::path& path,
                             const std::string& content) {
    // The temp sits NEXT TO the destination (same directory) so the
    // final rename is a same-filesystem swap — atomic on POSIX.
    const std::filesystem::path tmp = path.string() + ".tmp";
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    {
        std::ofstream out(tmp);
        if (!out) {
            return MC_FAIL("cannot open save file for writing: '" +
                           path.string() + "'");
        }
        out << content;
        out.flush();
        if (!out) {
            std::filesystem::remove(tmp, ec); // best effort
            return MC_FAIL("write error for save file '" + path.string() +
                           "'");
        }
        out.close();
        if (!out) {
            std::filesystem::remove(tmp, ec); // best effort
            return MC_FAIL("write error for save file '" + path.string() +
                           "'");
        }
    }
    std::filesystem::rename(tmp, path, ec);
    if (ec) {
        // The failed rename leaves the destination untouched (the old
        // complete save, if any, survives) — drop the temp and report.
        std::filesystem::remove(tmp, ec); // best effort
        return MC_FAIL("cannot move '" + tmp.string() + "' to '" +
                       path.string() + "': " + ec.message());
    }
    return {};
}

Result<void> validateStats(const std::map<std::string, std::uint64_t>& stats,
                           const MobRegistry& mobs) {
    // SURV-7: the closed vocabulary — "deathCount" (fixed) plus one
    // "kill.<name>" per REGISTERED mob. Anything else is a named
    // rejection (the grows_to / on_hit_effect discipline).
    for (const auto& [name, count] : stats) {
        if (name == "deathCount") {
            continue;
        }
        static const char kKillPrefix[] = "kill.";
        if (name.size() <= sizeof(kKillPrefix) - 1 ||
            name.compare(0, sizeof(kKillPrefix) - 1, kKillPrefix) != 0) {
            return MC_FAIL("save 'player.stats' holds the unknown stat '" +
                           name + "' (the vocabulary is \"deathCount\" "
                           "and one \"kill.<mob>\" per registered mob)");
        }
        const std::string mobName =
            name.substr(sizeof(kKillPrefix) - 1);
        if (mobs.findByName(mobName) == nullptr) {
            return MC_FAIL("save 'player.stats' holds the stat "
                           "'" + name + "' but no mob named '" + mobName +
                           "' is registered");
        }
        (void)count; // the count's range is parse-checked
    }
    return {};
}

} // namespace mc
