#include <gtest/gtest.h>

#include <string>

#include "core/asset_manager.hpp"
#include "world/worldgen_config.hpp"

using namespace mc;

namespace {

Json parseJson(const std::string& text) {
    auto r = Json::parse(text);
    if (!r.ok()) {
        ADD_FAILURE() << "parse failed: " << r.error().message;
        return Json::object();
    }
    return *r;
}

} // namespace

TEST(WorldgenConfigTest, EmptyObjectYieldsDefaults) {
    auto r = WorldgenParams::parse(parseJson(R"({})"));
    ASSERT_TRUE(r.ok()) << r.error().message;
    const WorldgenParams& p = *r;
    EXPECT_EQ(p.baseHeight, 40);
    EXPECT_EQ(p.heightAmplitude, 24);
    EXPECT_EQ(p.seaLevel, 34);
    EXPECT_FLOAT_EQ(p.heightNoiseScale, 0.02f);
    EXPECT_EQ(p.heightOctaves, 4);
    EXPECT_EQ(p.bottomBlock, "stone");
    EXPECT_EQ(p.deepBlock, "stone");
    EXPECT_EQ(p.bedrockLayers, 1); // GEN-7 default: the legacy single layer
    EXPECT_FLOAT_EQ(p.biomeNoiseScale, 0.02f);
    EXPECT_TRUE(p.cavesEnabled);
    EXPECT_FLOAT_EQ(p.caveThreshold, 0.68f);
    EXPECT_EQ(p.caveMinY, 2);
    EXPECT_EQ(p.surfaceGuard, 4);
    EXPECT_EQ(p.oreLattice, 8);
    EXPECT_TRUE(p.ores.empty());
    EXPECT_EQ(p.treeLattice, 4);
    EXPECT_EQ(p.treeTrunk, "wood");
    EXPECT_EQ(p.treeLeaves, "leaves");
    EXPECT_EQ(p.treeMinHeight, 4);
    EXPECT_EQ(p.treeHeightRange, 3);
}

TEST(WorldgenConfigTest, FullOverride) {
    const std::string def =
        R"({)"
        R"("terrain":{"base_height":50,"height_amplitude":10,"sea_level":30,)"
        R"("height_noise_scale":0.02,"height_octaves":3,)"
        R"("bottom_block":"bedrock","deep_block":"stone"},)"
        R"("biomes":{"noise_scale":0.005},)"
        R"("caves":{"enabled":false,"noise_scale":0.05,"threshold":0.7,)"
        R"("min_y":3,"surface_guard":6},)"
        R"("ores":{"lattice":12,"veins":[)"
        R"({"block":"coal_ore","probability":0.5,"vein_size":8,"max_y":80},)"
        R"({"block":"diamond_ore","probability":0.1,"vein_size":5,"max_y":16}]},)"
        R"("trees":{"lattice":6,"trunk":"wood","leaves":"leaves",)"
        R"("min_height":5,"height_range":2}})";
    auto r = WorldgenParams::parse(parseJson(def));
    ASSERT_TRUE(r.ok()) << r.error().message;
    const WorldgenParams& p = *r;
    EXPECT_EQ(p.baseHeight, 50);
    EXPECT_EQ(p.heightAmplitude, 10);
    EXPECT_EQ(p.seaLevel, 30);
    EXPECT_FLOAT_EQ(p.heightNoiseScale, 0.02f);
    EXPECT_EQ(p.heightOctaves, 3);
    EXPECT_EQ(p.bottomBlock, "bedrock");
    EXPECT_FLOAT_EQ(p.biomeNoiseScale, 0.005f);
    EXPECT_FALSE(p.cavesEnabled);
    EXPECT_EQ(p.caveMinY, 3);
    EXPECT_EQ(p.surfaceGuard, 6);
    EXPECT_EQ(p.oreLattice, 12);
    ASSERT_EQ(p.ores.size(), 2u);
    EXPECT_EQ(p.ores[0].block, "coal_ore");
    EXPECT_FLOAT_EQ(p.ores[0].probability, 0.5f);
    EXPECT_EQ(p.ores[0].veinSize, 8);
    EXPECT_EQ(p.ores[0].maxY, 80);
    EXPECT_EQ(p.ores[1].block, "diamond_ore");
    EXPECT_EQ(p.ores[1].veinSize, 5);
    EXPECT_EQ(p.treeLattice, 6);
    EXPECT_EQ(p.treeMinHeight, 5);
    EXPECT_EQ(p.treeHeightRange, 2);
}

TEST(WorldgenConfigTest, PartialSectionKeepsOtherDefaults) {
    auto r = WorldgenParams::parse(parseJson(R"({"caves":{"enabled":false}})"));
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_FALSE(r->cavesEnabled);
    // Unmentioned fields keep their defaults.
    EXPECT_FLOAT_EQ(r->caveThreshold, 0.68f);
    EXPECT_EQ(r->baseHeight, 40);
    EXPECT_TRUE(r->ores.empty());
}

TEST(WorldgenConfigTest, RejectsNonObject) {
    auto r = WorldgenParams::parse(parseJson(R"([1,2,3])"));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("object"), std::string::npos);
}

TEST(WorldgenConfigTest, RejectsWrongTypeInSection) {
    auto r = WorldgenParams::parse(
        parseJson(R"({"terrain":{"base_height":"44"}})"));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("terrain.base_height"), std::string::npos);
}

TEST(WorldgenConfigTest, RejectsSectionWrongType) {
    auto r = WorldgenParams::parse(parseJson(R"({"caves":[true]})"));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("caves"), std::string::npos);
}

TEST(WorldgenConfigTest, RejectsVeinsNotArray) {
    auto r = WorldgenParams::parse(
        parseJson(R"({"ores":{"veins":{"block":"coal_ore"}}})"));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("ores.veins"), std::string::npos);
}

TEST(WorldgenConfigTest, RejectsVeinMissingBlock) {
    auto r = WorldgenParams::parse(parseJson(
        R"({"ores":{"veins":[{"probability":0.5,"vein_size":8,"max_y":80}]}})"));
    ASSERT_FALSE(r.ok());
    // Absent 'block' keeps the default "", which the non-empty check rejects.
    EXPECT_NE(r.error().message.find("block"), std::string::npos);
}

TEST(WorldgenConfigTest, RejectsVeinProbabilityOutOfRange) {
    auto r = WorldgenParams::parse(parseJson(
        R"({"ores":{"veins":[{"block":"coal_ore","probability":1.5,)"
        R"("vein_size":8,"max_y":80}]}})"));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("probability"), std::string::npos);
}

TEST(WorldgenConfigTest, RejectsVeinSizeOutOfRange) {
    auto r = WorldgenParams::parse(parseJson(
        R"({"ores":{"veins":[{"block":"coal_ore","probability":0.5,)"
        R"("vein_size":0,"max_y":80}]}})"));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("vein_size"), std::string::npos);
}

TEST(WorldgenConfigTest, RejectsMaxYOutOfRange) {
    auto r = WorldgenParams::parse(parseJson(
        R"({"ores":{"veins":[{"block":"coal_ore","probability":0.5,)"
        R"("vein_size":8,"max_y":0}]}})"));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("max_y"), std::string::npos);
}

TEST(WorldgenConfigTest, RejectsInvalidThreshold) {
    auto r = WorldgenParams::parse(
        parseJson(R"({"caves":{"threshold":1.0}})"));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("threshold"), std::string::npos);
}

// ---------------------------------------------------------------------------
// GEN-1 (spaghetti caves): the new cave tuning — tube_threshold (the mid-
// band of the two-noise intersection), spawn_clear_columns (the spawn
// protection), surface_guard 0 (openings reach the surface). The legacy
// `threshold` key is retired: old configs WARN about it and load, the key
// is still parsed + range-validated (legacy tolerance) but the generator
// does not read it.
// ---------------------------------------------------------------------------

TEST(WorldgenConfigTest, ParsesCaveTuning) {
    auto r = WorldgenParams::parse(parseJson(
        R"({"caves":{"enabled":true,"noise_scale":0.09,"tube_threshold":0.06,)"
        R"("min_y":2,"surface_guard":0,"spawn_clear_columns":2}})"));
    ASSERT_TRUE(r.ok()) << r.error().message;
    const WorldgenParams& p = *r;
    EXPECT_FLOAT_EQ(p.caveNoiseScale, 0.09f);
    EXPECT_FLOAT_EQ(p.caveTubeThreshold, 0.06f);
    EXPECT_EQ(p.caveMinY, 2);
    EXPECT_EQ(p.surfaceGuard, 0);
    EXPECT_EQ(p.caveSpawnClearColumns, 2);
}

TEST(WorldgenConfigTest, CaveTuningDefaults) {
    // Absent keys keep the documented defaults (the shipped values).
    auto r = WorldgenParams::parse(parseJson(R"({"caves":{"enabled":true}})"));
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_FLOAT_EQ(r->caveNoiseScale, 0.06f);
    EXPECT_FLOAT_EQ(r->caveTubeThreshold, 0.06f);
    EXPECT_EQ(r->surfaceGuard, 4);
    EXPECT_EQ(r->caveSpawnClearColumns, 2);
}

TEST(WorldgenConfigTest, RejectsTubeThresholdOutOfRange) {
    // 0.5 is the full mid-band (every noise value — degenerate); above it
    // is impossible by definition.
    auto hi = WorldgenParams::parse(
        parseJson(R"({"caves":{"tube_threshold":0.51}})"));
    ASSERT_FALSE(hi.ok());
    EXPECT_NE(hi.error().message.find("tube_threshold"), std::string::npos);
    auto lo = WorldgenParams::parse(
        parseJson(R"({"caves":{"tube_threshold":-0.01}})"));
    ASSERT_FALSE(lo.ok());
    EXPECT_NE(lo.error().message.find("tube_threshold"), std::string::npos);
    // The inclusive boundaries parse (0 = no caves, 0.5 = full band).
    auto z = WorldgenParams::parse(parseJson(R"({"caves":{"tube_threshold":0}})"));
    EXPECT_TRUE(z.ok());
    auto f = WorldgenParams::parse(
        parseJson(R"({"caves":{"tube_threshold":0.5}})"));
    EXPECT_TRUE(f.ok());
}

TEST(WorldgenConfigTest, RejectsSpawnClearColumnsOutOfRange) {
    auto lo = WorldgenParams::parse(
        parseJson(R"({"caves":{"spawn_clear_columns":-1}})"));
    ASSERT_FALSE(lo.ok());
    EXPECT_NE(lo.error().message.find("spawn_clear_columns"),
              std::string::npos);
    auto hi = WorldgenParams::parse(
        parseJson(R"({"caves":{"spawn_clear_columns":33}})"));
    ASSERT_FALSE(hi.ok());
    EXPECT_NE(hi.error().message.find("spawn_clear_columns"),
              std::string::npos);
}

TEST(WorldgenConfigTest, RejectsNonNumberCaveTuning) {
    auto r = WorldgenParams::parse(
        parseJson(R"({"caves":{"tube_threshold":"six"}})"));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("tube_threshold"), std::string::npos);
}

TEST(WorldgenConfigTest, OldThresholdKeyLoadsAndIsLegacy) {
    // A pre-GEN-1 config: the retired `threshold` key WARNS (unknown field)
    // but still loads, is still parsed (legacy tolerance), and coexists
    // with the live `tube_threshold`. The generator reads only
    // tube_threshold (pinned in the generator tests).
    auto r = WorldgenParams::parse(parseJson(
        R"({"caves":{"threshold":0.68,"tube_threshold":0.06}})"));
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_FLOAT_EQ(r->caveThreshold, 0.68f); // legacy value, unread
    EXPECT_FLOAT_EQ(r->caveTubeThreshold, 0.06f); // the live knob
}

// Content pin: the shipped worldgen.json carries the calibrated cave
// tuning. GEN-1 set noise_scale 0.09 / tube_threshold 0.06; GEN-4
// re-calibrated the FRAGMENTATION (the user report "caves too many and
// too broken, small caves constantly" → the single-round probe grid,
// tests/cave_probe.hpp + build/bin/cave_probe): noise_scale 0.09 → 0.05
// (coarser tube features → fewer, much bigger caves; the 4-6% density
// band holds, pocket cells drop ~97%, component count ~4x down),
// tube_threshold 0.06 (unchanged — the pocket-merging knob was not
// needed). surface_guard 0 / spawn_clear_columns 2 unchanged.
TEST(WorldgenConfigTest, Gen1ShippedCaveTuning) {
    AssetManager assets;
    ASSERT_TRUE(assets.setRoot("assets").ok())
        << "run the tests from the repository root";
    auto r = WorldgenParams::load(assets);
    ASSERT_TRUE(r.ok()) << r.error().message;
    const WorldgenParams& p = *r;
    EXPECT_TRUE(p.cavesEnabled);
    EXPECT_FLOAT_EQ(p.caveNoiseScale, 0.05f); // GEN-4 (was 0.09 in GEN-1)
    EXPECT_FLOAT_EQ(p.caveTubeThreshold, 0.06f);
    EXPECT_EQ(p.caveMinY, 2);
    EXPECT_EQ(p.surfaceGuard, 0);
    EXPECT_EQ(p.caveSpawnClearColumns, 2);
    // Raw check: the retired `threshold` key is not shipped.
    auto text = assets.loadText("data/worldgen.json");
    ASSERT_TRUE(text.ok());
    auto parsed = Json::parse(*text);
    ASSERT_TRUE(parsed.ok());
    const Json* c = parsed->find("caves");
    ASSERT_TRUE(c != nullptr && c->isObject());
    EXPECT_EQ(c->find("threshold"), nullptr);
}

TEST(WorldgenConfigTest, RejectsEmptyBottomBlock) {
    auto r = WorldgenParams::parse(
        parseJson(R"({"terrain":{"bottom_block":""}})"));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("bottom"), std::string::npos);
}

TEST(WorldgenConfigTest, RejectsNegativeAmplitude) {
    auto r = WorldgenParams::parse(
        parseJson(R"({"terrain":{"height_amplitude":-5}})"));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("height_amplitude"), std::string::npos);
}

TEST(WorldgenConfigTest, RejectsOctavesOutOfRange) {
    auto r = WorldgenParams::parse(
        parseJson(R"({"terrain":{"height_octaves":9}})"));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("height_octaves"), std::string::npos);
}

// ---------------------------------------------------------------------------
// GEN-7: the multi-layer bedrock floor (`terrain.bedrock_layers`).
// ---------------------------------------------------------------------------

TEST(WorldgenConfigTest, ParsesBedrockLayers) {
    auto r =
        WorldgenParams::parse(parseJson(R"({"terrain":{"bedrock_layers":3}})"));
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_EQ(r->bedrockLayers, 3);
    // The other terrain defaults are untouched by the new key.
    EXPECT_EQ(r->bottomBlock, "stone");
    EXPECT_EQ(r->deepBlock, "stone");
}

TEST(WorldgenConfigTest, RejectsBedrockLayersOutOfRange) {
    for (const char* v : {"0", "9", "-1"}) {
        auto r = WorldgenParams::parse(
            parseJson(std::string("{\"terrain\":{\"bedrock_layers\":") + v +
                     "}}"));
        ASSERT_FALSE(r.ok()) << v;
        EXPECT_NE(r.error().message.find("bedrock_layers"), std::string::npos)
            << v;
    }
}

TEST(WorldgenConfigTest, RejectsBedrockLayersNonInteger) {
    auto r = WorldgenParams::parse(
        parseJson(R"({"terrain":{"bedrock_layers":2.5}})"));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("bedrock_layers"), std::string::npos);
}

TEST(WorldgenConfigTest, RejectsBedrockLayersWrongType) {
    auto r = WorldgenParams::parse(
        parseJson(R"({"terrain":{"bedrock_layers":"three"}})"));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("bedrock_layers"), std::string::npos);
}

// The shipped worldgen.json uses the Minecraft classic 3-layer floor
// (y0 always, y1 50%, y2 always).
TEST(WorldgenConfigTest, Gen7ShippedBedrockLayers) {
    AssetManager assets;
    ASSERT_TRUE(assets.setRoot("assets").ok())
        << "run the tests from the repository root";
    auto r = WorldgenParams::load(assets);
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_EQ(r->bedrockLayers, 3);
    EXPECT_EQ(r->bottomBlock, "bedrock");
}

// ---------------------------------------------------------------------------
// Phase 59 content pin (GEN-3 evolved): the ACTUAL worldgen.json must
// include the gravel vein (a stone-replacing gravel patch, the Minecraft
// gravel source). GEN-3: the ellipsoid cluster count is 1 (shipped all
// K=1) and the probability is the single-round calibration value. The
// values are the content contract; a typo'd probability/size parses fine,
// so this is the unit-level guard.
// ---------------------------------------------------------------------------

TEST(WorldgenConfigTest, Phase59GravelVeinContent) {
    AssetManager assets;
    ASSERT_TRUE(assets.setRoot("assets").ok())
        << "run the tests from the repository root";
    auto r = WorldgenParams::load(assets);
    ASSERT_TRUE(r.ok()) << r.error().message;
    const WorldgenParams& p = *r;
    const OreConfig* gravel = nullptr;
    for (const OreConfig& o : p.ores) {
        if (o.block == "gravel") {
            gravel = &o;
        }
    }
    ASSERT_TRUE(gravel != nullptr) << "no gravel vein in worldgen.json";
    EXPECT_FLOAT_EQ(gravel->probability, 0.0445f);
    EXPECT_EQ(gravel->veinSize, 1);
    EXPECT_EQ(gravel->maxY, 80);
}

// GEN-3: the vein_size domain is the ELLIPSOID CLUSTER count [1, 8]
// (kMaxOreVeinClusters) — the retired random-walk domain was [1, 32].
TEST(WorldgenConfigTest, Gen3VeinSizeClusterDomain) {
    // 0 is rejected (the pre-GEN-3 test keeps pinning this bound).
    auto r0 = WorldgenParams::parse(parseJson(
        R"({"ores":{"veins":[{"block":"coal_ore","probability":0.5,)"
        R"("vein_size":0,"max_y":80}]}})"));
    ASSERT_FALSE(r0.ok());
    EXPECT_NE(r0.error().message.find("vein_size"), std::string::npos);
    // 9 is rejected (the new upper bound — the old domain allowed it).
    auto r9 = WorldgenParams::parse(parseJson(
        R"({"ores":{"veins":[{"block":"coal_ore","probability":0.5,)"
        R"("vein_size":9,"max_y":80}]}})"));
    ASSERT_FALSE(r9.ok());
    EXPECT_NE(r9.error().message.find("vein_size"), std::string::npos);
    EXPECT_NE(r9.error().message.find("8"), std::string::npos);
    // 1 and 8 are the bounds of the new domain (legal).
    auto r1 = WorldgenParams::parse(parseJson(
        R"({"ores":{"veins":[{"block":"coal_ore","probability":0.5,)"
        R"("vein_size":1,"max_y":80}]}})"));
    ASSERT_TRUE(r1.ok()) << r1.error().message;
    EXPECT_EQ(r1->ores[0].veinSize, 1);
    auto r8 = WorldgenParams::parse(parseJson(
        R"({"ores":{"veins":[{"block":"coal_ore","probability":0.5,)"
        R"("vein_size":8,"max_y":80}]}})"));
    ASSERT_TRUE(r8.ok()) << r8.error().message;
    EXPECT_EQ(r8->ores[0].veinSize, 8);
}

// GEN-3 calibration pin: the shipped six veins are ALL single-ellipsoid
// (K = 1) with the one-round re-tuned probabilities (the probe's
// new/old cell-count ratio pulled the density back to the old baseline —
// see docs/KNOWN_ISSUES for the recorded diamond/lapis residuals). The
// block names + max_y are unchanged by GEN-3 (the shape + density only).
TEST(WorldgenConfigTest, Gen3ShippedOreVeinsPinned) {
    AssetManager assets;
    ASSERT_TRUE(assets.setRoot("assets").ok())
        << "run the tests from the repository root";
    auto r = WorldgenParams::load(assets);
    ASSERT_TRUE(r.ok()) << r.error().message;
    const WorldgenParams& p = *r;
    EXPECT_EQ(p.oreLattice, 8);
    ASSERT_EQ(p.ores.size(), 6u);
    const struct {
        const char* block;
        float probability;
        int veinSize;
        int maxY;
    } want[] = {
        {"coal_ore", 0.154f, 1, 80},
        {"iron_ore", 0.0855f, 1, 56},
        {"gold_ore", 0.0376f, 1, 36},
        {"diamond_ore", 0.0101f, 1, 20},
        {"lapis_ore", 0.0208f, 1, 32},
        {"gravel", 0.0445f, 1, 80},
    };
    for (std::size_t i = 0; i < p.ores.size(); ++i) {
        EXPECT_EQ(p.ores[i].block, want[i].block) << "vein " << i;
        EXPECT_FLOAT_EQ(p.ores[i].probability, want[i].probability)
            << "vein " << i;
        EXPECT_EQ(p.ores[i].veinSize, want[i].veinSize) << "vein " << i;
        EXPECT_EQ(p.ores[i].maxY, want[i].maxY) << "vein " << i;
    }
}
