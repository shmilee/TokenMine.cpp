#include "block/chance_drop.hpp"

#include "world/noise.hpp"

namespace mc {

namespace {

// Dedicated salt so the chance-drop roll never tracks the crop-growth or
// worldgen rolls (which use their own salts — same discipline as the
// tree/decoration separation).
constexpr std::uint64_t kChanceDropSalt = 0x01B3E0D5E77C2A93ULL;

// Reinterpret an (possibly negative) int as 64 bits for hashing (two's
// complement is a bijection — the worldgen/growth convention).
std::uint64_t asU64(int v) {
    return static_cast<std::uint64_t>(static_cast<std::int64_t>(v));
}

} // namespace

bool rollChanceDrop(std::uint64_t seed, int x, int y, int z, double chance) {
    if (chance <= 0.0) {
        return false;
    }
    if (chance >= 1.0) {
        return true;
    }
    // A uniform [0,1) value from the hash's top 53 bits (the double mantissa
    // precision), compared against the chance. Deterministic per cell.
    const std::uint64_t h =
        splitmix64(seed + hash3(asU64(x), asU64(y), asU64(z), kChanceDropSalt));
    const double r = static_cast<double>(h >> 11) *
                     (1.0 / 9007199254740992.0); // 2^-53
    return r < chance;
}

} // namespace mc
