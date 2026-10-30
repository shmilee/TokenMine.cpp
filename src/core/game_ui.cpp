#include "core/game.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "core/game_local.hpp"
#include "core/ui_layout.hpp"
#include "ui/item_tooltip.hpp"
#include "ui/ui_scale.hpp"

namespace mc {

namespace {

// CRAFT-7: the double-click collect window (real dt, the survival-timer
// class — input-only state, a no-input run never touches it). 0.3 s is
// the Minecraft-style "fast double tap" band: wide enough for xdotool's
// ~50 ms click jitter (the E2E protocol), narrow enough that a deliberate
// pick-and-place (>= a glance) never collects by accident.
constexpr double kDoubleClickSeconds = 0.3;

// Phase 15 UI (screen-space, pixels; the font is 16x16 at scale 1,
// Phase 35).
constexpr int kHotbarSlotSize = 44;

constexpr int kHotbarSlotGap = 2;

constexpr int kHotbarBottomMargin = 8;

constexpr int kHotbarIconPx = 30;

constexpr int kHeldIconPx = 76;

constexpr int kHeldIconOffset = 72; // center of the held icon from the corner

constexpr int kF3Margin = 8;

constexpr int kF3LineHeight = 18; // 16 px glyph (Phase 35) + 2 px gap

// The icon cubes are drawn at a fixed tilt (deterministic; no bob).
constexpr float kIconTiltYaw = 0.65f;

constexpr float kIconTiltPitch = -0.35f;

constexpr double kStepSoundInterval = 0.4;

constexpr float kStepMinSpeed = 0.5f; // blocks/s horizontal to count as walking

// Phase 16 survival: the red damage veil's lifetime (it fades linearly to
// 0 over this window after a damage event).
constexpr double kHurtFlashSeconds = 0.5;

} // namespace

void Game::updateEffects(double dt) {
    // Phase 84: push the effect-derived move-speed + damage-reduction onto
    // the player every frame (the player's movement + damage read them; the
    // identity 1.0 / 0.0 when no such effect is active). Runs even for a
    // dead player so the values reset cleanly on respawn.
    m_player->setSpeedMultiplier(
        m_playerEffects.totalSpeedMultiplier(m_effects));
    m_player->setDamageReduction(
        m_playerEffects.totalDamageReduction(m_effects));
    if (m_player->isDead()) {
        return;
    }
    // Phase 72: the absorption pool is synced only when the effects'
    // total ACTUALLY changes this frame (an expiry, a water extinguish).
    // A per-frame SET would fight the damage drain: the pool must persist
    // between sync points (the Phase 56 bonus sync was idempotent, the
    // pool is not — damage lowers it below the total).
    const float absorptionBefore = m_playerEffects.totalAbsorption(m_effects);
    // Phase 57: the fire system (the data-flagged effect, resolved at
    // init). Runs EVEN with no active effects — the ignition is a source:
    // while the player is in the lava OR overlapping a fire block
    // (Phase 74: the fire block ignites the player the way the lava
    // does — the P70 direct contact damage is gone; both sources share
    // this one gate) the fire is (re-)applied EVERY frame (the duration
    // resets, the tick phase is kept — standing in the fire source burns
    // continuously, Minecraft); after leaving, the effect counts down
    // fireDurationSeconds and burns until it expires. Water
    // EXTINGUISHES instantly (and beats both: in water, no fire).
    if (m_fireEffectId != -1) {
        if (m_player->inLava() || m_player->isInFire()) {
            m_playerEffects.apply(
                m_fireEffectId,
                m_player->params().fireDurationSeconds);
        }
        if (m_player->inWater()) {
            m_playerEffects.clearExtinguishable(m_effects);
        }
    }
    if (m_playerEffects.count() == 0) {
        return;
    }
    // The ticks are REAL dt (the survival-timer class, like hunger and
    // the eat channel). The HP changes apply against the player HERE
    // (not in StatusEffects): the damage tick respects the effect's
    // min_health floor (poison keeps the player at >= 1 HP and never
    // kills — a plain damage effect with floor 0 may), the heal tick
    // clamps at max health. A poison tick that drops health shows the
    // red hurt veil like any other damage (updateSurvival polls the
    // health each frame; the v1 has no separate poison tint).
    m_playerEffects.update(
        dt, m_effects, [this](const Effect& def) {
            if (def.tick == Effect::Tick::Damage) {
                if (m_player->health() - def.tickAmount >= def.minHealth) {
                    m_player->damage(def.tickAmount);
                }
            } else if (def.tick == Effect::Tick::Heal) {
                m_player->heal(def.tickAmount);
            }
        });
    // Phase 72: an expired (or extinguished) absorption layer drops the
    // pool to the remaining total (the health is untouched — the pool is
    // not HP). Unchanged totals leave the pool alone (the drain since
    // the last sync point persists).
    if (m_playerEffects.totalAbsorption(m_effects) != absorptionBefore) {
        syncAbsorption();
    }
}

void Game::syncAbsorption() {
    m_player->setAbsorption(m_playerEffects.totalAbsorption(m_effects));
}

void Game::applyPlayerEffect(const std::string& name, double duration,
                             int tier) {
    // SURV-4 (potions): the single choke point for applying a NAMED effect
    // to the player. Every grant source (a food's grants_effect(s), a
    // drinkable's, a mob's on_hit_effect) is init-cross-checked to name an
    // EXISTING effect, so idForName cannot fail here (the dead check in the
    // call sites guards both). The apply uses the RESET semantics (an
    // already-active instance is re-timed to `duration`, the tick phase
    // kept — drinking the same potion twice resets, it does not stack).
    // SURV-6: the grant's `tier` rides along (a re-apply at a DIFFERENT
    // tier replaces the instance's tier — no amplifier stacking; tier 0
    // is the pre-SURV-6 value, bit-identical through this choke point).
    // Then settle any INSTANT action the effect data carries: a positive
    // instant_hp heals (clamped at the base max — the healing potion), a
    // negative one deals damage with the armor EXEMPTED (the harming
    // potion — Minecraft's magic/potion damage, the SURV-2 drowning
    // bypassArmor twin; the resistance effect still applies, as in the
    // damage pipeline). Effects without an instant action settle nothing
    // (the pre-SURV-4 apply sites are bit-identical through the helper).
    // The caller owns the surrounding syncAbsorption() (the pool is a pure
    // function of the effects at every sync point).
    const int id = m_effects.idForName(name);
    if (id == -1) {
        return; // init guarantees existence; this is a defensive no-op.
    }
    m_playerEffects.apply(id, duration, tier);
    const Effect& def = m_effects.at(id);
    if (def.instantHp > 0.0f) {
        m_player->heal(def.instantHp);
    } else if (def.instantHp < 0.0f) {
        m_player->damage(-def.instantHp, true);
    }
}

void Game::handleUiKeys() {
    if (m_input.wasPressed(GameKey::F3)) {
        m_debugOverlay = !m_debugOverlay;
    }
    // CRAFT-7: the top-row digits select the hotbar slot directly
    // (Minecraft; the Tab cycle stays). In the GLOBAL key section
    // (handleUiKeys, not the locked-world section): it must work with a
    // panel open, as in Minecraft — the world section early-returns
    // while the pointer is free (panel open).
    for (int d = 0; d < 9; ++d) {
        if (m_input.wasPressed(static_cast<GameKey>(
                static_cast<int>(GameKey::Digit1) + d))) {
            m_selectedSlot = static_cast<std::size_t>(d);
        }
    }
    if (m_input.wasPressed(GameKey::F5)) {
        if (m_args.saveSlot.has_value()) {
            auto saved = writeSave();
            if (!saved.ok()) {
                MC_LOG_ERROR("F5 save failed: " + saved.error().describe());
            }
        } else {
            MC_LOG_WARN("F5: no save slot (pass --save <slot>)");
        }
    }
    if (m_input.wasPressed(GameKey::E)) {
        if (panelOpen()) {
            closePanel();
        } else if (m_window.relativeMouseMode()) {
            openPanel(OpenPanel::Inventory);
        }
    }
    if (panelOpen() && m_input.wasPressed(GameKey::Escape)) {
        closePanel();
    }
}

void Game::openPanel(OpenPanel panel) {
    m_window.setRelativeMouseMode(false); // free the cursor for clicking
    m_panel.openPanel = panel;
    resetBreaking();
    resetBowDraw(); // a panel interrupts a draw (Phase 24)
    resetEatChannel(); // a panel interrupts an eat too (Phase 28)
    m_sound.play("ui_open");
}

void Game::closePanel() {
    m_panel.openPanel = OpenPanel::None;
    m_sound.play("ui_close");
}

// Phase 44: the furnace panel opens for ONE block — bind the UI to that
// block's state (the setBlock invariant guarantees a furnace block owns
// one; furnaceAt's default-construct is defensive). The state is stable
// while the panel is open: block interaction is paused and no other code
// path mutates or removes a furnace block in the meantime.
void Game::openFurnacePanel(const BlockPos& pos) {
    m_panel.openFurnacePos = pos;
    m_panel.furnaceUi.bind(furnaceAt(pos));
    openPanel(OpenPanel::Furnace);
}

FurnaceState& Game::furnaceAt(const BlockPos& pos) {
    auto it = m_states.furnaces.find(pos);
    if (it == m_states.furnaces.end()) {
        it = m_states.furnaces.emplace(pos, FurnaceState {}).first;
    }
    return it->second;
}

// CRAFT-1: the table panel opens for ONE block — bind the UI to that
// block's grid (the setBlock invariant guarantees a table block owns
// one; tableAt's default-construct is defensive). The grid is stable
// while the panel is open: block interaction is paused and no other code
// path mutates or removes a table block in the meantime (the Phase 44
// furnace argument, the twin).
void Game::openTablePanel(const BlockPos& pos) {
    m_panel.tableUi.bind(tableAt(pos));
    openPanel(OpenPanel::Table);
}

CraftingGrid& Game::tableAt(const BlockPos& pos) {
    auto it = m_states.tables.find(pos);
    if (it == m_states.tables.end()) {
        it = m_states.tables.emplace(pos, CraftingGrid {}).first;
    }
    return it->second;
}

CampfireCookState& Game::campfireAt(const BlockPos& pos) {
    auto it = m_states.campfires.find(pos);
    if (it == m_states.campfires.end()) {
        it = m_states.campfires.emplace(pos, CampfireCookState {}).first;
    }
    return it->second;
}

void Game::tryCampfireCook(const BlockPos& pos) {
    // The click is ALWAYS consumed (the caller returns) — a campfire is
    // never placed through this branch, whatever the hand holds.
    CampfireCookState& st = campfireAt(pos);
    const ItemStack& held = m_inventory.at(m_selectedSlot);
    // Phase 76: the relight — an ignites_flamable item (the flint and
    // steel) on a campfire-family block with an `ignites_to` target sets
    // the target in place (unlit -> lit: the cooking state survives the
    // setBlock choke point and resumes; light 0 -> 15, local re-light).
    // It runs HERE because this branch consumes the click BEFORE the
    // general igniter branch; the item is NOT consumed (the P60
    // semantics) and the "ignite" sound plays. An igniter on a LIT
    // campfire (no ignites_to) falls into the place/take logic below and
    // no-ops (it is not smeltable).
    if (!held.empty() && m_items.at(held.item).ignitesFlamable) {
        const Block& b = m_blocks.at(
            m_world.getBlock(pos.x, pos.y, pos.z));
        if (!b.properties.ignitesTo.empty()) {
            // The name is init-validated (the grows_to discipline), so
            // idForName cannot fail.
            const BlockID toId = m_blocks.idForName(b.properties.ignitesTo);
            setBlock(pos.x, pos.y, pos.z, toId);
            Lighting::updateLocal(m_world, m_blocks, pos);
            m_sound.play("ignite");
        }
        return;
    }
    // Phase 75: only a LIT campfire cooks — placing food on the unlit
    // logs does nothing (the fire must be there, Minecraft); the
    // take-back works on EITHER (the items are in the unlit campfire
    // too). There is no panel (v1): the slot choice is by index — first
    // empty (place) / first occupied (take), deterministic.
    const bool lit =
        m_world.getBlock(pos.x, pos.y, pos.z) == m_states.campfireLitId;
    if (!held.empty()) {
        const Item& it = m_items.at(held.item);
        if (lit && !it.smeltableTo.empty()) {
            for (std::size_t i = 0; i < kCampfireSlotCount; ++i) {
                if (st.slots[i].empty()) {
                    // Place one unit in the slot (Minecraft: a smeltable
                    // item on an empty campfire spot). One is consumed
                    // from the hand; the cooking starts at the next tick.
                    st.slots[i] = ItemStack {held.item, 1};
                    st.progressSubsteps[i] = 0;
                    m_inventory.take(m_selectedSlot, 1);
                    m_sound.play("place");
                    break;
                }
            }
            // All four slots full (or a non-smeltable / unlit target):
            // nothing happens (documented v1 simplification — no panel).
        }
        // The take needs a FREE hand (Minecraft).
        return;
    }
    // Empty hand: take the first occupied slot back (smelted or not) —
    // the selected slot is empty, so the write is a direct set.
    for (std::size_t i = 0; i < kCampfireSlotCount; ++i) {
        if (!st.slots[i].empty()) {
            m_inventory.setSlot(m_selectedSlot, st.slots[i]);
            st.slots[i] = ItemStack {};
            st.progressSubsteps[i] = 0;
            m_sound.play("pickup");
            break;
        }
    }
}

// The shapeless recipes in registry order: the E panel's craft list
// (shaped recipes are crafted in the grids, not from the list).
std::vector<const Recipe*> Game::shapelessRecipes() const {
    std::vector<const Recipe*> out;
    for (const Recipe& r : m_recipes.all()) {
        if (r.type == RecipeType::Shapeless) {
            out.push_back(&r);
        }
    }
    return out;
}

// CRAFT-4: the shaped recipes in registry order: the table panel's hint
// list (display-only — the player crafts them in the 3x3 grid following
// the mini pattern).
std::vector<const Recipe*> Game::shapedRecipes() const {
    std::vector<const Recipe*> out;
    for (const Recipe& r : m_recipes.all()) {
        if (r.type == RecipeType::Shaped) {
            out.push_back(&r);
        }
    }
    return out;
}

void Game::handlePanelClicks() {
    // The layout and the mouse hit tests both live in UI pixels (Phase
    // 34): InventoryLayout / FurnaceLayout are pure geometry, so passing
    // the UI space scales the panel identically to the rendered one.
    const UiSize space = uiSize();
    const int w = space.width;
    const int h = space.height;

    // CRAFT-7: the interaction dispatch — shift-click quick-moves, a
    // fast second left click on the same player-inventory slot collects.
    // The double-click window is REAL dt (the survival-timer class, hard
    // rule 8: input-only state — a no-input run never touches it), and
    // any non-slot click / other button / shift resets the pairing.
    const bool shift = m_input.isDown(GameKey::LeftShift);
    const bool leftPressed = m_input.wasPressed(MouseButton::Left);
    const bool rightPressed = m_input.wasPressed(MouseButton::Right);
    const double now = nowSeconds();
    auto isDoubleClick = [&](int slot) {
        return !shift && slot >= 0 && slot == m_panel.lastLeftSlot &&
               m_panel.lastLeftClickTime >= 0.0 &&
               (now - m_panel.lastLeftClickTime) <= kDoubleClickSeconds;
    };
    auto armDoubleClick = [&](int slot) {
        m_panel.lastLeftSlot = slot;
        m_panel.lastLeftClickTime = slot >= 0 ? now : -1.0;
    };
    auto disarmDoubleClick = [&]() {
        m_panel.lastLeftSlot = -1;
        m_panel.lastLeftClickTime = -1.0;
    };

    // Phase 40: the furnace panel. Same click model as the crafting
    // panels: the layout hit tests are disjoint (at most one of a
    // 27-slot / input / fuel / output is active; a left click with all of
    // them inactive drops the carry). No craft action — the output fills
    // by itself.
    if (m_panel.openPanel == OpenPanel::Furnace) {
        const FurnaceLayout layout = FurnaceLayout::compute(w, h);
        const std::pair<int, int> mp = uiMouse();
        const int slot = layout.slotAt(mp.first, mp.second);
        const int fslot = layout.furnaceSlotAt(mp.first, mp.second);
        if (leftPressed) {
            if (shift) {
                m_panel.furnaceUi.quickMove(slot, fslot);
            } else if (fslot < 0 && isDoubleClick(slot)) {
                m_panel.furnaceUi.doubleCollect(slot);
            } else {
                m_panel.furnaceUi.leftClick(slot, fslot);
            }
            if (fslot < 0 && !shift) {
                armDoubleClick(slot);
            } else {
                disarmDoubleClick();
            }
            return;
        }
        if (rightPressed) {
            if (shift) {
                m_panel.furnaceUi.quickMove(slot, fslot);
            } else {
                m_panel.furnaceUi.rightClick(slot, fslot);
            }
            disarmDoubleClick();
        }
        return;
    }

    const bool table = m_panel.openPanel == OpenPanel::Table;
    const PanelKind kind =
        table ? PanelKind::CraftingTable : PanelKind::Inventory;
    // The hint list per kind: shapeless craft rows (E panel) or shaped
    // display-only hint rows (table panel, CRAFT-4).
    std::vector<const Recipe*> hints =
        table ? shapedRecipes() : shapelessRecipes();
    const InventoryLayout layout =
        InventoryLayout::compute(w, h, static_cast<int>(hints.size()), kind);
    const std::pair<int, int> mp = uiMouse();
    const int px = mp.first;
    const int py = mp.second;
    InventoryUI& ui = table ? m_panel.tableUi : m_panel.inventoryUi;
    // The layout hit tests are disjoint: at most one of slot / grid cell /
    // output is active (the UI dispatch checks output first, then grid,
    // then slot; all-inactive drops the carry on a left click).
    const int cell = layout.gridCellAt(px, py);
    const int gridSlot =
        cell >= 0 ? layout.gridSlots[static_cast<std::size_t>(cell)] : -1;
    const bool onOutput = layout.inOutput(px, py);
    const int slot = layout.slotAt(px, py);
    if (leftPressed) {
        // Craft rows first (they sit beside the slots, so the hit test is
        // unambiguous): an E-panel row crafts when the inventory can
        // satisfy the recipe (all-or-nothing, see craft()). A table-panel
        // hint row (CRAFT-4) is display-only: the click is CONSUMED (no
        // craft, and no carry drop like a click outside every slot).
        const int row = layout.recipeRowAt(px, py);
        if (row >= 0) {
            if (table) {
                disarmDoubleClick();
                return;
            }
            const Recipe& recipe = *hints[static_cast<std::size_t>(row)];
            if (canCraft(m_inventory, recipe, m_items)) {
                auto crafted = craft(m_inventory, recipe, m_items);
                if (crafted.ok()) {
                    m_sound.play("craft");
                } else {
                    MC_LOG_ERROR("Craft failed: " + crafted.error().describe());
                }
            }
            disarmDoubleClick();
            return;
        }
        if (shift) {
            ui.quickMove(slot, gridSlot, onOutput);
        } else if (gridSlot < 0 && !onOutput && isDoubleClick(slot)) {
            ui.doubleCollect(slot);
        } else {
            if (ui.leftClick(slot, gridSlot, onOutput)) {
                m_sound.play("craft");
            }
        }
        if (gridSlot < 0 && !onOutput && !shift) {
            armDoubleClick(slot);
        } else {
            disarmDoubleClick();
        }
        return;
    }
    if (rightPressed) {
        if (shift) {
            ui.quickMove(slot, gridSlot, onOutput);
        } else {
            if (ui.rightClick(slot, gridSlot, onOutput)) {
                m_sound.play("craft");
            }
        }
        disarmDoubleClick();
    }
}

void Game::updateSoundTimers(double dt) {
    if (!m_player || !m_window.relativeMouseMode() || panelOpen()) {
        return;
    }
    const Vec3 v = m_player->velocity();
    const bool moving =
        std::abs(v.x) > kStepMinSpeed || std::abs(v.z) > kStepMinSpeed;
    if (moving && m_player->onGround()) {
        m_interaction.stepTimer += dt;
        if (m_interaction.stepTimer >= kStepSoundInterval) {
            m_interaction.stepTimer = 0.0;
            m_sound.play("step");
        }
    } else {
        m_interaction.stepTimer = 0.0;
    }
}

void Game::updateAutoSave(double dt) {
    // SAVE-1: the real-dt auto-save timer (hard rule 8 — the accumulator
    // is NOT frame state: never saved, never touches the deterministic
    // frame; it is the audio-timer class, the survival-timer rhythm).
    // The slot gate is structural: without --save the timer never arms.
    if (!m_args.saveSlot.has_value()) {
        return;
    }
    if (!autoSaveStep(m_autoSaveAccum, dt, m_autoSaveInterval)) {
        return;
    }
    auto saved = writeSave(true);
    if (saved.ok()) {
        m_autoSaveAccum = 0.0;
    } else {
        // A failed write keeps the accumulator armed (>= the interval):
        // the NEXT frame retries (no backoff — the documented
        // simplification; the save path's error is the signal).
        MC_LOG_ERROR("Auto-save failed: " + saved.error().describe());
    }
}

void Game::updateSurvival(double dt) {
    if (m_hurtFlash > 0.0) {
        m_hurtFlash = std::max(0.0, m_hurtFlash - dt);
    }
    const float hp = m_player->health();
    const bool dead = m_player->isDead();
    if (m_prevHealth >= 0.0f && hp < m_prevHealth) {
        // A health drop this frame: the red veil restarts and the hurt
        // sound plays (suppressed when the drop was lethal — the death
        // sound follows instead).
        m_hurtFlash = kHurtFlashSeconds;
        if (!dead) {
            m_sound.play("hurt");
        }
    }
    if (dead && m_prevHealth >= 0.0f && m_prevHealth > 0.0f) {
        // Alive -> dead transition: the death screen owns the input, so
        // release the pointer (it was locked while the fall happened).
        // Phase 46: the active status effects die with the player (the
        // respawn starts clean — Minecraft parity).
        m_window.setRelativeMouseMode(false);
        m_sound.play("death");
        // SURV-7: the death counter (the alive->dead transition — one
        // increment per life lost, whatever the cause; frame-locked).
        m_stats["deathCount"]++;
        m_playerEffects.clearAll();
        syncAbsorption(); // Phase 72: the pool dies with the player
        // SURV-2: the air refills with the death (the respawn
        // convergence point — the next life starts on the dry spawn
        // with full oxygen; while dead the pass is inert and the
        // state's decay is harmless).
        m_drown = DrownState {};
    }
    m_prevHealth = hp;
    if (dead && m_input.wasPressed(GameKey::R)) {
        m_player->respawn();
        m_prevHealth = m_player->health(); // no hurt flash on respawn
    }
}

Game::UiSize Game::uiSize() const {
    const double scale = static_cast<double>(m_uiScale);
    return UiSize {
        std::max(1, static_cast<int>(std::lround(
            static_cast<double>(m_window.logicalWidth()) / scale))),
        std::max(1, static_cast<int>(std::lround(
            static_cast<double>(m_window.logicalHeight()) / scale)))};
}

std::pair<int, int> Game::uiMouse() const {
    const Vec2 p = m_input.mousePosition();
    const double scale = static_cast<double>(m_uiScale);
    return {static_cast<int>(std::lround(p.x / scale)),
            static_cast<int>(std::lround(p.y / scale))};
}

Mat4 Game::uiProjection(int w, int h) const {
    // UI pixels (y = 0 is the window top), stretched onto the drawable by
    // the ortho range [0, w] x [0, h]. The depth range
    // must cover the 3D block-cube icons (the 76px held icon spans
    // |z| up to ~66px after its tilt); everything 2D sits at z = 0 and
    // the UI pass runs with the depth test off, so the wide range is
    // harmless.
    return Mat4::ortho(0.0f, static_cast<float>(w), static_cast<float>(h),
                       0.0f, -128.0f, 128.0f);
}

void Game::drawUiBatch(const std::vector<MeshVertex>& vertices,
                       const Mat4& projection, const Texture& texture,
                       bool textured, const Vec4& color) {
    if (vertices.empty()) {
        return;
    }
    Mesh mesh;
    auto up = mesh.upload(vertices);
    if (!up.ok()) {
        MC_LOG_ERROR("UI mesh upload failed: " + up.error().describe());
        return;
    }
    m_uiShader.bind();
    m_uiShader.setUniform1i("u_texture", 0);
    m_uiShader.setUniform1i("u_useTexture", textured ? 1 : 0);
    m_uiShader.setUniform4f("u_color", color.x, color.y, color.z, color.w);
    if (textured) {
        texture.bind();
    }
    m_renderer.draw(mesh, m_uiShader, Mat4::identity(), projection,
                    Mat4::identity());
}

void Game::renderUi() {
    // All UI geometry lives in the UI-pixel space (logical window size /
    // ui.scale, Phase 34): uiProjection stretches it onto the drawable,
    // so a larger ui.scale upsamples every element (font, hotbar, panels,
    // F3, death screen) via the GPU. At dppx 1 with ui.scale 1.0 the
    // space is exactly the drawable — the no-input baseline is unchanged.
    const UiSize space = uiSize();
    const int w = space.width;
    const int h = space.height;
    const bool locked = m_window.relativeMouseMode();
    const Mat4 ortho = uiProjection(w, h);

    // Palette (Minecraft-like: dark translucent panels, gray borders, the
    // selected slot in orange).
    const Vec4 kSlotBg {0.0f, 0.0f, 0.0f, 0.45f};
    const Vec4 kBorder {1.0f, 1.0f, 1.0f, 0.30f};
    const Vec4 kSelected {1.0f, 0.82f, 0.0f, 1.0f};
    const Vec4 kHover {1.0f, 1.0f, 1.0f, 0.85f};
    const Vec4 kPanel {0.08f, 0.08f, 0.11f, 0.88f};
    const Vec4 kText {1.0f, 1.0f, 1.0f, 0.92f};
    const Vec4 kTextDim {1.0f, 1.0f, 1.0f, 0.55f};
    const Vec4 kCrosshair {1.0f, 1.0f, 1.0f, 0.85f};
    // SURV-3 (durability bar): the strip drawn under a DAMAGED item
    // icon — 2 px tall at the icon's bottom edge, filled left-to-right
    // with the REMAINING fraction (1 − damage/durability) in three
    // tiers (green >= 1/2, yellow >= 1/4, red < 1/4 — the documented
    // simplification of Minecraft's continuous gradient). Damage 0
    // (full durability) draws NOTHING (the no-input baseline's
    // structural immunity: a fresh run carries full-durability items).
    constexpr int kDurabilityBarPx = 2;
    constexpr float kDurabilityGreenTier = 0.5f;
    constexpr float kDurabilityYellowTier = 0.25f;
    const Vec4 kDurabilityGreen {0.16f, 0.69f, 0.13f, 1.0f};
    const Vec4 kDurabilityYellow {0.93f, 0.78f, 0.12f, 1.0f};
    const Vec4 kDurabilityRed {0.78f, 0.12f, 0.12f, 1.0f};

    std::vector<UiBatch> batches;
    auto batchFor = [&](const Vec4& color, bool textured,
                        const Texture* tex) -> UiBatch& {
        for (auto& b : batches) {
            if (b.textured == textured && b.texture == tex &&
                b.color.x == color.x && b.color.y == color.y &&
                b.color.z == color.z && b.color.w == color.w) {
                return b;
            }
        }
        UiBatch b;
        b.color = color;
        b.textured = textured;
        b.texture = tex;
        batches.push_back(std::move(b));
        return batches.back();
    };
    // A screen-space quad with an explicit UV rect (image space, v-down:
    // v=0 is the texture's TOP row — textures upload without a flip).
    // Textured batches (item icons, hearts) sample their rect; untextured
    // batches ignore the UVs (u_useTexture=0).
    auto addQuadUv = [&batches](UiBatch& b, int x, int y, int qw, int qh,
                                 const Vec2& uvMin, const Vec2& uvMax) {
        const float x0 = static_cast<float>(x);
        const float y0 = static_cast<float>(y);
        const float x1 = x0 + static_cast<float>(qw);
        const float y1 = y0 + static_cast<float>(qh);
        const auto put = [&](float px, float py, float u, float pv) {
            MeshVertex vert;
            vert.position = {px, py, 0.0f};
            vert.uv = {u, pv};
            b.vertices.push_back(vert);
        };
        put(x0, y0, uvMin.x, uvMin.y);
        put(x1, y0, uvMax.x, uvMin.y);
        put(x1, y1, uvMax.x, uvMax.y);
        put(x0, y0, uvMin.x, uvMin.y);
        put(x1, y1, uvMax.x, uvMax.y);
        put(x0, y1, uvMin.x, uvMax.y);
    };
    auto addQuad = [&batches, &addQuadUv](UiBatch& b, int x, int y, int qw,
                                          int qh) {
        // Full-rect UVs: a single-tile textured quad (item icons) samples
        // the whole texture. (P15 bug fix: the old code left the UVs at
        // (0, 0) and the icon rendered as its top-left pixel.)
        addQuadUv(b, x, y, qw, qh, Vec2 {0.0f, 0.0f}, Vec2 {1.0f, 1.0f});
    };
    auto addBorder = [&](UiBatch& b, const UIRect& r, int thickness) {
        addQuad(b, r.x, r.y, r.w, thickness);
        addQuad(b, r.x, r.y + r.h - thickness, r.w, thickness);
        addQuad(b, r.x, r.y, thickness, r.h);
        addQuad(b, r.x + r.w - thickness, r.y, thickness, r.h);
    };
    auto addText = [&](const std::string& text, int x, int y, double scale,
                       const Vec4& color) {
        makeTextVertices(text, x, y, scale,
                         batchFor(color, true, &m_fontTexture).vertices);
    };

    // PRE-12 (tooltip): the hover tooltip lines are collected into
    // separate batches drawn AFTER the 2D batches and the 3D block-cube
    // icons. A cursor-adjacent tooltip sits over slot rects/icons; the
    // shared kText batch (inserted at the panel title, before the slot
    // backgrounds) would draw UNDER them and vanish at the panel's fit
    // scale (Phase 15's "only the name" tooltip was effectively hidden
    // for the same reason).
    std::vector<UiBatch> tooltipBatches;
    auto appendTooltip = [&](const ItemTooltip& tip, int x, int y,
                             double s, int lineGap) {
        UiBatch title;
        title.color = kText;
        title.textured = true;
        title.texture = &m_fontTexture;
        makeTextVertices(tip.title, x, y, s, title.vertices);
        tooltipBatches.push_back(std::move(title));
        if (!tip.durability.empty()) {
            UiBatch dur;
            dur.color = kTextDim;
            dur.textured = true;
            dur.texture = &m_fontTexture;
            makeTextVertices(tip.durability, x, y + lineGap, s, dur.vertices);
            tooltipBatches.push_back(std::move(dur));
        }
    };

    // Fluid overlay (Phase 99.5): when the camera's cell holds a fluid,
    // the fluid geometry around the eye self-culls (same-kind faces hide
    // each other — a level-8 cell in a deep pool has no faces at all), so
    // a fully submerged player saw "air" (the 99.x user report). A flat
    // translucent tint under the HUD — Minecraft's fluid overlay —
    // restores the submerged read. This is the FIRST batch (drawn first =
    // under every HUD element). No-op out of fluid (the no-input baseline
    // camera is in air: byte-identical).
    {
        const Vec3 eye = m_camera.position;
        const std::optional<FluidKind> submerged = m_world.getFluidKind(
            static_cast<int>(std::floor(eye.x)),
            static_cast<int>(std::floor(eye.y)),
            static_cast<int>(std::floor(eye.z)));
        if (submerged) {
            // Visual tuning constants (overlay strength + hue): water ≈
            // MC's blue depth tint, lava ≈ its orange glow.
            const Vec4 tint = *submerged == FluidKind::Water
                ? (Vec4 {0.20f, 0.42f, 0.85f, 0.45f})
                : (Vec4 {0.95f, 0.42f, 0.05f, 0.60f});
            auto& b = batchFor(tint, false, nullptr);
            addQuad(b, 0, 0, w, h);
        }
    }

    // Item icons: 2D textures (tools, materials) are batched quads like
    // everything else; block items render a lit 3D cube, which is
    // RECORDED and drawn only after all 2D batches — the slot backgrounds
    // are 2D batches, and drawing the cubes first would put them under
    // the backgrounds.
    struct UiIconDraw {
        ItemStack stack;
        float cx, cy;
        int sizePx;
    };
    std::vector<UiIconDraw> iconDraws;
    auto drawIcon = [&](const ItemStack& stack, float cx, float cy,
                        int sizePx) {
        if (stack.empty()) {
            return;
        }
        const Item& item = m_items.at(stack.item);
        if (item.type == ItemType::Block) {
            if (itemMesh(item.block).valid()) {
                iconDraws.push_back(UiIconDraw {stack, cx, cy, sizePx});
            }
            return;
        }
        const std::size_t idx = static_cast<std::size_t>(stack.item.value);
        if (idx >= m_itemIcons.size() || !m_itemIcons[idx].valid()) {
            return;
        }
        UiBatch& b = batchFor(Vec4 {1.0f, 1.0f, 1.0f, 1.0f}, true,
                              &m_itemIcons[idx]);
        addQuad(b, static_cast<int>(cx) - sizePx / 2,
                static_cast<int>(cy) - sizePx / 2, sizePx, sizePx);
        // SURV-3: the durability bar (the 2D-icon path only — block
        // items are not in the durability's allowed set, so the 3D-cube
        // path never carries one).
        if (stack.damage > 0.0f && item.durability > 0.0f) {
            const float remaining = 1.0f - stack.damage / item.durability;
            const Vec4 bar =
                remaining >= kDurabilityGreenTier
                    ? kDurabilityGreen
                    : (remaining >= kDurabilityYellowTier
                           ? kDurabilityYellow : kDurabilityRed);
            const int barW =
                static_cast<int>(std::lround(sizePx * remaining));
            if (barW > 0) {
                auto& bb = batchFor(bar, false, nullptr);
                addQuad(bb, static_cast<int>(cx) - sizePx / 2,
                        static_cast<int>(cy) + sizePx / 2 -
                            kDurabilityBarPx,
                        barW, kDurabilityBarPx);
            }
        }
    };

    const std::pair<int, int> mouse = uiMouse();
    const int mx = mouse.first;
    const int my = mouse.second;

    // -- hotbar + crosshair + held item (while captured, no panel open) --
    if (locked && !panelOpen()) {
        const int n = Inventory::kHotbarSize;
        const int total = n * kHotbarSlotSize + (n - 1) * kHotbarSlotGap;
        const int x0 = (w - total) / 2;
        const int y0 = h - kHotbarBottomMargin - kHotbarSlotSize;
        for (int i = 0; i < n; ++i) {
            const int sx = x0 + i * (kHotbarSlotSize + kHotbarSlotGap);
            const UIRect slot {sx, y0, kHotbarSlotSize, kHotbarSlotSize};
            addQuad(batchFor(kSlotBg, false, nullptr), slot.x, slot.y,
                    slot.w, slot.h);
            if (static_cast<std::size_t>(i) == m_selectedSlot) {
                addBorder(batchFor(kSelected, false, nullptr), slot, 2);
            } else {
                addBorder(batchFor(kBorder, false, nullptr), slot, 1);
            }
            const ItemStack& stack = m_inventory.at(i);
            drawIcon(stack, static_cast<float>(sx + kHotbarSlotSize / 2),
                     static_cast<float>(y0 + kHotbarSlotSize / 2),
                     kHotbarIconPx);
            if (!stack.empty() &&
                stack.count < m_items.at(stack.item).maxStack) {
                const std::string count = std::to_string(stack.count);
                addText(count,
                        sx + kHotbarSlotSize - 4 -
                            static_cast<int>(textWidth(count)),
                        y0 + kHotbarSlotSize - 4 - kGlyphSize, 1.0, kText);
            }
        }
        // Health bar (Phase 16) + hunger bar (Phase 23): hearts above the
        // hotbar, 2 points per icon, three-state tiles (full / half /
        // empty); the hunger strip (drumsticks, same tiles) sits on the
        // SAME row to the right of the hearts. Both strips share the icon
        // metrics; the icon count follows the configured max (the default
        // 20 = the 10-icon Phase 16/23 layout, bit-identical).
        {
            // The UI icon metrics come from the single source of truth
            // (core/ui_layout.hpp — the pure layout is unit-test-owned).
            constexpr int kIconPx = kUiIconPx;
            constexpr int kIconGap = kUiIconGap;
            constexpr int kStripGap = kUiStripGap;
            const float baseMaxHealth = m_player->params().maxHealth;
            const int heartIcons =
                static_cast<int>(std::ceil(baseMaxHealth / 2.0f));
            const int hungerIcons =
                static_cast<int>(std::ceil(m_player->maxHunger() / 2.0f));
            const int oxygen = m_drown.oxygen;
            const float absorbPool = m_player->absorption();
            const int absIcons = absorbPool > 0.0f
                ? static_cast<int>(std::ceil(absorbPool / 2.0f))
                : 0;
            const int absW =
                absIcons > 0
                    ? absIcons * kIconPx + (absIcons - 1) * kIconGap
                    : 0;
            // The heart row (Phase 16/23; PRE-11: the narrow-window
            // fallback): the pure heartRowLayout — the right strip is
            // the oxygen bar while the air is below full (it REPLACES
            // the hunger strip at its position, SURV-2), so the layout
            // reserves the strip that renders THIS frame. Legacy
            // centered when the row fits (bit-exact); shifted left
            // until the right strip fits otherwise.
            const int rightIcons =
                oxygen < kOxygenMax ? kOxygenBarIconCount : hungerIcons;
            const HeartRowLayout heartRow =
                heartRowLayout(w, heartIcons, absIcons, rightIcons);
            const int hx0 = heartRow.heartsX;
            const int gx0 = heartRow.rightX;
            const int hy = y0 - kIconPx - kStripGap;
            // The absorption pool (Phase 72, the golden apple): extra
            // RED hearts just LEFT of the normal strip (the Minecraft
            // layout). The strip shows the POOL itself — damage empties
            // it before any normal heart is (the two-component model).
            // Pool 0 = nothing drawn (the no-input baseline is
            // bit-identical: a fresh run has no effects, hence no pool).
            // One pass per strip: tile t occupies u in [t/3, (t+1)/3].
            auto drawStrip = [&](int x0, int icons, const Texture& tex,
                                 float value, const Vec4& tint) {
                UiBatch& batch = batchFor(tint, true, &tex);
                for (int i = 0; i < icons; ++i) {
                    // Icon i covers the interval [2i, 2i+2); three states
                    // (a sub-half fill renders as half — see docs/KNOWN_ISSUES).
                    const float fill =
                        std::clamp(value - 2.0f * i, 0.0f, 2.0f);
                    const int tile =
                        fill >= 1.99f ? 0 : (fill >= 0.99f ? 1 : 2);
                    addQuadUv(batch, x0 + i * (kIconPx + kIconGap), hy,
                              kIconPx, kIconPx,
                              Vec2 {static_cast<float>(tile) / 3.0f, 0.0f},
                              Vec2 {static_cast<float>(tile + 1) / 3.0f,
                                    1.0f});
                }
            };
            if (absIcons > 0) {
                // The pool strip: red-tinted hearts, value = the pool
                // (0..the effects' total; a drained pool draws no strip).
                drawStrip(hx0 - kStripGap - absW, absIcons, m_healthTexture,
                          absorbPool, Vec4 {1.0f, 0.35f, 0.35f, 1.0f});
            }
            drawStrip(hx0, heartIcons, m_healthTexture, m_player->health(),
                      Vec4 {1.0f, 1.0f, 1.0f, 1.0f});
            // SURV-2: the oxygen bubble row REPLACES the hunger row at
            // its position (gx0, hy — the same row, right of the hearts)
            // while the air is below full (Minecraft: the hunger bar
            // hides while the air bar shows; out of the fluid the air
            // refills instantly = the row disappears the same frame).
            // Single-tile texture, no half-state: icon i is drawn while
            // oxygen > i * kOxygenPerIcon, so the fill count is
            // ceil(oxygen / kOxygenPerIcon) of kOxygenBarIconCount.
            // (oxygen is computed above — the layout reserves the strip
            // that renders this frame.)
            if (oxygen < kOxygenMax) {
                UiBatch& batch =
                    batchFor(Vec4 {1.0f, 1.0f, 1.0f, 1.0f}, true,
                             &m_oxygenTexture);
                for (int i = 0; i < kOxygenBarIconCount; ++i) {
                    if (oxygen > i * kOxygenPerIcon) {
                        addQuadUv(batch, gx0 + i * (kIconPx + kIconGap), hy,
                                  kIconPx, kIconPx, Vec2 {0.0f, 0.0f},
                                  Vec2 {1.0f, 1.0f});
                    }
                }
            } else {
                drawStrip(gx0, hungerIcons, m_hungerTexture,
                          m_player->hunger(), Vec4 {1.0f, 1.0f, 1.0f, 1.0f});
            }
            // Status effect icons (Phase 46; PRE-10: the dedicated row):
            // one 16 px effects.png tile per ACTIVE effect in application
            // order (tile t occupies u in [t/N, (t+1)/N]; the width gives
            // N). PRE-10: the icons no longer share the heart row — a
            // long icon row (>= 5 effects) drew OVER the absorption strip
            // (the Phase-56 KNOWN_ISSUES overlap, triggerable with shipped
            // content: golden apple + spider + beacon + ...). The
            // Minecraft stack is three separate rows: hearts (hy),
            // effects (the row above), armor (the row above that). The
            // effect row is left-aligned on the heart row's LEFT edge —
            // the absorption strip's left edge when the pool is up, the
            // plain hearts' left edge otherwise. No effects = nothing
            // drawn (the no-input baseline is untouched).
            const int effX0 = heartRow.rowLeft;
            const int ey = hy - kIconPx - kStripGap;
            if (m_playerEffects.count() > 0) {
                const int n = static_cast<int>(
                    m_effectTexture.width() / kIconPx);
                UiBatch& batch =
                    batchFor(Vec4 {1.0f, 1.0f, 1.0f, 1.0f}, true,
                             &m_effectTexture);
                int ex = effX0;
                for (const ActiveEffect& a : m_playerEffects.list()) {
                    const Effect& def = m_effects.at(a.effect);
                    addQuadUv(batch, ex, ey, kIconPx, kIconPx,
                              Vec2 {static_cast<float>(def.icon) / n, 0.0f},
                              Vec2 {static_cast<float>(def.icon + 1) / n,
                                    1.0f});
                    ex += kIconPx + kIconGap;
                }
            }
            // Armor row (SURV-1; PRE-10: the top row of the three-row
            // stack): the four equipment slots ABOVE the effect row
            // (hy − 2×(16 + kStripGap)) — every HUD row is to itself, so
            // no icon count can collide with another strip. Fixed
            // positions head -> feet left to right, left-aligned on the
            // hearts' left edge hx0, one 16 px per-item icon (the
            // independent PNG through the shared drawIcon path) per
            // OCCUPIED slot only. No armor = nothing drawn (the
            // no-input baseline is bit-identical: a fresh run wears
            // nothing).
            const int ay = hy - 2 * (kIconPx + kStripGap);
            for (int slot = 0; slot < kArmorSlotCount; ++slot) {
                const ItemStack& worn = m_player->armorSlot(slot);
                if (worn.empty()) {
                    continue;
                }
                const int ax = hx0 + slot * (kIconPx + kIconGap);
                drawIcon(worn, static_cast<float>(ax + kIconPx / 2),
                         static_cast<float>(ay + kIconPx / 2), kIconPx);
            }
        }
        // Crosshair: a 2px thin cross at the screen center.
        UiBatch& cross = batchFor(kCrosshair, false, nullptr);
        addQuad(cross, w / 2 - 5, h / 2 - 1, 11, 2);
        addQuad(cross, w / 2 - 1, h / 2 - 5, 2, 11);
        // Held item: a large icon in the bottom-right corner. Phase 42:
        // the icon stays put — the draw/eat animation is the 3D hand
        // (drawHeldHand), which pulls back / rises with the charge (the
        // Phase 24/28 2D icon pull is gone).
        const float heldX = static_cast<float>(w - kHeldIconOffset);
        const float heldY = static_cast<float>(h - kHeldIconOffset);
        drawIcon(m_inventory.at(m_selectedSlot), heldX, heldY, kHeldIconPx);
    }

    // -- death screen (Phase 16, while the player is dead) ------------------
    // The player physics is frozen; the pointer was released on death, so
    // R (any pointer state) respawns.
    if (m_player && m_player->isDead()) {
        addQuad(batchFor(Vec4 {0.0f, 0.0f, 0.0f, 0.55f}, false, nullptr), 0,
                0, w, h);
        // PRE-1: the user-visible strings are localized to Chinese
        // (the CJK glyphs live in the font atlas rows 0..3); the F3
        // debug overlay stays English (a developer tool).
        const std::string died = "你死了！";
        const std::string hint = "按 R 重生";
        // 16 px glyphs (Phase 35): "died" at scale 2 = 32 px tall, hint
        // 16 px — the block spans h/2-40 .. h/2+28, roughly centered.
        addText(died, w / 2 - static_cast<int>(textWidth(died, 2.0)) / 2,
                h / 2 - 40, 2.0, Vec4 {1.0f, 0.35f, 0.35f, 1.0f});
        addText(hint, w / 2 - static_cast<int>(textWidth(hint)) / 2,
                h / 2 + 12, 1.0, kText);
    }

    // -- furnace panel (Phase 40/44): the smelting GUI (right-clicked
    // furnace) — rendering the ONE furnace block the panel is bound to
    // (its per-block state). The 27 inventory slots + the smelting
    // section (input, progress bar, output, fuel). No crafting of any
    // kind happens here — the output fills by itself (the furnace tick).
    if (m_panel.openPanel == OpenPanel::Furnace) {
        const FurnaceState& f = furnaceAt(m_panel.openFurnacePos);
        const FurnaceLayout layout = FurnaceLayout::compute(w, h);
        const double s = layout.scale;
        const auto L = [s](int v) {
            return static_cast<int>(std::lround(v * s));
        };
        const int slotIcon = L(34); // icons inside slot-sized rects
        const int titleOffset = L(4); // 24 px title text in the 40 px row

        // Panel background + title.
        addQuad(batchFor(kPanel, false, nullptr), layout.panel.x,
                layout.panel.y, layout.panel.w, layout.panel.h);
        addText("熔炉", layout.title.x, layout.title.y + titleOffset,
                InventoryLayout::kTitleFontScale * s, kText);

        const int hoverSlot = layout.slotAt(mx, my);
        const int hoverFslot = layout.furnaceSlotAt(mx, my);

        // The 27 inventory slots (drawn like the crafting panels').
        for (std::size_t i = 0; i < layout.slots.size(); ++i) {
            const UIRect& r = layout.slots[i];
            addQuad(batchFor(kSlotBg, false, nullptr), r.x, r.y, r.w, r.h);
            if (i == m_selectedSlot) {
                addBorder(batchFor(kSelected, false, nullptr), r, 2);
            } else if (static_cast<int>(i) == hoverSlot) {
                addBorder(batchFor(kHover, false, nullptr), r, 2);
            } else {
                addBorder(batchFor(kBorder, false, nullptr), r, 1);
            }
            const ItemStack& stack = m_inventory.at(i);
            drawIcon(stack, static_cast<float>(r.x + r.w / 2),
                     static_cast<float>(r.y + r.h / 2), slotIcon);
            if (!stack.empty() &&
                stack.count < m_items.at(stack.item).maxStack) {
                const std::string count = std::to_string(stack.count);
                addText(count,
                        r.x + r.w - L(4) - static_cast<int>(textWidth(count, s)),
                        r.y + r.h - L(4) - L(kGlyphSize), s, kText);
            }
        }

        // The smelting section slots (0=input, 1=fuel, 2=output).
        const UIRect* frects[3] = {&layout.input, &layout.fuel,
                                   &layout.output};
        const ItemStack* fslots[3] = {&f.input, &f.fuel,
                                      &f.output};
        for (int i = 0; i < 3; ++i) {
            const UIRect& r = *frects[i];
            addQuad(batchFor(kSlotBg, false, nullptr), r.x, r.y, r.w, r.h);
            if (i == hoverFslot) {
                addBorder(batchFor(kHover, false, nullptr), r, 2);
            } else {
                addBorder(batchFor(kBorder, false, nullptr), r, 1);
            }
            const ItemStack& stack = *fslots[i];
            drawIcon(stack, static_cast<float>(r.x + r.w / 2),
                     static_cast<float>(r.y + r.h / 2), slotIcon);
            if (!stack.empty() &&
                stack.count < m_items.at(stack.item).maxStack) {
                const std::string count = std::to_string(stack.count);
                addText(count,
                        r.x + r.w - L(4) - static_cast<int>(textWidth(count, s)),
                        r.y + r.h - L(4) - L(kGlyphSize), s, kText);
            }
        }

        // The progress bar (Minecraft's arrow): a dark track + a fill
        // proportional to the current smelt progress (0 when the input is
        // empty / not smeltable — the tick keeps it that way).
        {
            const UIRect& r = layout.progress;
            addQuad(batchFor(kSlotBg, false, nullptr), r.x, r.y, r.w, r.h);
            addBorder(batchFor(kBorder, false, nullptr), r, 1);
            double frac = 0.0;
            if (!f.input.empty() &&
                !m_items.at(f.input.item).smeltableTo.empty()) {
                const int total =
                    smeltSubstepsFor(m_items.at(f.input.item));
                if (total > 0) {
                    frac = static_cast<double>(f.progressSubsteps) /
                           static_cast<double>(total);
                }
            }
            const int fill = static_cast<int>(std::lround(r.w * frac));
            if (fill > 0) {
                addQuad(batchFor(kText, false, nullptr), r.x, r.y, fill,
                        r.h);
            }
        }

        // The hovered slot's item name + the carried stack at the cursor +
        // the close hint.
        if (hoverSlot >= 0) {
            const ItemStack& stack = m_inventory.at(hoverSlot);
            if (!stack.empty()) {
                // PRE-12 (tooltip): name + " xN" (count > 1) + the
                // d/max durability readout line (the pure
                // ui/item_tooltip; the Phase-15 "only the name" residue).
                appendTooltip(itemTooltip(m_items.at(stack.item), stack),
                              mx + L(12), my + L(12), s, L(16));
            }
        }
        if (m_panel.furnaceUi.carrying()) {
            drawIcon(m_panel.furnaceUi.carry(), static_cast<float>(mx),
                     static_cast<float>(my), slotIcon);
        }
        const std::string hint = "按 E 关闭"; // PRE-1
        addText(hint,
                layout.panel.x + static_cast<int>(std::lround(
                    (layout.panel.w - textWidth(hint, s)) / 2.0)),
                layout.panel.y + layout.panel.h - L(20), s, kTextDim);
    }

    // -- crafting panel (while open): the E inventory (2x2 grid + the
    // shapeless recipe craft list) or the crafting table (3x3 grid + the
    // shaped recipe hint list, CRAFT-4). ------------------------------------
    if (panelOpen() && m_panel.openPanel != OpenPanel::Furnace) {
        const bool table = m_panel.openPanel == OpenPanel::Table;
        const PanelKind kind =
            table ? PanelKind::CraftingTable : PanelKind::Inventory;
        // The hint list per kind (same source handlePanelClicks uses).
        std::vector<const Recipe*> hints =
            table ? shapedRecipes() : shapelessRecipes();
        const InventoryLayout layout =
            InventoryLayout::compute(w, h, static_cast<int>(hints.size()),
                                     kind);
        InventoryUI& ui = table ? m_panel.tableUi : m_panel.inventoryUi;

        // Panel fit factor (Phase 35): below the native width the whole
        // panel scales down; the text (font scale) and icons (quad sizes)
        // follow the SAME factor so everything stays proportional.
        const double s = layout.scale;
        const auto L = [s](int v) {
            return static_cast<int>(std::lround(v * s));
        };
        const int slotIcon = L(34); // icons inside slot-sized rects
        const int rowIcon = L(28);  // icons inside the recipe rows
        const int titleOffset = L(4); // 24 px title text in the 40 px row

        // Panel background + titles.
        addQuad(batchFor(kPanel, false, nullptr), layout.panel.x,
                layout.panel.y, layout.panel.w, layout.panel.h);
        addText(table ? "工作台" : "背包", layout.title.x,
                layout.title.y + titleOffset,
                InventoryLayout::kTitleFontScale * s, kText);
        // The list header ("合成") now serves both panels (CRAFT-4: the
        // table panel's hint column).
        addText("合成", layout.craftHeader.x,
                layout.craftHeader.y + titleOffset,
                InventoryLayout::kTitleFontScale * s, kText);

        const int hoverSlot = layout.slotAt(mx, my);
        const int hoverCell = layout.gridCellAt(mx, my);
        const bool hoverOutput = layout.inOutput(mx, my);
        for (std::size_t i = 0; i < layout.slots.size(); ++i) {
            const UIRect& r = layout.slots[i];
            addQuad(batchFor(kSlotBg, false, nullptr), r.x, r.y, r.w, r.h);
            if (i == m_selectedSlot) {
                addBorder(batchFor(kSelected, false, nullptr), r, 2);
            } else if (static_cast<int>(i) == hoverSlot) {
                addBorder(batchFor(kHover, false, nullptr), r, 2);
            } else {
                addBorder(batchFor(kBorder, false, nullptr), r, 1);
            }
            const ItemStack& stack = m_inventory.at(i);
            drawIcon(stack, static_cast<float>(r.x + r.w / 2),
                     static_cast<float>(r.y + r.h / 2), slotIcon);
            if (!stack.empty() &&
                stack.count < m_items.at(stack.item).maxStack) {
                const std::string count = std::to_string(stack.count);
                addText(count,
                        r.x + r.w - L(4) - static_cast<int>(textWidth(count, s)),
                        r.y + r.h - L(4) - L(kGlyphSize), s, kText);
            }
        }

        // Crafting grid cells + the output slot (the matched recipe's
        // result — shaped first, then shapeless; see matchGridRecipe).
        for (std::size_t i = 0; i < layout.gridCells.size(); ++i) {
            const UIRect& r = layout.gridCells[i];
            addQuad(batchFor(kSlotBg, false, nullptr), r.x, r.y, r.w, r.h);
            if (static_cast<int>(i) == hoverCell) {
                addBorder(batchFor(kHover, false, nullptr), r, 2);
            } else {
                addBorder(batchFor(kBorder, false, nullptr), r, 1);
            }
            const ItemStack& stack = ui.grid().at(layout.gridSlots[i]);
            drawIcon(stack, static_cast<float>(r.x + r.w / 2),
                     static_cast<float>(r.y + r.h / 2), slotIcon);
            if (!stack.empty() &&
                stack.count < m_items.at(stack.item).maxStack) {
                const std::string count = std::to_string(stack.count);
                addText(count,
                        r.x + r.w - L(4) - static_cast<int>(textWidth(count, s)),
                        r.y + r.h - L(4) - L(kGlyphSize), s, kText);
            }
        }
        {
            const UIRect& r = layout.output;
            addQuad(batchFor(kSlotBg, false, nullptr), r.x, r.y, r.w, r.h);
            if (hoverOutput) {
                addBorder(batchFor(kHover, false, nullptr), r, 2);
            } else {
                addBorder(batchFor(kBorder, false, nullptr), r, 1);
            }
            const GridMatch m = ui.match();
            if (m.matched()) {
                const ItemStack out {m.recipe->output,
                                     m.recipe->outputCount};
                drawIcon(out, static_cast<float>(r.x + r.w / 2),
                         static_cast<float>(r.y + r.h / 2), slotIcon);
                if (out.count < m_items.at(out.item).maxStack) {
                    const std::string count = std::to_string(out.count);
                    addText(count,
                            r.x + r.w - L(4) -
                                static_cast<int>(textWidth(count, s)),
                            r.y + r.h - L(4) - L(kGlyphSize), s, kText);
                }
            }
        }

        // Crafting rows: the E panel's shapeless CRAFT rows (output icon +
        // "name xN" white when craftable, dim otherwise + the ingredient
        // list) and the table panel's shaped HINT rows (CRAFT-4: the mini
        // pattern + output icon + "name xN", white when the inventory can
        // supply the pattern, dim otherwise — display-only, the click is
        // consumed in handlePanelClicks).
        if (table) {
            const int pcell = L(14); // the mini pattern's cell
            const int pgap = L(2);    // between mini pattern cells
            for (std::size_t i = 0; i < layout.recipes.size(); ++i) {
                const Recipe& recipe = *hints[static_cast<std::size_t>(i)];
                const UIRect& r = layout.recipes[i];
                const bool can = canSupply(m_inventory, patternBag(recipe));
                // The 3x3 mini pattern, top-left aligned exactly as defined
                // (a valid placement the grid match accepts): dark cell
                // background + the keyed item's icon; a space = empty cell.
                for (int pr = 0; pr < CraftingGrid::kColumns; ++pr) {
                    for (int pc = 0; pc < CraftingGrid::kColumns; ++pc) {
                        const int cx = r.x + L(3) + pc * (pcell + pgap);
                        const int cy = r.y + L(3) + pr * (pcell + pgap);
                        addQuad(batchFor(kSlotBg, false, nullptr), cx, cy,
                                pcell, pcell);
                        const std::string* row =
                            pr < static_cast<int>(recipe.pattern.size())
                                ? &recipe.pattern[static_cast<std::size_t>(pr)]
                                : nullptr;
                        if (row != nullptr &&
                            pc < static_cast<int>(row->size()) &&
                            row->at(static_cast<std::size_t>(pc)) != ' ') {
                            const char ch =
                                row->at(static_cast<std::size_t>(pc));
                            const ItemID item = recipe.key.at(ch);
                            drawIcon(ItemStack {item, 1},
                                     static_cast<float>(cx + pcell / 2),
                                     static_cast<float>(cy + pcell / 2),
                                     L(12));
                        }
                    }
                }
                const Vec4 outColor = can ? kText : kTextDim;
                drawIcon(ItemStack {recipe.output, recipe.outputCount},
                         static_cast<float>(r.x + L(55)),
                         static_cast<float>(r.y + r.h / 2), rowIcon);
                // The label fits the row: the " xN" suffix is kept, a long
                // item id is trimmed so it never bleeds into the other
                // column (deterministic, see fitLabel).
                const std::string suffix =
                    " x" + std::to_string(recipe.outputCount);
                const std::string label =
                    fitLabel(m_items.at(recipe.output).name, suffix, s,
                             std::max(0, r.w - L(89) - L(4))) +
                    suffix;
                addText(label, r.x + L(89), r.y + L(18), s, outColor);
            }
        } else {
            for (std::size_t i = 0; i < layout.recipes.size(); ++i) {
                const Recipe& recipe = *hints[static_cast<std::size_t>(i)];
                const UIRect& r = layout.recipes[i];
                const bool can = canCraft(m_inventory, recipe, m_items);
                const Vec4 outColor = can ? kText : kTextDim;
                drawIcon(ItemStack {recipe.output, recipe.outputCount},
                         static_cast<float>(r.x + L(14)),
                         static_cast<float>(r.y + r.h / 2), rowIcon);
                addText(m_items.at(recipe.output).name + " x" +
                            std::to_string(recipe.outputCount),
                        r.x + L(32), r.y + L(8), s, outColor);
                std::string needs;
                for (const auto& [item, count] : recipe.ingredients) {
                    if (!needs.empty()) {
                        needs += ", ";
                    }
                    needs += m_items.at(item).name + " x" +
                             std::to_string(count);
                }
                addText("需要：" + needs, r.x + L(32), r.y + L(28), s,
                        kTextDim); // PRE-1 (item ids stay English data)
            }
        }

        // The hovered slot/cell's item name + the carried stack at the
        // cursor.
        if (hoverSlot >= 0) {
            const ItemStack& stack = m_inventory.at(hoverSlot);
            if (!stack.empty()) {
                // PRE-12 (tooltip): name + " xN" (count > 1) + the
                // d/max durability readout line (the pure
                // ui/item_tooltip; the Phase-15 "only the name" residue).
                // Drawn via the tooltip batches (after the 2D batches and
                // the 3D block-cube icons — see appendTooltip).
                appendTooltip(itemTooltip(m_items.at(stack.item), stack),
                              mx + L(12), my + L(12), s, L(16));
            }
        } else if (hoverCell >= 0) {
            const ItemStack& stack =
                ui.grid().at(layout.gridSlots[static_cast<std::size_t>(hoverCell)]);
            if (!stack.empty()) {
                appendTooltip(itemTooltip(m_items.at(stack.item), stack),
                              mx + L(12), my + L(12), s, L(16));
            }
        }
        if (ui.carrying()) {
            drawIcon(ui.carry(), static_cast<float>(mx),
                     static_cast<float>(my), slotIcon);
        }
        // Close hint, centered at the panel bottom.
        const std::string hint = "按 E 关闭"; // PRE-1
        addText(hint,
                layout.panel.x + static_cast<int>(std::lround(
                    (layout.panel.w - textWidth(hint, s)) / 2.0)),
                layout.panel.y + layout.panel.h - L(20), s, kTextDim);
    }

    // -- F3 debug overlay (while toggled) ------------------------------------
    if (m_debugOverlay) {
        DebugInfo info;
        info.fps = static_cast<int>(std::lround(m_fpsSmoothed));
        info.frame = m_frameCount;
        info.drawCalls = m_renderer.stats().drawCalls;
        info.vertices = m_renderer.stats().vertices;
        info.chunks = static_cast<int>(m_world.chunkCount());
        if (m_player) {
            info.posX = m_player->position().x;
            info.posY = m_player->position().y;
            info.posZ = m_player->position().z;
            info.yaw = m_player->yaw;
            info.pitch = m_player->pitch;
        }
        if (m_player) {
            info.columnX =
                floorDiv(static_cast<int>(std::floor(m_player->position().x)),
                         kChunkSize);
            info.columnZ =
                floorDiv(static_cast<int>(std::floor(m_player->position().z)),
                         kChunkSize);
        }
        info.seed = m_generator ? m_generator->seed() : 0;
        info.timeOfDay = m_dayCycle.timeOfDay();
        info.dayFactor = m_dayCycle.dayFactor();
        info.streamingEngaged = m_streaming.streamingEngaged;
        info.streamPending = static_cast<int>(m_streaming.streamPending.size());
        info.streamRadius = m_streaming.streamRadius;
        info.pointerLocked = locked;
        info.inventoryOpen = panelOpen();
        info.particles = m_particles.size();
        info.entities = m_entities.size();
        if (m_player) {
            info.health = m_player->health();
            info.maxHealth = m_player->maxHealth();
            info.absorption = m_player->absorption();
            info.armorPoints = m_player->armorPoints(); // SURV-1
            info.oxygen = m_drown.oxygen; // SURV-2
            info.hunger = m_player->hunger();
            info.maxHunger = m_player->maxHunger();
        }
        // SURV-3: the held item's line — "(none)" on an empty hand, the
        // item name otherwise, plus " damage/durability" when the item
        // has one (the d/max readout; a full-durability held item shows
        // 0.0).
        {
            const ItemStack& held = m_inventory.at(m_selectedSlot);
            if (held.empty()) {
                info.heldItem = "(none)";
            } else {
                const Item& hi = m_items.at(held.item);
                char buf[128];
                if (hi.durability > 0.0f) {
                    std::snprintf(buf, sizeof(buf), "%s %.1f/%.0f",
                                  hi.name.c_str(), held.damage,
                                  hi.durability);
                } else {
                    std::snprintf(buf, sizeof(buf), "%s", hi.name.c_str());
                }
                info.heldItem = buf;
            }
        }
        info.difficulty = difficultyName(m_mobs.difficulty);
        // Phase 46: the active status effects, "name I 12.3s; name 4.5s"
        // (application order); empty when the player has none. PRE-12
        // (the SURV-6 display boundary): every effect carries its
        // amplifier roman (tier 0 = "I").
        for (const ActiveEffect& a : m_playerEffects.list()) {
            char buf[80];
            std::snprintf(buf, sizeof(buf), "%s %s %.1fs",
                          m_effects.at(a.effect).name.c_str(),
                          effectTierRoman(a.tier), a.remaining);
            if (!info.effects.empty()) {
                info.effects += "; ";
            }
            info.effects += buf;
        }
        // PRE-12 (the SURV-7 display boundary): the player stats
        // (deathCount + kill.<name>; the map holds non-zero entries in
        // key order).
        info.stats = m_stats;
        int y = kF3Margin;
        for (const std::string& line : debugOverlayLines(info)) {
            addText(line, kF3Margin, y, 1, kText);
            y += kF3LineHeight;
        }
    }

    // Upload + draw every 2D batch (backgrounds, borders, text, 2D item
    // icons). The 3D block-cube icons are drawn after this loop.
    for (auto& b : batches) {
        drawUiBatch(b.vertices, ortho, b.textured ? *b.texture
                                                  : m_fontTexture,
                    b.textured, b.color);
    }

    // 3D block-cube icons last: on top of every 2D element (the slot
    // backgrounds would otherwise cover them). The batch draws above may
    // have bound the font/icon textures to unit 0; the cube UVs live in
    // the block atlas, so re-bind it. The cube draws through the BASIC
    // shader, whose per-draw uniforms (u_useTexture / u_color / u_alpha)
    // persist from the last world draw — the world's solid-color draws
    // (beacon beam, the clouds) leave u_useTexture = 0 + white u_color +
    // u_alpha < 1, which the cube would otherwise inherit and render as
    // a translucent white box (10-26 user report). Set the textured
    // recipe explicitly.
    if (!iconDraws.empty()) {
        m_atlas.texture().bind();
        m_basicShader.bind();
        m_basicShader.setUniform1i("u_useTexture", 1);
        m_basicShader.setUniform3f("u_color", 1.0f, 1.0f, 1.0f);
        m_basicShader.setUniform1f("u_alpha", 1.0f);
        for (const UiIconDraw& d : iconDraws) {
            const Mesh& mesh = itemMesh(m_items.at(d.stack.item).block);
            if (!mesh.valid()) {
                continue;
            }
            const float s =
                static_cast<float>(d.sizePx) / ItemEntity::kSize;
            const Mat4 model = Mat4::multiply(
                Mat4::multiply(
                    Mat4::multiply(
                        Mat4::translate({d.cx, d.cy, 0.0f}),
                        Mat4::rotateY(kIconTiltYaw)),
                    Mat4::rotateX(kIconTiltPitch)),
                Mat4::scale(s));
            m_renderer.draw(mesh, m_basicShader, Mat4::identity(), ortho,
                            model);
        }
    }

    // PRE-12: the hover tooltip — LAST of the UI passes, above the 2D
    // batches and the 3D block-cube icons (it sits cursor-adjacent, over
    // slot rects and icons). No hovered stack = zero batches = the
    // no-input baseline is untouched.
    for (auto& tb : tooltipBatches) {
        drawUiBatch(tb.vertices, ortho, tb.textured ? *tb.texture
                                                    : m_fontTexture,
                    tb.textured, tb.color);
    }

    // PRE-7: the effect tints — a full-screen veil per ACTIVE player
    // effect carrying a data-driven tint (poison = the Minecraft purple
    // veil). Constant alpha (the pulsing poison veil is a documented
    // deviation). No active tinted effect = zero quads: the no-input
    // baseline carries no effects, so nothing is drawn there (the
    // structural immunity).
    {
        const std::vector<EffectTint> tints =
            activeEffectTints(m_playerEffects, m_effects);
        for (const EffectTint& t : tints) {
            UiBatch veil;
            veil.color = {t.r, t.g, t.b, t.a};
            addQuad(veil, 0, 0, w, h);
            drawUiBatch(veil.vertices, ortho, m_fontTexture, false,
                        veil.color);
        }
    }

    // Hurt flash (Phase 16): a full-screen red veil that decays over
    // kHurtFlashSeconds after a damage event. Drawn last (over every UI
    // element); the timer is 0 in the no-input baseline, so nothing is
    // drawn there.
    if (m_hurtFlash > 0.0) {
        const float a =
            0.35f * static_cast<float>(m_hurtFlash / kHurtFlashSeconds);
        UiBatch flash;
        flash.color = {1.0f, 0.0f, 0.0f, a};
        addQuad(flash, 0, 0, w, h);
        drawUiBatch(flash.vertices, ortho, m_fontTexture, false,
                    flash.color);
    }
}

} // namespace mc
