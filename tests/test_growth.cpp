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
#include <set>
#include <string>
#include <tuple>
#include <vector>

#include "block/block_registry.hpp"
#include "world/growth.hpp"
#include "world/lighting.hpp"
#include "world/tree_shape.hpp"
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

// ---------------------------------------------------------------------------
// GEN-9: soilIsWet — the soil-cell wetness predicate (the farmland moisture
// visual's one function; the crop dry_slowdown is the same rule viewed from
// the crop's cell). World fixture: a bare chunk, water placed by level.
// ---------------------------------------------------------------------------

TEST(GrowthTest, SoilIsWetWithinRadius4Inclusive) {
    World world;
    world.getOrCreateChunk({0, 0, 0});
    const BlockPos soil {8, 1, 8};
    // Dry by default.
    EXPECT_FALSE(soilIsWet(world, soil));
    // Chebyshev distance 4 (the corner) still wets (inclusive radius).
    world.setFluid(12, 1, 12, FluidKind::Water, 8);
    EXPECT_TRUE(soilIsWet(world, soil));
    world.setFluid(12, 1, 12, FluidKind::Water, 0); // drained
    EXPECT_FALSE(soilIsWet(world, soil));
    // Distance 5 dries.
    world.setFluid(13, 1, 8, FluidKind::Water, 8);
    EXPECT_FALSE(soilIsWet(world, soil));
}

TEST(GrowthTest, SoilIsWetOnlyAtTheSoilY) {
    World world;
    world.getOrCreateChunk({0, 0, 0});
    const BlockPos soil {8, 1, 8};
    // Water one level ABOVE the soil (the crop's own level): no wet.
    world.setFluid(9, 2, 8, FluidKind::Water, 8);
    EXPECT_FALSE(soilIsWet(world, soil));
    world.setFluid(9, 2, 8, FluidKind::Water, 0);
    // Water one level BELOW the soil: no wet.
    world.setFluid(9, 0, 8, FluidKind::Water, 8);
    EXPECT_FALSE(soilIsWet(world, soil));
    world.setFluid(9, 0, 8, FluidKind::Water, 0);
    // Water at the soil's own y: wet.
    world.setFluid(9, 1, 8, FluidKind::Water, 8);
    EXPECT_TRUE(soilIsWet(world, soil));
}

TEST(GrowthTest, SoilIsWetIgnoresLava) {
    // Phase 54: lava burns instead of wets (the crop's known-gap burn).
    World world;
    world.getOrCreateChunk({0, 0, 0});
    const BlockPos soil {8, 1, 8};
    world.setFluid(10, 1, 8, FluidKind::Lava, 8);
    EXPECT_FALSE(soilIsWet(world, soil));
}

TEST(GrowthTest, CropIsWetIsSoilIsWetOneBelow) {
    // The crop-facing wrapper delegates to the soil predicate one level
    // down (the soil is the cell below the crop) — the two consumers can
    // never disagree.
    World world;
    world.getOrCreateChunk({0, 0, 0});
    const BlockPos crop {8, 2, 8};
    const BlockPos soil {8, 1, 8};
    world.setFluid(10, 1, 8, FluidKind::Water, 8); // the soil's level
    EXPECT_TRUE(cropIsWet(world, crop));
    EXPECT_TRUE(soilIsWet(world, soil));
    world.setFluid(10, 1, 8, FluidKind::Water, 0);
    world.setFluid(10, 2, 8, FluidKind::Water, 8); // the crop's level only
    EXPECT_FALSE(cropIsWet(world, crop));
    EXPECT_FALSE(soilIsWet(world, soil));
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

// ---------------------------------------------------------------------------
// Light gate with REAL leaf propagation (LGT-2): the canopy's per-cell
// opacity (leaves = 2) sets the sky light at the crop cell. The crop's
// growth_light is 9 (the Minecraft wheat value): 3 canopy layers leave
// exactly 9 (15 - 2*3 — the threshold, the roll runs) and 4 layers leave
// 7 (15 - 2*4 — below, the growth stops). The enclosure (opaque stone
// columns from the floor up past the canopy) makes the vertical rescan
// path the ONLY light route into the crop cell.
// ---------------------------------------------------------------------------

namespace {

// soil + stone (opaque, derived 15) + leaves (explicit 2) + the 2-stage
// crop chain (growth_ticks 1, grows_on "soil", growth_light 9).
BlockRegistry leafCrownRegistry() {
    BlockRegistry reg;
    auto soilR = reg.registerBlock(parseDef(
        R"({"id":"soil","properties":{"hardness":1.0},)"
        R"("model":{"textures":{"top":"t","side":"t","bottom":"t"}}})"));
    if (!soilR.ok()) {
        throw std::runtime_error(soilR.error().message);
    }
    auto stoneR = reg.registerBlock(parseDef(
        R"({"id":"stone","properties":{"hardness":1.0},)"
        R"("model":{"textures":{"top":"t","side":"t","bottom":"t"}}})"));
    if (!stoneR.ok()) {
        throw std::runtime_error(stoneR.error().message);
    }
    auto leavesR = reg.registerBlock(parseDef(
        R"({"id":"leaves","properties":{"hardness":0.2,"solid":false,"opaque":false,"transparent":true,"light_opacity":2},)"
        R"("model":{"textures":{"top":"t","side":"t","bottom":"t"}}})"));
    if (!leavesR.ok()) {
        throw std::runtime_error(leavesR.error().message);
    }
    auto aR = reg.registerBlock(parseDef(
        R"({"id":"crop_a","properties":{"hardness":0.0,"solid":false,"opaque":false,"transparent":true,"grows_to":"crop_b","growth_ticks":1,"grows_on":"soil","growth_light":9},)"
        R"("model":{"textures":{"top":"t","side":"t","bottom":"t"}}})"));
    if (!aR.ok()) {
        throw std::runtime_error(aR.error().message);
    }
    auto bR = reg.registerBlock(parseDef(
        R"({"id":"crop_b","properties":{"hardness":0.0,"solid":false,"opaque":false,"transparent":true,"drops":"wheat","drops_count":2},)"
        R"("model":{"textures":{"top":"t","side":"t","bottom":"t"}}})"));
    if (!bR.ok()) {
        throw std::runtime_error(bR.error().message);
    }
    return reg;
}

// One crop at (8,2,8) under a leaf canopy of `layers` (y 3..2+layers),
// fully propagated.
struct LeafCrownScene {
    World world;
    std::map<BlockPos, BlockID> crops;
    BlockID a;
    int underCrownLight;

    explicit LeafCrownScene(const BlockRegistry& reg, int layers)
        : a(reg.idForName("crop_a")) {
        const BlockID soil = reg.idForName("soil");
        const BlockID stone = reg.idForName("stone");
        const BlockID leaves = reg.idForName("leaves");
        world.getOrCreateChunk({0, 0, 0});
        world.setBlock(8, 1, 8, soil);
        world.setBlock(8, 2, 8, a);
        crops[{8, 2, 8}] = a;
        const int top = 2 + layers;
        for (int y = 3; y <= top; ++y) {
            world.setBlock(8, y, 8, leaves);
        }
        // The enclosing opaque columns, from the floor up past the canopy.
        for (int y = 0; y <= top; ++y) {
            world.setBlock(7, y, 8, stone);
            world.setBlock(9, y, 8, stone);
            world.setBlock(8, y, 7, stone);
            world.setBlock(8, y, 9, stone);
        }
        Lighting::propagateAll(world, reg);
        underCrownLight = world.getSkyLight(8, 2, 8);
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

TEST(GrowthEnvTest, LeafCrownLightGateBoundary) {
    const BlockRegistry reg = leafCrownRegistry();
    // 3 layers: 15 - 2*3 = 9 = the threshold exactly -> the roll runs
    // every frame (growth_ticks 1).
    LeafCrownScene pass(reg, 3);
    EXPECT_EQ(pass.underCrownLight, 9);
    EXPECT_EQ(pass.stepsOver(reg, 42, 20), 20);
    // 4 layers: 15 - 2*4 = 7 < 9 -> the light gate stops the growth.
    LeafCrownScene stop(reg, 4);
    EXPECT_EQ(stop.underCrownLight, 7);
    EXPECT_EQ(stop.stepsOver(reg, 42, 20), 0);
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

// ---------------------------------------------------------------------------
// GEN-2 sapling growth (saplingGrowthSteps): the cropGrowthSteps twin over
// the `tree` blocks — the stage is the per-instance STATE (ENT-2), the
// completion plans the tree (world/tree_shape).
// ---------------------------------------------------------------------------

namespace {

// A sapling definition: the cross + the 8-state stage pair + the soil
// gate + the light gate + the tree object. `ticks` 1 makes the roll pass
// every frame (the exact-sequence pattern); height_range 0 pins the
// rolled height at minH (the fits tests need a deterministic plan).
std::string saplingDef(int ticks, int minH, int range, int radius,
                       const char* shape, int light) {
    return std::string("{\"id\":\"sapling\",\"properties\":{\"hardness\":0.0,") +
        "\"solid\":false,\"opaque\":false,\"transparent\":true,"
        "\"plant_on\":[\"soil\"],\"growth_light\":" + std::to_string(light) +
        ",\"growth_ticks\":" + std::to_string(ticks) +
        ",\"tree\":{\"trunk\":\"trunkblk\",\"leaves\":\"leavesblk\","
        "\"min_height\":" + std::to_string(minH) +
        ",\"height_range\":" + std::to_string(range) +
        ",\"leaf_radius\":" + std::to_string(radius) +
        ",\"shape\":\"" + shape + "\"}" +
        "},\"model\":{\"shape\":\"cross\",\"state_count\":8,"
        "\"state_default\":0,\"textures\":{\"cross\":\"sap\"}}}";
}

BlockRegistry saplingRegistry(int ticks, int minH, int range, int radius,
                              const char* shape, int light) {
    BlockRegistry reg;
    auto soil = reg.registerBlock(parseDef(cubeDef("soil")));
    auto stone = reg.registerBlock(parseDef(cubeDef("stone")));
    auto trunk = reg.registerBlock(parseDef(cubeDef("trunkblk")));
    auto leaves = reg.registerBlock(parseDef(
        R"({"id":"leavesblk","properties":{"solid":false,"opaque":false,"transparent":true},)"
        R"("model":{"textures":{"top":"t","side":"t","bottom":"t"}}})"));
    auto sapling = reg.registerBlock(parseDef(
        saplingDef(ticks, minH, range, radius, shape, light)));
    if (!soil.ok() || !stone.ok() || !trunk.ok() || !leaves.ok() ||
        !sapling.ok()) {
        throw std::runtime_error("fixture failed");
    }
    return reg;
}

// One sapling at (8, 2, 8) over soil (8, 1, 8), the light set at the
// sapling cell. The tracking set mirrors the game's choke point.
struct SaplingScene {
    World world;
    std::map<BlockPos, BlockID> saplings;
    BlockID soil;
    BlockID stone;
    BlockID trunk;
    BlockID leaves;
    BlockID sapling;
    const BlockPos pos {8, 2, 8};

    SaplingScene(BlockRegistry reg, int light)
        : soil(reg.idForName("soil")), stone(reg.idForName("stone")),
          trunk(reg.idForName("trunkblk")),
          leaves(reg.idForName("leavesblk")),
          sapling(reg.idForName("sapling")) {
        world.getOrCreateChunk({0, 0, 0});
        world.setBlock(pos.x, pos.y - 1, pos.z, soil);
        world.setBlock(pos.x, pos.y, pos.z, sapling);
        world.setBlockState(pos.x, pos.y, pos.z, 0);
        saplings[pos] = sapling;
        if (light > 0) {
            world.findChunkMut({0, 0, 0})->setSkyLightAt(pos.x, pos.y, pos.z,
                                                          light);
        }
    }
};

// Apply one frame's steps like the game does (Advance -> setBlockState;
// Complete -> the planTree cells through setBlock + the tracking drop).
void saplingFrame(World& world, const BlockRegistry& reg,
                  std::map<BlockPos, BlockID>& saplings, std::uint64_t seed,
                  std::uint64_t frame, int& advances, int& completes,
                  int& outHeight) {
    const std::vector<SaplingStep> steps =
        saplingGrowthSteps(world, reg, saplings, seed, frame);
    for (const SaplingStep& s : steps) {
        if (s.action == SaplingAction::Advance) {
            world.setBlockState(s.pos.x, s.pos.y, s.pos.z,
                                static_cast<std::uint8_t>(s.stage));
            ++advances;
        } else {
            const BlockID id = saplings.at(s.pos);
            const TreeSpec spec = treeSpecFromBlock(reg.at(id));
            outHeight = s.height;
            for (const TreeCell& c : planTree(s.height, spec)) {
                world.setBlock(s.pos.x + c.offset.x, s.pos.y + c.offset.y,
                               s.pos.z + c.offset.z,
                               c.trunkCell ? reg.idForName("trunkblk")
                                           : reg.idForName("leavesblk"));
            }
            saplings.erase(s.pos);
            ++completes;
        }
    }
}

} // namespace

TEST(SaplingGrowthTest, EmptyAndStaleSetsYieldNoSteps) {
    const BlockRegistry reg = saplingRegistry(1, 4, 0, 2, "oak", 9);
    World world;
    world.getOrCreateChunk({0, 0, 0});
    std::map<BlockPos, BlockID> empty;
    EXPECT_TRUE(saplingGrowthSteps(world, reg, empty, 42, 0).empty());
    // Tracked, but the cell holds air (stale) -> no step.
    std::map<BlockPos, BlockID> stale;
    stale[{3, 4, 5}] = reg.idForName("sapling");
    EXPECT_TRUE(saplingGrowthSteps(world, reg, stale, 42, 0).empty());
}

TEST(SaplingGrowthTest, SoilAndLightGates) {
    // The soil gate (plant_on = ["soil"]): air below / the wrong block
    // below -> no step; the soil below -> the step runs (ticks 1 = the
    // roll always passes; light 15 = the light gate open).
    {
        const BlockRegistry reg = saplingRegistry(1, 4, 0, 2, "oak", 9);
        SaplingScene s(reg, 15);
        s.world.setBlock(s.pos.x, s.pos.y - 1, s.pos.z, BlockID::air());
        EXPECT_TRUE(
            saplingGrowthSteps(s.world, reg, s.saplings, 7, 0).empty());
    }
    {
        const BlockRegistry reg = saplingRegistry(1, 4, 0, 2, "oak", 9);
        SaplingScene s(reg, 15);
        s.world.setBlock(s.pos.x, s.pos.y - 1, s.pos.z, s.stone);
        EXPECT_TRUE(
            saplingGrowthSteps(s.world, reg, s.saplings, 7, 0).empty());
    }
    {
        const BlockRegistry reg = saplingRegistry(1, 4, 0, 2, "oak", 9);
        SaplingScene s(reg, 15);
        const auto steps = saplingGrowthSteps(s.world, reg, s.saplings, 7, 0);
        ASSERT_EQ(steps.size(), 1u);
        EXPECT_EQ(steps[0].pos, s.pos);
    }
    // The light gate (growth_light 9): 8 blocks, 9 passes, 11 passes
    // (the under-canopy value a LGT-2 crown would leave).
    {
        const BlockRegistry reg = saplingRegistry(1, 4, 0, 2, "oak", 9);
        SaplingScene s(reg, 8);
        EXPECT_TRUE(
            saplingGrowthSteps(s.world, reg, s.saplings, 7, 0).empty());
    }
    {
        const BlockRegistry reg = saplingRegistry(1, 4, 0, 2, "oak", 9);
        SaplingScene s(reg, 9);
        EXPECT_FALSE(
            saplingGrowthSteps(s.world, reg, s.saplings, 7, 0).empty());
    }
    {
        const BlockRegistry reg = saplingRegistry(1, 4, 0, 2, "oak", 9);
        SaplingScene s(reg, 11);
        EXPECT_FALSE(
            saplingGrowthSteps(s.world, reg, s.saplings, 7, 0).empty());
    }
}

TEST(SaplingGrowthTest, MaturesThroughStagesAndCompletes) {
    // Ticks 1: every frame the gates pass, the stage advances (0..7 =
    // seven Advances), then the final stage completes (one Complete with
    // the pinned height 4 — height_range 0). The stage IS the state byte:
    // the world's state tracks the sapling through the run.
    const BlockRegistry reg = saplingRegistry(1, 4, 0, 2, "oak", 9);
    SaplingScene s(reg, 15);
    int advances = 0, completes = 0, height = 0;
    for (std::uint64_t f = 0; f < 30;
         ++f, saplingFrame(s.world, reg, s.saplings, 42, f, advances,
                           completes, height)) {
        if (completes > 0) {
            break;
        }
    }
    EXPECT_EQ(advances, 7);
    EXPECT_EQ(completes, 1);
    EXPECT_EQ(height, 4);
    // The grown tree: the exact planTree cells (oak T = 4 — the
    // worldgen's placeTrees geometry), the root cell is the trunk base,
    // the sapling is gone from the tracking set.
    const TreeSpec spec = treeSpecFromBlock(reg.at(s.sapling));
    const std::vector<TreeCell> plan = planTree(4, spec);
    for (const TreeCell& c : plan) {
        const BlockID want = c.trunkCell ? s.trunk : s.leaves;
        EXPECT_EQ(
            s.world.getBlock(s.pos.x + c.offset.x, s.pos.y + c.offset.y,
                             s.pos.z + c.offset.z),
            want)
            << "(" << c.offset.x << "," << c.offset.y << "," << c.offset.z
            << ")";
    }
    EXPECT_EQ(s.world.getBlock(s.pos.x, s.pos.y, s.pos.z), s.trunk);
    EXPECT_TRUE(s.saplings.empty());
    // Neighbors OUTSIDE the plan stayed untouched (the soil below the
    // root; air three over — the plan reaches only to x + 2).
    EXPECT_EQ(s.world.getBlock(s.pos.x, s.pos.y - 1, s.pos.z), s.soil);
    EXPECT_EQ(s.world.getBlock(s.pos.x + 3, s.pos.y, s.pos.z),
              BlockID::air());
}

TEST(SaplingGrowthTest, StochasticPaceAdvances) {
    // The shipped pace (120): over a 20k-frame window the sapling
    // advances all 7 stages and completes (the expected ~166 roll
    // passes make this deterministic in practice — the exact sequence).
    const BlockRegistry reg = saplingRegistry(120, 4, 0, 2, "oak", 9);
    SaplingScene s(reg, 15);
    int advances = 0, completes = 0, height = 0;
    for (std::uint64_t f = 0; f < 20000 && completes == 0;
         ++f, saplingFrame(s.world, reg, s.saplings, 999, f, advances,
                           completes, height)) {
    }
    EXPECT_EQ(completes, 1);
    EXPECT_EQ(advances, 7);
    EXPECT_EQ(height, 4);
    // Seed decorrelation: two FRESH scenes under different seeds take
    // their first step on different frames (a lockstep bug would match).
    const auto firstStepFrame = [](const BlockRegistry& reg2,
                                   std::uint64_t seed) {
        SaplingScene s2(reg2, 15);
        for (std::uint64_t f = 0; f < 20000; ++f) {
            const auto steps = saplingGrowthSteps(s2.world, reg2,
                                                  s2.saplings, seed, f);
            if (!steps.empty()) {
                return std::optional<std::uint64_t> {f};
            }
        }
        return std::optional<std::uint64_t> {};
    };
    const auto firstA = firstStepFrame(reg, 999);
    const auto firstB = firstStepFrame(reg, 1234567);
    ASSERT_TRUE(firstA.has_value());
    ASSERT_TRUE(firstB.has_value());
    EXPECT_NE(*firstA, *firstB);
}

TEST(SaplingGrowthTest, CompleteFitsExactCellsAndNoResidue) {
    // The Complete's cell set is EXACT (a stray write would land a trunk
    // or leaves where the plan has none — scan the whole 8x8x8 box
    // around the root) and the sapling leaves NO stateful residue (the
    // root holds the trunk — non-stateful; the world's state read on it
    // is inert for the save derivation, which keys on the live block).
    const BlockRegistry reg = saplingRegistry(1, 4, 0, 2, "oak", 9);
    SaplingScene s(reg, 15);
    s.world.setBlockState(8, 2, 8, 7); // final stage
    int advances = 0, completes = 0, height = 0;
    saplingFrame(s.world, reg, s.saplings, 42, 0, advances, completes,
                 height);
    ASSERT_EQ(completes, 1);
    const TreeSpec spec = treeSpecFromBlock(reg.at(s.sapling));
    const std::vector<TreeCell> plan = planTree(height, spec);
    std::set<std::tuple<int, int, int>> planned;
    for (const TreeCell& c : plan) {
        planned.insert({c.offset.x, c.offset.y, c.offset.z});
    }
    for (int dx = -4; dx <= 4; ++dx) {
        for (int dy = -1; dy <= 6; ++dy) {
            for (int dz = -4; dz <= 4; ++dz) {
                const BlockID got =
                    s.world.getBlock(s.pos.x + dx, s.pos.y + dy,
                                     s.pos.z + dz);
                const bool plannedHere =
                    planned.count({dx, dy, dz}) != 0;
                if (plannedHere) {
                    EXPECT_TRUE(got == s.trunk || got == s.leaves)
                        << "(" << dx << "," << dy << "," << dz << ")";
                } else {
                    // Outside the plan: the original content (soil at
                    // (0,-1,0), air elsewhere).
                    const bool isSoil = (dx == 0 && dy == -1 && dz == 0);
                    EXPECT_EQ(got, isSoil ? s.soil : BlockID::air())
                        << "(" << dx << "," << dy << "," << dz << ")";
                }
            }
        }
    }
    EXPECT_TRUE(s.saplings.empty());
}

TEST(SaplingGrowthTest, CeilingAndWaterBlockTheCompletion) {
    // A canopy cell occupied (stone ceiling at the top cap) or wetted
    // (water in a skirt cell) blocks the Complete: the final-stage sapling
    // yields NO step (the wait-and-retry — the rolled height is
    // frame-independent, so clearing the blocker completes with the SAME
    // height). Ticks 1 = the roll always passes, so the blocker is the
    // only variable.
    const BlockRegistry reg = saplingRegistry(1, 4, 0, 2, "oak", 9);
    {
        SaplingScene s(reg, 15);
        s.world.setBlockState(8, 2, 8, 7);
        // The ceiling: the top cap centre (offset (0, 4, 0) = absolute
        // (8, 6, 8)).
        s.world.setBlock(8, 6, 8, s.stone);
        EXPECT_TRUE(
            saplingGrowthSteps(s.world, reg, s.saplings, 7, 0).empty());
        // Clear the ceiling -> the Complete (height 4, the pinned roll).
        s.world.setBlock(8, 6, 8, BlockID::air());
        auto steps = saplingGrowthSteps(s.world, reg, s.saplings, 7, 0);
        ASSERT_EQ(steps.size(), 1u);
        EXPECT_EQ(steps[0].action, SaplingAction::Complete);
        EXPECT_EQ(steps[0].height, 4);
    }
    {
        SaplingScene s(reg, 15);
        s.world.setBlockState(8, 2, 8, 7);
        // Water in a skirt cell of the radius-2 layer (offset (-2, 2, 0)
        // = absolute (6, 4, 8)).
        s.world.setFluid(6, 4, 8, FluidKind::Water, 8);
        EXPECT_TRUE(
            saplingGrowthSteps(s.world, reg, s.saplings, 7, 0).empty());
        s.world.setFluid(6, 4, 8, FluidKind::Water, 0);
        auto steps = saplingGrowthSteps(s.world, reg, s.saplings, 7, 0);
        ASSERT_EQ(steps.size(), 1u);
        EXPECT_EQ(steps[0].action, SaplingAction::Complete);
    }
    // A mid-stage sapling under the ceiling still ADVANCES (the fits
    // probe runs only at the final stage).
    {
        SaplingScene s(reg, 15);
        s.world.setBlock(8, 6, 8, s.stone);
        auto steps = saplingGrowthSteps(s.world, reg, s.saplings, 7, 0);
        ASSERT_EQ(steps.size(), 1u);
        EXPECT_EQ(steps[0].action, SaplingAction::Advance);
        EXPECT_EQ(steps[0].stage, 1);
    }
}

TEST(SaplingGrowthTest, CrossChunkTreeLandsInBothChunks) {
    // A root near the chunk corner (14, 2, 14): the oak T = 4 plan spans
    // x/z 12..16 — into the neighbor chunks (1, 0) and (0, 1). Every
    // planned cell lands in whichever chunk holds it (the game's
    // setBlock completes first-touch columns; here the test pre-creates
    // them) and the chunks both go dirty.
    const BlockRegistry reg = saplingRegistry(1, 4, 0, 2, "oak", 9);
    World world;
    for (int cx = 0; cx <= 1; ++cx) {
        for (int cz = 0; cz <= 1; ++cz) {
            world.getOrCreateChunk({cx, 0, cz});
        }
    }
    const BlockID soil = reg.idForName("soil");
    const BlockID sapling = reg.idForName("sapling");
    world.setBlock(14, 1, 14, soil);
    world.setBlock(14, 2, 14, sapling);
    world.setBlockState(14, 2, 14, 7); // the final stage
    world.findChunkMut({0, 0, 0})->setSkyLightAt(14, 2, 14, 15);
    std::map<BlockPos, BlockID> saplings;
    saplings[{14, 2, 14}] = sapling;
    int advances = 0, completes = 0, height = 0;
    saplingFrame(world, reg, saplings, 42, 0, advances, completes, height);
    ASSERT_EQ(completes, 1);
    const TreeSpec spec = treeSpecFromBlock(reg.at(sapling));
    const std::vector<TreeCell> plan = planTree(height, spec);
    int outside = 0;
    for (const TreeCell& c : plan) {
        const int wx = 14 + c.offset.x, wy = 2 + c.offset.y, wz = 14 + c.offset.z;
        EXPECT_EQ(world.getBlock(wx, wy, wz),
                  c.trunkCell ? reg.idForName("trunkblk")
                              : reg.idForName("leavesblk"));
        if (wx >= 16 || wz >= 16) {
            ++outside;
        }
    }
    EXPECT_GT(outside, 0); // the plan really crossed the boundary
    EXPECT_TRUE(saplings.empty());
}
