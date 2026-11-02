// V2-4: the weather state machine (rain/thunder) — the PURE function the
// unit tests own (the campfire pattern: the game is the thin adapter).
//
// Model (session-derived — NOT saved, zero format; the cloud-drift
// precedent class): a CLEAR/RAIN segment alternation (the first segment
// is ALWAYS CLEAR), each segment's duration rolled from its dedicated
// range the moment the previous segment ends, driven by the WORLD
// SEED's splitmix64 ADVANCING stream (PITS #4: state += golden BEFORE
// the finalizer — the finalizer alone does not progress; a constant
// input would yield a constant stream). Thunder is a RAIN-internal
// schedule on an INDEPENDENT salted stream (one strike every
// [kThunderMin, kThunderMax) frames of rain). Same world seed -> same
// weather sequence across runs (hard rule 8: the game steps the machine
// once per fixed 1/60 frame, so the event sequence is frame-locked).
//
// The 60-frame no-input baseline (the base gate) never sees rain: the
// initial CLEAR duration is >= kClearMin (9000 frames = 150 s) — the
// structural-immunity root.
//
// Audio never enters the frame state (the PRE-4 discipline: the thunder
// sound is heard, never seen or saved). The presentation half (the rain
// drop field, the sky dimming, the thunder pulse) is the renderer
// boundary's; the constants + the pure drop oracle live here so the unit
// tests own them.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "core/config.hpp"
#include "core/error.hpp"
#include "core/math/vec3.hpp"

namespace mc {

// -- the segment / strike ranges (fixed 1/60 frames) --------------------
// CLEAR 150-300 s / RAIN 50-100 s: the Minecraft ratio compressed to
// this project's day_length 600 s scale (the v2.4.0 design decision —
// the unit tests pin the exact first-five-cycle sequence).
inline constexpr int kClearMin = 9000;
inline constexpr int kClearMax = 18000;
inline constexpr int kRainMin = 3000;
inline constexpr int kRainMax = 6000;
// Thunder: one strike every 10-30 s of rain (the independent stream).
inline constexpr int kThunderMin = 600;
inline constexpr int kThunderMax = 1800;

// -- presentation constants (the renderer boundary consumes them) -------
// RAIN dims the sky by this factor on the per-frame dayFactor channel
// (the single convergence point: the u_dayFactor uniform + the same-
// frame sky clear the fog color inherits; the shader expression trees
// are untouched).
inline constexpr float kRainSkyDim = 0.85f;
// The thunder's sky-brightness pulse: kThunderPulseFrames of linear
// decay starting at kThunderPulseLevel (0 the frame after the window).
inline constexpr int kThunderPulseFrames = 5;
inline constexpr float kThunderPulseLevel = 0.15f;
// The rain drop field: kRainDropCount drops in the kRainBoxSize^3 box
// around the player (one thin bar quad per drop — 0.02 x 0.5, double-
// sided, alpha-blended, no texture; the dedicated shader program is the
// cloud program's fourth precedent: a static VBO with zero per-frame
// upload, the vertex shader re-deriving each drop's position from the
// frame-bounded fall offset + the player-anchored box origin).
inline constexpr int kRainDropCount = 300;
inline constexpr int kRainBoxSize = 16;
inline constexpr int kRainBoxHalf = kRainBoxSize / 2;
// The drop fall speed in blocks per fixed frame. kRainDropFallSpeed *
// kRainFallPeriodFrames = kRainBoxSize — the seamless-wrap condition the
// bounded fall-offset uniform relies on (the magnitude stays float32-
// exact over a long session; the cloud shader's float premise).
inline constexpr float kRainDropFallSpeed = 1.0f;
inline constexpr int kRainFallPeriodFrames = kRainBoxSize;
inline constexpr float kRainDropWidth = 0.02f;
inline constexpr float kRainDropLength = 0.5f;
inline constexpr float kRainDropAlpha = 0.35f;
// The drop's white-blue (200, 210, 235) / 255 (the design's no-texture
// color).
inline constexpr float kRainDropColorR = 200.0f / 255.0f;
inline constexpr float kRainDropColorG = 210.0f / 255.0f;
inline constexpr float kRainDropColorB = 235.0f / 255.0f;

// The dedicated salt for the THUNDER stream (seed = world seed + salt —
// collision-checked against every kSalt* / k*RollSalt / kAmbienceSalt in
// src/ in the 32-bit domain AND the zero-extended 64-bit domain: no
// other system hashes with it, and the thunder stream never tracks the
// main CLEAR/RAIN duration stream).
inline constexpr std::uint64_t kSaltWeatherThunder = 0x7E42B1D9ULL;
// The dedicated salt for the RAIN DROP field generation (same discipline
// — the drop field never tracks any other deterministic stream).
inline constexpr std::uint64_t kSaltRainDrop = 0xD93A5E17ULL;

// The session's weather state (the machine's bookkeeping; the game owns
// the instance + the two streams).
struct WeatherState {
    bool raining = false;
    // Frames left in the CURRENT segment (the segment flips at 0 and the
    // new duration is rolled in the same step).
    int framesLeft = 0;
    // RAIN only: frames until the next thunder strike (re-rolled after
    // every strike; 0 = inactive in CLEAR).
    int thunderLeft = 0;
};

// The frame's weather events (the game logs them verbatim + plays the
// thunder; no event = the frame's state is unchanged but advanced).
struct WeatherEvent {
    bool enteredRain = false;
    bool exitedRain = false;
    bool thunder = false;
};

// The session's initial state: the first segment is ALWAYS CLEAR with
// its duration rolled from `stream` (>= kClearMin — the no-input
// baseline's structural immunity); `startRaining` (the --weather rain
// override) begins in RAIN with the FIXED kRainMin opening duration (the
// design decision: the initial rain length is constant, reproducible) +
// the first thunder gap rolled from the independent `thunderStream`
// (the main stream is left unconsumed — the override touches the initial
// state only, the streams take over from there).
WeatherState weatherInitialState(bool startRaining, std::uint64_t& stream,
                                 std::uint64_t& thunderStream);

// One fixed 1/60 frame step of the machine (see the file header for the
// segment/thunder model). Pure over (state, streams) — no game, no
// clock: same inputs, same event + state, across runs and threads.
WeatherEvent weatherStep(WeatherState& state, std::uint64_t& stream,
                         std::uint64_t& thunderStream);

// The thunder's sky-brightness pulse (the presentation pure function):
// kThunderPulseLevel on `eventFrame`, decaying linearly to 0 over
// kThunderPulseFrames; 0 before the event, after the window, or with
// eventFrame < 0 (no strike yet).
float thunderPulse(int frame, int eventFrame);

// One rain drop's parameters (the static VBO's per-vertex data, uploaded
// once at init): the spawn offset in the kRainBoxSize^3 box (each axis in
// [0, kRainBoxSize)) + the fall phase in [0, 1) (a boxSize-unit vertical
// offset that decorrelates the drops' fall).
struct RainDrop {
    Vec3 offset {0.0f, 0.0f, 0.0f};
    float phase = 0.0f;
};

// The kRainDropCount drop field: the dedicated kSaltRainDrop advancing
// stream over the world seed (PITS #4 — each axis/phase roll consumes
// one stream value; same world, same drops across runs).
std::vector<RainDrop> makeRainDrops(std::int64_t seed);

// The CPU ORACLE of rain.vert: the drop's box-local position at fixed
// frame `frame` (the y-axis fall wrapped in the box; x/z constant). The
// host adds the player-anchored box origin at draw time (u_boxOrigin).
// frame >= 0 (the machine's frame count — the game never steps it
// negative).
Vec3 rainDropPosition(const RainDrop& drop, int frame);

// The --weather CLI word (the parseDifficulty precedent — strict, the
// unknown value named and rejected): "clear" = false, "rain" = true.
Result<bool> parseWeather(const std::string& value);

// render.weather (strict read — the SAVE-1 discipline: the Config
// getters would SILENTLY fall back to the default on a type mismatch,
// so the raw value is read): absent key = true (the code default, the
// render.clouds precedent — the committed config.json carries no such
// key); a present key must be a JSON boolean; anything else is named and
// rejected at init.
Result<bool> weatherEnabled(const Config& config);

} // namespace mc
