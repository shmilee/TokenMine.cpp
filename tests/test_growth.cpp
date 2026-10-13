// Crop-growth tests (Phase 48): the deterministic per-tick roll
// (cropGrowthSteps). The pure function is registry-aware (it reads each
// cell's block's growsTo / growthTicks), so the fixture registers a
// wheat_1..wheat_7 chain. growth_ticks == 1 makes the roll pass EVERY
// tick (roll % 1 == 0 always), which turns the stochastic growth into an
// exact, fully deterministic sequence for precise assertions.
#include <gtest/gtest.h>

#include <cstdint>
#include <map>
#include <optional>
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

// ---------------------------------------------------------------------------
// Environment conditions (Phase 50): soil / light / water proximity.
// growth_ticks = 1 in these fixtures so the roll passes every frame the
// environment allows — the condition under test is the only variable.
// ---------------------------------------------------------------------------

namespace {

// A plain cube def (soil / stand-in blocks).
std::string cubeDef(const std::string& id) {
    return std::string("{\"id\":\"") + id +
           "\",\"properties\":{\"hardness\":1.0},"
           "\"model\":{\"textures\":{\"top\":\"t\",\"side\":\"t\","
           "\"bottom\":\"t\"}}}";
}

// A 2-stage crop chain crop_a -> crop_b (b mature) with the Phase 50
// environment fields: soil ("" = no requirement), minLight (0 = none),
// slowdown (1 = none).
BlockRegistry envRegistry(const std::string& soil, int minLight,
                          int slowdown) {
    BlockRegistry reg;
    auto soilR = reg.registerBlock(parseDef(cubeDef("soil")));
    if (!soilR.ok()) {
        throw std::runtime_error(soilR.error().message);
    }
    std::string a = std::string("{\"id\":\"crop_a\",\"properties\":{") +
        "\"hardness\":0.0,\"grows_to\":\"crop_b\",\"growth_ticks\":1";
    if (!soil.empty()) {
        a += ",\"grows_on\":\"" + soil + "\"";
    }
    if (minLight > 0) {
        a += ",\"growth_light\":" + std::to_string(minLight);
    }
    if (slowdown > 1) {
        a += ",\"dry_slowdown\":" + std::to_string(slowdown);
    }
    a += "},\"model\":{\"textures\":{\"top\":\"t\",\"side\":\"t\","
         "\"bottom\":\"t\"}}}";
    auto aR = reg.registerBlock(parseDef(a));
    if (!aR.ok()) {
        throw std::runtime_error(aR.error().message);
    }
    auto bR = reg.registerBlock(
        parseDef(std::string("{\"id\":\"crop_b\",\"properties\":{") +
                 "\"hardness\":0.0"
                 "},\"model\":{\"textures\":{\"top\":\"t\",\"side\":\"t\","
                 "\"bottom\":\"t\"}}}"));
    if (!bR.ok()) {
        throw std::runtime_error(bR.error().message);
    }
    return reg;
}

// One crop at (8,2,8); soil (if any) at (8,1,8); sky light `light` at the
// crop cell; `waterAt` optional (dx from the soil cell, same soil y).
struct EnvScene {
    World world;
    std::map<BlockPos, BlockID> crops;
    BlockID a;
    BlockID b;

    EnvScene(BlockRegistry reg, const std::string& soilName, int light,
             std::optional<int> waterDx = std::nullopt)
        : a(reg.idForName("crop_a")), b(reg.idForName("crop_b")) {
        world.getOrCreateChunk({0, 0, 0});
        world.setBlock(8, 2, 8, a);
        crops[{8, 2, 8}] = a;
        if (!soilName.empty()) {
            world.setBlock(8, 1, 8, reg.idForName(soilName));
        }
        if (light > 0) {
            world.findChunkMut({0, 0, 0})->setSkyLightAt(8, 2, 8, light);
        }
        if (waterDx.has_value() && *waterDx != 0) {
            world.setFluid(8 + *waterDx, 1, 8, FluidKind::Water, 8);
        }
    }

    // Count the steps the pure function yields over frames [0, n) WITHOUT
    // applying them (the crop stays at stage a — the roll is stage-blind).
    int stepsOver(const BlockRegistry& reg, std::uint64_t seed, int n) {
        int nSteps = 0;
        for (int f = 0; f < n; ++f) {
            nSteps += static_cast<int>(
                cropGrowthSteps(world, reg, crops, seed,
                                static_cast<std::uint64_t>(f)).size());
        }
        return nSteps;
    }
};

} // namespace

TEST(GrowthEnvTest, AbsentEnvironmentKeepsOldBehavior) {
    // No environment fields (pre-Phase-50 data): air below, no light,
    // no water — the crop still rolls exactly as before.
    const BlockRegistry reg = envRegistry("", 0, 1);
    EnvScene s(reg, "", 0);
    EXPECT_EQ(s.stepsOver(reg, 42, 20), 20); // ticks 1 -> every frame
}

TEST(GrowthEnvTest, MissingSoilBlocksGrowth) {
    const BlockRegistry reg = envRegistry("soil", 0, 1);
    EnvScene s(reg, "", 0); // soil REQUIRED, but the cell below is air
    EXPECT_EQ(s.stepsOver(reg, 42, 20), 0);
}

TEST(GrowthEnvTest, WrongSoilBlocksGrowth) {
    const BlockRegistry reg = envRegistry("soil", 0, 1);
    EnvScene s(reg, "crop_b", 0); // below holds crop_b, not the required soil
    EXPECT_EQ(s.stepsOver(reg, 42, 20), 0);
}

TEST(GrowthEnvTest, CorrectSoilAllowsGrowth) {
    const BlockRegistry reg = envRegistry("soil", 0, 1);
    EnvScene s(reg, "soil", 0);
    EXPECT_EQ(s.stepsOver(reg, 42, 20), 20);
}

TEST(GrowthEnvTest, LightBelowThresholdBlocksGrowth) {
    const BlockRegistry reg = envRegistry("soil", 9, 1);
    EnvScene s(reg, "soil", 8); // 8 < 9
    EXPECT_EQ(s.stepsOver(reg, 42, 20), 0);
}

TEST(GrowthEnvTest, LightAtThresholdAllowsGrowth) {
    const BlockRegistry reg = envRegistry("soil", 9, 1);
    EnvScene s(reg, "soil", 9); // exactly 9
    EXPECT_EQ(s.stepsOver(reg, 42, 20), 20);
}

TEST(GrowthEnvTest, BlockLightCountsThroughMax) {
    // max(sky, block): sky 5 + block 12 passes a 9 threshold.
    const BlockRegistry reg = envRegistry("soil", 9, 1);
    EnvScene s(reg, "soil", 5);
    s.world.findChunkMut({0, 0, 0})->setBlockLightAt(8, 2, 8, 12);
    EXPECT_EQ(s.stepsOver(reg, 42, 20), 20);
    // And max(5, 5) = 5 < 9 still blocks.
    const BlockRegistry reg2 = envRegistry("soil", 9, 1);
    EnvScene s2(reg2, "soil", 5);
    s2.world.findChunkMut({0, 0, 0})->setBlockLightAt(8, 2, 8, 5);
    EXPECT_EQ(s2.stepsOver(reg2, 42, 20), 0);
}

TEST(GrowthEnvTest, WaterNearSoilRemovesSlowdown) {
    const BlockRegistry reg = envRegistry("soil", 0, 2);
    // Dry (no water): the roll modulus is 2 — strictly fewer passes.
    EnvScene dry(reg, "soil", 0);
    const int drySteps = dry.stepsOver(reg, 42, 200);
    // Wet (water 2 from the soil, same y): full pace, every frame.
    EnvScene wet(reg, "soil", 0, 2);
    const int wetSteps = wet.stepsOver(reg, 42, 200);
    EXPECT_EQ(wetSteps, 200);
    EXPECT_GT(drySteps, 0);
    EXPECT_LT(drySteps, wetSteps);
}

TEST(GrowthEnvTest, WaterBoundaryAtRadius4) {
    const BlockRegistry reg = envRegistry("soil", 0, 2);
    // Chebyshev distance 4 from the soil cell => still water (wet pace).
    EnvScene at4(reg, "soil", 0, 4);
    EXPECT_EQ(at4.stepsOver(reg, 42, 200), 200);
    // Distance 5 => dry pace.
    EnvScene at5(reg, "soil", 0, 5);
    EXPECT_LT(at5.stepsOver(reg, 42, 200), 200);
}

TEST(GrowthEnvTest, WaterAtOtherYDoesNotCount) {
    const BlockRegistry reg = envRegistry("soil", 0, 2);
    // Water at the CROP's y (soil y + 1), 2 away: not the soil level.
    World world;
    world.getOrCreateChunk({0, 0, 0});
    world.setBlock(8, 2, 8, reg.idForName("crop_a"));
    world.setBlock(8, 1, 8, reg.idForName("soil"));
    world.setFluid(10, 2, 8, FluidKind::Water, 8); // y = 2 = the crop's y, not the soil's y=1
    std::map<BlockPos, BlockID> crops;
    crops[{8, 2, 8}] = reg.idForName("crop_a");
    int nSteps = 0;
    for (int f = 0; f < 200; ++f) {
        nSteps += static_cast<int>(
            cropGrowthSteps(world, reg, crops, 42,
                            static_cast<std::uint64_t>(f)).size());
    }
    EXPECT_LT(nSteps, 200); // dry pace — the water is at the wrong level
}

TEST(GrowthEnvTest, LavaNearSoilDoesNotWet) {
    // Phase 54: the wet-pace counts WATER only. Lava beside the soil
    // (Minecraft: it would burn the crop, not water it) keeps the dry
    // pace — EXACTLY the no-fluid scene's step count (same modulus).
    const BlockRegistry reg = envRegistry("soil", 0, 2);
    EnvScene dry(reg, "soil", 0);
    const int drySteps = dry.stepsOver(reg, 42, 200);
    ASSERT_GT(drySteps, 0);
    ASSERT_LT(drySteps, 200); // sanity: the dry pace is slower here
    EnvScene lava(reg, "soil", 0);
    lava.world.setFluid(10, 1, 8, FluidKind::Lava, 8); // 2 from the soil, same y
    EXPECT_EQ(lava.stepsOver(reg, 42, 200), drySteps);
    // And water at the same spot DOES wet (the wet path stays alive).
    EnvScene wet(reg, "soil", 0, 2);
    EXPECT_GT(wet.stepsOver(reg, 42, 200), drySteps);
}
