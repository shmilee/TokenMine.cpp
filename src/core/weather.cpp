// V2-4: the weather state machine's implementation (see the header for
// the model). Every function here is a pure function of its arguments
// (the streams advance in place — the PITS #4 discipline) so the unit
// tests own the sequence; the game wires it to the frame clock, the log
// lines, the thunder sound and the presentation.
#include "core/weather.hpp"

#include <cmath>

#include "world/noise.hpp" // splitmix64 (the advancing-stream finalizer)

namespace mc {

namespace {

// PITS #4: splitmix64 is a PURE finalizer — the advancing stream adds
// the golden step BEFORE the mix (a constant input would yield a
// constant stream; the cloud/timestep advance, the same constant).
constexpr std::uint64_t kGoldenStep = 0x9E3779B97F4A7C15ULL;

// 2^53 — the largest integer exactly representable in a double;
// (h >> 11) / kU53 yields a uniform [0, 1) value (the starfield
// reduction, world/day_cycle).
constexpr double kU53 = 9007199254740992.0;

// One advancing-stream value (the PITS #4 step-then-mix).
std::uint64_t advanceMix(std::uint64_t& state) {
    state += kGoldenStep;
    return splitmix64(state);
}

// One uniform [0, 1) roll off the advancing stream.
double unitRoll(std::uint64_t& state) {
    return static_cast<double>(advanceMix(state) >> 11) / kU53;
}

// One [min, max) integer roll (the 53-bit mantissa scaled into the range
// — the roll's top bit is the splitmix64 finalizer's, the reduction is a
// pure function of it, so same stream, same range -> same duration).
int rangeRoll(std::uint64_t& state, int min, int max) {
    return min + static_cast<int>(unitRoll(state) * (max - min));
}

} // namespace

WeatherState weatherInitialState(bool startRaining, std::uint64_t& stream,
                                 std::uint64_t& thunderStream) {
    WeatherState s;
    if (startRaining) {
        // The --weather rain override: the FIXED kRainMin opening rain
        // length (the design decision — reproducible) + the first thunder
        // gap from the INDEPENDENT salt stream. The main `stream` is left
        // unconsumed: the override touches the initial state only.
        s.raining = true;
        s.framesLeft = kRainMin;
        s.thunderLeft = rangeRoll(thunderStream, kThunderMin, kThunderMax);
    } else {
        // The first segment is ALWAYS CLEAR (>= kClearMin — the 60-frame
        // no-input baseline never sees rain, the structural-immunity root).
        s.framesLeft = rangeRoll(stream, kClearMin, kClearMax);
    }
    return s;
}

WeatherEvent weatherStep(WeatherState& state, std::uint64_t& stream,
                         std::uint64_t& thunderStream) {
    WeatherEvent ev;
    --state.framesLeft;
    if (state.framesLeft <= 0) {
        // The current segment ends this frame: flip it (the CLEAR ->
        // RAIN -> CLEAR alternation) and roll the new segment's duration
        // from the main stream in the same step.
        state.raining = !state.raining;
        if (state.raining) {
            ev.enteredRain = true;
            state.framesLeft = rangeRoll(stream, kRainMin, kRainMax);
            state.thunderLeft =
                rangeRoll(thunderStream, kThunderMin, kThunderMax);
        } else {
            ev.exitedRain = true;
            state.framesLeft = rangeRoll(stream, kClearMin, kClearMax);
            state.thunderLeft = 0; // the schedule is RAIN-internal only
        }
    } else if (state.raining) {
        // The thunder schedule: one strike per gap, re-rolled from the
        // independent salt stream after every strike (the gap is counted
        // in frames of rain, starting at the rain's entry frame).
        --state.thunderLeft;
        if (state.thunderLeft <= 0) {
            ev.thunder = true;
            state.thunderLeft =
                rangeRoll(thunderStream, kThunderMin, kThunderMax);
        }
    }
    return ev;
}

float thunderPulse(int frame, int eventFrame) {
    // No strike yet, or the frame precedes the event: no pulse.
    if (eventFrame < 0) {
        return 0.0f;
    }
    const int age = frame - eventFrame;
    if (age < 0 || age >= kThunderPulseFrames) {
        return 0.0f; // before the event / past the 5-frame window
    }
    // (5, 4, 3, 2, 1) / 5 x the level: +0.15 on the strike frame, linear
    // decay to 0 over kThunderPulseFrames.
    return static_cast<float>(kThunderPulseFrames - age) /
           static_cast<float>(kThunderPulseFrames) * kThunderPulseLevel;
}

std::vector<RainDrop> makeRainDrops(std::int64_t seed) {
    std::vector<RainDrop> drops;
    drops.reserve(kRainDropCount);
    // The dedicated kSaltRainDrop advancing stream over the world seed
    // (the PITS #4 step-then-mix; each axis/phase roll consumes one
    // stream value — the drops never track any other deterministic
    // stream, and same world, same drops across runs).
    std::uint64_t state = static_cast<std::uint64_t>(seed) + kSaltRainDrop;
    for (int i = 0; i < kRainDropCount; ++i) {
        RainDrop d;
        d.offset.x = static_cast<float>(unitRoll(state) * kRainBoxSize);
        d.offset.y = static_cast<float>(unitRoll(state) * kRainBoxSize);
        d.offset.z = static_cast<float>(unitRoll(state) * kRainBoxSize);
        d.phase = static_cast<float>(unitRoll(state));
        drops.push_back(d);
    }
    return drops;
}

Vec3 rainDropPosition(const RainDrop& drop, int frame) {
    // The CPU oracle of rain.vert: the vertical fall wraps in the box.
    // The bounded (frame % kRainFallPeriodFrames) term keeps the magnitudes
    // float32-exact over a long session (kRainDropFallSpeed *
    // kRainFallPeriodFrames = kRainBoxSize, so the wrap is seamless — the
    // full expression fallSpeed * (frame + phase) mod boxSize, restated
    // in bounded form).
    const float fallOffset =
        static_cast<float>(frame % kRainFallPeriodFrames) * kRainDropFallSpeed;
    const float y = std::fmod(drop.offset.y + fallOffset +
                                 drop.phase * static_cast<float>(kRainBoxSize),
                             static_cast<float>(kRainBoxSize));
    return Vec3 {drop.offset.x, y, drop.offset.z};
}

Result<bool> parseWeather(const std::string& value) {
    if (value == "clear") {
        return false;
    }
    if (value == "rain") {
        return true;
    }
    return MC_FAIL("invalid --weather: '" + value +
                   "' (use clear|rain)");
}

Result<bool> weatherEnabled(const Config& config) {
    const Json* j = config.at("render.weather");
    if (j == nullptr) {
        return true; // absent: the code default (the render.clouds
                     // precedent — config.json is untouched)
    }
    if (!j->isBool()) {
        return MC_FAIL("config render.weather must be a boolean "
                       "(true/false), got a non-boolean");
    }
    return j->asBool();
}

} // namespace mc
