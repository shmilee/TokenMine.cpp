// PRE-2: the distance-fog range for the streaming disc edge.
//
// In steady state the stream disk holds R_eff = min(view_distance,
// kStreamCapRadius) chunk columns around the player; beyond its rim the
// world is raw chunk cross-sections — a hard world edge in view. A linear
// distance fog that ramps to the same-frame sky color dissolves that edge
// into the horizon (the Minecraft fog semantics).
//
// This file is the pure function behind it. The world constants (the far
// plane kFar, the far-plane cap kStreamCapRadius, kChunkSize) live in
// src/core/game.cpp and are PASSED IN here — never duplicated, so a
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

} // namespace mc
