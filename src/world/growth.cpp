#include "world/growth.hpp"

#include "world/noise.hpp"

namespace mc {

namespace {

// Dedicated salt so growth rolls never track the worldgen decoration rolls
// (which use their own salts — same discipline as the tree/decoration
// separation, Phase 45).
constexpr std::uint64_t kCropRollSalt = 0xC1B1D57E7AE0C9A1ULL;

// Reinterpret an (possibly negative) int as 64 bits for hashing (the
// worldgen's u64 — two's-complement is a bijection).
std::uint64_t asU64(int v) {
    return static_cast<std::uint64_t>(static_cast<std::int64_t>(v));
}

} // namespace

std::vector<GrowthStep> cropGrowthSteps(const World& world,
                                        const BlockRegistry& blocks,
                                        const std::map<BlockPos, BlockID>& cropCells,
                                        std::uint64_t seed,
                                        std::uint64_t frame) {
    std::vector<GrowthStep> steps;
    // Position-ordered iteration (std::map) — deterministic step list.
    for (const auto& [pos, tracked] : cropCells) {
        // Trust the world: the cell must still hold the tracked id (a
        // mismatch is a stale entry — defensive, the game maintains the
        // set through its setBlock choke point).
        const BlockID current = world.getBlock(pos.x, pos.y, pos.z);
        if (!current.valid() || current != tracked) {
            continue;
        }
        const Block& block = blocks.at(current);
        if (block.properties.growsTo.empty() ||
            block.properties.growthTicks < 1) {
            continue; // mature (or malformed) — never rolls
        }
        // The roll state advances with the frame (PITS #4) and is
        // per-cell via the position hash; splitmix64 finalizes.
        const std::uint64_t state = seed +
            frame * 0x9E3779B97F4A7C15ULL +
            hash3(asU64(pos.x), asU64(pos.y), asU64(pos.z), kCropRollSalt);
        const std::uint64_t roll = splitmix64(state);
        if (roll % static_cast<std::uint64_t>(block.properties.growthTicks) ==
            0) {
            const BlockID to = blocks.idForName(block.properties.growsTo);
            if (to.valid() && to != current) {
                steps.push_back(GrowthStep{pos, to});
            }
        }
    }
    return steps;
}

} // namespace mc
