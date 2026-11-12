#include "core/demo.hpp" // V3-16a: the panel-click's vocabularies
#include "core/game.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "core/game_local.hpp"
#include "core/ui_layout.hpp"
#include "core/xp_curve.hpp" // V2-11: the XP bar's pure level/fill math
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

// V2-11 (XP): the XP bar's geometry (the Minecraft position — directly
// below the hotbar row, the hotbar's full width, 2 px tall) and the
// level number's offset above the bar (the glyph height — the number
// floats over the bar's top edge, the MC HUD look). Named, not magic:
// the bar's layout is a pure function of these + the hotbar metrics.
constexpr int kXpBarGap = 4;

constexpr int kXpBarHeight = 2;

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

int Game::armorProtectionPoints(const Player& p) const {
    // V2-12 (protection): 4 * min(20, the sum of the protection levels over
    // the four worn pieces). The pure accessor per piece is
    // protectionLevel (0 when the piece is empty / unenchantable / has no
    // protection). No protection worn = 0 (the pre-V2-12 bit-identical
    // path). V3-12b: per-player (the ×N effects pass's read — solo: the
    // only player, the original value).
    int sum = 0;
    for (int slot = 0; slot < kArmorSlotCount; ++slot) {
        sum += protectionLevel(p.armorSlot(slot), m_enchants);
    }
    return 4 * std::min(20, sum);
}

void Game::updateEffects(double dt) {
    // V3-12b: the ×N effects pass (solo: one player, the pre-V3-12b
    // sequence bit-identical — the loop's single iteration).
    for (std::size_t i = 0; i < m_players.size(); ++i) {
        Player& p = *m_players[i];
        PlayerSession& s = *m_sessions[i];
    // Phase 84: push the effect-derived move-speed + damage-reduction onto
    // the player every frame (the player's movement + damage read them; the
    // identity 1.0 / 0.0 when no such effect is active). Runs even for a
    // dead player so the values reset cleanly on respawn.
        p.setSpeedMultiplier(s.effects.totalSpeedMultiplier(m_effects));
        p.setDamageReduction(s.effects.totalDamageReduction(m_effects));
    // V2-12 (protection): push the armor protection bonus each frame (0 =
    // none — the pre-V2-12 bit-identical damage path).
        p.setProtectionBonus(armorProtectionPoints(p));
    // V2-12 (unbreaking): push each worn piece's unbreaking level (0 =
    // none — the pre-V2-12 bit-identical armor wear).
        for (int slot = 0; slot < kArmorSlotCount; ++slot) {
            p.setArmorUnbreaking(
                slot, unbreakingLevel(p.armorSlot(slot), m_enchants));
        }
        if (p.isDead()) {
            continue;
        }
    // Phase 72: the absorption pool is synced only when the effects'
    // total ACTUALLY changes this frame (an expiry, a water extinguish).
    // A per-frame SET would fight the damage drain: the pool must persist
    // between sync points (the Phase 56 bonus sync was idempotent, the
    // pool is not — damage lowers it below the total).
        const float absorptionBefore = s.effects.totalAbsorption(m_effects);
    // Phase 57: the fire system (the data-flagged effect, resolved at
    // init). Runs EVEN with no active effects — the ignition is a source:
    // while the player is in the lava OR overlapping a fire block
    // (Phase 74: the fire block ignites the player the way the lava
    // does — the P70 direct contact damage is gone; both sources share
    // this one gate) the fire is (re-)applied EVERY frame (the duration
    // resets, the tick phase is kept — standing in the fire source burns
    // continuously, Minecraft); after leaving, the effect counts down
    // fireDurationSeconds and burns until it expires. Water
    // EXTINGUISHES instantly (and beats both: in water, no fire). V2-4:
    // the rain extinguishes the same way (the inWater ∨ raining
    // extension — the re-ignition runs first, so a player on a fire block
    // in the rain stays out, the water-beats-fire order; no roof-shade
    // check = the documented deviation; off/clear = the water-only path
    // bit-identical).
        if (m_fireEffectId != -1) {
            // V3-15 (netherite): the fire immunity — no re-ignition while
            // a fire-resistant piece is worn (the lava tick's suppression
            // is the player's own, the Phase-57 twin).
            if ((p.inLava() || p.isInFire()) &&
                !p.hasFireResistantArmor()) {
                s.effects.apply(m_fireEffectId,
                                p.params().fireDurationSeconds);
            }
            if (p.inWater() || m_weather.raining) {
                s.effects.clearExtinguishable(m_effects);
            }
        }
        if (s.effects.count() == 0) {
            continue;
        }
    // The ticks are REAL dt (the survival-timer class, like hunger and
    // the eat channel). The HP changes apply against the player HERE
    // (not in StatusEffects): the damage tick respects the effect's
    // min_health floor (poison keeps the player at >= 1 HP and never
    // kills — a plain damage effect with floor 0 may), the heal tick
    // clamps at max health. A poison tick that drops health shows the
    // red hurt veil like any other damage (updateSurvival polls the
    // health each frame; the v1 has no separate poison tint).
        s.effects.update(
            dt, m_effects, [&p](const Effect& def) {
                if (def.tick == Effect::Tick::Damage) {
                    // V3-15 (netherite): the fire's damage tick is
                    // suppressed by a worn fire-resistant piece (the
                    // ignition itself never lands — the re-apply gate
                    // above; a burn started before the piece equips keeps
                    // its visual but stops damaging, the Minecraft
                    // fire-resistance semantics).
                    const bool fireImmune =
                        def.name == "fire" && p.hasFireResistantArmor();
                    if (!fireImmune &&
                        p.health() - def.tickAmount >= def.minHealth) {
                        p.damage(def.tickAmount, false,
                                 {Player::DamageSource::Effect,
                                  def.name});
                    }
                } else if (def.tick == Effect::Tick::Heal) {
                    p.heal(def.tickAmount);
                }
            });
    // Phase 72: an expired (or extinguished) absorption layer drops the
    // pool to the remaining total (the health is untouched — the pool is
    // not HP). Unchanged totals leave the pool alone (the drain since
    // the last sync point persists).
        if (s.effects.totalAbsorption(m_effects) != absorptionBefore) {
            syncAbsorptionFor(p, s);
        }
    }
}

void Game::syncAbsorption() {
    // V3-12b: the primary's sync (solo: the only player, the pre-V3-12b
    // behavior; the ×N callers use syncAbsorptionFor).
    syncAbsorptionFor(*m_player, *m_sessions[0]);
}

void Game::syncAbsorptionFor(Player& p, PlayerSession& s) {
    p.setAbsorption(s.effects.totalAbsorption(m_effects));
}

void Game::applyPlayerEffect(const std::string& name, double duration,
                             int tier) {
    // V3-12b: the primary's apply (solo: the only player, the pre-V3-12b
    // behavior; the ×N callers use applyPlayerEffectFor).
    applyPlayerEffectFor(*m_player, *m_sessions[0], name, duration, tier);
}

void Game::applyPlayerEffectFor(Player& p, PlayerSession& s,
                                const std::string& name, double duration,
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
    s.effects.apply(id, duration, tier);
    const Effect& def = m_effects.at(id);
    if (def.instantHp > 0.0f) {
        p.heal(def.instantHp);
    } else if (def.instantHp < 0.0f) {
        p.damage(-def.instantHp, true);
    }
}

void Game::handleUiKeys() {
    // V3-12b: the split — the LOCAL group (F3 / F5 / C / the wheel zoom:
    // per-machine presentation + the host's save, the live pump) keeps
    // the original order, then the ×N SIM group (the hotbar digits / G /
    // T / E / the panel ESC) runs per player over the N−D input (the
    // unified relay rule — solo: m_input live, the pre-V3-12b sequence
    // bit-identical; the C→wheel and the G→T→E relative orders are
    // preserved, the regrouped branches share no state).
    if (m_input.wasPressed(GameKey::F3)) {
        m_debugOverlay = !m_debugOverlay;
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
    // UX-4: C cycles the camera (V2-1: the Minecraft F5 order — First ->
    // ThirdBack -> ThirdFront -> First; F5 is the manual save here).
    // Locked-pointer only: with the cursor free (panel open / death
    // screen) C does nothing — the E-key pattern. The mode is session
    // state (never saved): a loaded world starts in first person, the
    // Minecraft convention.
    if (m_input.wasPressed(GameKey::C) && (m_demoPlayer ? true : m_window.relativeMouseMode())) {
        m_cameraMode =
            m_cameraMode == CameraMode::First
                ? CameraMode::ThirdBack
                : m_cameraMode == CameraMode::ThirdBack
                      ? CameraMode::ThirdFront
                      : CameraMode::First;
    }
    // V2-19: the mouse wheel zooms the third-person orbit distance
    // (session state — never saved, the camera-mode precedent; the C
    // mode switch does NOT reset it). Third views only: First is a
    // no-op (the design gate).
    if (m_input.wheelDelta() != 0 && m_cameraMode != CameraMode::First) {
        m_thirdPersonDistance = std::clamp(
            m_thirdPersonDistance + kZoomStep * m_input.wheelDelta(),
            kZoomMin, kZoomMax);
    }

    // V3-12c1: the net's CHAT input (the local machine's act — the T
    // toggle + the frameText's accumulation + the Enter's send / the
    // ESC's cancel; the live pump's face, never the sim: the chat is
    // presentation, the hash's exclusion by construction). The solo's
    // T stays the tutorial's skip (the ×N group's gate above).
    if (netActive()) {
        if (m_chatInputActive) {
            m_chatBuffer += m_input.frameText();
            if (m_input.frameBackspace() && !m_chatBuffer.empty()) {
                m_chatBuffer.pop_back();
            }
            if (m_input.wasPressed(GameKey::Enter)) {
                const std::string text = latinOnly(m_chatBuffer);
                if (!text.empty()) {
                    if (m_netIsHost) {
                        displayChat(0, text);
                        for (auto& sess : m_nets) {
                            (void)sess->send(NetType::Chat, packChat(0, text));
                        }
                    } else {
                        displayChat(m_localSlot, text);
                        netSendChat(text);
                    }
                }
                m_chatBuffer.clear();
                m_chatInputActive = false;
                m_window.setTextInputEnabled(false);
                m_input.setTypingSuppressed(false);
            } else if (m_input.wasPressed(GameKey::Escape)) {
                m_chatInputActive = false;
                m_chatBuffer.clear();
                m_window.setTextInputEnabled(false);
                m_input.setTypingSuppressed(false);
            }
        } else if (m_input.wasPressed(GameKey::T) &&
                   (m_demoPlayer ? true : m_window.relativeMouseMode()) && !panelOpen() &&
                   !localPlayer()->isDead()) {
            m_chatInputActive = true;
            m_chatBuffer.clear();
            m_window.setTextInputEnabled(true);
            m_input.setTypingSuppressed(true);
        }
    }

    // The ×N SIM group.
    for (std::size_t i = 0; i < m_players.size(); ++i) {
        Player& p = *m_players[i];
        PlayerSession& s = *m_sessions[i];
        const Input& in = inputFor(i);
        // R18: the sim lock read — solo: the SDL state (the original
        // value); net: the player's N−D FrameState flag. V3-16e: the
        // demo's player = the pure injection (always "locked" — the
        // physical flap's drop's twin, see game_frame's inputLocked
        // mirror).
        const bool locked =
            (m_players.size() == 1)
                ? (m_demoPlayer ? true : m_window.relativeMouseMode())
                : s.inputLocked;
        // CRAFT-7: the top-row digits select the hotbar slot directly
        // (Minecraft; the Tab cycle stays). V3-11b: with the creative
        // search FOCUSED (the 物品栏 tab open) a digit is TEXT (it types
        // into the search — the key's printable character) and does NOT
        // switch the hotbar (the Minecraft's search-focus semantics; the
        // slot is chosen before the panel opens, or with the pointer
        // locked). The text side-effect is the panel branch's
        // appendFrameText — the digit is typed either way.
        const bool searchFocused =
            s.panel.openPanel == OpenPanel::Inventory && p.isCreative() &&
            s.panel.creativeTab == 0;
        for (int d = 0; d < 9; ++d) {
            if (in.wasPressed(static_cast<GameKey>(
                    static_cast<int>(GameKey::Digit1) + d))) {
                if (!searchFocused) {
                    s.selectedSlot = static_cast<std::size_t>(d);
                }
            }
        }
        // V3-11a: the creative mode toggle (G). D11: creative MULTI is
        // disabled in 12b — the branch is solo-only (the net sessions'
        // players stay survival; the solo creative is untouched).
        if (m_players.size() == 1 && in.wasPressed(GameKey::G) && locked &&
            !p.isDead()) {
            const bool on = !p.isCreative();
            p.setCreative(on);
            p.setFlying(on);
            MC_LOG_INFO("Creative mode " + std::string(on ? "ON" : "OFF") +
                        " (G)");
        }
        // UX-7 / V2-3: T skips the tutorial (locked pointer only). D12:
        // the tutorial anchors the PRIMARY in 12b (host-only — the
        // client's user's T is a no-op, the host's T relays). V3-12c1:
        // the SOLO-only gate — the net's T is the chat's toggle (the
        // local face), the tutorial-skip's net deferral (the D12's
        // host-only + the c1's chat's distinct key).
        if (i == 0 && m_players.size() == 1 &&
            in.wasPressed(GameKey::T) && locked && s.tutorialEnabled &&
            !s.tutorial.done) {
            if (s.tutorial.seriesIndex + 1 <
                static_cast<int>(m_tutorialSeries.size())) {
                s.tutorial.seriesIndex += 1;
                s.tutorial.stepIndex = 0;
                s.tutorial.progress = 0;
                MC_LOG_INFO("Tutorial: series skipped (" +
                            std::to_string(s.tutorial.seriesIndex) + "/" +
                            std::to_string(m_tutorialSeries.size()) + ")");
            } else {
                s.tutorial.done = true;
                s.tutorialToastFrames = kTutorialToastFrames;
                MC_LOG_INFO("Tutorial: skipped");
            }
        }
        // R17: the E-key panel is the LOCAL player's face (the remote
        // player's E is a no-op in 12b — its inventory moves only via
        // the Q drop + the pickup's world state).
        const bool isLocal = (i == static_cast<std::size_t>(m_localSlot));
        // V3-12c1: the E / panel-ESC's edge is the RELAYED input's —
        // processed for EVERY player on every machine (the panel state
        // is the simulation; the owner machine's physical press is
        // what generates the edge, captured + relayed like every other
        // key). The isLocal gate was the 12b's "the remote's E is a
        // no-op" reading — it desyncs the host player's panel (the
        // client ignores the host's E, the host pauses the body, the
        // frame-N hash trips — the gate-catch's root). The window
        // acts self-gate inside openPanelFor / closePanelFor (the
        // localOwner's check), so a remote player's panel never
        // touches this machine's pointer / text input.
        if (in.wasPressed(GameKey::E)) {
            if (s.panel.openPanel != OpenPanel::None) {
                closePanelFor(p, s);
            } else if (locked) {
                openPanelFor(p, s, OpenPanel::Inventory);
            }
        }
        if (s.panel.openPanel != OpenPanel::None &&
            in.wasPressed(GameKey::Escape)) {
            closePanelFor(p, s);
        }
        (void)isLocal;
    }
}

void Game::openPanelFor(Player& p, PlayerSession& s, OpenPanel panel) {
    // V3-12c1: the WINDOW + SOUND acts are the LOCAL presentation (the
    // pointer free / the text input / the ui_open timbre are this
    // machine's window's facts) — applied only when the panel's owner
    // is this machine's player. The PANEL STATE (s.panel + the channel
    // resets) is SIMULATION — it runs for every player's E / RMB edge
    // on every machine (the relayed input's face — the isLocal-gate's
    // host-player-panel desync's gate-catch fix: the client ignored
    // the host player's E, the host paused the body's physics, the
    // frame-N hash tripped). Solo: the single player is always the
    // local one — the original acts, bit-identical.
    const bool localOwner = (&p == localPlayer());
    if (localOwner) {
        m_window.setRelativeMouseMode(false); // free the cursor for clicking
    }
    s.panel.openPanel = panel;
    resetBreaking(s);
    resetBowDraw(s); // a panel interrupts a draw (Phase 24)
    resetEatChannel(s); // a panel interrupts an eat too (Phase 28)
    // V3-11b: the creative's E opens on the 物品栏 tab (the Minecraft's
    // creative-tab default — the search + grant face first), with a
    // FRESH session state (the enchant slots' precedent). The typed
    // text is enabled ONLY for the creative's Inventory panel —
    // SDL3's text input is off by default, so every other open (and
    // the whole no-input run) never delivers a TEXT_INPUT event
    // (the structural-immunity root).
    if (panel == OpenPanel::Inventory && p.isCreative()) {
        s.panel.creativeUi.reset();
        s.panel.creativeTab = 0;
    }
    if (localOwner) {
        m_window.setTextInputEnabled(
            panel == OpenPanel::Inventory && p.isCreative());
        m_sound.play("ui_open");
    }
}

void Game::closePanelFor(Player& p, PlayerSession& s) {
    // V2-12: the enchant panel's item + lapis slots are SESSION state —
    // they return to the inventory on close (a full inventory drops them at
    // the player's feet — the documented return rule). The other panels'
    // states persist (the furnace per-block, the grids), so only the
    // Enchant panel triggers the return.
    if (s.panel.openPanel == OpenPanel::Enchant) {
        returnEnchantSlotFor(p, s, s.panel.enchantItemSlot);
        returnEnchantSlotFor(p, s, s.panel.enchantLapisSlot);
        s.panel.enchantOffers = EnchantOffers {};
    }
    // V3-11b: the typed text is a creative-Inventory-panel state — a
    // close (of any panel) stops it (the open's twin; SDL3's default
    // stays off, so the no-input run's immunity holds across close).
    // V3-12c1: the window/text-input act is the local owner's only
    // (the openPanelFor's twin — a remote player's close must not
    // touch this machine's text input).
    if (&p == localPlayer()) {
        m_window.setTextInputEnabled(false);
    }
    s.panel.openPanel = OpenPanel::None;
    if (&p == localPlayer()) {
        m_sound.play("ui_close");
    }
}

// Phase 44: the furnace panel opens for ONE block — bind the UI to that
// block's state (the setBlock invariant guarantees a furnace block owns
// one; furnaceAt's default-construct is defensive). The state is stable
// while the panel is open: block interaction is paused and no other code
// path mutates or removes a furnace block in the meantime.
void Game::openFurnacePanelFor(Player& p, PlayerSession& s, const BlockPos& pos) {
    s.panel.openFurnacePos = pos;
    s.panel.furnaceUi.bind(furnaceAt(pos));
    openPanelFor(p, s, OpenPanel::Furnace);
    // UX-7: the tutorial interact event (the panel-open success point —
    // the block under the click, the data-driven name).
    emitTutorialEvent(
        "interact." +
            m_blocks.at(m_world.getBlock(pos.x, pos.y, pos.z)).name, 1, &p);
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
void Game::openTablePanelFor(Player& p, PlayerSession& s, const BlockPos& pos) {
    s.panel.tableUi.bind(tableAt(pos));
    openPanelFor(p, s, OpenPanel::Table);
    // UX-7: the tutorial interact event (the panel-open success point —
    // the block under the click, the data-driven name).
    emitTutorialEvent(
        "interact." +
            m_blocks.at(m_world.getBlock(pos.x, pos.y, pos.z)).name, 1, &p);
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

// V2-12: the enchanting panel (the furnace GUI's session-state twin). The
// offers are rolled on demand (deterministic — bookshelfCount + the item's
// class/enchantability + the player's xpSeed); the item/lapis slots are
// session state returned to the inventory on close (the documented
// deviation from the furnace's per-block persistence).
void Game::openEnchantPanelFor(Player& p, PlayerSession& s, const BlockPos& pos) {
    s.panel.openEnchantPos = pos;
    s.panel.enchantItemSlot = ItemStack {};
    s.panel.enchantLapisSlot = ItemStack {};
    s.panel.enchantUi.bind(s.panel.enchantItemSlot, s.panel.enchantLapisSlot);
    openPanelFor(p, s, OpenPanel::Enchant);
    regenEnchantOffersFor(p, s); // empty slot -> the offers clear
    // UX-7: the tutorial interact event (the panel-open success point).
    emitTutorialEvent(
        "interact." +
            m_blocks.at(m_world.getBlock(pos.x, pos.y, pos.z)).name, 1, &p);
}

void Game::regenEnchantOffersFor(Player& p, PlayerSession& s) {
    if (s.panel.openPanel != OpenPanel::Enchant) {
        return;
    }
    const ItemStack& itemSlot = s.panel.enchantItemSlot;
    if (itemSlot.empty()) {
        s.panel.enchantOffers = EnchantOffers {};
        return;
    }
    const Item& item = m_items.at(itemSlot.item);
    // bookshelfCount is evaluated ONLY while the panel is open (no panel =
    // zero cost, the structural-immunity root).
    const int B =
        bookshelfCount(m_world, m_blocks, s.panel.openEnchantPos);
    s.panel.enchantOffers = generateOffers(
        static_cast<std::uint64_t>(m_generator->seed()),
        s.panel.openEnchantPos, item.enchantClass, item.enchantability,
        p.xpSeed(), B, m_enchants);
}

// V2-12: return one panel slot's stack to the inventory (add-merge; a full
// inventory drops it at the player's feet — the documented return rule).
void Game::returnEnchantSlotFor(Player& p, PlayerSession& s, ItemStack& stack) {
    if (stack.empty()) {
        return;
    }
    const Item& item = m_items.at(stack.item);
    const int slot =
        s.inventory.add(stack.item, stack.count, m_items, stack.damage,
                        stack.enchants);
    if (slot < 0) {
        m_entities.push_back(std::make_unique<ItemEntity>(
            m_world, m_blocks, item.name, item.maxStack, stack.count,
            p.position(), stack.damage, stack.enchants));
    }
    stack = ItemStack {};
}

bool Game::applyEnchantOfferFor(Player& p, PlayerSession& s, int row) {
    if (row < 0 || row > 2) {
        return false;
    }
    const EnchantOffer* o =
        row == 0 ? &s.panel.enchantOffers.top
        : row == 1 ? &s.panel.enchantOffers.mid
                   : &s.panel.enchantOffers.bottom;
    const ItemStack& itemSlot = s.panel.enchantItemSlot;
    if (itemSlot.empty()) {
        return false;
    }
    const Item& item = m_items.at(itemSlot.item);
    const int poolLevel = levelFromTotalXp(p.xpTotal());
    const int lapisCount =
        s.panel.enchantLapisSlot.empty() ? 0 : s.panel.enchantLapisSlot.count;
    if (!offerApplicable(itemSlot, item.enchantability, poolLevel, lapisCount,
                         *o)) {
        return false;
    }
    // Apply: the item slot gains the offer's enchants (one apply per item —
    // the offerApplicable gate ensures it was unenchanted).
    s.panel.enchantItemSlot.enchants = o->enchants;
    // Deduct the whole XP levels (the within-level remainder is kept).
    p.setXpTotal(
        applyEnchantXpCharge(p.xpTotal(), o->xpCost));
    // Deduct the lapis from the panel slot.
    s.panel.enchantLapisSlot.count -= o->lapisCost;
    if (s.panel.enchantLapisSlot.count <= 0) {
        s.panel.enchantLapisSlot = ItemStack {};
    }
    // Advance the xpSeed (the next panel-open roll differs — the PITS #4
    // step-then-mix).
    p.setXpSeed(xpSeedStep(p.xpSeed()));
    // The item is now enchanted: clear the offers (a re-place re-rolls).
    s.panel.enchantOffers = EnchantOffers {};
    m_sound.play("craft"); // reused (no dedicated enchant sound asset)
    MC_LOG_DEBUG("enchant applied: " +
                 std::to_string(s.panel.enchantItemSlot.enchants.size()) +
                 " enchant(s), xpCost " + std::to_string(o->xpCost) +
                 ", lapisCost " + std::to_string(o->lapisCost));
    return true;
}

void Game::tryCampfireCookFor(PlayerSession& s, const BlockPos& pos) {
    // The click is ALWAYS consumed (the caller returns) — a campfire is
    // never placed through this branch, whatever the hand holds.
    CampfireCookState& st = campfireAt(pos);
    const ItemStack& held = s.inventory.at(s.selectedSlot);
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
            // UX-7: the tutorial interact event (the relight completed —
            // the block the click turned into, the data-driven name).
            emitTutorialEvent("interact." + m_blocks.at(toId).name, 1);
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
                    s.inventory.take(s.selectedSlot, 1);
                    m_sound.play("place");
                    // UX-7: the tutorial interact event (a unit was
                    // placed on the fire — the block under the click).
                    emitTutorialEvent(
                        "interact." +
                            m_blocks.at(m_world.getBlock(pos.x, pos.y,
                                                         pos.z))
                                .name,
                        1);
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
            s.inventory.setSlot(s.selectedSlot, st.slots[i]);
            st.slots[i] = ItemStack {};
            st.progressSubsteps[i] = 0;
            m_sound.play("pickup");
            // UX-7: the tutorial interact event (a unit was taken back —
            // the block under the click, lit or extinguished).
            emitTutorialEvent(
                "interact." +
                    m_blocks.at(m_world.getBlock(pos.x, pos.y, pos.z)).name,
                1);
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

void Game::handlePanelClicks(Player& p, PlayerSession& s, const Input& in) {
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
    const bool shift = in.isDown(GameKey::LeftShift);
    const bool leftPressed = in.wasPressed(MouseButton::Left);
    const bool rightPressed = in.wasPressed(MouseButton::Right);
    // R2: the collect window's clock — solo: the wall clock (the
    // original read, bit-identical); net: the shared sim clock.
    const double now =
        (m_players.size() == 1) ? nowSeconds() : m_simTime;
    auto isDoubleClick = [&](int slot) {
        return !shift && slot >= 0 && slot == s.panel.lastLeftSlot &&
               s.panel.lastLeftClickTime >= 0.0 &&
               (now - s.panel.lastLeftClickTime) <= kDoubleClickSeconds;
    };
    auto armDoubleClick = [&](int slot) {
        s.panel.lastLeftSlot = slot;
        s.panel.lastLeftClickTime = slot >= 0 ? now : -1.0;
    };
    auto disarmDoubleClick = [&]() {
        s.panel.lastLeftSlot = -1;
        s.panel.lastLeftClickTime = -1.0;
    };

    // Phase 40: the furnace panel. Same click model as the crafting
    // panels: the layout hit tests are disjoint (at most one of a
    // 36-slot / input / fuel / output is active; a left click with all of
    // them inactive drops the carry). No craft action — the output fills
    // by itself.
    if (s.panel.openPanel == OpenPanel::Furnace) {
        const FurnaceLayout layout = FurnaceLayout::compute(w, h);
        const std::pair<int, int> mp = uiMouseFor(in);
        const int slot = layout.slotAt(mp.first, mp.second);
        const int fslot = layout.furnaceSlotAt(mp.first, mp.second);
        if (leftPressed) {
            if (shift) {
                s.panel.furnaceUi.quickMove(slot, fslot);
            } else if (fslot < 0 && isDoubleClick(slot)) {
                s.panel.furnaceUi.doubleCollect(slot);
            } else {
                s.panel.furnaceUi.leftClick(slot, fslot);
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
                s.panel.furnaceUi.quickMove(slot, fslot);
            } else {
                s.panel.furnaceUi.rightClick(slot, fslot);
            }
            disarmDoubleClick();
        }
        return;
    }

    // V2-12: the enchanting panel. The same pick-and-place model as the
    // furnace (the layout hit tests are disjoint: at most one of a
    // 36-slot / item / lapis / offer row is active). The offer rows (the
    // panelSlotAt 2..4) apply the offer when the four gates pass; the item
    // / lapis slots are ordinary pick-and-place (the lapis slot's
    // enchant_material gate lives in the EnchantUI). A left click that
    // changes the item slot re-rolls the offers (the one-shot generation).
    if (s.panel.openPanel == OpenPanel::Enchant) {
        const EnchantLayout layout = EnchantLayout::compute(w, h);
        const std::pair<int, int> mp = uiMouseFor(in);
        const int slot = layout.slotAt(mp.first, mp.second);
        const int pslot = layout.panelSlotAt(mp.first, mp.second);
        if (leftPressed) {
            if (shift) {
                s.panel.enchantUi.quickMove(slot, pslot);
            } else {
                const int offerRow = (pslot >= 2 && pslot <= 4) ? pslot - 2 : -1;
                if (offerRow >= 0) {
                    applyEnchantOfferFor(p, s, offerRow); // the sound plays inside
                } else if (s.panel.enchantUi.leftClick(slot,
                                                       pslot == 0 || pslot == 1
                                                           ? pslot
                                                           : -1)) {
                    regenEnchantOffersFor(p, s); // the item slot changed
                }
            }
            disarmDoubleClick();
            return;
        }
        if (rightPressed) {
            if (shift) {
                s.panel.enchantUi.quickMove(slot, pslot);
            } else {
                const bool itemChanged =
                    s.panel.enchantUi.rightClick(
                        slot, pslot == 0 || pslot == 1 ? pslot : -1);
                if (itemChanged) {
                    regenEnchantOffersFor(p, s);
                }
            }
            disarmDoubleClick();
        }
        return;
    }

    const bool table = s.panel.openPanel == OpenPanel::Table;
    const PanelKind kind =
        table ? PanelKind::CraftingTable : PanelKind::Inventory;
    // The hint list per kind: shapeless craft rows (E panel) or shaped
    // display-only hint rows (table panel, CRAFT-4). UX-8c: the E panel
    // orders its rows craftable-first (orderedRecipeHints — the SAME seam
    // the render path applies, so a row index means the same recipe for
    // the click and the draw); the table list stays in registry order.
    std::vector<const Recipe*> hints =
        table ? shapedRecipes() : shapelessRecipes();
    if (!table) {
        hints = orderedRecipeHints(hints, s.inventory, m_items);
    }
    const InventoryLayout layout =
        InventoryLayout::compute(w, h, static_cast<int>(hints.size()), kind);
    const std::pair<int, int> mp = uiMouseFor(in);
    // V3-11b: the creative's E-panel face (the tabs + the 物品栏 tab).
    // The outer layout above is shared (the 背包 tab's content IS the
    // ordinary E panel's — bit-identical to the survival's; only the
    // title row's face differs). The 物品栏 tab consumes the frame's
    // typed text (the search), the wheel (the scroll) and the clicks
    // (the tab switch / the grant).
    if (!table && p.isCreative()) {
        const CreativeLayout cl =
            CreativeLayout::compute(w, h, static_cast<int>(hints.size()));
        if (s.panel.creativeTab == 0) {
            if (!in.frameText().empty()) {
                s.panel.creativeUi.appendFrameText(in.frameText());
            }
            if (in.frameBackspace()) {
                s.panel.creativeUi.backspace();
            }
            s.panel.creativeUi.scrollBy(in.wheelDelta());
        }
        const int tab = cl.tabAt(mp.first, mp.second);
        if (leftPressed && tab >= 0 && tab != s.panel.creativeTab) {
            s.panel.creativeTab = tab;
            disarmDoubleClick();
            return;
        }
        if (s.panel.creativeTab == 0) {
            const int ccell = cl.cellAt(mp.first, mp.second);
            if (leftPressed || rightPressed) {
                if (ccell >= 0 &&
                    s.panel.creativeUi.grant(
                        static_cast<int>(s.selectedSlot), ccell,
                        leftPressed)) {
                    m_sound.play("pickup");
                }
                disarmDoubleClick();
            }
            return; // the 物品栏 tab: no slot pick-and-place
        }
        // The 背包 tab: fall through to the ordinary routing below.
    }
    const int px = mp.first;
    const int py = mp.second;
    InventoryUI& ui = table ? s.panel.tableUi : s.panel.inventoryUi;
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
            if (canCraft(s.inventory, recipe, m_items)) {
                auto crafted = craft(s.inventory, recipe, m_items);
                if (crafted.ok()) {
                    m_sound.play("craft");
                    // UX-7: the tutorial craft event — the RECIPE-ROW
                    // path (the grid/output click path's twin): one
                    // event per successful craft. A recipe-row click is
                    // ALWAYS a true craft (canCraft matched just above
                    // — the UX-5 repair case only lives in the grid
                    // match, which this path never touches), so no
                    // repair discriminator is needed here.
                    emitTutorialEvent("craft." +
                                         m_items.at(recipe.output).name,
                                     1, &p);
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
            // UX-7: the tutorial craft event — the RECIPE branch only
            // (a UX-5 repair completion ALSO plays the "craft" sound
            // but crafts nothing — the pre-click match's recipe pointer
            // is the branch discriminator; the grid is consumed on a
            // successful click, so the match must be read BEFORE it).
            const GridMatch tutMatch = onOutput ? ui.match() : GridMatch {};
            if (ui.leftClick(slot, gridSlot, onOutput)) {
                m_sound.play("craft");
                if (tutMatch.recipe != nullptr) {
                    emitTutorialEvent(
                        "craft." +
                            m_items.at(tutMatch.recipe->output).name,
                        1, &p);
                }
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
            // UX-7: the recipe-branch discriminator (the pre-click match,
            // the left-click branch's twin — a repair completion does not
            // emit).
            const GridMatch tutMatch = onOutput ? ui.match() : GridMatch {};
            if (ui.rightClick(slot, gridSlot, onOutput)) {
                m_sound.play("craft");
                if (tutMatch.recipe != nullptr) {
                    emitTutorialEvent(
                        "craft." +
                            m_items.at(tutMatch.recipe->output).name,
                        1, &p);
                }
            }
        }
        disarmDoubleClick();
    }
}


Result<void> Game::demoPanelClick(std::uint32_t panel, std::int32_t slot,
                                  std::int16_t button) {
    // V3-16a: the demo's panel-click's resolver (the PanelClick event's
    // game face). The three steps — (1) the requested panel's open
    // check (the problem's name = the demo's break's loud line, the
    // gate's grep face), (2) the layout's slot rect (the click path's
    // TWIN compute — the same pure geometry as the render), (3) the
    // synthetic click through the genuine Input closures (the panel's
    // logic's zero new path: the position + the shift (the button's
    // face) + the press's edge, the down + the up's same frame).
    const Player& p = *m_player;
    const PanelState& ps = localPanel();
    switch (panel) {
        case kDemoPanelInventory:
            // The player's 36 slots are the shared face of the three
            // slot-showing panels (the E's inventory / the table's /
            // the furnace's all show the same slots — the layout's
            // kind's twin): the id's inventory's face accepts all
            // three (the g0..g3 / the out's cells are the crafting's
            // exclusive, the slot's range check's below).
            if (ps.openPanel != OpenPanel::Inventory &&
                ps.openPanel != OpenPanel::Table &&
                ps.openPanel != OpenPanel::Furnace) {
                return MC_FAIL("the inventory panel is not open (open it "
                               "with 'e' first)");
            }
            break;
        case kDemoPanelTable:
            if (ps.openPanel != OpenPanel::Table) {
                return MC_FAIL("the crafting table's panel is not open "
                               "(right-click the table first)");
            }
            break;
        case kDemoPanelFurnace:
            if (ps.openPanel != OpenPanel::Furnace) {
                return MC_FAIL("the furnace's panel is not open (right-"
                               "click the furnace first)");
            }
            break;
        case kDemoPanelCreative:
            if (ps.openPanel != OpenPanel::Inventory || !p.isCreative() ||
                ps.creativeTab != 0) {
                return MC_FAIL("the creative's items tab is not open "
                               "(creative mode + 'e' + the items tab "
                               "first)");
            }
            break;
        default:
            return MC_FAIL("the demo's panel id " + std::to_string(panel) +
                           " is not a panel (inventory/table/furnace/"
                           "creative)");
    }
    const UiSize space = uiSize();
    const int w = space.width;
    const int h = space.height;
    UIRect rect;
    bool have = false;
    if (panel == kDemoPanelFurnace) {
        const FurnaceLayout layout = FurnaceLayout::compute(w, h);
        if (slot >= 0 && slot <= 35) {
            // The furnace's panel's shared 36 slots (the player's
            // inventory's face, the layout's slots' twin)
            rect = layout.slots[slot];
            have = true;
        } else if (slot == 120) {
            rect = layout.input;
            have = true;
        } else if (slot == 121) {
            rect = layout.fuel;
            have = true;
        } else if (slot == 122) {
            rect = layout.output;
            have = true;
        }
    } else if (panel == kDemoPanelCreative) {
        const auto hints = shapelessRecipes();
        const CreativeLayout layout =
            CreativeLayout::compute(w, h, static_cast<int>(hints.size()));
        if (slot >= 300 && slot <= 323) {
            rect = layout.cells[slot - 300];
            have = true;
        }
    } else {
        // The crafting's shared face: the layout follows the OPEN
        // panel (the render's twin — the E's 2x2's hint list vs the
        // table's 3x3's vs the furnace's smelting section's: the 36
        // slots' RECTS differ between the layouts, the open's twin
        // must hold), the slot id's panel tag selects the id's space
        // (the inventory's 0..35's shared + the grid's / the output's
        // per-panel's exclusive).
        if (ps.openPanel == OpenPanel::Furnace) {
            // The furnace's open: the shared 36 slots' rects are the
            // FURNACE layout's (the smelting section's geometry).
            const FurnaceLayout flayout = FurnaceLayout::compute(w, h);
            if (slot >= 0 && slot <= 35) {
                rect = flayout.slots[slot];
                have = true;
            }
        } else {
            const bool tableOpen = ps.openPanel == OpenPanel::Table;
            const PanelKind kind = tableOpen ? PanelKind::CraftingTable
                                             : PanelKind::Inventory;
            const auto hints =
                tableOpen ? shapedRecipes() : shapelessRecipes();
            const InventoryLayout layout = InventoryLayout::compute(
                w, h, static_cast<int>(hints.size()), kind);
            if (slot >= 0 && slot <= 35) {
                rect = layout.slots[slot];
                have = true;
            } else if (panel == kDemoPanelTable && slot >= 200 &&
                       slot <= 208) {
                if (!tableOpen) {
                    have = false;
                } else {
                    rect = layout.gridCells[slot - 200];
                    have = true;
                }
            } else if (panel == kDemoPanelInventory && slot >= 100 &&
                       slot <= 103) {
                rect = layout.gridCells[slot - 100];
                have = true;
            } else if (slot == 110) {
                // The output (the E's 2x2's craft + the table's 3x3's —
                // both kinds' layout's output rect, the open's twin).
                rect = layout.output;
                have = true;
            } else {
                have = false;
            }
        }
    }
    if (!have) {
        return MC_FAIL("the slot '" +
                       demoPanelSlotName(panel, slot) +
                       "' is not in the " + demoPanelName(panel) +
                       " panel (the demo's break's named line)");
    }
    // The rect's center (UI px) -> the window's px (the uiScale's
    // inverse — the hit test's lround twin, the slot's width's
    // 11+-UI-px's margin absorbs the +-1-UI-px's rounding).
    const double scale = static_cast<double>(m_uiScale);
    const int x = static_cast<int>(std::lround(
        (rect.x + rect.w * 0.5) * scale));
    const int y = static_cast<int>(std::lround(
        (rect.y + rect.h * 0.5) * scale));
    m_input.handleMouseMotion(0.0f, 0.0f, static_cast<float>(x),
                              static_cast<float>(y));
    const bool shift = (button == kDemoPanelButtonShift);
    const int sdlButton =
        (button == kDemoPanelButtonRight) ? SDL_BUTTON_RIGHT
                                          : SDL_BUTTON_LEFT;
    if (shift) {
        m_input.handleKey(SDLK_LSHIFT, true);
    }
    m_input.handleMouseButton(sdlButton, true);
    m_input.handleMouseButton(sdlButton, false);
    if (shift) {
        m_input.handleKey(SDLK_LSHIFT, false);
    }
    return {};
}
void Game::updateSoundTimers(double dt) {
    // V3-12b: the ×N step-timer pass (the per-player footstep RHYTHM —
    // solo: one player, the pre-V3-12b sequence bit-identical). D8: the
    // SOUND plays for the local player only (the remote player is
    // silent — the 3D-propagation's documented absence).
    for (std::size_t i = 0; i < m_players.size(); ++i) {
        Player& p = *m_players[i];
        PlayerSession& s = *m_sessions[i];
        // R18: the solo lock mirror (the original read's value); net:
        // the per-player flag (the FrameState's local fact).
        if (m_players.size() == 1) {
            s.inputLocked = m_window.relativeMouseMode();
        }
        if (!s.inputLocked || s.panel.openPanel != OpenPanel::None) {
            continue;
        }
        const Vec3 v = p.velocity();
        const bool moving =
            std::abs(v.x) > kStepMinSpeed || std::abs(v.z) > kStepMinSpeed;
        if (moving && p.onGround()) {
            s.interaction.stepTimer += dt;
            if (s.interaction.stepTimer >= kStepSoundInterval) {
                s.interaction.stepTimer = 0.0;
                if (i == static_cast<std::size_t>(m_localSlot)) {
                    m_sound.play("step");
                }
            }
        } else {
            s.interaction.stepTimer = 0.0;
        }
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
    // V3-12b: the ×N survival pass (solo: one player, the pre-V3-12b
    // sequence bit-identical). D8: the hurt / death sounds + the pointer
    // release are LOCAL presentation (the local player's death screen);
    // the state (the flash / the stats / the effects / the drown reset /
    // the respawn) is per-player simulation.
    for (std::size_t i = 0; i < m_players.size(); ++i) {
        Player& p = *m_players[i];
        PlayerSession& s = *m_sessions[i];
        const bool local = (i == static_cast<std::size_t>(m_localSlot));
        if (s.hurtFlash > 0.0) {
            s.hurtFlash = std::max(0.0, s.hurtFlash - dt);
        }
        const float hp = p.health();
        const bool dead = p.isDead();
        if (s.prevHealth >= 0.0f && hp < s.prevHealth) {
            // A health drop this frame: the red veil restarts and the
            // hurt sound plays (suppressed when the drop was lethal —
            // the death sound follows instead).
            s.hurtFlash = kHurtFlashSeconds;
            if (!dead && local) {
                m_sound.play("hurt");
            }
        }
        if (dead && s.prevHealth >= 0.0f && s.prevHealth > 0.0f) {
            // Alive -> dead transition: the death screen owns the input,
            // so release the pointer (it was locked while the fall
            // happened) — the LOCAL machine's pointer (the death screen
            // is local UI; the remote player's death releases nothing
            // here — its body is simulated, D8). Phase 46: the active
            // status effects die with the player (the respawn starts
            // clean — Minecraft parity).
            if (local) {
                m_window.setRelativeMouseMode(false);
            }
            if (local) {
                m_sound.play("death");
            }
            // SURV-7: the death counter (the alive->dead transition —
            // one increment per life lost, whatever the cause;
            // frame-locked).
            s.stats["deathCount"]++;
            // V3-19: the survival line's SNAPSHOT (the transition's
            // frame — the death screen reads it instead of the live
            // m_frameCount, which keeps ticking while the world sim
            // runs on after death).
            s.deathFrame = m_frameCount;
            s.effects.clearAll();
            syncAbsorptionFor(p, s); // Phase 72: the pool dies
            // SURV-2: the air refills with the death (the respawn
            // convergence point — the next life starts on the dry spawn
            // with full oxygen; while dead the pass is inert and the
            // state's decay is harmless).
            s.drown = DrownState {};
        }
        s.prevHealth = hp;
        if (dead && inputFor(i).wasPressed(GameKey::R)) {
            respawnPlayerFor(p, s);
            s.prevHealth = p.health(); // no hurt flash on respawn
        }
    }
}

void Game::respawnPlayer() {
    // V3-12b: the primary's respawn (solo: the only player, the
    // pre-V3-12b behavior; the ×N callers use respawnPlayerFor).
    respawnPlayerFor(*m_player, *m_sessions[0]);
}

void Game::respawnPlayerFor(Player& p, PlayerSession& s) {
    (void)s;
    // V2-13 (core/bed): the R key's single choke point — the RESPAWN-
    // TIME bed validation (the Minecraft rule: the saved coordinate is
    // re-checked only at the respawn; the bed's break / recolor /
    // reorientation in between do not move it). The remembered spawn
    // (the player's m_spawnPos — the bed cell's center from the last
    // sleep / point-set, or the world spawn for a player who never
    // slept): valid = the cell still holds a bed FAMILY block (the
    // kDyeBaseBed property, color-blind) with room for the player's
    // AABB -> respawn AT the bed; invalid -> the WORLD spawn
    // (m_mobs.spawnPoint — captured at init before the save load, the
    // spawn-protection anchor) and the memory re-pins to it (a broken
    // bed therefore restores the world spawn permanently — the
    // Minecraft rule, the coordinate memory follows the respawn).
    const Vec3 remembered = p.spawnPos();
    const Vec3 target = bedRespawnValid(
        m_world, m_blocks, remembered, p.params().width,
        p.params().height)
        ? remembered
        : m_mobs.spawnPoint;
    // V3-1 Phase B (D8): a NETHER death respawns in the OVERWORLD (the
    // Minecraft-exact ruling — the respawn point is always the
    // overworld's bed / spawn; the nether holds no spawn point, the
    // documented deviation 5: no mob, no spawn-protection need). The
    // rebind + the catch-up disk run BEFORE the position restore (the
    // overworld's world is the respawn's coordinate space). V3-10a: an
    // END death respawns in the OVERWORLD too (the Minecraft "the End
    // can only be left by dying or the exit" ruling — the D8 triple;
    // the End holds no spawn point either).
    if (playerInNether() || playerInEnd()) {
        p.setWorld(m_world);
        m_playerDimension = Dimension::Overworld;
        {
            const int cx = floorDiv(static_cast<int>(std::floor(target.x)),
                                    kChunkSize);
            const int cz = floorDiv(static_cast<int>(std::floor(target.z)),
                                    kChunkSize);
            catchUpDisk(Dimension::Overworld, cx, cz);
        }
        // V3-10a: the End-death return's ambient restore (the config
        // floor — the u_ambient's per-dimension shape; the nether-death
        // leg is a value-identical no-op, the floor is unchanged).
        applyDimensionAmbient();
    }
    p.setSpawnPos(target);
    p.respawn();
}

Game::UiSize Game::uiSize() const {
    const double scale = static_cast<double>(m_uiScale);
    return UiSize {
        std::max(1, static_cast<int>(std::lround(
            static_cast<double>(m_window.logicalWidth()) / scale))),
        std::max(1, static_cast<int>(std::lround(
            static_cast<double>(m_window.logicalHeight()) / scale)))};
}


// V3-12b: the primary delegates (the pre-V3-12b call shapes — solo: the
// only player, bit-identical; the ×N callers use the For forms).
void Game::openEnchantPanel(const BlockPos& pos) {
    openEnchantPanelFor(*m_player, *m_sessions[0], pos);
}

void Game::tryCampfireCook(const BlockPos& pos) {
    tryCampfireCookFor(*m_sessions[0], pos);
}

void Game::regenEnchantOffers() {
    regenEnchantOffersFor(*m_player, *m_sessions[0]);
}

bool Game::applyEnchantOffer(int row) {
    return applyEnchantOfferFor(*m_player, *m_sessions[0], row);
}

void Game::returnEnchantSlot(ItemStack& stack) {
    returnEnchantSlotFor(*m_player, *m_sessions[0], stack);
}

void Game::openPanel(OpenPanel panel) {
    openPanelFor(*m_player, *m_sessions[0], panel);
}

void Game::closePanel() {
    closePanelFor(*m_player, *m_sessions[0]);
}


void Game::openFurnacePanel(const BlockPos& pos) {
    openFurnacePanelFor(*m_player, *m_sessions[0], pos);
}

void Game::openTablePanel(const BlockPos& pos) {
    openTablePanelFor(*m_player, *m_sessions[0], pos);
}

std::pair<int, int> Game::uiMouse() const {
    const Vec2 p = m_input.mousePosition();
    const double scale = static_cast<double>(m_uiScale);
    return {static_cast<int>(std::lround(p.x / scale)),
            static_cast<int>(std::lround(p.y / scale))};
}
// V3-12b: the per-input mouse (the ×N panel pass reads the player's own
// relayed coordinates — R8's window-parity handshake makes the cell
// lands agree; the local UI keeps uiMouse()'s live pump).
std::pair<int, int> Game::uiMouseFor(const Input& in) const {
    const Vec2 p = in.mousePosition();
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
    // V3-12b: the LOCAL player / session (the HUD + hotbar +
    // hearts + tooltip + death screen + hurt veil + F3 all
    // read the machine's own player — solo: the only
    // player, the pre-V3-12b value).
    Player* lplayer = localPlayer();
    PlayerSession& lsess = localSession();
    const bool locked = m_demoPlayer ? true : m_window.relativeMouseMode();
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
    // V2-11 (XP): the XP bar's colors — the dark track (the empty bar,
    // always drawn: level 0 is the track + "0", the MC look) and the
    // fill (the Minecraft yellow-green — the bar's fill AND the level
    // number, the MC same-style green; the orb's world color is the
    // entity-side named constant, the (0,1,0) MC green).
    const Vec4 kXpBarBg {0.0f, 0.0f, 0.0f, 0.65f};
    const Vec4 kXpBarFill {0.5f, 1.0f, 0.2f, 1.0f};
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
    // UX-8e (ui.font_scale): the text-only zoom. Every text scale in
    // this function passes through fScale; the icons, slot rects, panel
    // geometry and the HUD row pitch NEVER do (that independence is the
    // point of the key). fs = 1.0 (the default) is the bit-exact
    // v1.44.0 baseline — s * 1.0 is IEEE-exact, so every vertex, width
    // and line step below stays unchanged.
    const double fs = m_fontScale;
    auto fScale = [fs](double s) { return s * fs; };

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
        // UX-8e: the tooltip text follows the font scale, and its line
        // gap is a TEXT-STACK pitch (the 16 px row metric the panel
        // scales with s), so it grows with the glyphs too (lround —
        // identity at fs = 1.0). The icon metrics stay untouched.
        const double ts = fScale(s);
        UiBatch title;
        title.color = kText;
        title.textured = true;
        title.texture = &m_fontTexture;
        makeTextVertices(tip.title, x, y, ts, title.vertices);
        tooltipBatches.push_back(std::move(title));
        if (!tip.durability.empty()) {
            UiBatch dur;
            dur.color = kTextDim;
            dur.textured = true;
            dur.texture = &m_fontTexture;
            makeTextVertices(tip.durability,
                             x, y + static_cast<int>(std::lround(lineGap * fs)),
                             ts, dur.vertices);
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
        // V3-1 Phase B: the CURRENT world (a player submerged in the
        // nether's lava sea gets the lava overlay there).
        const std::optional<FluidKind> submerged = currentWorld().getFluidKind(
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
            if (static_cast<std::size_t>(i) == lsess.selectedSlot) {
                addBorder(batchFor(kSelected, false, nullptr), slot, 2);
            } else {
                addBorder(batchFor(kBorder, false, nullptr), slot, 1);
            }
            const ItemStack& stack = lsess.inventory.at(i);
            drawIcon(stack, static_cast<float>(sx + kHotbarSlotSize / 2),
                     static_cast<float>(y0 + kHotbarSlotSize / 2),
                     kHotbarIconPx);
            if (!stack.empty() &&
                stack.count < m_items.at(stack.item).maxStack) {
                const std::string count = std::to_string(stack.count);
                addText(count,
                        sx + kHotbarSlotSize - 4 -
                            static_cast<int>(textWidth(count, fScale(1.0))),
                        y0 + kHotbarSlotSize - 4 - kGlyphSize, fScale(1.0),
                        kText);
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
            const float baseMaxHealth = lplayer->params().maxHealth;
            const int heartIcons =
                static_cast<int>(std::ceil(baseMaxHealth / 2.0f));
            const int hungerIcons =
                static_cast<int>(std::ceil(lplayer->maxHunger() / 2.0f));
            const int oxygen = lsess.drown.oxygen;
            const float absorbPool = lplayer->absorption();
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
            drawStrip(hx0, heartIcons, m_healthTexture, lplayer->health(),
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
                          lplayer->hunger(), Vec4 {1.0f, 1.0f, 1.0f, 1.0f});
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
            if (lsess.effects.count() > 0) {
                const int n = static_cast<int>(
                    m_effectTexture.width() / kIconPx);
                UiBatch& batch =
                    batchFor(Vec4 {1.0f, 1.0f, 1.0f, 1.0f}, true,
                             &m_effectTexture);
                int ex = effX0;
                for (const ActiveEffect& a : lsess.effects.list()) {
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
                const ItemStack& worn = lplayer->armorSlot(slot);
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
        drawIcon(lsess.inventory.at(lsess.selectedSlot), heldX, heldY, kHeldIconPx);
        // UX-7: the tutorial target line (centered, one row above the
        // heart row — the effect row / armor row push it up a tier when
        // active, the PRE-10 stacked-rows discipline) + the completion
        // toast (the same slot, the kTutorialToastFrames window armed
        // by the done flip / the T skip). UX-8b: when the current step
        // carries a tip, one more row ABOVE the target line (kTextDim,
        // the same stack pitch — the topmost HUD row, so it cannot
        // collide with the heart / effect / armor rows). Gated: enabled
        // ∧ !dead ∧ (in progress ∨ toast armed) — the death screen
        // releases the pointer, so the locked segment (hence this line)
        // is free of the dead case anyway; the explicit !isDead is the
        // brief's HUD discipline. A no-input run is pointer-free, so
        // nothing here ever draws (structural immunity — the base gate).
        // kText (the primary HUD text color, the death-hint precedent):
        // the target is the primary guidance line; kTextDim (0.55) was
        // too faint against bright terrain for a first-time player.
        if (lsess.tutorialEnabled && !lplayer->isDead() &&
            (!lsess.tutorial.done || lsess.tutorialToastFrames > 0)) {
            const int hy = y0 - kUiIconPx - kUiStripGap; // heart row top
            bool hasArmor = false;
            for (int slot = 0; slot < kArmorSlotCount && !hasArmor; ++slot) {
                hasArmor = !lplayer->armorSlot(slot).empty();
            }
            const int topRow =
                hasArmor ? 2 : (lsess.effects.count() > 0 ? 1 : 0);
            const int tutY =
                hy - topRow * (kUiIconPx + kUiStripGap) - kUiStripGap -
                kGlyphSize;
            const std::string line =
                lsess.tutorial.done
                    ? std::string(kTutorialDoneText)
                    : tutorialTargetLine(lsess.tutorial, m_tutorialSeries);
            addText(line,
                    (w - static_cast<int>(textWidth(line, fScale(1.0)))) / 2,
                    tutY, fScale(1.0), kText);
            // UX-8b: the tip line (the secondary guidance row) — one
            // stack pitch above the main line (the HUD's named row
            // constant, the heart/effect/armor spacing), centered by its
            // own width (no forced equal width), kTextDim (the
            // minor-information color; the main line stays kText).
            // Same gate as the main line, plus !done (the toast owns the
            // slot) and a non-empty tip: a tip-less step adds ZERO rows
            // (bit-identical to v1.41.0).
            if (!lsess.tutorial.done) {
                const std::string tip =
                    tutorialTipLine(lsess.tutorial, m_tutorialSeries);
                if (!tip.empty()) {
                    // UX-8e: the tip/main offset is a TEXT-STACK pitch —
                    // it grows with the glyphs (lround, identity at
                    // fs = 1.0); the icon rows below never do.
                    addText(tip,
                            (w -
                                 static_cast<int>(
                                     textWidth(tip, fScale(1.0)))) /
                                2,
                            tutY - static_cast<int>(std::lround(
                                (kUiIconPx + kUiStripGap) * fs)),
                            fScale(1.0), kTextDim);
                }
            }
        }
    }

    // -- V2-11 (XP): the XP bar + level number -------------------------------
    // ALWAYS visible during play (the documented re-anchor pixel source —
    // gate 182 S1: unlike the hotbar row above it, this does NOT gate on
    // the pointer lock, so the no-input baseline renders it too; a panel
    // hides it, the HUD convention). The Minecraft position: directly
    // below the hotbar row (the hotbar's own geometry — drawn or not),
    // the hotbar's full width, kXpBarHeight (2) px tall. SOLID-FILL
    // implementation (the 10-segment alternative — the documented
    // choice: one background quad + one fill quad, the fill width = the
    // xpIntoLevel / xpNeededForLevel ratio of the bar, exact integer
    // math). The level number floats centered above the bar (the glyph
    // height — the MC look, the digits 0-9 are existing font glyphs,
    // zero new font). The pool is the player's single int
    // (m_xpTotal); the level + fill derive through the pure
    // xp_curve functions (the level is never stored). xpNeededForLevel
    // is always >= 7, so the ratio never divides by zero.
    if (!panelOpen() && lplayer) {
        const int n = Inventory::kHotbarSize;
        const int total = n * kHotbarSlotSize + (n - 1) * kHotbarSlotGap;
        const int x0 = (w - total) / 2;
        const int hotbarY = h - kHotbarBottomMargin - kHotbarSlotSize;
        const int barY = hotbarY + kHotbarSlotSize + kXpBarGap;
        const int xpTotal = lplayer->xpTotal();
        const int level = levelFromTotalXp(xpTotal);
        const int needed = xpNeededForLevel(level);
        const int into = xpIntoLevel(xpTotal, level);
        auto& bg = batchFor(kXpBarBg, false, nullptr);
        addQuad(bg, x0, barY, total, kXpBarHeight);
        const int fillW = (into * total) / needed;
        if (fillW > 0) {
            auto& fill = batchFor(kXpBarFill, false, nullptr);
            addQuad(fill, x0, barY, fillW, kXpBarHeight);
        }
        const std::string lvl = std::to_string(level);
        addText(lvl, w / 2 - static_cast<int>(textWidth(lvl, fScale(1.0))) / 2,
                barY - kGlyphSize, fScale(1.0), kXpBarFill);
    }

    // -- V3-10d (the ender dragon): the top-center boss bar ---------------
    // The !panelOpen + playerInEnd + dragon-present gate (the HUD
    // convention, the XP bar's twin): a no-End session never draws it (the
    // S1's structural-immunity root — the font's 末影龙 glyphs are CJK
    // rows, drawn only here). The 2-s death fade-out reads the ascension's
    // session timer (the render-only alpha; the bar holds its final fill
    // through the fade). The top region is free (the clock is top-right,
    // the F3 is left, the tutorial mid-bottom).
    if (!panelOpen() && lplayer && playerInEnd() &&
        m_end.m_dragon != nullptr) {
        const int f = m_end.m_dragon->ascensionFrames();
        // The fade: alive = 1.0; the ascension's first 120 frames (2 s)
        // linearly out, then the bar is gone (f >= 120 = no draw).
        const float alpha = (f < 0)
                                ? 1.0f
                                : (f >= 120 ? 0.0f
                                            : 1.0f - f / 120.0f);
        if (alpha > 0.0f) {
            constexpr int kBossBarW = 186;
            constexpr int kBossBarH = 8;
            // The top margin leaves room for the name glyph above the
            // bar (the by - kGlyphSize line must stay on-screen — the
            // gate's first-frame crop caught the 8px margin clipping it).
            constexpr int kBossBarTopMargin = 24;
            const int bx = (w - kBossBarW) / 2;
            const int by = kBossBarTopMargin;
            const float hp = m_end.m_dragon->health();
            const float cap =
                m_end.m_dragon->def().properties.health; // 200, the data
            const int fillW = static_cast<int>(
                (hp / cap) * kBossBarW); // the exact-integer ratio read
            const Vec4 kBossBg {0.0f, 0.0f, 0.0f, 0.6f * alpha};
            const Vec4 kBossFill {0.55f, 0.2f, 0.7f, 1.0f * alpha};
            const Vec4 kBossText {1.0f, 1.0f, 1.0f, 1.0f * alpha};
            auto& bg = batchFor(kBossBg, false, nullptr);
            addQuad(bg, bx, by, kBossBarW, kBossBarH);
            if (fillW > 0) {
                auto& fill = batchFor(kBossFill, false, nullptr);
                addQuad(fill, bx, by, fillW, kBossBarH);
            }
            // The 10 segments' dividers (the MC per-10-HP read — the
            // project's 20-HP-segment scale, the D-segment's note).
            auto& div = batchFor(kBossBg, false, nullptr);
            for (int i = 1; i < 10; ++i) {
                addQuad(div, bx + (kBossBarW * i) / 10, by, 1, kBossBarH);
            }
            // The name 末影龙 (the CJK glyphs — the V3-10d font addition,
            // the death-screen's localized-string twin) centered above.
            const std::string name = "末影龙";
            addText(name, bx + kBossBarW / 2 -
                              static_cast<int>(textWidth(name, fScale(1.0))) /
                              2,
                    by - kGlyphSize, fScale(1.0), kBossText);
        }
    }

    // -- death screen (Phase 16, while the player is dead) ------------------
    // The player physics is frozen; the pointer was released on death, so
    // R (any pointer state) respawns.
    if (lplayer && lplayer->isDead()) {
        addQuad(batchFor(Vec4 {0.0f, 0.0f, 0.0f, 0.55f}, false, nullptr), 0,
                0, w, h);
        // PRE-1: the user-visible strings are localized to Chinese
        // (the CJK glyphs live in the font atlas rows 0..3); the F3
        // debug overlay stays English (a developer tool). V3-13: the
        // death STATS screen — the cause line (the killing blow's
        // source, the font-safe string) + the session stats (mined /
        // placed / killed / distance / survival — session only, the
        // documented deviation: they reset with the process) + the
        // R hint. The line step tracks fScale (no overlap at any
        // ui.scale).
        const std::string died = "你死了！";
        const std::string cause = lplayer->deathCauseLine(
            lplayer->deathInfo());
        // V3-19: the survival line reads the DEATH-FRAME snapshot (the
        // alive->dead transition's m_frameCount — the session's, the
        // local player's), not the live m_frameCount: after death the
        // world sim keeps ticking and the live read crawled upward
        // forever (the 11-08 field report). The -1 fallback (never
        // died in this session) = the legacy live read, unreachable
        // here (the screen only draws while dead) but kept for the
        // helper's bit-identity contract.
        const PlayerSession& lsession = *m_sessions[m_localSlot];
        const std::string survived =
            Player::survivalStatsLine(lsession.deathFrame, m_frameCount);
        const std::string mined =
            "采挖 " + std::to_string(m_sessionStats.blocksMined) + " 块";
        const std::string placed =
            "放置 " + std::to_string(m_sessionStats.blocksPlaced) + " 块";
        const std::string killed =
            "击杀 " + std::to_string(m_sessionStats.mobsKilled) + " 只";
        const std::string walked =
            "行走 " + std::to_string(static_cast<int>(lplayer->
                                     statsDistance())) + " 格";
        const std::string hint = "按 R 重生";
        // Fixed physical offsets (the demo panel's precedent — fScale
        // carries the font_scale only; the glyph's ui.scale growth is
        // covered by the 26 px line step at every shipped scale).
        const std::vector<std::tuple<std::string, double, int>> lines = {
            {died, 2.0, h / 2 - 130},
            {cause, 1.0, h / 2 - 56},
            {mined, 1.0, h / 2 - 30},
            {placed, 1.0, h / 2 - 4},
            {killed, 1.0, h / 2 + 22},
            {walked, 1.0, h / 2 + 48},
            {survived, 1.0, h / 2 + 74},
            {hint, 1.0, h / 2 + 100},
        };
        for (const auto& [text, scale, y] : lines) {
            const bool title = scale > 1.0;
            addText(text, w / 2 -
                         static_cast<int>(textWidth(text, fScale(scale))) / 2,
                    y, fScale(scale),
                    title ? Vec4 {1.0f, 0.35f, 0.35f, 1.0f}
                          : (text == hint ? kText : kTextDim));
        }
    }

    // -- furnace panel (Phase 40/44): the smelting GUI (right-clicked
    // furnace) — rendering the ONE furnace block the panel is bound to
    // (its per-block state). The 36 inventory slots + the smelting
    // section (input, progress bar, output, fuel). No crafting of any
    // kind happens here — the output fills by itself (the furnace tick).
    if (localPanel().openPanel == OpenPanel::Furnace) {
        const FurnaceState& f = furnaceAt(localPanel().openFurnacePos);
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
                fScale(InventoryLayout::kTitleFontScale * s), kText);

        const int hoverSlot = layout.slotAt(mx, my);
        const int hoverFslot = layout.furnaceSlotAt(mx, my);

        // The 36 inventory slots (drawn like the crafting panels').
        for (std::size_t i = 0; i < layout.slots.size(); ++i) {
            const UIRect& r = layout.slots[i];
            addQuad(batchFor(kSlotBg, false, nullptr), r.x, r.y, r.w, r.h);
            if (i == lsess.selectedSlot) {
                addBorder(batchFor(kSelected, false, nullptr), r, 2);
            } else if (static_cast<int>(i) == hoverSlot) {
                addBorder(batchFor(kHover, false, nullptr), r, 2);
            } else {
                addBorder(batchFor(kBorder, false, nullptr), r, 1);
            }
            const ItemStack& stack = lsess.inventory.at(i);
            drawIcon(stack, static_cast<float>(r.x + r.w / 2),
                     static_cast<float>(r.y + r.h / 2), slotIcon);
            if (!stack.empty() &&
                stack.count < m_items.at(stack.item).maxStack) {
                const std::string count = std::to_string(stack.count);
                addText(count,
                        r.x + r.w - L(4) -
                            static_cast<int>(textWidth(count, fScale(s))),
                        r.y + r.h - L(4) - L(kGlyphSize), fScale(s), kText);
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
                        r.x + r.w - L(4) -
                            static_cast<int>(textWidth(count, fScale(s))),
                        r.y + r.h - L(4) - L(kGlyphSize), fScale(s), kText);
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
            const ItemStack& stack = lsess.inventory.at(hoverSlot);
            if (!stack.empty()) {
                // PRE-12 (tooltip): name + " xN" (count > 1) + the
                // d/max durability readout line (the pure
                // ui/item_tooltip; the Phase-15 "only the name" residue).
                appendTooltip(itemTooltip(m_items.at(stack.item), stack),
                              mx + L(12), my + L(12), s, L(16));
            }
        }
        if (localPanel().furnaceUi.carrying()) {
            drawIcon(localPanel().furnaceUi.carry(), static_cast<float>(mx),
                     static_cast<float>(my), slotIcon);
        }
        const std::string hint = "按 E 关闭"; // PRE-1
        addText(hint,
                layout.panel.x + static_cast<int>(std::lround(
                    (layout.panel.w - textWidth(hint, fScale(s))) / 2.0)),
                layout.panel.y + layout.panel.h - L(20), fScale(s), kTextDim);
    }

    // -- V2-12: the enchanting panel (the 3-offer GUI) ----------------------
    if (localPanel().openPanel == OpenPanel::Enchant) {
        const EnchantLayout layout = EnchantLayout::compute(w, h);
        const double s = layout.scale;
        const auto L = [s](int v) {
            return static_cast<int>(std::lround(v * s));
        };
        const int slotIcon = L(34);
        const int titleOffset = L(4);

        addQuad(batchFor(kPanel, false, nullptr), layout.panel.x,
                layout.panel.y, layout.panel.w, layout.panel.h);
        addText("附魔台", layout.title.x, layout.title.y + titleOffset,
                fScale(InventoryLayout::kTitleFontScale * s), kText);

        const int hoverSlot = layout.slotAt(mx, my);
        const int hoverPslot = layout.panelSlotAt(mx, my);

        // The 36 inventory slots (drawn like the furnace's).
        for (std::size_t i = 0; i < layout.slots.size(); ++i) {
            const UIRect& r = layout.slots[i];
            addQuad(batchFor(kSlotBg, false, nullptr), r.x, r.y, r.w, r.h);
            if (i == lsess.selectedSlot) {
                addBorder(batchFor(kSelected, false, nullptr), r, 2);
            } else if (static_cast<int>(i) == hoverSlot) {
                addBorder(batchFor(kHover, false, nullptr), r, 2);
            } else {
                addBorder(batchFor(kBorder, false, nullptr), r, 1);
            }
            const ItemStack& stack = lsess.inventory.at(i);
            drawIcon(stack, static_cast<float>(r.x + r.w / 2),
                     static_cast<float>(r.y + r.h / 2), slotIcon);
            if (!stack.empty() &&
                stack.count < m_items.at(stack.item).maxStack) {
                const std::string count = std::to_string(stack.count);
                addText(count,
                        r.x + r.w - L(4) -
                            static_cast<int>(textWidth(count, fScale(s))),
                        r.y + r.h - L(4) - L(kGlyphSize), fScale(s), kText);
            }
        }

        // The item + lapis slots.
        const UIRect* pRects[2] = {&layout.item, &layout.lapis};
        const ItemStack* pSlots[2] = {&localPanel().enchantItemSlot,
                                      &localPanel().enchantLapisSlot};
        for (int i = 0; i < 2; ++i) {
            const UIRect& r = *pRects[i];
            addQuad(batchFor(kSlotBg, false, nullptr), r.x, r.y, r.w, r.h);
            if (i == hoverPslot) {
                addBorder(batchFor(kHover, false, nullptr), r, 2);
            } else {
                addBorder(batchFor(kBorder, false, nullptr), r, 1);
            }
            const ItemStack& stack = *pSlots[i];
            drawIcon(stack, static_cast<float>(r.x + r.w / 2),
                     static_cast<float>(r.y + r.h / 2), slotIcon);
            if (!stack.empty() &&
                stack.count < m_items.at(stack.item).maxStack) {
                const std::string count = std::to_string(stack.count);
                addText(count,
                        r.x + r.w - L(4) -
                            static_cast<int>(textWidth(count, fScale(s))),
                        r.y + r.h - L(4) - L(kGlyphSize), fScale(s), kText);
            }
        }

        // The three offer rows: "name roman xpCost" (the first enchant's
        // display name + its level's roman + the row's xp cost). An empty
        // offer (no enchants) draws a dimmed placeholder.
        const EnchantOffer* offers[3] = {&localPanel().enchantOffers.top,
                                         &localPanel().enchantOffers.mid,
                                         &localPanel().enchantOffers.bottom};
        for (int r = 0; r < 3; ++r) {
            const UIRect& rect = layout.offers[r];
            addQuad(batchFor(kSlotBg, false, nullptr), rect.x, rect.y,
                    rect.w, rect.h);
            if (2 + r == hoverPslot) {
                addBorder(batchFor(kHover, false, nullptr), rect, 2);
            } else {
                addBorder(batchFor(kBorder, false, nullptr), rect, 1);
            }
            const EnchantOffer& o = *offers[r];
            std::string label;
            Vec4 color = kTextDim;
            if (!o.enchants.empty()) {
                // The map is id-sorted; the FIRST entry is the display.
                const auto& first = *o.enchants.begin();
                const Enchantment* e = m_enchants.find(first.first);
                const std::string name =
                    e != nullptr ? e->name : first.first;
                label = name + " " + effectTierRoman(first.second - 1) +
                        " " + std::to_string(o.xpCost);
                // Affordable (the four gates minus the "not yet enchanted"
                // — the display brightens when the player could apply it):
                const int poolLevel = levelFromTotalXp(lplayer->xpTotal());
                const int lapisCount = localPanel().enchantLapisSlot.empty()
                    ? 0 : localPanel().enchantLapisSlot.count;
                const bool affordable =
                    poolLevel >= o.xpCost && lapisCount >= o.lapisCost &&
                    !localPanel().enchantItemSlot.empty() &&
                    m_items.at(localPanel().enchantItemSlot.item).enchantability >
                        0;
                color = affordable ? kText : kTextDim;
            } else {
                label = "—";
            }
            addText(label, rect.x + L(6),
                    rect.y + (rect.h - L(kGlyphSize)) / 2, fScale(s),
                    color);
        }

        // The hovered slot's item name + the carried stack + the close hint.
        if (hoverSlot >= 0) {
            const ItemStack& stack = lsess.inventory.at(hoverSlot);
            if (!stack.empty()) {
                appendTooltip(itemTooltip(m_items.at(stack.item), stack),
                              mx + L(12), my + L(12), s, L(16));
            }
        }
        if (localPanel().enchantUi.carrying()) {
            drawIcon(localPanel().enchantUi.carry(), static_cast<float>(mx),
                     static_cast<float>(my), slotIcon);
        }
        const std::string hint = "按 E 关闭"; // PRE-1
        addText(hint,
                layout.panel.x + static_cast<int>(std::lround(
                    (layout.panel.w - textWidth(hint, fScale(s))) / 2.0)),
                layout.panel.y + layout.panel.h - L(20), fScale(s), kTextDim);
    }

    // -- crafting panel (while open): the E inventory (2x2 grid + the
    // shapeless recipe craft list) or the crafting table (3x3 grid + the
    // shaped recipe hint list, CRAFT-4). ------------------------------------
    if (panelOpen() && localPanel().openPanel != OpenPanel::Furnace &&
        localPanel().openPanel != OpenPanel::Enchant) {
        const bool table = localPanel().openPanel == OpenPanel::Table;
        const PanelKind kind =
            table ? PanelKind::CraftingTable : PanelKind::Inventory;
        // The hint list per kind (same source — and, for the E panel, the
        // same UX-8c orderedRecipeHints seam — handlePanelClicks uses, so
        // a row index means the same recipe for the click and the draw).
        std::vector<const Recipe*> hints =
            table ? shapedRecipes() : shapelessRecipes();
        if (!table) {
            hints = orderedRecipeHints(hints, lsess.inventory, m_items);
        }
        const InventoryLayout layout =
            InventoryLayout::compute(w, h, static_cast<int>(hints.size()),
                                     kind);
        InventoryUI& ui = table ? localPanel().tableUi : localPanel().inventoryUi;

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
        // V3-11b: the creative's E-panel face (the E kind only — the
        // table panel is a survival-style face for everyone): the title
        // row becomes two tab buttons, and the 物品栏 tab swaps the
        // content for the search box + the grant grid. The 背包 tab's
        // content IS the ordinary face below (bit-identical to the
        // survival's — the creative's delta is the title row + the
        // 物品栏 tab only).
        const bool creative = !table && lplayer->isCreative();
        const bool ordinaryFace = !creative || localPanel().creativeTab == 1;
        if (creative) {
            const CreativeLayout cl =
                CreativeLayout::compute(w, h, static_cast<int>(hints.size()));
            const std::string tabNames[2] = {"物品栏", "背包"};
            for (int t = 0; t < 2; ++t) {
                const UIRect& tr = t == 0 ? cl.tabItems : cl.tabBackpack;
                addQuad(batchFor(kSlotBg, false, nullptr), tr.x, tr.y, tr.w,
                        tr.h);
                addBorder(
                    batchFor(t == localPanel().creativeTab ? kSelected : kBorder,
                             false, nullptr),
                    tr, 2);
                const std::string& name = tabNames[static_cast<std::size_t>(t)];
                addText(name, tr.x + (tr.w - textWidth(name, fScale(s))) / 2,
                        tr.y + (tr.h - L(kGlyphSize)) / 2, fScale(s), kText);
            }
            if (!ordinaryFace) {
                // The 物品栏 tab: the search box (the typed text + the
                // frame-locked cursor blink, the portal phase's twin) +
                // the 6x4 grant grid + the two hint rows.
                const std::string& text = localPanel().creativeUi.text();
                addQuad(batchFor(kSlotBg, false, nullptr), cl.search.x,
                        cl.search.y, cl.search.w, cl.search.h);
                addBorder(batchFor(kBorder, false, nullptr), cl.search, 1);
                const bool placeholder = text.empty();
                const std::string shown = placeholder ? "输入匹配" : text;
                const int textY =
                    cl.search.y + (cl.search.h - L(kGlyphSize)) / 2;
                const int textX = cl.search.x + L(6);
                addText(shown, textX, textY, fScale(s),
                        placeholder ? kTextDim : kText);
                if ((m_frameCount % 30) < 15) { // the cursor's blink
                    addText("_", textX + textWidth(shown, fScale(s)) + L(2),
                            textY, fScale(s), kTextDim);
                }
                const std::vector<ItemID>& list = localPanel().creativeUi.filtered();
                const int start = localPanel().creativeUi.visibleStart();
                const int hoverCell = cl.cellAt(mx, my);
                for (int i = 0; i < CreativeLayout::kVisible; ++i) {
                    const UIRect& r = cl.cells[static_cast<std::size_t>(i)];
                    addQuad(batchFor(kSlotBg, false, nullptr), r.x, r.y, r.w,
                            r.h);
                    addBorder(
                        batchFor(i == hoverCell ? kHover : kBorder, false,
                                 nullptr),
                        r, 2);
                    const int idx = start + i;
                    if (idx < static_cast<int>(list.size())) {
                        drawIcon(ItemStack {list[static_cast<std::size_t>(idx)],
                                            1, 0.0f, {}},
                                 static_cast<float>(r.x + r.w / 2),
                                 static_cast<float>(r.y + r.h / 2), slotIcon);
                    }
                }
                // The hint rows (every CJK char in the 428-glyph set —
                // the PITS 397 char-check discipline, zero new glyphs).
                const std::string hint1 = "点击 = 放入选中热栏一整组 · 右键 = 1 个";
                const std::string hint2 = "滚轮 = 切换 · 输入 = 匹配";
                const int hintY = cl.cells.back().y + cl.cells.back().h + L(8);
                addText(hint1, cl.tabItems.x, hintY, fScale(s), kTextDim);
                addText(hint2, cl.tabItems.x, hintY + L(18), fScale(s),
                        kTextDim);
                if (hoverCell >= 0) {
                    const int idx = start + hoverCell;
                    if (idx < static_cast<int>(list.size())) {
                        // The count-1 stack renders the title only (the
                        // x-count line's >1 gate — the name's clean read).
                        const ItemID hid =
                            list[static_cast<std::size_t>(idx)];
                        appendTooltip(
                            itemTooltip(m_items.at(hid),
                                        ItemStack {hid, 1, 0.0f, {}}),
                            mx + L(12), my + L(12), s, L(16));
                    }
                }
                const std::string close = "按 E 关闭"; // PRE-1
                addText(close,
                        layout.panel.x + static_cast<int>(std::lround(
                            (layout.panel.w - textWidth(close, fScale(s))) /
                            2.0)),
                        layout.panel.y + layout.panel.h - L(20), fScale(s),
                        kTextDim);
            }
        } else {
            addText(table ? "工作台" : "背包", layout.title.x,
                    layout.title.y + titleOffset,
                    fScale(InventoryLayout::kTitleFontScale * s), kText);
        }
        if (ordinaryFace) {
        // The list header ("合成") now serves both panels (CRAFT-4: the
        // table panel's hint column).
        addText("合成", layout.craftHeader.x,
                layout.craftHeader.y + titleOffset,
                fScale(InventoryLayout::kTitleFontScale * s), kText);

        const int hoverSlot = layout.slotAt(mx, my);
        const int hoverCell = layout.gridCellAt(mx, my);
        const bool hoverOutput = layout.inOutput(mx, my);
        for (std::size_t i = 0; i < layout.slots.size(); ++i) {
            const UIRect& r = layout.slots[i];
            addQuad(batchFor(kSlotBg, false, nullptr), r.x, r.y, r.w, r.h);
            if (i == lsess.selectedSlot) {
                addBorder(batchFor(kSelected, false, nullptr), r, 2);
            } else if (static_cast<int>(i) == hoverSlot) {
                addBorder(batchFor(kHover, false, nullptr), r, 2);
            } else {
                addBorder(batchFor(kBorder, false, nullptr), r, 1);
            }
            const ItemStack& stack = lsess.inventory.at(i);
            drawIcon(stack, static_cast<float>(r.x + r.w / 2),
                     static_cast<float>(r.y + r.h / 2), slotIcon);
            if (!stack.empty() &&
                stack.count < m_items.at(stack.item).maxStack) {
                const std::string count = std::to_string(stack.count);
                addText(count,
                        r.x + r.w - L(4) -
                            static_cast<int>(textWidth(count, fScale(s))),
                        r.y + r.h - L(4) - L(kGlyphSize), fScale(s), kText);
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
                        r.x + r.w - L(4) -
                            static_cast<int>(textWidth(count, fScale(s))),
                        r.y + r.h - L(4) - L(kGlyphSize), fScale(s), kText);
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
                // The output stack: the recipe's result, or (UX-5: no
                // recipe matched) the repaired tool itself — the tool
                // cell's item + count at the match's reduced damage.
                // drawIcon's durability bar reads the stack's damage,
                // so the bar shows on the repair output for free.
                ItemStack out;
                if (m.recipe != nullptr) {
                    out = ItemStack {m.recipe->output,
                                     m.recipe->outputCount};
                } else {
                    const ItemStack& cell =
                        ui.grid().at(m.repair->itemSlot);
                    out = ItemStack {cell.item, cell.count,
                                     m.repair->outputDamage};
                }
                drawIcon(out, static_cast<float>(r.x + r.w / 2),
                         static_cast<float>(r.y + r.h / 2), slotIcon);
                if (out.count < m_items.at(out.item).maxStack) {
                    const std::string count = std::to_string(out.count);
                    addText(count,
                            r.x + r.w - L(4) -
                                static_cast<int>(textWidth(count, fScale(s))),
                            r.y + r.h - L(4) - L(kGlyphSize), fScale(s),
                            kText);
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
                const bool can = canSupply(lsess.inventory, patternBag(recipe));
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
                    fitLabel(m_items.at(recipe.output).name, suffix, fScale(s),
                             std::max(0, r.w - L(89) - L(4))) +
                    suffix;
                addText(label, r.x + L(89), r.y + L(18), fScale(s), outColor);
            }
        } else {
            for (std::size_t i = 0; i < layout.recipes.size(); ++i) {
                const Recipe& recipe = *hints[static_cast<std::size_t>(i)];
                const UIRect& r = layout.recipes[i];
                const bool can = canCraft(lsess.inventory, recipe, m_items);
                const Vec4 outColor = can ? kText : kTextDim;
                drawIcon(ItemStack {recipe.output, recipe.outputCount},
                         static_cast<float>(r.x + L(14)),
                         static_cast<float>(r.y + r.h / 2), rowIcon);
                // UX-8c: the two text lines fit the (now half-width)
                // column — the " xN" suffix is kept, a long item id or a
                // long ingredient list is tail-trimmed so it never bleeds
                // into the other column (deterministic, see fitLabel — the
                // table panel's hint rows use the same discipline).
                const int avail = std::max(0, r.w - L(32) - L(4));
                const std::string suffix =
                    " x" + std::to_string(recipe.outputCount);
                const std::string label =
                    fitLabel(m_items.at(recipe.output).name, suffix,
                             fScale(s), avail) +
                    suffix;
                addText(label, r.x + L(32), r.y + L(8), fScale(s), outColor);
                std::string needs;
                for (const auto& [item, count] : recipe.ingredients) {
                    if (!needs.empty()) {
                        needs += ", ";
                    }
                    needs += m_items.at(item).name + " x" +
                             std::to_string(count);
                }
                // PRE-1 (item ids stay English); the "需要：" prefix is the
                // string head, so fitLabel trims the ingredient tail only.
                addText(fitLabel("需要：" + needs, "", fScale(s), avail),
                        r.x + L(32), r.y + L(28), fScale(s), kTextDim);
            }
        }

        // The hovered slot/cell's item name + the carried stack at the
        // cursor.
        if (hoverSlot >= 0) {
            const ItemStack& stack = lsess.inventory.at(hoverSlot);
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
                    (layout.panel.w - textWidth(hint, fScale(s))) / 2.0)),
                layout.panel.y + layout.panel.h - L(20), fScale(s), kTextDim);
        } // the ordinaryFace (the 背包 tab / the survival's face)
    }

    // -- V3-12c1: the net's CHAT overlay (the 8-line ring's bottom-
    // left — the frame stamp's 300-frame = 5-s fade; the Latin's
    // zero-glyph face; the input prompt's caret line while active) ----
    if (netActive() && (!m_chatLines.empty() || m_chatInputActive)) {
        const int chatLineH = 16;
        const int chatX = kF3Margin;
        const int nLines =
            static_cast<int>(m_chatLines.size()) + (m_chatInputActive ? 1 : 0);
        int chatY = h - kF3Margin - nLines * chatLineH;
        for (const ChatLine& cl : m_chatLines) {
            const int age = m_frameCount - cl.frame;
            if (age <= 300) {
                addText(cl.who + ": " + cl.text, chatX, chatY, 1.0f, kText);
            }
            chatY += chatLineH;
        }
        if (m_chatInputActive) {
            addText("> " + m_chatBuffer + "_", chatX, chatY, 1.0f, kText);
        }
    }

    // -- V3-12c1: the NAMEPLATE (the remote player's Latin name above
    // the head — the lobby's render face; the world->screen
    // projection's the 3D pass's same fov / near / far, the shared
    // constants' single source; the local player's no plate, the
    // first-person's absence). -------------------------------
    if (netActive() && m_cameraMode != CameraMode::First) {
        const Mat4 projection = Mat4::perspective(
            kFovDegrees * static_cast<float>(M_PI) / 180.0f,
            static_cast<float>(w) / static_cast<float>(h), kNear, kFar);
        const Mat4 view = m_camera.viewMatrix();
        const Mat4 vp = Mat4::multiply(projection, view);
        for (std::size_t i = 0; i < m_players.size(); ++i) {
            if (i == static_cast<std::size_t>(m_localSlot)) {
                continue; // the local player's own plate = the first
                           // person's absence (the MC's twin)
            }
            const Player& pl = *m_players[i];
            const Vec3 head =
                pl.position() + Vec3 {0.0f, pl.params().height + 0.3f, 0.0f};
            const Vec3 ndc = vp.transformPoint(head);
            if (ndc.z < -1.0f || ndc.z > 1.0f) {
                continue; // behind the camera / past the far plane
            }
            const int sx =
                static_cast<int>(std::lround((ndc.x + 1.0f) * 0.5f * w));
            const int sy =
                static_cast<int>(std::lround((1.0f - ndc.y) * 0.5f * h));
            const std::string name =
                (i < m_playerNames.size()) ? m_playerNames[i]
                                           : ("player" + std::to_string(i));
            const int half = static_cast<int>(name.size()) * 4;
            addText(name, sx - half, sy - 10, 1.0f, kText);
        }
    }

    // -- F3 debug overlay (while toggled) ------------------------------------
    if (m_debugOverlay) {
        DebugInfo info;
        // V3-12b: the F3 readout is the LOCAL player (each machine's
        // own view; solo: local == primary, the pre-V3-12b value).
        Player* lp = localPlayer();
        PlayerSession& s = localSession();
        info.fps = static_cast<int>(std::lround(m_fpsSmoothed));
        info.frame = m_frameCount;
        info.drawCalls = m_renderer.stats().drawCalls;
        info.vertices = m_renderer.stats().vertices;
        // V3-1 Phase B: the CURRENT dimension's world / entity list (the
        // F3 readout is the dimension the player is in).
        info.chunks = static_cast<int>(currentWorld().chunkCount());
        if (lp) {
            info.posX = lp->position().x;
            info.posY = lp->position().y;
            info.posZ = lp->position().z;
            info.yaw = lp->yaw;
            info.pitch = lp->pitch;
        }
        if (lp) {
            info.columnX =
                floorDiv(static_cast<int>(std::floor(lp->position().x)),
                         kChunkSize);
            info.columnZ =
                floorDiv(static_cast<int>(std::floor(lp->position().z)),
                         kChunkSize);
        }
        info.seed = m_generator ? m_generator->seed() : 0;
        info.timeOfDay = m_dayCycle.timeOfDay();
        info.dayFactor = m_dayCycle.dayFactor();
        // V2-4: the weather line (the F3 stays English — the developer-
        // tool discipline): the machine's current segment, "clear" /
        // "rain" (render.weather off = the machine never runs, so the
        // line reads "clear" there too).
        info.weather = m_weather.raining ? "rain" : "clear";
        info.streamingEngaged = m_streaming.streamingEngaged;
        info.streamPending = static_cast<int>(m_streaming.streamPending.size());
        info.streamRadius = m_streaming.streamRadius;
        info.pointerLocked = locked;
        info.inventoryOpen = panelOpen();
        info.particles = m_particles.size();
        info.entities = currentEntities().size();
        if (lp) {
            info.health = lp->health();
            info.maxHealth = lp->maxHealth();
            info.absorption = lp->absorption();
            info.armorPoints = lp->armorPoints(); // SURV-1
            info.oxygen = s.drown.oxygen; // SURV-2
            info.hunger = lp->hunger();
            info.maxHunger = lp->maxHunger();
        }
        // SURV-3: the held item's line — "(none)" on an empty hand, the
        // item name otherwise, plus " damage/durability" when the item
        // has one (the d/max readout; a full-durability held item shows
        // 0.0).
        {
            const ItemStack& held = s.inventory.at(s.selectedSlot);
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
        for (const ActiveEffect& a : s.effects.list()) {
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
        info.stats = s.stats;
        // UX-7 / V2-3: the tutorial line (the F3 stays English — the
        // developer-tool discipline): "off" when disabled, "done" when
        // finished or skipped, "<series+1>/<total> <id>
        // <progress>/<count>" while in progress (the series position is
        // 1-based, e.g. "2/3 seed 0/1").
        if (!s.tutorialEnabled) {
            info.tutorial = "off";
        } else if (s.tutorial.done) {
            info.tutorial = "done";
        } else if (s.tutorial.seriesIndex >= 0 &&
                   s.tutorial.seriesIndex <
                       static_cast<int>(m_tutorialSeries.size()) &&
                   s.tutorial.stepIndex >= 0 &&
                   s.tutorial.stepIndex <
                       static_cast<int>(
                           m_tutorialSeries[s.tutorial.seriesIndex].size())) {
            const TutorialState& tut = s.tutorial;
            const TutorialStep& stp =
                m_tutorialSeries[tut.seriesIndex]
                                   [static_cast<std::size_t>(
                                       tut.stepIndex)];
            char buf[96];
            std::snprintf(buf, sizeof(buf), "%d/%d %s %d/%d",
                          tut.seriesIndex + 1,
                          static_cast<int>(m_tutorialSeries.size()),
                          stp.id.c_str(), tut.progress, stp.count);
            info.tutorial = buf;
        }
        // V2-19: the camera line (the F3 stays English — the developer-
        // tool discipline): the mode name + the third-person orbit
        // distance (the wheel-zoom readout; First shows the session value
        // too — it only APPLIES to the third views) + the fly marker.
        info.camMode =
            m_cameraMode == CameraMode::ThirdBack
                ? "ThirdBack"
                : (m_cameraMode == CameraMode::ThirdFront ? "ThirdFront"
                                                          : "First");
        info.camDistance = m_thirdPersonDistance;
        info.flying = lp ? lp->isFlying() : false;
        // V3-8: the gamepad line (the F3 stays English — the developer-
        // tool discipline): the Input's connection state (the single
        // consumer of the pump's gamepad events) — "gamepad: none" when
        // no gamepad is connected, the SDL device name otherwise.
        info.gamepadConnected = m_input.gamepadConnected();
        info.gamepadName = m_input.gamepadName();
        // V3-1 Phase B: the dimension line (the F3's 20th line — the
        // developer-tool discipline: the F3 stays English). V3-10a:
        // the three-way (the "end" value — the F3 pin family's +1).
        info.dimension =
            playerInNether() ? "nether"
            : playerInEnd() ? "end" : "overworld";
        // V3-12c1: the players line (the lobby's developer face — the
        // Latin names; the solo's "solo" baseline).
        if (netActive()) {
            info.players = std::to_string(m_players.size()) + ": ";
            for (std::size_t i = 0; i < m_players.size(); ++i) {
                if (i > 0) {
                    info.players += "; ";
                }
                info.players +=
                    (i < m_playerNames.size())
                        ? m_playerNames[i]
                        : ("player" + std::to_string(i));
            }
        }
        // V3-10d2: the dragon line (the boss's session-state readout —
        // the End only; a no-End session renders the "(none)" baseline,
        // the F3's English developer-tool discipline).
        if (playerInEnd() && m_end.world) {
            if (m_end.m_dragonSummonCountdown >= 0) {
                info.dragon = "summon " +
                    std::to_string(m_end.m_dragonSummonCountdown) + "f";
            } else if (m_end.m_dragon != nullptr) {
                if (m_end.m_dragon->dead()) {
                    info.dragon = "ascend " +
                        std::to_string(m_end.m_dragon->ascensionFrames()) +
                        "/" + std::to_string(kDragonAscentTotalFrames);
                } else {
                    info.dragon =
                        m_end.m_dragon->state() == MobState::Wander
                            ? "orbit"
                            : (m_end.m_dragon->state() ==
                                     MobState::RangedAttack
                                   ? "strafe"
                                   : "perch");
                }
            } else if (m_end.m_dragonSpawnDelay >= 0) {
                info.dragon = "spawn " +
                    std::to_string(m_end.m_dragonSpawnDelay) + "f";
            } else if (m_end.world->getBlock(
                           static_cast<int>(std::floor(kEndEggPos.x)),
                           static_cast<int>(std::floor(kEndEggPos.y)),
                           static_cast<int>(std::floor(kEndEggPos.z))) ==
                       m_states.dragonEggId) {
                info.dragon =
                    m_end.m_eggRespawnFrame >= 0
                        ? "egg respawn " +
                              std::to_string(m_end.m_eggRespawnFrame -
                                             m_frameCount) +
                          "f"
                        : "egg";
            } else {
                info.dragon = "none";
            }
        }
        // UX-8e: the F3 line step is a TEXT-STACK pitch — it grows with
        // the glyphs (lround, identity at fs = 1.0); the margin (an
        // icon-space inset) does not.
        const int f3Step =
            static_cast<int>(std::lround(kF3LineHeight * fs));
        int y = kF3Margin;
        for (const std::string& line : debugOverlayLines(info)) {
            addText(line, kF3Margin, y, fScale(1.0), kText);
            y += f3Step;
        }
    }

    // -- V2-19: the HUD clock (ALWAYS VISIBLE) --------------------------------
    // The 24-hour time of day (DayCycle::clockString over the frame-locked
    // time — the pure mapping the unit tests own: dawn edge 06:00, noon
    // 12:00, dusk 18:00, night center 00:00), white, top-right on the
    // F3-margin inset (the hotbar/heart-row coordinate paradigm: the 8 px
    // HUD margin, the 16 px glyph). "Always visible" = the user's request
    // (the vanilla Minecraft has no HUD clock — the documented new
    // feature): it draws in EVERY pointer/panel/death state, on top of
    // everything (it is the last batch added before the upload). A
    // no-input run draws it too — the frame-locked time makes the pixels
    // a deterministic function of the frame count (the base gate's
    // re-anchor is the clock region only, S1; 1 game minute = 25 frames
    // at the shipped 600 s day, so the anchor reads the frame-60
    // frozen value).
    {
        const std::string clock = DayCycle::clockString(m_dayCycle.timeOfDay());
        addText(clock,
                w - static_cast<int>(textWidth(clock, fScale(1.0))) - kF3Margin,
                kF3Margin, fScale(1.0), kText);
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

    // -- V3-9: the demo replay's audience UI (replay mode only — a normal
    // run executes ZERO of this block, the structural immunity). The
    // batches are SELF-CONTAINED: each quad / text line gets its OWN
    // UiBatch (no batchFor merge into the main `batches` — a merge would
    // draw the demo's text at an EXISTING same-color batch's vector
    // position, under the veil), and they are flushed HERE — AFTER the
    // main 2D flush + the 3D block-cube icons (the demo player can open
    // a panel mid-replay: the veil sits over every player-panel element
    // including the cube icons), BEFORE the cursor tooltip + the effect
    // tints + the hurt flash (their LAST-of-UI-passes discipline keeps
    // them above). The badge (the corner "DEMO x<speed>" line — the
    // ASCII 'x' stands for the multiplication sign, the font carries no
    // U+00D7) is added LAST = above the veil (the death-screen's HUD-over-
    // veil precedent); it sits BELOW the top-right clock (the clock's
    // 16 px line at y 8..24, the badge at y 28). The strings are ENGLISH
    // (the font's CJK set carries no demo vocabulary — the F3 overlay's
    // English developer-tool precedent); the panel = the pause /
    // finished state (the ESC's replay semantics: the pause MENU, not
    // the exit; Q quits).
    if (m_demoMode == DemoMode::Replay) {
        std::vector<UiBatch> demoBatches;
        auto demoQuad = [&demoBatches, &addQuadUv](const Vec4& color, int x,
                                                   int y, int qw, int qh) {
            UiBatch b;
            b.color = color;
            addQuadUv(b, x, y, qw, qh, Vec2 {0.0f, 0.0f}, Vec2 {1.0f, 1.0f});
            demoBatches.push_back(std::move(b));
        };
        auto demoText = [this, &demoBatches](const std::string& text, int x,
                                             int y, double scale,
                                             const Vec4& color) {
            UiBatch b;
            b.color = color;
            b.textured = true;
            b.texture = &m_fontTexture;
            makeTextVertices(text, x, y, scale, b.vertices);
            demoBatches.push_back(std::move(b));
        };
        if (m_demoPaused || (m_demoEnded && m_demoEndPanelOpen)) {
            demoQuad(Vec4 {0.0f, 0.0f, 0.0f, 0.55f}, 0, 0, w, h);
            const std::string title =
                m_demoPaused ? "Demo paused" : "Demo finished";
            const std::string name = demoFileName(m_demoPath);
            // v3.13.0: the informational recorded-with line (the sim
            // gate's message carries it too — the build string is
            // display-only, it never rejects). Its own line: a
            // combined name + recorded-with line overflows the 679 px
            // gate window at ui.scale 2 (16 px glyphs x2 = 32 px/char).
            const std::string recorded = "recorded with " +
                                         m_args.demoFile->recordedWith;
            const std::string speed =
                "Speed " + demoSpeedLabel(m_demoSpeed);
            const std::string hint =
                m_demoPaused ? "ESC to resume - Q to quit"
                             : "ESC to keep watching - Q to quit";
            const double s2 = fScale(2.0);
            const double s1 = fScale(1.0);
            demoText(title,
                     w / 2 - static_cast<int>(textWidth(title, s2)) / 2,
                     h / 2 - 68, s2, kText);
            demoText(name, w / 2 - static_cast<int>(textWidth(name, s1)) / 2,
                     h / 2 - 36, s1, kTextDim);
            demoText(speed,
                     w / 2 - static_cast<int>(textWidth(speed, s1)) / 2,
                     h / 2 - 8, s1, kText);
            demoText(recorded,
                     w / 2 - static_cast<int>(textWidth(recorded, s1)) / 2,
                     h / 2 + 16, s1, kTextDim);
            demoText(hint,
                     w / 2 - static_cast<int>(textWidth(hint, s1)) / 2,
                     h / 2 + 40, s1, kTextDim);
        }
        // The badge (always, above the veil when one is up).
        const std::string badge = "DEMO " + demoSpeedLabel(m_demoSpeed);
        demoText(badge,
                 w - static_cast<int>(textWidth(badge, fScale(1.0))) - kF3Margin,
                 28, fScale(1.0), kTextDim);
        // The flush (this position = after the main 2D flush + the 3D
        // cube icons, before the tooltip / tints / hurt flash).
        for (auto& b : demoBatches) {
            drawUiBatch(b.vertices, ortho, b.textured ? *b.texture
                                                      : m_fontTexture,
                        b.textured, b.color);
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
            activeEffectTints(lsess.effects, m_effects);
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
    if (lsess.hurtFlash > 0.0) {
        const float a =
            0.35f * static_cast<float>(lsess.hurtFlash / kHurtFlashSeconds);
        UiBatch flash;
        flash.color = {1.0f, 0.0f, 0.0f, a};
        addQuad(flash, 0, 0, w, h);
        drawUiBatch(flash.vertices, ortho, m_fontTexture, false,
                    flash.color);
    }
}

} // namespace mc
