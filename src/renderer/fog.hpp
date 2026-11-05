// PRE-2: the distance-fog range for the streaming disc edge.
//
// In steady state the stream disk holds R_eff = min(view_distance,
// kStreamCapRadius) chunk columns around the player; beyond its rim the
// world is raw chunk cross-sections — a hard world edge in view. A linear
// distance fog that ramps to the same-frame sky color dissolves that edge
// into the horizon (the Minecraft fog semantics).
//
// This file is the pure function behind it. The world constants (the far
// plane kFar in src/core/game_render.cpp, the far-plane cap
// kStreamCapRadius in src/core/game_local.hpp, kChunkSize in
// world/coordinates.hpp) are PASSED IN here — never duplicated, so a
// far-plane change stays a one-line edit there.
#pragma once

namespace mc {

// The fog start as a fraction of the fog end (named constant, zero
// config): the last quarter of the disc radius is the transition band.
constexpr float kFogStartScale = 0.75f;

struct FogRange {
    float start = 0.0f;
    float end = 0.0f;
};

// end = min(farPlane, streamRadiusBlocks); start = kFogStartScale * end.
// streamRadiusBlocks is the effective stream radius in BLOCKS
// (kChunkSize * min(view_distance, kStreamCapRadius) — the caller does
// the cap; this function only sees blocks). A zero or negative radius
// yields a degenerate range (end <= start) that the caller turns off —
// the fog must never be enabled without a real disc edge.
FogRange fogDistanceRange(float streamRadiusBlocks, float farPlane);

// V3-6: the fill-aware clamp on the fog end: the static end (the
// existing PRE-2 value — the far-plane / stream-radius-clamped
// overworld end, or the nether's constant end) further limited by the
// actually-filled disk's Chebyshev rim in BLOCKS (the caller converts
// filledChebyshevRadius * kChunkSize — the world constants stay
// passed-in, the fog.hpp premise). A fully filled disk (
// filledRadiusBlocks >= staticEnd) returns staticEnd bit-for-bit —
// the steady-state bit-identity red line.
float fillClampedEnd(float staticEnd, float filledRadiusBlocks);

// V3-6: the fill-aware overworld range: end = min(farPlane,
// streamRadiusBlocks, filledRadiusBlocks); start = kFogStartScale *
// end (the scale tracks the CLAMPED end — the ramp always ends where
// the filled rim is). A fully filled disk (filledRadiusBlocks >=
// min(farPlane, streamRadiusBlocks)) returns fogDistanceRange(
// streamRadiusBlocks, farPlane) bit-for-bit — the steady-state red
// line.
FogRange fogFilledRange(float streamRadiusBlocks, float filledRadiusBlocks,
                        float farPlane);

} // namespace mc
