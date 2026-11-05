// V2-6 blob shadow geometry: the flat quad's placement + size (a pure
// function of the entity's position + model width) — the "no blob
// shadow" documented deviation consumed.
#include <gtest/gtest.h>

#include "entity/blob_shadow.hpp"

using namespace mc;

TEST(BlobShadowTest, CenterIsTopOfBlockBelowFeet) {
    // Resting on a block surface (integer feet y = 5.0, the top of
    // block 4): EXACTLY the supporting face + the z-fight lift.
    const Vec3 c = blobShadowCenter({1.2f, 5.0f, -3.4f});
    EXPECT_FLOAT_EQ(1.2f, c.x);
    EXPECT_FLOAT_EQ(5.0f + kMobShadowLift, c.y);
    EXPECT_FLOAT_EQ(-3.4f, c.z);
    // Mid-air (a jump, feet at 5.7): the top of the cell below the feet
    // (the MC blob look — the shadow does not float at the feet).
    const Vec3 m = blobShadowCenter({1.2f, 5.7f, -3.4f});
    EXPECT_FLOAT_EQ(5.0f + kMobShadowLift, m.y);
    // Negative coordinates (floor, not truncate — the hard-rule floorDiv
    // convention for the render's ground probe).
    const Vec3 n = blobShadowCenter({-0.5f, -2.3f, 0.25f});
    EXPECT_FLOAT_EQ(-3.0f + kMobShadowLift, n.y);
}

TEST(BlobShadowTest, SideScalesWithWidth) {
    // Pinned (v3.12.0): the quad's side = 1.0 x the entity's model
    // width (the body's own footprint — the 1.5x scale's edge
    // overhang read as shadow-support at cliff edges, the user field
    // report; the physics stays the AABB's center-of-mass rule).
    EXPECT_FLOAT_EQ(0.6f, blobShadowSide(0.6f)); // a mob (width 0.6)
    EXPECT_FLOAT_EQ(1.0f, blobShadowSide(1.0f)); // the player (width 1.0)
    EXPECT_FLOAT_EQ(0.5f, blobShadowSide(0.5f));
    EXPECT_FLOAT_EQ(blobShadowSide(0.6f) * 2.0f,
                    blobShadowSide(1.2f)); // linear in width
}

TEST(BlobShadowTest, ConstantsPinned) {
    EXPECT_FLOAT_EQ(1.0f, kMobShadowSideScale);
    EXPECT_FLOAT_EQ(0.01f, kMobShadowLift);
    EXPECT_FLOAT_EQ(0.3f, kMobShadowAlpha);
}
