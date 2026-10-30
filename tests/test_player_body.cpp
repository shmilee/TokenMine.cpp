// UX-3: the player's world-space body — the shipped content pin + the
// swing-clip acceptance, mirroring the ENT-1b mob-model discipline
// (test_mob_model.cpp):
//   - data/player_body.json pins the 6 boxes (exact pos/size/texture) +
//     the 4 swings (exact amplitude/group) + the shared-tile convention
//     (both arms one tile, both legs one tile) + the joint geometry
//     (leg pivots AT the body bottom, arm pivots AT the body top — the
//     ENT-1b "joint out of the body" rule: zero overlap at rest);
//   - checkSwingClip (mob_swing_clip) is run on the shipped boxes with
//     the renderer's exact per-phase (sin, cos): every swing part, every
//     one of the 64 phases, clip-free (the acceptance) + a non-vacuous
//     joint kiss at the max-swing phase;
//   - the parseModel label ("player body model") propagates into every
//     strict-parse error (the negative cases);
//   - the five generated skin PNGs are byte-pinned (FNV-1a 64, the
//     font.png precedent) AND the 15 pre-UX-3 mob tiles are pinned
//     unchanged (the "existing assets untouched" red line).
#include <gtest/gtest.h>

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <sstream>
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

constexpr const char* kLabel = "player body model";

// The shipped player body, parsed through the SAME strict path the game
// loader uses (MobRegistry::parseModel, the label = "player body model").
// Fills `out`, returns false (with a recorded failure) on any problem.
bool loadPlayerBody(MobModel& out) {
    AssetManager assets;
    if (!assets.setRoot("assets").ok()) {
        ADD_FAILURE() << "run the tests from the repository root";
        return false;
    }
    auto text = assets.loadText("data/player_body.json");
    if (!text.ok()) {
        ADD_FAILURE() << text.error().message;
        return false;
    }
    auto def = Json::parse(*text);
    if (!def.ok()) {
        ADD_FAILURE() << def.error().message;
        return false;
    }
    // The model-only discipline: exactly one top-level 'model' section
    // (no id / properties / AI fields — the task's red line).
    if (!def->isObject() || def->members().size() != 1 ||
        def->find("model") == nullptr ||
        !def->find("model")->isObject()) {
        ADD_FAILURE() << "player_body.json must contain exactly the "
                         "'model' section (no id / properties / AI fields)";
        return false;
    }
    auto parsed = MobRegistry::parseModel(*def->find("model"), out, kLabel);
    if (!parsed.ok()) {
        ADD_FAILURE() << parsed.error().message;
        return false;
    }
    return true;
}

// The 4 part tiles in a small synthetic layout (buildSwingModel resolves
// each box's texture through the layout — the makeQuadLayout pattern).
AtlasLayout makePlayerLayout() {
    auto r = AtlasLayout::parse(parseJson(
        R"({"columns":4,"rows":2,"tiles":{
            "player_head":[0,0],"player_body":[1,0],
            "player_arm":[2,0],"player_leg":[3,0]}})"));
    if (!r.ok()) {
        ADD_FAILURE() << r.error().message;
    }
    return *r;
}

const MobBox* findBox(const MobModel& model, const std::string& name) {
    for (const MobBox& box : model.boxes) {
        if (box.name == name) {
            return &box;
        }
    }
    return nullptr;
}

float boxBottom(const MobBox& b) { return b.center.y - b.size.y * 0.5f; }
float boxTop(const MobBox& b) { return b.center.y + b.size.y * 0.5f; }

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

std::string readAll(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    EXPECT_TRUE(in) << path;
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

// The project FNV pin (the font.png precedent: offset basis
// 1469598103934665603, prime 1099511628211) over the file bytes.
std::string fnv1a64(const std::string& data) {
    std::uint64_t h = 1469598103934665603ULL; // FNV-1a 64 offset basis
    for (char ch : data) {
        h ^= static_cast<unsigned char>(ch);
        h *= 1099511628211ULL;
    }
    char hex[17];
    std::snprintf(hex, sizeof(hex), "%016llx",
                  static_cast<unsigned long long>(h));
    return hex;
}

} // namespace

// ---------------------------------------------------------------------------
// The shipped content pin (data + atlas).
// ---------------------------------------------------------------------------

TEST(PlayerBodyTest, ShippedModelPinned) {
    MobModel m;
    ASSERT_TRUE(loadPlayerBody(m));
    EXPECT_FLOAT_EQ(m.width, 1.0f);   // the box VALIDATION bound (NOT the
    EXPECT_FLOAT_EQ(m.height, 1.8f);  // collision AABB — that stays
                                      // Player::Params 0.6 x 1.8)
    ASSERT_EQ(m.boxes.size(), 6u);
    // Box 0: the head (unnamed — it never swings), 0.5^3 at the top.
    const MobBox& head = m.boxes[0];
    EXPECT_TRUE(head.name.empty());
    EXPECT_FLOAT_EQ(head.center.x, 0.0f);
    EXPECT_FLOAT_EQ(head.center.y, 1.55f);
    EXPECT_FLOAT_EQ(head.center.z, 0.0f);
    EXPECT_FLOAT_EQ(head.size.x, 0.5f);
    EXPECT_FLOAT_EQ(head.size.y, 0.5f);
    EXPECT_FLOAT_EQ(head.size.z, 0.5f);
    EXPECT_EQ(head.texture, "player_head");
    // The body: pos [0, 1.025, 0], size [0.5, 0.55, 0.25] — the bottom
    // flush with the leg pivots (0.75), the top flush with the head
    // bottom (1.3) + the arm pivots (1.3).
    const MobBox* body = findBox(m, "body");
    ASSERT_NE(body, nullptr);
    EXPECT_FLOAT_EQ(body->center.x, 0.0f);
    EXPECT_FLOAT_EQ(body->center.y, 1.025f);
    EXPECT_FLOAT_EQ(body->center.z, 0.0f);
    EXPECT_FLOAT_EQ(body->size.x, 0.5f);
    EXPECT_FLOAT_EQ(body->size.y, 0.55f);
    EXPECT_FLOAT_EQ(body->size.z, 0.25f);
    EXPECT_EQ(body->texture, "player_body");
    // The four limbs (the shared-tile convention: both arms player_arm,
    // both legs player_leg — the Minecraft convention).
    const struct {
        const char* name;
        float x;
        float y;
    } limbs[] = {{"arm_l", -0.375f, 0.925f}, {"arm_r", 0.375f, 0.925f},
                 {"leg_l", -0.125f, 0.375f}, {"leg_r", 0.125f, 0.375f}};
    for (const auto& l : limbs) {
        const MobBox* box = findBox(m, l.name);
        ASSERT_NE(box, nullptr) << l.name;
        EXPECT_FLOAT_EQ(box->center.x, l.x) << l.name;
        EXPECT_FLOAT_EQ(box->center.y, l.y) << l.name;
        EXPECT_FLOAT_EQ(box->center.z, 0.0f) << l.name;
        EXPECT_FLOAT_EQ(box->size.x, 0.25f) << l.name;
        EXPECT_FLOAT_EQ(box->size.y, 0.75f) << l.name;
        EXPECT_FLOAT_EQ(box->size.z, 0.25f) << l.name;
        const std::string expectedTexture =
            l.name[0] == 'a' ? "player_arm" : "player_leg";
        EXPECT_EQ(box->texture, expectedTexture) << l.name;
    }
    // The joints (the ENT-1b rule, pinned): the leg pivots (box
    // top-center) sit EXACTLY at the body bottom — zero overlap at rest
    // (a face-touch reads 0 in the clip predicate); the arm pivots sit
    // at the body top (the shoulder).
    for (const char* n : {"leg_l", "leg_r"}) {
        EXPECT_FLOAT_EQ(boxTop(*findBox(m, n)), boxBottom(*body));
    }
    for (const char* n : {"arm_l", "arm_r"}) {
        EXPECT_FLOAT_EQ(boxTop(*findBox(m, n)), boxTop(*body));
    }
    // The head sits on the body (the bottom flush with the body top).
    EXPECT_FLOAT_EQ(boxBottom(head), boxTop(*body));
    // The diagonal gait: arm_l + leg_r in group 0, arm_r + leg_l in
    // group 1 (the opposite-side pairs swing together); legs 0.35 rad,
    // arms 0.3 rad.
    ASSERT_EQ(m.swings.size(), 4u);
    const struct {
        const char* part;
        float amplitude;
        int group;
    } expSwings[] = {{"arm_l", 0.3f, 0}, {"leg_r", 0.35f, 0},
                     {"arm_r", 0.3f, 1}, {"leg_l", 0.35f, 1}};
    for (int i = 0; i < 4; ++i) {
        const MobSwing& s = m.swings[static_cast<std::size_t>(i)];
        const std::string part =
            m.boxes[static_cast<std::size_t>(s.boxIndex)].name;
        EXPECT_EQ(part, expSwings[i].part) << "swing " << i;
        EXPECT_FLOAT_EQ(s.amplitude, expSwings[i].amplitude) << "swing "
                                                             << i;
        EXPECT_EQ(s.group, expSwings[i].group) << "swing " << i;
    }
    // Every box inside the model AABB (the parser enforces it — pin the
    // bound values too: the arm outer face at exactly width/2, the head
    // top at exactly the height).
    for (std::size_t i = 0; i < m.boxes.size(); ++i) {
        const MobBox& b = m.boxes[i];
        EXPECT_LE(std::abs(b.center.x) + b.size.x * 0.5f, m.width / 2 + 1e-6f)
            << "box " << i;
        EXPECT_LE(std::abs(b.center.z) + b.size.z * 0.5f, m.width / 2 + 1e-6f)
            << "box " << i;
        EXPECT_GE(b.center.y - b.size.y * 0.5f, -1e-6f) << "box " << i;
        EXPECT_LE(b.center.y + b.size.y * 0.5f, m.height + 1e-6f) << "box "
                                                                  << i;
    }
}

TEST(PlayerBodyTest, ShippedTilesInAtlasLayout) {
    // The four part tiles have their appended cells in the atlas layout
    // (the farmland_wet GEN-9 append-only precedent: [10..13, 4]).
    AssetManager assets;
    ASSERT_TRUE(assets.setRoot("assets").ok())
        << "run the tests from the repository root";
    auto text = assets.loadText("data/textures/block_atlas.json");
    ASSERT_TRUE(text.ok());
    auto def = Json::parse(*text);
    ASSERT_TRUE(def.ok());
    const Json* tiles = def->find("tiles");
    ASSERT_NE(tiles, nullptr);
    const std::pair<int, int> exp[] = {
        {10, 4}, // player_head
        {11, 4}, // player_body
        {12, 4}, // player_arm
        {13, 4}, // player_leg
    };
    const char* names[] = {"player_head", "player_body", "player_arm",
                           "player_leg"};
    for (int i = 0; i < 4; ++i) {
        const Json* cell = tiles->find(names[i]);
        ASSERT_NE(cell, nullptr) << names[i];
        ASSERT_TRUE(cell->isArray() && cell->size() == 2);
        EXPECT_EQ(static_cast<int>(cell->at(0).asDouble(0)), exp[i].first);
        EXPECT_EQ(static_cast<int>(cell->at(1).asDouble(0)), exp[i].second);
    }
}

// ---------------------------------------------------------------------------
// The clip acceptance (the ENT-1b "ShippedAllPhasesNoClip" twin).
// ---------------------------------------------------------------------------

TEST(PlayerBodyTest, ShippedAllPhasesNoClip) {
    // THE acceptance: every swing part of the shipped player body, at
    // every one of the 64 phases, with the renderer's exact per-phase
    // (sin, cos) (the SwingPart::sinCos the mesh upload reads), passes
    // the clip predicate against the body box.
    MobModel m;
    ASSERT_TRUE(loadPlayerBody(m));
    const MobBox& body = *findBox(m, "body");
    const SwingModel sm = buildSwingModel(m, makePlayerLayout());
    ASSERT_TRUE(sm.active());
    ASSERT_EQ(sm.parts.size(), 4u);
    for (const SwingPart& part : sm.parts) {
        const MobBox& limb =
            m.boxes[static_cast<std::size_t>(part.boxIndex)];
        for (int phase = 0; phase < kSwingPhaseCount; ++phase) {
            const int eff = (part.group == 1)
                                ? (phase + kSwingHalf) % kSwingPhaseCount
                                : phase;
            const float s = part.sinCos[2 * eff];
            const float c = part.sinCos[2 * eff + 1];
            const SwingClipResult r = checkSwingClip(limb, body, s, c);
            EXPECT_TRUE(r.ok()) << limb.name << " phase " << phase
                                << ": tip " << r.tipCenterY << " corner "
                                << r.cornerMinY << " overlap "
                                << r.overlapVolume << " forbidden "
                                << r.forbiddenVolume;
        }
    }
}

TEST(PlayerBodyTest, ShippedMaxSwingJointKissNonVacuous) {
    // The all-phases pass is only meaningful if the leg really KISSES
    // the body at full stride (the pivot-end corner sliver the 25% bound
    // exists for) and clears the floor — and if the arm NEVER touches
    // the body (its x interval is disjoint from the body's at every
    // phase: the rotation about the X axis leaves x invariant).
    MobModel m;
    ASSERT_TRUE(loadPlayerBody(m));
    const MobBox& body = *findBox(m, "body");
    const SwingModel sm = buildSwingModel(m, makePlayerLayout());
    const int eff = kSwingPhaseCount / 4; // 16: the max-swing phase
    for (const SwingPart& part : sm.parts) {
        const MobBox& limb =
            m.boxes[static_cast<std::size_t>(part.boxIndex)];
        // The part's baked (s, c) at the max-swing phase IS the
        // renderer's exact fixed-point sin/cos of the full amplitude
        // (eff 16 = the table's +1 entry: theta = amplitude exactly).
        float s = 0.0f;
        float c = 1.0f;
        rendererSwingCos(
            static_cast<float>(
                part.amplitudeFixed) / static_cast<float>(kSwingFixedScale),
            s, c);
        EXPECT_FLOAT_EQ(part.sinCos[2 * eff], s) << limb.name;
        EXPECT_FLOAT_EQ(part.sinCos[2 * eff + 1], c) << limb.name;
        const SwingClipResult r = checkSwingClip(limb, body, s, c);
        EXPECT_TRUE(r.ok()) << limb.name;
        const float limbVolume =
            limb.size.x * limb.size.y * limb.size.z;
        if (limb.name == "leg_l" || limb.name == "leg_r") {
            // The leg: a real, positive, few-percent overlap at the
            // pivot end — and the foot corner clears the floor.
            EXPECT_GT(r.overlapVolume, 0.0f) << limb.name;
            EXPECT_LT(r.overlapVolume,
                      kClipJointOverlapFraction * limbVolume)
                << limb.name;
            EXPECT_LE(r.forbiddenVolume, kClipForbiddenSliver)
                << limb.name;
            EXPECT_GT(r.cornerMinY, -kClipGroundSliver) << limb.name;
        } else {
            // The arm: structurally zero overlap (x-disjoint from the
            // body, the rotation leaves x invariant).
            EXPECT_FLOAT_EQ(r.overlapVolume, 0.0f) << limb.name;
            EXPECT_FLOAT_EQ(r.forbiddenVolume, 0.0f) << limb.name;
        }
    }
}

// ---------------------------------------------------------------------------
// The strict parse (the label = "player body model" in every error).
// ---------------------------------------------------------------------------

TEST(PlayerBodyTest, ParseLabelsAndRejects) {
    // A box outside the AABB: the error names the box + the AABB.
    MobModel m1;
    auto r1 = MobRegistry::parseModel(parseJson(
                                           R"({"width":1.0,"height":1.8,"boxes":)"
                                           R"([{"pos":[0,2.0,0],"size":[0.5,0.5,0.5],)"
                                           R"("texture":"player_head"}]})"),
                                       m1, kLabel);
    ASSERT_FALSE(r1.ok());
    EXPECT_NE(r1.error().message.find(kLabel), std::string::npos)
        << r1.error().message;
    EXPECT_NE(r1.error().message.find("lies outside"), std::string::npos)
        << r1.error().message;
    // A swing referencing an unknown part.
    MobModel m2;
    auto r2 = MobRegistry::parseModel(
        parseJson(R"({"width":1.0,"height":1.8,"boxes":)"
                  R"([{"name":"arm_l","pos":[-0.375,0.925,0],)"
                  R"("size":[0.25,0.75,0.25],"texture":"player_arm"}],)"
                  R"("swings":[{"part":"arm_x","amplitude":0.3,"group":0}]})"),
        m2, kLabel);
    ASSERT_FALSE(r2.ok());
    EXPECT_NE(r2.error().message.find(kLabel), std::string::npos)
        << r2.error().message;
    EXPECT_NE(r2.error().message.find("unknown part 'arm_x'"),
              std::string::npos)
        << r2.error().message;
    // An amplitude of 0 (the data range is (0, 1]).
    MobModel m3;
    auto r3 = MobRegistry::parseModel(
        parseJson(R"({"width":1.0,"height":1.8,"boxes":)"
                  R"([{"name":"arm_l","pos":[-0.375,0.925,0],)"
                  R"("size":[0.25,0.75,0.25],"texture":"player_arm"}],)"
                  R"("swings":[{"part":"arm_l","amplitude":0,"group":0}]})"),
        m3, kLabel);
    ASSERT_FALSE(r3.ok());
    EXPECT_NE(r3.error().message.find(kLabel), std::string::npos)
        << r3.error().message;
    EXPECT_NE(r3.error().message.find("'amplitude' must be in (0, 1]"),
              std::string::npos)
        << r3.error().message;
    // An unknown box field (the strict box discipline).
    MobModel m4;
    auto r4 = MobRegistry::parseModel(
        parseJson(R"({"width":1.0,"height":1.8,"boxes":)"
                  R"([{"pos":[0,1.55,0],"size":[0.5,0.5,0.5],)"
                  R"("texture":"player_head","uv":0.5}]})"),
        m4, kLabel);
    ASSERT_FALSE(r4.ok());
    EXPECT_NE(r4.error().message.find(kLabel), std::string::npos)
        << r4.error().message;
    EXPECT_NE(r4.error().message.find("has unknown field 'uv'"),
              std::string::npos)
        << r4.error().message;
    // An unknown MODEL field (the strict model discipline).
    MobModel m5;
    auto r5 = MobRegistry::parseModel(
        parseJson(R"({"width":1.0,"height":1.8,"ai":{"mood":"happy"},"boxes":)"
                  R"([{"pos":[0,1.55,0],"size":[0.5,0.5,0.5],)"
                  R"("texture":"player_head"}]})"),
        m5, kLabel);
    ASSERT_FALSE(r5.ok());
    EXPECT_NE(r5.error().message.find(kLabel), std::string::npos)
        << r5.error().message;
    EXPECT_NE(r5.error().message.find("has unknown field 'ai'"),
              std::string::npos)
        << r5.error().message;
    // The default label stays the mob's (the pre-UX-3 messages are
    // bit-identical — the existing call site is untouched).
    MobModel m6;
    auto r6 = MobRegistry::parseModel(
        parseJson(R"({"width":0,"height":1.8,"boxes":)"
                  R"([{"pos":[0,1.55,0],"size":[0.5,0.5,0.5],)"
                  R"("texture":"zombie_head"}]})"),
        m6);
    ASSERT_FALSE(r6.ok());
    EXPECT_NE(r6.error().message.find("mob model"), std::string::npos)
        << r6.error().message;
}

// ---------------------------------------------------------------------------
// The generated skin assets (byte pins — the font.png FNV precedent).
// ---------------------------------------------------------------------------

TEST(PlayerBodyTest, SkinPngBytesPinned) {
    // The five UX-3 files are byte-stable (re-running
    // tools/generate_player_skin.py must not change them).
    const struct {
        const char* path;
        const char* fnv;
    } newFiles[] = {
        {"assets/textures/player/player_skin.png", "537f4015dccd62f1"},
        {"assets/textures/mobs/player_head.png", "47fc72c279990511"},
        {"assets/textures/mobs/player_body.png", "76d6f6b7c9675eb1"},
        {"assets/textures/mobs/player_arm.png", "878e9e2f77428cf5"},
        {"assets/textures/mobs/player_leg.png", "e3e1cb8051e76e23"},
    };
    for (const auto& f : newFiles) {
        EXPECT_EQ(fnv1a64(readAll(f.path)), f.fnv) << f.path;
    }
    // The 15 pre-UX-3 mob tiles are UNCHANGED (the existing-assets red
    // line — a re-run of the generator touching them turns this red).
    const struct {
        const char* name;
        const char* fnv;
    } oldTiles[] = {
        {"zombie_head", "8d30c25812ef79ef"}, {"zombie_body", "5cc03df4a3dfac56"},
        {"cow_head", "a4a3f2ad0cae75ac"}, {"cow_body", "264067f8715d4d82"},
        {"cow_legs", "a4b38ddfd0b4b95a"}, {"sheep_head", "bf48b64dfdd29cb4"},
        {"sheep_body", "7669a885e8f61ae7"}, {"sheep_legs", "827bb4ad0c257052"},
        {"chicken_head", "51fc5b0be6781e20"}, {"chicken_body", "0820182c10a5d163"},
        {"chicken_legs", "b144d6480bec42e8"}, {"skeleton_head", "0ce4ae9ec3010644"},
        {"skeleton_body", "3f2c6b5f89fb6584"}, {"spider_head", "82f2b8470e0b9f99"},
        {"spider_body", "27ad7a29340e1c29"},
    };
    for (const auto& t : oldTiles) {
        EXPECT_EQ(fnv1a64(readAll(std::string("assets/textures/mobs/") +
                                  t.name + ".png")),
                  t.fnv)
            << t.name;
    }
}
