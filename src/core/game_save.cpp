#include "core/game.hpp"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <system_error>

#include "core/json.hpp"
#include "entity/breath_cloud.hpp"

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
        // V2-12 (format 27): the piece's applied enchantments (empty when
        // unworn / unenchanted — the serializer omits the key then).
        save.playerArmorEnchants[static_cast<std::size_t>(slot)] =
            worn.empty() ? std::map<std::string, int> {} : worn.enchants;
    }
    // SURV-2: the player's oxygen (format 17, always written — the air
    // units left, kOxygenMax while breathing). The drownTicks
    // accumulator is deliberately NOT carried (it restarts at 0 on
    // load — the tick-phase convention, the effects pattern).
    save.playerOxygen = m_drown.oxygen;
    // V2-11 (format 26): the player's XP pool (the TOTAL points — always
    // written, the oxygen / velocity key discipline). The level is NOT
    // carried: it is derived from the pool through core/xp_curve.hpp
    // (the single source of truth — a stored level would be a second,
    // driftable copy).
    save.playerXp = m_player->xpTotal();
    // V2-12 (format 27): the player's XP seed (always written — the oxygen
    // / velocity key discipline; the game always has the value, set at
    // init / load).
    save.playerXpSeed = m_player->xpSeed();
    // V2-13 (format 28): the player's respawn point (always written — the
    // oxygen / velocity key discipline; the game always has the value, the
    // world spawn at init, the bed's cell center after a sleep / day
    // point-set). The serializer stores the int cell triple (the floor of
    // the components — exact for both position classes); the parse
    // reconstructs the cell center.
    save.playerRespawn = m_player->spawnPos();
    // V3-1 Phase B (format 30): the player's dimension — ALWAYS written
    // (the strict vocabulary word; the position's frame of reference).
    // V3-10a (format 32): the three-way (the "end" word — the ≥32
    // three-word vocabulary, the per-format version-lie gate's twin).
    save.playerDimension =
        playerInNether() ? "nether"
        : playerInEnd() ? "end" : "overworld";
    // V3-11a (format 31): the creative flag — ALWAYS written (the
    // live mode; the collectSave's single choke point).
    save.playerCreative = m_player->isCreative();
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
        save.invGrid[static_cast<std::size_t>(i)] = m_sessions[0]->panel.inventoryGrid.at(i);
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
    const bool journalSave = m_streaming.saveJournal.valid && !m_args.saveFullDiff;
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
        // goes through the setBlock choke point (m_streaming.editedColumns
        // protection: the column never unloads) and the sapling stage
        // writes track m_states.saplings (protected the same way) — so the
        // journal-only derivation stays complete.
        std::vector<SavedState> states;
        for (const auto& entry : m_streaming.saveJournal.blocks) {
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
    // UX-7 / V2-3 (format 25): the player's tutorial progress — ALWAYS
    // carried (an empty object when done/skipped — the key discipline;
    // the disabled tutorial carries its untouched initial state
    // {series 0, first step, 0}, the brief's decision for a uniform
    // key). The series + step id come from the CURRENT data (the
    // machine's indexes are data-order — the validateTutorialGoals +
    // checkSeriesGlobalIds cross-checks made the ids stable within a
    // run; a save outlives data edits through the load-time reset).
    if (m_tutorial.done) {
        save.tutorial = TutorialSaveState::Done;
    } else {
        save.tutorial = TutorialSaveState::InProgress;
        save.tutorialSeries = m_tutorial.seriesIndex;
        save.tutorialId =
            m_tutorialSeries[m_tutorial.seriesIndex]
                [static_cast<std::size_t>(m_tutorial.stepIndex)].id;
        save.tutorialProgress = m_tutorial.progress;
    }
    // V3-12c2 (format 33): the PER-PLAYER array (entry 0 = the primary
    // mirror — the legacy members above; entries 1.. = the remote
    // players, the 12b D13's lifted face: the remote's personal state
    // IS the world state — the REJOIN's snapshot's authority). A solo
    // save is a one-entry array (the pre-33 shape's value-faithful
    // relocation).
    save.players.clear();
    for (std::size_t i = 0; i < m_players.size(); ++i) {
        const Player& p = *m_players[i];
        const PlayerSession& ps = *m_sessions[i];
        SavePlayer sp;
        if (i == 0) {
            sp.pos = save.playerPos;
            sp.velocity = save.playerVelocity;
            sp.yaw = save.playerYaw;
            sp.pitch = save.playerPitch;
            sp.health = save.playerHealth;
            sp.absorption = save.playerAbsorption;
            sp.hunger = save.playerHunger;
            sp.saturation = save.playerSaturation;
            sp.selectedSlot = save.selectedSlot;
            sp.slots = save.slots;
            sp.invGrid = save.invGrid;
            sp.effects = save.playerEffects;
            sp.stats = save.playerStats;
            sp.tutorial = save.tutorial;
            sp.tutorialSeries = save.tutorialSeries;
            sp.tutorialId = save.tutorialId;
            sp.tutorialProgress = save.tutorialProgress;
            sp.armor = save.playerArmor;
            sp.armorDamage = save.playerArmorDamage;
            sp.armorEnchants = save.playerArmorEnchants;
            sp.oxygen = save.playerOxygen;
            sp.xp = save.playerXp;
            sp.xpSeed = save.playerXpSeed;
            sp.respawn = save.playerRespawn;
            sp.dimension = save.playerDimension;
            sp.creative = save.playerCreative;
        } else {
            sp.pos = p.position();
            sp.velocity = p.velocity();
            sp.yaw = p.yaw;
            sp.pitch = p.pitch;
            sp.health = p.health();
            sp.absorption = p.absorption();
            sp.hunger = p.hunger();
            sp.saturation = p.saturation();
            sp.selectedSlot = static_cast<int>(ps.selectedSlot);
            sp.slots.resize(ps.inventory.size());
            for (std::size_t k = 0; k < sp.slots.size(); ++k) {
                sp.slots[k] = ps.inventory.at(k);
            }
            sp.invGrid.resize(CraftingGrid::kSize);
            for (int k = 0; k < CraftingGrid::kSize; ++k) {
                sp.invGrid[static_cast<std::size_t>(k)] =
                    ps.panel.inventoryGrid.at(static_cast<std::size_t>(k));
            }
            for (const ActiveEffect& a : ps.effects.list()) {
                sp.effects.push_back(
                    SavedEffect {m_effects.at(a.effect).name, a.remaining,
                                 a.tier});
            }
            sp.stats = ps.stats;
            if (ps.tutorial.done) {
                sp.tutorial = TutorialSaveState::Done;
            } else {
                sp.tutorial = TutorialSaveState::InProgress;
                sp.tutorialSeries = ps.tutorial.seriesIndex;
                if (ps.tutorial.seriesIndex >= 0 &&
                    ps.tutorial.seriesIndex <
                        static_cast<int>(m_tutorialSeries.size()) &&
                    ps.tutorial.stepIndex >= 0 &&
                    ps.tutorial.stepIndex < static_cast<int>(
                        m_tutorialSeries[static_cast<std::size_t>(
                            ps.tutorial.seriesIndex)].size())) {
                    sp.tutorialId = m_tutorialSeries
                        [static_cast<std::size_t>(ps.tutorial.seriesIndex)]
                        [static_cast<std::size_t>(ps.tutorial.stepIndex)]
                            .id;
                }
                sp.tutorialProgress = ps.tutorial.progress;
            }
            for (int k = 0; k < kArmorSlotCount; ++k) {
                const ItemStack& worn = p.armorSlot(k);
                sp.armor[static_cast<std::size_t>(k)] =
                    worn.empty() ? std::string {} : m_items.at(worn.item).name;
                sp.armorDamage[static_cast<std::size_t>(k)] =
                    worn.empty() ? 0.0f : worn.damage;
                sp.armorEnchants[static_cast<std::size_t>(k)] =
                    worn.empty() ? std::map<std::string, int> {} : worn.enchants;
            }
            sp.oxygen = ps.drown.oxygen;
            sp.xp = p.xpTotal();
            sp.xpSeed = p.xpSeed();
            sp.respawn = p.spawnPos();
            // The remote body lives in the primary's dimension (D9's
            // global-dimension ruling) — the entry's word mirrors it.
            sp.dimension = save.playerDimension;
            sp.creative = p.isCreative();
        }
        save.players.push_back(std::move(sp));
    }
    for (const std::unique_ptr<Entity>& e : m_entities) {
        // V2-11 (format 26): the XP orbs are the SECOND persisted entity
        // kind (the strict {type, xp, pos, age, pickup_delay} save entry —
        // no item / count / damage: the orb carries no stack). The
        // dynamic_cast isolation: the item path below is bit-level
        // unchanged (an orb is not an ItemEntity).
        if (const ExpOrbEntity* orb =
                dynamic_cast<const ExpOrbEntity*>(e.get())) {
            SavedXpEntity se;
            se.xp = orb->value();
            se.pos = orb->position();
            se.age = orb->age();
            se.pickupDelay = orb->pickupDelayRemaining();
            save.xpEntities.push_back(std::move(se));
            continue;
        }
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
        // V2-12 (format 27): the dropped stack's enchantments (empty =
        // unenchanted — the serializer omits the key then).
        se.enchants = item->enchants();
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
                                m_streaming.saveJournal.blocks, m_streaming.saveJournal.fluids,
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
    mergeRetained(save.blockEdits, save.fluidEdits,
                  m_streaming.retainedColumns);
    // V3-1 Phase B (format 30): the NETHER section (the overworld's diff's
    // twin — the lazy D2: the nether's world exists only after engagement,
    // so a no-travel session serializes the empty nether section = the
    // whitelist's keys with empty arrays, the S1 gate's "恰增白名单键").
    // The nether's journal (the nether's StreamingState — the overworld's
    // journal's twin; the nether's cell-mutation observer feeds it at
    // ensureNether). The nether's states derive from the nether's journal
    // (or the nether's world scan on the fallback path) exactly like the
    // overworld's: the nether's worldgen (fillNetherChunk) places no
    // stateful block (netherrack / bedrock are stateless, the lava is a
    // fluid), so every nether stateful cell (the player's portals) was
    // block-edited this session and is in the nether's block journal —
    // the journal-only derivation's completeness argument holds.
    if (m_nether.world) {
        const bool netherJournalSave =
            journalSave && m_nether.streaming.saveJournal.valid;
        if (netherJournalSave) {
            auto diff = journalDiff(
                *m_nether.world, makeNetherChunkGenerator(), m_blocks,
                m_nether.streaming.saveJournal.blocks,
                m_nether.streaming.saveJournal.fluids,
                save.netherBlockEdits, save.netherFluidEdits,
                kSaveDiffWorkers);
            if (!diff.ok()) {
                return diff.error();
            }
        } else {
            auto diff = diffWorld(*m_nether.world, makeNetherChunkGenerator(),
                                  m_blocks, save.netherBlockEdits,
                                  save.netherFluidEdits, kSaveDiffWorkers);
            if (!diff.ok()) {
                return diff.error();
            }
        }
        mergeRetained(save.netherBlockEdits, save.netherFluidEdits,
                      m_nether.streaming.retainedColumns);
        // The nether's per-instance states (the overworld's derivation's
        // twin — the journal path or the world scan).
        if (netherJournalSave) {
            std::vector<SavedState> netherStates;
            for (const auto& entry : m_nether.streaming.saveJournal.blocks) {
                for (const BlockPos& p : entry.second) {
                    const BlockID id =
                        m_nether.world->getBlock(p.x, p.y, p.z);
                    if (id == BlockID::air() ||
                        !m_blocks.at(id).hasState()) {
                        continue;
                    }
                    const std::uint8_t state =
                        m_nether.world->getBlockState(p.x, p.y, p.z);
                    const std::uint8_t ns =
                        (state != Chunk::kBlockStateUnset)
                            ? state
                            : static_cast<std::uint8_t>(
                                  m_blocks.at(id).stateDefault());
                    netherStates.push_back(SavedState {p, ns});
                }
            }
            std::sort(netherStates.begin(), netherStates.end(),
                      [](const SavedState& a, const SavedState& b) {
                          return a.pos < b.pos;
                      });
            save.netherStates = std::move(netherStates);
        } else {
            for (const ChunkPos& cp : m_nether.world->chunkPositions()) {
                const Chunk* chunk = m_nether.world->findChunk(cp);
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
                            const std::uint8_t ns =
                                (state != Chunk::kBlockStateUnset)
                                    ? state
                                    : static_cast<std::uint8_t>(
                                          m_blocks.at(id).stateDefault());
                            save.netherStates.push_back(SavedState {p, ns});
                        }
                    }
                }
            }
        }
    }
    // V3-1 Phase B (format 30): the nether's dropped items (the
    // overworld's entity loop's twin — the ITEM kind only; an XP orb in
    // the nether is the loud failure, the nether carries no XP grant
    // source in Phase B, the documented deviation 6).
    for (const std::unique_ptr<Entity>& e : m_nether.entities) {
        if (dynamic_cast<const ExpOrbEntity*>(e.get()) != nullptr) {
            return MC_FAIL(
                "cannot save: the nether holds an XP orb (the nether "
                "carries no XP grant source in Phase B)");
        }
        const ItemEntity* item = dynamic_cast<const ItemEntity*>(e.get());
        if (item == nullptr) {
            if (dynamic_cast<const MobEntity*>(e.get()) == nullptr &&
                dynamic_cast<const Projectile*>(e.get()) == nullptr) {
                return MC_FAIL(
                    "cannot save: an entity of a kind this build does not "
                    "persist exists in the nether");
            }
            continue; // mobs / arrows are ephemeral (the overworld twin)
        }
        SavedItemEntity se;
        se.item = item->itemName();
        se.count = item->count();
        se.pos = item->position();
        se.age = item->age();
        se.pickupDelay = item->pickupDelayRemaining();
        se.damage = item->damage();
        se.enchants = item->enchants();
        save.netherEntities.push_back(std::move(se));
    }
    // V3-10a (format 32): the END section (the nether section's member-
    // for-member twin — the lazy D2: the End's world exists only after
    // engagement, so a no-travel session serializes the empty end
    // section = the whitelist's keys with empty arrays, the S1 gate's
    // "恰增白名单键" triple). The End's worldgen (fillEndChunk) places
    // no stateful block (endstone / obsidian / the frame family are
    // stateless), so the journal-only derivation's completeness argument
    // holds exactly as the nether's.
    if (m_end.world) {
        const bool endJournalSave =
            journalSave && m_end.streaming.saveJournal.valid;
        if (endJournalSave) {
            auto diff = journalDiff(
                *m_end.world, makeEndChunkGenerator(), m_blocks,
                m_end.streaming.saveJournal.blocks,
                m_end.streaming.saveJournal.fluids,
                save.endBlockEdits, save.endFluidEdits, kSaveDiffWorkers);
            if (!diff.ok()) {
                return diff.error();
            }
        } else {
            auto diff = diffWorld(*m_end.world, makeEndChunkGenerator(),
                                  m_blocks, save.endBlockEdits,
                                  save.endFluidEdits, kSaveDiffWorkers);
            if (!diff.ok()) {
                return diff.error();
            }
        }
        mergeRetained(save.endBlockEdits, save.endFluidEdits,
                      m_end.streaming.retainedColumns);
        // The End's per-instance states (the nether's derivation twin —
        // the journal path or the world scan; 10a's fill is stateless,
        // so a player-placed stateful block is the only source).
        if (endJournalSave) {
            std::vector<SavedState> endStates;
            for (const auto& entry : m_end.streaming.saveJournal.blocks) {
                for (const BlockPos& p : entry.second) {
                    const BlockID id = m_end.world->getBlock(p.x, p.y, p.z);
                    if (id == BlockID::air() ||
                        !m_blocks.at(id).hasState()) {
                        continue;
                    }
                    const std::uint8_t state =
                        m_end.world->getBlockState(p.x, p.y, p.z);
                    const std::uint8_t ns =
                        (state != Chunk::kBlockStateUnset)
                            ? state
                            : static_cast<std::uint8_t>(
                                  m_blocks.at(id).stateDefault());
                    endStates.push_back(SavedState {p, ns});
                }
            }
            std::sort(endStates.begin(), endStates.end(),
                      [](const SavedState& a, const SavedState& b) {
                          return a.pos < b.pos;
                      });
            save.endStates = std::move(endStates);
        } else {
            for (const ChunkPos& cp : m_end.world->chunkPositions()) {
                const Chunk* chunk = m_end.world->findChunk(cp);
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
                            const std::uint8_t ns =
                                (state != Chunk::kBlockStateUnset)
                                    ? state
                                    : static_cast<std::uint8_t>(
                                          m_blocks.at(id).stateDefault());
                            save.endStates.push_back(SavedState {p, ns});
                        }
                    }
                }
            }
        }
    }
    // V3-10a (format 32): the End's dropped items (the nether's entity
    // loop's twin — the ITEM kind only; the mobs (10c) / the arrows /
    // the clouds are EPHEMERAL (skipped, the overworld's mob belt) and
    // the 10d dragon's XP orbs are SKIPPED too (the documented
    // deviation: the End's orbs are not persisted — their 120-s fuse
    // bounds the loss, the MC persistence read would need the
    // format-33 end_xp_entities key, the 10d2 re-discussion).
    // V3-12c2: the End's XP orbs are PERSISTED (the 10d1's deferred
    // face — the skip lifted: the dragon's burst / the enderman's
    // kills survive a reload; the orbs ride the dedicated
    // end_xp_entities segment, the end_entities' item-only shape
    // unchanged).
    for (const std::unique_ptr<Entity>& e : m_end.entities) {
        if (const ExpOrbEntity* orb =
                dynamic_cast<const ExpOrbEntity*>(e.get())) {
            SavedXpEntity se;
            se.xp = orb->value();
            se.pos = orb->position();
            se.age = orb->age();
            se.pickupDelay = orb->pickupDelayRemaining();
            save.endXpEntities.push_back(std::move(se));
            continue;
        }
        const ItemEntity* item = dynamic_cast<const ItemEntity*>(e.get());
        if (item == nullptr) {
            if (dynamic_cast<const MobEntity*>(e.get()) == nullptr &&
                dynamic_cast<const Projectile*>(e.get()) == nullptr &&
                dynamic_cast<const BreathCloudEntity*>(e.get()) ==
                    nullptr) {
                return MC_FAIL(
                    "cannot save: an entity of a kind this build does not "
                    "persist exists in the End");
            }
            continue; // mobs / arrows / clouds are ephemeral (the
                      // overworld twin)
        }
        SavedItemEntity se;
        se.item = item->itemName();
        se.count = item->count();
        se.pos = item->position();
        se.age = item->age();
        se.pickupDelay = item->pickupDelayRemaining();
        se.damage = item->damage();
        se.enchants = item->enchants();
        save.endEntities.push_back(std::move(se));
    }
    return save;
}

Result<void> Game::writeSave(bool byAutoSave) const {
    // V3-7: the save-write duration (real-dt wall clock — the
    // performance log line, never part of a byte gate; the perf A/B's
    // IO-increment metric).
    const auto started = std::chrono::steady_clock::now();
    auto saved = collectSave();
    MC_TRY(saved);
    auto serialized = serializeSave(*saved, m_items);
    MC_TRY(serialized);
    const std::filesystem::path path = savePath();
    const std::string payload = serialized->dump(2) + '\n';
    // V3-7: the on-disk encoding (save.compression): Gzip = the
    // deterministic container (kGzipHeader + raw deflate + CRC32 /
    // ISIZE — byte-reproducible, the determinism first-class rule);
    // None = the plaintext payload (the pre-V3-7 behavior,
    // bit-identical). Either encoding goes through the SAME atomic
    // write (a kill mid-write cannot corrupt the slot).
    std::string disk = payload;
    if (m_saveCompression == SaveCompression::Gzip) {
        auto gz = gzipEncode(payload);
        MC_TRY(gz);
        disk = std::move(*gz);
    }
    // SAVE-1: the ATOMIC write (same-directory temp + rename) shared by
    // the exit save, the F5 save and the periodic auto-save — the slot
    // file holds either the previous complete save or this one, never a
    // half-written one (a kill mid-write cannot corrupt it).
    MC_TRY(writeAtomicFile(path, disk));
    char msBuf[32];
    std::snprintf(msBuf, sizeof(msBuf), "%.1f",
                  std::chrono::duration<double, std::milli>(
                      std::chrono::steady_clock::now() - started)
                      .count());
    const std::string encoding =
        m_saveCompression == SaveCompression::Gzip
            ? "gzip: " + std::to_string(payload.size()) +
                  " B payload -> " + std::to_string(disk.size()) +
                  " B on disk"
            : "plain: " + std::to_string(payload.size()) + " B";
    MC_LOG_INFO((byAutoSave ? "World auto-saved to '" : "World saved to '") +
                path.string() + "' (" +
                std::to_string(saved->blockEdits.size()) +
                " block edits, " + std::to_string(saved->fluidEdits.size()) +
                " fluid edits, " +
                std::to_string(saved->entities.size() +
                               saved->xpEntities.size()) +
                " entities, " + std::to_string(saved->slots.size()) +
                " slots) in " + msBuf + " ms (" + encoding + ")");
    return {};
}

Result<SaveGame> Game::readSaveFile(const std::filesystem::path& path) const {
    std::string raw;
    {
        // V3-7: binary mode — the file may be the gzip container
        // (this build's write default) as well as the legacy plaintext.
        std::ifstream in(path, std::ios::binary);
        if (!in) {
            return MC_FAIL("cannot open save file '" + path.string() + "'");
        }
        std::stringstream ss;
        ss << in.rdbuf();
        raw = ss.str();
    }
    // V3-7: the on-disk encoding probe — the gzip magic 1f 8b = the
    // deterministic gzip container (strict decode: the header pinned
    // byte-for-byte, the body inflated, the CRC32 / ISIZE trailer
    // double-checked — a failure is named, carrying the path);
    // anything else = the legacy plaintext (format 28 and older),
    // loaded directly (the zero-migration rule).
    std::string text = raw;
    if (looksLikeGzip(raw)) {
        auto decoded = gzipDecode(raw, path.string());
        MC_TRY(decoded);
        text = std::move(*decoded);
    }
    auto parsed = Json::parse(text);
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
    MC_TRY(validateStats(save->playerStats, m_mobs.mobTypes));
    // V2-12 (format 27): the enchant cross-check (every enchant id on any
    // stack is registered, within its max, class-compatible with the item,
    // and not an incompatible pair). The registries are not parseSave
    // parameters, so it runs here at the single game-load choke point (the
    // validateStats twin).
    MC_TRY(validateEnchants(*save, m_items, m_enchants));
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
        m_streaming.editedColumns.insert({floorDiv(e.pos.x, kChunkSize),
                                floorDiv(e.pos.z, kChunkSize)});
    }
    for (const SavedFluidEdit& e : save.fluidEdits) {
        m_streaming.editedColumns.insert({floorDiv(e.pos.x, kChunkSize),
                                floorDiv(e.pos.z, kChunkSize)});
    }
    // 2) Lighting: a full re-propagation is exact for any edit count (the
    //    incremental local updates converge to the same values).
    Lighting::propagateAll(m_world, m_blocks);
    // 2b) V3-1 Phase B (D9's load ordering): the NETHER — engage the
    //     dimension when the save needs it (the nether section is
    //     non-empty OR the player sits in the nether), replay the
    //     nether's cell edits onto the generated nether (the overworld's
    //     applyWorldEdits on the nether's world + the nether's derived-
    //     seed fill reference), protect the edited columns, re-light the
    //     nether (the SKYLESS source 0 — the stored nether sky field is
    //     structurally 0, the day/night shader term inert), apply the
    //     nether's per-instance states (the portal axes — the overworld's
    //     states replay's twin), and restore the nether's dropped items
    //     (the nether's entity list — D3: the items stay in their
    //     dimension). A no-nether save (a format-29 migration, or a
    //     no-travel session) never engages the nether (the lazy D2 —
    //     the structural-immunity root: the load allocates nothing).
    const bool netherNeeded = !save.netherBlockEdits.empty() ||
                              !save.netherFluidEdits.empty() ||
                              !save.netherStates.empty() ||
                              !save.netherEntities.empty() ||
                              save.playerDimension == "nether";
    if (netherNeeded) {
        ensureNether();
        if (!m_nether.world) {
            return MC_FAIL("cannot load the nether section: the nether "
                           "is unbuildable (the asset set is missing the "
                           "netherrack or bedrock block)");
        }
        // The nether's edits replay (the overworld's applyWorldEdits on
        // the nether's world — the temporary SaveGame view carries the
        // nether's lists in the overworld's fields, the function's
        // contract: save.blockEdits / save.fluidEdits).
        SaveGame netherView;
        netherView.blockEdits = save.netherBlockEdits;
        netherView.fluidEdits = save.netherFluidEdits;
        MC_TRY(applyWorldEdits(netherView, m_blocks, *m_nether.world));
        // The nether's edited columns (the overworld's protection's
        // twin — the nether's diff must survive a re-save even when the
        // player is far away).
        for (const SavedBlockEdit& e : save.netherBlockEdits) {
            m_nether.streaming.editedColumns.insert(
                {floorDiv(e.pos.x, kChunkSize),
                 floorDiv(e.pos.z, kChunkSize)});
        }
        for (const SavedFluidEdit& e : save.netherFluidEdits) {
            m_nether.streaming.editedColumns.insert(
                {floorDiv(e.pos.x, kChunkSize),
                 floorDiv(e.pos.z, kChunkSize)});
        }
        // The nether's lighting (the SKYLESS source 0 — the stored sky
        // field is structurally 0, the nether's day/night inert).
        Lighting::propagateAll(*m_nether.world, m_blocks, 0);
        // The nether's per-instance states (the overworld's states
        // replay's twin — the saved value is authoritative: it
        // overwrites whatever the replay left).
        for (const SavedState& ss : save.netherStates) {
            m_nether.world->setBlockState(ss.pos.x, ss.pos.y, ss.pos.z,
                                          ss.state);
        }
        // The nether's dropped items (the nether's entity list — the
        // overworld's item restore's twin; the nether carries the ITEM
        // kind only, the parse's rejection).
        for (const SavedItemEntity& e : save.netherEntities) {
            const Item& item = m_items.at(m_items.idForName(e.item));
            auto entity = std::make_unique<ItemEntity>(
                *m_nether.world, m_blocks, e.item, item.maxStack, e.count,
                e.pos, e.damage, e.enchants);
            entity->setAge(e.age);
            entity->setPickupDelay(e.pickupDelay);
            m_nether.entities.push_back(std::move(entity));
        }
    }
    // V3-10a (format 32): the END section (the nether section's
    // member-for-member twin — the lazy D2: engaged only when the save
    // carries an end edit / the player sits in the End).
    const bool endNeeded = !save.endBlockEdits.empty() ||
        !save.endFluidEdits.empty() || !save.endStates.empty() ||
        !save.endEntities.empty() || !save.endXpEntities.empty() ||
        save.playerDimension == "end";
    if (endNeeded) {
        ensureEnd();
        if (!m_end.world) {
            return MC_FAIL("cannot load the end section: the End is "
                           "unbuildable (the asset set is missing the "
                           "endstone or obsidian block)");
        }
        // The End's edits replay (the netherView pattern — the
        // temporary SaveGame view carries the End's lists).
        SaveGame endView;
        endView.blockEdits = save.endBlockEdits;
        endView.fluidEdits = save.endFluidEdits;
        MC_TRY(applyWorldEdits(endView, m_blocks, *m_end.world));
        // The End's edited columns (the nether's protection twin).
        for (const SavedBlockEdit& e : save.endBlockEdits) {
            m_end.streaming.editedColumns.insert(
                {floorDiv(e.pos.x, kChunkSize),
                 floorDiv(e.pos.z, kChunkSize)});
        }
        for (const SavedFluidEdit& e : save.endFluidEdits) {
            m_end.streaming.editedColumns.insert(
                {floorDiv(e.pos.x, kChunkSize),
                 floorDiv(e.pos.z, kChunkSize)});
        }
        // The End's lighting (the SKYLESS source 0 — the nether's twin).
        Lighting::propagateAll(*m_end.world, m_blocks, 0);
        // The End's per-instance states (the nether's states replay
        // twin — the saved value is authoritative).
        for (const SavedState& ss : save.endStates) {
            m_end.world->setBlockState(ss.pos.x, ss.pos.y, ss.pos.z,
                                       ss.state);
        }
        // The End's dropped items (the nether's entity restore twin).
        for (const SavedItemEntity& e : save.endEntities) {
            const Item& item = m_items.at(m_items.idForName(e.item));
            auto entity = std::make_unique<ItemEntity>(
                *m_end.world, m_blocks, e.item, item.maxStack, e.count,
                e.pos, e.damage, e.enchants);
            entity->setAge(e.age);
            entity->setPickupDelay(e.pickupDelay);
            m_end.entities.push_back(std::move(entity));
        }
        // V3-12c2 (format 33): the End's XP orbs (the 10d1's deferred
        // face — the 5-bis's End twin: the dragon's burst / the
        // enderman's kills restore with their value + position + the
        // two lifetime timers).
        for (const SavedXpEntity& e : save.endXpEntities) {
            auto orb =
                std::make_unique<ExpOrbEntity>(*m_end.world, m_blocks,
                                               e.xp, e.pos);
            orb->setAge(e.age);
            orb->setPickupDelay(e.pickupDelay);
            m_end.entities.push_back(std::move(orb));
        }
    }
    // 3) Player. restorePosition teleports (velocity zeroed) and re-pins
    //    the fall tracking to the restored Y (Phase 36: a save taken
    //    mid-air has no leave-the-ground transition on load, so without
    //    the pin the saved fall would land damage-free). When the save
    //    was taken at rest the one-frame gravity re-settle is
    //    position-exact (the floor plane is an integer). V3-1 Phase B:
    //    the player's position is in the CURRENT dimension's
    //    coordinates (the format-30 dimension key's frame of reference) —
    //    a nether load rebinds the player to the nether's world (D3's
    //    setWorld — the only cross-dimension entity) + flips the
    //    dimension + runs the catch-up disk around the landing (the
    //    travel's twin: the nether's streaming has never engaged this
    //    column, the 3x3 disk ensures the neighborhood is complete +
    //    lit). The overworld load (the default — the pre-Phase-B path)
    //    runs the exact legacy sequence, bit-for-bit.
    // V3-11a (format 31): the creative mode restore (the flag + the
    // flight default — a creative load flies, the G toggle's twin; a
    // survival load is untouched, bit-identical).
    m_player->setCreative(save.playerCreative);
    if (save.playerCreative) {
        m_player->setFlying(true);
    }
    if (save.playerDimension == "nether") {
        m_player->setWorld(*m_nether.world);
        m_playerDimension = Dimension::Nether;
        {
            const int cx = floorDiv(
                static_cast<int>(std::floor(save.playerPos.x)), kChunkSize);
            const int cz = floorDiv(
                static_cast<int>(std::floor(save.playerPos.z)), kChunkSize);
            catchUpDisk(Dimension::Nether, cx, cz);
        }
    } else if (save.playerDimension == "end") {
        // V3-10a (format 32): the End's player rebind (the nether's
        // branch verbatim — the D3 setWorld + the flip + the catch-up
        // disk around the landing; the u_ambient's flip-frame set —
        // the End's kAmbientEnd, the load's presentation restore).
        m_player->setWorld(*m_end.world);
        m_playerDimension = Dimension::End;
        {
            const int cx = floorDiv(
                static_cast<int>(std::floor(save.playerPos.x)), kChunkSize);
            const int cz = floorDiv(
                static_cast<int>(std::floor(save.playerPos.z)), kChunkSize);
            catchUpDisk(Dimension::End, cx, cz);
        }
        applyDimensionAmbient();
    }
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
    m_sessions[0]->panel.inventoryGrid.clear();
    for (int i = 0; i < CraftingGrid::kSize; ++i) {
        if (i < static_cast<int>(save.invGrid.size())) {
            m_sessions[0]->panel.inventoryGrid.setSlot(i, save.invGrid[static_cast<std::size_t>(i)]);
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
        // V2-12 (format 27): the piece's enchantments ride the stack
        // (parse-shape-validated; the full cross-check runs in
        // validateEnchants, the validateStats twin).
        ItemStack piece {item.id, 1, dmg};
        piece.enchants =
            save.playerArmorEnchants[static_cast<std::size_t>(slot)];
        m_player->setArmorSlot(slot, piece, item.armorPoints,
                               item.durability, item.fireResistant);
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
    // 4g-bis) The player's XP pool (format 26; V2-11). A format-26
    // save carries the value (parse-validated whole number >= 0);
    // format 1-25 loads 0 (the sentinel -1 — the system did not
    // exist, the player had no XP; setXpTotal's clamp handles it).
    // The level is derived on read (core/xp_curve.hpp — never saved).
    m_player->setXpTotal(save.playerXp);
    // 4g-ter) The player's XP seed (format 27; V2-12). parseSave ALWAYS
    // fills this (a format-27 read, or the deterministic worldSeed ^
    // kXpSeedInit migration for a pre-27 load), so the restore is direct.
    m_player->setXpSeed(save.playerXpSeed);
    // 4g-quater) The player's respawn point (format 28; V2-13). A
    // format-28 save carries the cell center (parse-reconstructed —
    // the bed's cell from the last sleep / point-set, or the world
    // spawn's cell); the restore is direct. A pre-28 load carries the
    // sentinel (the system did not exist — the respawn was always the
    // world spawn): the migration re-pins the remembered spawn to the
    // WORLD SPAWN (m_mobs.spawnPoint — captured at init BEFORE this
    // save load, the spawn-protection anchor; the parse cannot know it
    // — the generator's room-checked scan lives in the game's init).
    if (save.playerRespawn.y >= 0.0f) {
        m_player->setSpawnPos(save.playerRespawn);
    } else {
        m_player->setSpawnPos(m_mobs.spawnPoint);
    }
    // 4h) The player's tutorial progress (format 23; UX-7, the series
    // from format 25 / V2-3). The pure restore (tutorialRestore) owns
    // the cross-checks: the series must name a series of the CURRENT
    // data (else: reset to fresh — the data is content, the save is
    // state), the id must name a step of THAT series (else: the same
    // reset — the kickoff decision: an info log, NOT a load failure),
    // and the progress must sit strictly below the step's count (a
    // completed step would have advanced — corruption, named
    // rejection). A format 23/24 save carries no series (it defaults
    // to 0 — the series-0 data is byte-unchanged, value-faithful, the
    // UX-8a twin); pre-23 loads migrate to FRESH (the system did not
    // exist). A loaded done/skipped state does NOT arm the toast (the
    // toast is a live-session presentation — it belongs to the
    // completion frame, the camera-mode session-state class):
    // m_tutorialToastFrames stays 0.
    bool tutReset = false;
    auto tutState = tutorialRestore(save.tutorial, save.tutorialSeries,
                                    save.tutorialId, save.tutorialProgress,
                                    m_tutorialSeries, &tutReset);
    MC_TRY(tutState);
    m_tutorial = *tutState;
    if (tutReset) {
        MC_LOG_INFO("Save tutorial step '" + save.tutorialId +
                    "' (series " + std::to_string(save.tutorialSeries) +
                    ") is not in the current tutorial data — resetting "
                    "to the first step");
    }
    // V3-12c2 (format 33): the GHOST players (the save's players[]
    // entries 1.. — the remote players of the session that took the
    // save). A solo load of a multi-player save restores their bodies
    // as idle sim entities (the documented 12c2 face: the personal
    // state IS the world state — the bodies run zero input, the
    // net session's zero-relay twin). A one-entry array (the solo /
    // pre-33-migrated shape) runs this loop zero times — the
    // bit-identical pre-33 load. The NET HOST (m_args.listenPort != 0)
    // SUPPRESSES the ghosts: after a restart the saved other-players
    // are stale (their connections died) — the host's player set is
    // {itself} + {the newly-connected clients} (the slot 1.. base's
    // netAddPlayer's premise, the m_nets' index = slot-1's map); a
    // reconnecting client gets a FRESH body (the D25's deviation — its
    // old items rode the removal's 100% drop, the document's entities'
    // pickup's path). The net CLIENT keeps the ghosts (it reconstructs
    // the OTHER connected players' bodies from the document, the host
    // keeps them live via its netAddPlayer's — the lockstep's same-set).
    if (m_args.listenPort == 0) {
        for (std::size_t i = 1; i < save.players.size(); ++i) {
            MC_TRY(applyGhostPlayer(save.players[i]));
        }
    }
    // 5) Dropped items (with their lifetime/pickup timers restored;
    // SURV-3: the stack's accumulated damage; V2-12 format 27: the
    // stack's enchantments too).
    for (const SavedItemEntity& e : save.entities) {
        const Item& item = m_items.at(m_items.idForName(e.item));
        auto entity = std::make_unique<ItemEntity>(
            m_world, m_blocks, e.item, item.maxStack, e.count, e.pos,
            e.damage, e.enchants);
        entity->setAge(e.age);
        entity->setPickupDelay(e.pickupDelay);
        m_entities.push_back(std::move(entity));
    }
    // 5-bis) The XP orbs (format 26; V2-11): the second entity kind,
    // restored with their value + position + the two lifetime timers
    // (the item-entity twin — the value is parse-validated 1..9, the
    // constructor's clamp is unreachable). A format-25-and-older save
    // carries none (the list is empty — the migration).
    for (const SavedXpEntity& e : save.xpEntities) {
        auto orb =
            std::make_unique<ExpOrbEntity>(m_world, m_blocks, e.xp, e.pos);
        orb->setAge(e.age);
        orb->setPickupDelay(e.pickupDelay);
        m_entities.push_back(std::move(orb));
    }
    // 6) Day-cycle time (wins over --time for a loaded save).
    m_dayCycle.setTimeOfDay(save.time);
    return {};
}

Result<Json> Game::collectNetSnapshot() const {
    // V3-12c2: the REJOIN's snapshot document (collectSave's twin +
    // the session's counter family + the live particles — the hash
    // field list's state the on-disk save does not carry; the
    // mob-quiet's contract: a live mob's AI state is the deferred
    // face, the D24's named rejection — the initial window's
    // save-form join is structurally mob-free (the format-30's
    // ephemeral design: a loaded world has no mobs at frame zero)).
    for (const auto* ents :
         {&m_entities, &m_nether.entities, &m_end.entities}) {
        for (const auto& e : *ents) {
            if (dynamic_cast<const MobEntity*>(e.get()) != nullptr) {
                return MC_FAIL("net: the rejoin snapshot cannot carry "
                               "mobs (the AI state's transfer is the "
                               "deferred face — the D24's named "
                               "rejection)");
            }
        }
    }
    auto cs = collectSave();
    MC_TRY(cs);
    SaveGame save = std::move(*cs);
    for (const Particle& pt : m_particles.particles()) {
        SavedParticle sp;
        sp.position = pt.position;
        sp.velocity = pt.velocity;
        sp.age = pt.age;
        sp.life = pt.life;
        sp.block = m_blocks.at(pt.block).name;
        sp.skyLight = pt.skyLight;
        sp.blockLight = pt.blockLight;
        sp.gravity = pt.gravity;
        save.particles.push_back(std::move(sp));
    }
    save.netCounters.weatherPresent = true;
    save.netCounters.raining = m_weather.raining;
    save.netCounters.weatherFramesLeft = m_weather.framesLeft;
    save.netCounters.weatherThunderLeft = m_weather.thunderLeft;
    save.netCounters.weatherStream = m_weatherStream;
    save.netCounters.weatherThunderStream = m_weatherThunderStream;
    save.netCounters.spawnCountersPresent = true;
    save.netCounters.spawnCounterOverworld = m_mobs.spawnCounter;
    save.netCounters.spawnCounterNether = m_nether.mobs.spawnCounter;
    save.netCounters.spawnCounterEnd = m_end.mobs.spawnCounter;
    save.netCounters.particleBreakCounterPresent = true;
    save.netCounters.particleBreakCounter = m_particleBreakCounter;
    // The per-player session state (the hash's per-player face the
    // persistable entry does not carry — the index alignment with
    // save.players[]).
    save.netPlayers.clear();
    for (std::size_t i = 0; i < m_players.size(); ++i) {
        const Player& p = *m_players[i];
        const PlayerSession& ps = *m_sessions[i];
        SavedNetPlayer npv;
        npv.prevHealth = ps.prevHealth;
        npv.hurtFlash = ps.hurtFlash;
        npv.lastSpacePressTime = ps.lastSpacePressTime;
        npv.pearlThrowCooldownFrames =
            ps.pearlThrowCooldownFrames;
        npv.drownTicks = ps.drown.drownTicks;
        npv.flying = p.isFlying();
        save.netPlayers.push_back(npv);
    }
    return serializeSave(save, m_items);
}

Result<void> Game::applyNetSnapshot() {
    // V3-12c2: the client's WELCOME-document's apply (the host's
    // collectNetSnapshot at the handshake — the parse/apply path's
    // full reuse over the worldgen'd world, the file-load's twin;
    // the seed's expected = the WELCOME's authority, the host's
    // world). The document's players[] entries 0..N-2 restore the
    // existing bodies (entry 0 = the primary's applySave path,
    // entries 1.. = the ghosts' applyGhostPlayer's loop); the
    // joining body (entry N) is the netAddPlayer's fresh
    // construction (the starter gift — the D25's new-body's
    // deviation: the old body's drops ride the document's entities,
    // the pickup's path).
    auto doc = Json::parse(m_netSnapshotPayload);
    MC_TRY(doc);
    auto save = parseSave(*doc, m_items, m_blocks, m_effects,
                          m_netSeedOverride);
    MC_TRY(save);
    MC_TRY(applySave(*save));
    // The session's counter family (applySave's blind spot — the
    // on-disk document never carries them, the solo's fresh-
    // derivation's single-machine premise; the rejoin's two-machine
    // continuation).
    const SavedNetCounters& nc = save->netCounters;
    if (nc.weatherPresent) {
        m_weather.raining = nc.raining;
        m_weather.framesLeft = nc.weatherFramesLeft;
        m_weather.thunderLeft = nc.weatherThunderLeft;
        m_weatherStream = nc.weatherStream;
        m_weatherThunderStream = nc.weatherThunderStream;
    }
    if (nc.spawnCountersPresent) {
        m_mobs.spawnCounter = nc.spawnCounterOverworld;
        m_nether.mobs.spawnCounter = nc.spawnCounterNether;
        m_end.mobs.spawnCounter = nc.spawnCounterEnd;
    }
    if (nc.particleBreakCounterPresent) {
        m_particleBreakCounter = nc.particleBreakCounter;
    }
    for (const SavedParticle& sp : save->particles) {
        Particle pt;
        pt.position = sp.position;
        pt.velocity = sp.velocity;
        pt.age = sp.age;
        pt.life = sp.life;
        pt.block = m_blocks.idForName(sp.block);
        pt.skyLight = sp.skyLight;
        pt.blockLight = sp.blockLight;
        pt.gravity = sp.gravity;
        m_particles.addRestored(pt);
    }
    // The per-player session state (the hash's per-player face — the
    // apply's fresh-zero override: the host's live timers at the
    // handshake; the index alignment with the restored bodies).
    if (save->netPlayers.size() == m_players.size()) {
        for (std::size_t i = 0; i < save->netPlayers.size(); ++i) {
            const SavedNetPlayer& npv = save->netPlayers[i];
            Player& p = *m_players[i];
            PlayerSession& ps = *m_sessions[i];
            ps.prevHealth = npv.prevHealth;
            ps.hurtFlash = npv.hurtFlash;
            ps.lastSpacePressTime = npv.lastSpacePressTime;
            ps.pearlThrowCooldownFrames = npv.pearlThrowCooldownFrames;
            ps.drown.drownTicks = npv.drownTicks;
            p.setFlying(npv.flying);
        }
    }
    // The frame's adoption (the pacing's alignment — the host's
    // m_frameCount at the handshake; the day cycle + the weather's
    // time source is the document's already).
    m_frameCount = m_netStartFrame;
    MC_LOG_INFO("net: the rejoin snapshot applied (the host's frame " +
                std::to_string(m_netStartFrame) + ")");
    return {};
}

Result<void> Game::applyGhostPlayer(const SavePlayer& sp) {
    // V3-12c2 (format 33): one ghost body's restore (the save's
    // players[] entry i >= 1 — the primary's 3)/4a-4h sequence's
    // per-player twin, the session's fresh construction). The ghost
    // lives in the PRIMARY's dimension (the collectSave's D9 ruling —
    // every entry carries the primary's word), so the dimension's
    // world is already ensured by the primary's branch: the rebind is
    // the setWorld + the catch-up disk around the ghost's own column
    // (its neighborhood may be far from the primary's disk).
    auto params = Player::Params::fromConfig(m_config);
    MC_TRY(params);
    m_sessions.push_back(
        std::make_unique<PlayerSession>(m_items, m_recipes));
    PlayerSession& ps = *m_sessions.back();
    auto p = std::make_unique<Player>(m_world, m_blocks, sp.pos, *params);
    Player& raw = *p;
    if (sp.dimension == "nether") {
        raw.setWorld(*m_nether.world);
        const int cx = floorDiv(
            static_cast<int>(std::floor(sp.pos.x)), kChunkSize);
        const int cz = floorDiv(
            static_cast<int>(std::floor(sp.pos.z)), kChunkSize);
        catchUpDisk(Dimension::Nether, cx, cz);
    } else if (sp.dimension == "end") {
        raw.setWorld(*m_end.world);
        const int cx = floorDiv(
            static_cast<int>(std::floor(sp.pos.x)), kChunkSize);
        const int cz = floorDiv(
            static_cast<int>(std::floor(sp.pos.z)), kChunkSize);
        catchUpDisk(Dimension::End, cx, cz);
    }
    raw.restorePosition(sp.pos);
    raw.setWorldSeed(static_cast<std::uint64_t>(
        m_config.getInt64("world.seed", 0)));
    raw.setVelocity(sp.velocity);
    raw.yaw = sp.yaw;
    raw.pitch = sp.pitch;
    raw.setHunger(sp.hunger < 0.0f ? raw.maxHunger() : sp.hunger);
    raw.setSaturation(sp.saturation < 0.0f ? 0.0f : sp.saturation);
    // The inventory (the 4) twin).
    for (std::size_t i = 0; i < ps.inventory.size(); ++i) {
        if (i < sp.slots.size()) {
            ps.inventory.setSlot(i, sp.slots[i]);
        }
    }
    ps.selectedSlot =
        static_cast<std::size_t>(sp.selectedSlot);
    // The panel grid (the 4b's twin).
    ps.panel.inventoryGrid.clear();
    for (int i = 0; i < CraftingGrid::kSize; ++i) {
        if (i < static_cast<int>(sp.invGrid.size())) {
            ps.panel.inventoryGrid.setSlot(
                static_cast<std::size_t>(i), sp.invGrid[static_cast<std::size_t>(i)]);
        }
    }
    // The effects (the 4d's twin) + the stats.
    ps.effects.clearAll();
    for (const SavedEffect& se : sp.effects) {
        ps.effects.apply(m_effects.idForName(se.name), se.remaining,
                         se.tier);
    }
    ps.stats = sp.stats;
    // The health + absorption (the 4e's twin — the two pools'
    // sentinel / range disciplines verbatim).
    const float baseMax = raw.maxHealth();
    const float effectsTotal = ps.effects.totalAbsorption(m_effects);
    if (sp.absorption < 0.0f) {
        const float h = sp.health < 0.0f ? baseMax : sp.health;
        if (h > baseMax + effectsTotal + 1e-4f) {
            return MC_FAIL("save players' 'health' is above the base max "
                           "plus the effects' absorption total (a "
                           "hand-edited shared health)");
        }
        raw.setAbsorption(std::max(0.0f, h - baseMax));
        raw.setHealth(std::min(h, baseMax));
    } else {
        if (sp.health >= 0.0f && sp.health > baseMax + 1e-4f) {
            return MC_FAIL("save players' 'health' is above the base max "
                           "(the absorption is a separate pool)");
        }
        raw.setAbsorption(sp.absorption);
        raw.setHealth(sp.health < 0.0f ? baseMax : sp.health);
        ps.prevHealth = raw.health(); // no hurt event from the load
                                      // itself (the primary's twin)
    }
    // The armor (the 4f's twin).
    for (int slot = 0; slot < kArmorSlotCount; ++slot) {
        const std::string& name = sp.armor[static_cast<std::size_t>(slot)];
        if (name.empty()) {
            raw.setArmorSlot(slot, ItemStack {}, 0);
            continue;
        }
        const Item& item = m_items.at(m_items.idForName(name));
        const float dmg =
            sp.armorDamage[static_cast<std::size_t>(slot)];
        ItemStack piece {item.id, 1, dmg};
        piece.enchants =
            sp.armorEnchants[static_cast<std::size_t>(slot)];
        raw.setArmorSlot(slot, piece, item.armorPoints,
                         item.durability, item.fireResistant);
    }
    // The oxygen (the 4g's twin).
    ps.drown.oxygen = sp.oxygen < 0 ? kOxygenMax : sp.oxygen;
    ps.drown.drownTicks = 0;
    // The XP pool / seed / respawn (the 4g-bis/ter/quater's twins).
    raw.setXpTotal(sp.xp);
    raw.setXpSeed(sp.xpSeed);
    if (sp.respawn.y >= 0.0f) {
        raw.setSpawnPos(sp.respawn);
    } else {
        raw.setSpawnPos(m_mobs.spawnPoint);
    }
    // The creative flag (the format-31's twin).
    raw.setCreative(sp.creative);
    if (sp.creative) {
        raw.setFlying(true);
    }
    // The tutorial (the 4h's twin — the pure restore's cross-checks;
    // no toast, the live-session presentation's discipline).
    bool tutReset = false;
    auto tutState = tutorialRestore(sp.tutorial, sp.tutorialSeries,
                                    sp.tutorialId, sp.tutorialProgress,
                                    m_tutorialSeries, &tutReset);
    MC_TRY(tutState);
    ps.tutorial = *tutState;
    m_players.push_back(std::move(p));
    MC_LOG_INFO("Save ghost player restored at (" +
                std::to_string(sp.pos.x) + ", " + std::to_string(sp.pos.y) +
                ", " + std::to_string(sp.pos.z) + ") in " + sp.dimension);
    return {};
}

Result<void> Game::tryLoadSave() {
    // V3-16a: the demo's SCENE spec (the committed guide asset's load —
    // the slot's twin face, the read-only by construction: an asset,
    // never the exit write's path).
    if (!m_demoSceneName.empty()) {
        const std::filesystem::path path = demoScenePath(m_demoSceneName);
        const auto started = std::chrono::steady_clock::now();
        auto save = readSaveFile(path);
        MC_TRY(save);
        MC_TRY(applySave(*save));
        const double loadMs =
            std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - started)
                .count();
        char msBuf[32];
        std::snprintf(msBuf, sizeof(msBuf), "%.1f", loadMs);
        MC_LOG_INFO("Loaded demo scene '" + path.string() + "' (" +
                    std::to_string(save->blockEdits.size()) +
                    " block edits, " +
                    std::to_string(save->fluidEdits.size()) +
                    " fluid edits, " +
                    std::to_string(save->entities.size()) +
                    " entities) in " + msBuf + " ms");
        return {};
    }
    if (!m_args.saveSlot.has_value() || m_args.newWorld) {
        return {};
    }
    const std::filesystem::path path = savePath();
    if (!std::filesystem::exists(path)) {
        MC_LOG_INFO("No save at '" + path.string() + "' — starting fresh");
        return {};
    }
    // V3-7: the load log line's encoding marker — the file's on-disk
    // encoding probed by the same magic the reader uses (1f 8b = the
    // deterministic gzip container; anything else = the legacy
    // plaintext, the zero-migration read path). Real-dt performance
    // evidence, never part of a byte gate.
    std::string encoding = "plain";
    {
        std::string head(2, '\0');
        {
            std::ifstream probeIn(path, std::ios::binary);
            probeIn.read(&head[0], 2);
        }
        if (looksLikeGzip(head)) {
            std::error_code ec;
            const std::uintmax_t bytes = std::filesystem::file_size(path, ec);
            encoding =
                ec ? "gzip" : "gzip: " + std::to_string(bytes) + " B on disk";
        }
    }
    const auto started = std::chrono::steady_clock::now();
    auto save = readSaveFile(path);
    MC_TRY(save);
    const double loadMs =
        std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - started)
            .count();
    MC_TRY(applySave(*save));
    char msBuf[32];
    std::snprintf(msBuf, sizeof(msBuf), "%.1f", loadMs);
    MC_LOG_INFO("Loaded save '" + path.string() + "' (" +
                std::to_string(save->blockEdits.size()) +
                " block edits, " + std::to_string(save->fluidEdits.size()) +
                " fluid edits, " +
                std::to_string(save->entities.size() +
                               save->xpEntities.size()) +
                " entities) in " + msBuf + " ms (" + encoding + ")");
    return {};
}

} // namespace mc
