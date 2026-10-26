// PRE-6: the drop-light sampler (itemDropLight) — the cell selection
// (AABB center, per-axis floor including negative coordinates), the
// 0..1 normalization, the World getters' fallbacks (void, below the
// world), day-independence (the function takes NO dayFactor — the
// fragment's existing formula applies the day/night scale) and the
// render.drop_light config semantics (code default true, the
// render.fog getBool precedent — config.json is untouched).

#include <gtest/gtest.h>

#include <utility>

#include "block/block_registry.hpp"
#include "core/config.hpp"
#include "entity/item_entity.hpp"
#include "entity/item_light.hpp"
#include "world/chunk.hpp"
#include "world/world.hpp"

using namespace mc;

namespace {

// A world with chunk (0,0,0) only; the item registry is empty (the
// constructor keeps the reference, the physics is never run here).
struct Fixture {
    World world;
    BlockRegistry blocks;
    ItemEntity item {world, blocks, "stone", 64, 1, Vec3{0, 0, 0}};

    Fixture() { world.getOrCreateChunk({0, 0, 0}); }

    void putSky(int x, int y, int z, std::uint8_t v) {
        const BlockPos p {x, y, z};
        const ChunkPos cp = worldToChunk(p);
        const BlockPos local = worldToChunkLocal(cp, p);
        world.getOrCreateChunk(cp).setSkyLightAt(local.x, local.y, local.z, v);
    }
    void putBlock(int x, int y, int z, std::uint8_t v) {
        const BlockPos p {x, y, z};
        const ChunkPos cp = worldToChunk(p);
        const BlockPos local = worldToChunkLocal(cp, p);
        world.getOrCreateChunk(cp).setBlockLightAt(local.x, local.y, local.z,
                                                   v);
    }
};

// The light of the AABB-CENTER cell: the item at (2.5, 3.0, 4.5) has its
// center at (2.5, 3.125, 4.5) -> cell (2, 3, 4). A block-light 3 in the
// cell ABOVE the center ((2, 4, 4)) must not be read.
TEST(ItemDropLightTest, CenterCellSelection) {
    Fixture f;
    f.putBlock(2, 3, 4, 8);
    f.putBlock(2, 4, 4, 3); // above the center: ignored
    f.item.setPosition({2.5f, 3.0f, 4.5f});
    const std::pair<float, float> light = itemDropLight(f.world, f.item);
    EXPECT_FLOAT_EQ(light.first, 0.0f);
    EXPECT_FLOAT_EQ(light.second, 8.0f / 15.0f);
}

// The center exactly ON the cell boundary (pos.y 2.875 -> center 3.0):
// floor keeps the UPPER cell (the item's cell is the one its center is
// in, Minecraft "entity cell" semantics).
TEST(ItemDropLightTest, CenterOnBoundaryFLoorsToUpperCell) {
    Fixture f;
    f.putBlock(2, 3, 4, 8);
    f.putBlock(2, 2, 4, 5); // the lower cell: ignored
    f.item.setPosition({2.5f, 2.875f, 4.5f});
    const std::pair<float, float> light = itemDropLight(f.world, f.item);
    EXPECT_FLOAT_EQ(light.second, 8.0f / 15.0f);
}

// Negative coordinates: floor (NOT truncation) — the center at
// (-0.075, 8.125, -0.075) is in cell (-1, 8, -1). Truncation would read
// (0, 8, 0), whose chunk is never created here and would yield block 0.
TEST(ItemDropLightTest, NegativeCoordinatesUseFloor) {
    World world;
    BlockRegistry blocks;
    const BlockPos c {-1, 8, -1};
    const ChunkPos cp = worldToChunk(c);
    const BlockPos local = worldToChunkLocal(cp, c);
    world.getOrCreateChunk(cp).setBlockLightAt(local.x, local.y, local.z, 5);
    ItemEntity item {world, blocks, "stone", 64, 1, Vec3{-0.075f, 8.0f,
                                                         -0.075f}};
    const std::pair<float, float> light = itemDropLight(world, item);
    EXPECT_FLOAT_EQ(light.first, 0.0f);
    EXPECT_FLOAT_EQ(light.second, 5.0f / 15.0f);
}

// Normalization: the exact 0..15/15 values (sky 15 -> 1.0, block 8 ->
// 8/15, a mixed cell 7/3).
TEST(ItemDropLightTest, ExactNormalizedValues) {
    Fixture f;
    f.putSky(2, 3, 4, 15);
    f.item.setPosition({2.5f, 3.0f, 4.5f});
    auto light = itemDropLight(f.world, f.item);
    EXPECT_FLOAT_EQ(light.first, 1.0f);
    EXPECT_FLOAT_EQ(light.second, 0.0f);

    f.putSky(3, 3, 4, 7);
    f.putBlock(3, 3, 4, 3);
    f.item.setPosition({3.5f, 3.0f, 4.5f});
    light = itemDropLight(f.world, f.item);
    EXPECT_FLOAT_EQ(light.first, 7.0f / 15.0f);
    EXPECT_FLOAT_EQ(light.second, 3.0f / 15.0f);
}

// The World getters' fallbacks: a cell whose chunk was never created
// (void around the world) reads open sky (15) / no block light (0); a
// cell below the world (y < 0) reads 0 / 0.
TEST(ItemDropLightTest, WorldFallbacks) {
    Fixture f; // only chunk (0,0,0) created
    f.item.setPosition({100.5f, 8.0f, 100.5f}); // void (chunk (6,0,6) absent)
    auto light = itemDropLight(f.world, f.item);
    EXPECT_FLOAT_EQ(light.first, 1.0f);
    EXPECT_FLOAT_EQ(light.second, 0.0f);

    f.item.setPosition({2.5f, -0.5f, 4.5f}); // center y -0.375 -> cell -1
    light = itemDropLight(f.world, f.item);
    EXPECT_FLOAT_EQ(light.first, 0.0f);
    EXPECT_FLOAT_EQ(light.second, 0.0f);
}

// Day-independence: the function takes NO dayFactor — the raw light is
// returned and the fragment's existing formula (max(v_skyLight *
// u_dayFactor, v_blockLight)) applies the day/night scale, so a drop
// brightens and dims with the world exactly like the chunks. Calling it
// twice on an unchanged world is bit-identical (the per-frame resample
// is pure).
TEST(ItemDropLightTest, DayIndependentAndPure) {
    Fixture f;
    f.putSky(2, 3, 4, 9);
    f.putBlock(2, 3, 4, 4);
    f.item.setPosition({2.5f, 3.0f, 4.5f});
    const auto a = itemDropLight(f.world, f.item);
    const auto b = itemDropLight(f.world, f.item);
    EXPECT_FLOAT_EQ(a.first, b.first);
    EXPECT_FLOAT_EQ(a.second, b.second);
    EXPECT_FLOAT_EQ(a.first, 9.0f / 15.0f);
    EXPECT_FLOAT_EQ(a.second, 4.0f / 15.0f);
}

// render.drop_light: the key is absent from config.json by design — the
// code default (true, the render.fog / render.water_ripples getBool
// precedent) applies.
TEST(DropLightConfigTest, MissingKeyDefaultsToOn) {
    Config cfg;
    EXPECT_TRUE(cfg.getBool("render.drop_light", true));
}

// An explicit false turns the drop light off (the A/B "off" control =
// the v0.29.0 pipeline bit-identical).
TEST(DropLightConfigTest, ExplicitFalseTurnsOff) {
    Config cfg;
    cfg.set("render.drop_light", Json(false));
    EXPECT_FALSE(cfg.getBool("render.drop_light", true));
}

} // namespace
