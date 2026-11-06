#include "core/game.hpp"

#include <algorithm>
#include <cmath>

#include "block/block_sound.hpp"
#include "block/chance_drop.hpp"
#include "block/drop_scatter.hpp"
#include "core/bow.hpp"
#include "core/fly.hpp"
#include "core/game_local.hpp"
#include "core/third_person.hpp"
#include "item/armor.hpp"
#include "item/durability.hpp"
#include "world/growth.hpp"

namespace mc {

namespace {

// How far the crosshair raycast reaches (blocks; Minecraft's reach).
constexpr float kReach = 5.0f;

// Mobs (Phase 20): the player's attack cadence + the knockback a MELEE hit
// applies (the horizontal speed along the aim + a vertical lift; the
// MobEntity's knockback window decays it). Phase 42: an ARROW hit knocks
// back with core/bow.hpp arrowKnockback — the same base values, scaled by
// the shot's charge power.
constexpr double kSwingInterval = 0.25;  // seconds between attack hits

constexpr float kKnockbackSpeed = 4.0f;  // horizontal knockback (blocks/s)

constexpr float kKnockbackLift = 2.5f;   // vertical knockback (blocks/s)

// Phase 15 sound cadence (seconds).
constexpr double kDigSoundInterval = 0.3;

} // namespace

float Game::aimReach() const {
    // V2-1: the front camera sits IN FRONT of the eye (it is the
    // player's visual eye in that view), so the eye ray gets the D
    // compensation — the full Minecraft reach behind the player (the MC
    // front-view convention). V2-19: D is the wheel-zoomed session
    // distance (m_thirdPersonDistance — zoom in = reach the near, zoom
    // out = reach the far, the front view's convention at any zoom; the
    // default kThirdPersonDistance keeps the pre-V2-19 value). First /
    // ThirdBack return the legacy kReach BIT-IDENTICALLY (the same
    // constexpr value the pre-V2-1 call sites passed as literals — the
    // structural-immunity root: no-input and back-view runs are
    // unchanged). The aim RAYS still leave from the eye (UX-8g,
    // untouched — only the budget moves with the camera).
    return m_cameraMode == CameraMode::ThirdFront
               ? m_thirdPersonDistance + kReach
               : kReach;
}

Vec3 Game::aimForward() const {
    // V3-9: the aim ray's direction — every world-facing ray (the break
    // / the attack's knockback / the mob / the bow / the bucket / the
    // slab hit point) reads this. The NORMAL path = the camera's forward
    // (the pre-V3-9 expression, bit-identically — the function's only
    // cost is the mode check). The REPLAY = the mode's BASE view (the
    // audience's orbit is RENDER-ONLY — it re-resolves the camera but
    // must not steer the demo player's aim: the input-routing red line,
    // the demo stays the pure function of its file). The ThirdFront's
    // flip is the pre-V3-9 camera's own (the screen-center aim, V2-1).
    if (m_demoMode == DemoMode::Replay) {
        float yaw = m_player->yaw;
        float pitch = m_player->pitch;
        if (m_cameraMode == CameraMode::ThirdFront) {
            const ThirdPersonAngles fa = thirdPersonFrontAngles(yaw, pitch);
            yaw = fa.yaw;
            pitch = fa.pitch;
        }
        const float cp = std::cos(pitch);
        return {std::sin(yaw) * -cp, std::sin(pitch), std::cos(yaw) * -cp};
    }
    return m_camera.forward();
}

void Game::handleInteraction(double dt) {
    // CRAFT-3: the listener for the spatialized sounds (the material dig /
    // place / break at the block, the pickup at the item) — the camera
    // state at the frame start. The camera was synced from the player at
    // the END of the previous frame (the player physics run after this in
    // the frame), so the listener is one frame stale: zero cost, zero
    // non-determinism — audio is outside the frame state (hard rule 8).
    // The 2D paths (step / shoot / eat / ui_*, the held-item breaks)
    // ignore the listener entirely.
    m_sound.setListener(m_camera.position, m_camera.yaw);
    const bool locked = m_window.relativeMouseMode();
    if (!locked) {
        // Left click captures the pointer (standard first-person behavior)
        // — but never while dead (the death screen owns the input; R
        // respawns first).
        if (!m_player->isDead() && m_input.wasPressed(MouseButton::Left)) {
            m_window.setRelativeMouseMode(true);
            // Debug line: the E2E input scripts verify the capture landed
            // by waiting for this (a covering window can steal the click
            // silently — P25-class pollution, 2026-10-21 STRM-1 E2E).
            MC_LOG_DEBUG("Pointer captured");
        }
        resetBreaking();
        m_interaction.swingCooldown = 0.0;
        resetBowDraw();
        resetEatChannel();
        return;
    }

    if (m_input.wasPressed(GameKey::Escape)) {
        m_window.setRelativeMouseMode(false);
        resetBreaking();
        m_interaction.swingCooldown = 0.0;
        resetBowDraw();
        resetEatChannel();
        return;
    }
    // V2-19: the double-tap Space toggles the player's FLY (the creative-
    // style toggle, "the Chinese can fly" — faster-than-walk exploration;
    // the physics live in Player, the session state on the player). The
    // double-tap window is REAL dt input state (the CRAFT-7
    // kDoubleClickSeconds pattern — never frame state, never saved: a
    // no-input run never presses Space = the structural-immunity root).
    // Locked pointer only (the C-key pattern — the panel frees the
    // pointer, so a panel-open press never reaches here; the death
    // screen owns its input), alive only (the toggle does not fight the
    // frozen body). The decision is the pure core/fly function; a SINGLE
    // tap only records its time (the jump is exactly the pre-V2-19 one),
    // a DOUBLE tap flips the fly and swallows the second tap's jump
    // grant (Player::suppressJumpGrant — the creative's toggle consumes
    // it: on the toggle-ON frame the fly branch owns Space, on the
    // toggle-OFF frame the grant gate does).
    if (m_input.wasPressed(GameKey::Space) && !m_player->isDead()) {
        const double now = nowSeconds();
        const FlyToggleDecision d =
            flyToggleDecision(now, m_lastSpacePressTime,
                              m_player->isFlying());
        m_lastSpacePressTime = now;
        if (d.doubleTap) {
            m_player->setFlying(d.nowFlying);
            m_player->suppressJumpGrant();
        }
    }
    // V3-8b: the gamepad's DEDICATED flight toggle (the B/EAST button —
    // GameKey::FlyToggle is gamepad-only, mapKey never returns it): a
    // SINGLE press flips the fly, no double-tap window, no jump grant
    // to swallow (FlyToggle is not Space, so no jump is granted). Same
    // guards as the Space toggle (the locked-pointer section, alive
    // only). The A's double-tap through the Space's path still works —
    // the documented redundancy (the user's ruling: the dedicated
    // button, the keyboard convention untouched).
    if (m_input.wasPressed(GameKey::FlyToggle) && !m_player->isDead()) {
        m_player->setFlying(!m_player->isFlying());
    }
    if (m_input.wasPressed(MouseButton::Right)) {
        // Campfire cooking (Phase 71): a campfire under the crosshair
        // owns the click BEFORE the item channels — Minecraft places the
        // held food on the fire instead of starting to eat (essential:
        // every smeltable item in this project is a food, so without the
        // precedence the cooking slot could never be filled). The click
        // is consumed either way (a campfire never places through the
        // right button).
        // V3-1 Phase B: the campfire's cooking state is OVERWORLD-ONLY
        // (the D9 closed-set ruling — the nether holds no campfire
        // state; a nether campfire is a plain block the player cannot
        // cook on: the branch is skipped, the click falls through to
        // the item channels).
        if (!playerInNether()) {
            if (const std::optional<BlockHit> hit = targetBlock(); hit) {
                const Block& b = m_blocks.at(
                    currentWorld().getBlock(hit->block.x, hit->block.y,
                                             hit->block.z));
                if (b.properties.interact == kInteractCampfire) {
                    tryCampfireCook(hit->block);
                    return;
                }
            }
        }
        // Food (Phase 23): a selected food item is EATEN, not used — the
        // click is consumed (no interact, no placement). At full hunger the
        // eat is a no-op but the click is still consumed (Minecraft: you
        // cannot place while "eating" a full bar).
        if (tryEat()) {
            return;
        }
        // Drinkable container (Phase 43): a selected item with a
        // 'drink_time' is DRUNK, not used — the same hold channel as the
        // eat (data-driven duration); completing it consumes one, returns
        // the empty container and clears the status effects (v1: the hurt
        // flash). The click is consumed either way (no interact, no
        // placement — a milk bucket in hand never places).
        if (tryDrink()) {
            return;
        }
        // Launcher (Phase 24): the right button is the DRAW. Starting it
        // consumes the click entirely (no interact, no placement — a bow
        // in hand never places). The draw starts only when ammo is
        // available (Minecraft: no draw without arrows).
        if (selectedShootSpeed() > 0.0f) {
            if (ammoAvailable()) {
                m_interaction.bowDrawing = true;
                m_interaction.bowDrawSeconds = 0.0;
                m_sound.play("bow");
            }
            return;
        }
        // Milking (Phase 43, SURV-10): a mob whose data pairs
        // 'milk_with' with the selected item's name, under the crosshair
        // (melee targeting: kReach + block occlusion), is MILKED — one
        // held item consumed, one 'milk_gives' item added, the mob
        // unharmed (no damage, death or drop) but put on the 5-minute
        // Minecraft milking cooldown (re-milking early does nothing; the
        // click is still consumed, the failed-milk rule).
        if (tryMilk()) {
            return;
        }
        // V2-15 (sheep): shearing — a selected item carrying the 'shears'
        // data flag on a mob whose data carries 'wool_drops' (the instance
        // still woolly) shears the mob: 1-3 of the instance-color wool
        // drop at the mob (ground items, the Q-drop precedent), the mob
        // enters the bare regrow window, the shears wear kShearWear and
        // the "shear" cue plays at the mob. The bare state is a no-op
        // that still consumes the click (Minecraft, the failed-milk rule).
        if (tryShear()) {
            return;
        }
        // V2-15 (sheep): the mob dye — a selected DYE (dyeColor
        // non-empty — the data flag, the block recolor's predicate) on a
        // wool-drops mob recolors the instance wool: one dye consumed +
        // the "cloth" cue (the block recolor's sound twin). Minecraft: a
        // sheared (bare) mob refuses the dye — the click is still
        // consumed.
        if (tryDyeMob()) {
            return;
        }
        // Data-driven interaction (Phase 17): a block whose `interact`
        // property names a known GUI opens it instead of placing.
        if (!playerInNether()) {
            // V3-1 Phase B: the container panels (the per-block states
            // are OVERWORLD-ONLY — the D9 closed-set ruling: opening a
            // nether container would bind an overworld-state entry to a
            // nether position, the cross-dimension key collision; the
            // nether's container state is Phase C).
            if (const std::optional<BlockHit> hit = targetBlock(); hit) {
                const Block& b = m_blocks.at(
                    currentWorld().getBlock(hit->block.x, hit->block.y,
                                             hit->block.z));
                if (b.properties.interact == kInteractCraftingTable) {
                    // CRAFT-1: the panel is bound to THIS block's grid
                    // (per-block table states — the Phase 44 furnace
                    // pattern).
                    openTablePanel(hit->block);
                    return;
                }
                if (b.properties.interact == kInteractFurnace) {
                    // Phase 44: the panel is bound to THIS block's state.
                    openFurnacePanel(hit->block);
                    return;
                }
                if (b.properties.interact == kInteractEnchant) {
                    // V2-12: the panel rolls its offers for THIS block's
                    // position (bookshelfCount around it) + the item the
                    // player places in the slot (session state, the
                    // furnace twin).
                    openEnchantPanel(hit->block);
                    return;
                }
                // The campfire (Phase 71) is handled at the TOP of this
                // handler, before the item channels (see above).
            }
        }
        // Bed sleep (V2-13, core/bed): a bed FAMILY block (the
        // kDyeBaseBed dye_base property — the family check, never a
        // color id) under the crosshair attempts the sleep: the three
        // gates (distance / no hostile nearby / the night window)
        // decide between the immediate night skip and the bare
        // respawn-point set (the Minecraft day rule). The click is
        // consumed either way (no placement fall-through). A HELD DYE
        // does not sleep: the bed is a dye family like the wool, so
        // the Phase-41 recolor path below owns the click instead.
        // V3-1 Phase B: the sleep (the respawn point is the OVERWORLD's
        // bed — the D8 ruling: a respawn is always the overworld; a
        // nether bed is the documented no-op — the Minecraft nether-bed
        // explosion is Phase C, the deviation-8 family).
        if (!playerInNether()) {
            if (const std::optional<BlockHit> hit = targetBlock(); hit) {
                const Block& b = m_blocks.at(
                    currentWorld().getBlock(hit->block.x, hit->block.y,
                                             hit->block.z));
                if (b.properties.dyeBase == kDyeBaseBed) {
                    const ItemStack& held = m_inventory.at(m_selectedSlot);
                    if (held.empty() ||
                        m_items.at(held.item).dyeColor.empty()) {
                        trySleep(*hit);
                        return;
                    }
                }
            }
        }
        // Dye recoloring (Phase 41): a selected dye item right-clicked on
        // a dye-family block converts the block to the dyed variant
        // (dyeVariantName) and consumes one dye — instant (no channel,
        // Minecraft) and the click is consumed (no placement). A block
        // already in that color is left alone and the click falls through
        // (the dye is not a block item, so placeBlock is a no-op — the
        // documented v1 rule: a duplicate dye is not consumed).
        if (const std::optional<BlockHit> hit = targetBlock(); hit) {
            const Block& b = m_blocks.at(
                currentWorld().getBlock(hit->block.x, hit->block.y,
                                         hit->block.z));
            if (!b.properties.dyeBase.empty()) {
                const ItemStack& held = m_inventory.at(m_selectedSlot);
                if (!held.empty() &&
                    !m_items.at(held.item).dyeColor.empty()) {
                    const std::string variant =
                        dyeVariantName(b.properties.dyeBase,
                                       m_items.at(held.item).dyeColor);
                    if (variant != b.name) {
                        // Game init (family completeness) guarantees the
                        // variant block exists; the null guard is a
                        // defensive no-op.
                        if (const Block* v = m_blocks.findByName(variant);
                            v != nullptr) {
                            // Game::setBlock (Phase 42): also releases any
                            // arrow stuck in the recolored cell.
                            setBlock(hit->block.x, hit->block.y,
                                     hit->block.z, v->id);
                            Lighting::updateLocal(currentWorld(), m_blocks,
                                                  hit->block);
                            m_inventory.take(m_selectedSlot, 1);
                            m_sound.play("cloth");
                        }
                        return;
                    }
                }
            }
        }
        // Fluid bucket (Phase 47): a selected bucket side collects a
        // source cell or pours one — the click is consumed either way
        // (a bucket never places a block). Runs AFTER the block
        // interactions: a furnace or crafting table under the crosshair
        // still opens its panel even when a bucket is selected (Minecraft).
        if (tryFluidBucket()) {
            return;
        }
        // Ignition (Phase 60, fire visuals Phase 64, the relight target
        // Phase 76): a selected item with the 'ignites_flamable' flag
        // acts on the block under the crosshair — (a) a block with a
        // non-empty `ignites_to` is set to THAT block in place (the
        // RELIGHT: campfire_extinguished -> campfire, its cooking state
        // survives the setBlock choke point and resumes — Minecraft:
        // flint and steel relights an unlit campfire), (b) otherwise a
        // `flamable` block burns — setBlock(FIRE), NO drops (the Phase 58
        // lava-burn semantics). Neither consumes the item (Minecraft: the
        // flint and steel survives its uses) and both play "ignite". The
        // fire block is a light source that burns out after
        // fire.lifetime_seconds (the setBlock choke point starts its
        // countdown) and is put out by water. Runs AFTER the panel
        // interactions: a furnace or crafting table under the crosshair
        // still opens its panel even when an igniter is selected. Other
        // targets fall through (the item is a material, so placeBlock is
        // a no-op).
        if (const ItemStack& held = m_inventory.at(m_selectedSlot);
            !held.empty() && m_items.at(held.item).ignitesFlamable) {
            if (const std::optional<BlockHit> hit = targetBlock(); hit) {
                const Block& b = m_blocks.at(
                    currentWorld().getBlock(hit->block.x, hit->block.y,
                                             hit->block.z));
                if (!b.properties.ignitesTo.empty()) {
                    // (a) the relight target (Phase 76): set the named
                    // block in place. The name is init-validated (the
                    // grows_to discipline), so idForName cannot fail.
                    const BlockID toId =
                        m_blocks.idForName(b.properties.ignitesTo);
                    setBlock(hit->block.x, hit->block.y, hit->block.z,
                             toId);
                    Lighting::updateLocal(currentWorld(), m_blocks,
                                          hit->block);
                    m_sound.play("ignite");
                    // SURV-3: the successful relight wears the held
                    // igniter one point (the action completed — a
                    // flint and steel that breaks is simply gone).
                    if (m_items.at(held.item).durability > 0.0f) {
                        if (wearItem(m_inventory.at(m_selectedSlot),
                                     kIgniteWear)) {
                            m_sound.play("break"); // it broke on the
                                                   // successful ignition
                        }
                    }
                } else if (b.properties.flamable) {
                    // (b) m_states.fireId is valid here: a flamable target
                    // exists, and the init cross-check requires the fire
                    // block whenever the registry holds one.
                    setBlock(hit->block.x, hit->block.y, hit->block.z,
                             m_states.fireId);
                    Lighting::updateLocal(currentWorld(), m_blocks,
                                          hit->block);
                    m_sound.play("ignite");
                    // SURV-3: the successful burn wears the held igniter
                    // one point (the action completed).
                    if (m_items.at(held.item).durability > 0.0f) {
                        if (wearItem(m_inventory.at(m_selectedSlot),
                                     kIgniteWear)) {
                            m_sound.play("break"); // it broke on the
                                                   // successful ignition
                        }
                    }
                } else if (m_states.obsidianId.valid() &&
                           m_states.portalId.valid() &&
                           b.id == m_states.obsidianId) {
                    // (c) V3-1 Phase A: the portal ignition — the ray
                    // can never aim at the frame's interior air directly
                    // (the obsidian ring is coplanar with it and
                    // occludes it): the hit is the ring cell, and the
                    // FIRE lands in the hit face's neighbor along the
                    // normal — when that neighbor is the STRICTLY-
                    // INTERIOR air of a complete frame (the slab
                    // placement's hit-face-adjacent-cell pattern). The
                    // player aims the ring cell's face TOWARD the frame's
                    // interior. The choke point auto-activates on the
                    // fire placement (the setBlock below triggers
                    // checkPortalTransition). An obsidian hit with no
                    // complete frame owning the neighbor = no reaction
                    // (the material's placeBlock no-op fall-through, the
                    // existing behavior bit-invariant).
                    const BlockPos adj {hit->block.x +
                                            static_cast<int>(std::lround(
                                                hit->normal.x)),
                                        hit->block.y +
                                            static_cast<int>(std::lround(
                                                hit->normal.y)),
                                        hit->block.z +
                                            static_cast<int>(std::lround(
                                                hit->normal.z))};
                    if (currentWorld().getBlock(adj.x, adj.y, adj.z) ==
                        BlockID::air()) {
                        // The hit cell is on the candidate frames'
                        // perimeter (or corner) by the ring-obsidian
                        // premise; the first (deterministic-order)
                        // complete frame whose strictly-interior holds
                        // the neighbor — and no portal (an already-live
                        // frame's interior is portal, ignition refused) —
                        // takes the fire.
                        const std::vector<PortalFrameStatus> cands =
                            scanPortalFrames(currentWorld(), hit->block,
                                             m_states.obsidianId,
                                             m_states.fireId,
                                             m_states.portalId);
                        for (const PortalFrameStatus& st : cands) {
                            if (!st.ringComplete || st.interiorPortal != 0 ||
                                !portalFrameStrictlyInterior(st.frame, adj)) {
                                continue;
                            }
                            setBlock(adj.x, adj.y, adj.z, m_states.fireId);
                            Lighting::updateLocal(currentWorld(), m_blocks,
                                                  adj);
                            m_sound.play("ignite");
                            // SURV-3: the successful ignition wears the
                            // held igniter one point (the action
                            // completed — the (a) / (b) twin).
                            if (m_items.at(held.item).durability > 0.0f) {
                                if (wearItem(m_inventory.at(m_selectedSlot),
                                             kIgniteWear)) {
                                    m_sound.play("break"); // it broke on
                                                           // the ignition
                                }
                            }
                            break;
                        }
                    }
                }
            }
        }
        // Armor (SURV-1): a held piece equips (an occupied slot swaps
        // it with the held one), an empty hand unequips the first worn
        // slot (the fixed order head -> chest -> legs -> feet) — the
        // click is consumed whenever it acts (armor is a material,
        // placeBlock would be a no-op; the channel owns the right
        // button, so it runs BEFORE the sneak-rotate: equipping is an
        // item action even while sneaking — Minecraft). Runs AFTER the
        // item channels + the igniter: a campfire under the crosshair
        // still cooks (it owns the click at the top of the handler),
        // food still eats, a bucket still scoops, an igniter still
        // lights (the channel-ownership precedence, unchanged).
        if (tryArmor()) {
            return;
        }
        // Block-state rotation (Phase 92 stair ring, per-instance in
        // Phase 94, generalized in ENT-2): a SNEAKING right-click on a
        // stateful block (hasState()) rotates it IN PLACE — the per-instance
        // state (the World's per-cell byte, the project's first per-block
        // state) advances (state + 1) % stateCount (a stair: (facing + 1) %
        // 4, the shipped behavior unchanged). The click is consumed (no
        // placement). Minecraft: sneak + right-click rotates a stair. Runs
        // AFTER the item channels + panel interactions: a furnace/crafting
        // table under the crosshair still opens its panel; a stateful block
        // (no panel) rotates. (Phase 92 used the block-ID ring — the 4
        // variants; Phase 94 replaced it with the per-instance facing;
        // ENT-2 generalizes the gate to hasState.)
        if (m_player->isCrouching()) {
            if (const std::optional<BlockHit> hit = targetBlock(); hit) {
                const Block& b = m_blocks.at(
                    currentWorld().getBlock(hit->block.x, hit->block.y,
                                             hit->block.z));
                // V2-17 deviation 4: a SLAB's state is its VERTICAL
                // position (bottom / top / double), not an orientation —
                // the (s+1)%count cycle would destroy material (a
                // state-2 double slab cycled to 0 vanishes a half block),
                // so the rotation gate excludes the Slab shape: a
                // crouching right-click on a slab PLACES instead (the
                // Minecraft behavior — the stairs' orientation ring is
                // untouched, the state-0 default is unchanged).
                if (b.hasState() && b.model.shape != BlockModel::Shape::Slab) {
                    const int cur = currentWorld().getBlockState(hit->block.x,
                                                          hit->block.y,
                                                          hit->block.z);
                    // Unset (the sentinel, e.g. a hand-edited load) falls
                    // back to the block's state default — the same rule as
                    // the collision / mesh consumers.
                    const int s =
                        (cur != Chunk::kBlockStateUnset) ? cur
                                                         : b.stateDefault();
                    currentWorld().setBlockState(hit->block.x, hit->block.y,
                                          hit->block.z,
                                          static_cast<std::uint8_t>(
                                              (s + 1) % b.stateCount()));
                    // CRAFT-3: the rotated block's material timbre at the
                    // block center (no material = the generic "place").
                    m_sound.play(
                        blockSoundName(b, "place"),
                        Vec3{static_cast<float>(hit->block.x) + 0.5f,
                             static_cast<float>(hit->block.y) + 0.5f,
                             static_cast<float>(hit->block.z) + 0.5f});
                    return; // the rotation consumes the click (no placement)
                }
            }
        }
        placeBlock();
    }
    if (m_input.wasPressed(GameKey::Tab)) {
        cycleHotbar();
    }
    if (m_input.wasPressed(GameKey::Q)) {
        dropSelectedItem();
    }
    // Left button (Phases 11/20/21/24): a launcher item selected (the bow
    // — data 'shoot_speed' > 0) MELEE-attacks a mob under the crosshair
    // (one hit per kSwingInterval while held — the item's data
    // attack_damage, the bow's 1) but NEVER mines (Minecraft: the bow is
    // a weapon, not a tool; the shot is the right-button draw).
    // Otherwise: on a mob under the crosshair the swing attacks it
    // (damage + knockback, the block behind it is NOT mined); on a block
    // it mines (hold-to-break progress). Leaving either target resets
    // the other's state.
    if (m_input.isDown(MouseButton::Left)) {
        // A swing cancels an in-progress eat (Phase 28, Minecraft) — the
        // food is not consumed; the swing itself proceeds normally.
        if (m_interaction.eatChannel.active) {
            resetEatChannel();
        }
        if (selectedShootSpeed() > 0.0f) {
            resetBreaking();
            if (const std::optional<MobHit> mob = targetMob()) {
                updateAttacking(dt, *mob->mob);
            } else {
                m_interaction.swingCooldown = 0.0;
            }
        } else if (const std::optional<MobHit> mob = targetMob()) {
            resetBreaking();
            updateAttacking(dt, *mob->mob);
        } else {
            m_interaction.swingCooldown = 0.0;
            updateBreaking(dt);
        }
    } else {
        resetBreaking();
        m_interaction.swingCooldown = 0.0;
    }
    // Draw charge (Phase 24): with a launcher selected, the held right
    // button accumulates draw time (real dt, like the survival timers)
    // and the release fires one arrow with the charge's power.
    if (selectedShootSpeed() > 0.0f) {
        updateBowDraw(dt);
    }
    // Eat channel (Phase 28): the held right button fills the 1.6 s eat
    // (real dt, like the survival timers); completion consumes the food.
    // No-op while inactive (food and launcher are exclusive in the
    // selected slot, so the two channels never overlap).
    updateEatChannel(dt);
}

std::optional<BlockHit> Game::targetBlock() const {
    // UX-8g: the aim ray leaves from the EYE in both camera modes
    // (Minecraft semantics: the screen center IS the eye's line of
    // sight, and the player never occludes its own aim). First person:
    // game_frame keeps m_camera.position == eyePosition bit-exactly
    // (the First branch is the legacy assignment), so the origin is
    // unchanged there. Third person: the camera sits
    // kThirdPersonDistance behind the eye on the SAME line (yaw/pitch
    // are the player's), so the old camera origin ate 4 blocks of the
    // kReach budget (effective reach 1) and made the nearest hit the
    // player's own head area — the crosshair-on-itself bug.
    // V2-1: the front view reuses this same ray with the camera's own
    // angles: the origin stays the eye, the direction is the camera's
    // forward = the player's BACKWARD (the screen-center direction —
    // the region behind the player the front camera looks at), and the
    // reach is kThirdPersonDistance + kReach (aimReach, the MC
    // front-view convention: the front camera is the player's visual
    // eye, D from the real eye). First / ThirdBack: aimReach() = kReach,
    // bit-identical to the pre-V2-1 literal.
    return raycastBlock(currentWorld(), m_blocks, m_player->eyePosition(),
                        aimForward(), aimReach());
}

void Game::resetBreaking() {
    m_interaction.breakTarget.reset();
    m_interaction.breakProgress = 0.0;
    m_interaction.lastDigProgress = 0.0;
}

void Game::updateBreaking(double dt) {
    const std::optional<BlockHit> hit = targetBlock();
    if (!hit) {
        resetBreaking();
        return;
    }
    const Block& block =
        m_blocks.at(currentWorld().getBlock(hit->block.x, hit->block.y,
                                            hit->block.z));
    // Breakability is data-driven: water/bedrock set destroyable=false.
    if (!block.properties.destroyable) {
        resetBreaking();
        return;
    }
    if (!m_interaction.breakTarget.has_value() || *m_interaction.breakTarget != hit->block) {
        m_interaction.breakTarget = hit->block;
        m_interaction.breakProgress = 0.0;
    }
    const double duration = breakDurationSeconds(
        block.properties.hardness, selectedBreakSpeed(block));
    if (duration <= 0.0) {
        // Data-driven instant break (hardness 0 while destroyable).
        breakBlock(*hit);
        resetBreaking();
        return;
    }
    m_interaction.breakProgress += dt;
    // Mining tick (Phase 15): a dig sound every kDigSoundInterval of held
    // progress (the crack overlay renders the same progress, see render).
    // CRAFT-3: the broken block's material timbre at the block center
    // (no material = the generic "dig").
    const Vec3 digPos {static_cast<float>(hit->block.x) + 0.5f,
                       static_cast<float>(hit->block.y) + 0.5f,
                       static_cast<float>(hit->block.z) + 0.5f};
    while (m_interaction.breakProgress >= m_interaction.lastDigProgress + kDigSoundInterval) {
        m_sound.play(blockSoundName(block, "dig"), digPos);
        m_interaction.lastDigProgress += kDigSoundInterval;
    }
    if (m_interaction.breakProgress >= duration) {
        breakBlock(*hit);
        resetBreaking();
    }
}

std::optional<Game::MobHit> Game::targetMob() const {
    // The block occludes: the nearer of the block hit and the mob AABB
    // hits wins (a mob behind the aimed block is not hit; a mob in front
    // of the block is). V2-1: the initial budget is aimReach() (the
    // block hit that sets bestT comes from targetBlock with the SAME
    // budget, so the occlusion comparison stays consistent in every
    // mode; First / ThirdBack: aimReach() = kReach, bit-identical).
    float bestT = aimReach();
    if (const std::optional<BlockHit> b = targetBlock()) {
        bestT = b->distance;
    }
    MobEntity* best = nullptr;
    // UX-8g: the origin is the eye in both modes, matching targetBlock
    // (the block hit that sets the initial bestT is measured from the
    // SAME point, so the block-vs-mob occlusion comparison stays
    // consistent; first person is bit-identical as above).
    const Vec3 o = m_player->eyePosition();
    const Vec3 d = aimForward();
    for (const auto& entity : currentEntities()) {
        const MobEntity* mob =
            dynamic_cast<const MobEntity*>(entity.get());
        if (mob == nullptr || mob->dead()) {
            continue;
        }
        const float t =
            raycastAabb(o, d, mob->aabbMin(), mob->aabbMax());
        if (t >= 0.0f && t < bestT) {
            bestT = t;
            best = const_cast<MobEntity*>(mob);
        }
    }
    if (best == nullptr) {
        return std::nullopt;
    }
    return MobHit {best, bestT};
}

void Game::updateAttacking(double dt, MobEntity& mob) {
    if (m_interaction.swingCooldown > 0.0) {
        m_interaction.swingCooldown -= dt;
        return;
    }
    // A swing landed: data damage (selected item) + knockback along the
    // aim (horizontal) with a lift. Removal + drops happen in the
    // updateEntities despawn pass (same frame).
    m_interaction.swingCooldown = kSwingInterval;
    mob.damage(selectedAttackDamage());
    // SURV-3 (durability): the held TOOL wears two points per melee hit
    // that LANDS (the event table: pickaxe / axe / shovel +2). A bow's
    // melee swing does not wear (it is a material, not a tool — its wear
    // is the shot, the fireBow site), and a bare hand has no item. The
    // hit completed (the damage + knockback landed) — a tool that breaks
    // on this swing is simply gone.
    {
        ItemStack& held = m_inventory.at(m_selectedSlot);
        if (!held.empty()) {
            const Item& heldItem = m_items.at(held.item);
            if (heldItem.type == ItemType::Tool &&
                heldItem.durability > 0.0f) {
                if (wearItem(held, kToolMeleeWear)) {
                    m_sound.play("break"); // the tool broke on this hit
                }
            }
        }
    }
    const Vec3 f = aimForward();
    Vec3 kb {f.x, 0.0f, f.z};
    const float len = std::sqrt(kb.x * kb.x + kb.z * kb.z);
    if (len > 1e-4f) {
        kb.x *= kKnockbackSpeed / len;
        kb.z *= kKnockbackSpeed / len;
    }
    kb.y = kKnockbackLift;
    mob.addKnockback(kb);
    m_sound.play("hurt");
}

float Game::selectedAttackDamage() const {
    const ItemStack& stack = m_inventory.at(m_selectedSlot);
    const float base =
        stack.empty() ? 1.0f  // bare hand
                      : m_items.at(stack.item).attackDamage;  // data (1.0)
    // V2-12: SHARPNESS adds (L+1) melee damage points (the held pure
    // accessor — 0 for a bare hand / an unenchantable / an unenchanted
    // item, so the pre-V2-12 melee is bit-identical).
    const int sharp = sharpnessLevel(stack, m_enchants);
    const float sharpBonus =
        sharp > 0 ? static_cast<float>(sharp + 1) : 0.0f;
    // Phase 46: the active weakness effects subtract their melee penalty;
    // Phase 84: the strength effects ADD their melee bonus (the pair — the
    // net is clamped at 0, so a fully weakened hand deals no damage even
    // with no strength).
    return std::max(0.0f, base + sharpBonus -
                               m_playerEffects.totalMeleePenalty(m_effects) +
                               m_playerEffects.totalMeleeBonus(m_effects));
}

float Game::selectedShootSpeed() const {
    const ItemStack& stack = m_inventory.at(m_selectedSlot);
    if (stack.empty()) {
        return 0.0f;  // no launcher
    }
    return m_items.at(stack.item).shootSpeed;  // data (0 = not a launcher)
}

bool Game::ammoAvailable() const {
    const ItemStack& stack = m_inventory.at(m_selectedSlot);
    if (stack.empty()) {
        return false; // not a launcher (selectedShootSpeed guards callers)
    }
    const Item& item = m_items.at(stack.item);
    if (item.ammo.empty()) {
        return true; // no 'ammo' data = unlimited (pre-Phase-24 semantics)
    }
    const ItemID ammo = m_items.idForName(item.ammo); // init cross-checked
    for (std::size_t i = 0; i < m_inventory.size(); ++i) {
        if (m_inventory.at(i).item == ammo) {
            return true;
        }
    }
    return false;
}

void Game::updateBowDraw(double dt) {
    if (!m_interaction.bowDrawing) {
        return;
    }
    if (m_input.isDown(MouseButton::Right)) {
        // The charge fills in real dt, capped at the full-draw time
        // (core/bow.hpp). A press and a release in the same frame still
        // fire (at the minimum power), like a Minecraft quick click.
        m_interaction.bowDrawSeconds =
            std::min(m_interaction.bowDrawSeconds + dt, kBowFullDrawSeconds);
    } else if (m_input.wasReleased(MouseButton::Right)) {
        fireBow();
    } else {
        // The button is up without a release edge this frame (a stale
        // event, or the lock was lost): cancel the draw rather than hang
        // it.
        resetBowDraw();
    }
}

void Game::fireBow() {
    ItemStack& stack = m_inventory.at(m_selectedSlot);
    const Item& item = m_items.at(stack.item);
    // V2-12: the held bow's enchant bonuses (the pure accessors — 0 for an
    // unenchanted bow, the pre-V2-12 bit-identical shot).
    const float powerBonus = static_cast<float>(powerLevel(stack, m_enchants));
    const float speedBonus = static_cast<float>(speedLevel(stack, m_enchants));
    const bool infinite =
        item.infinite || heldInfinite(stack, m_enchants);
    // Ammo (Phase 24): one from the lowest-index slot that holds the
    // launcher's data 'ammo' item. The inventory can run out between the
    // draw start and the release — then the shot is cancelled silently.
    // Phase 77 (Infinity): an `infinite` launcher is still gated on
    // holding at least one (found below) but consumes NONE — the one
    // arrow is never spent (Minecraft's bow Infinity enchantment,
    // data-driven here). V2-12: the INFINITE ENCHANT is the field's twin
    // (field OR enchant — the infinite_bow / master_bow variants keep
    // working, the documented coexistence).
    if (!item.ammo.empty()) {
        const ItemID ammo = m_items.idForName(item.ammo);
        int slot = -1;
        for (std::size_t i = 0; i < m_inventory.size(); ++i) {
            if (m_inventory.at(i).item == ammo) {
                slot = static_cast<int>(i);
                break;
            }
        }
        if (slot < 0) {
            resetBowDraw();
            return;
        }
        if (!infinite) {
            m_inventory.take(static_cast<std::size_t>(slot), 1);
        }
    }
    // The arrow spawns in front of the eye along the aim (the launch
    // origin never sits inside the near plane), at the item's data
    // speed/damage scaled by the charge's power (core/bow.hpp — a full
    // draw is the item's full data values). The updateEntities pass runs
    // its swept arrow-vs-mob hit test; the despawn pass removes it on
    // age / kill plane, and a block impact STICKS it (Phase 24). Arrows
    // are not saved (ephemeral, like mobs).
    const float power = bowPower(m_interaction.bowDrawSeconds);
    const Vec3 f = aimForward();
    // UX-8g: the arrow leaves from in front of the EYE in both modes
    // (first person: m_camera.position == eyePosition = the bit-
    // identical legacy origin). Third person the camera sits 4 blocks
    // behind the eye, so the old origin made the shot appear out of
    // the player's back; the eye origin = the Minecraft shot from in
    // front, on the aim line.
    const Vec3 o = m_player->eyePosition() + f * kArrowSpawnOffset;
    // Phase 42: the arrow carries the shot's charge (power — scales the
    // mob-hit knockback) and the launcher's ammo item (a stuck arrow
    // picked up returns it to the inventory).
    // SURV-5: the launcher's arrow effect — the data name resolves to the
    // registry id here (init cross-checked the name AND its
    // affects_mobs flag, so idForName cannot fail); -1 on a plain bow =
    // the pre-SURV-5 projectile, bit-identical.
    int effectId = -1;
    double effectDuration = 0.0;
    int effectTier = 0;
    if (!item.arrowEffectName.empty()) {
        effectId = m_effects.idForName(item.arrowEffectName);
        effectDuration = item.arrowEffectDuration;
        effectTier = item.arrowEffectTier;
    }
    // V2-12: the POWER enchant adds to the shot damage, the SPEED enchant
    // adds to the shot velocity (the documented deviation: MC's Quick
    // Charge is a draw-time reduction, the project has no draw-time model,
    // so speed = the projectile-speed paradigm, +L to shoot_speed). Both
    // scale by the charge power as the base data does.
    currentEntities().push_back(std::make_unique<Projectile>(
        currentWorld(), m_blocks, o, f, (item.shootSpeed + speedBonus) * power,
        (item.shootDamage + powerBonus) * power, /*hurtPlayer=*/false, power,
        item.ammo, effectId, effectDuration, effectTier));
    // SURV-3 (durability): the SHOT wears the launcher one point (any
    // charge; Infinity included — the arrow is not consumed, the bow
    // still is). The shot completed (the arrow is flying) — a bow that
    // breaks on this shot is simply gone. V2-12: the unbreaking skip.
    if (item.durability > 0.0f) {
        if (wearItem(stack, kLauncherShotWear)) {
            m_sound.play("break"); // the bow broke on this shot
        }
    }
    m_sound.play("shoot");
    resetBowDraw();
}

void Game::resetBowDraw() {
    m_interaction.bowDrawing = false;
    m_interaction.bowDrawSeconds = 0.0;
}

void Game::setBlock(int x, int y, int z, BlockID id) {
    // V3-1 Phase B: the player-facing choke point (the legacy name) —
    // the edit lands in the PLAYER's dimension (the pre-Phase-B
    // bit-identical path while the player is in the overworld).
    setBlockInWorld(currentWorld(), x, y, z, id);
}

void Game::setBlockInWorld(World& world, int x, int y, int z, BlockID id) {
    // The dimension-explicit choke point (V3-1 Phase B): the bookkeeping
    // resolves from the WORLD (m_world = the overworld family; any other
    // world = the nether family — only the two dimensions exist, so the
    // pointer test is the discriminator). The overworld path is the
    // pre-Phase-B code verbatim (the bit-identical red line); the nether
    // runs the same mechanics over its parallel family — EXCEPT the seven
    // per-block state maps (furnaces / campfires / tables / crops /
    // saplings / moisture / beacons / fires) are OVERWORLD-ONLY (the D9
    // closed-set ruling: the nether holds no container / plant / fire
    // state in Phase B — a furnace placed in the nether is a plain block,
    // the P70 family-inert pattern; the nether fire segment is Phase C).
    const bool nether = (&world != &m_world);
    StreamingState& st = nether ? m_nether.streaming : m_streaming;
    std::set<ChunkPos>& flammableChunks =
        nether ? m_nether.flammableChunks : m_flammableChunks;
    std::vector<std::unique_ptr<Entity>>& ents =
        nether ? m_nether.entities : m_entities;
    // A first-touch column must be COMPLETE before the edit: the World's
    // setBlock creates only the target chunk, and the streaming loader
    // counts a column as loaded when ANY chunk exists — a partial column
    // would keep its other chunks void forever (a hole around the edit).
    // Completing it is the documented synchronous generation (the
    // placement path, ~1.6 ms for a fresh column; a no-op cost of 8 map
    // lookups when the column already exists).
    const ChunkPos cp = worldToChunk({x, y, z});
    if (!world.columnComplete(cp.x, cp.z)) {
        // First touch of a new column: complete it synchronously and
        // register the column for streaming (loaded), unload (tracked),
        // and the lighting job (new terrain).
        world.ensureColumn(cp.x, cp.z);
        st.loadedColumns.insert({cp.x, cp.z});
        (nether ? m_nether.lightPendingColumns : m_lightPendingColumns)
            .insert({cp.x, cp.z});
        st.unlitColumns.insert({cp.x, cp.z});
    }
    // The single choke point for every in-game block change (break,
    // place, dye recolor). Phase 42: in addition to the raw setBlock, any
    // STUCK arrow hung on the changed cell is released (Minecraft: arrows
    // fall out when their block is broken or replaced) — it rejoins
    // flight physics with zero velocity (gravity) and its cumulative age.
    // Phase 44: the per-block furnace state follows the cell — a furnace
    // block always owns a state (created on placement, empty), and a
    // cell that stops holding a furnace drops its state (the invariant
    // the format-7 save cross-checks against the block edits).
    // V3-1 Phase A: the cell's pre-write content (the obsidian
    // destruction's pre-event ring view — the break's brokenState
    // precedent: the state is read BEFORE the write erases it).
    const BlockID oldId = world.getBlock(x, y, z);
    world.setBlock(x, y, z, id);
    // Burn-pass gate (Phase 99.8): a flamable placement/rotation makes
    // the chunk "ever held a flamable" (over-approximation — removals do
    // not prune; see m_flammableChunks).
    if (m_blocks.at(id).properties.flamable) {
        flammableChunks.insert(cp);
    }
    // The column now DIFFERS from pristine generated terrain (break /
    // place / rotate / growth / burn / campfire — everything goes through
    // this choke point): the save records the diff, so the column's
    // chunks must survive the unload pass (player-edited columns are
    // never dropped — a reloaded save would otherwise lose the edits).
    st.editedColumns.insert({floorDiv(x, kChunkSize),
                             floorDiv(z, kChunkSize)});
    if (!nether) {
        // The seven per-block state families (OVERWORLD-ONLY — see the
        // function header's D9 ruling: the nether's closed set {blocks,
        // fluids, states} carries no container / plant / fire state in
        // Phase B).
        const BlockPos fpos {x, y, z};
        const bool furnace =
            m_blocks.at(id).properties.interact == kInteractFurnace;
        if (furnace) {
            m_states.furnaces.emplace(fpos, FurnaceState {});
        } else {
            m_states.furnaces.erase(fpos);
        }
        // Phase 48: the crop-growth tracking set follows the cell (like
        // the furnace states): a block with a non-empty growsTo is
        // tracked, a cell that stops holding one drops out — maturity
        // included (the final stage has no growsTo and never rolls
        // again).
        const BlockPos cpos {x, y, z};
        if (!m_blocks.at(id).properties.growsTo.empty()) {
            m_states.crops[cpos] = id;
        } else {
            m_states.crops.erase(cpos);
        }
        // GEN-2: the sapling tracking set follows the cell (the
        // m_states.crops twin, the `tree` predicate): a sapling
        // placement/break registers/drops the cell; a GROWN tree's cells
        // (trunk / leaves) never match, so the completion drops the
        // entry through this same path (the root cell becoming the
        // trunk). The stage is NOT touched here — the ENT-2 block below
        // it seeds/clears the state byte independently.
        const BlockPos spos {x, y, z};
        if (m_blocks.at(id).hasTree()) {
            m_states.saplings[spos] = id;
        } else {
            m_states.saplings.erase(spos);
        }
        // GEN-9: the moisture tracking set follows the cell (the
        // m_states.saplings twin, the model.moisture_top predicate): a
        // farmland placement/break registers/drops the cell, seeded with
        // its wetness NOW so the first mesh (and the first refresh) see
        // the correct state. The WETNESS is derived at mesh time — this
        // entry is only the change detector for the fluid-tick re-mesh
        // trigger (refreshMoistureStates).
        const BlockPos mpos {x, y, z};
        if (!m_blocks.at(id).model.moistureTop.empty()) {
            m_states.moistureBlocks[mpos] = soilIsWet(m_world, mpos);
            // Seed-time re-mesh: the cell's chunk may hold a STALE mesh
            // (baked before this placement, or before the water neighbor
            // existed — the sweep's flip gate then never fires, because
            // the tracked state already equals the live one). Forcing
            // one re-mesh here makes the top face derive from the
            // now-current fluid field; the sweep covers the later
            // chunk-load case.
            m_world.markChunkDirty(worldToChunk(mpos));
        } else {
            m_states.moistureBlocks.erase(mpos);
        }
        // Phase 83 (a list in Phase 91): the beacon tracking set follows
        // the cell (like the crops): a block with a non-empty
        // beaconEffects list is a beacon (tracked; its power level is
        // re-derived from the world each frame, so no per-instance state
        // is stored), a cell that stops holding one drops out.
        const BlockPos bpos {x, y, z};
        if (!m_blocks.at(id).properties.beaconEffects.empty()) {
            m_states.beacons[bpos] = id;
        } else {
            m_states.beacons.erase(bpos);
        }
        // Phase 64: the fire remaining life follows the cell (like the
        // furnace states): a cell that holds a fire block always owns a
        // life (created with the FULL lifetime on placement/burn —
        // emplace keeps an existing countdown, so a same-block re-set
        // never resets it), and a cell that stops holding a fire drops
        // its life (the invariant the format-10 save cross-checks).
        // m_states.fireId is invalid only in an asset set without the
        // fire block (then this is a no-op, like the other registry-
        // keyed bookkeeping).
        const BlockPos ppos {x, y, z};
        if (m_states.fireId.valid() && id == m_states.fireId) {
            m_states.fires.emplace(ppos, m_states.fireLifetime);
        } else {
            m_states.fires.erase(ppos);
        }
        // Phase 71: the campfire cooking state follows the cell (like
        // the furnace states): a campfire block always owns a state
        // (created on placement, empty), and a cell that stops holding a
        // campfire drops its state (the invariant the format-13 save
        // cross-checks against the block edits).
        const BlockPos kpos {x, y, z};
        if (m_blocks.at(id).properties.interact == kInteractCampfire) {
            m_states.campfires.emplace(kpos, CampfireCookState {});
        } else {
            m_states.campfires.erase(kpos);
        }
        // CRAFT-1: the table's grid follows the cell (like the furnace
        // states): a crafting_table block always owns a grid (created on
        // placement, empty), and a cell that stops holding one drops its
        // grid (the invariant the format-19 save cross-checks against
        // the block edits). BREAKING a table loses the items in it — the
        // container contents do not drop (the furnace-break semantics:
        // only the block's own `drops` spawn; the Minecraft rule).
        const BlockPos tpos {x, y, z};
        if (m_blocks.at(id).properties.interact == kInteractCraftingTable) {
            m_states.tables.emplace(tpos, CraftingGrid {});
        } else {
            m_states.tables.erase(tpos);
        }
    }
    // ENT-2: the per-instance block state follows the cell (the
    // project's first per-block state — Phase 94's stair facing,
    // generalized). A stateful block (hasState(): the Stairs shape or an
    // explicit state_count) owns a state — set to the block's state
    // DEFAULT on placement (the placeBlock override can set a placement-
    // specific value afterward), and a cell that stops holding a
    // stateful block keeps its byte (the World API rejects the sentinel
    // write — no change, no dirty mark; the consumers are shape/state-
    // gated, so the stale value is inert). The collision (solidRange) +
    // the mesh (emitStairs / emitSlab) + the save read it from the World.
    // V3-1 Phase B: BOTH dimensions (the nether's portal axis state —
    // the nether's save `states` segment — seeds through this same path).
    {
        const Block& b = m_blocks.at(id);
        if (b.hasState()) {
            world.setBlockState(x, y, z,
                                static_cast<std::uint8_t>(b.stateDefault()));
        } else {
            // No state: the sentinel write is rejected (World validates
            // < 255) — zero change, zero dirty marks.
            world.setBlockState(x, y, z, Chunk::kBlockStateUnset);
        }
    }
    // V3-1 Phase A: the portal tracking set follows the cell (the
    // beacons' twin, the portalId predicate): the ambient pass iterates
    // it; a cell that stops holding a portal drops out (the choke point's
    // set of plans keeps it exact — the rescan rebuilds it on load).
    // V3-1 Phase B: PER-DIMENSION (the overworld's m_states.portals /
    // the nether's m_nether.portals — the nether's portals track there).
    {
        const BlockPos pp {x, y, z};
        std::map<BlockPos, BlockID>& portals =
            nether ? m_nether.portals : m_states.portals;
        if (m_states.portalId.valid() && id == m_states.portalId) {
            portals[pp] = id;
        } else {
            portals.erase(pp);
        }
    }
    // V3-1 Phase A: the portal frame event (an obsidian or fire placement
    // or destruction — the bounded scan + the pure transition decision +
    // the plans, see checkPortalTransition). Structurally inert without
    // the portal + obsidian blocks and on every other event class (the
    // base run's immunity root: a no-input worldgen run never places
    // obsidian or fire through this choke point — the fluid reaction's
    // obsidian goes through the raw World API, the journal-seed path).
    // V3-1 Phase B: dimension-explicit (the frame event runs over the
    // dimension the edit landed in).
    if (m_states.portalId.valid() && m_states.obsidianId.valid() &&
        (oldId == m_states.obsidianId || id == m_states.obsidianId ||
         oldId == m_states.fireId || id == m_states.fireId)) {
        checkPortalTransition(world, oldId, id, x, y, z);
    }
    const float half = Projectile::kSize * 0.5f;
    for (auto& entity : ents) {
        auto* arrow = dynamic_cast<Projectile*>(entity.get());
        if (arrow == nullptr || !arrow->stuck()) {
            continue;
        }
        const Vec3 p = arrow->position();
        const float fx = static_cast<float>(x);
        const float fy = static_cast<float>(y);
        const float fz = static_cast<float>(z);
        // The collision resolver clamps a stuck arrow EXACTLY onto the
        // face it struck: its box touches the STUCK cell's boundary and
        // its center sits strictly outside that face. The stuck cell is
        // the changed cell iff, per axis, the box range meets the cell's
        // closed range (inAxis) and in at least one axis the box touches
        // a face from the OUTSIDE (touchAxis — the center strictly beyond
        // the face). A cell the box merely occupies from inside (no face
        // contact) does not qualify, so placing into the air cell around
        // a wall-stuck arrow does not release it.
        const auto inAxis = [half](float c, float lo, float hi) {
            return c - half <= hi && c + half >= lo;
        };
        const auto touchAxis = [half](float c, float lo, float hi) {
            return (c + half >= lo && c < lo) ||
                   (c - half <= hi && c > hi);
        };
        if (inAxis(p.x, fx, fx + 1.0f) && inAxis(p.y, fy, fy + 1.0f) &&
            inAxis(p.z, fz, fz + 1.0f) &&
            (touchAxis(p.x, fx, fx + 1.0f) ||
             touchAxis(p.y, fy, fy + 1.0f) ||
             touchAxis(p.z, fz, fz + 1.0f))) {
            arrow->release();
            MC_LOG_DEBUG("arrow released from changed block at (" +
                         std::to_string(p.x) + ", " + std::to_string(p.y) +
                         ", " + std::to_string(p.z) + ")");
        }
    }
}

void Game::checkPortalTransition(World& world, BlockID oldId, BlockID newId,
                                 int x, int y, int z) {
    (void)newId; // the decision reads the OLD id + the world as it is now
    // The bounded scan over the event cell's two vertical planes (the
    // sub-millisecond worst case, the core/portal discipline) + the pure
    // transition decision (the unit tests own both). Deactivation plans
    // apply before activation plans (the lists are mutually exclusive per
    // frame — a frame is either complete now or broken by the event —
    // and their interiors are disjoint, so the snapshot's census stays
    // valid through the application). V3-1 Phase B: dimension-explicit
    // (the frame event runs over the dimension the edit landed in — the
    // nether's portals build / deactivate there).
    const BlockPos cell {x, y, z};
    const std::vector<PortalFrameStatus> candidates = scanPortalFrames(
        world, cell, m_states.obsidianId, m_states.fireId,
        m_states.portalId);
    const PortalTransitionDecision d =
        portalTransitionDecision(candidates, oldId, m_states.obsidianId);
    for (const PortalFrameStatus& st : d.deactivate) {
        deactivatePortal(world, st.frame);
    }
    for (const PortalFrameStatus& st : d.activate) {
        activatePortal(world, st.frame);
    }
}

void Game::activatePortal(World& world, const PortalFrame& frame) {
    // The pure plan (the interior's air + fire cells — the fire cell is
    // REPLACED, Minecraft's "one of them replacing the fire") applied
    // through the choke point (the journal + the tracking sets + the
    // ENT-2 state seeding ride along; a replaced fire drops its burn
    // life with the cell). The choke point seeds each portal's state
    // default; the portal's state IS the frame's axis (the per-instance
    // byte the save's format-15 states segment carries — zero format).
    for (const BlockPos& p :
         portalActivationPlan(world, frame, m_states.fireId)) {
        setBlockInWorld(world, p.x, p.y, p.z, m_states.portalId);
        world.setBlockState(p.x, p.y, p.z,
                            static_cast<std::uint8_t>(frame.axis));
    }
    // ONE relight over the frame's bounding box (the portal is a light-11
    // source; the fluid-tick bounding-box pattern, not per-cell).
    Lighting::updateRegion(world, m_blocks,
                           BlockPos {frame.lo, frame.y0, frame.plane},
                           BlockPos {frame.hi, frame.y1, frame.plane});
    // The bottom long edge's center (the log's anchor — deterministic).
    MC_LOG_INFO("Portal activated at (" +
                std::to_string((frame.lo + frame.hi) / 2) + ", " +
                std::to_string(frame.y0) + ", " +
                std::to_string(frame.plane) + ") (" +
                std::to_string(frame.longEdge()) + "x" +
                std::to_string(frame.height()) + ")");
}

void Game::deactivatePortal(World& world, const PortalFrame& frame) {
    // The pure plan (the interior's portal cells -> air) through the
    // choke point (the tracking set drops the cells, the journal records
    // the fix so the next save persists it).
    for (const BlockPos& p :
         portalDeactivationPlan(world, frame, m_states.portalId)) {
        setBlockInWorld(world, p.x, p.y, p.z, BlockID::air());
    }
    Lighting::updateRegion(world, m_blocks,
                           BlockPos {frame.lo, frame.y0, frame.plane},
                           BlockPos {frame.hi, frame.y1, frame.plane});
    MC_LOG_INFO("Portal deactivated at (" +
                std::to_string((frame.lo + frame.hi) / 2) + ", " +
                std::to_string(frame.y0) + ", " +
                std::to_string(frame.plane) + ") (" +
                std::to_string(frame.longEdge()) + "x" +
                std::to_string(frame.height()) + ")");
}

void Game::breakBlock(const BlockHit& hit) {
    const BlockID id = currentWorld().getBlock(hit.block.x, hit.block.y,
                                                hit.block.z);
    // V2-17: the broken cell's per-instance state, read BEFORE the
    // setBlock(air) below erases it (the setBlock choke point reseeds the
    // byte for the NEW block — the sentinel for air): the per-state drop
    // count (drops_count_by_state) needs it. The unset -> stateDefault
    // rule (the emitStairs / collision / mesh twin).
    const Block& brokenBlock = m_blocks.at(id);
    int brokenState = brokenBlock.stateDefault();
    if (brokenBlock.hasState()) {
        const int bs = currentWorld().getBlockState(hit.block.x,
                                                     hit.block.y,
                                                     hit.block.z);
        if (bs != Chunk::kBlockStateUnset) {
            brokenState = bs;
        }
    }
    // setBlock marks the chunk (and face-sharing neighbors) dirty; the mesh
    // rebuild happens on the next frame.
    setBlock(hit.block.x, hit.block.y, hit.block.z, BlockID::air());
    // Relight the region around the change (a broken occluder lets light
    // in; the broken cell stops blocking).
    Lighting::updateLocal(currentWorld(), m_blocks, hit.block);
    // UX-7: the tutorial break event (the block the break COMPLETED on —
    // the data-driven name, chance drops irrelevant: the goal counts the
    // block, not the drop). Frame-locked: breakBlock only runs from the
    // hold-to-break completion / the instant-break path inside the update
    // frame.
    emitTutorialEvent("break." + m_blocks.at(id).name, 1);

    // Drops are data-driven: `properties.drops` names an item (validated
    // against the ItemRegistry at init). Each spawned item rests at its
    // spawn position with zero velocity, so headless runs stay
    // byte-reproducible (see ItemEntity). UX-8d: the spawns scatter
    // deterministically around the block center (dropSpawnOffset — a pure
    // function of (seed, position, drop index); the item always stays
    // inside the broken cell, see its bound argument) so a multi-drop block
    // no longer stacks its items at one point and a broken column no longer
    // lines up on the block centers.
    const Block& block = m_blocks.at(id);
    // The break sound stays at the exact block center (the BLOCK is the
    // sound's source, not the scattered items).
    const Vec3 dropPos {static_cast<float>(hit.block.x) + 0.5f,
                        static_cast<float>(hit.block.y) + 0.5f,
                        static_cast<float>(hit.block.z) + 0.5f};
    const std::uint64_t dropSeed = static_cast<std::uint64_t>(currentSeed());
    // The scattered spawn for `dropIndex`: 0 = primary `drops`,
    // 1 = `chance_drop`, 2 + i = `chance_drops` list entry i (the same
    // 0-based list order the entry roll uses — block/drop_scatter).
    const auto dropAt = [&](int dropIndex) {
        const Vec2 off = dropSpawnOffset(dropSeed, hit.block.x, hit.block.y,
                                         hit.block.z, dropIndex);
        return Vec3 {dropPos.x + off.x, dropPos.y, dropPos.z + off.y};
    };
    if (!block.properties.drops.empty()) {
        const Item& dropItem = m_items.at(m_items.idForName(block.properties.drops));
        // dropsCount (validated at init to fit the item's max stack).
        // V2-17: the PER-STATE count — a stateful block with a
        // drops_count_by_state list yields the entry for the broken
        // cell's state (read above, before the setBlock erasure): the
        // shipped double slab (state 2) drops TWO slabs (the Minecraft
        // material conservation); an empty list = dropsCount for every
        // state (the pre-V2-17 behavior, the drop item's name unchanged).
        int dropCount = block.properties.dropsCount;
        if (!block.properties.dropsCountByState.empty()) {
            dropCount = block.properties.dropsCountByState[brokenState];
        }
        currentEntities().push_back(std::make_unique<ItemEntity>(
            currentWorld(), m_blocks, dropItem.name, dropItem.maxStack,
            dropCount, dropAt(0)));
    }
    // Chance secondary drop (Phase 49): an OPTIONAL extra item rolled per
    // break, independent of (and in addition to) the primary drop. The roll
    // is a pure function of (world seed, position, chance) — headless
    // byte-reproducible (no wall-clock, no per-instance RNG). Spawns at its
    // own scattered position (dropIndex 1), decorrelated from the primary.
    if (!block.properties.chanceDropItem.empty()) {
        if (rollChanceDrop(dropSeed, hit.block.x, hit.block.y, hit.block.z,
                           block.properties.chanceDropChance)) {
            const Item& cdItem =
                m_items.at(m_items.idForName(block.properties.chanceDropItem));
            currentEntities().push_back(std::make_unique<ItemEntity>(
                currentWorld(), m_blocks, cdItem.name, cdItem.maxStack,
                block.properties.chanceDropCount, dropAt(1)));
        }
    }
    // Chance secondary drop LIST (Phase 52): each entry rolls independently
    // (the entry-indexed salt keeps them decorrelated — block/chance_drop)
    // and each passing entry spawns its own item at its own scattered
    // position (dropIndex 2 + i).
    for (std::size_t i = 0; i < block.properties.chanceDrops.size(); ++i) {
        const ChanceDrop& cd = block.properties.chanceDrops[i];
        if (rollChanceDropEntry(dropSeed, hit.block.x, hit.block.y, hit.block.z,
                                static_cast<int>(i), cd.chance)) {
            const Item& cdItem = m_items.at(m_items.idForName(cd.item));
            currentEntities().push_back(std::make_unique<ItemEntity>(
                currentWorld(), m_blocks, cdItem.name, cdItem.maxStack,
                cd.count, dropAt(2 + static_cast<int>(i))));
        }
    }
    // V2-11 (XP): the mine-complete grant — one orb worth the block's
    // data mine_xp, at the break position's center (the dropPos the
    // items scatter around — the unscattered center, the break sound's
    // position). mine_xp 0 (the default) = no orb (the data gate). The
    // drop-less paths (the flint-and-steel burn, the fire burn) never
    // run breakBlock, so they grant nothing — by construction.
    if (block.properties.mineXp > 0) {
        spawnExpOrb(block.properties.mineXp, dropPos);
    }

    // Break particles (Phase 15): a deterministic burst (world seed mixed
    // with an increasing per-break counter: the same input sequence gives
    // the same burst). The light is baked here — updateLocal already ran
    // above — because the render pass must not read the light arrays
    // (worker lighting jobs may be running while it renders).
    const Chunk* chunk = currentWorld().findChunk(worldToChunk(hit.block));
    float sky = 1.0f;
    float blockLight = 0.0f;
    if (chunk != nullptr) {
        const BlockPos local =
            worldToChunkLocal(worldToChunk(hit.block), hit.block);
        sky = static_cast<float>(chunk->skyLightAt(local.x, local.y,
                                                   local.z)) /
              15.0f;
        blockLight = static_cast<float>(
                         chunk->blockLightAt(local.x, local.y, local.z)) /
                     15.0f;
    }
    const std::uint64_t rngSeed =
        currentSeed() +
        (++m_particleBreakCounter) * 0x9E3779B97F4A7C15ULL;
    m_particles.spawnBreak(hit.block.x, hit.block.y, hit.block.z, id, sky,
                           blockLight, rngSeed);
    // SURV-3 (durability): the held TOOL wears one point per NON-instant
    // block broken (hardness 0 = the "naturally breaks instantly"
    // approximation = 0 wear — the documented deviation; hand-mining and
    // non-tool items wear nothing — the event table's "tool +1" gate).
    // The break COMPLETES regardless (the block is already air +
    // dropped + sounded — the wear is the action's epilogue): a tool
    // that breaks on this block is simply gone from the slot.
    if (block.properties.hardness != 0.0f) {
        ItemStack& held = m_inventory.at(m_selectedSlot);
        if (!held.empty()) {
            const Item& heldItem = m_items.at(held.item);
            if (heldItem.type == ItemType::Tool &&
                heldItem.durability > 0.0f) {
                if (wearItem(held, kToolBlockWear)) {
                    // The tool BROKE on this block: the item-break cue
                    // (the existing "break" wav — the sound table has no
                    // dedicated item-break asset, the design's reuse).
                    // CRAFT-3: the held item breaks at the PLAYER (gain 1,
                    // panned centered = the 2D no-origin call, the
                    // documented choice).
                    m_sound.play("break");
                }
            }
        }
    }
    // CRAFT-3: the broken BLOCK's material timbre at the block center
    // (dropPos — no material = the generic "break").
    m_sound.play(blockSoundName(block, "break"), dropPos);
}

void Game::placeBlock() {
    // The selected inventory slot must hold a placeable block item.
    const ItemStack& stack = m_inventory.at(m_selectedSlot);
    if (stack.empty()) {
        return;
    }
    const Item& item = m_items.at(stack.item);
    if (item.type != ItemType::Block) {
        return; // tools (and future non-block items) are not placeable
    }
    const std::optional<BlockHit> hit = targetBlock();
    if (!hit) {
        return;
    }
    // The target cell is the face neighbor of the hit block. The normal is a
    // unit axis vector, so the int cast is exact.
    const BlockPos target {
        hit->block.x + static_cast<int>(hit->normal.x),
        hit->block.y + static_cast<int>(hit->normal.y),
        hit->block.z + static_cast<int>(hit->normal.z)};
    // V2-17: the slab placement state rule (the Phase-97 stair auto-orient
    // slot — the placement-state-rule extension point). A stateful slab
    // (the shape + the hasState data flag — no species switch) places by
    // the aim hit (slabStateFromHit, the Minecraft rule): a top-face aim =
    // the bottom slab above (the pre-V2-17 behavior, the bit-level root),
    // a bottom-face aim (the ceiling) = the top slab below, a side aim =
    // by the hit height within the target cell (strictly above the middle
    // -> top). A target cell that ALREADY holds the same slab in its
    // complementary half completes the DOUBLE slab (state 2 — the
    // Minecraft same-cell merge, one item consumed); a state-2 target is
    // not a merge (the full cell — canPlaceAt's solid rejection below
    // handles it, the legacy path).
    const Block& placedBlock = m_blocks.at(item.block);
    int slabPlaceState = -1;
    bool slabMerge = false;
    if (placedBlock.model.shape == BlockModel::Shape::Slab &&
        placedBlock.hasState()) {
        // The aim ray's hit point: eye + forward * distance (the
        // targetBlock ray — the BlockHit's distance is the ray parameter,
        // so the hit point is reconstructible, the targetBlock contract).
        const Vec3 hitPoint =
            m_player->eyePosition() + aimForward() * hit->distance;
        slabPlaceState =
            slabStateFromHit(hitPoint, hit->normal,
                             static_cast<float>(target.y));
        const BlockID targetId = currentWorld().getBlock(target.x,
                                                         target.y, target.z);
        if (targetId == item.block) {
            const int cur =
                currentWorld().getBlockState(target.x, target.y, target.z);
            const int targetState =
                (cur != Chunk::kBlockStateUnset) ? cur
                                                 : placedBlock.stateDefault();
            slabMerge = slabMergesToDouble(targetId, item.block,
                                           targetState, slabPlaceState);
        }
    }
    if (slabMerge) {
        // The merge bypasses canPlaceAt's SOLID rejection (the target
        // cell holds the complementary half — the merge's precondition)
        // but keeps its Y-range + player-overlap rejections: the
        // completed double slab is a FULL solid, so a cell the player's
        // body overlaps may not become one (the strict canPlaceAt
        // convention, inlined — the merge is the only caller with a
        // solid target; canPlantAt is skipped — the first half was
        // soil-validated when it was placed, the soil is unchanged).
        if (target.y < 0 || target.y >= kWorldHeight) {
            return;
        }
        const Vec3& pmin = m_player->aabbMin();
        const Vec3& pmax = m_player->aabbMax();
        const float cx = static_cast<float>(target.x);
        const float cy = static_cast<float>(target.y);
        const float cz = static_cast<float>(target.z);
        if (pmin.x < cx + 1.0f && pmax.x > cx && pmin.y < cy + 1.0f &&
            pmax.y > cy && pmin.z < cz + 1.0f && pmax.z > cz) {
            return;
        }
        setBlock(target.x, target.y, target.z, item.block);
        // The completed double slab (state 2 — the single-cell
        // representation of the Minecraft same-cell pair).
        currentWorld().setBlockState(target.x, target.y, target.z, 2);
        Lighting::updateLocal(currentWorld(), m_blocks, target);
        m_inventory.take(m_selectedSlot, 1);
        emitTutorialEvent("place." + placedBlock.name, 1);
        m_sound.play(
            blockSoundName(placedBlock, "place"),
            Vec3{static_cast<float>(target.x) + 0.5f,
                 static_cast<float>(target.y) + 0.5f,
                 static_cast<float>(target.z) + 0.5f});
        return;
    }
    if (!canPlaceAt(currentWorld(), m_blocks, target, m_player->aabbMin(),
                    m_player->aabbMax())) {
        return;
    }
    // Planting placement restriction (Phase 53): if the placed block names a
    // plant_on soil list, the cell DIRECTLY BELOW the target must hold one of
    // the listed blocks (the soil). Absent = unrestricted (the generic
    // placeBlock rule). This is a placement-time gate, independent of the
    // growth-time grows_on gate (Phase 50). A wrong-soil placement is a
    // silent no-op (the block is not consumed, no sound — same as any other
    // rejected placement).
    if (!canPlantAt(currentWorld(), m_blocks, target,
                    m_blocks.at(item.block).properties.plantOn)) {
        return;
    }
    // Game::setBlock (Phase 42): also releases any arrow stuck in the
    // newly occupied cell (placing into a cell an arrow hangs in).
    setBlock(target.x, target.y, target.z, item.block);
    // Phase 97: stair placement AUTO-ORIENTS (Minecraft): the per-instance
    // facing (Phase 94, the state in ENT-2) is set from the player's yaw —
    // the stair's low side faces where the player looks. setBlock above
    // seeded the state with the block's state default; this overrides it
    // for stairs. Any stair-shaped block (the base stone_stairs or a
    // Phase-89 variant) gets the same placement rule (the variant's baked
    // facing only matters outside the game, e.g. unit fixtures). This slot
    // is the placement-state-rule extension point (ENT-2): the slab's aim
    // state (V2-17, slabStateFromHit) consumes it below; other stateful
    // shapes keep the choke point's default.
    if (m_blocks.at(item.block).model.shape == BlockModel::Shape::Stairs) {
        currentWorld().setBlockState(target.x, target.y, target.z,
                                      static_cast<std::uint8_t>(
                                          stairFacingFromYaw(m_player->yaw)));
    } else if (slabPlaceState >= 0) {
        // V2-17: the slab's placement state (slabStateFromHit — the aim
        // rule above). The setBlock choke point seeded the block's state
        // default; this overrides it (the stair auto-orient twin, the
        // ENT-2 post-choke-point pattern). A state-0 placement rewrites
        // the seeded default — a no-op write (zero dirty marks), the
        // pre-V2-17 behavior's bit-level root.
        currentWorld().setBlockState(target.x, target.y, target.z,
                                      static_cast<std::uint8_t>(
                                          slabPlaceState));
    }
    // Relight the region around the change (a placed occluder blocks light;
    // a placed glowstone is a new light source).
    Lighting::updateLocal(currentWorld(), m_blocks, target);
    // Placing consumes one from the selected stack (the inventory is the
    // source of blocks — no more infinite placement).
    m_inventory.take(m_selectedSlot, 1);
    // UX-7: the tutorial place event (the block that was PLACED — the
    // success point: setBlock committed + the item consumed).
    emitTutorialEvent("place." + m_blocks.at(item.block).name, 1);
    // CRAFT-3: the PLACED block's material timbre at the new cell's
    // center (no material = the generic "place").
    m_sound.play(
        blockSoundName(m_blocks.at(item.block), "place"),
        Vec3{static_cast<float>(target.x) + 0.5f,
             static_cast<float>(target.y) + 0.5f,
             static_cast<float>(target.z) + 0.5f});
}

bool Game::tryEat() {
    // Only the selected slot can feed the player (Minecraft). The item is
    // food when its data marks restore_hunger (Item::restoreHunger > 0);
    // there is no hardcoded item list.
    if (!m_player || m_player->isDead()) {
        return false;
    }
    const ItemStack& stack = m_inventory.at(m_selectedSlot);
    if (stack.empty()) {
        return false;
    }
    const Item& item = m_items.at(stack.item);
    if (item.restoreHunger <= 0) {
        return false; // not food: fall through to the normal right-click
    }
    // Food is selected: the click is consumed either way.
    if (m_player->hunger() >= m_player->maxHunger()) {
        return true; // full: the eat is a no-op (nothing placed, no sound)
    }
    // Eat channel (Phase 28): the eat is no longer instant — holding the
    // right button for 1.6 s eats (updateEatChannel owns the completion:
    // consume + restore + sound). An extra press while channeling is
    // ignored (Minecraft: the eat continues).
    if (m_interaction.eatChannel.active) {
        return true;
    }
    m_interaction.eatChannel.start();
    m_interaction.eatSlot = m_selectedSlot;
    m_interaction.eatItem = stack.item;
    m_interaction.eatStartHealth = m_player->health();
    return true;
}

bool Game::tryDrink() {
    // Drinkable container (Phase 43): only the selected slot can feed
    // the player (Minecraft). The item is drinkable when its data marks
    // drink_time (Item::drinkTime > 0); there is no hardcoded item list.
    if (!m_player || m_player->isDead()) {
        return false;
    }
    const ItemStack& stack = m_inventory.at(m_selectedSlot);
    if (stack.empty()) {
        return false;
    }
    const Item& item = m_items.at(stack.item);
    if (item.drinkTime <= 0.0f) {
        return false; // not drinkable: fall through to the normal right-click
    }
    // A drinkable is selected: the click is consumed either way. No room
    // for the returned container (Minecraft: you cannot drink when the
    // empty bucket would have nowhere to go): a no-op, click consumed.
    // isFull(item) = "not even one more unit fits" (a partial stack of
    // the item or an empty slot exists = false), the exact probe add(1)
    // needs.
    if (!item.returnsItem.empty()) {
        const ItemID ret = m_items.idForName(item.returnsItem);
        if (ret.valid() && m_inventory.isFull(ret, m_items)) {
            return true; // no room for the container: no-op (no sound)
        }
    }
    // Drink channel (Phase 43): the shared eat channel armed with the
    // item's DATA-DRIVEN duration (updateEatChannel owns the completion:
    // consume + return container + clear status effects). An extra press
    // while channeling is ignored (the drink continues). The "milk"
    // sound plays on START (Minecraft: the glug is up front, even when
    // the drink is cancelled — same as the bow draw sound).
    if (m_interaction.eatChannel.active) {
        return true;
    }
    m_interaction.eatChannel.start(static_cast<double>(item.drinkTime));
    m_interaction.eatSlot = m_selectedSlot;
    m_interaction.eatItem = stack.item;
    m_interaction.eatStartHealth = m_player->health();
    m_sound.play("milk");
    return true;
}

bool Game::tryMilk() {
    // Milking (Phase 43, cow): the crosshair must be ON a milkable mob
    // (mob data 'milk_with' == the selected item's name; melee targeting
    // — kReach with block occlusion, see targetMob) and the player must
    // hold the mob's 'milk_with' item. One held item is consumed and one
    // 'milk_gives' item is added; the mob is NOT damaged, killed or made
    // to drop. SURV-10: a milked mob is on the 5-minute Minecraft milking
    // cooldown — re-milking it early does nothing (the click is still
    // consumed, the failed-milk rule below). The click is consumed
    // whenever the crosshair is on the mob (Minecraft: a failed milk —
    // e.g. a full inventory — still eats the click).
    if (!m_player || m_player->isDead()) {
        return false;
    }
    const ItemStack& stack = m_inventory.at(m_selectedSlot);
    if (stack.empty()) {
        return false;
    }
    const Item& item = m_items.at(stack.item);
    const std::optional<MobHit> mob = targetMob();
    if (!mob) {
        return false;
    }
    const MobProperties& props = mob->mob->def().properties;
    if (props.milkWith.empty() || props.milkWith != item.name) {
        return false; // not a milkable mob, or the wrong item in hand
    }
    // SURV-10: the milking cooldown (Minecraft's 5-minute rule) — the
    // same mob cannot be milked again until it expires. No milk, no
    // sound, the click is consumed (the failed-milk rule).
    if (!mob->mob->milkReady()) {
        return true;
    }
    // Init cross-checks guarantee the exists; the guard is a defensive
    // no-op.
    const ItemID gives = m_items.idForName(props.milkGives);
    if (!gives.valid()) {
        return true;
    }
    if (m_inventory.isFull(gives, m_items)) {
        return true; // full: no milk, click consumed (Minecraft)
    }
    mob->mob->startMilkCooldown();
    m_inventory.take(m_selectedSlot, 1);
    m_inventory.add(gives, 1, m_items);
    m_sound.play("milk");
    MC_LOG_DEBUG("milked mob '" + mob->mob->def().name + "' (" +
                 item.name + " -> " + props.milkGives + ")");
    return true;
}

bool Game::tryShear() {
    // V2-15 (sheep): the shearing action (the tryMilk paradigm — both
    // data flags, no species switch: the item's 'shears' flag x the mob's
    // 'wool_drops' flag). The crosshair must be on a wool-drops mob
    // (melee targeting: kReach + block occlusion, see targetMob) and the
    // selected item must be the shears (the data predicate — the
    // ignitesFlamable pattern). The woolly instance: the mob's OWN wool
    // drop entry's count (1-3 on the shipped sheep — the per-mob stream
    // roll, the death pass's twin) of the INSTANCE-COLOR wool (the
    // dyeVariantName item, the init cross-checked family) drops at the
    // mob as a ground item (zero velocity + the pickup delay — the Q-
    // drop precedent), the mob enters the bare regrow window
    // (kSheepRegrowFrames — deviation 3's fixed duration), the shears
    // wear kShearWear (the successful-action wear, the kIgniteWear twin —
    // a shears that breaks on the shear is simply gone), and the "shear"
    // cue plays spatialized at the mob. The BARE instance: a no-op that
    // still consumes the click (Minecraft, the failed-milk rule). The
    // click is consumed whenever the crosshair is on the target mob.
    if (!m_player || m_player->isDead()) {
        return false;
    }
    const ItemStack& stack = m_inventory.at(m_selectedSlot);
    if (stack.empty()) {
        return false;
    }
    const Item& item = m_items.at(stack.item);
    if (!item.shearsTool) {
        return false; // not the shears: fall through to the next channel
    }
    const std::optional<MobHit> mob = targetMob();
    if (!mob) {
        return false;
    }
    const MobProperties& props = mob->mob->def().properties;
    if (!props.woolDrops) {
        return false; // a non-wool mob: the shears do nothing here
    }
    // The bare instance (Minecraft: shearing a sheared mob does nothing):
    // no wool, no wear, no sound — the click is still consumed.
    if (mob->mob->isBare()) {
        return true;
    }
    // The successful shear: the state change + the count roll (the
    // per-mob stream — the rollDropCount twin inside shearWool).
    const int count = mob->mob->shearWool();
    const std::string woolName =
        dyeVariantName(kWoolDyeFamily, mob->mob->woolColor());
    const ItemID wid = m_items.idForName(woolName);
    if (count > 0 && wid.valid()) {
        // One ground item at the mob's bottom-center (the death pass's
        // rest-position twin — zero velocity, the deterministic
        // convention; the pickup delay keeps it from re-collecting on
        // the next frame).
        const Item& w = m_items.at(wid);
        currentEntities().push_back(std::make_unique<ItemEntity>(
            currentWorld(), m_blocks, w.name, w.maxStack, count,
            mob->mob->position()));
    }
    m_sound.play("shear", mob->mob->position());
    // kShearWear (the successful-action wear — the kIgniteWear twin).
    if (item.durability > 0.0f) {
        if (wearItem(m_inventory.at(m_selectedSlot), kShearWear)) {
            m_sound.play("break"); // the shears broke on the shear
        }
    }
    MC_LOG_DEBUG("sheared '" + mob->mob->def().name + "' (" +
                 std::to_string(count) + " x " + woolName + ")");
    return true;
}

bool Game::tryDyeMob() {
    // V2-15 (sheep): the mob dye (the block recolor's mob twin — the
    // tryMilk paradigm, both data flags: the item's dyeColor x the mob's
    // 'wool_drops'). The crosshair must be on a wool-drops mob and the
    // selected item a DYE (dyeColor non-empty — the data predicate,
    // never an item name). The woolly instance: the color becomes the
    // dye's (the data — no stream draw, the recolor is a pure function
    // of the input), one dye is consumed, the "cloth" cue plays (the
    // block recolor's sound twin). MINECRAFT: a sheared (bare) mob
    // REFUSES the dye — no color change, no consumption — but the click
    // is still consumed (the failed-milk rule). The click is consumed
    // whenever the crosshair is on the target mob.
    if (!m_player || m_player->isDead()) {
        return false;
    }
    const ItemStack& stack = m_inventory.at(m_selectedSlot);
    if (stack.empty()) {
        return false;
    }
    const Item& item = m_items.at(stack.item);
    if (item.dyeColor.empty()) {
        return false; // not a dye: fall through to the next channel
    }
    const std::optional<MobHit> mob = targetMob();
    if (!mob) {
        return false;
    }
    const MobProperties& props = mob->mob->def().properties;
    if (!props.woolDrops) {
        return false; // a non-wool mob: the dye does nothing here
    }
    // The bare instance (Minecraft: a sheared mob cannot be dyed): no
    // color, no dye consumed — the click is still consumed.
    if (mob->mob->isBare()) {
        return true;
    }
    mob->mob->dyeWool(item.dyeColor);
    m_inventory.take(m_selectedSlot, 1);
    m_sound.play("cloth");
    MC_LOG_DEBUG("dyed '" + mob->mob->def().name + "' to " + item.dyeColor);
    return true;
}

bool Game::tryFluidBucket() {
    // Fluid bucket (Phase 47; kinds in Phase 51 — water and lava). The
    // selected item must be a bucket side (item data 'collects_fluid' or
    // 'pours_fluid'; the pair symmetry is cross-checked at init). The
    // COLLECT side: the bucket-target ray (kReach; the first cell that is
    // a non-air block or carries fluid — a wall occludes, Minecraft) must
    // land on a level-8 (source) cell of a fluid the bucket names: the
    // source is removed and one held item is swapped for the full bucket
    // of THAT fluid (the unique item that pours it back — derived, the
    // init cross-check). The POUR side (bucketPourTarget, FLUID-4): the
    // face neighbor of the first non-air block — or the fluid ray's own
    // hit cell when no block is in reach — must be air without fluid
    // (the legacy placement) or a FLOWING cell of the bucket's own kind
    // (pour onto a water surface: it becomes the source), in the Y range
    // and not overlapping the player: a level-8 source of the item's
    // fluid is placed and the item swaps to the empty bucket. Both play
    // "splash" and consume the click (a bucket never places a block); a
    // no-action case falls through to the normal right-click (placeBlock
    // is a no-op for a material — nothing happens, e.g. on an
    // already-source cell of the bucket's own kind, a water-on-lava
    // reaction pour with no dry-air placement cell (FLUID-5), or a
    // solid target).
    if (!m_player || m_player->isDead()) {
        return false;
    }
    const ItemStack& stack = m_inventory.at(m_selectedSlot);
    if (stack.empty()) {
        return false;
    }
    const Item& item = m_items.at(stack.item);
    if (item.collectsFluids.empty() && item.poursFluid.empty()) {
        return false; // not a bucket side: fall through
    }
    const bool collect = !item.collectsFluids.empty();
    // V3-1 Phase B: the dimension aliases (the bucket acts on the
    // player's current world — the nether's lava sea scoops). T1
    // (v3.7.0): the nether WATER POUR evaporates (the bucket empties,
    // no cell touched — see the pour side's dimension gate below).
    World& world = currentWorld();
    std::set<ColumnPos>& editedColumns =
        playerInNether() ? m_nether.streaming.editedColumns
                         : m_streaming.editedColumns;
    if (collect) {
        // UX-8g: the bucket-target ray leaves from the eye in both
        // modes (first person: m_camera.position == eyePosition =
        // bit-identical; third person the camera behind the eye would
        // otherwise eat the reach budget and occlude itself, the same
        // family as the crosshair bug). V2-1: the budget is aimReach()
        // (front view: D + kReach, the same convention as targetBlock
        // — First / ThirdBack bit-identical).
        const std::optional<BlockPos> cell =
            raycastFluid(world, m_blocks, m_player->eyePosition(),
                         aimForward(), aimReach());
        // A source cell is level kMaxFluidLevel (flowing cells are lower
        // and cannot be collected, Minecraft). A block cell reads dry.
        const std::optional<FluidKind> kind =
            cell ? world.getFluidKind(cell->x, cell->y, cell->z)
                 : std::nullopt;
        const bool isSource =
            kind &&
            world.getFluidOfKind(cell->x, cell->y, cell->z, *kind) ==
                kMaxFluidLevel;
        // The source's fluid must be one the bucket names (Minecraft: one
        // empty bucket scoops every fluid it names).
        const std::string kindName =
            kind ? fluidKindName(*kind) : std::string();
        const bool named =
            kind && std::find(item.collectsFluids.begin(),
                              item.collectsFluids.end(),
                              kindName) != item.collectsFluids.end();
        if (!cell || !isSource || !named) {
            return false; // no named source in reach: fall through
        }
        // The full bucket of that fluid (init guarantees exactly one).
        const Item* full = m_items.fullBucketFor(kindName, item.name);
        if (full == nullptr) {
            return true; // defensive (init cross-check); consume the click
        }
        const ItemID becomes = full->id;
        if (m_inventory.isFull(becomes, m_items)) {
            return true; // no room: nothing happens (Minecraft)
        }
        auto finish = [&]() {
            m_inventory.take(m_selectedSlot, 1);
            m_inventory.add(becomes, 1, m_items);
            m_sound.play("splash");
        };
        world.setFluid(cell->x, cell->y, cell->z, *kind, 0);
        // The fluid diff is save-recorded (no setBlock choke point here):
        // keep the column (never unloaded).
        editedColumns.insert({floorDiv(cell->x, kChunkSize),
                              floorDiv(cell->z, kChunkSize)});
        Lighting::updateLocal(world, m_blocks, *cell);
        finish();
        MC_LOG_DEBUG("scooped " + kindName + " at (" +
                     std::to_string(cell->x) + ", " +
                     std::to_string(cell->y) + ", " +
                     std::to_string(cell->z) + ") (" + item.name + " -> " +
                     full->name + ")");
        return true;
    }
    // The pour side: the item's fluid kind (init guarantees the name is
    // in the vocabulary).
    const FluidKind kind = *fluidKindForName(item.poursFluid);
    const ItemID becomes = m_items.idForName(item.poursBecomes);
    if (!becomes.valid()) {
        return true; // defensive (init cross-check); consume the click
    }
    // T1 (v3.7.0): the nether WATER POUR = evaporation (Minecraft's
    // water-cannot-exist-in-the-nether rule — the v3.4.0 documented
    // no-op deviation's UPGRADE): the water simply vanishes — the item
    // swaps to the empty bucket, the "evaporate" sound plays (the 2D
    // centered path — the bucket action is the player's, the splash's
    // twin) and the info log is the E2E evidence. NO cell is touched
    // (no water is placed, no fluid diff, no light update — the
    // evaporation is a bucket-only event) and the click is ALWAYS
    // consumed (the water is gone, whatever the aim — the nether has no
    // placement cell for it). The no-room case: nothing happens, the
    // click consumed (the pour side's own isFull pattern). The pure
    // dimension gate (netherWaterPourEvaporates — the unit tests own
    // the four forms): the OVERWORLD path below is bit-identical (the
    // same statements, the same order — the false branch), and the
    // nether LAVA pour falls through to the normal placement
    // (Minecraft's twin: lava is legal in the nether — the recorded
    // deviation).
    if (netherWaterPourEvaporates(kind, playerInNether())) {
        if (m_inventory.isFull(becomes, m_items)) {
            return true; // no room for the companion: nothing happens
        }
        m_inventory.take(m_selectedSlot, 1);
        m_inventory.add(becomes, 1, m_items);
        m_sound.play("evaporate");
        MC_LOG_INFO(std::string("Evaporated ") + item.poursFluid +
                    " bucket in the nether (water cannot exist there)");
        return true;
    }
    if (m_inventory.isFull(becomes, m_items)) {
        return true; // no room for the companion: nothing happens (Minecraft)
    }
    auto finish = [&]() {
        m_inventory.take(m_selectedSlot, 1);
        m_inventory.add(becomes, 1, m_items);
        m_sound.play("splash");
    };
    const std::optional<BlockHit> hit = targetBlock();
    std::optional<BlockPos> fluidHit;
    if (!hit) {
        // FLUID-4: no block face in reach — open water beyond block
        // reach is the fluid ray's own hit cell (a wall in reach would
        // have produced the block hit, so the fluid hit here is
        // unoccluded by definition). UX-8g: the origin is the eye,
        // matching the targetBlock block hit above — same line and same
        // reach budget in both modes (V2-1: the budget is aimReach() —
        // front view D + kReach; first person bit-identical).
        fluidHit = raycastFluid(world, m_blocks, m_player->eyePosition(),
                                aimForward(), aimReach());
    }
    // The pour cell (bucketPourTarget): the face neighbor of the hit
    // block (the placement-target convention — the block ray passes
    // through fluids, so a floor under a 1-deep pool makes the pool's
    // top water cell the candidate), or the fluid hit when no block is
    // in reach. The candidate is either AIR without fluid (the legacy
    // placement, bit-identical) or a FLOWING cell of the bucket's own
    // kind (FLUID-4 pour onto a water surface — it becomes the source);
    // canPlaceAt keeps the Y range / solid / player-overlap tests.
    const std::optional<BlockPos> target = bucketPourTarget(
        world, m_blocks, hit, fluidHit, kind, m_player->aabbMin(),
        m_player->aabbMax());

    if (!target) {
        return false; // no acceptable pour cell: fall through
    }
    const BlockPos& cell = *target;
    world.setFluid(cell.x, cell.y, cell.z, kind, kMaxFluidLevel);
    // The fluid diff is save-recorded (no setBlock choke point here):
    // keep the column (never unloaded).
    editedColumns.insert({floorDiv(cell.x, kChunkSize),
                          floorDiv(cell.z, kChunkSize)});
    Lighting::updateLocal(world, m_blocks, cell);
    finish();
    MC_LOG_DEBUG("poured " + item.poursFluid + " at (" +
                 std::to_string(cell.x) + ", " +
                 std::to_string(cell.y) + ", " +
                 std::to_string(cell.z) + ") (" + item.name + " -> " +
                 item.poursBecomes + ")");
    return true;
}

bool Game::tryArmor() {
    // Armor (SURV-1): the right-click channel of the armor pieces. The
    // decision + every state write is the pure armorRightClickUse (item/
    // armor.hpp — the campfire pattern): a HELD piece equips onto its
    // data slot (an occupied slot SWAPS the piece with the held one), an
    // EMPTY hand unequips the first occupied slot in the fixed order
    // head -> chest -> legs -> feet. The click is consumed whenever a
    // piece is equipped, swapped or unequipped; otherwise it falls
    // through (the sneak-rotate / place no-op).
    if (!m_player || m_player->isDead()) {
        return false;
    }
    const ArmorUseAction action =
        armorRightClickUse(*m_player, m_inventory, m_items, m_selectedSlot);
    if (action == ArmorUseAction::None) {
        return false;
    }
    m_sound.play("cloth"); // the Minecraft armor sound (no new wav)
    return true;
}

void Game::trySleep(const BlockHit& hit) {
    // V2-13 (core/bed): the bed sleep's three gates, in the design's
    // order — the pure predicates own the boundaries (the unit tests
    // pin them), the adapter owns the state writes. The caller
    // guarantees the hit block is a bed FAMILY member (the kDyeBaseBed
    // property) and the hand holds no dye (the recolor path's).
    const Vec3 bedCenter =
        bedSpawnPosition(hit.block.x, hit.block.y, hit.block.z);
    // Gate b: the player's feet (the AABB bottom center) within 3
    // blocks of the bed's cell center (the Minecraft value).
    if (!sleepDistanceOk(m_player->position(), bedCenter)) {
        return; // refused: the click is consumed, no action (the
                // documented deviation 6 — the point-set is part of
                // the sleep, so a refused sleep sets nothing)
    }
    // Gate c: no HOSTILE mob in the 8x5 box around the bed (the
    // Minecraft head-volume rule — the hostile check is the
    // spawn-protection's data flag, never a species switch; passive
    // mobs, item drops and arrows are exempt, the protection twin).
    for (const std::unique_ptr<Entity>& e : m_entities) {
        const MobEntity* mob = dynamic_cast<const MobEntity*>(e.get());
        if (mob != nullptr && mob->def().properties.hostile &&
            inSleepHostileBox(bedCenter, mob->position())) {
            return; // refused: no sleep, no point (the deviation 6)
        }
    }
    // Gates b + c passed: the respawn point is the bed (the Minecraft
    // rule — a right-click on a bed sets it, night OR day).
    m_player->setSpawnPos(bedCenter);
    // Gate a: the night window decides the sleep vs the bare point-set.
    const double t = m_dayCycle.timeOfDay();
    if (!sleepNightWindow(t)) {
        MC_LOG_INFO("Bed spawn set at (" + std::to_string(hit.block.x) +
                    ", " + std::to_string(hit.block.y) + ", " +
                    std::to_string(hit.block.z) + ") — day: the point "
                    "is set, the night is not skipped");
        return; // the Minecraft day rule: awake, the point is set
    }
    // The sleep (the immediate night skip — the documented deviation 2,
    // no 101-tick entry animation / no hurt-wake-up): the time jumps
    // to the dawn edge (kDawnStart = the 06:00 clock, the ramp's
    // plateau — the day resumes in full light).
    m_dayCycle.setTimeOfDay(DayCycle::kDawnStart);
    // The weather reset (the Minecraft "sleeping clears the rain" rule):
    // only a RAINING weather is replaced — by the V2-4 machine's pure
    // initial state (a fresh CLEAR segment off the same world-seed
    // streams — the session-derived class, the stream's continuation
    // stays deterministic); a clear sky is untouched.
    if (m_weather.raining) {
        m_weather = weatherInitialState(false, m_weatherStream,
                                        m_weatherThunderStream);
        MC_LOG_INFO("Weather: rain stopped");
    }
    // The "sleep" cue (spatialized at the bed — the block-center
    // pattern; audio outside the frame state, the PRE-4 discipline) +
    // the tutorial event (the closed vocabulary's new bare word — no
    // series JSON change; the emit is gated on the tutorial being live
    // and not done, the choke point's invariant).
    m_sound.play("sleep", bedCenter);
    emitTutorialEvent("sleep", 1);
    MC_LOG_INFO("Slept at the bed (" + std::to_string(hit.block.x) +
                ", " + std::to_string(hit.block.y) + ", " +
                std::to_string(hit.block.z) + ") — the night skipped "
                "to dawn, the spawn is the bed");
}

void Game::clearStatusEffects() {
    // Phase 43 (completed by Phase 46): a completed drinkable container
    // resets the red damage veil (m_hurtFlash) and removes every NEGATIVE
    // active status effect — Minecraft's milk semantics (the positive
    // effects, like regeneration, survive the drink). The fall-damage
    // tracking (Player::fallStartY) is deliberately NOT touched —
    // Minecraft's fall damage is not a status effect, so drinking
    // mid-air must not reduce the fall damage owed on landing.
    m_hurtFlash = 0.0;
    m_playerEffects.clearNegative(m_effects);
    syncAbsorption(); // Phase 72: the pool drops with the cleared effects
}

void Game::updateEatChannel(double dt) {
    if (!m_interaction.eatChannel.active) {
        return;
    }
    // Interrupts (Phase 28, Minecraft): damage, death, pointer unlock, a
    // panel, or the food leaving its slot (selection change, Q-drop, or
    // the stack emptied some other way). Any of them cancels the channel
    // — the food is NOT consumed.
    const ItemStack& stack = m_inventory.at(m_selectedSlot);
    if (m_player->isDead() || !m_window.relativeMouseMode() ||
        panelOpen() || m_player->health() < m_interaction.eatStartHealth ||
        m_selectedSlot != m_interaction.eatSlot || stack.item != m_interaction.eatItem) {
        resetEatChannel();
        return;
    }
    const EatEvent ev =
        m_interaction.eatChannel.update(dt, m_input.isDown(MouseButton::Right));
    if (ev == EatEvent::Completed) {
        // The channel finished: consume one from the captured slot (its
        // validity was just re-checked). Food restores hunger plus the
        // hidden saturation buffer (core/food.hpp) and plays the eat
        // sound; a drinkable container (Phase 43) returns its empty
        // container to the inventory (start checked the capacity — the
        // add cannot fail) and clears the player's status effects (the
        // v1 set: the hurt flash; see clearStatusEffects).
        const Item& item = m_items.at(m_interaction.eatItem);
        m_inventory.take(m_interaction.eatSlot, 1);
        if (item.restoreHunger > 0) {
            m_player->restoreHunger(static_cast<float>(item.restoreHunger));
            // SURV-9: the per-food satiety (the 'saturation' field, the
            // Minecraft values; absent = the legacy 2x rule).
            m_player->restoreSaturation(foodSatiety(item));
            // Food that grants a status effect on a full eat (Phase 49,
            // golden apple; the list form Phase 56): applied through the
            // single choke point (applyPlayerEffect — the name is
            // cross-checked at init, the list entries IN ORDER, the instant
            // settlement for the SURV-4 effects). Bit-identical to the old
            // direct apply for the pre-SURV-4 food (no instant effects).
            if (!item.grantsEffect.empty()) {
                applyPlayerEffect(item.grantsEffect,
                                  item.grantsEffectDuration,
                                  item.grantsEffectTier);
            }
            for (const Item::GrantEffect& g : item.grantsEffects) {
                applyPlayerEffect(g.name, g.duration, g.tier);
            }
            syncAbsorption(); // Phase 72: the pool takes the new total
            m_sound.play("eat");
            // UX-7: the tutorial eat event (the frame the eat channel
            // FULLY COMPLETES — the hunger-grant point). The drink
            // completion (the else branch) does NOT emit (the brief's
            // documented deviation — a drink is not an eat).
            emitTutorialEvent("eat", 1);
        } else {
            if (!item.returnsItem.empty()) {
                m_inventory.add(m_items.idForName(item.returnsItem), 1,
                                m_items);
            }
            // SURV-4 (potions): a drinkable that GRANTS effects applies them
            // on a completed drink — the milk's clear is now the NO-GRANT
            // default, not the universal behavior. The grant entries apply
            // IN ORDER (the data's order), each through the single choke
            // point (applyPlayerEffect — the RESET semantics + the instant
            // settlement). A drinkable WITHOUT grants (milk / water) keeps
            // the original clear-negative behavior, bit-identical.
            if (!item.grantsEffect.empty() || !item.grantsEffects.empty()) {
                if (!item.grantsEffect.empty()) {
                    applyPlayerEffect(item.grantsEffect,
                                      item.grantsEffectDuration,
                                      item.grantsEffectTier);
                }
                for (const Item::GrantEffect& g : item.grantsEffects) {
                    applyPlayerEffect(g.name, g.duration, g.tier);
                }
                syncAbsorption(); // Phase 72: the pool takes the new total
            } else {
                clearStatusEffects(); // milk / water: the original clear
            }
            MC_LOG_DEBUG("drank '" + item.name + "'" +
                         (item.returnsItem.empty()
                              ? std::string()
                              : " (returned '" + item.returnsItem + "')"));
        }
        resetEatChannel();
    } else if (ev == EatEvent::Cancelled) {
        // Early release: nothing is consumed, no sound.
        resetEatChannel();
    }
}

void Game::resetEatChannel() {
    m_interaction.eatChannel.reset();
    m_interaction.eatItem = ItemID::invalid();
    m_interaction.eatStartHealth = -1.0f;
}

void Game::cycleHotbar() {
    m_selectedSlot =
        (m_selectedSlot + 1) % static_cast<std::size_t>(Inventory::kHotbarSize);
    // The selected item (and its break speed) changed: restart any break
    // in progress (Minecraft behavior), cancel an in-progress bow draw
    // (the launcher changed; Phase 24) and an in-progress eat (the food
    // left its slot; Phase 28).
    resetBreaking();
    resetBowDraw();
    resetEatChannel();
}

void Game::dropSelectedItem() {
    const ItemStack& stack = m_inventory.at(m_selectedSlot);
    if (stack.empty()) {
        return;
    }
    const Item& item = m_items.at(stack.item);
    if (m_inventory.take(m_selectedSlot, 1) == 0) {
        return;
    }
    // Spawn at the player's feet (bottom-center) with zero velocity — the
    // same determinism convention as block drops. The item's pickup delay
    // (see ItemEntity::kPickupDelaySeconds) keeps it from being
    // re-picked up on the next frame. SURV-3: the dropped unit carries
    // the stack's accumulated damage (captured BEFORE the take above —
    // an emptied stack would lose it; the take keeps the remainder's).
    // V2-12 (format 27): the dropped unit carries the stack's enchantments
    // too (the same captured-before-take discipline).
    const float droppedDamage = stack.damage;
    const std::map<std::string, int> droppedEnchants = stack.enchants;
    const Vec3 pos = m_player->position();
    currentEntities().push_back(std::make_unique<ItemEntity>(
        currentWorld(), m_blocks, item.name, item.maxStack, /*count=*/1, pos,
        droppedDamage, droppedEnchants));
    // CRAFT-3: the timbre is the generic "pickup" (items have no
    // sound_material — YAGNI, documented), only the spatialization is
    // added (at the drop position, the player's feet).
    m_sound.play("pickup", pos);
}

float Game::selectedBreakSpeed(const Block& target) const {
    const ItemStack& stack = m_inventory.at(m_selectedSlot);
    if (stack.empty()) {
        return 1.0f; // bare hands
    }
    const Item& item = m_items.at(stack.item);
    // Phase 67: the fast_against list (axes vs wood) is part of the
    // item's data — effectiveBreakSpeed applies it against `target`.
    float speed = effectiveBreakSpeed(item, target);
    // V2-12: EFFICIENCY — on the CORRECT tool (the target block is in the
    // item's fast_against list, the Minecraft rule: a wrong tool / bare
    // hand gets no efficiency), add (1 + L^2) to the break speed. L = the
    // held efficiency level (0 for an unenchantable / unenchanted item —
    // the pre-V2-12 speed is bit-identical).
    const int eff = efficiencyLevel(stack, m_enchants);
    if (eff > 0 && item.fastAgainstMultiplier > 0.0f) {
        bool fast = false;
        for (const std::string& n : item.fastAgainst) {
            if (n == target.name) {
                fast = true;
                break;
            }
        }
        if (fast) {
            speed += 1.0f + static_cast<float>(eff * eff);
        }
    }
    return speed;
}

bool Game::wearItem(ItemStack& stack, float wear) {
    // V2-12 (unbreaking): the tool / bow / igniter wear choke point. The
    // unbreaking level (0 = none, the pre-V2-12 bit-identical wear) rolls
    // the per-point skip; the state is the world seed ^ the stack's damage
    // BEFORE this event ^ the item id (NO frame dependence — the damage
    // history determines the roll, so a re-derivation is identical).
    const int L = unbreakingLevel(stack, m_enchants);
    const float applied =
        unbreakingWear(L, /*isArmor=*/false, wear,
                       unbreakingState(
                           static_cast<std::uint64_t>(m_generator->seed()),
                           stack.damage, stack.item.value));
    if (applied <= 0.0f) {
        return false; // the roll skipped the whole event (no wear, no break)
    }
    return applyDurability(stack, applied, m_items);
}

// ---------------------------------------------------------------------------
// Entities (Phase 10)
// ---------------------------------------------------------------------------

} // namespace mc
