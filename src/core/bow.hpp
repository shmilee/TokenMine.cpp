// Bow charge model (Phase 24): the pure mapping from draw time to shot
// power.
//
// Minecraft's bow: holding the draw fills the charge over 1.6 s, and a
// shot's arrow speed and damage scale with the charge (an instant release
// still fires, at 10 % power). The game accumulates the draw in real dt
// while the right button is held with a launcher selected (like the other
// survival timers); the release fires ONE arrow with
//
//     speed  = shoot_speed  * power
//     damage = shoot_damage * power
//     power  = clamp(drawSeconds / kBowFullDrawSeconds, kBowMinPower, 1)
//
// Header-only and side-effect free so the game and the unit tests share
// the exact mapping.
#pragma once

#include <algorithm>

#include "core/math/vec3.hpp"

namespace mc {

// Seconds of draw for a full-power shot (Minecraft: 1.6 s).
inline constexpr double kBowFullDrawSeconds = 1.6;
// Power of an instant release (Minecraft's minimum; the arrow still
// flies).
inline constexpr float kBowMinPower = 0.1f;

// The shot power for a given draw time (see the file header).
inline float bowPower(double drawSeconds) {
    const double t = drawSeconds / kBowFullDrawSeconds;
    return static_cast<float>(
        std::clamp(t, static_cast<double>(kBowMinPower), 1.0));
}

// Mob-hit knockback (Phase 42): a melee-style impulse along the arrow's
// horizontal velocity (constant magnitude) + a vertical lift, scaled by
// the shot's charge power. A full-power shot (power 1.0) reproduces the
// Phase 21/27 constants bit-exactly. `arrowVelocity` is the arrow's live
// velocity at impact; a (near-)horizontal-zero velocity keeps the lift
// and drops the horizontal push.
inline constexpr float kArrowKnockbackSpeed = 4.0f; // horizontal (blocks/s)
inline constexpr float kArrowKnockbackLift = 2.5f;  // vertical (blocks/s)

inline Vec3 arrowKnockback(const Vec3& arrowVelocity, float power) {
    Vec3 kb {arrowVelocity.x, 0.0f, arrowVelocity.z};
    const float len = std::sqrt(kb.x * kb.x + kb.z * kb.z);
    if (len > 1e-4f) {
        kb.x *= kArrowKnockbackSpeed * power / len;
        kb.z *= kArrowKnockbackSpeed * power / len;
    }
    kb.y = kArrowKnockbackLift * power;
    return kb;
}

} // namespace mc
