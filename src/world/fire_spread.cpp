#include "world/fire_spread.hpp"

#include <algorithm>

#include "world/noise.hpp"

namespace mc {

namespace {

// Dedicated salt so spread rolls never track the crop growth rolls (which
// use their own salt — same discipline as the worldgen tree/decoration
// salt separation, Phase 45) or the chance-drop rolls (Phase 49/52).
constexpr std::uint64_t kFireSpreadRollSalt = 0xF17E5A3D0B4C9E21ULL;

// Second golden-ratio advance so the neighbor index decorrelates the five
// rolls of one fire cell (PITS #4: the state must advance per roll).
constexpr std::uint64_t kFireSpreadNeighborAdvance = 0xC2B2AE3D27D4EB4FULL;

// Reinterpret an (possibly negative) int as 64 bits for hashing (the
// worldgen's u64 — two's-complement is a bijection).
std::uint64_t asU64(int v) {
    return static_cast<std::uint64_t>(static_cast<std::int64_t>(v));
}

} // namespace

std::vector<BlockPos> fireSpreadIgnitions(const World& world,
                                          const BlockRegistry& blocks,
                                          const std::vector<BlockPos>& firePositions,
                                          std::uint64_t seed,
                                          std::uint64_t frame,
                                          double chance) {
    if (firePositions.empty() || chance <= 0.0) {
        return {};
    }
    // Fast exit: no flamable block in the registry -> nothing to ignite
    // (the lava burn pass uses the same gate, Phase 58).
    bool anyFlamable = false;
    for (const BlockID id : blocks.allIds()) {
        if (blocks.at(id).properties.flamable) {
            anyFlamable = true;
            break;
        }
    }
    if (!anyFlamable) {
        return {};
    }

    std::vector<BlockPos> ignitions;
    for (const BlockPos& pos : firePositions) {
        for (int i = 0; i < 5; ++i) {
            const BlockPos n = {pos.x + kFireSpreadNeighbors[i].x,
                                pos.y + kFireSpreadNeighbors[i].y,
                                pos.z + kFireSpreadNeighbors[i].z};
            const BlockID current = world.getBlock(n.x, n.y, n.z);
            if (!current.valid() || !blocks.at(current).properties.flamable) {
                continue;
            }
            if (chance < 1.0) {
                // The roll state advances with the frame (PITS #4) and is
                // per (fire cell, neighbor) via the position hash +
                // neighbor advance; splitmix64 finalizes into [0, 1).
                const std::uint64_t state = seed +
                    frame * 0x9E3779B97F4A7C15ULL +
                    hash3(asU64(pos.x), asU64(pos.y), asU64(pos.z),
                          kFireSpreadRollSalt) +
                    static_cast<std::uint64_t>(i) *
                        kFireSpreadNeighborAdvance;
                const double u =
                    static_cast<double>(splitmix64(state) >> 11) /
                    static_cast<double>(1ull << 53);
                if (u >= chance) {
                    continue;
                }
            }
            ignitions.push_back(n);
        }
    }
    // A flammable cell adjacent to two fire cells rolls per fire neighbor
    // (any pass ignites it) — collapse it to one ignition (sorted, so the
    // apply order is deterministic).
    std::sort(ignitions.begin(), ignitions.end());
    ignitions.erase(std::unique(ignitions.begin(), ignitions.end()),
                    ignitions.end());
    return ignitions;
}

} // namespace mc
