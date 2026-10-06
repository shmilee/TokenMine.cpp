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

namespace mc {

// Seconds of held right button for a full eat (Minecraft: 1.6 s).
inline constexpr double kEatDurationSeconds = 1.6;

// Saturation (hidden satiety buffer, Phase 28) gained per point of
// restored hunger. Minecraft's cooked-food rule of thumb (saturation
// ~2x the hunger value); see docs/KNOWN_ISSUES for the v1 simplifications.
inline constexpr float kSaturationFactor = 2.0f;

enum class EatEvent {
    None,      // the channel is still running (or was inactive)
    Completed, // the channel reached the end: eat the item
    Cancelled, // the right button was released early: eat nothing
};

// The channel state. `start()` arms it with a full clock; `update()`
// advances it in real dt while `rightHeld` is true and reports the
// terminal event exactly once.
struct EatChannel {
    bool active = false;
    double remaining = 0.0;

    void start() {
        active = true;
        remaining = kEatDurationSeconds;
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
    // the held-item pull indicator (Phase 28 render).
    float progress() const {
        if (!active) {
            return 0.0f;
        }
        const float p = 1.0f -
                        static_cast<float>(remaining / kEatDurationSeconds);
        return p < 0.0f ? 0.0f : (p > 1.0f ? 1.0f : p);
    }
};

} // namespace mc
