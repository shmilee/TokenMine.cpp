#include "block/drop_scatter.hpp"

#include "world/noise.hpp"

namespace mc {

namespace {

// Dedicated base salt so the scatter never tracks the chance-drop rolls
// (kChanceDropSalt / kChanceDropsSalt in block/chance_drop.cpp) or the
// crop-growth / worldgen rolls (same discipline as the tree/decoration
// salt separation).
constexpr std::uint64_t kSaltDropScatter = 0x4E7A2C9D5B1F8E03ULL;

// The golden-ratio 64-bit constant (the chance_drops-entry / growth-roll
// convention) advances the base salt per drop index: advance FIRST, then
// mix (PITS #4 — splitmix64 is a pure finalizer; the index makes the
// input vary per drop, which decorrelates the drops of one break).
constexpr std::uint64_t kGolden64 = 0x9E3779B97F4A7C15ULL;

// Reinterpret an (possibly negative) int as 64 bits for hashing (two's
// complement is a bijection — the worldgen/growth convention).
std::uint64_t asU64(int v) {
    return static_cast<std::uint64_t>(static_cast<std::int64_t>(v));
}

} // namespace

Vec2 dropSpawnOffset(std::uint64_t seed, int x, int y, int z,
                     int dropIndex) {
    // The per-index salt (see the constants above), then the same
    // position-hash + finalizer chain as the chance-drop rolls.
    const std::uint64_t indexSalt =
        splitmix64(kSaltDropScatter +
                   static_cast<std::uint64_t>(dropIndex) * kGolden64);
    const std::uint64_t h =
        splitmix64(seed + hash3(asU64(x), asU64(y), asU64(z), indexSalt));
    // Two INDEPENDENT 32-bit halves (splitmix64 avalanches all 64 bits):
    // the top half drives the x component, the bottom half drives z. The
    // 32-bit values are mapped to [0,1) through double (exact for a
    // 32-bit integer — the chance_drop 53-bit convention, no mantissa
    // bucketing) and then to [-kDropScatterRange, +kDropScatterRange):
    const double kUnit = 1.0 / 4294967296.0; // 2^-32
    const double tx = static_cast<double>(h >> 32) * kUnit;
    const double tz = static_cast<double>(h & 0xFFFFFFFFULL) * kUnit;
    return {static_cast<float>((2.0 * tx - 1.0) * kDropScatterRange),
            static_cast<float>((2.0 * tz - 1.0) * kDropScatterRange)};
}

} // namespace mc
