#include "world/growth.hpp"

#include <algorithm>

#include "world/noise.hpp"

namespace mc {

namespace {

// Dedicated salt so growth rolls never track the worldgen decoration rolls
// (which use their own salts — same discipline as the tree/decoration
// separation, Phase 45).
constexpr std::uint64_t kCropRollSalt = 0xC1B1D57E7AE0C9A1ULL;

// Water proximity for the dry/wet pace (Phase 50, the Minecraft
// farmland value): any fluid level within this CHEBYSHEV radius of the
// soil cell (the cell below the crop), at the soil cell's y, counts as
// water. A poured bucket's source sits on the same level as the farmland,
// so the bucket workflow works.
constexpr int kWetWaterRadius = 4;

// True when water is near the crop's soil cell (see kWetWaterRadius).
bool cropIsWet(const World& world, const BlockPos& cropPos) {
    const int soilY = cropPos.y - 1;
    for (int dx = -kWetWaterRadius; dx <= kWetWaterRadius; ++dx) {
        for (int dz = -kWetWaterRadius; dz <= kWetWaterRadius; ++dz) {
            if (world.getFluid(cropPos.x + dx, soilY, cropPos.z + dz) > 0) {
                return true;
            }
        }
    }
    return false;
}

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
        // Soil requirement (Phase 50, data-driven): the cell BELOW the crop
        // must hold the block named by growsOn (empty = no requirement —
        // pre-Phase-50 crops keep their old behavior). The hard gate: no
        // soil, no roll.
        if (!block.properties.growsOn.empty()) {
            const BlockID soil = world.getBlock(pos.x, pos.y - 1, pos.z);
            if (!soil.valid() ||
                blocks.at(soil).name != block.properties.growsOn) {
                continue;
            }
        }
        // Light requirement (Phase 50): the light level at the CROP cell
        // (max of sky and block light, the same combination the renderer
        // uses) must reach growthLight (0 = no requirement; 9 = the
        // Minecraft crop value).
        if (block.properties.growthLight > 0) {
            const int light = std::max(
                static_cast<int>(world.getSkyLight(pos.x, pos.y, pos.z)),
                static_cast<int>(world.getBlockLight(pos.x, pos.y, pos.z)));
            if (light < block.properties.growthLight) {
                continue;
            }
        }
        // Dry/wet pace (Phase 50): near water the crop rolls at full pace
        // (modulus growthTicks); dry, the modulus is multiplied by
        // drySlowdown (1 = no slowdown). The roll value is unchanged, so
        // every dry pass is also a wet pass — exactly 1/drySlowdown as
        // often, deterministic per (seed, frame, cell).
        const int ticks =
            block.properties.growthTicks *
            (cropIsWet(world, pos) ? 1 : block.properties.drySlowdown);
        // The roll state advances with the frame (PITS #4) and is
        // per-cell via the position hash; splitmix64 finalizes.
        const std::uint64_t state = seed +
            frame * 0x9E3779B97F4A7C15ULL +
            hash3(asU64(pos.x), asU64(pos.y), asU64(pos.z), kCropRollSalt);
        const std::uint64_t roll = splitmix64(state);
        if (roll % static_cast<std::uint64_t>(ticks) == 0) {
            const BlockID to = blocks.idForName(block.properties.growsTo);
            if (to.valid() && to != current) {
                steps.push_back(GrowthStep{pos, to});
            }
        }
    }
    return steps;
}

} // namespace mc
