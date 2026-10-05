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

} // namespace mc
