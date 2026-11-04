// V3-1 Phase A (v3.2.0): the nether portal frame — the pure function
// family (core/portal): the bounded frame scan, the findFrameAt /
// portalValidFrame selections, the transition decision + plans, the
// standing-countdown step, the 8:1 mapping and the ambient roll. The
// fixture registers an obsidian (the frame material), a fire, a
// nether_portal (state_count 2 — the axis state) and a stone (the
// interior blocker) — the fire_spread registry pattern.
#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

#include "block/block_registry.hpp"
#include "core/json.hpp"
#include "core/portal.hpp"
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

// The portal fixture registry: obsidian (the frame material), fire (the
// igniter), nether_portal (the fill block, state_count 2 — the axis) and
// stone (the interior blocker, the non-air/fire content).
BlockRegistry portalRegistry() {
    BlockRegistry reg;
    const std::string obsidian =
        "{\"id\":\"obsidian\",\"properties\":{\"hardness\":10.0,"
        "\"drops\":\"obsidian\"},"
        "\"model\":{\"textures\":{\"top\":\"obsidian\",\"side\":\"obsidian\","
        "\"bottom\":\"obsidian\"}}}";
    const std::string fire =
        "{\"id\":\"fire\",\"properties\":{\"hardness\":0.0,"
        "\"solid\":false,\"opaque\":false,\"transparent\":true,"
        "\"light_emission\":15},"
        "\"model\":{\"shape\":\"cross\",\"textures\":{\"cross\":\"fire\"}}}";
    const std::string portal =
        "{\"id\":\"nether_portal\",\"properties\":{\"hardness\":0.0,"
        "\"solid\":false,\"opaque\":false,\"transparent\":true,"
        "\"destroyable\":false,\"light_emission\":11},"
        "\"model\":{\"shape\":\"full_cube\",\"state_count\":2,"
        "\"state_default\":0,\"textures\":{\"top\":\"nether_portal\","
        "\"side\":\"nether_portal\",\"bottom\":\"nether_portal\"}}}";
    const std::string stone =
        "{\"id\":\"stone\",\"properties\":{\"hardness\":1.5,"
        "\"drops\":\"cobblestone\"},"
        "\"model\":{\"textures\":{\"top\":\"stone\",\"side\":\"stone\","
        "\"bottom\":\"stone\"}}}";
    for (const std::string& def : {obsidian, fire, portal, stone}) {
        auto r = reg.registerBlock(parseDef(def));
        if (!r.ok()) {
            throw std::runtime_error(r.error().message);
        }
    }
    return reg;
}

// A world with the single origin chunk (x, y, z in [0, 16)).
World singleChunkWorld() {
    World world;
    world.getOrCreateChunk({0, 0, 0});
    return world;
}

// A world with a 32 x 32 x 16 volume (x, y in [0, 32), z in [0, 16)) —
// the 24x24 / 23x23 maximum-size frames' space.
World bigWorld() {
    World world;
    for (int cy = 0; cy < 2; ++cy) {
        for (int cx = 0; cx < 2; ++cx) {
            world.getOrCreateChunk({cx, cy, 0});
        }
    }
    return world;
}

// Places the obsidian RING of a frame (the corners INCLUDED unless
// listed in missingCorners): axis 0 = the long edge along X (the ring in
// the plane z = plane), outer [u0, u0 + W) x [y0, y0 + H); axis 1 = the
// long edge along Z (the plane x = plane). The interior is left as-is
// (the test fills it: air by default, fire / stone / portal per case).
void placeRing(World& world, BlockID obs, int axis, int u0, int y0, int W,
               int H, int plane,
               const std::vector<BlockPos>* missingCorners = nullptr) {
    auto put = [&](int u, int y) {
        const BlockPos p = (axis == 0) ? BlockPos {u, y, plane}
                                       : BlockPos {plane, y, u};
        if (missingCorners != nullptr) {
            for (const BlockPos& m : *missingCorners) {
                if (m == p) {
                    return; // the missing corner: any content (air here)
                }
            }
        }
        world.setBlock(p.x, p.y, p.z, obs);
    };
    for (int u = u0; u < u0 + W; ++u) {
        put(u, y0);
        put(u, y0 + H - 1);
    }
    for (int y = y0 + 1; y < y0 + H - 1; ++y) {
        put(u0, y);
        put(u0 + W - 1, y);
    }
}

// The four corners of the frame (the optional cells).
std::vector<BlockPos> ringCorners(int axis, int u0, int y0, int W, int H,
                                  int plane) {
    auto c = [&](int u, int y) {
        return (axis == 0) ? BlockPos {u, y, plane} : BlockPos {plane, y, u};
    };
    return {c(u0, y0), c(u0 + W - 1, y0), c(u0, y0 + H - 1),
            c(u0 + W - 1, y0 + H - 1)};
}

// Fills the frame's interior with `fill` (air = the default no-op).
void fillInterior(World& world, int axis, int u0, int y0, int W, int H,
                  int plane, BlockID fill) {
    for (int y = y0 + 1; y < y0 + H - 1; ++y) {
        for (int u = u0 + 1; u < u0 + W - 1; ++u) {
            const BlockPos p = (axis == 0) ? BlockPos {u, y, plane}
                                           : BlockPos {plane, y, u};
            if (fill != BlockID::air()) {
                world.setBlock(p.x, p.y, p.z, fill);
            }
        }
    }
}

// The canonical 4x5 axis-0 frame (the Minecraft minimum, corners
// included): x in [4, 7], y in [2, 6], the plane z = 8.
constexpr int kFrameU0 = 4;
constexpr int kFrameY0 = 2;
constexpr int kFrameW = 4;
constexpr int kFrameH = 5;
constexpr int kFramePlane = 8;

} // namespace

// ---------------------------------------------------------------------------
// The size rule (Minecraft's 4x5..23x23 outer extent).
// ---------------------------------------------------------------------------

TEST(PortalSizeTest, SizeRuleBounds) {
    EXPECT_TRUE(portalFrameSizeValid({0, 4, 7, 2, 6, 8}));   // 4x5 minimum
    EXPECT_TRUE(portalFrameSizeValid({0, 0, 22, 0, 22, 8})); // 23x23 maximum
    EXPECT_TRUE(portalFrameSizeValid({1, 0, 22, 0, 22, 4})); // 23x23 axis 1
    EXPECT_TRUE(portalFrameSizeValid({0, 4, 8, 2, 6, 8}));   // 5x5 square
    // Below the minimum: the 3x4 (both edges short) and the 4x4 (the
    // height needs 5 — the long edge alone at 4 does not carry the frame).
    EXPECT_FALSE(portalFrameSizeValid({0, 4, 6, 2, 5, 8}));  // 3x4
    EXPECT_FALSE(portalFrameSizeValid({0, 4, 7, 2, 5, 8}));  // 4x4
    // Above the maximum: one edge over is enough (24x23 / 23x24 / 24x24).
    EXPECT_FALSE(portalFrameSizeValid({0, 0, 23, 0, 22, 8}));  // 24x23
    EXPECT_FALSE(portalFrameSizeValid({0, 0, 22, 0, 23, 8}));  // 23x24
    EXPECT_FALSE(portalFrameSizeValid({0, 0, 23, 0, 23, 8}));  // 24x24
}

// ---------------------------------------------------------------------------
// The frame geometry (the ring / interior cell lists + the predicates).
// ---------------------------------------------------------------------------

TEST(PortalGeometryTest, RingAndInteriorCounts) {
    const PortalFrame f {0, 4, 7, 2, 6, 8}; // the 4x5 axis-0 frame
    EXPECT_EQ(f.longEdge(), 4);
    EXPECT_EQ(f.height(), 5);
    const std::vector<BlockPos> ring = portalFrameRing(f);
    const std::vector<BlockPos> interior = portalFrameInterior(f);
    // The perimeter: 2*4 + 2*5 - 4 = 14 (the 10 non-corners + 4 corners).
    EXPECT_EQ(ring.size(), 14u);
    // The interior: (4-2) * (5-2) = 6.
    EXPECT_EQ(interior.size(), 6u);
    const std::vector<BlockPos> corners =
        ringCorners(0, kFrameU0, kFrameY0, kFrameW, kFrameH, kFramePlane);
    for (const BlockPos& c : corners) {
        EXPECT_TRUE(portalFrameIsCorner(f, c)) << c.x << "," << c.y << "," << c.z;
        EXPECT_TRUE(portalFrameOnRing(f, c));
        EXPECT_FALSE(portalFrameStrictlyInterior(f, c));
    }
    for (const BlockPos& p : interior) {
        EXPECT_FALSE(portalFrameIsCorner(f, p));
        EXPECT_FALSE(portalFrameOnRing(f, p));
        EXPECT_TRUE(portalFrameStrictlyInterior(f, p));
    }
    // The axis-1 twin: the same extents, the long edge along Z (the plane
    // x = 8). The cell mapping swaps the plane coordinate.
    const PortalFrame fz {1, 4, 7, 2, 6, 8};
    const BlockPos fzCell {8, 3, 5};
    const BlockPos fCell {5, 3, 8};
    EXPECT_EQ(portalFrameCell(fz, 5, 3), fzCell);
    EXPECT_EQ(portalFrameCell(f, 5, 3), fCell);
    EXPECT_EQ(portalFrameInterior(fz).size(), 6u);
    // A cell off the plane is neither ring, corner nor interior.
    EXPECT_FALSE(portalFrameOnRing(f, BlockPos {5, 3, 9}));
    EXPECT_FALSE(portalFrameIsCorner(f, BlockPos {5, 3, 9}));
    EXPECT_FALSE(portalFrameStrictlyInterior(f, BlockPos {5, 3, 9}));
}

// ---------------------------------------------------------------------------
// findFrameAt (the largest complete strict frame containing the cell).
// ---------------------------------------------------------------------------

TEST(PortalFindTest, Finds4x5WithCorners) {
    const BlockRegistry reg = portalRegistry();
    const BlockID obs = reg.idForName("obsidian");
    const BlockID fire = reg.idForName("fire");
    World world = singleChunkWorld();
    placeRing(world, obs, 0, kFrameU0, kFrameY0, kFrameW, kFrameH, kFramePlane);
    // From a ring cell (the obsidian placement's event cell)...
    auto f = findFrameAt(world, {5, 2, 8}, obs, fire);
    ASSERT_TRUE(f.has_value());
    EXPECT_EQ(f->axis, 0);
    EXPECT_EQ(f->lo, 4);
    EXPECT_EQ(f->hi, 7);
    EXPECT_EQ(f->y0, 2);
    EXPECT_EQ(f->y1, 6);
    EXPECT_EQ(f->plane, 8);
    // ...and from an interior air cell (the fire placement's event cell).
    f = findFrameAt(world, {5, 4, 8}, obs, fire);
    ASSERT_TRUE(f.has_value());
    const PortalFrame want45 {0, 4, 7, 2, 6, 8};
    EXPECT_EQ(*f, want45);
}

TEST(PortalFindTest, MissingTwoCornersStillComplete) {
    // The corners are OPTIONAL (the player may save four obsidian): two
    // missing corners keep the frame complete (the 4x5 activation case).
    const BlockRegistry reg = portalRegistry();
    const BlockID obs = reg.idForName("obsidian");
    const BlockID fire = reg.idForName("fire");
    World world = singleChunkWorld();
    const std::vector<BlockPos> missing = {
        ringCorners(0, kFrameU0, kFrameY0, kFrameW, kFrameH, kFramePlane)[0],
        ringCorners(0, kFrameU0, kFrameY0, kFrameW, kFrameH, kFramePlane)[3]};
    placeRing(world, obs, 0, kFrameU0, kFrameY0, kFrameW, kFrameH, kFramePlane,
              &missing);
    EXPECT_TRUE(
        findFrameAt(world, {5, 2, 8}, obs, fire).has_value());
    // All FOUR corners missing is still a frame (Minecraft's minimum
    // four-obsidian saving).
    World world2 = singleChunkWorld();
    const std::vector<BlockPos> allFour =
        ringCorners(0, kFrameU0, kFrameY0, kFrameW, kFrameH, kFramePlane);
    placeRing(world2, obs, 0, kFrameU0, kFrameY0, kFrameW, kFrameH,
              kFramePlane, &allFour);
    EXPECT_TRUE(
        findFrameAt(world2, {5, 4, 8}, obs, fire).has_value());
}

TEST(PortalFindTest, AxisZFrame) {
    // The long edge along Z (the ring in the plane x = 8): the portal
    // blocks' state 1.
    const BlockRegistry reg = portalRegistry();
    const BlockID obs = reg.idForName("obsidian");
    const BlockID fire = reg.idForName("fire");
    World world = singleChunkWorld();
    placeRing(world, obs, 1, 4, 2, 4, 5, 8);
    auto f = findFrameAt(world, {8, 4, 5}, obs, fire); // an interior cell
    ASSERT_TRUE(f.has_value());
    EXPECT_EQ(f->axis, 1);
    EXPECT_EQ(f->plane, 8);
    EXPECT_EQ(f->lo, 4);
    EXPECT_EQ(f->hi, 7);
    EXPECT_EQ(f->y0, 2);
    EXPECT_EQ(f->y1, 6);
}

TEST(PortalFindTest, RejectsBelowMinimum) {
    const BlockRegistry reg = portalRegistry();
    const BlockID obs = reg.idForName("obsidian");
    const BlockID fire = reg.idForName("fire");
    // The 3x4: both edges below the floor (no valid frame contains its
    // ring cells — a sub-rectangle's ring crosses the 3x4's air interior).
    World world = singleChunkWorld();
    placeRing(world, obs, 0, 4, 2, 3, 4, 8);
    EXPECT_FALSE(findFrameAt(world, {5, 2, 8}, obs, fire).has_value());
    EXPECT_FALSE(findFrameAt(world, {5, 3, 8}, obs, fire).has_value());
    // The 4x4: the height at the long-edge minimum (the frame needs 5).
    World world2 = singleChunkWorld();
    placeRing(world2, obs, 0, 4, 2, 4, 4, 8);
    EXPECT_FALSE(findFrameAt(world2, {5, 2, 8}, obs, fire).has_value());
}

TEST(PortalFindTest, RejectsAboveMaximum) {
    const BlockRegistry reg = portalRegistry();
    const BlockID obs = reg.idForName("obsidian");
    const BlockID fire = reg.idForName("fire");
    // The 24x24: one edge over the maximum (no sub-frame's ring is
    // complete — every candidate's perimeter crosses the 24x24's air
    // interior). The 32x32 world holds it.
    World world = bigWorld();
    placeRing(world, obs, 0, 0, 0, 24, 24, 8);
    EXPECT_FALSE(findFrameAt(world, {12, 0, 8}, obs, fire).has_value());
    EXPECT_FALSE(findFrameAt(world, {0, 12, 8}, obs, fire).has_value());
    EXPECT_FALSE(findFrameAt(world, {12, 12, 8}, obs, fire).has_value());
    // The 24x23 (the long edge over, the height legal) is rejected too.
    World world2 = bigWorld();
    placeRing(world2, obs, 0, 0, 0, 24, 23, 8);
    EXPECT_FALSE(findFrameAt(world2, {12, 0, 8}, obs, fire).has_value());
    // And the 23x24 (the height over).
    World world3 = bigWorld();
    placeRing(world3, obs, 0, 0, 0, 23, 24, 8);
    EXPECT_FALSE(findFrameAt(world3, {0, 12, 8}, obs, fire).has_value());
}

TEST(PortalFindTest, RejectsHorizontalFrame) {
    // A "frame" lying in a HORIZONTAL plane (the End-portal shape): the
    // portal ring is vertical-only — the 4x4 square at y = 3 (x in
    // [4, 7], z in [8, 11]) owns no vertical frame.
    const BlockRegistry reg = portalRegistry();
    const BlockID obs = reg.idForName("obsidian");
    const BlockID fire = reg.idForName("fire");
    World world = singleChunkWorld();
    for (int x = 4; x <= 7; ++x) {
        for (int z = 8; z <= 11; ++z) {
            if (x == 4 || x == 7 || z == 8 || z == 11) {
                world.setBlock(x, 3, z, obs);
            }
        }
    }
    EXPECT_FALSE(findFrameAt(world, {5, 3, 9}, obs, fire).has_value());
    EXPECT_FALSE(findFrameAt(world, {4, 3, 8}, obs, fire).has_value());
}

TEST(PortalFindTest, RejectsInteriorBlocker) {
    // A non-air/fire block inside the ring (the stone): the frame is not
    // a frame (the interior must be all air / fire).
    const BlockRegistry reg = portalRegistry();
    const BlockID obs = reg.idForName("obsidian");
    const BlockID fire = reg.idForName("fire");
    const BlockID stone = reg.idForName("stone");
    World world = singleChunkWorld();
    placeRing(world, obs, 0, kFrameU0, kFrameY0, kFrameW, kFrameH, kFramePlane);
    world.setBlock(5, 4, 8, stone); // the interior's center-ish cell
    EXPECT_FALSE(findFrameAt(world, {5, 2, 8}, obs, fire).has_value());
    EXPECT_FALSE(findFrameAt(world, {5, 4, 8}, obs, fire).has_value());
    // The fire is the other legal interior content: an all-fire interior
    // is a frame (the activation's full-interior-fire case).
    World world2 = singleChunkWorld();
    placeRing(world2, obs, 0, kFrameU0, kFrameY0, kFrameW, kFrameH, kFramePlane);
    fillInterior(world2, 0, kFrameU0, kFrameY0, kFrameW, kFrameH, kFramePlane,
                 fire);
    EXPECT_TRUE(
        findFrameAt(world2, {5, 4, 8}, obs, fire).has_value());
}

TEST(PortalFindTest, AdjacentSharedObsidianFrames) {
    // Two 4x5 frames sharing the column x = 7 (A: x in [4, 7], B: x in
    // [7, 10]): the shared column's obsidian serves both rings. From the
    // shared column's cell BOTH frames are candidates (the scan lists
    // them, deterministic order); from A's interior only A is (B's box
    // does not contain it).
    const BlockRegistry reg = portalRegistry();
    const BlockID obs = reg.idForName("obsidian");
    const BlockID fire = reg.idForName("fire");
    World world = singleChunkWorld();
    placeRing(world, obs, 0, 4, 2, 4, 5, 8);  // A
    placeRing(world, obs, 0, 7, 2, 4, 5, 8);  // B (the shared x = 7)
    const std::vector<PortalFrameStatus> shared =
        scanPortalFrames(world, {7, 4, 8}, obs, fire,
                         reg.idForName("nether_portal"));
    ASSERT_EQ(shared.size(), 2u);
    const PortalFrame wantA {0, 4, 7, 2, 6, 8};
    const PortalFrame wantB {0, 7, 10, 2, 6, 8};
    EXPECT_EQ(shared[0].frame, wantA);
    EXPECT_EQ(shared[1].frame, wantB);
    EXPECT_TRUE(shared[0].ringComplete);
    EXPECT_TRUE(shared[1].ringComplete);
    // From A's interior air: only A (the bounding-box containment).
    auto f = findFrameAt(world, {5, 4, 8}, obs, fire);
    ASSERT_TRUE(f.has_value());
    EXPECT_EQ(f->lo, 4);
    EXPECT_EQ(f->hi, 7);
}

TEST(PortalFindTest, ScanBoundedMaxReach) {
    // The scan domain's root pin: the 23x23 maximum frame is found from
    // its FAR CORNER (the event cell 22 away from the opposite edge —
    // the ±22 bounded window's exact reach; the design's 25x25/625-cell
    // estimate is insufficient for large frames, the documented
    // deviation). The 32x32 world holds x, y in [0, 22].
    const BlockRegistry reg = portalRegistry();
    const BlockID obs = reg.idForName("obsidian");
    const BlockID fire = reg.idForName("fire");
    World world = bigWorld();
    placeRing(world, obs, 0, 0, 0, 23, 23, 8);
    auto f = findFrameAt(world, {0, 0, 8}, obs, fire); // the corner
    ASSERT_TRUE(f.has_value());
    EXPECT_EQ(f->lo, 0);
    EXPECT_EQ(f->hi, 22);
    EXPECT_EQ(f->y0, 0);
    EXPECT_EQ(f->y1, 22);
    // ...and from its center (the interior cell, the fire-placement case).
    f = findFrameAt(world, {11, 11, 8}, obs, fire);
    ASSERT_TRUE(f.has_value());
    const PortalFrame want2323 {0, 0, 22, 0, 22, 8};
    EXPECT_EQ(*f, want2323);
}

TEST(PortalFindTest, NoFrameIsNoFrame) {
    // A bare obsidian wall (no ring): the scan finds nothing.
    const BlockRegistry reg = portalRegistry();
    const BlockID obs = reg.idForName("obsidian");
    const BlockID fire = reg.idForName("fire");
    World world = singleChunkWorld();
    for (int y = 2; y <= 6; ++y) {
        world.setBlock(4, y, 8, obs);
    }
    EXPECT_FALSE(findFrameAt(world, {4, 4, 8}, obs, fire).has_value());
    EXPECT_TRUE(scanPortalFrames(world, {4, 4, 8}, obs, fire,
                                 reg.idForName("nether_portal")).empty());
}

// ---------------------------------------------------------------------------
// The transition decision + plans (the choke-point logic, pure).
// ---------------------------------------------------------------------------

TEST(PortalTransitionTest, ActivationPlanConvertsAirAndFire) {
    // A complete frame, 5 air + 1 fire interior: the fire placement's
    // event decides activation, and the plan covers EVERY interior cell
    // (the air AND the fire — "one of them replacing the fire").
    const BlockRegistry reg = portalRegistry();
    const BlockID obs = reg.idForName("obsidian");
    const BlockID fire = reg.idForName("fire");
    const BlockID portal = reg.idForName("nether_portal");
    World world = singleChunkWorld();
    placeRing(world, obs, 0, kFrameU0, kFrameY0, kFrameW, kFrameH, kFramePlane);
    world.setBlock(6, 4, 8, fire); // one interior fire
    const std::vector<PortalFrameStatus> cands =
        scanPortalFrames(world, {6, 4, 8}, obs, fire, portal);
    ASSERT_EQ(cands.size(), 1u);
    EXPECT_TRUE(cands[0].ringComplete);
    EXPECT_EQ(cands[0].interiorFire, 1);
    EXPECT_EQ(cands[0].interiorAir, 5);
    EXPECT_EQ(cands[0].interiorPortal, 0);
    const PortalTransitionDecision d =
        portalTransitionDecision(cands, fire, obs);
    ASSERT_EQ(d.activate.size(), 1u);
    EXPECT_TRUE(d.deactivate.empty());
    const std::vector<BlockPos> plan =
        portalActivationPlan(world, d.activate[0].frame, fire);
    // The full 6-cell interior (the frameInterior order: y outer, u in).
    EXPECT_EQ(plan, (std::vector<BlockPos> {
                         {5, 3, 8}, {6, 3, 8},
                         {5, 4, 8}, {6, 4, 8},
                         {5, 5, 8}, {6, 5, 8}}));
}

TEST(PortalTransitionTest, ObsidianCompletionActivates) {
    // The both-sides-checked deviation: the LAST obsidian placed on a
    // frame that already holds an interior fire activates (Minecraft's
    // "fire last" quirk is NOT reproduced — the design's ruling).
    const BlockRegistry reg = portalRegistry();
    const BlockID obs = reg.idForName("obsidian");
    const BlockID fire = reg.idForName("fire");
    const BlockID portal = reg.idForName("nether_portal");
    World world = singleChunkWorld();
    // The ring with the bottom-right corner missing (the corner is
    // optional) + the interior fire: not yet a frame (the non-corner
    // cell (6, 2, 8) is still air — wait, the corner (7, 2, 8) is the
    // optional one; build the ring complete EXCEPT the non-corner ring
    // cell (6, 2, 8) = the last obsidian).
    placeRing(world, obs, 0, kFrameU0, kFrameY0, kFrameW, kFrameH, kFramePlane);
    world.setBlock(6, 2, 8, BlockID::air()); // break the bottom edge
    world.setBlock(5, 4, 8, fire);
    // Pre-placement: the frame is NOT complete (the non-corner ring cell
    // (6, 2, 8) is air) — no activation yet.
    const auto pre = scanPortalFrames(world, {6, 2, 8}, obs, fire, portal);
    ASSERT_FALSE(pre.empty());
    EXPECT_FALSE(pre[0].ringComplete);
    EXPECT_TRUE(pre[0].ringCompletePreEvent);
    // Place the last obsidian (the event cell (6, 2, 8), oldId air).
    world.setBlock(6, 2, 8, obs);
    const std::vector<PortalFrameStatus> cands =
        scanPortalFrames(world, {6, 2, 8}, obs, fire, portal);
    ASSERT_EQ(cands.size(), 1u);
    EXPECT_TRUE(cands[0].ringComplete);
    const PortalTransitionDecision d = portalTransitionDecision(cands,
                                                                BlockID::air(),
                                                                obs);
    ASSERT_EQ(d.activate.size(), 1u);
    EXPECT_TRUE(d.deactivate.empty());
}

TEST(PortalTransitionTest, NonCornerBreakDeactivates) {
    // A live frame (the interior all portal): the obsidian destruction of
    // a NON-CORNER ring cell deactivates (the pre-event view is complete,
    // the current view is broken).
    const BlockRegistry reg = portalRegistry();
    const BlockID obs = reg.idForName("obsidian");
    const BlockID fire = reg.idForName("fire");
    const BlockID portal = reg.idForName("nether_portal");
    World world = singleChunkWorld();
    placeRing(world, obs, 0, kFrameU0, kFrameY0, kFrameW, kFrameH, kFramePlane);
    fillInterior(world, 0, kFrameU0, kFrameY0, kFrameW, kFrameH, kFramePlane,
                 portal);
    world.setBlock(5, 2, 8, BlockID::air()); // the non-corner break
    const std::vector<PortalFrameStatus> cands =
        scanPortalFrames(world, {5, 2, 8}, obs, fire, portal);
    ASSERT_EQ(cands.size(), 1u);
    EXPECT_FALSE(cands[0].ringComplete);
    EXPECT_TRUE(cands[0].ringCompletePreEvent);
    EXPECT_TRUE(cands[0].eventOnRing);
    EXPECT_FALSE(cands[0].eventIsCorner);
    EXPECT_EQ(cands[0].interiorPortal, 6);
    const PortalTransitionDecision d =
        portalTransitionDecision(cands, obs, obs);
    ASSERT_EQ(d.deactivate.size(), 1u);
    EXPECT_TRUE(d.activate.empty());
    // The plan: every interior portal cell (the 6-cell reverse domain).
    EXPECT_EQ(portalDeactivationPlan(world, d.deactivate[0].frame, portal).size(),
              6u);
}

TEST(PortalTransitionTest, CornerBreakKeepsThePortal) {
    // The four corners may be removed WITHOUT destroying the portal
    // (Minecraft-exact): a corner break leaves the ring complete (the
    // corners are unchecked) and decides NOTHING.
    const BlockRegistry reg = portalRegistry();
    const BlockID obs = reg.idForName("obsidian");
    const BlockID fire = reg.idForName("fire");
    const BlockID portal = reg.idForName("nether_portal");
    World world = singleChunkWorld();
    placeRing(world, obs, 0, kFrameU0, kFrameY0, kFrameW, kFrameH, kFramePlane);
    fillInterior(world, 0, kFrameU0, kFrameY0, kFrameW, kFrameH, kFramePlane,
                 portal);
    world.setBlock(4, 2, 8, BlockID::air()); // the (lo, y0) corner
    const std::vector<PortalFrameStatus> cands =
        scanPortalFrames(world, {4, 2, 8}, obs, fire, portal);
    ASSERT_EQ(cands.size(), 1u);
    EXPECT_TRUE(cands[0].ringComplete); // the corner is optional
    EXPECT_TRUE(cands[0].eventIsCorner);
    const PortalTransitionDecision d =
        portalTransitionDecision(cands, obs, obs);
    EXPECT_TRUE(d.deactivate.empty());
    EXPECT_TRUE(d.activate.empty()); // the interior portal blocks re-light
    // All four corners broken: still a frame, still no deactivation.
    for (const BlockPos& c :
         ringCorners(0, kFrameU0, kFrameY0, kFrameW, kFrameH, kFramePlane)) {
        world.setBlock(c.x, c.y, c.z, BlockID::air());
    }
    const auto cands2 = scanPortalFrames(world, {5, 2, 8}, obs, fire, portal);
    ASSERT_EQ(cands2.size(), 1u);
    EXPECT_TRUE(cands2[0].ringComplete);
    EXPECT_TRUE(portalTransitionDecision(cands2, obs, obs).deactivate.empty());
}

TEST(PortalTransitionTest, FireExtinguishNeverDeactivates) {
    // The portal is SELF-SUSTAINING: the interior fire's burn-out (or
    // water put-out — the oldId == fireId events) decides no deactivation
    // (the decision rule reads oldId == obsidianId only). The pin: a
    // complete frame's interior fire extinguishes -> no transition at
    // all (no fire left, no activation either).
    const BlockRegistry reg = portalRegistry();
    const BlockID obs = reg.idForName("obsidian");
    const BlockID fire = reg.idForName("fire");
    const BlockID portal = reg.idForName("nether_portal");
    World world = singleChunkWorld();
    placeRing(world, obs, 0, kFrameU0, kFrameY0, kFrameW, kFrameH, kFramePlane);
    world.setBlock(5, 4, 8, fire);
    world.setBlock(5, 4, 8, BlockID::air()); // the burn-out / put-out
    const std::vector<PortalFrameStatus> cands =
        scanPortalFrames(world, {5, 4, 8}, obs, fire, portal);
    ASSERT_EQ(cands.size(), 1u);
    EXPECT_TRUE(cands[0].ringComplete);
    EXPECT_EQ(cands[0].interiorFire, 0);
    const PortalTransitionDecision d = portalTransitionDecision(cands, fire,
                                                                obs);
    EXPECT_TRUE(d.activate.empty());
    EXPECT_TRUE(d.deactivate.empty());
}

TEST(PortalTransitionTest, IdempotentRepeatEvents) {
    // A repeated event on an already-settled frame decides nothing:
    // the broken frame re-broken (oldId air, not obsidian) and the live
    // frame re-ignited (the interior portal blocks the activation).
    const BlockRegistry reg = portalRegistry();
    const BlockID obs = reg.idForName("obsidian");
    const BlockID fire = reg.idForName("fire");
    const BlockID portal = reg.idForName("nether_portal");
    // The re-broken frame (the first break's deactivation already ran).
    World world = singleChunkWorld();
    placeRing(world, obs, 0, kFrameU0, kFrameY0, kFrameW, kFrameH, kFramePlane);
    fillInterior(world, 0, kFrameU0, kFrameY0, kFrameW, kFrameH, kFramePlane,
                 portal);
    world.setBlock(5, 2, 8, BlockID::air()); // the first break
    const auto cands1 = scanPortalFrames(world, {5, 2, 8}, obs, fire, portal);
    ASSERT_EQ(cands1.size(), 1u);
    const auto d1 = portalTransitionDecision(cands1, obs, obs);
    EXPECT_EQ(d1.deactivate.size(), 1u);
    // The deactivation applies (the interior -> air)...
    for (const BlockPos& p : portalDeactivationPlan(world, d1.deactivate[0].frame,
                                                    portal)) {
        world.setBlock(p.x, p.y, p.z, BlockID::air());
    }
    // ...and the SAME cell's repeat event (oldId air now) is a no-op.
    const auto cands2 = scanPortalFrames(world, {5, 2, 8}, obs, fire, portal);
    const auto d2 = portalTransitionDecision(cands2, BlockID::air(), obs);
    EXPECT_TRUE(d2.deactivate.empty());
    EXPECT_TRUE(d2.activate.empty());
    // The live frame's re-ignition: a fire event at an interior PORTAL
    // cell (oldId air) — the interior portal blocks the activation.
    World world2 = singleChunkWorld();
    placeRing(world2, obs, 0, kFrameU0, kFrameY0, kFrameW, kFrameH, kFramePlane);
    fillInterior(world2, 0, kFrameU0, kFrameY0, kFrameW, kFrameH, kFramePlane,
                 portal);
    const auto cands3 =
        scanPortalFrames(world2, {5, 4, 8}, obs, fire, portal);
    ASSERT_EQ(cands3.size(), 1u);
    const auto d3 = portalTransitionDecision(cands3, BlockID::air(), obs);
    EXPECT_TRUE(d3.activate.empty()); // interiorPortal != 0
    EXPECT_TRUE(d3.deactivate.empty());
}

TEST(PortalTransitionTest, FireOnRingCellIsObsidianBreak) {
    // Replacing a ring cell with fire (the oldId obsidian, the newId
    // fire event) breaks the frame like a break: the deactivation fires
    // (a live frame), and the fire-on-ring is NOT an interior igniter
    // (the activation needs the ring complete — the event cell's fire
    // breaks it).
    const BlockRegistry reg = portalRegistry();
    const BlockID obs = reg.idForName("obsidian");
    const BlockID fire = reg.idForName("fire");
    const BlockID portal = reg.idForName("nether_portal");
    World world = singleChunkWorld();
    placeRing(world, obs, 0, kFrameU0, kFrameY0, kFrameW, kFrameH, kFramePlane);
    fillInterior(world, 0, kFrameU0, kFrameY0, kFrameW, kFrameH, kFramePlane,
                 portal);
    world.setBlock(6, 6, 8, fire); // the top edge's non-corner cell
    const std::vector<PortalFrameStatus> cands =
        scanPortalFrames(world, {6, 6, 8}, obs, fire, portal);
    ASSERT_EQ(cands.size(), 1u);
    EXPECT_FALSE(cands[0].ringComplete);
    EXPECT_TRUE(cands[0].ringCompletePreEvent);
    const PortalTransitionDecision d = portalTransitionDecision(cands, obs,
                                                                obs);
    ASSERT_EQ(d.deactivate.size(), 1u);
    EXPECT_TRUE(d.activate.empty());
}

// ---------------------------------------------------------------------------
// The load-time rescan's pure predicate (the portalValidFrame twin).
// ---------------------------------------------------------------------------

TEST(PortalRescanTest, OrphanPortalCellIsInvalid) {
    // A lone portal block (no frame at all): the rescan's air-fix target.
    const BlockRegistry reg = portalRegistry();
    const BlockID obs = reg.idForName("obsidian");
    const BlockID fire = reg.idForName("fire");
    const BlockID portal = reg.idForName("nether_portal");
    World world = singleChunkWorld();
    world.setBlock(8, 8, 8, portal);
    EXPECT_FALSE(portalValidFrame(world, {8, 8, 8}, obs, fire, portal)
                     .has_value());
    // A portal cell that sits ON the ring (not the interior): hand-
    // edited, the rescan's air-fix too (the interior is the portal's
    // living space).
    World world2 = singleChunkWorld();
    placeRing(world2, obs, 0, kFrameU0, kFrameY0, kFrameW, kFrameH, kFramePlane);
    world2.setBlock(5, 2, 8, portal); // the bottom edge (a ring cell)
    EXPECT_FALSE(portalValidFrame(world2, {5, 2, 8}, obs, fire, portal)
                     .has_value());
}

TEST(PortalRescanTest, LiveFramePortalCellSurvives) {
    // A complete frame's interior portal: the rescan keeps it (the frame
    // + the interior {air, fire, portal} predicate).
    const BlockRegistry reg = portalRegistry();
    const BlockID obs = reg.idForName("obsidian");
    const BlockID fire = reg.idForName("fire");
    const BlockID portal = reg.idForName("nether_portal");
    World world = singleChunkWorld();
    placeRing(world, obs, 0, kFrameU0, kFrameY0, kFrameW, kFrameH, kFramePlane);
    fillInterior(world, 0, kFrameU0, kFrameY0, kFrameW, kFrameH, kFramePlane,
                 portal);
    auto f = portalValidFrame(world, {5, 4, 8}, obs, fire, portal);
    ASSERT_TRUE(f.has_value());
    const PortalFrame want45 {0, 4, 7, 2, 6, 8};
    EXPECT_EQ(*f, want45);
    // Every interior cell validates (not just the queried one).
    for (const BlockPos& p : portalFrameInterior(*f)) {
        EXPECT_TRUE(portalValidFrame(world, p, obs, fire, portal).has_value())
            << p.x << "," << p.y << "," << p.z;
    }
}

TEST(PortalRescanTest, BrokenFramePortalsAreOrphans) {
    // A live frame whose NON-CORNER ring cell was destroyed (the save
    // holds the portals, not the fix — the hand-edited / the world
    // outlived the frame): every portal cell is an orphan now.
    const BlockRegistry reg = portalRegistry();
    const BlockID obs = reg.idForName("obsidian");
    const BlockID fire = reg.idForName("fire");
    const BlockID portal = reg.idForName("nether_portal");
    World world = singleChunkWorld();
    placeRing(world, obs, 0, kFrameU0, kFrameY0, kFrameW, kFrameH, kFramePlane);
    fillInterior(world, 0, kFrameU0, kFrameY0, kFrameW, kFrameH, kFramePlane,
                 portal);
    world.setBlock(5, 2, 8, BlockID::air()); // the non-corner break
    EXPECT_FALSE(portalValidFrame(world, {5, 4, 8}, obs, fire, portal)
                     .has_value());
}

// ---------------------------------------------------------------------------
// The standing countdown step (the frame-locked pure logic, hard rule 8).
// ---------------------------------------------------------------------------

TEST(PortalEntryStepTest, ArmsOnFirstInPortalFrame) {
    // The arm frame is the FIRST of the 60 (the countdown includes it):
    // the step leaves kPortalEntryFrames - 1 remaining.
    const PortalEntryStep s = portalEntryStep(true, 0);
    EXPECT_EQ(s.framesRemaining, kPortalEntryFrames - 1);
    EXPECT_FALSE(s.completed);
    EXPECT_EQ(kPortalEntryFrames, 60); // the named constant's value pin
}

TEST(PortalEntryStepTest, CompletesOnTheSixtiethConsecutiveFrame) {
    int armed = 0;
    bool completed = false;
    int completedOn = -1;
    for (int i = 1; i <= 60; ++i) {
        const PortalEntryStep s = portalEntryStep(true, armed);
        armed = s.framesRemaining;
        if (s.completed) {
            completed = true;
            completedOn = i;
        }
        if (i < 60) {
            EXPECT_FALSE(s.completed) << "frame " << i;
        }
    }
    EXPECT_TRUE(completed);
    EXPECT_EQ(completedOn, 60);
    EXPECT_EQ(armed, 0);
    // No cooldown: the 61st consecutive frame re-arms (the standing
    // player logs "Nether ready" again on frame 120, 180, ... — the
    // Phase A semantics pin; the re-arm frame is the first of the new
    // 60, so it leaves 59).
    const PortalEntryStep re = portalEntryStep(true, armed);
    EXPECT_EQ(re.framesRemaining, kPortalEntryFrames - 1);
    EXPECT_FALSE(re.completed);
}

TEST(PortalEntryStepTest, LeavingCancelsImmediately) {
    int armed = kPortalEntryFrames;
    for (int i = 0; i < 59; ++i) {
        const PortalEntryStep s = portalEntryStep(true, armed);
        armed = s.framesRemaining;
    }
    EXPECT_EQ(armed, 1); // one frame from the completion
    const PortalEntryStep s = portalEntryStep(false, armed);
    EXPECT_EQ(s.framesRemaining, 0);
    EXPECT_FALSE(s.completed); // the cancel is NOT a completion
}

TEST(PortalEntryStepTest, ReEntryRestartsTheFullCount) {
    int armed = kPortalEntryFrames;
    for (int i = 0; i < 30; ++i) {
        armed = portalEntryStep(true, armed).framesRemaining;
    }
    EXPECT_EQ(armed, 30);
    armed = portalEntryStep(false, armed).framesRemaining; // leave
    EXPECT_EQ(armed, 0);
    const PortalEntryStep re = portalEntryStep(true, armed); // re-enter
    EXPECT_EQ(re.framesRemaining, kPortalEntryFrames - 1);
    EXPECT_FALSE(re.completed);
}

// ---------------------------------------------------------------------------
// The 8:1 dimension mapping (Phase A logs it, Phase B consumes it).
// ---------------------------------------------------------------------------

TEST(PortalMappingTest, OverworldToNetherFloors) {
    // floorDiv = the negative-safe floor (the C++ truncation-trap the
    // coordinates.hpp discipline pins): -7 / 8 = -1, not 0.
    const struct {
        int x, y, z;
        BlockPos want;
    } cases[] = {
        {7, 5, 7, BlockPos {0, 5, 0}},
        {-7, 5, -7, BlockPos {-1, 5, -1}},
        {16, 3, 8, BlockPos {2, 3, 1}},
        {-16, 3, -8, BlockPos {-2, 3, -1}},
        {8, 8, 8, BlockPos {1, 8, 1}},
        {0, 0, 0, BlockPos {0, 0, 0}},
        {23, 1, -23, BlockPos {2, 1, -3}},
    };
    for (const auto& c : cases) {
        EXPECT_EQ(overworldToNether(c.x, c.y, c.z), c.want)
            << c.x << "," << c.y << "," << c.z;
    }
}

TEST(PortalMappingTest, NetherToOverworldScales) {
    // V3-1 Phase B: the nether -> overworld mapping (the exact integer
    // product — no floor: the nether coordinate scaled up, Y unscaled;
    // the negative values scale symmetrically). The seven-group table
    // (the OverworldToNetherFloors twin): the round trip's stability is
    // the gate's travel evidence (the nether's landing cell's overworld
    // origin is the 8x8 block that floor-divides to it).
    const struct {
        int x, y, z;
        BlockPos want;
    } cases[] = {
        {1, 5, 1, BlockPos {8, 5, 8}},
        {-1, 5, -1, BlockPos {-8, 5, -8}},
        {2, 3, 1, BlockPos {16, 3, 8}},
        {-2, 3, -1, BlockPos {-16, 3, -8}},
        {0, 0, 0, BlockPos {0, 0, 0}},
        {7, 8, -7, BlockPos {56, 8, -56}},
        {3, 1, -3, BlockPos {24, 1, -24}},
    };
    for (const auto& c : cases) {
        EXPECT_EQ(netherToOverworld(c.x, c.y, c.z), c.want)
            << c.x << "," << c.y << "," << c.z;
    }
}

TEST(PortalMappingTest, RoundTripStability) {
    // The round trip (the wiki's 8:1 semantics): overworld -> nether ->
    // overworld lands in the 8x8 overworld block that maps to the nether
    // cell (floorDiv(ox, 8) * 8 <= ox < floorDiv(ox, 8) * 8 + 8); the
    // nether -> overworld -> nether round trip is EXACT (the product is
    // the block's corner — the cell that floor-divides to it includes
    // the corner). Negative coordinates included (the floorDiscipline's
    // negative-safe floor).
    const struct {
        int ox, oy, oz;
    } overworld[] = {
        {0, 55, -2}, {7, 3, 7}, {-7, 5, -7}, {15, 3, 8},
        {-16, 3, -8}, {100, 64, -200}, {1, 0, 127},
    };
    for (const auto& c : overworld) {
        const BlockPos n = overworldToNether(c.ox, c.oy, c.oz);
        const BlockPos back = netherToOverworld(n.x, n.y, n.z);
        // The overworld round trip lands in the 8x8 block.
        EXPECT_EQ(floorDiv(back.x, kPortalMappingScale), n.x)
            << c.ox << "," << c.oy << "," << c.oz;
        EXPECT_EQ(floorDiv(back.z, kPortalMappingScale), n.z)
            << c.ox << "," << c.oy << "," << c.oz;
        EXPECT_EQ(back.y, n.y) << c.ox << "," << c.oy << "," << c.oz;
        // The nether round trip is exact (the product's corner cell
        // floor-divides back to the nether coordinate).
        const BlockPos n2 = overworldToNether(back.x, back.y, back.z);
        EXPECT_EQ(n2, n) << c.ox << "," << c.oy << "," << c.oz;
    }
}

// ---------------------------------------------------------------------------
// The portal ambience roll (the frame-locked pure stream, hard rule 8).
// ---------------------------------------------------------------------------

TEST(PortalAmbientRollTest, DeterministicAndNonDegenerate) {
    // Same inputs -> same answer across calls (the event sequence is the
    // E2E's log evidence)...
    const BlockPos cell {10, 5, 12};
    const bool a = portalAmbientRoll(42, 100, cell);
    for (int i = 0; i < 8; ++i) {
        EXPECT_EQ(portalAmbientRoll(42, 100, cell), a);
    }
    // ...and the stream is neither silent nor stuck (the PITS 382
    // degenerate-roll guard: over 1200 frames at p = 1/120 the expected
    // ~10 hits must exist, and the roll must not fire every frame).
    int hits = 0;
    for (unsigned f = 0; f < 1200; ++f) {
        if (portalAmbientRoll(42, f, cell)) {
            ++hits;
        }
    }
    EXPECT_GT(hits, 0);
    EXPECT_LT(hits, 600);
    // A different cell decorrelates (the hash3 position salt — the
    // two portals do not squeak in lockstep).
    int lockstep = 0;
    for (unsigned f = 0; f < 240; ++f) {
        if (portalAmbientRoll(42, f, cell) ==
            portalAmbientRoll(42, f, {11, 5, 12})) {
            ++lockstep;
        }
    }
    EXPECT_LT(lockstep, 240); // not identical streams (p ≈ 0.9997 per
                               // frame of a 240-frame full match)
}

// ---------------------------------------------------------------------------
// The S6-style content rejections (the new block's field family over the
// existing strict machinery — the init arms consume these at the gate).
// ---------------------------------------------------------------------------

TEST(PortalDefTest, MissingStateDefaultRejected) {
    // state_count without state_default (the ENT-2 strict pair — the
    // portal's axis state would have no default; the choke point's seed
    // and the save's domain check both read it).
    auto r = BlockRegistry{}.registerBlock(parseDef(
        R"({"id":"nether_portal","properties":{"solid":false,"opaque":false)"
        R"(,"transparent":true,"light_emission":11},"model":)"
        R"({"shape":"full_cube","state_count":2,"textures":{"top":"nether_portal","side":"nether_portal","bottom":"nether_portal"}}})"));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("strict pair"), std::string::npos);
}

TEST(PortalDefTest, LightEmissionOutOfRangeRejected) {
    // light_emission 12 (the portal's Minecraft value is 11; the block
    // light scale is 0..15 — 16 is the scale's overflow, the existing
    // RejectsLightEmissionOutOfRange's arm; the portal family's pin is
    // the 11 content, the S6 gate's arm is the scale's rejection).
    auto r = BlockRegistry{}.registerBlock(parseDef(
        R"({"id":"nether_portal","properties":{"solid":false,"opaque":false)"
        R"(,"transparent":true,"light_emission":16},"model":)"
        R"({"shape":"full_cube","state_count":2,"state_default":0,"textures":{"top":"nether_portal","side":"nether_portal","bottom":"nether_portal"}}})"));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("light_emission"), std::string::npos);
}

TEST(PortalDefTest, DestroyableFlagParsesStrict) {
    // The destroyable flag is the strict boolean (the lava precedent):
    // the false pins the survival unbreakable (the S6 gate's arm is the
    // non-bool rejection over the same machinery).
    BlockRegistry reg;
    auto r = reg.registerBlock(parseDef(
        R"({"id":"nether_portal","properties":{"solid":false,"opaque":false)"
        R"(,"transparent":true,"light_emission":11,"destroyable":false},"model":)"
        R"({"shape":"full_cube","state_count":2,"state_default":0,"textures":{"top":"nether_portal","side":"nether_portal","bottom":"nether_portal"}}})"));
    ASSERT_TRUE(r.ok()) << r.error().message;
    const Block& b = reg.at(*r);
    EXPECT_FALSE(b.properties.destroyable);
}
