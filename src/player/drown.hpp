// SURV-2: the player's oxygen / drowning model — the pure frame-locked
// step + the head-under-fluid predicate, as a module the unit tests own
// (the campfire pattern: the decision + every state write live here; the
// Game's per-frame pass is the thin adapter that feeds the returned
// damage into Player::damage).
//
// Minecraft semantics (verified against the wiki damage table): 15 s of
// air (150 units, 1 unit per 2 MC ticks = 1 unit per 6 frames at this
// project's 60 fps frame lock), then 2.0 HP of drowning damage every
// 2.5 s (150 frames) while the head stays under fluid — WATER or LAVA
// (Minecraft: lava drowns the same way). Breathing (the head out of the
// fluid) refills the air instantly. Drowning damage BYPASSES the armor
// reduction (wiki: Drowning = "Armor: No effect") but the resistance
// effect and the absorption pool apply as for any other damage (the
// Player::damage pipeline — the bypass flag skips the armor step only).
//
// Frame lock: drownStep is driven by the absolute frame counter (the
// Game's m_frameCount, 0-based) — the fixed-1/60 class like the day
// cycle, so a no-input run is byte-reproducible at any host frame rate.
// Exact timeline (head continuously under fluid, counted from frame 0,
// global phase frame % kOxygenDecayFrames):
//   - frames 0..894: one air unit per 6 frames -> oxygen 0 after frame
//     894 (14.9 s)
//   - frame 895 on: the drownTicks accumulator runs -> FIRST damage at
//     frame 1044 (17.4 s), then every 150 frames
//   - a save preset to oxygen 0: damages on frames 149/299/.../1499
//     (10 damages = 20 HP = the death frame is 1499)
//
// Out of scope (documented deviations, see docs/KNOWN_ISSUES): the
// bubble/breath sounds (Minecraft's bubble noise is random =
// non-deterministic), mob drowning (Minecraft mobs do not drown), the
// Respiration enchantment / water breathing (no such systems in this
// project), and config keys (the Minecraft values are the named
// constants — YAGNI).
#pragma once

#include <cstddef>

#include "player/player.hpp"
#include "world/world.hpp"

namespace mc {

// The air cap: 150 units = 15 s of breath (Minecraft's 15 s, 60 fps
// frame lock: 900 frames at the decay pace below).
inline constexpr int kOxygenMax = 150;
// Air decay: one unit per 6 frames (Minecraft: one unit per 2 ticks).
inline constexpr int kOxygenDecayFrames = 6;
// Drowning damage: 2.0 HP (kDrownDamage) every 150 frames (2.5 s) once
// the air is exhausted (Minecraft's pace).
inline constexpr int kDrownDamageFrames = 150;
inline constexpr float kDrownDamage = 2.0f;
// The HUD oxygen bubble row (which REPLACES the hunger row while the
// air is below full — Minecraft: the hunger bar hides while the air bar
// shows): 10 bubbles, icon i fills while oxygen > i * kOxygenPerIcon
// (the fill count is ceil(oxygen / 15), no half-state — the texture is
// a single tile, an unfilled position draws nothing).
inline constexpr int kOxygenBarIconCount = 10;
inline constexpr int kOxygenPerIcon = kOxygenMax / kOxygenBarIconCount;

// The player's drowning state. The Game owns the single instance; the
// save persists oxygen only — drownTicks is NOT saved (a load restarts
// the accumulator at 0, the tick-phase convention of the status effects
// — the documented deviation: Minecraft saves the air supply too).
struct DrownState {
    int oxygen = kOxygenMax;  // the air units left, [0, kOxygenMax]
    int drownTicks = 0;       // frames since exhaustion, [0, kDrownDamageFrames)
};

// One frame of the drowning model. `frame` is the current absolute
// frame counter (0-based; only its kOxygenDecayFrames phase matters).
// Returns the damage dealt THIS frame (0 or kDrownDamage) for the
// adapter to feed into Player::damage. The logic order IS the semantics:
//   1. the head out of the fluid: breathe — oxygen refills, the
//      accumulator resets (Minecraft: the surface is instant).
//   2. oxygen > 0: on the decay phase, one unit is consumed.
//   3. oxygen == 0: the accumulator counts frames; at
//      kDrownDamageFrames it resets and deals kDrownDamage.
// A dead player is the adapter's problem (Player::damage no-ops); the
// step itself keeps ticking harmlessly.
float drownStep(DrownState& state, bool headInFluid, unsigned frame);

// The head-under-fluid predicate (Minecraft's "eyes in the water" rule):
// the SINGLE cell the eye position (Player::eyePosition — the crouch-
// reduced eye height) lies in holds any fluid (water OR lava — the lava
// drowns too). Standing in a 1-deep pool (the eyes above the water
// surface) breathes; the surface float (the feet at the water plane, the
// eyes 1.62 above it) breathes; diving / a 2-deep pool does not. An
// out-of-range or uncreated cell reads as air (the World convention).
bool headInFluid(const Player& player, const World& world);

} // namespace mc
