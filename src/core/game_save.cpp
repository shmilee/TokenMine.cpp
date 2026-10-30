#include "core/game.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>

#include "core/json.hpp"

namespace mc {

// ---------------------------------------------------------------------------
// Save / load (Phase 13): seed + modifications + full game state
// ---------------------------------------------------------------------------

Result<SaveGame> Game::collectSave() const {
    SaveGame save;
    save.seed = m_generator->seed();
    save.time = m_dayCycle.timeOfDay();
    save.playerPos = m_player->position();
    // SAVE-2 (format 22): the player's velocity — the game always has
    // the real value ([0,0,0] at rest: the ENT-7 stop epsilon makes
    // rest exactly zero, so a no-input save carries the zero triple).
    save.playerVelocity = m_player->velocity();
    save.playerYaw = m_player->yaw;
    save.playerPitch = m_player->pitch;
    save.playerHealth = m_player->health();
    // Phase 72: the absorption pool (format 12; 0 when no layer is
    // active — the game never writes a pool without its effects).
    save.playerAbsorption = m_player->absorption();
    save.playerHunger = m_player->hunger();
    save.playerSaturation = m_player->saturation();
    // SURV-1: the equipped armor (format 16) — the piece names by slot
    // ("" = empty; the count is always 1 by the armor items' max_stack,
    // so the name is the whole state). SURV-3 (format 18): the pieces'
    // accumulated damage by slot (0 = full — the "damage" key is
    // written only when > 0, which flips the entry to its object shape).
    for (int slot = 0; slot < kArmorSlotCount; ++slot) {
        const ItemStack& worn = m_player->armorSlot(slot);
        save.playerArmor[static_cast<std::size_t>(slot)] =
            worn.empty() ? std::string {} : m_items.at(worn.item).name;
        save.playerArmorDamage[static_cast<std::size_t>(slot)] =
            worn.empty() ? 0.0f : worn.damage;
    }
    // SURV-2: the player's oxygen (format 17, always written — the air
    // units left, kOxygenMax while breathing). The drownTicks
    // accumulator is deliberately NOT carried (it restarts at 0 on
    // load — the tick-phase convention, the effects pattern).
    save.playerOxygen = m_drown.oxygen;
    save.selectedSlot = static_cast<int>(m_selectedSlot);
    save.slots.resize(m_inventory.size());
    for (std::size_t i = 0; i < save.slots.size(); ++i) {
        save.slots[i] = m_inventory.at(i);
    }
    // The crafting grids (format 3): items left in a grid are the player's
    // — saving them keeps a mid-craft state loadable. CRAFT-1: the E
    // panel's grid is the only global one — the table's grids are
    // per-block (the "tables" section below).
    save.invGrid.resize(CraftingGrid::kSize);
    for (int i = 0; i < CraftingGrid::kSize; ++i) {
        save.invGrid[static_cast<std::size_t>(i)] = m_inventoryGrid.at(i);
    }
    // The per-block furnace states (format 7; Phase 44): map iteration is
    // position-sorted (the save's order); every furnace block has an entry
    // (the setBlock invariant), empty states included.
    for (const auto& [pos, st] : m_states.furnaces) {
        save.furnaces.push_back({pos, st});
    }
    // The burning fire blocks (format 10; Phase 64): positions + the
    // real-dt seconds remaining (the map is position-sorted — the
    // serializer's write order; a reload continues burning).
    for (const auto& [pos, remaining] : m_states.fires) {
        save.fires.push_back(SavedFire {pos, remaining});
    }
    // The campfire cooking states (format 11; Phase 71): map iteration is
    // position-sorted (the save's order); every campfire block has an
    // entry (the setBlock invariant), empty states included.
    for (const auto& [pos, st] : m_states.campfires) {
        save.campfires.push_back({pos, st});
    }
    // The per-block crafting-table grids (format 19; CRAFT-1): map
    // iteration is position-sorted (the save's order); every
    // crafting_table block has an entry (the setBlock invariant), empty
    // grids included.
    for (const auto& [pos, grid] : m_states.tables) {
        save.tables.push_back(SavedTable {pos, grid});
    }
    // PIPE-2: the journal path is the default; it falls back to the
    // legacy full-world path when the journal is invalid (the
    // kSaveJournalMaxEntries cap was exceeded — logged at the crossing)
    // or --save-full-diff was given (the in-process equivalence oracle
    // and escape hammer). Both paths produce the same file format.
    const bool journalSave = m_saveJournal.valid && !m_args.saveFullDiff;
    // The per-instance block states (format 15; Phase 94 stair facing,
    // generalized in ENT-2).
    if (journalSave) {
        // PIPE-2: derived from the JOURNAL instead of the O(N) world
        // scan: worldgen places no stateful block (the init cross-check in
        // WorldGenerator::create + the content test pin it), so every
        // stateful cell was block-edited this session — it is in the block
        // journal. The live state read + the sentinel fallback are the same
        // logic the old scan had; a journal cell whose live block is no
        // longer stateful (broken again) is skipped, exactly as the old
        // scan's hasState gate. STRM-2: the retained (unloaded) columns'
        // block edits are never stateful either — a stateful placement
        // goes through the setBlock choke point (m_editedColumns
        // protection: the column never unloads) and the sapling stage
        // writes track m_states.saplings (protected the same way) — so the
        // journal-only derivation stays complete.
        std::vector<SavedState> states;
        for (const auto& entry : m_saveJournal.blocks) {
            for (const BlockPos& p : entry.second) {
                const BlockID id = m_world.getBlock(p.x, p.y, p.z);
                if (id == BlockID::air() || !m_blocks.at(id).hasState()) {
                    continue;
                }
                const std::uint8_t state =
                    m_world.getBlockState(p.x, p.y, p.z);
                // The state is always set for a placed stateful block (the
                // setBlock invariant); the sentinel (unset) is a
                // hand-edited stateful block in a format-13 file — fall
                // back to the block's state default.
                const std::uint8_t s =
                    (state != Chunk::kBlockStateUnset)
                        ? state
                        : static_cast<std::uint8_t>(
                              m_blocks.at(id).stateDefault());
                states.push_back(SavedState {p, s});
            }
        }
        std::sort(states.begin(), states.end(),
                  [](const SavedState& a, const SavedState& b) {
                      return a.pos < b.pos;
                  });
        save.states = std::move(states);
    } else {
        // Legacy (pre-PIPE-2): scan the world for the stateful cells (the
        // block hasState()) + record their per-instance state (the World's
        // per-cell byte — the project's first per-block state).
        // Position-sorted (the chunk/scan order). Every stateful block has
        // an entry (the setBlock invariant sets the state on placement).
        for (const ChunkPos& cp : m_world.chunkPositions()) {
            const Chunk* chunk = m_world.findChunk(cp);
            if (chunk == nullptr) {
                continue;
            }
            const int bx = cp.x * kChunkSize;
            const int by = cp.y * kChunkSize;
            const int bz = cp.z * kChunkSize;
            for (int ly = 0; ly < kChunkSize; ++ly) {
                for (int lz = 0; lz < kChunkSize; ++lz) {
                    for (int lx = 0; lx < kChunkSize; ++lx) {
                        const BlockID id = chunk->get(lx, ly, lz);
                        if (id == BlockID::air() ||
                            !m_blocks.at(id).hasState()) {
                            continue;
                        }
                        const std::uint8_t state =
                            chunk->stateAt(lx, ly, lz);
                        const BlockPos p {bx + lx, by + ly, bz + lz};
                        // The state is always set for a placed stateful
                        // block (the setBlock invariant); the sentinel
                        // (unset) is a hand-edited stateful block in a
                        // format-13 file — fall back to the block's state
                        // default.
                        const std::uint8_t s =
                            (state != Chunk::kBlockStateUnset)
                                ? state
                                : static_cast<std::uint8_t>(
                                      m_blocks.at(id).stateDefault());
                        save.states.push_back(SavedState {p, s});
                    }
                }
            }
        }
    }
    // The player's active status effects (format 8; Phase 46): names +
    // the real-dt seconds remaining (the serializer name-sorts; the tick
    // phase is deliberately not saved — a reload restarts the next tick
    // at the full interval).
    for (const ActiveEffect& a : m_playerEffects.list()) {
        // SURV-6: the LIVE instance's tier rides the save (the
        // aggregate's 3rd field — an active tier-1 effect saved as
        // tier 0 would silently downgrade on load; the gate-133 S2b
        // final save asserts the tier round trip).
        save.playerEffects.push_back(
            SavedEffect {m_effects.at(a.effect).name, a.remaining,
                         a.tier});
    }
    // The player's death-related statistics (format 21; SURV-7): the
    // game's counter map IS the save's (name-sorted by construction;
    // zero counters are absent from the map).
    save.playerStats = m_stats;
    for (const std::unique_ptr<Entity>& e : m_entities) {
        // Persisted entities are ItemEntities. Mobs (Phase 20) and arrows
        // (Phase 21) are EPHEMERAL by design — the save format expresses
        // them by not storing them (a mob/arrow alive at save time is
        // simply gone on load), so they are skipped, not an error. Any
        // OTHER kind means a code path this save format cannot express —
        // fail loudly.
        const ItemEntity* item = dynamic_cast<const ItemEntity*>(e.get());
        if (item == nullptr) {
            if (dynamic_cast<const MobEntity*>(e.get()) == nullptr &&
                dynamic_cast<const Projectile*>(e.get()) == nullptr) {
                return MC_FAIL(
                    "cannot save: an entity of a kind this build does not "
                    "persist exists");
            }
            continue;
        }
        SavedItemEntity se;
        se.item = item->itemName();
        se.count = item->count();
        se.pos = item->position();
        se.age = item->age();
        se.pickupDelay = item->pickupDelayRemaining();
        se.damage = item->damage(); // SURV-3 (the stack's accumulated)
        save.entities.push_back(std::move(se));
    }
    // The modifications: every cell that differs from what the seed
    // generates (block + fluid), sorted by position.
    if (journalSave) {
        // PIPE-2: the incremental diff — only the journal's EXISTING
        // chunks are regenerated + compared (the full-world O(N) scan is
        // gone). Byte-identical to diffWorld for the same world (the
        // equivalence the unit tests pin; --save-full-diff keeps the
        // legacy path as the in-process oracle). Phase 37: large journal
        // footprints diff on kSaveDiffWorkers threads.
        auto diff = journalDiff(m_world, makeChunkGenerator(), m_blocks,
                                m_saveJournal.blocks, m_saveJournal.fluids,
                                save.blockEdits, save.fluidEdits,
                                kSaveDiffWorkers);
        if (!diff.ok()) {
            return diff.error();
        }
    } else {
        // Legacy (pre-PIPE-2, and the fallback/oracle path): the full
        // world diff. Phase 37: large worlds diff on kSaveDiffWorkers
        // threads (P33: ~14 s exit on a ~100k-chunk world single-
        // threaded); the position-sorted output is byte-identical
        // either way.
        auto diff = diffWorld(m_world, makeChunkGenerator(), m_blocks,
                              save.blockEdits, save.fluidEdits,
                              kSaveDiffWorkers);
        if (!diff.ok()) {
            return diff.error();
        }
    }
    // STRM-2: the retained (unloaded) columns' diffs merge into the save
    // in BOTH paths (the journal diff and the legacy/oracle full diff
    // cover EXISTING chunks only — a retained column is unloaded, and its
    // journal entries were evicted at unload; without the merge the exit
    // save would silently drop the session's far-column fluid changes).
    // Duplicate cells are structurally impossible (the one-of invariant);
    // a session that never unloaded a modified column passes an empty
    // map (the no-op — the pre-STRM-2 file shape, bit-identical).
    mergeRetained(save, m_retainedColumns);
    return save;
}

Result<void> Game::writeSave(bool byAutoSave) const {
    auto saved = collectSave();
    MC_TRY(saved);
    auto serialized = serializeSave(*saved, m_items);
    MC_TRY(serialized);
    const std::filesystem::path path = savePath();
    // SAVE-1: the ATOMIC write (same-directory temp + rename) shared by
    // the exit save, the F5 save and the periodic auto-save — the slot
    // file holds either the previous complete save or this one, never a
    // half-written one (a kill mid-write cannot corrupt it).
    MC_TRY(writeAtomicFile(path, serialized->dump(2) + '\n'));
    MC_LOG_INFO((byAutoSave ? "World auto-saved to '" : "World saved to '") +
                path.string() + "' (" +
                std::to_string(saved->blockEdits.size()) +
                " block edits, " + std::to_string(saved->fluidEdits.size()) +
                " fluid edits, " + std::to_string(saved->entities.size()) +
                " entities, " + std::to_string(saved->slots.size()) +
                " slots)");
    return {};
}

Result<SaveGame> Game::readSaveFile(const std::filesystem::path& path) const {
    std::ifstream in(path);
    if (!in) {
        return MC_FAIL("cannot open save file '" + path.string() + "'");
    }
    std::stringstream ss;
    ss << in.rdbuf();
    auto parsed = Json::parse(ss.str());
    if (!parsed.ok()) {
        return MC_FAIL("invalid JSON in save file '" + path.string() + "': " +
                       parsed.error().message);
    }
    auto save =
        parseSave(*parsed, m_items, m_blocks, m_effects, m_generator->seed());
    if (!save.ok()) {
        return MC_FAIL("save file '" + path.string() + "': " +
                       save.error().message);
    }
    // SURV-7: the stats vocabulary check (every "kill.<name>" names a
    // REGISTERED mob; "deathCount" is fixed). The mob registry is not a
    // parseSave parameter (180+ call sites), so the check runs here, at
    // the single game-load choke point (the named rejection is the same
    // either way — the rescan-pattern twin).
    MC_TRY(validateStats(save->playerStats, m_mobTypes));
    return *save;
}

Result<void> Game::applySave(const SaveGame& save) {
    // 1) Cell edits onto the generated world (blocks first, then fluids).
    MC_TRY(applyWorldEdits(save, m_blocks, m_world));
    // The edited columns differ from the pristine generated terrain — the
    // save's diff must survive a re-save even when the player is currently
    // far away: protect them from the unload pass (applyWorldEdits writes
    // the World directly, bypassing the setBlock choke point's marking).
    for (const SavedBlockEdit& e : save.blockEdits) {
        m_editedColumns.insert({floorDiv(e.pos.x, kChunkSize),
                                floorDiv(e.pos.z, kChunkSize)});
    }
    for (const SavedFluidEdit& e : save.fluidEdits) {
        m_editedColumns.insert({floorDiv(e.pos.x, kChunkSize),
                                floorDiv(e.pos.z, kChunkSize)});
    }
    // 2) Lighting: a full re-propagation is exact for any edit count (the
    //    incremental local updates converge to the same values).
    Lighting::propagateAll(m_world, m_blocks);
    // 3) Player. restorePosition teleports (velocity zeroed) and re-pins
    //    the fall tracking to the restored Y (Phase 36: a save taken
    //    mid-air has no leave-the-ground transition on load, so without
    //    the pin the saved fall would land damage-free). When the save
    //    was taken at rest the one-frame gravity re-settle is
    //    position-exact (the floor plane is an integer).
    m_player->restorePosition(save.playerPos);
    // SAVE-2 (format 22): re-apply the saved velocity (pre-22 files
    // carry the zero triple = the legacy zeroed restore, bit-for-bit).
    // restorePosition's teleport already zeroed it, so this single
    // assignment is the whole motion restore.
    m_player->setVelocity(save.playerVelocity);
    m_player->yaw = save.playerYaw;
    m_player->pitch = save.playerPitch;
    // Hunger (Phase 23): format 1-3 saves have none (-1 = full).
    m_player->setHunger(save.playerHunger < 0.0f
                            ? m_player->maxHunger()
                            : save.playerHunger);
    // Saturation (Phase 28): format 1-4 saves have none (-1 = no buffer).
    m_player->setSaturation(
        save.playerSaturation < 0.0f ? 0.0f : save.playerSaturation);
    // Health is applied in 4e) AFTER the status effects: the Phase 56
    // absorption layer raises the effective max, and a saved health above
    // the BASE max is legal exactly when the saved effects carry a layer.
    m_camera.position = m_player->eyePosition();
    m_camera.yaw = m_player->yaw;
    m_camera.pitch = m_player->pitch;
    // 4) Inventory (replaces the hotbar.json fill) + selection.
    for (std::size_t i = 0; i < m_inventory.size(); ++i) {
        m_inventory.setSlot(i, save.slots[i]);
    }
    m_selectedSlot = static_cast<std::size_t>(save.selectedSlot);
    // 4b) Crafting grids (format 3; format 1/2 saves load them empty).
    // CRAFT-1: the E panel's grid is the only global one — the table's
    // grids are per-block (the 4c-quinque section below).
    m_inventoryGrid.clear();
    for (int i = 0; i < CraftingGrid::kSize; ++i) {
        if (i < static_cast<int>(save.invGrid.size())) {
            m_inventoryGrid.setSlot(i, save.invGrid[static_cast<std::size_t>(i)]);
        }
    }
    // 4c) The per-block furnace states (format 7; Phase 44). Format-6
    // saves load their ONE global state already migrated (exactly one
    // furnace block: assigned there; otherwise: discarded — save.cpp);
    // format 1-5 load no entries at all. The parser derived progressFor
    // on each state; the copy keeps it.
    m_states.furnaces.clear();
    for (const SavedFurnace& sf : save.furnaces) {
        m_states.furnaces.emplace(sf.pos, sf.state);
    }
    // 4c-bis) The burning fire blocks (format 10; Phase 64): positions +
    // the real-dt seconds remaining (a reload continues burning — like
    // the player's fire effect). The entries are parse-validated
    // (remaining > 0, one per fire block).
    m_states.fires.clear();
    for (const SavedFire& sf : save.fires) {
        m_states.fires.emplace(sf.pos, sf.remainingSeconds);
    }
    // Migration (format 9 and earlier): a fire block in the world's block
    // edits WITHOUT a fires entry is only possible in a hand-edited old
    // file (the fire block did not exist before format 10) — it starts at
    // the full lifetime (the empty-furnace-state migration pattern).
    if (m_states.fireId.valid()) {
        for (const SavedBlockEdit& e : save.blockEdits) {
            if (e.blockName == "fire") {
                m_states.fires.emplace(e.pos, m_states.fireLifetime);
            }
        }
    }
    // 4c-ter) The campfire cooking states (format 11; Phase 71). Format-
    // 11 saves carry one entry per campfire block (parse-validated,
    // cross-checked in both directions); format 10 and earlier carry none.
    m_states.campfires.clear();
    for (const SavedCampfire& sc : save.campfires) {
        m_states.campfires.emplace(sc.pos, sc.state);
    }
    // Migration (format 10 and earlier): a campfire-FAMILY block in the
    // world's block edits WITHOUT a campfires entry is only possible in
    // a hand-edited old file (the campfire block did not exist before
    // format 11, the extinguished one before format 13) — it starts
    // empty (the empty-furnace-state migration pattern).
    for (const SavedBlockEdit& e : save.blockEdits) {
        if ((e.blockName == "campfire" ||
             e.blockName == "campfire_extinguished") &&
            m_states.campfires.count(e.pos) == 0) {
            m_states.campfires.emplace(e.pos, CampfireCookState {});
        }
    }
    // 4c-quinque) The per-block crafting-table grids (format 19;
    // CRAFT-1). Format-19 saves carry one entry per crafting_table block
    // (parse-validated, cross-checked in both directions); format 18 and
    // earlier carry the ONE global "table_grid", migrated at parse
    // (sole attribution to the single table block, otherwise discarded —
    // the Phase 44 precedent, save.cpp). The parse-time invariant
    // guarantees an entry for EVERY table block in the world's block
    // edits, so the map is complete after this copy (the furnace load
    // path, the twin — no rescan needed).
    m_states.tables.clear();
    for (const SavedTable& st : save.tables) {
        m_states.tables.emplace(st.pos, st.grid);
    }
    // 4c-quater) The per-instance block states (format 15; Phase 94 stair
    // facing, generalized in ENT-2). Format-15 saves carry one entry per
    // stateful block (parse-validated, cross-checked in both directions);
    // format 14 carries the "stairs" entries (parsed + migrated into the
    // states, value-preserving); format 13 and earlier carry none (the
    // per-instance state did not exist — the cells keep the sentinel, and
    // the consumers fall back to the block's state default on load).
    // The block/fluid replay above ran through the World (not the game's
    // choke point), so the saved value is authoritative: it overwrites
    // whatever the replay left.
    for (const SavedState& ss : save.states) {
        m_world.setBlockState(ss.pos.x, ss.pos.y, ss.pos.z, ss.state);
    }
    // 4d) The player's active status effects (format 8; Phase 46). The
    // names are parse-validated against the registry (idForName cannot
    // fail); the tick phase starts at 0 (it is not saved — the next tick
    // lands at the full interval).
    m_playerEffects.clearAll();
    for (const SavedEffect& se : save.playerEffects) {
        // SURV-6: the saved tier (absent in format-19 files = 0 — the
        // migration; range-checked against the registry at parse).
        m_playerEffects.apply(m_effects.idForName(se.name), se.remaining,
                              se.tier);
    }
    // 4d-bis) The death-related statistics (format 21; SURV-7): the map
    // IS the state (vocabulary validated in readSaveFile; a pre-21 load
    // migrates to EMPTY — the system did not exist).
    m_stats = save.playerStats;
    // 4e) The player's health + absorption (Phase 16; the two SEPARATE
    // pools are set here in Phase 72, AFTER the effects of 4d). A
    // format-1 save has no health (-1 = full); a saved 0 reloads the
    // player dead (the death screen offers R). No syncAbsorption here:
    //   - a format-12 save carries BOTH pools, parse-validated (health
    //     in range, absorption in [0, 64] and <= the effects' total) and
    //     authoritative — the pool may sit BELOW the total (damage
    //     drains it between sync points; reloading must keep the drain);
    //   - a format <= 11 save holds the SHARED P56 health (the value
    //     included the layer): the migration splits it — the excess
    //     above the base max becomes the pool (it is <= the effects'
    //     total by the old model's own invariant), the health keeps the
    //     base part. A hand-edited format-11 health above base + total
    //     is a contract violation, named and rejected.
    const float baseMax = m_player->maxHealth();
    const float effectsTotal = m_playerEffects.totalAbsorption(m_effects);
    if (save.playerAbsorption < 0.0f) {
        const float h =
            save.playerHealth < 0.0f ? baseMax : save.playerHealth;
        if (h > baseMax + effectsTotal + 1e-4f) {
            return MC_FAIL("save 'player.health' is above the base max plus "
                           "the effects' absorption total (format 11 and "
                           "below: a hand-edited shared health)");
        }
        m_player->setAbsorption(std::max(0.0f, h - baseMax));
        m_player->setHealth(std::min(h, baseMax));
    } else {
        if (save.playerHealth >= 0.0f &&
            save.playerHealth > baseMax + 1e-4f) {
            return MC_FAIL("save 'player.health' is above the base max "
                           "(format 12: the absorption is a separate pool)");
        }
        m_player->setAbsorption(save.playerAbsorption);
        m_player->setHealth(save.playerHealth < 0.0f ? baseMax
                                                     : save.playerHealth);
    }
    // 4f) The player's armor (format 16; SURV-1). The names are
    // parse-validated (registered items whose data armor_slot matches
    // the save's key — idForName cannot fail); format 1-15 saves carry
    // no armor (all the slots stay empty, the default). SURV-3 (format
    // 18): the pieces' accumulated damage rides the slot (parse-
    // validated against the item's data durability) and the setter's
    // durability argument (the damage()'s wear step needs it — the
    // player holds no ItemRegistry, the points pattern).
    for (int slot = 0; slot < kArmorSlotCount; ++slot) {
        const std::string& name =
            save.playerArmor[static_cast<std::size_t>(slot)];
        if (name.empty()) {
            m_player->setArmorSlot(slot, ItemStack {}, 0);
            continue;
        }
        const Item& item = m_items.at(m_items.idForName(name));
        const float dmg =
            save.playerArmorDamage[static_cast<std::size_t>(slot)];
        m_player->setArmorSlot(slot, ItemStack {item.id, 1, dmg},
                               item.armorPoints, item.durability);
    }
    m_prevHealth = m_player->health(); // no hurt event from the load itself
    // 4g) The player's oxygen (format 17; SURV-2, the drowning
    // system). A format-17 save carries the value (parse-validated
    // [0, kOxygenMax]); format 1-16 loads full air (the sentinel —
    // the system did not exist, the player had full air). The
    // drownTicks accumulator is never saved — it restarts at 0 (the
    // tick-phase convention, the effects pattern).
    m_drown.oxygen =
        save.playerOxygen < 0 ? kOxygenMax : save.playerOxygen;
    m_drown.drownTicks = 0;
    // 5) Dropped items (with their lifetime/pickup timers restored;
    // SURV-3: the stack's accumulated damage too).
    for (const SavedItemEntity& e : save.entities) {
        const Item& item = m_items.at(m_items.idForName(e.item));
        auto entity = std::make_unique<ItemEntity>(
            m_world, m_blocks, e.item, item.maxStack, e.count, e.pos,
            e.damage);
        entity->setAge(e.age);
        entity->setPickupDelay(e.pickupDelay);
        m_entities.push_back(std::move(entity));
    }
    // 6) Day-cycle time (wins over --time for a loaded save).
    m_dayCycle.setTimeOfDay(save.time);
    return {};
}

Result<void> Game::tryLoadSave() {
    if (!m_args.saveSlot.has_value() || m_args.newWorld) {
        return {};
    }
    const std::filesystem::path path = savePath();
    if (!std::filesystem::exists(path)) {
        MC_LOG_INFO("No save at '" + path.string() + "' — starting fresh");
        return {};
    }
    auto save = readSaveFile(path);
    MC_TRY(save);
    MC_TRY(applySave(*save));
    MC_LOG_INFO("Loaded save '" + path.string() + "' (" +
                std::to_string(save->blockEdits.size()) +
                " block edits, " + std::to_string(save->fluidEdits.size()) +
                " fluid edits, " + std::to_string(save->entities.size()) +
                " entities)");
    return {};
}

} // namespace mc
