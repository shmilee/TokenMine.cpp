#include "ui/debug_overlay.hpp"

#include <cstdarg>
#include <cstdio>

namespace mc {

namespace {

std::string fmt(const char* format, ...) {
    va_list args;
    va_start(args, format);
    char buf[160];
    std::vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);
    return buf;
}

} // namespace

std::vector<std::string> debugOverlayLines(const DebugInfo& info) {
    return {
        "MineCraft v0.0.1 (Phase 25)",
        fmt("%d FPS | frame %d", info.fps, info.frame),
        fmt("chunks %d | draws %u | verts %llu", info.chunks, info.drawCalls,
            info.vertices),
        fmt("pos %.2f, %.2f, %.2f  yaw %.2f  pitch %.2f", info.posX,
            info.posY, info.posZ, info.yaw, info.pitch),
        fmt("col (%d, %d)  seed %lld", info.columnX, info.columnZ,
            static_cast<long long>(info.seed)),
        fmt("time %.3f  day %.2f  %s", info.timeOfDay, info.dayFactor,
            info.pointerLocked ? "locked" : "free"),
        fmt("streaming %s  pending %d  radius %d",
            info.streamingEngaged ? "engaged" : "idle", info.streamPending,
            info.streamRadius),
        fmt("inventory %s  particles %zu  entities %zu",
            info.inventoryOpen ? "open" : "closed", info.particles,
            info.entities),
        fmt("health %.1f / %.1f", info.health, info.maxHealth),
        fmt("hunger %.0f / %.0f", info.hunger, info.maxHunger),
        fmt("difficulty %s", info.difficulty.c_str()),
        // Only the line's data when active (an empty string renders as
        // "effects (none)" — the no-effect baseline state).
        info.effects.empty() ? std::string("effects (none)")
                             : fmt("effects %s", info.effects.c_str()),
    };
}

} // namespace mc
