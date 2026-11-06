// V3-10a (v3.23.0): the end portal's pure function family
// (core/end_portal — the 5x5 body-level ring, the data predicates
// end_frame / end_frame_eyed / end_portal, the activation plan, and the
// entry's spawn platform plan + landing point/yaw). The nether portal's
// second consumption (the scanPortalFrames pattern): the decision lives
// here (pure over the World + BlockRegistry, unit-tested headless), the
// game's interaction / travel are thin adapters.
#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <set>
#include <string>
#include <vector>

#include "block/block_registry.hpp"
#include "core/end_portal.hpp"
#include "core/json.hpp"
#include "world/end_gen.hpp" // kEndIslandTop (the platform level's oracle)
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

// The end portal fixture registry: the frame (un-eyed), the eyed frame,
// the portal, the stone (the mixed-center blocker) and the endstone
// (the non-air content that is NOT a portal — the centerAir's witness).
struct EndPortalFixture {
    BlockRegistry reg;
    BlockID frame;
    BlockID frameEyed;
    BlockID portal;
    BlockID stone;
    BlockID endstone;

    EndPortalFixture() {
        const std::string frameDef =
            "{\"id\":\"end_portal_frame\",\"properties\":{\"end_frame\":true},"
            "\"model\":{\"textures\":{\"top\":\"f\",\"side\":\"f\","
            "\"bottom\":\"f\"}}}";
        const std::string frameEyedDef =
            "{\"id\":\"end_portal_frame_eyed\",\"properties\":{\"end_frame\":"
            "true,\"end_frame_eyed\":true},"
            "\"model\":{\"textures\":{\"top\":\"fe\",\"side\":\"fe\","
            "\"bottom\":\"fe\"}}}";
        const std::string portalDef =
            "{\"id\":\"end_portal\",\"properties\":{\"end_portal\":true},"
            "\"model\":{\"textures\":{\"top\":\"p\",\"side\":\"p\","
            "\"bottom\":\"p\"}}}";
        const std::string stoneDef =
            "{\"id\":\"stone\",\"properties\":{},"
            "\"model\":{\"textures\":{\"top\":\"s\",\"side\":\"s\","
            "\"bottom\":\"s\"}}}";
        const std::string endstoneDef =
            "{\"id\":\"endstone\",\"properties\":{},"
            "\"model\":{\"textures\":{\"top\":\"e\",\"side\":\"e\","
            "\"bottom\":\"e\"}}}";
        for (const std::string& def :
             {frameDef, frameEyedDef, portalDef, stoneDef, endstoneDef}) {
            auto r = reg.registerBlock(parseDef(def));
            if (!r.ok()) {
                throw std::runtime_error(r.error().message);
            }
        }
        frame = reg.idForName("end_portal_frame");
        frameEyed = reg.idForName("end_portal_frame_eyed");
        portal = reg.idForName("end_portal");
        stone = reg.idForName("stone");
        endstone = reg.idForName("endstone");
    }
};

// Builds the 5x5 ring centered on (cx, level, cz): `eyedCount` ring
// cells hold the EYED frame (deterministic ring order), the remaining
// ring cells hold the plain frame, `portalCenter` cells (the center's
// deterministic order) hold end_portal, the rest of the center holds
// `centerFill` (air = the activatable center).
void buildRing(EndPortalFixture& f, World& world, int cx, int level,
               int cz, int eyedCount, int portalCenter, BlockID centerFill) {
    int i = 0;
    for (const BlockPos& c : endPortalRingCells(cx, level, cz)) {
        world.setBlock(c.x, c.y, c.z,
                       (i < eyedCount) ? f.frameEyed : f.frame);
        ++i;
    }
    i = 0;
    for (const BlockPos& c : endPortalCenterCells(cx, level, cz)) {
        world.setBlock(c.x, c.y, c.z,
                       (i < portalCenter) ? f.portal : centerFill);
        ++i;
    }
}

} // namespace

// ---------------------------------------------------------------------------
// The ring / center cell families (the deterministic geometry).
// ---------------------------------------------------------------------------

TEST(EndPortalTest, RingCellsAreTheTwelve) {
    const std::vector<BlockPos> ring = endPortalRingCells(10, 53, 20);
    ASSERT_EQ(ring.size(), 12u);
    // The four corners are ABSENT (the 5x5 ring is the 12 edge cells).
    const std::set<std::string> corners = {
        "8/53/18", "12/53/18", "8/53/22", "12/53/22"};
    for (const BlockPos& c : ring) {
        const std::string key = std::to_string(c.x) + "/" +
                                std::to_string(c.y) + "/" +
                                std::to_string(c.z);
        EXPECT_FALSE(corners.count(key)) << key;
        EXPECT_EQ(c.y, 53);
        EXPECT_TRUE((std::abs(c.x - 10) == 2) || (std::abs(c.z - 20) == 2));
    }
    // The deterministic (dz, dx) order: the first cell is (dz=-2, dx=-1)
    // = (9, 53, 18); the last is (dz=+2, dx=+1) = (11, 53, 22).
    EXPECT_EQ(ring.front().x, 9);
    EXPECT_EQ(ring.front().z, 18);
    EXPECT_EQ(ring.back().x, 11);
    EXPECT_EQ(ring.back().z, 22);
}

TEST(EndPortalTest, CenterCellsAreTheNine) {
    const std::vector<BlockPos> center = endPortalCenterCells(10, 53, 20);
    ASSERT_EQ(center.size(), 9u);
    for (const BlockPos& c : center) {
        EXPECT_TRUE(std::abs(c.x - 10) <= 1 && std::abs(c.z - 20) <= 1);
        EXPECT_EQ(c.y, 53);
    }
    EXPECT_EQ(center.front().x, 9);
    EXPECT_EQ(center.front().z, 19);
}

// ---------------------------------------------------------------------------
// The ring scan (the pure predicate over the world).
// ---------------------------------------------------------------------------

TEST(EndPortalTest, ScanAllEyedCenterAirActivatable) {
    EndPortalFixture f;
    World world;
    buildRing(f, world, 0, 53, 0, 12, 0, BlockID::air());
    const EndPortalScan s =
        scanEndPortalRing(world, f.reg, 0, 53, 0);
    EXPECT_TRUE(s.ringComplete);
    EXPECT_EQ(s.eyedCount, 12);
    EXPECT_TRUE(s.centerAir);
    EXPECT_FALSE(s.centerPortal);
}

TEST(EndPortalTest, ScanPartialEyed) {
    // 7 of 12 ring cells eyed: the ring is COMPLETE (all frames) but
    // NOT activatable (the eyed count reports the shortfall).
    EndPortalFixture f;
    World world;
    buildRing(f, world, 0, 53, 0, 7, 0, BlockID::air());
    const EndPortalScan s = scanEndPortalRing(world, f.reg, 0, 53, 0);
    EXPECT_TRUE(s.ringComplete);
    EXPECT_EQ(s.eyedCount, 7);
    EXPECT_TRUE(s.centerAir);
}

TEST(EndPortalTest, ScanIncompleteRing) {
    // One ring cell is STONE (not a frame): ringComplete false (the
    // eyed count is meaningless — the scan short-circuits).
    EndPortalFixture f;
    World world;
    buildRing(f, world, 0, 53, 0, 12, 0, BlockID::air());
    const BlockPos hole = endPortalRingCells(0, 53, 0).front();
    world.setBlock(hole.x, hole.y, hole.z, f.stone);
    const EndPortalScan s = scanEndPortalRing(world, f.reg, 0, 53, 0);
    EXPECT_FALSE(s.ringComplete);
}

TEST(EndPortalTest, ScanMixedCenterIsInvalid) {
    // All-eyed ring + a center that is neither all-air nor all-portal
    // (5 portals + 4 endstones): an invalid (mixed) center — the
    // activation is refused until the center is cleared (the frame is
    // unbreakable, so the mix can only be a data error).
    EndPortalFixture f;
    World world;
    buildRing(f, world, 0, 53, 0, 12, 5, f.endstone);
    const EndPortalScan s = scanEndPortalRing(world, f.reg, 0, 53, 0);
    EXPECT_TRUE(s.ringComplete);
    EXPECT_EQ(s.eyedCount, 12);
    EXPECT_FALSE(s.centerAir);
    EXPECT_FALSE(s.centerPortal);
}

TEST(EndPortalTest, ScanActiveCenter) {
    // The active portal read (the worldgen's exit): all-eyed ring + the
    // 3x3 all end_portal.
    EndPortalFixture f;
    World world;
    buildRing(f, world, 0, 53, 0, 12, 9, BlockID::air());
    const EndPortalScan s = scanEndPortalRing(world, f.reg, 0, 53, 0);
    EXPECT_TRUE(s.ringComplete);
    EXPECT_EQ(s.eyedCount, 12);
    EXPECT_FALSE(s.centerAir);
    EXPECT_TRUE(s.centerPortal);
}

TEST(EndPortalTest, ScanOffCenterSeesNothing) {
    // The same world scanned at a shifted center: the ring is
    // incomplete (the geometry is the center's, not the block's).
    EndPortalFixture f;
    World world;
    buildRing(f, world, 0, 53, 0, 12, 9, BlockID::air());
    const EndPortalScan s = scanEndPortalRing(world, f.reg, 1, 53, 1);
    EXPECT_FALSE(s.ringComplete);
}

// ---------------------------------------------------------------------------
// The activation plan + the entry platform / landing (the pure write
// plans the game applies).
// ---------------------------------------------------------------------------

TEST(EndPortalTest, ActivationPlanIsTheCenter) {
    const std::vector<BlockPos> plan = endPortalActivationPlan(3, 53, -4);
    ASSERT_EQ(plan.size(), 9u);
    EXPECT_EQ(plan, endPortalCenterCells(3, 53, -4));
}

TEST(EndPortalTest, EntryPlatformPlanPinned) {
    const EndEntryPlatformPlan p = endEntryPlatformPlan();
    // The 5x5 obsidian square at the island's top layer (52 — the feet
    // land on its top, 53), centered on (40, 0): 25 cells.
    ASSERT_EQ(p.setObsidian.size(), 25u);
    for (const BlockPos& c : p.setObsidian) {
        EXPECT_EQ(c.y, kEndEntryPlatformLevel);
        EXPECT_EQ(c.y, kEndIslandTop); // the platform sits ON the island
        EXPECT_TRUE(std::abs(c.x - kEndEntryX) <= 2 &&
                    std::abs(c.z - kEndEntryZ) <= 2);
    }
    // The clear-above: 25 columns x 3 layers = 75 cells (53..55).
    ASSERT_EQ(p.clearAir.size(), 75u);
    for (const BlockPos& c : p.clearAir) {
        EXPECT_TRUE(c.y > kEndEntryPlatformLevel &&
                    c.y <= kEndEntryPlatformLevel + kEndEntryPlatformClearAbove);
        EXPECT_TRUE(std::abs(c.x - kEndEntryX) <= 2 &&
                    std::abs(c.z - kEndEntryZ) <= 2);
    }
    // The landing point: the platform's top center, the feet; the yaw:
    // west (+pi/2, the camera convention's quarter turn left of -Z).
    const Vec3 pos = endEntryPosition();
    EXPECT_FLOAT_EQ(pos.x, static_cast<float>(kEndEntryX) + 0.5f);
    EXPECT_FLOAT_EQ(pos.y, static_cast<float>(kEndEntryPlatformLevel) + 1.0f);
    EXPECT_FLOAT_EQ(pos.z, static_cast<float>(kEndEntryZ) + 0.5f);
    EXPECT_FLOAT_EQ(endEntryYaw(),
                    static_cast<float>(std::numbers::pi) / 2.0f);
}

TEST(EndPortalTest, EntryPlatformConstantsPinned) {
    EXPECT_EQ(kEndEntryPlatformLevel, 52);
    EXPECT_EQ(kEndEntryX, 40);
    EXPECT_EQ(kEndEntryZ, 0);
    EXPECT_EQ(kEndEntryPlatformClearAbove, 3);
}
