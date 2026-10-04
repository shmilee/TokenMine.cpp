#include "world/day_cycle.hpp"

#include <algorithm>
#include <cmath>

#include "world/noise.hpp" // splitmix64 (starfield hashing)

namespace mc {

namespace {

// 2^53 — the largest integer exactly representable in a double; dividing a
// 53-bit integer by it yields a uniform [0, 1) value.
constexpr double kU53 = 9007199254740992.0;

double smoothstep01(double x) {
    x = std::clamp(x, 0.0, 1.0);
    return x * x * (3.0 - 2.0 * x);
}

struct Keyframe {
    double t;
    Vec3 color;
};

// Sky palette (RGB 0..1). kDaySky is the Phase 1-7 sky color, so a noon
// screenshot is unchanged by the day cycle.
constexpr Vec3 kNightSky {0.02f, 0.03f, 0.08f};
constexpr Vec3 kDawnSky {0.85f, 0.45f, 0.25f};
constexpr Vec3 kDaySky {0.47f, 0.71f, 0.95f};
constexpr Vec3 kDuskSky {0.85f, 0.40f, 0.20f};

// Sorted; the last entry wraps the [0, 1) domain (t < 1.0 always).
constexpr Keyframe kSkyKeyframes[] = {
    {0.00, kNightSky},
    {0.10, kDawnSky},
    {0.15, kDaySky},
    {0.45, kDaySky},
    {0.50, kDuskSky},
    {0.60, kNightSky},
    {1.00, kNightSky},
};

} // namespace

DayCycle::DayCycle(Params params) {
    // A zero/negative cycle length would divide by zero in timeOfDay().
    m_params = params;
    m_params.dayLengthSeconds = std::max(m_params.dayLengthSeconds, 1.0);
}

void DayCycle::update(double dt) {
    m_totalSeconds += dt;
}

void DayCycle::setTimeOfDay(double t) {
    t = std::fmod(t, 1.0);
    if (t < 0.0) {
        t += 1.0;
    }
    // timeOfDay() = fmod(startTimeOfDay + totalSeconds / dayLength, 1), so
    // solve for the total-seconds delta in [0, 1) cycles.
    double delta = std::fmod(t - m_params.startTimeOfDay, 1.0);
    if (delta < 0.0) {
        delta += 1.0;
    }
    m_totalSeconds = delta * m_params.dayLengthSeconds;
}

double DayCycle::timeOfDay() const {
    double t = m_params.startTimeOfDay + m_totalSeconds / m_params.dayLengthSeconds;
    t = std::fmod(t, 1.0);
    if (t < 0.0) {
        t += 1.0;
    }
    return t;
}

float DayCycle::dayFactor() const {
    const double t = timeOfDay();
    if (t < kDawnStart || t >= kDuskEnd) {
        return 0.0f; // night
    }
    if (t >= kDawnEnd && t < kDuskStart) {
        return 1.0f; // day
    }
    if (t < kDawnEnd) {
        // Dawn ramp [kDawnStart, kDawnEnd).
        return static_cast<float>(
            smoothstep01((t - kDawnStart) / (kDawnEnd - kDawnStart)));
    }
    // Dusk ramp [kDuskStart, kDuskEnd).
    return static_cast<float>(
        1.0 - smoothstep01((t - kDuskStart) / (kDuskEnd - kDuskStart)));
}

float DayCycle::starAlpha() const {
    return 1.0f - dayFactor();
}

Vec3 DayCycle::skyColor() const {
    const double t = timeOfDay();
    for (std::size_t i = 0; i + 1 < sizeof(kSkyKeyframes) / sizeof(kSkyKeyframes[0]);
         ++i) {
        if (t >= kSkyKeyframes[i].t && t < kSkyKeyframes[i + 1].t) {
            const Vec3& a = kSkyKeyframes[i].color;
            const Vec3& b = kSkyKeyframes[i + 1].color;
            if (a == b) {
                // Constant segment (the day/night plateaus): return the
                // exact keyframe value. A lerp of identical endpoints can
                // drift by 1 ulp in float, which would break byte-identical
                // screenshots.
                return a;
            }
            const double f = (t - kSkyKeyframes[i].t) /
                             (kSkyKeyframes[i + 1].t - kSkyKeyframes[i].t);
            return a + (b - a) * static_cast<float>(f);
        }
    }
    return kSkyKeyframes[0].color; // unreachable: t < 1.0
}

Vec3 DayCycle::sunDirection() const {
    // The sun travels a full circle once per day in a plane tilted from
    // east (+X) through south (-Z) to west (-X):
    //   t = 0    : east horizon
    //   t = 0.25 : zenith
    //   t = 0.5  : west horizon
    //   t = 0.75 : nadir (below the world)
    const double angle = 2.0 * M_PI * timeOfDay();
    const double azimuth = angle;                 // 0 = +X, PI = -X
    const double elevation = std::sin(angle) * (M_PI / 2.0);
    const Vec3 dir {
        static_cast<float>(std::cos(azimuth) * std::cos(elevation)),
        static_cast<float>(std::sin(elevation)),
        static_cast<float>(-std::sin(azimuth) * std::cos(elevation))};
    return normalized(dir);
}

Vec3 DayCycle::moonDirection() const {
    return -sunDirection();
}

std::vector<Vec3> DayCycle::makeStarPositions(std::uint64_t seed, int count,
                                              double radius) {
    std::vector<Vec3> out;
    out.reserve(count);
    for (int i = 0; i < count; ++i) {
        const std::uint64_t h =
            splitmix64(seed + static_cast<std::uint64_t>(i) * 0x9E3779B97F4A7C15ULL);
        const double u1 = static_cast<double>(h >> 11) / kU53;
        const double u2 = static_cast<double>(splitmix64(h) >> 11) / kU53;
        const double theta = 2.0 * M_PI * u1;
        // Uniform elevation between the horizon margin and the zenith.
        const double y = kStarHorizonMargin + (1.0 - kStarHorizonMargin) * u2;
        const double r = std::sqrt(1.0 - y * y);
        out.push_back({static_cast<float>(r * std::cos(theta) * radius),
                       static_cast<float>(y * radius),
                       static_cast<float>(r * std::sin(theta) * radius)});
    }
    return out;
}

} // namespace mc
