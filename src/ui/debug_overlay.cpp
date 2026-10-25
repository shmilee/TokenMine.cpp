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
        "TokenMine.cpp v" TM_VERSION,
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
        fmt("health %.1f / %.1f  absorption %.1f  armor %d",
            info.health, info.maxHealth, info.absorption, info.armorPoints),
        fmt("oxygen %d", info.oxygen), // SURV-2 (the air units left)
        fmt("hunger %.0f / %.0f", info.hunger, info.maxHunger),
        // SURV-3: the held item (name + " d/max" when it has a
        // durability; an unset / empty hand renders "(none)").
        info.heldItem.empty() ? std::string("held (none)")
                              : fmt("held %s", info.heldItem.c_str()),
        fmt("difficulty %s", info.difficulty.c_str()),
        // Only the line's data when active (an empty string renders as
        // "effects (none)" — the no-effect baseline state).
        info.effects.empty() ? std::string("effects (none)")
                             : fmt("effects %s", info.effects.c_str()),
    };
}

} // namespace mc
