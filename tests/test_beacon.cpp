// Beacon power-level tests (Phase 83): the pure pyramid scan of the
// `beacon_base` layers directly below a beacon (world/beacon) — level 0..4
// for full 3x3 / 5x5 / 7x7 / 9x9 layers, stopping at the first incomplete
// layer. The function is registry-aware (it reads each cell's `beacon_base`
// property), so the fixture flags an iron base block and an inert stone.
#include <gtest/gtest.h>

#include <cstdint>
#include <string>

#include "block/block_registry.hpp"
#include "core/json.hpp"
#include "world/beacon.hpp"
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

// A registry with a beacon_base iron, an inert stone (a base cell holding
// it must NOT count), and the beacon itself (its presence is irrelevant to
// the scan — the level reads only the layers BELOW).
BlockRegistry beaconRegistry() {
    BlockRegistry reg;
    const std::string iron =
        "{\"id\":\"iron\",\"properties\":{\"hardness\":5.0,"
        "\"drops\":\"iron\",\"beacon_base\":true},"
        "\"model\":{\"textures\":{\"top\":\"iron\",\"side\":\"iron\","
        "\"bottom\":\"iron\"}}}";
    const std::string stone =
        "{\"id\":\"stone\",\"properties\":{\"hardness\":1.5,"
        "\"drops\":\"stone\"},"
        "\"model\":{\"textures\":{\"top\":\"stone\",\"side\":\"stone\","
        "\"bottom\":\"stone\"}}}";
    const std::string beacon =
        "{\"id\":\"beacon\",\"properties\":{\"hardness\":8.0,"
        "\"drops\":\"beacon\",\"beacon_effect\":\"absorption\"},"
        "\"model\":{\"textures\":{\"top\":\"beacon\",\"side\":\"beacon\","
        "\"bottom\":\"beacon\"}}}";
    for (const std::string& def : {iron, stone, beacon}) {
        auto r = reg.registerBlock(parseDef(def));
        if (!r.ok()) {
            throw std::runtime_error(r.error().message);
        }
    }
    return reg;
}

// Place a full (2*half+1) square of `fill` at y, centred on (cx, cz).
void fillSquare(World& world, BlockID fill, int cx, int y, int cz, int half) {
    for (int dx = -half; dx <= half; ++dx) {
        for (int dz = -half; dz <= half; ++dz) {
            world.setBlock(cx + dx, y, cz + dz, fill);
        }
    }
}

// A world with a beacon at (8, 4, 8) and N full base layers below it
// (3x3 at y=3, 5x5 at y=2, 7x7 at y=1, 9x9 at y=0).
World beaconWorld(BlockID beacon, BlockID base, int layers) {
    World world;
    world.getOrCreateChunk({0, 0, 0});
    world.setBlock(8, 4, 8, beacon);
    const int halfs[4] = {1, 2, 3, 4};
    for (int i = 0; i < layers && i < 4; ++i) {
        fillSquare(world, base, 8, 3 - i, 8, halfs[i]);
    }
    return world;
}

} // namespace

TEST(BeaconLevelTest, EmptyWorldLevelZero) {
    const BlockRegistry reg = beaconRegistry();
    World world;
    world.getOrCreateChunk({0, 0, 0});
    world.setBlock(8, 4, 8, reg.idForName("beacon"));
    EXPECT_EQ(beaconLevel(world, reg, 8, 4, 8), 0);
}

TEST(BeaconLevelTest, FullLayersGiveThePyramidLevel) {
    const BlockRegistry reg = beaconRegistry();
    const BlockID beacon = reg.idForName("beacon");
    const BlockID base = reg.idForName("iron");
    EXPECT_EQ(beaconLevel(beaconWorld(beacon, base, 1), reg, 8, 4, 8), 1);
    EXPECT_EQ(beaconLevel(beaconWorld(beacon, base, 2), reg, 8, 4, 8), 2);
    EXPECT_EQ(beaconLevel(beaconWorld(beacon, base, 3), reg, 8, 4, 8), 3);
    EXPECT_EQ(beaconLevel(beaconWorld(beacon, base, 4), reg, 8, 4, 8), 4);
}

TEST(BeaconLevelTest, IncompleteLayerStopsTheCount) {
    const BlockRegistry reg = beaconRegistry();
    const BlockID beacon = reg.idForName("beacon");
    const BlockID base = reg.idForName("iron");
    // A full 3x3 (level 1) but the 5x5 layer has ONE missing corner -> the
    // count stops at 1 (the pyramid is only as strong as its weakest layer).
    World world = beaconWorld(beacon, base, 2);
    world.setBlock(6, 2, 6, reg.idForName("stone")); // knock out a 5x5 corner
    EXPECT_EQ(beaconLevel(world, reg, 8, 4, 8), 1);
}

TEST(BeaconLevelTest, AMissingLowerLayerCancelsTheHigher) {
    const BlockRegistry reg = beaconRegistry();
    const BlockID beacon = reg.idForName("beacon");
    const BlockID base = reg.idForName("iron");
    // The 3x3 is missing but the 5x5 is full -> level 0 (the layers must be
    // consecutive from the top down, Minecraft).
    World world;
    world.getOrCreateChunk({0, 0, 0});
    world.setBlock(8, 4, 8, beacon);
    fillSquare(world, base, 8, 2, 8, 2); // 5x5 only, no 3x3 above it
    EXPECT_EQ(beaconLevel(world, reg, 8, 4, 8), 0);
}

TEST(BeaconLevelTest, NonBaseBlocksNeverCount) {
    const BlockRegistry reg = beaconRegistry();
    const BlockID beacon = reg.idForName("beacon");
    World world;
    world.getOrCreateChunk({0, 0, 0});
    world.setBlock(8, 4, 8, beacon);
    fillSquare(world, reg.idForName("stone"), 8, 3, 8, 1); // 3x3 of stone
    EXPECT_EQ(beaconLevel(world, reg, 8, 4, 8), 0);
}

TEST(BeaconLevelTest, RegistryWithoutBaseBlocksFastExits) {
    // A registry with no beacon_base block at all: every position is level 0
    // (the shipped world always has iron_block, but the gate is structural).
    BlockRegistry reg;
    const std::string stone =
        "{\"id\":\"stone\",\"properties\":{\"hardness\":1.5},"
        "\"model\":{\"textures\":{\"top\":\"stone\",\"side\":\"stone\","
        "\"bottom\":\"stone\"}}}";
    const std::string beacon =
        "{\"id\":\"beacon\",\"properties\":{\"hardness\":8.0,"
        "\"drops\":\"beacon\",\"beacon_effect\":\"absorption\"},"
        "\"model\":{\"textures\":{\"top\":\"beacon\",\"side\":\"beacon\","
        "\"bottom\":\"beacon\"}}}";
    for (const std::string& def : {stone, beacon}) {
        auto r = reg.registerBlock(parseDef(def));
        if (!r.ok()) {
            throw std::runtime_error(r.error().message);
        }
    }
    World world;
    world.getOrCreateChunk({0, 0, 0});
    world.setBlock(8, 4, 8, reg.idForName("beacon"));
    fillSquare(world, reg.idForName("stone"), 8, 3, 8, 1);
    EXPECT_EQ(beaconLevel(world, reg, 8, 4, 8), 0);
}

TEST(BeaconLevelTest, DeterministicRepeat) {
    const BlockRegistry reg = beaconRegistry();
    const World world =
        beaconWorld(reg.idForName("beacon"), reg.idForName("iron"), 3);
    for (int i = 0; i < 5; ++i) {
        EXPECT_EQ(beaconLevel(world, reg, 8, 4, 8), 3);
    }
}
