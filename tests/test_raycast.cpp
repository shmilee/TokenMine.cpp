#include <gtest/gtest.h>

#include <cmath>
#include <string>

#include "block/block_registry.hpp"
#include "core/json.hpp"
#include "world/raycast.hpp"
#include "world/world.hpp"

using namespace mc;

namespace {

constexpr float kInfT = 1000.0f; // generous reach for geometry tests

Json parseJson(const std::string& text) {
    auto r = Json::parse(text);
    if (!r.ok()) {
        ADD_FAILURE() << "parse failed: " << r.error().message;
        return Json::object();
    }
    return *r;
}

BlockRegistry makeBlocks() {
    BlockRegistry blocks;
    auto r = blocks.registerBlock(parseJson(
        R"({"id":"stone","model":{"textures":{"top":"t","side":"t","bottom":"t"}}})"));
    if (!r.ok()) {
        ADD_FAILURE() << r.error().message;
    }
    r = blocks.registerBlock(parseJson(
        R"({"id":"water","properties":{"solid":false,"opaque":false,"fluid":"water","destroyable":false},)"
        R"("model":{"textures":{"top":"w","side":"w","bottom":"w"}}})"));
    if (!r.ok()) {
        ADD_FAILURE() << r.error().message;
    }
    r = blocks.registerBlock(parseJson(
        R"({"id":"glass","properties":{"solid":false,"opaque":false,"transparent":true},)"
        R"("model":{"textures":{"top":"g","side":"g","bottom":"g"}}})"));
    if (!r.ok()) {
        ADD_FAILURE() << r.error().message;
    }
    return blocks;
}

bool nearF(float a, float b, float eps = 1e-4f) {
    return std::fabs(a - b) < eps;
}
bool vecNear(const Vec3& a, const Vec3& b, float eps = 1e-4f) {
    return nearF(a.x, b.x, eps) && nearF(a.y, b.y, eps) && nearF(a.z, b.z, eps);
}

} // namespace

// -- geometry -----------------------------------------------------------------

TEST(RaycastTest, EmptyWorldNoHit) {
    World world;
    EXPECT_FALSE(raycastBlock(world, {0.5f, 0.5f, 0.5f}, {0.0f, 0.0f, -1.0f},
                              kInfT).has_value());
}

TEST(RaycastTest, ZeroDirectionNoHit) {
    World world;
    ASSERT_TRUE(world.setBlock(0, 0, -1, BlockID{1}));
    EXPECT_FALSE(raycastBlock(world, {0.5f, 0.5f, 0.5f}, Vec3::zero(),
                              kInfT).has_value());
}

TEST(RaycastTest, NonPositiveReachNoHit) {
    World world;
    ASSERT_TRUE(world.setBlock(0, 0, -1, BlockID{1}));
    EXPECT_FALSE(raycastBlock(world, {0.5f, 0.5f, 0.5f}, {0.0f, 0.0f, -1.0f},
                              0.0f).has_value());
    EXPECT_FALSE(raycastBlock(world, {0.5f, 0.5f, 0.5f}, {0.0f, 0.0f, -1.0f},
                              -1.0f).has_value());
}

TEST(RaycastTest, HitsBlockInFront) {
    World world;
    ASSERT_TRUE(world.setBlock(0, 0, 0, BlockID{1}));
    auto hit = raycastBlock(world, {0.5f, 0.5f, 2.5f}, {0.0f, 0.0f, -1.0f},
                            kInfT);
    ASSERT_TRUE(hit.has_value());
    EXPECT_EQ(hit->block.x, 0);
    EXPECT_EQ(hit->block.y, 0);
    EXPECT_EQ(hit->block.z, 0);
    // The ray hit the +Z face of the block (the z=1 plane): normal points
    // back at the ray, distance is origin.z - 1.
    EXPECT_TRUE(vecNear(hit->normal, Vec3 {0.0f, 0.0f, 1.0f}));
    EXPECT_NEAR(hit->distance, 1.5f, 1e-4f);
}

TEST(RaycastTest, StopsAtFirstBlock) {
    World world;
    ASSERT_TRUE(world.setBlock(0, 0, 0, BlockID{1}));
    ASSERT_TRUE(world.setBlock(0, 0, -1, BlockID{1}));
    auto hit = raycastBlock(world, {0.5f, 0.5f, 2.5f}, {0.0f, 0.0f, -1.0f},
                            kInfT);
    ASSERT_TRUE(hit.has_value());
    EXPECT_EQ(hit->block.z, 0); // the nearer block wins
}

TEST(RaycastTest, AllSixFaceNormals) {
    const BlockID stone {1};
    struct Case {
        Vec3 origin;
        Vec3 dir;
        Vec3 expectedNormal;
    };
    const Case cases[] = {
        {{2.5f, 0.5f, 0.5f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},
        {{-2.5f, 0.5f, 0.5f}, {1.0f, 0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}},
        {{0.5f, 2.5f, 0.5f}, {0.0f, -1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
        {{0.5f, -2.5f, 0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, -1.0f, 0.0f}},
        {{0.5f, 0.5f, 2.5f}, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f, 1.0f}},
        {{0.5f, 0.5f, -2.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, -1.0f}},
    };
    for (const Case& c : cases) {
        World world;
        ASSERT_TRUE(world.setBlock(0, 0, 0, stone));
        auto hit = raycastBlock(world, c.origin, c.dir, kInfT);
        ASSERT_TRUE(hit.has_value());
        EXPECT_EQ(hit->block.x, 0) << "case " << c.origin.x;
        EXPECT_EQ(hit->block.y, 0);
        EXPECT_EQ(hit->block.z, 0);
        EXPECT_TRUE(vecNear(hit->normal, c.expectedNormal))
            << "origin " << c.origin.x << "," << c.origin.y << ","
            << c.origin.z;
    }
}

TEST(RaycastTest, DiagonalRayHitsExpectedCell) {
    World world;
    ASSERT_TRUE(world.setBlock(1, 0, 0, BlockID{1}));
    // direction (2,1,0)/sqrt(5) from (0.5,0.5,0.5): crosses the x=1 plane
    // at t = 0.5/sqrt(5)/2 = sqrt(5)/4 ~ 0.559, where y = 0.75 (cell y=0).
    const float inv = 1.0f / std::sqrt(5.0f);
    auto hit = raycastBlock(world, {0.5f, 0.5f, 0.5f},
                            {2.0f * inv, 1.0f * inv, 0.0f}, kInfT);
    ASSERT_TRUE(hit.has_value());
    EXPECT_EQ(hit->block.x, 1);
    EXPECT_EQ(hit->block.y, 0);
    EXPECT_EQ(hit->block.z, 0);
    EXPECT_TRUE(vecNear(hit->normal, Vec3 {-1.0f, 0.0f, 0.0f}));
    EXPECT_NEAR(hit->distance, 0.5f / (2.0f * inv), 1e-4f);
}

TEST(RaycastTest, NegativeCoordinates) {
    World world;
    ASSERT_TRUE(world.setBlock(-5, 3, -2, BlockID{1}));
    // From inside cell (-4,3,-1) with direction (-2,0,-3)/sqrt(13): first
    // crosses z=-2 (entering air cell (-4,3,-2)), then x=-5 into the block.
    // (No corner tie: the two planes are crossed at t = sqrt(13)/6 and
    // t = sqrt(13)/4.)
    const float inv = 1.0f / std::sqrt(13.0f);
    auto hit = raycastBlock(world, {-3.5f, 3.5f, -0.5f},
                            {-2.0f * inv, 0.0f, -3.0f * inv}, kInfT);
    ASSERT_TRUE(hit.has_value());
    EXPECT_EQ(hit->block.x, -5);
    EXPECT_EQ(hit->block.y, 3);
    EXPECT_EQ(hit->block.z, -2);
    EXPECT_TRUE(vecNear(hit->normal, Vec3 {1.0f, 0.0f, 0.0f}));
    EXPECT_NEAR(hit->distance, 0.5f / (2.0f * inv), 1e-4f);
}

TEST(RaycastTest, BeyondReachNoHit) {
    World world;
    // The near face of cell (0,0,-1) is the z=0 plane: distance 5.5 from
    // the origin.
    ASSERT_TRUE(world.setBlock(0, 0, -1, BlockID{1}));
    const Vec3 origin {0.5f, 0.5f, 5.5f};
    const Vec3 dir {0.0f, 0.0f, -1.0f};
    EXPECT_FALSE(raycastBlock(world, origin, dir, 5.0f).has_value());
    auto hit = raycastBlock(world, origin, dir, 5.5f);
    ASSERT_TRUE(hit.has_value()); // exactly at reach still hits
    EXPECT_NEAR(hit->distance, 5.5f, 1e-4f);
}

TEST(RaycastTest, OriginInsideBlockSkipsIt) {
    World world;
    ASSERT_TRUE(world.setBlock(0, 0, 0, BlockID{1}));
    ASSERT_TRUE(world.setBlock(0, 0, -1, BlockID{1}));
    // The eye is inside the first block: it is skipped, the next one wins.
    auto hit = raycastBlock(world, {0.5f, 0.5f, 0.5f}, {0.0f, 0.0f, -1.0f},
                            kInfT);
    ASSERT_TRUE(hit.has_value());
    EXPECT_EQ(hit->block.z, -1);
}

TEST(RaycastTest, OriginOnCellBoundary) {
    World world;
    ASSERT_TRUE(world.setBlock(0, 0, 0, BlockID{1}));
    // The eye sits exactly on the z=1 plane: the cell ahead (z=0) must be
    // tested immediately, not missed by an off-by-one.
    auto hit = raycastBlock(world, {0.5f, 0.5f, 1.0f}, {0.0f, 0.0f, -1.0f},
                            kInfT);
    ASSERT_TRUE(hit.has_value());
    EXPECT_EQ(hit->block.z, 0);
    EXPECT_NEAR(hit->distance, 0.0f, 1e-5f);
}

TEST(RaycastTest, ParallelAxesNeverCrossed) {
    World world;
    // A slab of stone at z=-3 spanning all x,y tested; the ray travels
    // purely along X (dy=dz=0) at y=0.5,z=0.5 and must NOT hit it...
    ASSERT_TRUE(world.setBlock(5, 0, -3, BlockID{1}));
    EXPECT_FALSE(raycastBlock(world, {0.5f, 0.5f, 0.5f}, {1.0f, 0.0f, 0.0f},
                              100.0f).has_value());
    // ...while a ray with a z component does.
    ASSERT_TRUE(world.setBlock(5, 0, 0, BlockID{1}));
    EXPECT_TRUE(raycastBlock(world, {0.5f, 0.5f, 0.5f}, {1.0f, 0.0f, 0.0f},
                             100.0f).has_value());
}

TEST(RaycastTest, FliesOutOfWorldWithoutHit) {
    World world;
    ASSERT_TRUE(world.setBlock(0, 0, 0, BlockID{1}));
    // Start above the world's Y range and shoot further up: getBlock reads
    // air, so there is no hit even with a huge reach.
    EXPECT_FALSE(
        raycastBlock(world, {0.5f, 200.5f, 0.5f}, {0.0f, 1.0f, 0.0f},
                     1000.0f)
            .has_value());
}

TEST(RaycastTest, NonSolidBlockIsHitGeometrically) {
    // The raycast is property-free: a non-solid block (glass) still blocks
    // the ray, and the break rule (destroyable) is applied by the caller.
    BlockRegistry blocks = makeBlocks();
    const BlockID glass = blocks.idForName("glass");
    World world;
    ASSERT_TRUE(world.setBlock(0, 0, -1, glass));
    auto hit = raycastBlock(world, {0.5f, 0.5f, 0.5f}, {0.0f, 0.0f, -1.0f},
                            kInfT);
    ASSERT_TRUE(hit.has_value());
    EXPECT_EQ(world.getBlock(hit->block.x, hit->block.y, hit->block.z), glass);
}

TEST(RaycastTest, RaycastPassesThroughFluid) {
    // Water lives in the fluid array (the block array reads air), so the
    // ray passes through it and hits the block behind — aiming through
    // water selects the lake floor, not the water.
    BlockRegistry blocks = makeBlocks();
    World world;
    world.getOrCreateChunk({0, 0, 0});
    ASSERT_TRUE(world.setFluid(0, 0, 0, FluidKind::Water, kMaxFluidLevel));
    ASSERT_TRUE(world.setBlock(0, 0, -1, BlockID{1}));
    auto hit = raycastBlock(world, {0.5f, 0.5f, 0.5f}, {0.0f, 0.0f, -1.0f},
                            kInfT);
    ASSERT_TRUE(hit.has_value());
    EXPECT_EQ(hit->block, (BlockPos{0, 0, -1}));
    EXPECT_NEAR(hit->distance, 0.5f, 1e-4f);
}

// -- placement validation ------------------------------------------------------

namespace {

// Player AABB of a player standing at (0.5, 1.0, 0.5) (feet on the floor at
// y=1), default 0.6x1.8 dimensions.
constexpr Vec3 kPlayerMin {0.2f, 1.0f, 0.2f};
constexpr Vec3 kPlayerMax {0.8f, 2.8f, 0.8f};

} // namespace

TEST(RaycastTest, CanPlaceInEmptyCell) {
    BlockRegistry blocks = makeBlocks();
    World world;
    EXPECT_TRUE(canPlaceAt(world, blocks, {0, 1, -2}, kPlayerMin, kPlayerMax));
}

TEST(RaycastTest, CannotPlaceIntoSolidCell) {
    BlockRegistry blocks = makeBlocks();
    const BlockID stone = blocks.idForName("stone");
    World world;
    ASSERT_TRUE(world.setBlock(0, 1, -2, stone));
    EXPECT_FALSE(canPlaceAt(world, blocks, {0, 1, -2}, kPlayerMin, kPlayerMax));
}

TEST(RaycastTest, CanPlaceIntoFluidCell) {
    // A fluid cell reads as air and is non-solid: placeable (placing a
    // block into water displaces the fluid, see World::setBlock).
    BlockRegistry blocks = makeBlocks();
    World world;
    world.getOrCreateChunk({0, 0, -1}); // z=-2 floors to chunk -1
    ASSERT_TRUE(world.setFluid(0, 1, -2, FluidKind::Water, kMaxFluidLevel));
    EXPECT_TRUE(canPlaceAt(world, blocks, {0, 1, -2}, kPlayerMin, kPlayerMax));
}

TEST(RaycastTest, CannotPlaceOutsideYRange) {
    BlockRegistry blocks = makeBlocks();
    World world;
    EXPECT_FALSE(canPlaceAt(world, blocks, {0, -1, 0}, kPlayerMin, kPlayerMax));
    EXPECT_FALSE(canPlaceAt(world, blocks, {0, kWorldHeight, 0}, kPlayerMin,
                            kPlayerMax));
    EXPECT_TRUE(canPlaceAt(world, blocks, {0, kWorldHeight - 1, 0}, kPlayerMin,
                           kPlayerMax));
}

TEST(RaycastTest, CannotPlaceIntoCellOverlappingPlayer) {
    BlockRegistry blocks = makeBlocks();
    World world;
    // The cell the player's feet stand in.
    EXPECT_FALSE(canPlaceAt(world, blocks, {0, 1, 0}, kPlayerMin, kPlayerMax));
    // A cell at head height inside the column.
    EXPECT_FALSE(canPlaceAt(world, blocks, {0, 2, 0}, kPlayerMin, kPlayerMax));
    // A player straddling the x=1 boundary pokes into cell (1, y, 0).
    EXPECT_FALSE(canPlaceAt(world, blocks, {1, 1, 0},
                            Vec3 {0.9f, 1.0f, 0.2f},
                            Vec3 {1.5f, 2.8f, 0.8f}));
}

TEST(RaycastTest, CanPlaceAdjacentToPlayer) {
    BlockRegistry blocks = makeBlocks();
    World world;
    // Below the feet (flush contact is not an overlap).
    EXPECT_TRUE(canPlaceAt(world, blocks, {0, 0, 0}, kPlayerMin, kPlayerMax));
    // Beside the player (flush on the x face).
    EXPECT_TRUE(canPlaceAt(world, blocks, {1, 2, 0}, kPlayerMin, kPlayerMax));
    // Above the head.
    EXPECT_TRUE(canPlaceAt(world, blocks, {0, 3, 0}, kPlayerMin, kPlayerMax));
}

// -- planting placement restriction (Phase 53) -------------------------------

TEST(RaycastTest, CanPlantOnListedSoil) {
    BlockRegistry blocks = makeBlocks();
    const BlockID stone = blocks.idForName("stone");
    World world;
    ASSERT_TRUE(world.setBlock(0, 0, 0, stone)); // soil below target (0,1,0)
    EXPECT_TRUE(canPlantAt(world, blocks, {0, 1, 0}, {"stone"}));
}

TEST(RaycastTest, CannotPlantOnWrongSoil) {
    BlockRegistry blocks = makeBlocks();
    const BlockID glass = blocks.idForName("glass");
    World world;
    ASSERT_TRUE(world.setBlock(0, 0, 0, glass));
    EXPECT_FALSE(canPlantAt(world, blocks, {0, 1, 0}, {"stone"}));
}

TEST(RaycastTest, CanPlantWithMultipleSoils) {
    BlockRegistry blocks = makeBlocks();
    const BlockID glass = blocks.idForName("glass");
    World world;
    ASSERT_TRUE(world.setBlock(0, 0, 0, glass));
    // Matches the SECOND entry of the list.
    EXPECT_TRUE(canPlantAt(world, blocks, {0, 1, 0}, {"stone", "glass"}));
}

TEST(RaycastTest, EmptyPlantOnUnrestricted) {
    BlockRegistry blocks = makeBlocks();
    World world;
    // Nothing below (air) — an empty list is still unrestricted.
    EXPECT_TRUE(canPlantAt(world, blocks, {0, 5, 0}, {}));
}

TEST(RaycastTest, CannotPlantWithAirBelow) {
    BlockRegistry blocks = makeBlocks();
    World world;
    // Nothing below (0,1,0) -> air, which is not "stone".
    EXPECT_FALSE(canPlantAt(world, blocks, {0, 1, 0}, {"stone"}));
}

TEST(RaycastTest, CannotPlantBelowBottomRow) {
    BlockRegistry blocks = makeBlocks();
    World world;
    // Target at y=0: below is y=-1 (out of range, reads as air) -> rejected.
    EXPECT_FALSE(canPlantAt(world, blocks, {0, 0, 0}, {"stone"}));
}

TEST(RaycastTest, CannotPlaceInCellPlayerIsWalkingInto) {
    BlockRegistry blocks = makeBlocks();
    World world;
    // A player one step ahead (feet at x=0.9..1.5, so the AABB pokes into
    // cell x=0's [0,1) range? No: min.x=0.9 < 1 && max.x=1.5 > 0 -> overlap
    // with cell (0, y, 0) only if min.x < 1.0: 0.9 < 1.0 yes).
    const Vec3 min {0.9f, 1.0f, 0.2f};
    const Vec3 max {1.5f, 2.8f, 0.8f};
    EXPECT_FALSE(canPlaceAt(world, blocks, {0, 1, 0}, min, max));
    // But the cell fully ahead (x in [1, 2)) is clear except the part the
    // AABB occupies: min.x=0.9 < 2 && max.x=1.5 > 1 -> overlaps (1,1,0).
    EXPECT_FALSE(canPlaceAt(world, blocks, {1, 1, 0}, min, max));
    // And two ahead is free.
    EXPECT_TRUE(canPlaceAt(world, blocks, {2, 1, 0}, min, max));
}

// -- line of sight (Phase 38, skeleton shooting) --------------------------------

TEST(RaycastTest, LineOfSightClearThroughAir) {
    World world;
    EXPECT_TRUE(hasLineOfSight(world, {0.5f, 1.5f, 5.5f}, {0.5f, 1.5f, -4.5f}));
}

TEST(RaycastTest, LineOfSightBlockedByWallBetween) {
    World world;
    // A block in the middle of the segment: the shot must not be fired.
    ASSERT_TRUE(world.setBlock(0, 1, 0, BlockID{1}));
    EXPECT_FALSE(hasLineOfSight(world, {0.5f, 1.5f, 5.5f},
                                {0.5f, 1.5f, -4.5f}));
}

TEST(RaycastTest, LineOfSightClearWhenWallIsBeyondTheTarget) {
    World world;
    // A wall PAST the target does not block the segment.
    ASSERT_TRUE(world.setBlock(0, 1, -6, BlockID{1}));
    EXPECT_TRUE(hasLineOfSight(world, {0.5f, 1.5f, 5.5f},
                               {0.5f, 1.5f, -4.5f}));
}

TEST(RaycastTest, LineOfSightExemptFromCell) {
    World world;
    // The ray never hits the block it starts in (raycastBlock convention):
    // a solid cell around `from` is transparent to the shot.
    ASSERT_TRUE(world.setBlock(0, 1, 5, BlockID{1}));
    EXPECT_TRUE(hasLineOfSight(world, {0.5f, 1.5f, 5.5f},
                               {0.5f, 1.5f, -4.5f}));
}

TEST(RaycastTest, LineOfSightDiagonalWall) {
    World world;
    // A diagonal line from (0.5, 1.5, 4.5) to (3.5, 1.5, 0.5) crosses the
    // cells (1, 1, 3), (2, 1, 2), ... — a block in any of them blocks,
    // one in a cell the line never enters does not.
    ASSERT_TRUE(world.setBlock(2, 1, 2, BlockID{1}));
    EXPECT_FALSE(hasLineOfSight(world, {0.5f, 1.5f, 4.5f},
                                {3.5f, 1.5f, 0.5f}));
    world = World();
    ASSERT_TRUE(world.setBlock(0, 1, 0, BlockID{1})); // off the diagonal
    EXPECT_TRUE(hasLineOfSight(world, {0.5f, 1.5f, 4.5f},
                               {3.5f, 1.5f, 0.5f}));
}

TEST(RaycastTest, LineOfSightCoincidentPoints) {
    World world;
    ASSERT_TRUE(world.setBlock(0, 0, 0, BlockID{1}));
    EXPECT_TRUE(hasLineOfSight(world, {0.5f, 0.5f, 0.5f},
                               {0.5f, 0.5f, 0.5f}));
}

// -- raycastFluid (Phase 47, the bucket's target ray) ------------------------

TEST(RaycastFluidTest, HitsFluidCellInOpenSpace) {
    // The first cell carrying fluid is returned (an otherwise empty
    // world). setFluid requires an existing chunk (the void stays dry),
    // so one block creates it (off the ray's path).
    World world;
    ASSERT_TRUE(world.setBlock(0, 0, 0, BlockID{1}));
    ASSERT_TRUE(world.setFluid(5, 3, 2, FluidKind::Water, 8));
    const auto hit = raycastFluid(world, {0.5f, 3.5f, 2.5f},
                                  {1.0f, 0.0f, 0.0f}, 20.0f);
    ASSERT_TRUE(hit.has_value());
    EXPECT_EQ(hit->x, 5);
    EXPECT_EQ(hit->y, 3);
    EXPECT_EQ(hit->z, 2);
}

TEST(RaycastFluidTest, BlockOccludesTheRay) {
    // A solid wall between the origin and the water returns the BLOCK
    // cell (Minecraft: the bucket uses block targeting — you cannot
    // scoop through a wall). The caller then sees fluid level 0 and does
    // nothing.
    World world;
    ASSERT_TRUE(world.setBlock(3, 3, 0, BlockID{1}));
    ASSERT_TRUE(world.setFluid(6, 3, 0, FluidKind::Water, 8));
    const auto hit = raycastFluid(world, {0.5f, 3.5f, 0.5f},
                                  {1.0f, 0.0f, 0.0f}, 20.0f);
    ASSERT_TRUE(hit.has_value());
    EXPECT_EQ(hit->x, 3);
    EXPECT_EQ(world.getFluid(3, 3, 0), 0); // the block cell: no fluid
}

TEST(RaycastFluidTest, StopsAtFirstOccluder) {
    // Fluid BEFORE the block: the fluid cell wins (it is nearer).
    World world;
    ASSERT_TRUE(world.setBlock(4, 1, 1, BlockID{1})); // creates the chunk
    ASSERT_TRUE(world.setFluid(2, 1, 1, FluidKind::Water, 7)); // flowing (not a source)
    const auto hit = raycastFluid(world, {0.5f, 1.5f, 1.5f},
                                  {1.0f, 0.0f, 0.0f}, 20.0f);
    ASSERT_TRUE(hit.has_value());
    EXPECT_EQ(hit->x, 2);
    EXPECT_EQ(world.getFluid(2, 1, 1), 7);
}

TEST(RaycastFluidTest, BeyondReachNoHit) {
    World world;
    ASSERT_TRUE(world.setBlock(0, 0, 0, BlockID{1})); // creates the chunk
    ASSERT_TRUE(world.setFluid(9, 1, 1, FluidKind::Water, 8));
    EXPECT_FALSE(raycastFluid(world, {0.5f, 1.5f, 1.5f},
                              {1.0f, 0.0f, 0.0f}, 5.0f)
                     .has_value());
}

TEST(RaycastFluidTest, EmptyWorldNoHit) {
    World world;
    EXPECT_FALSE(raycastFluid(world, {0.5f, 1.5f, 1.5f},
                              {1.0f, 0.0f, 0.0f}, 20.0f)
                     .has_value());
}

TEST(RaycastFluidTest, OriginCellFluidIsSkipped) {
    // The ray never hits the cell it starts in (the raycastBlock
    // convention): water around the eye is transparent, water further
    // ahead is not.
    World world;
    ASSERT_TRUE(world.setBlock(0, 0, 0, BlockID{1})); // creates the chunk
    ASSERT_TRUE(world.setFluid(0, 1, 1, FluidKind::Water, 8));  // the origin cell
    ASSERT_TRUE(world.setFluid(3, 1, 1, FluidKind::Water, 8));
    const auto hit = raycastFluid(world, {0.5f, 1.5f, 1.5f},
                                  {1.0f, 0.0f, 0.0f}, 20.0f);
    ASSERT_TRUE(hit.has_value());
    EXPECT_EQ(hit->x, 3);
}

// -- FLUID-4: bucketPourTarget (the pour onto a water surface) ----------------
//
// The phase-47 pour accepted only an AIR face neighbor (pouring against a
// water surface was a documented no-op). bucketPourTarget resolves the
// candidate (block-ray face neighbor, or the fluid ray's own hit cell when
// no block is in reach) and accepts either (a) air without fluid (the
// legacy placement, bit-identical) or (b) a FLOWING cell of the bucket's
// own kind (it becomes the source). An already-source cell or a foreign
// kind is a no-op (Minecraft: the bucket is kept).

namespace {

std::optional<BlockHit> floorHit(int y = 0) {
    return BlockHit {BlockPos {8, y, 8}, Vec3 {0.0f, 1.0f, 0.0f}, 4.0f};
}

// A player AABB far from any candidate cell (no overlap by construction).
const Vec3 kFarMin {-20.0f, 0.0f, -20.0f};
const Vec3 kFarMax {-19.0f, 1.0f, -19.0f};

} // namespace

TEST(RaycastPourTargetTest, LegacyAirFaceNeighbor) {
    const BlockRegistry reg = makeBlocks();
    const BlockID stone = reg.idForName("stone");
    World world;
    ASSERT_TRUE(world.setBlock(8, 0, 8, stone));
    // (8,1,8) is air without fluid: the legacy placement target.
    auto t = bucketPourTarget(world, reg, floorHit(), std::nullopt,
                              FluidKind::Water, kFarMin, kFarMax);
    ASSERT_TRUE(t.has_value());
    EXPECT_EQ(t->x, 8);
    EXPECT_EQ(t->y, 1);
    EXPECT_EQ(t->z, 8);
}

TEST(RaycastPourTargetTest, PourOntoFlowingFaceNeighbor) {
    const BlockRegistry reg = makeBlocks();
    const BlockID stone = reg.idForName("stone");
    World world;
    ASSERT_TRUE(world.setBlock(8, 0, 8, stone));
    ASSERT_TRUE(world.setFluid(8, 1, 8, FluidKind::Water, 5)); // flowing
    auto t = bucketPourTarget(world, reg, floorHit(), std::nullopt,
                              FluidKind::Water, kFarMin, kFarMax);
    ASSERT_TRUE(t.has_value()); // the flowing cell becomes the source
    EXPECT_EQ(t->y, 1);
}

TEST(RaycastPourTargetTest, AlreadySourceNoOp) {
    const BlockRegistry reg = makeBlocks();
    const BlockID stone = reg.idForName("stone");
    World world;
    ASSERT_TRUE(world.setBlock(8, 0, 8, stone));
    ASSERT_TRUE(world.setFluid(8, 1, 8, FluidKind::Water, 8)); // a source
    EXPECT_FALSE(bucketPourTarget(world, reg, floorHit(), std::nullopt,
                                  FluidKind::Water, kFarMin,
                                  kFarMax).has_value());
}

TEST(RaycastPourTargetTest, ForeignKindWaterOnLavaReactionPour) {
    // FLUID-5: a water bucket against a lava cell is the REACTION POUR
    // (the phase-47 no-op) — the target is the DRY AIR cell along the
    // hit normal (directly above the lava for a floor hit), NOT the
    // lava cell itself (the existing reaction pass converts the lava
    // on the next tick).
    const BlockRegistry reg = makeBlocks();
    const BlockID stone = reg.idForName("stone");
    World world;
    ASSERT_TRUE(world.setBlock(8, 0, 8, stone));
    ASSERT_TRUE(world.setFluid(8, 1, 8, FluidKind::Lava, 5)); // lava, not water
    auto t = bucketPourTarget(world, reg, floorHit(), std::nullopt,
                              FluidKind::Water, kFarMin, kFarMax);
    ASSERT_TRUE(t.has_value());
    EXPECT_EQ(t->x, 8);
    EXPECT_EQ(t->y, 2); // one step further along the +Y hit normal
    EXPECT_EQ(t->z, 8);
    // The lava bucket names its own kind: the same cell is its target.
    ASSERT_TRUE(bucketPourTarget(world, reg, floorHit(), std::nullopt,
                                 FluidKind::Lava, kFarMin,
                                 kFarMax).has_value());
}

TEST(RaycastPourTargetTest, SolidNeighborNoOp) {
    const BlockRegistry reg = makeBlocks();
    const BlockID stone = reg.idForName("stone");
    World world;
    ASSERT_TRUE(world.setBlock(8, 0, 8, stone));
    ASSERT_TRUE(world.setBlock(8, 1, 8, stone)); // a solid face neighbor
    EXPECT_FALSE(bucketPourTarget(world, reg, floorHit(), std::nullopt,
                                  FluidKind::Water, kFarMin,
                                  kFarMax).has_value());
}

TEST(RaycastPourTargetTest, PlayerOverlapNoOp) {
    const BlockRegistry reg = makeBlocks();
    const BlockID stone = reg.idForName("stone");
    World world;
    ASSERT_TRUE(world.setBlock(8, 0, 8, stone));
    ASSERT_TRUE(world.setFluid(8, 1, 8, FluidKind::Water, 5));
    // The player's feet inside the candidate cell: rejected.
    const Vec3 nearMin {7.5f, 0.5f, 7.5f};
    const Vec3 nearMax {8.5f, 1.5f, 8.5f};
    EXPECT_FALSE(bucketPourTarget(world, reg, floorHit(), std::nullopt,
                                  FluidKind::Water, nearMin,
                                  nearMax).has_value());
}

TEST(RaycastPourTargetTest, OpenWaterNoBlockHit) {
    // No block in reach (the block ray hit nothing): the fluid ray's own
    // hit cell is the candidate (open water beyond block reach).
    const BlockRegistry reg = makeBlocks();
    World world;
    ASSERT_TRUE(world.setBlock(0, 0, 0, reg.idForName("stone"))); // a chunk
    ASSERT_TRUE(world.setFluid(5, 2, 5, FluidKind::Water, 3));
    auto t = bucketPourTarget(world, reg, std::nullopt,
                              BlockPos {5, 2, 5}, FluidKind::Water, kFarMin,
                              kFarMax);
    ASSERT_TRUE(t.has_value());
    EXPECT_EQ(t->x, 5);
    EXPECT_EQ(t->y, 2);
    EXPECT_EQ(t->z, 5);
}

TEST(RaycastPourTargetTest, OpenWaterSourceNoOp) {
    const BlockRegistry reg = makeBlocks();
    World world;
    ASSERT_TRUE(world.setBlock(0, 0, 0, reg.idForName("stone"))); // a chunk
    ASSERT_TRUE(world.setFluid(5, 2, 5, FluidKind::Water, 8));
    EXPECT_FALSE(bucketPourTarget(world, reg, std::nullopt,
                                  BlockPos {5, 2, 5}, FluidKind::Water,
                                  kFarMin, kFarMax).has_value());
}

TEST(RaycastPourTargetTest, NoHitsNoOp) {
    const BlockRegistry reg = makeBlocks();
    World world;
    EXPECT_FALSE(bucketPourTarget(world, reg, std::nullopt, std::nullopt,
                                  FluidKind::Water, kFarMin, kFarMax)
                     .has_value());
}

TEST(RaycastPourTargetTest, YRangeNoOp) {
    const BlockRegistry reg = makeBlocks();
    const BlockID stone = reg.idForName("stone");
    World world;
    // A floor at the world's top row: the +Y face neighbor is out of range.
    ASSERT_TRUE(world.setBlock(8, kWorldHeight - 1, 8, stone));
    EXPECT_FALSE(bucketPourTarget(world, reg, floorHit(kWorldHeight - 1),
                                  std::nullopt, FluidKind::Water, kFarMin,
                                  kFarMax).has_value());
}

TEST(RaycastPourTargetTest, BlockHitWinsOverFluidHit) {
    // No cross-fallback: a block hit decides the candidate even when the
    // fluid ray hit a different water cell.
    const BlockRegistry reg = makeBlocks();
    const BlockID stone = reg.idForName("stone");
    World world;
    ASSERT_TRUE(world.setBlock(8, 0, 8, stone));
    ASSERT_TRUE(world.setFluid(5, 2, 5, FluidKind::Water, 4));
    auto t = bucketPourTarget(world, reg, floorHit(), BlockPos {5, 2, 5},
                              FluidKind::Water, kFarMin, kFarMax);
    ASSERT_TRUE(t.has_value());
    EXPECT_EQ(t->x, 8); // the face neighbor, not the fluid hit
    EXPECT_EQ(t->z, 8);
}

// -- FLUID-5: the foreign-kind bucket interactions ---------------------
//
// (c1) WATER bucket against a LAVA cell: the reaction pour — the water
// source goes to the dry-air cell along the hit normal; the existing
// reaction pass converts the lava on the next tick (no new rule).
// (c2) LAVA bucket against a WATER cell: the replacement pour — the
// water cell itself becomes the lava source (setFluid's existing
// replace-the-other-kind semantics).

TEST(RaycastPourTargetTest, WaterOnLavaSourceSameTarget) {
    // The pour target does not depend on the lava level (the reaction
    // pass — not the pour — decides the product from the neighbors).
    const BlockRegistry reg = makeBlocks();
    const BlockID stone = reg.idForName("stone");
    World world;
    ASSERT_TRUE(world.setBlock(8, 0, 8, stone));
    ASSERT_TRUE(world.setFluid(8, 1, 8, FluidKind::Lava, 8)); // a source
    auto t = bucketPourTarget(world, reg, floorHit(), std::nullopt,
                              FluidKind::Water, kFarMin, kFarMax);
    ASSERT_TRUE(t.has_value());
    EXPECT_EQ(t->x, 8);
    EXPECT_EQ(t->y, 2);
    EXPECT_EQ(t->z, 8);
}

TEST(RaycastPourTargetTest, WaterOnLavaNoDryAirAbove) {
    // No dry-air placement cell -> no-op, the bucket is kept.
    const BlockRegistry reg = makeBlocks();
    const BlockID stone = reg.idForName("stone");
    World world;
    ASSERT_TRUE(world.setBlock(8, 0, 8, stone));
    ASSERT_TRUE(world.setFluid(8, 1, 8, FluidKind::Lava, 5));
    ASSERT_TRUE(world.setBlock(8, 2, 8, stone)); // a ceiling
    EXPECT_FALSE(bucketPourTarget(world, reg, floorHit(), std::nullopt,
                                  FluidKind::Water, kFarMin,
                                  kFarMax).has_value());
}

TEST(RaycastPourTargetTest, WaterOnLavaPlaceOutOfYRange) {
    // Lava in the top row: the placement cell is out of the world.
    const BlockRegistry reg = makeBlocks();
    const BlockID stone = reg.idForName("stone");
    World world;
    ASSERT_TRUE(world.setBlock(8, kWorldHeight - 2, 8, stone));
    ASSERT_TRUE(world.setFluid(8, kWorldHeight - 1, 8, FluidKind::Lava, 5));
    EXPECT_FALSE(bucketPourTarget(world, reg, floorHit(kWorldHeight - 2),
                                  std::nullopt, FluidKind::Water, kFarMin,
                                  kFarMax).has_value());
}

TEST(RaycastPourTargetTest, WaterOnLavaPlacePlayerOverlap) {
    // The placement cell inside the player -> rejected.
    const BlockRegistry reg = makeBlocks();
    const BlockID stone = reg.idForName("stone");
    World world;
    ASSERT_TRUE(world.setBlock(8, 0, 8, stone));
    ASSERT_TRUE(world.setFluid(8, 1, 8, FluidKind::Lava, 5));
    const Vec3 nearMin {7.5f, 1.5f, 7.5f};  // the (8,2,8) place cell
    const Vec3 nearMax {8.5f, 2.5f, 8.5f};
    EXPECT_FALSE(bucketPourTarget(world, reg, floorHit(), std::nullopt,
                                  FluidKind::Water, nearMin,
                                  nearMax).has_value());
}

TEST(RaycastPourTargetTest, WaterOnLavaPlaceHoldsFluid) {
    // Defensive: a fluid in the placement cell (the reaction invariant
    // keeps the kinds 6-disjoint, but the check pins the rule).
    const BlockRegistry reg = makeBlocks();
    const BlockID stone = reg.idForName("stone");
    World world;
    ASSERT_TRUE(world.setBlock(8, 0, 8, stone));
    ASSERT_TRUE(world.setFluid(8, 1, 8, FluidKind::Lava, 5));
    ASSERT_TRUE(world.setFluid(8, 2, 8, FluidKind::Water, 8));
    EXPECT_FALSE(bucketPourTarget(world, reg, floorHit(), std::nullopt,
                                  FluidKind::Water, kFarMin,
                                  kFarMax).has_value());
}

TEST(RaycastPourTargetTest, WaterOnLavaHorizontalNormal) {
    // A wall-occluded aim: the hit normal is horizontal, so the water
    // goes to the open side of the lava, not above it.
    const BlockRegistry reg = makeBlocks();
    const BlockID stone = reg.idForName("stone");
    World world;
    ASSERT_TRUE(world.setBlock(8, 0, 8, stone));
    ASSERT_TRUE(world.setFluid(9, 0, 8, FluidKind::Lava, 5));
    const std::optional<BlockHit> sideHit =
        BlockHit {BlockPos {8, 0, 8}, Vec3 {1.0f, 0.0f, 0.0f}, 4.0f};
    auto t = bucketPourTarget(world, reg, sideHit, std::nullopt,
                              FluidKind::Water, kFarMin, kFarMax);
    ASSERT_TRUE(t.has_value());
    EXPECT_EQ(t->x, 10); // lava cell + the +X normal
    EXPECT_EQ(t->y, 0);
    EXPECT_EQ(t->z, 8);
}

TEST(RaycastPourTargetTest, WaterOnOpenLavaPlusYConvention) {
    // No block in reach (open lava): the +Y convention places the
    // water above the lava cell.
    const BlockRegistry reg = makeBlocks();
    World world;
    ASSERT_TRUE(world.setBlock(0, 0, 0, reg.idForName("stone"))); // a chunk
    ASSERT_TRUE(world.setFluid(5, 2, 5, FluidKind::Lava, 5));
    auto t = bucketPourTarget(world, reg, std::nullopt, BlockPos {5, 2, 5},
                              FluidKind::Water, kFarMin, kFarMax);
    ASSERT_TRUE(t.has_value());
    EXPECT_EQ(t->x, 5);
    EXPECT_EQ(t->y, 3);
    EXPECT_EQ(t->z, 5);
}

TEST(RaycastPourTargetTest, WaterOnOpenLavaBlockedAbove) {
    const BlockRegistry reg = makeBlocks();
    const BlockID stone = reg.idForName("stone");
    World world;
    ASSERT_TRUE(world.setBlock(0, 0, 0, stone)); // a chunk
    ASSERT_TRUE(world.setFluid(5, 2, 5, FluidKind::Lava, 5));
    ASSERT_TRUE(world.setBlock(5, 3, 5, stone)); // a ceiling
    EXPECT_FALSE(bucketPourTarget(world, reg, std::nullopt,
                                  BlockPos {5, 2, 5}, FluidKind::Water,
                                  kFarMin, kFarMax).has_value());
}

TEST(RaycastPourTargetTest, LavaOnWaterReplacementPour) {
    // (c2): the lava bucket against a water SOURCE cell -> the water
    // cell itself (the replacement; the reaction pass converts the new
    // lava source per the remaining water neighbors on the next tick).
    const BlockRegistry reg = makeBlocks();
    const BlockID stone = reg.idForName("stone");
    World world;
    ASSERT_TRUE(world.setBlock(8, 0, 8, stone));
    ASSERT_TRUE(world.setFluid(8, 1, 8, FluidKind::Water, 8)); // a source
    auto t = bucketPourTarget(world, reg, floorHit(), std::nullopt,
                              FluidKind::Lava, kFarMin, kFarMax);
    ASSERT_TRUE(t.has_value());
    EXPECT_EQ(t->x, 8);
    EXPECT_EQ(t->y, 1); // the water cell, not the air above it
    EXPECT_EQ(t->z, 8);
}

TEST(RaycastPourTargetTest, LavaOnFlowingWaterReplacementPour) {
    const BlockRegistry reg = makeBlocks();
    const BlockID stone = reg.idForName("stone");
    World world;
    ASSERT_TRUE(world.setBlock(8, 0, 8, stone));
    ASSERT_TRUE(world.setFluid(8, 1, 8, FluidKind::Water, 5)); // flowing
    auto t = bucketPourTarget(world, reg, floorHit(), std::nullopt,
                              FluidKind::Lava, kFarMin, kFarMax);
    ASSERT_TRUE(t.has_value());
    EXPECT_EQ(t->x, 8);
    EXPECT_EQ(t->y, 1);
    EXPECT_EQ(t->z, 8);
}
