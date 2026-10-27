#include "world/growth.hpp"

#include <algorithm>

#include "world/coordinates.hpp"
#include "world/noise.hpp"
#include "world/tree_shape.hpp"

namespace mc {

namespace {

// Dedicated salt so growth rolls never track the worldgen decoration rolls
// (which use their own salts — same discipline as the tree/decoration
// separation, Phase 45).
constexpr std::uint64_t kCropRollSalt = 0xC1B1D57E7AE0C9A1ULL;

// GEN-2: the sapling roll's dedicated salt — independent of the crop rolls
// (a wheat field and a sapling beside it do not advance in lockstep) and
// of the tree HEIGHT roll (kSaltTreeGrowth, world/tree_shape).
constexpr std::uint64_t kSaplingRollSalt = 0xC1B1D57E7AE0C9A2ULL;

// GEN-2: the fits probe for a Complete — every planned cell must be AIR,
// DRY and in-world (the root cell is excepted: it holds the sapling and
// becomes the trunk base). Out-of-range-y reads as air from the World, so
// the y bound is checked explicitly (a canopy past the world top does not
// fit — the sapling waits, the Minecraft blocked rule).
bool treeFits(const World& world, const BlockPos& root,
              const std::vector<TreeCell>& cells) {
    for (const TreeCell& c : cells) {
        if (c.offset.x == 0 && c.offset.y == 0 && c.offset.z == 0) {
            continue; // the sapling cell itself
        }
        const int wx = root.x + c.offset.x;
        const int wy = root.y + c.offset.y;
        const int wz = root.z + c.offset.z;
        if (!isWorldYValid(wy)) {
            return false;
        }
        if (world.getBlock(wx, wy, wz) != BlockID::air() ||
            world.getFluid(wx, wy, wz) != 0) {
            return false;
        }
    }
    return true;
}

// Reinterpret an (possibly negative) int as 64 bits for hashing (the
// worldgen's u64 — two's-complement is a bijection).
std::uint64_t asU64(int v) {
    return static_cast<std::uint64_t>(static_cast<std::int64_t>(v));
}

} // namespace

// Water proximity for the dry/wet pace (Phase 50, the Minecraft
// farmland value): any WATER level within this CHEBYSHEV radius of the
// SOIL cell, at the soil cell's y, counts as water. A poured bucket's
// source sits on the same level as the farmland, so the bucket workflow
// works. LAVA does not wet (Phase 54, Minecraft: lava burns crops
// instead — the crop burn is a known gap; counting lava would let dry
// fields beside lava grow at full pace).
//
// GEN-9: this predicate is the SOIL's, not the crop's — the farmland
// moisture visual (BlockModel::moistureTop) derives the same wetness for
// the farmland cell itself, so the visual and the growth pace can never
// disagree (one function, two consumers). Public + pure (a read over the
// World's fluid field) so the mesher and the game both call it.
bool soilIsWet(const World& world, const BlockPos& soilPos) {
    for (int dx = -kWetWaterRadius; dx <= kWetWaterRadius; ++dx) {
        for (int dz = -kWetWaterRadius; dz <= kWetWaterRadius; ++dz) {
            if (world.getFluidOfKind(soilPos.x + dx, soilPos.y,
                                     soilPos.z + dz,
                                     FluidKind::Water) > 0) {
                return true;
            }
        }
    }
    return false;
}

// True when water is near the crop's soil cell (the cell below the crop).
bool cropIsWet(const World& world, const BlockPos& cropPos) {
    return soilIsWet(world,
                     BlockPos {cropPos.x, cropPos.y - 1, cropPos.z});
}

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

std::vector<SaplingStep> saplingGrowthSteps(const World& world,
                                            const BlockRegistry& blocks,
                                            const std::map<BlockPos, BlockID>& saplingCells,
                                            std::uint64_t seed,
                                            std::uint64_t frame) {
    std::vector<SaplingStep> steps;
    // Position-ordered iteration (std::map) — deterministic step list.
    for (const auto& [pos, tracked] : saplingCells) {
        // Trust the world: the cell must still hold the tracked id (a
        // mismatch is a stale entry — defensive, the game maintains the
        // set through its setBlock choke point).
        const BlockID current = world.getBlock(pos.x, pos.y, pos.z);
        if (!current.valid() || current != tracked) {
            continue;
        }
        const Block& block = blocks.at(current);
        if (!block.hasTree() || block.properties.growthTicks < 1) {
            continue; // not a sapling (or malformed) — never rolls
        }
        // The soil gate: the cell BELOW holds one of the plant_on soils
        // (the placement gate IS the growth gate — the sapling dies to
        // nothing, it just stops growing and waits, the documented
        // deviation). Absent list = unrestricted.
        if (!block.properties.plantOn.empty()) {
            const BlockID soil = world.getBlock(pos.x, pos.y - 1, pos.z);
            bool onSoil = false;
            if (soil.valid()) {
                for (const std::string& name : block.properties.plantOn) {
                    if (blocks.at(soil).name == name) {
                        onSoil = true;
                        break;
                    }
                }
            }
            if (!onSoil) {
                continue;
            }
        }
        // The light gate: max(sky, block) at the sapling cell (the crop
        // rule — a canopy above can shade the growth, LGT-2 makes it real).
        if (block.properties.growthLight > 0) {
            const int light = std::max(
                static_cast<int>(world.getSkyLight(pos.x, pos.y, pos.z)),
                static_cast<int>(
                    world.getBlockLight(pos.x, pos.y, pos.z)));
            if (light < block.properties.growthLight) {
                continue;
            }
        }
        // The roll (the crop pattern, the dedicated salt): the state
        // advances with the frame (PITS #4) and is per-cell via the
        // position hash; splitmix64 finalizes.
        const std::uint64_t state = seed +
            frame * 0x9E3779B97F4A7C15ULL +
            hash3(asU64(pos.x), asU64(pos.y), asU64(pos.z), kSaplingRollSalt);
        const std::uint64_t roll = splitmix64(state);
        if (roll % static_cast<std::uint64_t>(block.properties.growthTicks) != 0) {
            continue;
        }
        // The stage (the ENT-2 per-instance state — the crop's block id,
        // generalized): the sentinel (a hand-edited load) falls back to
        // the block's state default, the same rule as the mesh consumer.
        const int stage =
            world.getBlockState(pos.x, pos.y, pos.z) ==
                    Chunk::kBlockStateUnset
                ? block.stateDefault()
                : static_cast<int>(world.getBlockState(pos.x, pos.y, pos.z));
        if (stage < block.stateCount() - 1) {
            steps.push_back(SaplingStep{pos, SaplingAction::Advance,
                                        stage + 1, 0});
            continue;
        }
        // The final stage: roll the height (frame-INDEPENDENT — a blocked
        // retry re-checks the same planned tree) and probe the fits.
        TreeSpec spec = treeSpecFromBlock(block);
        const int height = treeHeightRoll(seed, pos, spec);
        if (!treeFits(world, pos, planTree(height, spec))) {
            continue; // no space — wait for the next roll (Minecraft)
        }
        steps.push_back(SaplingStep{pos, SaplingAction::Complete, stage,
                                    height});
    }
    return steps;
}

} // namespace mc
