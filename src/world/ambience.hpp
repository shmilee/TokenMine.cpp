// PRE-4: the day/night/cave ambience (the frame-locked ambient sound
// schedule — the "render strip" audio twin of the clouds: pure
// presentation, no frame state).
//
// Three contexts (Minecraft's ambient-sound families, the minimal
// version):
//   - cave : the player's FEET cell has raw sky light 0 (no sky path —
//     the Minecraft "no sky" rule; takes priority over day/night)
//   - day  : dayFactor >= kAmbienceDayCutoff (the renderer's day/night
//     split)
//   - night: otherwise
//
// The SCHEDULE is frame-locked (hard rule 8, the day-cycle class): once
// every kAmbienceIntervalFrames (5 s at 60 fps) the game rolls the pure
// function ambienceRoll(worldSeed, frame, context) and plays the
// context's sound when the roll passes (kAmbienceProbability). Same
// (seed, frame, context) -> same answer across runs — the event
// sequence is deterministic (the E2E log evidence: the MC_LOG_INFO line
// per event; CRAFT-3's discipline that audio never enters the frame
// state keeps no-input runs byte-identical — the sound is heard, never
// seen or saved).
//
// PITS #4 discipline: the roll state advances with the frame by the
// golden step (splitmix64 is a pure finalizer — a constant input yields
// a constant stream); the context advances by a DIFFERENT constant
// (kAmbienceCtxStep — the hash3 decorrelation pattern) so no (frame,
// context) pair collides onto another's state.
#pragma once

#include <cstdint>

namespace mc {

// The ambience schedule (named constants — no config keys; the sound
// names themselves are code constants below, zero parse surface).
// The roll cadence: one roll every 300 frames (5 s at 60 fps).
inline constexpr int kAmbienceIntervalFrames = 300;
// The pass probability of a roll (Minecraft's ambient rhythm, the
// "about every other window" feel).
inline constexpr float kAmbienceProbability = 0.5f;
// The day/night split of the contexts: dayFactor >= 0.5 = day (the
// exact midpoint of the 0..1 sky-light scale).
inline constexpr float kAmbienceDayCutoff = 0.5f;
// The dedicated salt for the ambience roll (the kCropRollSalt pattern).
inline constexpr std::uint64_t kAmbienceSalt = 0xA1B65445ULL;

// The ambience context.
enum class AmbienceContext : int {
    Cave = 0,
    Day = 1,
    Night = 2,
};

// The context decision (pure): rawSkyLight = the player's feet cell's
// RAW sky light (0-15, the day/night unscaled stored value — 0 = no sky
// path = cave, priority over day/night); dayFactor = the same-day/night
// scale the renderer uses (0 = deep night, 1 = noon).
AmbienceContext ambienceContext(int rawSkyLight, float dayFactor);

// The schedule roll (pure): true = play the context's sound this frame.
// Deterministic in (seed, frame, context); ~kAmbienceProbability over a
// fixed context's frame sequence (the splitmix64 top bit).
bool ambienceRoll(std::uint64_t seed, std::uint64_t frame,
                  AmbienceContext context);

// The context's sound name (the wav's stem in assets/sounds — the code
// constant, zero parse surface; an unknown name is a silent no-op in
// the SoundBank, the asset-existence pin lives in the unit tests).
const char* ambienceSoundName(AmbienceContext context);

} // namespace mc
