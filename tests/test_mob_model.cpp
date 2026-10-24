// ENT-1b: shipped quadruped mob model proportions + the swing clip
// predicate. The user-visible contract, pinned in data + pure math:
//   - four-footed bodies are LONGER than they are tall (length:height
//     ~1.7-1.9:1, the Minecraft proportion band);
//   - every swing leg's pivot (box top-center) sits AT the body bottom
//     (the joint out of the body — a leg hangs from the torso's
//     underside, it is never buried in the torso);
//   - the amplitudes (cow/sheep 0.5, chicken 0.4, spider 0.45 rad) keep
//     the leg clear of the body and the floor at EVERY one of the 64
//     swing phases: checkSwingClip (mob_swing_clip) is run on the
//     shipped boxes with the renderer's exact per-phase (sin, cos).
// Any future JSON edit that re-buries a pivot, re-inflates an
// amplitude, or raises the body off the legs turns this file red
// (the data and the predicate are mutually pinned).
#include <gtest/gtest.h>

#include <cstdint>
#include <string>

#include "core/asset_manager.hpp"
#include "core/json.hpp"
#include "entity/mob_registry.hpp"
#include "entity/mob_swing.hpp"
#include "entity/mob_swing_clip.hpp"
#include "renderer/texture_atlas.hpp"

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

// A layout covering every tile the four shipped quadruped models use
// (buildSwingModel runs makeMobModelGeometry, which resolves each box's
// texture through the atlas layout).
AtlasLayout makeQuadLayout() {
    auto r = AtlasLayout::parse(parseJson(
        R"({"columns":4,"rows":4,"tiles":{
            "cow_body":[0,0],"cow_legs":[1,0],"cow_head":[2,0],
            "sheep_body":[3,0],"sheep_legs":[0,1],"sheep_head":[1,1],
            "chicken_body":[2,1],"chicken_legs":[3,1],"chicken_head":[0,2],
            "spider_body":[1,2],"spider_head":[2,2]}})"));
    if (!r.ok()) {
        ADD_FAILURE() << r.error().message;
    }
    return *r;
}

MobRegistry loadShippedMobs() {
    AssetManager assets;
    EXPECT_TRUE(assets.setRoot("assets").ok())
        << "run the tests from the repository root";
    MobRegistry mobs;
    EXPECT_TRUE(mobs.loadDirectory(assets, "data/mobs").ok());
    return mobs;
}

// The (s, c) the renderer applies at limb angle t radians (the 16.16
// quantization + integer Taylor — the same numbers buildSwingModel
// bakes into SwingPart::sinCos).
void rendererSwingCos(float t, float& s, float& c) {
    const std::int32_t t16 = swingFixedFromRadian(t);
    std::int32_t s16 = 0;
    std::int32_t c16 = 0;
    swingSinCosFixed(t16, s16, c16);
    s = static_cast<float>(s16) / static_cast<float>(kSwingFixedScale);
    c = static_cast<float>(c16) / static_cast<float>(kSwingFixedScale);
}

// Float noise of the parsed-box arithmetic (pos/size divisions) that a
// "pivot at the body bottom" data value can accumulate: the joint test
// tolerates exactly this much.
constexpr float kJointEps = 1e-6f;

// boxBottom/boxTop helpers (the parsed float values, not rounded).
float boxBottom(const MobBox& b) { return b.center.y - b.size.y * 0.5f; }
float boxTop(const MobBox& b) { return b.center.y + b.size.y * 0.5f; }

} // namespace

// ---------------------------------------------------------------------------
// The predicate itself (synthetic geometry — the renderer's exact (s,c)).
// ---------------------------------------------------------------------------

TEST(MobSwingClipTest, TolerancesPinned) {
    // The named constants (the design's "tolerances as named constants"):
    // 0.02 = 1.6x the shipped max foot-corner dip (chicken @ 0.4 rad);
    // 0.25 = the joint fraction; 1e-6 = the forbidden-zone sliver.
    EXPECT_FLOAT_EQ(kClipGroundSliver, 0.02f);
    EXPECT_FLOAT_EQ(kClipJointOverlapFraction, 0.25f);
    EXPECT_FLOAT_EQ(kClipForbiddenSliver, 1e-6f);
}

TEST(MobSwingClipTest, RestingJointAtBodyBottomIsClean) {
    // Leg hangs from the body underside (pivot == body bottom, the
    // binary-exact joint): at rest the two boxes only share a face —
    // zero overlap, tip on the floor, everything passes.
    const MobBox leg {Vec3 {0.0f, 0.25f, 0.0f}, Vec3 {0.2f, 0.5f, 0.2f},
                      "t", "leg"};
    const MobBox body {Vec3 {0.0f, 1.0f, 0.0f}, Vec3 {0.6f, 1.0f, 0.6f},
                       "t", "body"};
    const SwingClipResult r = checkSwingClip(leg, body, 0.0f, 1.0f);
    EXPECT_TRUE(r.groundOk);
    EXPECT_TRUE(r.bodyOk);
    EXPECT_TRUE(r.ok());
    EXPECT_FLOAT_EQ(r.tipCenterY, 0.0f);
    EXPECT_FLOAT_EQ(r.cornerMinY, 0.0f);
    EXPECT_FLOAT_EQ(r.overlapVolume, 0.0f);
    EXPECT_FLOAT_EQ(r.forbiddenVolume, 0.0f);
}

TEST(MobSwingClipTest, JointAtBodyBottomSmallSwingPasses) {
    // The legitimate case the data targets: the joint at the body
    // bottom, a real swing — the pivot-end corners kiss the body
    // underside (a few % of the leg volume, all inside the pivot-end
    // third), the foot stays above the floor.
    const MobBox leg {Vec3 {0.0f, 0.3f, 0.0f}, Vec3 {0.2f, 0.6f, 0.2f},
                      "t", "leg"};
    const MobBox body {Vec3 {0.0f, 1.1f, 0.0f}, Vec3 {0.6f, 1.0f, 0.6f},
                       "t", "body"};
    float s = 0.0f;
    float c = 1.0f;
    rendererSwingCos(0.3f, s, c);
    const SwingClipResult r = checkSwingClip(leg, body, s, c);
    EXPECT_TRUE(r.groundOk) << "tip " << r.tipCenterY << " corner "
                            << r.cornerMinY;
    EXPECT_TRUE(r.bodyOk) << "overlap " << r.overlapVolume << " forbidden "
                          << r.forbiddenVolume;
    EXPECT_TRUE(r.ok());
    // The kiss is non-vacuous: the rotated top corner really does enter
    // the body a hair, and the bound has headroom over it.
    EXPECT_GT(r.overlapVolume, 0.0f);
    EXPECT_LT(r.overlapVolume, kClipJointOverlapFraction * 0.2f * 0.6f * 0.2f);
    EXPECT_LE(r.forbiddenVolume, kClipForbiddenSliver);
}

TEST(MobSwingClipTest, BuriedPivotFailsAtRest) {
    // The pre-ENT-1b cow's joint: the pivot 0.3 buried in the body.
    // Even at REST the leg's top 0.3 is inside the torso = 40% of the
    // leg volume (> 25%) and 0.05 of it in the bottom two-thirds
    // (> the sliver): the predicate must reject the geometry, not just
    // the swing. (The leg sits under the torso center: full x/z
    // overlap, the buried depth is the only factor.)
    const MobBox leg {Vec3 {0.0f, 0.375f, 0.0f}, Vec3 {0.25f, 0.75f, 0.25f},
                      "t", "leg"};
    const MobBox body {Vec3 {0.0f, 0.95f, 0.0f}, Vec3 {0.75f, 1.0f, 0.625f},
                       "t", "body"};
    const SwingClipResult r = checkSwingClip(leg, body, 0.0f, 1.0f);
    EXPECT_FALSE(r.bodyOk);
    EXPECT_FALSE(r.ok());
    EXPECT_GT(r.overlapVolume, kClipJointOverlapFraction * 0.25f * 0.75f * 0.25f);
    EXPECT_GT(r.forbiddenVolume, kClipForbiddenSliver);
    // The ground rule is independent and still passes (foot on the floor).
    EXPECT_TRUE(r.groundOk);
}

TEST(MobSwingClipTest, FootBelowGroundFails) {
    // A leg longer than its pivot is below the floor already at rest
    // (the deep-swing / low-pivot failure mode): tip -0.1 << -0.02.
    const MobBox leg {Vec3 {0.0f, 0.15f, 0.0f}, Vec3 {0.25f, 0.5f, 0.25f},
                      "t", "leg"};
    const MobBox body {Vec3 {0.0f, 5.0f, 0.0f}, Vec3 {0.5f, 1.0f, 0.5f},
                       "t", "body"};
    const SwingClipResult r = checkSwingClip(leg, body, 0.0f, 1.0f);
    EXPECT_FALSE(r.groundOk);
    EXPECT_FALSE(r.ok());
    EXPECT_FLOAT_EQ(r.tipCenterY, -0.1f);
    EXPECT_FLOAT_EQ(r.cornerMinY, -0.1f);
}

TEST(MobSwingClipTest, ForbiddenZoneIsolatesFromFraction) {
    // A buried-ish joint where the TOTAL overlap is exactly at the 25%
    // bound (passes rule 2a) but the overlap reaches into the leg's
    // bottom two-thirds (fails rule 2b): the forbidden zone is what
    // catches mid-shank interpenetration the fraction alone tolerates.
    // Binary-exact boxes: leg 0.25x0.5x0.25 at y 0..0.5 (pivot 0.5);
    // body x half-width 0.0625 (half the leg's), y 0.25..1.25.
    const MobBox leg {Vec3 {0.0f, 0.25f, 0.0f}, Vec3 {0.25f, 0.5f, 0.25f},
                      "t", "leg"};
    const MobBox body {Vec3 {0.0f, 0.75f, 0.0f}, Vec3 {0.125f, 1.0f, 0.25f},
                       "t", "body"};
    const SwingClipResult r = checkSwingClip(leg, body, 0.0f, 1.0f);
    const float legVolume = 0.25f * 0.5f * 0.25f;
    // Total = 0.125 (x) * 0.25 (y) * 0.25 (z) = exactly 25% of the leg.
    EXPECT_FLOAT_EQ(r.overlapVolume, kClipJointOverlapFraction * legVolume);
    // Forbidden = 0.125 * (0.5 - 1/3 ... y 0.25..1/3) * 0.25 — the
    // bottom-two-thirds sliver, far above the 1e-6 tolerance.
    EXPECT_GT(r.forbiddenVolume, kClipForbiddenSliver);
    EXPECT_FALSE(r.bodyOk);
    EXPECT_FALSE(r.ok());
    EXPECT_TRUE(r.groundOk);
}

TEST(MobSwingClipTest, VolumeFractionEndpoints) {
    // The 25% bound is INCLUSIVE (<=): a binary-exact case at exactly
    // 25% passes, a 31.25% case fails. Leg 0.25x0.5x0.25 at y 0..0.5
    // (pivot 0.5, x/z full inside the body); the body bottom set by its
    // center (0.875 -> y0 0.375 = 25% overlap; 0.84375 -> 0.34375 =
    // 31.25%).
    const MobBox leg {Vec3 {0.0f, 0.25f, 0.0f}, Vec3 {0.25f, 0.5f, 0.25f},
                      "t", "leg"};
    const float legVolume = 0.25f * 0.5f * 0.25f;
    const MobBox bodyAt25 {Vec3 {0.0f, 0.875f, 0.0f},
                           Vec3 {1.0f, 1.0f, 1.0f}, "t", "body"};
    const SwingClipResult r25 = checkSwingClip(leg, bodyAt25, 0.0f, 1.0f);
    EXPECT_FLOAT_EQ(r25.overlapVolume, kClipJointOverlapFraction * legVolume);
    EXPECT_LE(r25.forbiddenVolume, kClipForbiddenSliver); // top-third only
    EXPECT_TRUE(r25.bodyOk);
    EXPECT_TRUE(r25.ok());

    const MobBox bodyOver {Vec3 {0.0f, 0.84375f, 0.0f},
                           Vec3 {1.0f, 1.0f, 1.0f}, "t", "body"};
    const SwingClipResult rover = checkSwingClip(leg, bodyOver, 0.0f, 1.0f);
    EXPECT_GT(rover.overlapVolume, kClipJointOverlapFraction * legVolume);
    EXPECT_FALSE(rover.bodyOk);
    EXPECT_FALSE(rover.ok());
}

// ---------------------------------------------------------------------------
// The shipped content (assets/data/mobs) against the new contract.
// ---------------------------------------------------------------------------

TEST(MobModelTest, ShippedBodiesLongerThanTall) {
    // The proportion fix: body LENGTH (z) > body HEIGHT (y), the exact
    // pinned sizes, and the joint at the body bottom (pivot <= bottom,
    // float noise tolerated by kJointEps) for every swing leg.
    const MobRegistry mobs = loadShippedMobs();
    const struct {
        const char* mob;
        float w;
        float h;
        float l;
    } cases[] = {
        {"cow", 0.75f, 0.55f, 1.05f},   // length:height 1.9:1
        {"sheep", 0.625f, 0.55f, 0.95f}, // 1.73:1
        {"chicken", 0.375f, 0.35f, 0.5f} // 1.43:1
    };
    for (const auto& c : cases) {
        const Mob& m = mobs.at(mobs.idForName(c.mob));
        const MobBox& body = m.model.boxes[0];
        EXPECT_FLOAT_EQ(body.size.x, c.w) << c.mob;
        EXPECT_FLOAT_EQ(body.size.y, c.h) << c.mob;
        EXPECT_FLOAT_EQ(body.size.z, c.l) << c.mob;
        EXPECT_GT(body.size.z, body.size.y) << c.mob;
        for (const MobSwing& sw : m.model.swings) {
            const MobBox& leg = m.model.boxes[static_cast<std::size_t>(sw.boxIndex)];
            EXPECT_LE(swingPivot(leg).y, boxBottom(body) + kJointEps)
                << c.mob << " " << leg.name << ": pivot "
                << swingPivot(leg).y << " body bottom " << boxBottom(body);
        }
    }
}

TEST(MobModelTest, ShippedSwingAmplitudesPinned) {
    // The ENT-1b amplitude schedule (down from 0.7/0.5/0.9 — the values
    // the predicate certifies clip-free over all 64 phases):
    // cow/sheep 0.5, chicken 0.4, spider 0.45 rad. Part names and the
    // gait groups are unchanged from ENT-1.
    struct Exp {
        const char* part;
        float amplitude;
        int group;
    };
    const Exp quadruped[] = {{"leg_fl", 0.5f, 0}, {"leg_fr", 0.5f, 1},
                             {"leg_rl", 0.5f, 1}, {"leg_rr", 0.5f, 0}};
    const Exp chicken[] = {{"leg_l", 0.4f, 0}, {"leg_r", 0.4f, 1}};
    const Exp spider[] = {{"leg_1", 0.45f, 0}, {"leg_2", 0.45f, 1},
                          {"leg_3", 0.45f, 0}, {"leg_4", 0.45f, 1},
                          {"leg_5", 0.45f, 0}, {"leg_6", 0.45f, 1},
                          {"leg_7", 0.45f, 0}, {"leg_8", 0.45f, 1}};
    const MobRegistry mobs = loadShippedMobs();
    auto expectSwings = [&mobs](const char* mobName, const Exp* exp, int n) {
        const Mob& m = mobs.at(mobs.idForName(mobName));
        ASSERT_EQ(static_cast<int>(m.model.swings.size()), n) << mobName;
        for (int i = 0; i < n; ++i) {
            const MobSwing& s = m.model.swings[static_cast<std::size_t>(i)];
            const std::string part =
                m.model.boxes[static_cast<std::size_t>(s.boxIndex)].name;
            EXPECT_EQ(part, exp[i].part) << mobName << " swing " << i;
            EXPECT_FLOAT_EQ(s.amplitude, exp[i].amplitude)
                << mobName << " swing " << i;
            EXPECT_EQ(s.group, exp[i].group) << mobName << " swing " << i;
        }
    };
    expectSwings("cow", quadruped, 4);
    expectSwings("sheep", quadruped, 4);
    expectSwings("chicken", chicken, 2);
    expectSwings("spider", spider, 8);
    // The humanoids keep their ENT-1 documented deviation (no limb
    // geometry): no swings.
    EXPECT_TRUE(mobs.at(mobs.idForName("zombie")).model.swings.empty());
    EXPECT_TRUE(mobs.at(mobs.idForName("skeleton")).model.swings.empty());
}

TEST(MobModelTest, ShippedSpiderGeometryPinned) {
    // The spider: body raised to leave a leg room (bottom 0.2), the 8
    // legs LONGER (0.2 > the old 0.16) and THINNER (0.08 > 0.1 wide no:
    // 0.08 < 0.1) and shifted OUTWARD (outer face 0.66 past the body
    // edge 0.60), pivots exactly at the body bottom, head on the body
    // front-top, everything reaching the ground.
    const MobRegistry mobs = loadShippedMobs();
    const Mob& sp = mobs.at(mobs.idForName("spider"));
    const MobBox& body = sp.model.boxes[0];
    EXPECT_FLOAT_EQ(body.center.y, 0.55f);
    EXPECT_FLOAT_EQ(body.size.x, 1.2f);
    EXPECT_FLOAT_EQ(body.size.y, 0.7f);
    EXPECT_FLOAT_EQ(body.size.z, 0.9f);
    ASSERT_EQ(sp.model.boxes.size(), 10u);
    for (std::size_t i = 1; i <= 8; ++i) {
        const MobBox& leg = sp.model.boxes[i];
        EXPECT_NEAR(std::abs(leg.center.x), 0.62f, 1e-6f) << "leg " << i;
        EXPECT_FLOAT_EQ(leg.center.y, 0.1f) << "leg " << i;
        EXPECT_FLOAT_EQ(leg.size.x, 0.08f) << "leg " << i;
        EXPECT_FLOAT_EQ(leg.size.y, 0.2f) << "leg " << i;
        EXPECT_FLOAT_EQ(leg.size.z, 0.08f) << "leg " << i;
        EXPECT_NEAR(boxBottom(leg), 0.0f, 1e-6f) << "leg " << i; // to ground
        EXPECT_LE(swingPivot(leg).y, boxBottom(body) + kJointEps)
            << "leg " << i;
    }
    const MobBox& head = sp.model.boxes[9];
    EXPECT_FLOAT_EQ(head.center.y, 0.65f);
    EXPECT_FLOAT_EQ(head.center.z, 0.4f);
    EXPECT_FLOAT_EQ(head.size.y, 0.5f);
    // The head top sits on the body top (the old flush relationship,
    // carried up with the body).
    EXPECT_NEAR(boxTop(head), boxTop(body), 1e-6f);
}

TEST(MobModelTest, ShippedBoxesInsideModelAabb) {
    // Every box of the four re-proportioned models stays inside the
    // physical AABB (the model width/height are the collision box —
    // untouched; the head/wings/beak re-placement must not escape it).
    const MobRegistry mobs = loadShippedMobs();
    const char* names[] = {"cow", "sheep", "chicken", "spider"};
    for (const char* name : names) {
        const Mob& m = mobs.at(mobs.idForName(name));
        const float halfW = m.model.width * 0.5f;
        for (std::size_t i = 0; i < m.model.boxes.size(); ++i) {
            const MobBox& b = m.model.boxes[i];
            EXPECT_LE(std::abs(b.center.x) + b.size.x * 0.5f, halfW + 1e-6f)
                << name << " box " << i;
            EXPECT_LE(std::abs(b.center.z) + b.size.z * 0.5f, halfW + 1e-6f)
                << name << " box " << i;
            EXPECT_GE(b.center.y - b.size.y * 0.5f, -1e-6f)
                << name << " box " << i;
            EXPECT_LE(b.center.y + b.size.y * 0.5f, m.model.height + 1e-6f)
                << name << " box " << i;
        }
    }
}

TEST(MobModelTest, ShippedAllPhasesNoClip) {
    // THE acceptance: every swing part of the four shipped mobs, at
    // every one of the 64 phases, with the renderer's exact per-phase
    // (sin, cos) (the SwingPart::sinCos the mesh upload reads), passes
    // the clip predicate against its body box (box 0 in every model).
    const MobRegistry mobs = loadShippedMobs();
    const AtlasLayout layout = makeQuadLayout();
    const char* names[] = {"cow", "sheep", "chicken", "spider"};
    for (const char* name : names) {
        const Mob& m = mobs.at(mobs.idForName(name));
        const SwingModel sm = buildSwingModel(m.model, layout);
        ASSERT_TRUE(sm.active()) << name;
        const MobBox& body = m.model.boxes[0];
        for (const SwingPart& part : sm.parts) {
            const MobBox& leg =
                m.model.boxes[static_cast<std::size_t>(part.boxIndex)];
            for (int phase = 0; phase < kSwingPhaseCount; ++phase) {
                const int eff = (part.group == 1)
                                    ? (phase + kSwingHalf) % kSwingPhaseCount
                                    : phase;
                const float s = part.sinCos[2 * eff];
                const float c = part.sinCos[2 * eff + 1];
                const SwingClipResult r = checkSwingClip(leg, body, s, c);
                EXPECT_TRUE(r.ok()) << name << " " << leg.name << " phase "
                                    << phase << ": tip " << r.tipCenterY
                                    << " corner " << r.cornerMinY
                                    << " overlap " << r.overlapVolume
                                    << " forbidden " << r.forbiddenVolume;
            }
        }
    }
}

TEST(MobModelTest, ShippedMaxSwingJointKissNonVacuous) {
    // The all-phases pass is only meaningful if the joint actually
    // kisses the body at full stride: the cow's front-left leg at its
    // max-swing phase (eff 16 = the table's +1 entry, group 0 = phase
    // 16) overlaps the body by a real, positive, few-percent volume —
    // the pivot-end corner sliver the 25% bound exists for.
    const MobRegistry mobs = loadShippedMobs();
    const Mob& cow = mobs.at(mobs.idForName("cow"));
    const SwingModel sm = buildSwingModel(cow.model, makeQuadLayout());
    ASSERT_EQ(sm.parts.size(), 4u);
    // parts[0] = leg_fl, group 0.
    EXPECT_EQ(cow.model.boxes[static_cast<std::size_t>(sm.parts[0].boxIndex)].name,
              "leg_fl");
    const int eff = kSwingPhaseCount / 4; // 16: the max-swing phase
    const float s = sm.parts[0].sinCos[2 * eff];
    const float c = sm.parts[0].sinCos[2 * eff + 1];
    const SwingClipResult r = checkSwingClip(
        cow.model.boxes[static_cast<std::size_t>(sm.parts[0].boxIndex)],
        cow.model.boxes[0], s, c);
    EXPECT_TRUE(r.ok());
    const float legVolume =
        0.25f * 0.75f * 0.25f; // the pinned cow leg size
    EXPECT_GT(r.overlapVolume, 0.0f);
    EXPECT_LT(r.overlapVolume, kClipJointOverlapFraction * legVolume);
    EXPECT_LE(r.forbiddenVolume, kClipForbiddenSliver);
    // The foot corner clears the floor with the 0.02 sliver to spare.
    EXPECT_GT(r.cornerMinY, 0.0f);
}
