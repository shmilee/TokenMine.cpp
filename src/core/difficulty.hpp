// Difficulty (Phase 25): the game difficulty and its effects on hostile
// mob spawning and damage.
//
// Config: mobs.difficulty = "easy" | "normal" | "hard" | "peaceful"
// (default "normal").
// The mob data (assets/data/mobs) is the NORMAL-difficulty baseline; the
// other difficulties scale it:
//
//   - Spawn protection: hostile mobs do not naturally spawn within
//     spawnProtectionRadius(d) blocks (horizontal, from the WORLD spawn
//     point — the fresh-world spawn, not the player's current position).
//     The radii are Minecraft's (8 / 12 / 16). Passive mobs are exempt
//     (Minecraft's protection only covers hostiles).
//   - Hostile damage: a hostile's data attack_damage is multiplied by
//     hostileDamageScale(d) (zombie 3 -> easy 1.5 / normal 3 / hard 4,
//     mirroring Minecraft's 2.5 / 3 / 4 relative shape with normal = data).
//   - Spawn rate: the data cadence (check_every_frames) is divided by
//     hostileSpawnRateScale(d) (30 -> 60 / 30 / 20 attempt cadence). The
//     cadence is shared by all mob types (v1: a global attempt cadence,
//     not per-type weights).
//   - Peaceful (ENT-8): the spawn-protection radius is
//     kSpawnProtectionPeaceful (the whole world — every natural hostile
//     spawn is rejected, passive mobs stay exempt and spawn as usual).
//     The hostile damage / rate scales are defined for API consistency
//     (no hostile exists to consume them). Mobs are ephemeral (never
//     saved) and the difficulty is read at init only — there is no
//     runtime switch, so "existing hostiles fade" (the full Minecraft
//     peaceful) reduces to "no hostiles exist" (deviation).
//
// Header-only and side-effect free (like bow.hpp): the game and the unit
// tests share the exact values.
#pragma once

#include <cmath>
#include <string>

#include "core/error.hpp"

namespace mc {

enum class Difficulty { Easy, Normal, Hard, Peaceful };

// The config spelling of a difficulty (F3 overlay, logs).
inline const char* difficultyName(Difficulty d) {
    switch (d) {
    case Difficulty::Easy:
        return "easy";
    case Difficulty::Normal:
        return "normal";
    case Difficulty::Hard:
        return "hard";
    case Difficulty::Peaceful:
        return "peaceful";
    }
    return "normal";  // unreachable: all cases covered
}

// Strict: exactly "easy", "normal" or "hard" (case-sensitive, non-empty).
inline Result<Difficulty> parseDifficulty(const std::string& s) {
    if (s == "easy") {
        return Difficulty::Easy;
    }
    if (s == "normal") {
        return Difficulty::Normal;
    }
    if (s == "hard") {
        return Difficulty::Hard;
    }
    if (s == "peaceful") {
        return Difficulty::Peaceful;
    }
    return MC_FAIL("mobs.difficulty: must be 'easy', 'normal', 'hard' or "
                   "'peaceful', got '" +
                   s + "'");
}

// Spawn-protection radius in blocks (Minecraft: 8 / 12 / 16).
inline constexpr float kSpawnProtectionEasy = 8.0f;
inline constexpr float kSpawnProtectionNormal = 12.0f;
inline constexpr float kSpawnProtectionHard = 16.0f;
// ENT-8: the peaceful protection covers the whole world (a hostile
// spawn cell is always within it — every natural hostile spawn is
// rejected). 1e6 keeps the squared comparison in float32 range.
inline constexpr float kSpawnProtectionPeaceful = 1e6f;
// Hostile damage scale (see the file header).
inline constexpr float kHostileDamageScaleEasy = 0.5f;
inline constexpr float kHostileDamageScaleNormal = 1.0f;
inline constexpr float kHostileDamageScaleHard = 4.0f / 3.0f;
// ENT-8: no hostile exists on peaceful — the scale is defined for API
// consistency (and reads as "hostiles would deal no damage").
inline constexpr float kHostileDamageScalePeaceful = 0.0f;
// Hostile spawn-rate scale (attempts per unit time; see the header).
inline constexpr float kHostileSpawnRateScaleEasy = 0.5f;
inline constexpr float kHostileSpawnRateScaleNormal = 1.0f;
inline constexpr float kHostileSpawnRateScaleHard = 1.5f;
// ENT-8: the rate scale is moot on peaceful (no hostile spawns pass the
// protection) — 1.0 keeps the cadence arithmetic unchanged.
inline constexpr float kHostileSpawnRateScalePeaceful = 1.0f;

inline float spawnProtectionRadius(Difficulty d) {
    switch (d) {
    case Difficulty::Easy:
        return kSpawnProtectionEasy;
    case Difficulty::Normal:
        return kSpawnProtectionNormal;
    case Difficulty::Hard:
        return kSpawnProtectionHard;
    case Difficulty::Peaceful:
        return kSpawnProtectionPeaceful;
    }
    return kSpawnProtectionNormal;  // unreachable: all cases covered
}

inline float hostileDamageScale(Difficulty d) {
    switch (d) {
    case Difficulty::Easy:
        return kHostileDamageScaleEasy;
    case Difficulty::Normal:
        return kHostileDamageScaleNormal;
    case Difficulty::Hard:
        return kHostileDamageScaleHard;
    case Difficulty::Peaceful:
        return kHostileDamageScalePeaceful;
    }
    return kHostileDamageScaleNormal;  // unreachable: all cases covered
}

inline float hostileSpawnRateScale(Difficulty d) {
    switch (d) {
    case Difficulty::Easy:
        return kHostileSpawnRateScaleEasy;
    case Difficulty::Normal:
        return kHostileSpawnRateScaleNormal;
    case Difficulty::Hard:
        return kHostileSpawnRateScaleHard;
    case Difficulty::Peaceful:
        return kHostileSpawnRateScalePeaceful;
    }
    return kHostileSpawnRateScaleNormal;  // unreachable: all cases covered
}

// The spawn-attempt cadence in frames for a difficulty: the data base
// divided by the rate scale, rounded to the nearest frame, at least 1
// (base 30 -> 60 / 30 / 20 for easy / normal / hard).
inline int checkEveryFramesFor(int baseFrames, Difficulty d) {
    const int eff = static_cast<int>(std::lround(
        static_cast<double>(baseFrames) / hostileSpawnRateScale(d)));
    return eff < 1 ? 1 : eff;
}

} // namespace mc
