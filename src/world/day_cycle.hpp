// Day cycle: deterministic in-game time of day driving the sky-light
// scale, the sky background color, and the sun/moon/star presentation.
//
// Time model: timeOfDay in [0, 1) — 0 = darkest pre-dawn, 0.25 = noon,
// 0.5 = sunset, 0.75 = midnight. The sun path matches: sunDirection(0) is
// the east horizon, (0.25) the zenith, (0.5) the west horizon, (0.75) the
// nadir. The moon is always opposite the sun.
//
// Day/night is a PRESENTATION-layer re-enactment of the night propagation
// (LGT-3, the Minecraft-style light retreat): light values stay stored at
// their daytime (source level 15) 0..15 values, and the effective sky
// level at dayFactor f is the source-affine re-derivation max(0, stored
// - 8 * (1 - f)) — the exact field a re-propagation with sky source
// level 7 + 8 * f would produce (kNightSkyLevel 7 = the Minecraft
// moonlight floor, so deep night is moonlit, never all black; see
// effectiveSkyLight in world/lighting.hpp). No re-propagation runs: the
// renderer (basic.frag) and the spawn gate (currentSpawnLight) evaluate
// the same formula per cell, so the lit region retreats toward cave
// mouths and canopy gaps along the stored = 8 * (1 - f) contour as night
// falls. Block light (glowstone) is unaffected by day/night.
//
// Determinism: update() is driven by a fixed per-frame step from the game
// loop (never the wall clock), so headless runs are byte-reproducible
// frame for frame.
#pragma once

#include <cstdint>
#include <vector>

#include "core/math/vec3.hpp"

namespace mc {

// Day-cycle parameters (config: world.day_length_seconds /
// world.start_time_of_day). Namespace scope (not nested) so the default
// argument below can value-initialize it.
struct DayCycleParams {
    double dayLengthSeconds = 240.0; // one full cycle
    double startTimeOfDay = 0.25;    // noon
};

class DayCycle {
public:
    using Params = DayCycleParams;

    // Sky-light ramp edges (timeOfDay fractions). Night is
    // [kDuskEnd, 1) + [0, kDawnStart), full day is [kDawnEnd, kDuskStart);
    // the ramps in between are smoothstep.
    static constexpr double kDawnStart = 0.05;
    static constexpr double kDawnEnd = 0.15;
    static constexpr double kDuskStart = 0.45;
    static constexpr double kDuskEnd = 0.55;
    // Stars sit at least this fraction of the radius above the horizon.
    static constexpr double kStarHorizonMargin = 0.05;

    explicit DayCycle(Params params = {});

    // Advance by dt seconds of game time.
    void update(double dt);

    // Set the time of day directly (save/load, Phase 13). t is normalized
    // into [0, 1); the stored total-seconds is the unique value whose
    // timeOfDay() lands on t (at most a 1-ulp difference from t for
    // values far from the configured start time — the day/night ramps are
    // constant or smooth there, so the presentation is unaffected).
    void setTimeOfDay(double t);

    double timeOfDay() const; // [0, 1)
    float dayFactor() const;  // 0..1 sky-light multiplier (0 = deep night)
    float starAlpha() const;  // 0..1 star visibility (1 = deep night)
    // Background color, interpolated over the day (exact keyframe values on
    // the constant day/night plateaus — see .cpp).
    Vec3 skyColor() const;
    // Unit vectors from the world origin toward the sun / moon.
    Vec3 sunDirection() const;
    Vec3 moonDirection() const;

    // Deterministic starfield: `count` positions on the upper hemisphere,
    // `radius` from the world origin. Same inputs always give the same
    // positions (splitmix64-hashed); every point is at least
    // kStarHorizonMargin * radius above the horizon.
    static std::vector<Vec3> makeStarPositions(std::uint64_t seed, int count,
                                               double radius);

private:
    Params m_params;
    double m_totalSeconds = 0.0;
};

} // namespace mc
