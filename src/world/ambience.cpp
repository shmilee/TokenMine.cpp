#include "world/ambience.hpp"

#include "world/noise.hpp" // splitmix64

namespace mc {

namespace {

// The golden step (the PITS #4 advance: the roll state must ADVANCE
// with the frame before the finalizer — the kFeatureStep / kGoldenStep
// discipline).
constexpr std::uint64_t kAmbienceFrameStep = 0x9E3779B97F4A7C15ULL;
// The context step (a DIFFERENT constant from the frame step — the
// hash3 decorrelation pattern: no (frame, context) pair collides onto
// another's state).
constexpr std::uint64_t kAmbienceCtxStep = 0xBF58476D1CE4E5B9ULL;
// 2^53 — the largest integer exactly representable in a double; dividing
// a 53-bit integer by it yields a uniform [0, 1) value (the day_cycle
// starfield convention).
constexpr double kU53 = 9007199254740992.0;

} // namespace

AmbienceContext ambienceContext(int rawSkyLight, float dayFactor) {
    // The cave takes priority: no sky path at the player's feet cell is
    // the Minecraft "no sky" rule (a cave is a cave at night too).
    if (rawSkyLight == 0) {
        return AmbienceContext::Cave;
    }
    if (dayFactor >= kAmbienceDayCutoff) {
        return AmbienceContext::Day;
    }
    return AmbienceContext::Night;
}

bool ambienceRoll(std::uint64_t seed, std::uint64_t frame,
                  AmbienceContext context) {
    // The state advances with the frame (PITS #4: splitmix64 is a pure
    // finalizer — a constant input yields a constant stream); the
    // context advances by its own step. The dedicated salt decorrelates
    // the ambience from every other roll stream.
    const std::uint64_t state = seed +
                                frame * kAmbienceFrameStep + kAmbienceSalt +
                                static_cast<std::uint64_t>(context) *
                                    kAmbienceCtxStep;
    const double u = static_cast<double>(splitmix64(state) >> 11) / kU53;
    return u < kAmbienceProbability;
}

const char* ambienceSoundName(AmbienceContext context) {
    switch (context) {
    case AmbienceContext::Cave:
        return "ambience_cave";
    case AmbienceContext::Day:
        return "ambience_day";
    case AmbienceContext::Night:
        return "ambience_night";
    }
    // Unreachable (every enumerator is handled above) — the return keeps
    // the non-void path warning-free.
    return "ambience_day";
}

} // namespace mc
