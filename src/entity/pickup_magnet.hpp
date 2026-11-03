// Pickup magnet (V2-6): the pure frame-locked pull of a READY drop (or a
// STUCK arrow) toward the player once the item's pickup delay has
// elapsed — the "fixed 1.0 block radius, no magnet" documented deviation
// consumed.
//
// Minecraft feel: the item is drawn in gradually — far = slow, close =
// fast (the approach ACCELERATES as the horizontal distance shrinks),
// with a small sideways swirl so the path is a tightening spiral rather
// a straight line. The pickup itself is untouched (radius 1.0, the 0.5 s
// delay); the magnet only moves the entity closer to it.
//
// Determinism (hard rule 8, the kMobStepPerFrame class): the pull is a
// FIXED per-frame displacement — a pure function of the entity/player
// positions, the global frame counter and the frame step, NEVER the wall
// clock — so headless runs are byte-reproducible. Beyond kMagnetRadius
// the pull is zero (the pre-V2-6 behavior: the fixed radius alone); a
// no-input run has no ready drops within the radius (zero drops at all
// in the 60-frame baseline) = zero motion = structural immunity.
#pragma once

#include <algorithm>
#include <cmath>

#include "core/math/vec3.hpp"

namespace mc {

// Pull radius (blocks, horizontal): entities further than this from the
// player's bottom-center are not drawn in (2.5 = the classic MC pickup
// pull range; the 1.0 pickup radius stays the collection gate).
inline constexpr float kMagnetRadius = 2.5f;
// The radial speed at the player (blocks/s): the squared falloff makes
// the far edge 0 and the approach visibly accelerate as the distance
// shrinks (the MC feel — a drop 2.4 away creeps, one 0.4 away snaps).
inline constexpr float kMagnetMaxSpeed = 6.0f;
// The swirl period in FRAMES (0.4 s at the 60 FPS cap — the item-bob's
// frame-derived pattern: a pure function of the frame counter, the
// kItemBobPeriodFrames discipline).
inline constexpr int kMagnetSwirlPeriodFrames = 24;
// The sideways wobble as a FRACTION of the radial speed (the spiral is
// tight: the radial pull always dominates, so the path monotone-
// approaches the player).
inline constexpr float kMagnetSwirlAmplitude = 0.25f;

// The radial (toward the player) pull speed (blocks/s) at horizontal
// distance `dist`: 0 at/beyond kMagnetRadius, monotonically growing to
// kMagnetMaxSpeed at the player. `dist` is clamped, so the function is
// total (no NaN at negative input).
inline float pickupMagnetRadialSpeed(float dist) {
    const float d = std::clamp(dist, 0.0f, kMagnetRadius);
    const float f = 1.0f - d / kMagnetRadius;
    return kMagnetMaxSpeed * f * f;
}

// The swirl phase (radians) at `frame`: 2π per kMagnetSwirlPeriodFrames,
// frame-BOUNDED by the modulo (the kRipplePeriodFrames discipline — the
// sin argument stays float32-exact over a long session; the displacement
// is periodic in `frame` with the period).
inline float pickupMagnetSwirlPhase(int frame) {
    const int bounded = frame % kMagnetSwirlPeriodFrames;
    return 2.0f * static_cast<float>(M_PI) *
           (static_cast<float>(bounded) /
            static_cast<float>(kMagnetSwirlPeriodFrames));
}

// This frame's pull displacement for an entity at `pos` relative to the
// player's bottom-center `player`: the radial step (toward the player)
// + the sideways swirl (90° about +Y), both scaled by the frame step
// `h` seconds. ZERO when the horizontal distance is 0 (the entity is at
// the player's feet — the pickup pass collects it; there is no
// direction, no NaN) or at/beyond kMagnetRadius. The y component is
// always 0: the pull is HORIZONTAL (the entity's own gravity keeps it on
// its surface; a ledge-gap pull is the player's to walk down).
inline Vec3 pickupMagnetDisplacement(const Vec3& pos, const Vec3& player,
                                     int frame, double h) {
    const float dx = player.x - pos.x;
    const float dz = player.z - pos.z;
    const float distSq = dx * dx + dz * dz;
    if (distSq <= 0.0f) {
        return Vec3::zero();
    }
    const float dist = std::sqrt(distSq);
    if (dist >= kMagnetRadius) {
        return Vec3::zero();
    }
    const float inv = 1.0f / dist;
    const float speed = pickupMagnetRadialSpeed(dist);
    const float step = static_cast<float>(h);
    const Vec3 radial {dx * inv, 0.0f, dz * inv};
    const Vec3 swirl {-dz * inv, 0.0f, dx * inv};
    return radial * (speed * step) +
           swirl * (std::sin(pickupMagnetSwirlPhase(frame)) *
                    kMagnetSwirlAmplitude * speed * step);
}

} // namespace mc
