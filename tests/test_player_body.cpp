// UX-3 / V2-2: the player's world-space body — the shipped content pin
// + the swing-clip acceptance, mirroring the ENT-1b mob-model discipline
// (test_mob_model.cpp):
//   - data/player_body.json pins the 6 boxes (exact pos/size/texture) +
//     the V2-2 per-face `faces` maps (exact: the Steve tiles) + the 4
//     swings (exact amplitude/group) + the shared-tile convention
//     (both arms one tile, both legs one tile) + the joint geometry
//     (leg pivots AT the body bottom, arm pivots AT the body top — the
//     ENT-1b "joint out of the body" rule: zero overlap at rest);
//   - checkSwingClip (mob_swing_clip) is run on the shipped boxes with
//     the renderer's exact per-phase (sin, cos): every swing part, every
//     one of the 64 phases, clip-free (the acceptance) + a non-vacuous
//     joint kiss at the max-swing phase;
//   - the parseModel label ("player body model") propagates into every
//     strict-parse error (the negative cases);
//   - the atlas layout pins the seven V2-2 per-face tiles (exact cells,
//     the appended row-5 free run) AND the 90 surviving legacy entries
//     zero-shift (the append-only red line) AND the four retired UX-3
//     grid tiles absent;
//   - the eight generated skin PNGs are byte-pinned (FNV-1a 64, the
//     font.png precedent) AND the 15 pre-UX-3 mob tiles are pinned
//     unchanged (the "existing assets untouched" red line) AND the four
//     retired UX-3 player PNGs are gone from disk.
#include <gtest/gtest.h>

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
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

// The 7 V2-2 per-face tiles in a small synthetic layout (buildSwingModel
// resolves each box's texture + faces through the layout — the
// makeQuadLayout pattern).
AtlasLayout makePlayerLayout() {
    auto r = AtlasLayout::parse(parseJson(
        R"({"columns":4,"rows":2,"tiles":{
            "ph_face":[0,0],"ph_hair":[1,0],"ph_side":[2,0],
            "pb_shirt":[3,0],"pa_arm":[0,1],"pl_pants":[1,1],
            "pl_boot":[2,1]}})"));
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
    EXPECT_EQ(head.texture, "ph_face");
    // V2-2: the per-face map — the Steve face front, the hair back/top/
    // bottom, the hair+ear sides (the yaw-0 = -Z convention: front = -Z
    // = the face the third-person camera meets).
    EXPECT_EQ(head.faces, (std::map<std::string, std::string>{
                               {"front", "ph_face"},
                               {"back", "ph_hair"},
                               {"left", "ph_side"},
                               {"right", "ph_side"},
                               {"top", "ph_hair"},
                               {"bottom", "ph_hair"}}));
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
    EXPECT_EQ(body->texture, "pb_shirt");
    // V2-2: all six faces the shirt tile (the flat-color torso — the
    // caps show the shirt art like the sides).
    EXPECT_EQ(body->faces, (std::map<std::string, std::string>{
                               {"front", "pb_shirt"},
                               {"back", "pb_shirt"},
                               {"left", "pb_shirt"},
                               {"right", "pb_shirt"},
                               {"top", "pb_shirt"},
                               {"bottom", "pb_shirt"}}));
    // The four limbs (the shared-tile convention: both arms pa_arm,
    // both legs pl_pants — the Minecraft convention).
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
        const bool isArm = l.name[0] == 'a';
        EXPECT_EQ(box->texture, isArm ? "pa_arm" : "pl_pants") << l.name;
        // V2-2: the arms take the sleeve/hand tile on all six faces;
        // the legs take the pants tile on every face EXCEPT the bottom
        // cap — the sole (pl_boot).
        if (isArm) {
            EXPECT_EQ(box->faces, (std::map<std::string, std::string>{
                                       {"front", "pa_arm"},
                                       {"back", "pa_arm"},
                                       {"left", "pa_arm"},
                                       {"right", "pa_arm"},
                                       {"top", "pa_arm"},
                                       {"bottom", "pa_arm"}}))
                << l.name;
        } else {
            EXPECT_EQ(box->faces, (std::map<std::string, std::string>{
                                       {"front", "pl_pants"},
                                       {"back", "pl_pants"},
                                       {"left", "pl_pants"},
                                       {"right", "pl_pants"},
                                       {"top", "pl_pants"},
                                       {"bottom", "pl_boot"}}))
                << l.name;
        }
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
    // The seven V2-2 per-face tiles have their appended cells in the
    // atlas layout (the farmland_wet GEN-9 append-only precedent: the
    // row-5 free run [9..15, 5] — the retired UX-3 cells [10..13, 4]
    // were NOT re-homed, the append went to the free slots).
    AssetManager assets;
    ASSERT_TRUE(assets.setRoot("assets").ok())
        << "run the tests from the repository root";
    auto text = assets.loadText("data/textures/block_atlas.json");
    ASSERT_TRUE(text.ok());
    auto def = Json::parse(*text);
    ASSERT_TRUE(def.ok());
    const Json* tiles = def->find("tiles");
    ASSERT_NE(tiles, nullptr);
    const struct {
        const char* name;
        int col;
        int row;
    } exp[] = {
        {"ph_face", 9, 5},  // head front (the Steve face, 2x)
        {"ph_hair", 10, 5}, // head back (hair; top/bottom reuse)
        {"ph_side", 11, 5}, // head right (hair + ear; left reuses)
        {"pb_shirt", 12, 5}, // body (all six faces reuse)
        {"pa_arm", 13, 5},   // arm (all six faces reuse)
        {"pl_pants", 14, 5}, // leg (five faces reuse)
        {"pl_boot", 15, 5},  // leg bottom cap (the sole)
    };
    for (const auto& e : exp) {
        const Json* cell = tiles->find(e.name);
        ASSERT_NE(cell, nullptr) << e.name;
        ASSERT_TRUE(cell->isArray() && cell->size() == 2);
        EXPECT_EQ(static_cast<int>(cell->at(0).asDouble(0)), e.col)
            << e.name;
        EXPECT_EQ(static_cast<int>(cell->at(1).asDouble(0)), e.row)
            << e.name;
    }
}

TEST(PlayerBodyTest, LegacyAtlasEntriesUnshifted) {
    // The V2-2 red line, per entry: the 90 surviving legacy entries
    // (the pre-V2-2 94 minus the four retired UX-3 player tiles) keep
    // their EXACT cells — zero UV shift for the whole shipped world
    // (the byte-stable baseline's atlas half). The retired four are
    // ABSENT (the generator no longer writes the PNGs; the layout no
    // longer names the cells).
    AssetManager assets;
    ASSERT_TRUE(assets.setRoot("assets").ok())
        << "run the tests from the repository root";
    auto text = assets.loadText("data/textures/block_atlas.json");
    ASSERT_TRUE(text.ok());
    auto def = Json::parse(*text);
    ASSERT_TRUE(def.ok());
    const Json* tiles = def->find("tiles");
    ASSERT_NE(tiles, nullptr);
    const struct {
        const char* name;
        int col;
        int row;
    } legacy[] = {
        {"bedrock", 0, 0},          {"coal_ore", 1, 0},       {"diamond_ore", 2, 0},
        {"dirt", 3, 0},             {"glass", 0, 1},          {"glowstone", 1, 1},
        {"gold_ore", 2, 1},         {"grass_side", 3, 1},     {"grass_top", 0, 2},
        {"iron_ore", 1, 2},         {"leaves", 2, 2},         {"sand", 3, 2},
        {"stone", 0, 3},            {"water", 1, 3},          {"wood_side", 2, 3},
        {"wood_top", 3, 3},         {"crafting_table_side", 0, 4},
        {"crafting_table_top", 1, 4}, {"planks", 2, 4},      {"zombie_head", 3, 4},
        {"zombie_body", 0, 5},      {"cow_body", 1, 5},      {"cow_head", 2, 5},
        {"sheep_body", 3, 5},       {"sheep_head", 0, 6},    {"chicken_body", 1, 6},
        {"chicken_head", 2, 6},     {"skeleton_body", 3, 6}, {"skeleton_head", 0, 7},
        {"furnace_side", 1, 7},     {"furnace_top", 2, 7},   {"wool", 3, 7},
        {"spider_body", 5, 7},      {"spider_head", 6, 7},   {"wool_red", 4, 0},
        {"wool_yellow", 5, 0},      {"wool_orange", 6, 0},   {"wool_pink", 7, 0},
        {"wool_brown", 4, 1},       {"wool_gray", 5, 1},     {"wool_black", 6, 1},
        {"wool_silver", 7, 1},      {"lapis_ore", 4, 2},     {"cactus", 4, 3},
        {"allium", 4, 4},           {"tulip", 4, 5},         {"cornflower", 4, 6},
        {"wool_blue", 4, 7},        {"wool_green", 5, 2},    {"wool_purple", 5, 3},
        {"wool_magenta", 5, 4},     {"wool_light_blue", 5, 5}, {"wool_cyan", 5, 6},
        {"wheat_1", 6, 2},          {"wheat_2", 6, 3},       {"wheat_3", 6, 4},
        {"wheat_4", 6, 5},          {"wheat_5", 6, 6},       {"wheat_6", 7, 2},
        {"wheat_7", 7, 3},          {"farmland_top", 7, 4},  {"farmland_side", 7, 5},
        {"lava", 7, 6},             {"honey", 15, 1},        {"obsidian", 7, 7},
        {"gravel", 8, 0},           {"cobblestone", 9, 0},   {"fire", 10, 0},
        {"torch", 11, 0},           {"campfire", 12, 0},     {"campfire_extinguished", 13, 0},
        {"sandstone", 14, 0},       {"smooth_sandstone", 15, 0}, {"iron_block", 8, 1},
        {"beacon", 9, 1},           {"fence", 8, 2},         {"glass_pane", 8, 3},
        {"chicken_legs", 8, 4},     {"cow_legs", 8, 5},      {"sheep_legs", 8, 6},
        {"oak_sapling", 10, 1},     {"spruce_sapling", 11, 1}, {"birch_sapling", 12, 1},
        {"spruce_trunk_side", 13, 1}, {"spruce_trunk_top", 14, 1},
        {"birch_trunk_side", 10, 2}, {"birch_trunk_top", 11, 2},
        {"spruce_leaves", 12, 2},   {"birch_leaves", 13, 2}, {"farmland_wet", 9, 4},
    };
    ASSERT_EQ(sizeof(legacy) / sizeof(legacy[0]), 90u); // the full census
    for (const auto& e : legacy) {
        const Json* cell = tiles->find(e.name);
        ASSERT_NE(cell, nullptr) << e.name;
        ASSERT_TRUE(cell->isArray() && cell->size() == 2);
        EXPECT_EQ(static_cast<int>(cell->at(0).asDouble(0)), e.col) << e.name;
        EXPECT_EQ(static_cast<int>(cell->at(1).asDouble(0)), e.row) << e.name;
    }
    for (const char* retired : {"player_head", "player_body", "player_arm",
                                "player_leg"}) {
        EXPECT_EQ(tiles->find(retired), nullptr) << retired;
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
    // The eight V2-2 files are byte-stable (re-running
    // tools/generate_player_skin.py must not change them — the script
    // prints the md5s; the x2-run identity is its self-proof).
    const struct {
        const char* path;
        const char* fnv;
    } newFiles[] = {
        {"assets/textures/player/player_skin.png", "ee33f13977b5eb69"},
        {"assets/textures/mobs/ph_face.png", "de62f0e3a6c9f0f1"},
        {"assets/textures/mobs/ph_hair.png", "838ea486bb1cd1aa"},
        {"assets/textures/mobs/ph_side.png", "c40981d1b0cd1128"},
        {"assets/textures/mobs/pb_shirt.png", "d4e2167d4e5922bf"},
        {"assets/textures/mobs/pa_arm.png", "1cfc2b33a972ee04"},
        {"assets/textures/mobs/pl_pants.png", "2a3363da12b37577"},
        {"assets/textures/mobs/pl_boot.png", "631efdd3bbae4e54"},
    };
    for (const auto& f : newFiles) {
        EXPECT_EQ(fnv1a64(readAll(f.path)), f.fnv) << f.path;
    }
    // The four retired UX-3 grid tiles are GONE from disk (the generator
    // no longer writes them; the layout no longer names them — the
    // upload's both-directions cross-check would fail otherwise).
    for (const char* retired : {"player_head", "player_body", "player_arm",
                                "player_leg"}) {
        EXPECT_FALSE(
            std::filesystem::exists(
                std::string("assets/textures/mobs/") + retired + ".png"))
            << retired;
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

TEST(PlayerBodyTest, V25PigMobTilesPinned) {
    // V2-5: the two generated pig tiles are byte-stable (re-running
    // tools/generate_mob_textures.py must not change them — the body tile
    // is the new salt 0x5011 pink + noise, the face is the explicit 8x8
    // grid 2x-upscaled, zero hash); the 22 pre-V2-5 tiles of this
    // directory stay pinned by SkinPngBytesPinned (the existing-assets
    // red line).
    const struct {
        const char* name;
        const char* fnv;
    } tiles[] = {
        {"pig_body", "417f84b406bbaee7"},
        {"pig_face", "d8a08749a6f6cc92"},
    };
    for (const auto& t : tiles) {
        EXPECT_EQ(fnv1a64(readAll(std::string("assets/textures/mobs/") +
                                   t.name + ".png")),
                  t.fnv)
            << t.name;
    }
}

TEST(PlayerBodyTest, V215SheepSkinTilePinned) {
    // V2-15: the generated bare-sheep skin tile is byte-stable (re-running
    // tools/generate_mob_textures.py must not change it — the new salt
    // 0x5EE4 pinkish-tan + noise, the sheep family's next slot; the three
    // existing sheep tiles + every other mob tile stay pinned by
    // SkinPngBytesPinned / V25PigMobTilesPinned — the zero-change red
    // line). The 15 dyed wool colors need NO new tile (the wool family's
    // existing block tiles, the atlas's zero-shift red line).
    EXPECT_EQ(fnv1a64(readAll("assets/textures/mobs/sheep_skin.png")),
              "247a2ad529474b20");
}
