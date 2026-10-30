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

// PRE-12: the stats map (already key-ordered) as "name N | name N".
std::string joinStats(const std::map<std::string, std::uint64_t>& stats) {
    std::string out;
    for (const auto& [name, value] : stats) {
        if (!out.empty()) {
            out += " | ";
        }
        out += name;
        out += ' ';
        out += std::to_string(value);
    }
    return out;
}

} // namespace

const char* effectTierRoman(int tier) {
    // SURV-6: tier is 0-based with tier_max ≤ 3 → tiers 0..2. The MC
    // convention renders amplifier 0 as "I". Clamp defensively (a
    // corrupted save is rejected at load, but the formatter must not
    // index out of bounds).
    static const char* const kRoman[3] = {"I", "II", "III"};
    if (tier < 0) {
        tier = 0;
    } else if (tier > 2) {
        tier = 2;
    }
    return kRoman[tier];
}

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
        // PRE-12 (the SURV-7 display boundary): the player stats in key
        // order, "name N" joined by " | "; "(none)" when the map is
        // empty (m_stats stores non-zero entries only — an empty map is
        // the no-deaths no-kills baseline state).
        info.stats.empty()
            ? std::string("stats (none)")
            : fmt("stats %s", joinStats(info.stats).c_str()),
        // UX-7: the tutorial line (the F3 stays English): "off" when
        // disabled, "done" when finished/skipped, "<id> p/c" in progress,
        // "(none)" for a unit-test DebugInfo with no data filled.
        info.tutorial.empty()
            ? std::string("tutorial (none)")
            : fmt("tutorial %s", info.tutorial.c_str()),
    };
}

} // namespace mc
