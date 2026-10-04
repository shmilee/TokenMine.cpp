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
#include <string>
#include <vector>

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
    double health = 0.0;   // Phase 16
    double maxHealth = 0.0;
};

// One line per entry, top to bottom.
std::vector<std::string> debugOverlayLines(const DebugInfo& info);

} // namespace mc
