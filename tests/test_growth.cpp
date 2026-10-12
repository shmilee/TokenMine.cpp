// Crop-growth tests (Phase 48): the deterministic per-tick roll
// (cropGrowthSteps). The pure function is registry-aware (it reads each
// cell's block's growsTo / growthTicks), so the fixture registers a
// wheat_1..wheat_7 chain. growth_ticks == 1 makes the roll pass EVERY
// tick (roll % 1 == 0 always), which turns the stochastic growth into an
// exact, fully deterministic sequence for precise assertions.
#include <gtest/gtest.h>

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "block/block_registry.hpp"
#include "world/growth.hpp"
#include "world/world.hpp"

using namespace mc;

namespace {

Json parseDef(const std::string& text) {
    auto r = Json::parse(text);
    if (!r.ok()) {
        ADD_FAILURE() << "parse failed: " << r.error().message;
        return Json::object();
    }
    return *r;
}

// A cross-shaped crop step: stage `n` (grows to n+1) with growthTicks.
// The mature stage (7) has no grows_to and drops wheat x2; the others
// drop a replantable wheat_1 seed.
std::string wheatDef(int n, int growthTicks) {
    const std::string drops = (n == 7)
        ? "\"drops\":\"wheat\",\"drops_count\":2"
        : "\"drops\":\"wheat_1\"";
    std::string s =
        std::string("{\"id\":\"wheat_") + std::to_string(n) +
        "\",\"properties\":{\"hardness\":0.0,\"solid\":false,"
        "\"opaque\":false,\"transparent\":true," + drops;
    if (n < 7) {
        s += ",\"grows_to\":\"wheat_" + std::to_string(n + 1) +
             "\",\"growth_ticks\":" + std::to_string(growthTicks);
    }
    s += "},\"model\":{\"shape\":\"cross\",\"textures\":{\"cross\":\"wheat_" +
         std::to_string(n) + "\"}}}";
    return s;
}

// A registry with the full wheat_1..wheat_7 chain at the given pace.
BlockRegistry wheatRegistry(int growthTicks) {
    BlockRegistry reg;
    for (int n = 1; n <= 7; ++n) {
        auto r = reg.registerBlock(parseDef(wheatDef(n, growthTicks)));
        if (!r.ok()) {
            throw std::runtime_error(r.error().message);
        }
    }
    return reg;
}

BlockID wheatId(const BlockRegistry& reg, int n) {
    return reg.idForName("wheat_" + std::to_string(n));
}

// One simulated game frame: compute the steps and apply them through the
// world + the tracking map (mirroring Game::setBlock maintenance).
void frame(World& world, const BlockRegistry& reg,
           std::map<BlockPos, BlockID>& crops, std::uint64_t seed,
           std::uint64_t frame) {
    const std::vector<GrowthStep> steps =
        cropGrowthSteps(world, reg, crops, seed, frame);
    for (const GrowthStep& s : steps) {
        world.setBlock(s.pos.x, s.pos.y, s.pos.z, s.to);
        crops[s.pos] = s.to;
    }
}

} // namespace

TEST(GrowthTest, EmptySetYieldsNoSteps) {
    World world;
    world.getOrCreateChunk({0, 0, 0});
    const BlockRegistry reg = wheatRegistry(1);
    std::map<BlockPos, BlockID> crops;
    EXPECT_TRUE(cropGrowthSteps(world, reg, crops, 42, 0).empty());
}

TEST(GrowthTest, MatureStageNeverRolls) {
    World world;
    world.getOrCreateChunk({0, 0, 0});
    const BlockRegistry reg = wheatRegistry(1);
    const BlockID mature = wheatId(reg, 7);
    world.setBlock(8, 1, 8, mature);
    std::map<BlockPos, BlockID> crops;
    crops[{8, 1, 8}] = mature;
    // wheat_7 has no grows_to -> never a step, even over many frames.
    for (std::uint64_t f = 0; f < 20; ++f) {
        EXPECT_TRUE(cropGrowthSteps(world, reg, crops, 7, f).empty());
    }
    EXPECT_EQ(world.getBlock(8, 1, 8), mature);
}

TEST(GrowthTest, StaleEntryYieldsNoStep) {
    World world;
    world.getOrCreateChunk({0, 0, 0});
    const BlockRegistry reg = wheatRegistry(1);
    const BlockID sprout = wheatId(reg, 1);
    // Tracked as a sprout but the world cell holds air (stale) -> no step.
    std::map<BlockPos, BlockID> crops;
    crops[{3, 4, 5}] = sprout;
    EXPECT_TRUE(cropGrowthSteps(world, reg, crops, 1, 0).empty());
}

TEST(GrowthTest, DeterministicIdenticalSteps) {
    World world;
    world.getOrCreateChunk({0, 0, 0});
    const BlockRegistry reg = wheatRegistry(1);
    const BlockID sprout = wheatId(reg, 1);
    world.setBlock(8, 1, 8, sprout);
    std::map<BlockPos, BlockID> crops;
    crops[{8, 1, 8}] = sprout;
    // Same world + set + seed + frame -> identical step list, every time.
    for (int i = 0; i < 5; ++i) {
        const std::vector<GrowthStep> a =
            cropGrowthSteps(world, reg, crops, 99, 123);
        const std::vector<GrowthStep> b =
            cropGrowthSteps(world, reg, crops, 99, 123);
        ASSERT_EQ(a.size(), b.size());
        for (std::size_t k = 0; k < a.size(); ++k) {
            EXPECT_EQ(a[k].pos, b[k].pos);
            EXPECT_EQ(a[k].to.value, b[k].to.value);
        }
    }
}

TEST(GrowthTest, AdvancesToMaturityThenStops) {
    World world;
    world.getOrCreateChunk({0, 0, 0});
    const BlockRegistry reg = wheatRegistry(1); // 1/1 -> advances every tick
    const BlockID sprout = wheatId(reg, 1);
    const BlockID mature = wheatId(reg, 7);
    world.setBlock(8, 1, 8, sprout);
    std::map<BlockPos, BlockID> crops;
    crops[{8, 1, 8}] = sprout;

    // Frame 0..: the crop advances one stage per frame until mature.
    for (std::uint64_t f = 0; f < 10; ++f) {
        frame(world, reg, crops, 42, f);
    }
    // 6 advances needed (1->7); 10 frames is more than enough.
    EXPECT_EQ(world.getBlock(8, 1, 8), mature);
    // Mature: the tracking entry is dropped by setBlock-style maintenance
    // (mature has no growsTo) — but even if left, it never rolls again.
    crops[{8, 1, 8}] = mature; // simulate a stale-but-present entry
    for (std::uint64_t f = 10; f < 30; ++f) {
        frame(world, reg, crops, 42, f);
    }
    EXPECT_EQ(world.getBlock(8, 1, 8), mature);
}

TEST(GrowthTest, StochasticPaceEventuallyMatures) {
    // A realistic pace (1/360 per tick): over enough frames the crop
    // matures with overwhelming (deterministic) certainty.
    World world;
    world.getOrCreateChunk({0, 0, 0});
    const BlockRegistry reg = wheatRegistry(360);
    const BlockID sprout = wheatId(reg, 1);
    const BlockID mature = wheatId(reg, 7);
    world.setBlock(8, 1, 8, sprout);
    std::map<BlockPos, BlockID> crops;
    crops[{8, 1, 8}] = sprout;

    // Expected advances over 40000 frames = 40000/360 ~= 111 >> 6 needed.
    for (std::uint64_t f = 0; f < 40000 && world.getBlock(8, 1, 8) != mature;
         ++f) {
        frame(world, reg, crops, 12345, f);
    }
    EXPECT_EQ(world.getBlock(8, 1, 8), mature);
}

TEST(GrowthTest, TwoCropsDecorrelate) {
    // Two crops at different positions must NOT advance in lockstep: there
    // exists a frame where exactly one of the two rolls (over a scan range).
    World world;
    world.getOrCreateChunk({0, 0, 0});
    const BlockRegistry reg = wheatRegistry(360);
    const BlockID sprout = wheatId(reg, 1);
    world.setBlock(0, 1, 0, sprout);
    world.setBlock(4, 1, 4, sprout);
    std::map<BlockPos, BlockID> crops;
    crops[{0, 1, 0}] = sprout;
    crops[{4, 1, 4}] = sprout;

    bool sawAsymmetric = false;
    for (std::uint64_t f = 0; f < 5000; ++f) {
        const std::vector<GrowthStep> steps =
            cropGrowthSteps(world, reg, crops, 777, f);
        const bool a = !steps.empty() && steps[0].pos == BlockPos{0, 1, 0};
        const bool b = steps.size() > 1 && steps[1].pos == BlockPos{4, 1, 4};
        if (a != b) {
            sawAsymmetric = true;
            break;
        }
    }
    EXPECT_TRUE(sawAsymmetric);
}
