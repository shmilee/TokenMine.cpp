// Phase 15: the F3 debug overlay text. The lines are computed by a pure
// function from a snapshot of game state, so the formatting is unit-
// testable without a running game (and deterministic for a given state).
//
// The FPS is an INTEGER on purpose: under vsync it is stable at 60, which
// keeps interactive-run screenshots byte-reproducible (a smoothed float
// FPS would differ by a last digit between runs).
#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "player/drown.hpp"

namespace mc {

struct DebugInfo {
    int fps = 0;
    int frame = 0;
    unsigned drawCalls = 0;
    unsigned long long vertices = 0;
    int chunks = 0;
    double posX = 0.0, posY = 0.0, posZ = 0.0;
    double yaw = 0.0, pitch = 0.0;
    int columnX = 0, columnZ = 0;
    std::int64_t seed = 0;
    double timeOfDay = 0.0;  // [0, 1)
    double dayFactor = 0.0;  // [0, 1]
    bool streamingEngaged = false;
    int streamPending = 0;
    int streamRadius = 0;
    bool pointerLocked = false;
    bool inventoryOpen = false;
    std::size_t particles = 0;
    std::size_t entities = 0; // Phase 19: the entity list (dropped items...)
    double health = 0.0;    // Phase 16
    double maxHealth = 0.0;
    double absorption = 0.0; // Phase 72 (the separate pool)
    int armorPoints = 0;     // SURV-1 (the capped equipped total)
    int oxygen = kOxygenMax; // SURV-2 (the air units left, [0, kOxygenMax])
    double hunger = 0.0;    // Phase 23
    double maxHunger = 0.0;
    // SURV-3: the held item's line ("(none)" on an empty hand, the item
    // name otherwise + " d/max" when the item has a durability).
    std::string heldItem;
    std::string difficulty; // Phase 25: "easy" | "normal" | "hard"
    // Phase 46: the active status effects, "name I 12.3s; ..."
    // (application order) or "" when the player has none. SURV-6 display
    // boundary (PRE-12): every active effect carries its amplifier roman
    // (MC convention: tier 0 = "I", 1 = "II", 2 = "III").
    std::string effects;
    // SURV-7 display boundary (PRE-12): the player stats (deathCount +
    // kill.<name>, key-ordered; m_stats holds non-zero entries only) or
    // empty when the player has none.
    std::map<std::string, std::uint64_t> stats;
};

// PRE-12 (the SURV-6 display boundary): the amplifier roman numeral for
// a 0-based tier (tier 0 = "I"); clamped to [0, 2] = the tier_max
// ceiling (SURV-6 tier_max ≤ 3).
const char* effectTierRoman(int tier);

// One line per entry, top to bottom.
std::vector<std::string> debugOverlayLines(const DebugInfo& info);

} // namespace mc
