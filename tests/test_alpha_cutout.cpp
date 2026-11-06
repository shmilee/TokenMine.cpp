// PRE-5 alpha cutout: the transparent-hole depth-write fix.
//
// Coverage:
//   - the block `cutout` property parsing (strict boolean, the
//     cutout-implies-transparent pairing rejected by name, default false)
//   - the SHIPPED content's exact cutout set (leaves family + the cross
//     plant family, pinned block by block) + the negative cases (glass /
//     the pane / the fluids stay in the blend pass — their textures carry
//     intermediate alpha)
//   - the mesher's three-way bucket routing (cube faces, cross quads, the
//     greedy-merge partition: a cutout face and a blend face sharing tile
//     + light never merge)
//   - the MeshVertex layout (the trailing cutout field at offset 56, the
//     60-byte stride, the default 0)
//   - the pure isPureCutoutImage census predicate
//   - the item-model geometry's cutout mark
//   - the render.alpha_test config key (code default true, the render.fog
//     precedent)
//   - V2-10: the full block-texture alpha census pin (every texture with
//     alpha-0 holes is routed on a discard-covered path — the cutout pass
//     for pure cutouts, the blend pass + the bucket-independent discard
//     for the intermediate-alpha glass; KNOWN_ISSUES 29 = PRE-5 full
//     coverage, zero render code)
#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <fstream>
#include <map>
#include <string>
#include <vector>

#include "block/block_registry.hpp"
#include "core/asset_manager.hpp"
#include "core/config.hpp"
#include "core/json.hpp"
#include "core/png.hpp"
#include "entity/item_model_mesh.hpp"
#include "item/item.hpp"
#include "renderer/alpha_test.hpp"
#include "renderer/mesh.hpp"
#include "renderer/texture_atlas.hpp"
#include "world/chunk_mesher.hpp"
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

std::string cubeDef(const std::string& id, const std::string& properties,
                    const std::string& textures) {
    return std::string("{\"id\":\"") + id +
           "\",\"properties\":" + properties +
           ",\"model\":{\"textures\":" + textures + "}}";
}

const char* kCubeTex = R"({"top":"t","side":"t","bottom":"t"})";

// stone (opaque) / glass (transparent, blend) / leaves (transparent +
// cutout) / a cross flower (transparent) / a cutout cross torch — the
// five routing cases of the three-way pass.
BlockRegistry makeRegistry() {
    BlockRegistry reg;
    auto stone = reg.registerBlock(
        parseDef(cubeDef("stone", "{}", kCubeTex)));
    auto glass = reg.registerBlock(parseDef(
        cubeDef("glass", R"({"opaque":false,"transparent":true,"solid":false})",
                kCubeTex)));
    auto leaves = reg.registerBlock(parseDef(
        cubeDef("leaves",
                R"({"opaque":false,"transparent":true,"solid":false,"cutout":true})",
                kCubeTex)));
    // A cross plant WITHOUT the cutout field (the pre-PRE-5 behavior: the
    // blend sub-mesh).
    auto flower = reg.registerBlock(parseDef(
        R"({"id":"flower","properties":{"hardness":0,"solid":false,"opaque":false,"transparent":true},"model":{"shape":"cross","textures":{"cross":"f"}}})"));
    // A cross plant WITH cutout (the shipped torch / flower pattern).
    auto torch = reg.registerBlock(parseDef(
        R"({"id":"torch","properties":{"hardness":0,"solid":false,"opaque":false,"transparent":true,"cutout":true},"model":{"shape":"cross","textures":{"cross":"f"}}})"));
    if (!stone.ok() || !glass.ok() || !leaves.ok() || !flower.ok() ||
        !torch.ok()) {
        ADD_FAILURE() << "test registry failed to build";
    }
    return reg;
}

// A 1-column strip atlas (power of two rows); each name gets its own row.
AtlasLayout makeLayout(const std::vector<std::string>& names) {
    int rows = 1;
    while (rows < static_cast<int>(names.size())) {
        rows *= 2;
    }
    std::string tiles = "{";
    for (std::size_t i = 0; i < names.size(); ++i) {
        if (i != 0) {
            tiles += ",";
        }
        tiles += "\"" + names[i] + "\":[0," + std::to_string(i) + "]";
    }
    tiles += "}";
    auto r = AtlasLayout::parse(parseDef(
        std::string("{\"columns\":1,\"rows\":") + std::to_string(rows) +
        ",\"tiles\":" + tiles + "}"));
    if (!r.ok()) {
        ADD_FAILURE() << r.error().message;
    }
    return *r;
}

ChunkMeshData mesh(const World& world, const BlockRegistry& reg,
                   const ChunkPos& pos) {
    return ChunkMesher(world, reg, makeLayout({"t", "f"})).mesh(pos);
}

// Counts the vertices carrying normal `n` (4 per emitted quad face).
int countVerticesWithNormal(const std::vector<MeshVertex>& verts,
                            const Vec3& n) {
    int count = 0;
    for (const MeshVertex& v : verts) {
        if (v.normal == n) {
            ++count;
        }
    }
    return count;
}

Item::ModelElement element(float x0, float y0, float z0, float x1, float y1,
                           float z1) {
    Item::ModelElement el;
    el.from = {x0, y0, z0};
    el.to = {x1, y1, z1};
    return el;
}

Item::ItemModel oneElementModel(Item::ModelElement el) {
    Item::ItemModel model;
    model.texture = "t";
    model.elements = {std::move(el)};
    return model;
}

} // namespace

// ---------------------------------------------------------------------------
// The block `cutout` property: strict parsing + the routing pairing.
// ---------------------------------------------------------------------------

TEST(AlphaCutoutPropertyTest, ParsesTrueAndDefaultsFalse) {
    BlockRegistry reg;
    auto r = reg.registerBlock(parseDef(
        cubeDef("a", R"({"opaque":false,"transparent":true,"cutout":true})",
                kCubeTex)));
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_TRUE(reg.findByName("a")->properties.cutout);
    // Absent = false (the pre-PRE-5 behavior, every field-less block).
    auto r2 = reg.registerBlock(parseDef(cubeDef("b", R"({"opaque":false,"transparent":true})", kCubeTex)));
    ASSERT_TRUE(r2.ok()) << r2.error().message;
    EXPECT_FALSE(reg.findByName("b")->properties.cutout);
}

TEST(AlphaCutoutPropertyTest, CutoutRequiresTransparent) {
    BlockRegistry reg;
    // cutout without transparent would route the geometry nowhere (the
    // cutout pass is a sub-pass of the transparent routing) — named.
    auto r = reg.registerBlock(
        parseDef(cubeDef("a", R"({"opaque":false,"cutout":true})", kCubeTex)));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("cutout"), std::string::npos);
    EXPECT_NE(r.error().message.find("transparent"), std::string::npos);
}

TEST(AlphaCutoutPropertyTest, NonBooleanIsRejectedByName) {
    BlockRegistry reg;
    auto r = reg.registerBlock(parseDef(
        cubeDef("a", R"({"opaque":false,"transparent":true,"cutout":"yes"})",
                kCubeTex)));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("cutout"), std::string::npos);
    EXPECT_NE(r.error().message.find("boolean"), std::string::npos);
}

TEST(AlphaCutoutPropertyTest, CutoutOnlyOnCubeAndCrossShapes) {
    // The other emitters keep their fixed bucket (fluids / the pane are
    // blend, slab / stairs / wall / fence are opaque) — a cutout flag on
    // them would be a dead field, rejected by name (the ammo discipline).
    const std::string pane =
        R"({"id":"pane","properties":{"opaque":false,"transparent":true,"cutout":true},"model":{"shape":"glass_pane","textures":{"side":"t"}}})";
    auto rp = BlockRegistry{}.registerBlock(parseDef(pane));
    ASSERT_FALSE(rp.ok());
    EXPECT_NE(rp.error().message.find("cutout"), std::string::npos);

    const std::string fluid = cubeDef(
        "wat", R"({"opaque":false,"transparent":true,"fluid":"water","solid":false,"cutout":true})",
        kCubeTex);
    auto rf = BlockRegistry{}.registerBlock(parseDef(fluid));
    ASSERT_FALSE(rf.ok());
    EXPECT_NE(rf.error().message.find("cutout"), std::string::npos);

    const std::string slab =
        R"({"id":"slab","properties":{"opaque":false,"transparent":true,"cutout":true},"model":{"shape":"slab","textures":{"top":"t","side":"t","bottom":"t"}}})";
    auto rs = BlockRegistry{}.registerBlock(parseDef(slab));
    ASSERT_FALSE(rs.ok());
    EXPECT_NE(rs.error().message.find("cutout"), std::string::npos);

    // The two honored shapes: a full cube and a cross.
    auto rc = BlockRegistry{}.registerBlock(
        parseDef(cubeDef("cub",
                         R"({"opaque":false,"transparent":true,"solid":false,"cutout":true})",
                         kCubeTex)));
    ASSERT_TRUE(rc.ok()) << rc.error().message;
    auto rx = BlockRegistry{}.registerBlock(parseDef(
        R"({"id":"crs","properties":{"solid":false,"opaque":false,"transparent":true,"cutout":true},"model":{"shape":"cross","textures":{"cross":"t"}}})"));
    ASSERT_TRUE(rx.ok()) << rx.error().message;
}

// ---------------------------------------------------------------------------
// The SHIPPED content's exact cutout set (the texture alpha census pinned
// in data: a block is cutout iff its texture's alpha set is ⊆ {0, 255}).
// ---------------------------------------------------------------------------

TEST(AlphaCutoutContentTest, ShippedCutoutSetIsExact) {
    AssetManager assets;
    ASSERT_TRUE(assets.setRoot("assets").ok())
        << "run the tests from the repository root";
    BlockRegistry reg;
    ASSERT_TRUE(reg.loadDirectory(assets, "data/blocks").ok());
    // The complete cutout set = the leaves family (3) + all the cross
    // plants (3 flowers + 7 wheat + 3 saplings + fire + torch + the campfire
    // family 2) = 20 blocks, nothing else.
    std::vector<std::string> cutouts;
    for (std::size_t i = 0; i < reg.count(); ++i) {
        const Block& b = reg.at(BlockID{static_cast<int>(i)});
        if (b.properties.cutout) {
            cutouts.push_back(b.name);
            EXPECT_TRUE(b.properties.transparent) << b.name; // the pairing
        }
    }
    std::sort(cutouts.begin(), cutouts.end());
    // V3-10b: 20 -> 21 (the chorus_plant's cross joins the cutout set).
    EXPECT_EQ(cutouts.size(), 21u);
    const std::vector<std::string> want = {
        "allium",            "birch_leaves", "birch_sapling", "campfire",
        "campfire_extinguished", "chorus_plant", "cornflower",   "fire",
        "leaves",            "oak_sapling", "spruce_leaves", "spruce_sapling",
        "torch",             "tulip",       "wheat_1",         "wheat_2",
        "wheat_3",           "wheat_4",     "wheat_5",         "wheat_6",
        "wheat_7",
    };
    EXPECT_EQ(cutouts, want);
}

TEST(AlphaCutoutContentTest, BlendPassBlocksAreNotCutout) {
    AssetManager assets;
    ASSERT_TRUE(assets.setRoot("assets").ok())
        << "run the tests from the repository root";
    BlockRegistry reg;
    ASSERT_TRUE(reg.loadDirectory(assets, "data/blocks").ok());
    // The negative cases — their textures carry intermediate alpha (glass
    // 220, the pane 90/200, water 200) so they keep the translucent-blend
    // pass (no depth write); their alpha-0 pixels are fixed by the
    // fragment's bucket-independent discard, not by the bucket routing.
    for (const char* name : {"glass", "glass_pane", "water", "lava", "cactus",
                             "stone"}) {
        const Block* b = reg.findByName(name);
        ASSERT_TRUE(b != nullptr) << name;
        EXPECT_FALSE(b->properties.cutout) << name;
    }
}

// V2-10 (KNOWN_ISSUES 29 = PRE-5 full coverage, zero render code): the
// FULL 16x16 block-texture alpha census pin. Every block texture carrying
// alpha-0 HOLES must be routed on a discard-covered path:
//   - a PURE cutout image (alpha set ⊆ {0, 255}) -> every referencing
//     block is cutout-routed (the cutout pass: discard active + depth
//     write on);
//   - an intermediate-alpha image (the glass: holes + a translucent
//     core) -> every referencing block stays in the blend pass, whose
//     draws carry the SAME fragment discard (u_discardEnabled is set on
//     the basic program per frame — bucket-independent), active under
//     the render.alpha_test CODE DEFAULT true (config.json carries no
//     override — zero config keys).
// A future texture that leaks alpha-0 holes onto an uncovered path fails
// this pin by name.
TEST(AlphaCutoutContentTest, EveryHoleTextureIsDiscardCovered) {
    AssetManager assets;
    ASSERT_TRUE(assets.setRoot("assets").ok())
        << "run the tests from the repository root";
    BlockRegistry reg;
    ASSERT_TRUE(reg.loadDirectory(assets, "data/blocks").ok());

    // Reverse index: texture name -> the referencing block names (every
    // model slot: the cube faces, the V2-7 per-face overrides, the cross,
    // the GEN-9 moisture top).
    std::map<std::string, std::vector<std::string>> blocksByTexture;
    for (std::size_t i = 0; i < reg.count(); ++i) {
        const Block& b = reg.at(BlockID{static_cast<int>(i)});
        const std::vector<std::string> slots = {
            b.model.textures.top,  b.model.textures.side,
            b.model.textures.bottom, b.model.textures.front,
            b.model.textures.back, b.model.textures.left,
            b.model.textures.right, b.model.textures.cross,
            b.model.moistureTop,
        };
        for (const std::string& s : slots) {
            if (!s.empty()) {
                blocksByTexture[s].push_back(b.name);
            }
        }
    }

    // The discard gate's code default, pinned two ways: a fresh config
    // resolves the getBool default, and the SHIPPED config carries no
    // override key (the zero-config red line — the code default is what
    // runs).
    {
        Config fresh;
        EXPECT_TRUE(fresh.getBool("render.alpha_test", true));
    }
    auto shipped = Config::load("config/config.json");
    ASSERT_TRUE(shipped.ok()) << shipped.error().message;
    EXPECT_EQ((*shipped).at("render.alpha_test"), nullptr);

    // The census: every PNG in textures/blocks (the 16x16 block
    // tiles — the directory and the block-referenced set are 1:1).
    auto files = assets.listDir("textures/blocks");
    ASSERT_TRUE(files.ok()) << files.error().message;
    int holeTextures = 0;
    int pureHoleTextures = 0;
    std::vector<std::string> nonPureHoleTextures;
    for (const std::string& file : *files) {
        if (file.size() < 4 || file.substr(file.size() - 4) != ".png") {
            continue;
        }
        const std::string name = file.substr(0, file.size() - 4);
        std::ifstream in(assets.root() / "textures/blocks" / file, std::ios::binary);
        ASSERT_TRUE(in) << "missing block texture: " << file;
        const std::string bytes(
            (std::istreambuf_iterator<char>(in)),
            std::istreambuf_iterator<char>());
        auto image = Png::decode(bytes);
        ASSERT_TRUE(image.ok()) << file << ": " << image.error().describe();
        int n0 = 0, nMid = 0;
        for (std::size_t i = 3; i < image->rgba.size(); i += 4) {
            const int a = image->rgba[i];
            if (a == 0) {
                ++n0;
            } else if (a < 255) {
                ++nMid;
            }
        }
        if (n0 == 0) {
            continue; // no holes — the discard never fires, any pass safe
        }
        ++holeTextures;
        const bool pure =
            isPureCutoutImage(image->rgba.data(), image->width,
                              image->height);
        EXPECT_EQ(pure, nMid == 0) << name; // the predicate and the census
        const auto it = blocksByTexture.find(name);
        ASSERT_NE(it, blocksByTexture.end())
            << name << " is on disk but no block references it";
        if (pure) {
            ++pureHoleTextures;
            // Pin: the CUTOUT pass (discard + depth write on).
            for (const std::string& b : it->second) {
                const Block* blk = reg.findByName(b);
                ASSERT_NE(blk, nullptr) << b;
                EXPECT_TRUE(blk->properties.cutout)
                    << name << " is a pure cutout image but '" << b
                    << "' leaves the cutout pass";
            }
        } else {
            nonPureHoleTextures.push_back(name);
            // Pin: the BLEND pass + the bucket-independent discard (the
            // alpha_test code default true pinned above).
            for (const std::string& b : it->second) {
                const Block* blk = reg.findByName(b);
                ASSERT_NE(blk, nullptr) << b;
                EXPECT_FALSE(blk->properties.cutout)
                    << name << " carries intermediate alpha — '" << b
                    << "' must stay in the blend pass";
            }
        }
    }
    // The v2.10.0 census, pinned: 21 hole textures = the 20 pure cutouts
    // (the leaves family + all the cross plants) + glass (the ONLY
    // intermediate-alpha hole texture — 187 holes + 9 core pixels).
    // water / glass_pane / honey are mid-only (zero holes, above the pin
    // line).
    // V3-10b: 21 -> 22 holes / 20 -> 21 pure (the chorus_plant's
    // cross — a hole-only texture, the wheat's class).
    EXPECT_EQ(holeTextures, 22);
    EXPECT_EQ(pureHoleTextures, 21);
    EXPECT_EQ(nonPureHoleTextures, (std::vector<std::string>{"glass"}));
}

// ---------------------------------------------------------------------------
// The mesher's three-way bucket routing.
// ---------------------------------------------------------------------------

TEST(AlphaCutoutMesherTest, CubeRoutesByCutout) {
    BlockRegistry reg = makeRegistry();
    World world;
    // A lone cutout cube (leaves): all six faces in the CUTOUT bucket,
    // nothing in the other two.
    world.setBlock(8, 4, 8, reg.idForName("leaves"));
    const auto d = mesh(world, reg, {0, 0, 0});
    EXPECT_EQ(d.cutoutVertices.size(), 24u);
    EXPECT_EQ(d.cutoutIndices.size(), 36u);
    EXPECT_TRUE(d.opaqueEmpty());
    EXPECT_TRUE(d.transparentEmpty());
}

TEST(AlphaCutoutMesherTest, GlassStaysInBlendBucket) {
    BlockRegistry reg = makeRegistry();
    World world;
    world.setBlock(8, 4, 8, reg.idForName("glass"));
    const auto d = mesh(world, reg, {0, 0, 0});
    EXPECT_EQ(d.transparentVertices.size(), 24u);
    EXPECT_TRUE(d.opaqueEmpty());
    EXPECT_TRUE(d.cutoutEmpty());
}

TEST(AlphaCutoutMesherTest, OpaqueRoutesToOpaqueBucket) {
    BlockRegistry reg = makeRegistry();
    World world;
    world.setBlock(8, 4, 8, reg.idForName("stone"));
    const auto d = mesh(world, reg, {0, 0, 0});
    EXPECT_EQ(d.opaqueVertices.size(), 24u);
    EXPECT_TRUE(d.cutoutEmpty());
    EXPECT_TRUE(d.transparentEmpty());
}

TEST(AlphaCutoutMesherTest, CrossRoutesByCutout) {
    BlockRegistry reg = makeRegistry();
    World world;
    // A cutout cross (the torch): two quads = 8 vertices in the CUTOUT
    // bucket, the blend bucket empty.
    world.setBlock(8, 4, 8, reg.idForName("torch"));
    const auto d = mesh(world, reg, {0, 0, 0});
    EXPECT_EQ(d.cutoutVertices.size(), 8u);
    EXPECT_EQ(d.cutoutIndices.size(), 12u);
    EXPECT_TRUE(d.transparentEmpty());
    EXPECT_TRUE(d.opaqueEmpty());
    // A non-cutout cross (the flower): the pre-PRE-5 blend bucket.
    World world2;
    world2.setBlock(8, 4, 8, reg.idForName("flower"));
    const auto d2 = mesh(world2, reg, {0, 0, 0});
    EXPECT_EQ(d2.transparentVertices.size(), 8u);
    EXPECT_TRUE(d2.cutoutEmpty());
}

TEST(AlphaCutoutMesherTest, CutoutAndBlendFacesDoNotMerge) {
    BlockRegistry reg = makeRegistry();
    World world;
    // A cutout cube next to a blend cube sharing the SAME tile + light:
    // without the cutout flag in the merge key they would flood-merge
    // into one 2x1 quad (and land in ONE bucket). With it, each keeps its
    // own quad in its own bucket — the +Z run is two 1x1 quads.
    world.setBlock(8, 4, 8, reg.idForName("leaves"));
    world.setBlock(9, 4, 8, reg.idForName("glass"));
    const auto d = mesh(world, reg, {0, 0, 0});
    const Vec3 plusZ {0.0f, 0.0f, 1.0f};
    EXPECT_EQ(countVerticesWithNormal(d.cutoutVertices, plusZ), 4);
    EXPECT_EQ(countVerticesWithNormal(d.transparentVertices, plusZ), 4);
    // The other five faces: each block's own (no shared-face culling
    // between two non-opaque blocks).
    EXPECT_EQ(d.cutoutVertices.size(), 24u);
    EXPECT_EQ(d.transparentVertices.size(), 24u);
    EXPECT_TRUE(d.opaqueEmpty());
}

// ---------------------------------------------------------------------------
// The MeshVertex layout: the trailing cutout field (56 -> 60 bytes).
// ---------------------------------------------------------------------------

TEST(AlphaCutoutVertexTest, CutoutFieldIsTrailingAtOffset56) {
    // The field order is the ABI: position/normal/uv/sky/block/uvLocal/
    // tiled/water/cutout. Pin the offsets + the packed size (a stray field
    // inserted BEFORE the tail would shift the layout and break every
    // existing mesh upload).
    EXPECT_EQ(offsetof(MeshVertex, position), 0u);
    EXPECT_EQ(offsetof(MeshVertex, water), 52u);
    EXPECT_EQ(offsetof(MeshVertex, cutout), 56u);
    EXPECT_EQ(sizeof(MeshVertex), 60u);
    static_assert(std::is_trivially_copyable_v<MeshVertex>);
    // The default constructor is 0 (every pre-PRE-5 producer is
    // byte-identical).
    const MeshVertex v;
    EXPECT_FLOAT_EQ(v.cutout, 0.0f);
    EXPECT_FLOAT_EQ(v.water, 0.0f);
    EXPECT_FLOAT_EQ(v.tiled, 0.0f);
}

// ---------------------------------------------------------------------------
// The pure image census (the init-time per-item alpha survey).
// ---------------------------------------------------------------------------

TEST(AlphaCutoutImageTest, IsPureCutoutByAlphaSet) {
    // {0, 255} mixed (the leaves / the item icons): a pure cutout.
    const std::uint8_t mixed[] = {
        255, 255, 255, 255, 0, 0, 0, 0,  // px0 opaque, px1 hole
        255, 255, 255, 0,     0, 0, 0, 255, // px2, px3
    };
    EXPECT_TRUE(isPureCutoutImage(mixed, 2, 2));
    // All-opaque (the planks / the stone): no holes, still pure (the
    // discard never fires, the depth write is correct).
    const std::uint8_t opaque[] = {255, 255, 255, 255, 255, 255, 255, 255};
    EXPECT_TRUE(isPureCutoutImage(opaque, 2, 1));
    // All-hole: degenerate but pure (every fragment discards).
    const std::uint8_t holes[] = {0, 0, 0, 0, 0, 0, 0, 0};
    EXPECT_TRUE(isPureCutoutImage(holes, 1, 2));
    // An intermediate alpha (the glass core 220, water 200, the pane 90):
    // a translucent surface — NOT a cutout (the blend pass).
    const std::uint8_t glass220[] = {255, 255, 255, 220, 255, 255, 255, 255};
    EXPECT_FALSE(isPureCutoutImage(glass220, 2, 1));
    const std::uint8_t water200[] = {255, 255, 255, 200, 255, 255, 255, 200};
    EXPECT_FALSE(isPureCutoutImage(water200, 2, 1));
    const std::uint8_t pane90[] = {255, 255, 255, 90, 255, 255, 255, 255};
    EXPECT_FALSE(isPureCutoutImage(pane90, 2, 1));
    // Empty / degenerate: not a pure cutout (there is nothing to route).
    EXPECT_FALSE(isPureCutoutImage(mixed, 0, 2));
    EXPECT_FALSE(isPureCutoutImage(nullptr, 2, 2));
}

// ---------------------------------------------------------------------------
// The item-model geometry's cutout mark (location 8).
// ---------------------------------------------------------------------------

TEST(AlphaCutoutItemModelTest, CutoutMarkOnVertices) {
    const Item::ItemModel m =
        oneElementModel(element(0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f));
    const ItemMeshGeometry marked = makeItemModelGeometry(m, true);
    ASSERT_EQ(marked.vertices.size(), 24u);
    for (const MeshVertex& v : marked.vertices) {
        EXPECT_FLOAT_EQ(v.cutout, 1.0f);
    }
    // The default (and a non-cutout icon): 0 on every vertex — the
    // pre-PRE-5 geometry byte-identical.
    const ItemMeshGeometry plain = makeItemModelGeometry(m, false);
    for (const MeshVertex& v : plain.vertices) {
        EXPECT_FLOAT_EQ(v.cutout, 0.0f);
    }
    // The mark never changes the geometry itself (positions / UVs /
    // lights are identical, the tail field only).
    for (std::size_t i = 0; i < marked.vertices.size(); ++i) {
        EXPECT_EQ(marked.vertices[i].position, plain.vertices[i].position);
        EXPECT_EQ(marked.vertices[i].uv, plain.vertices[i].uv);
        EXPECT_FLOAT_EQ(marked.vertices[i].skyLight,
                        plain.vertices[i].skyLight);
    }
    EXPECT_EQ(marked.indices, plain.indices);
}

// ---------------------------------------------------------------------------
// render.alpha_test: code default true (the render.fog / render.clouds
// precedent — config.json is untouched), explicit false = the v0.28.0
// pipeline (the gate's "new-off" control).
// ---------------------------------------------------------------------------

TEST(AlphaTestConfigTest, MissingKeyDefaultsToOn) {
    Config cfg;
    EXPECT_TRUE(cfg.getBool("render.alpha_test", true));
}

TEST(AlphaTestConfigTest, ExplicitFalseTurnsOff) {
    Config cfg;
    cfg.set("render.alpha_test", Json(false));
    EXPECT_FALSE(cfg.getBool("render.alpha_test", true));
}
