// Eating channel model (Phase 28): the pure state machine for the
// Minecraft-style eat animation.
//
// Right-clicking with food selected does NOT eat instantly anymore: the
// player starts an eat channel that fills over kEatDurationSeconds (1.6 s,
// Minecraft's eat time) of HELD right button (real dt, like the other
// survival timers and the bow draw). Holding to the end consumes one item
// and restores hunger + saturation; releasing early cancels it (nothing is
// consumed). The game also cancels the channel on damage, death, pointer
// unlock, panel open, or a selection change (see Game::updateEatChannel).
//
// Header-only and side-effect free so the game and the unit tests share
// the exact model.
#pragma once

#include "item/item.hpp" // SURV-9: foodSatiety (the per-food field)

namespace mc {

// Seconds of held right button for a full eat (Minecraft: 1.6 s).
inline constexpr double kEatDurationSeconds = 1.6;

// Saturation (hidden satiety buffer, Phase 28) gained per point of
// restored hunger. Minecraft's cooked-food rule of thumb (saturation
// ~2x the hunger value); see docs/KNOWN_ISSUES for the v1 simplifications.
inline constexpr float kSaturationFactor = 2.0f;

// SURV-9: the food's satiety (the hidden saturation buffer gain on a
// completed eat): the item's 'saturation' field when present (the
// Minecraft per-food values — the raw meats' low saturation), else the
// Phase 28 legacy rule (2x the restored hunger; bit-identical for
// content without the field).
inline float foodSatiety(const Item& item) {
    return item.saturation.value_or(
        static_cast<float>(item.restoreHunger) * kSaturationFactor);
}

enum class EatEvent {
    None,      // the channel is still running (or was inactive)
    Completed, // the channel reached the end: eat the item
    Cancelled, // the right button was released early: eat nothing
};

// The channel state. `start()` arms it with a full clock (the default
// duration is kEatDurationSeconds, the eat; a drinkable container passes
// its data-driven drink_time, Phase 43); `update()` advances it in real
// dt while `rightHeld` is true and reports the terminal event exactly
// once.
struct EatChannel {
    bool active = false;
    double remaining = 0.0;
    double total = kEatDurationSeconds;

    void start(double duration = kEatDurationSeconds) {
        active = true;
        total = duration;
        remaining = duration;
    }

    void reset() {
        active = false;
        remaining = 0.0;
    }

    EatEvent update(double dt, bool rightHeld) {
        if (!active) {
            return EatEvent::None;
        }
        if (!rightHeld) {
            // Early release (Minecraft): the eat is aborted, the food
            // stays in the inventory.
            reset();
            return EatEvent::Cancelled;
        }
        remaining -= dt;
        if (remaining <= 0.0) {
            reset();
            return EatEvent::Completed;
        }
        return EatEvent::None;
    }

    // Channel progress in [0, 1] (0 = just started, 1 = would complete):
    // the held-item pull indicator (Phase 28 render). Normalized by the
    // channel's own duration (`total` — the data-driven drink_time for
    // drinkables, Phase 43).
    float progress() const {
        if (!active) {
            return 0.0f;
        }
        const float p =
            1.0f - static_cast<float>(remaining / total);
        return p < 0.0f ? 0.0f : (p > 1.0f ? 1.0f : p);
    }
};

} // namespace mc
