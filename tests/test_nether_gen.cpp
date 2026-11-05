// V3-1 Phase B (v3.4.0): the nether's world generation — the pure chunk
// fill (world/nether_gen: the flat slab world, a pure function of its
// coordinates — zero noise, zero randomness, constructive determinism),
// the derived seed (worldSeed ^ kSaltNether — never stored, the parse
// rebuilds it, the xp_seed single-source-of-truth twin) and the travel
// destination's landing pure function (core/portal's
// portalTravelDestination: the ±16 portal search, the generation
// fallback, the hitbox probe, the orthogonal yaw turn).
#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "block/block_registry.hpp"
#include "core/asset_manager.hpp"
#include "core/json.hpp"
#include "core/portal.hpp"
#include "item/item_registry.hpp"
#include "item/recipe.hpp"
#include "player/player.hpp"
#include "renderer/texture_atlas.hpp" // AtlasLayout (the census face)
#include "world/nether_gen.hpp"
#include "world/world.hpp"
#include "world/world_generator.hpp" // kLattice24Scale + kFeatureStep (the
                                      // roll oracle's vocabulary)

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

// The nether fill's content registry (netherrack + bedrock — the two
// band blocks; the lava is a fluid, not a block). The minimal v3.4.0
// form: NO content ids, NO biome pool — the content family degrades to
// inert (the band fill runs on, the layer-band tests' oracle).
NetherGen netherGenFixture() {
    BlockRegistry reg;
    const std::string netherrack =
        "{\"id\":\"netherrack\",\"properties\":{\"hardness\":0.4,"
        "\"drops\":\"netherrack\"},"
        "\"model\":{\"textures\":{\"top\":\"netherrack\",\"side\":\"netherrack\","
        "\"bottom\":\"netherrack\"}}}";
    const std::string bedrock =
        "{\"id\":\"bedrock\",\"properties\":{\"hardness\":0.0,"
        "\"destroyable\":false},"
        "\"model\":{\"textures\":{\"top\":\"bedrock\",\"side\":\"bedrock\","
        "\"bottom\":\"bedrock\"}}}";
    for (const std::string& def : {netherrack, bedrock}) {
        auto r = reg.registerBlock(parseDef(def));
        if (!r.ok()) {
            throw std::runtime_error(r.error().message);
        }
    }
    NetherGen gen;
    gen.netherrack = reg.idForName("netherrack");
    gen.bedrock = reg.idForName("bedrock");
    return gen;
}

// V3-2 Part 1: the FULL content fixture (all 14 nether block names + the
// five-biome pool) — the content passes' test home. The pool mirrors the
// shipped biomes_nether data (weights 36/22/17/16/9, the surfaces, the
// tree_density column probabilities); `seed` is the derived nether seed
// (the pass's salt-stream root).
NetherGen netherContentFixture(std::int64_t seed = 0) {
    BlockRegistry reg;
    const std::string netherrack =
        "{\"id\":\"netherrack\",\"properties\":{\"hardness\":0.4,"
        "\"drops\":\"netherrack\"},"
        "\"model\":{\"textures\":{\"top\":\"netherrack\",\"side\":\"netherrack\","
        "\"bottom\":\"netherrack\"}}}";
    const std::string bedrock =
        "{\"id\":\"bedrock\",\"properties\":{\"hardness\":0.0,"
        "\"destroyable\":false},"
        "\"model\":{\"textures\":{\"top\":\"bedrock\",\"side\":\"bedrock\","
        "\"bottom\":\"bedrock\"}}}";
    // The 12 content blocks (full-cube, stateless — the nether_gen.cpp's
    // data vocabulary; a minimal properties face, the band blocks' form).
    const struct {
        const char* name;
    } content[12] = {
        {"soul_sand"},       {"soul_soil"},        {"crimson_nylium"},
        {"warped_nylium"},   {"basalt"},           {"crimson_stem"},
        {"warped_stem"},     {"quartz_ore"},       {"nether_gold_ore"},
        {"ancient_debris"},  {"magma_block"},      {"glowstone"},
    };
    for (const std::string& def : {netherrack, bedrock}) {
        auto r = reg.registerBlock(parseDef(def));
        if (!r.ok()) {
            throw std::runtime_error(r.error().message);
        }
    }
    for (const auto& c : content) {
        const std::string def =
            std::string("{\"id\":\"") + c.name +
            "\",\"properties\":{\"hardness\":1.0,\"drops\":\"" + c.name +
            "\"},\"model\":{\"textures\":{\"top\":\"" + c.name +
            "\",\"side\":\"" + c.name + "\",\"bottom\":\"" + c.name +
            "\"}}}";
        auto r = reg.registerBlock(parseDef(def));
        if (!r.ok()) {
            throw std::runtime_error(r.error().message);
        }
    }
    BiomeRegistry biomes;
    const struct {
        const char* name;
        int weight;
        const char* surface;
        int depth;
        float treeDensity;
    } shipped[] = {
        {"basalt_deltas", 16, "netherrack", 3, 0.5f},
        {"crimson_forest", 22, "crimson_nylium", 1, 0.5f},
        {"nether_wastes", 36, "netherrack", 3, 0.0f},
        {"soul_sand_valley", 17, "soul_sand", 3, 0.0f},
        {"warped_forest", 9, "warped_nylium", 1, 0.5f},
    };
    // Registered through the real strict schema (the weight >= 1 / the
    // depth [1, 16] validation) so the fixture IS the shipped data's
    // shape (the tree_density is a default-0 schema field the shipped
    // files carry).
    for (const auto& b : shipped) {
        const std::string def =
            std::string("{\"id\":\"") + b.name + "\",\"weight\":" +
            std::to_string(b.weight) + ",\"surface\":\"" + b.surface +
            "\",\"subsurface\":\"netherrack\",\"subsurface_depth\":" +
            std::to_string(b.depth) + ",\"tree_density\":" +
            std::to_string(b.treeDensity) + "}";
        auto r = biomes.registerBiome(parseDef(def));
        if (!r.ok()) {
            throw std::runtime_error(r.error().message);
        }
    }
    auto gen = NetherGen::create(seed, biomes, reg);
    if (!gen.ok()) {
        throw std::runtime_error(gen.error().message);
    }
    return *gen;
}

// The nether's band at a world Y (the fill's layer rule — the test's
// expectation oracle, independent of the fill's band walk): the floor
// bedrock / the lava sea / the netherrack plateau / the air cavity / the
// roof bedrock (the v3.5.0 band revision).
enum class NetherBand {
    FloorBedrock,
    LavaSea,
    Netherrack,
    AirCavity,
    RoofBedrock
};
NetherBand bandAt(int wy) {
    if (wy <= kNetherFloorBedrockTop) {
        return NetherBand::FloorBedrock;
    }
    if (wy <= kNetherLavaSeaTop) {
        return NetherBand::LavaSea;
    }
    if (wy <= kNetherRockTop) {
        return NetherBand::Netherrack;
    }
    if (wy <= kNetherAirTop) {
        return NetherBand::AirCavity;
    }
    return NetherBand::RoofBedrock;
}

// Checks one chunk's cells against the band rule (the layer boundary's
// oracle: the block / fluid content at every world Y of the chunk).
void expectChunkBands(const Chunk& chunk, const ChunkPos& pos,
                      const NetherGen& gen) {
    const int baseY = pos.y * kChunkSize;
    for (int ly = 0; ly < kChunkSize; ++ly) {
        const int wy = baseY + ly;
        for (int lz = 0; lz < kChunkSize; ++lz) {
            for (int lx = 0; lx < kChunkSize; ++lx) {
                const NetherBand band = bandAt(wy);
                if (band == NetherBand::LavaSea) {
                    EXPECT_EQ(chunk.get(lx, ly, lz), BlockID::air())
                        << "y=" << wy;
                    EXPECT_EQ(chunk.fluidAt(lx, ly, lz),
                              fluidCode(FluidKind::Lava, kNetherLavaSeaLevel))
                        << "y=" << wy;
                } else if (band == NetherBand::AirCavity) {
                    EXPECT_EQ(chunk.get(lx, ly, lz), BlockID::air())
                        << "y=" << wy;
                    EXPECT_EQ(chunk.fluidAt(lx, ly, lz), 0) << "y=" << wy;
                } else if (band == NetherBand::Netherrack) {
                    EXPECT_EQ(chunk.get(lx, ly, lz), gen.netherrack)
                        << "y=" << wy;
                    EXPECT_EQ(chunk.fluidAt(lx, ly, lz), 0) << "y=" << wy;
                } else {
                    EXPECT_EQ(chunk.get(lx, ly, lz), gen.bedrock)
                        << "y=" << wy;
                    EXPECT_EQ(chunk.fluidAt(lx, ly, lz), 0) << "y=" << wy;
                }
            }
        }
    }
}

} // namespace

// ---------------------------------------------------------------------------
// The layer bands (the five boundary groups — the fill's layer rule).
// ---------------------------------------------------------------------------

TEST(NetherGenTest, LayerBands) {
    const NetherGen gen = netherGenFixture();
    // A chunk at the lava-sea / plateau boundary (world y 24..39 holds
    // both the sea's top (31) and the plateau's bottom (32)).
    World world;
    Chunk& chunk = world.getOrCreateChunk({0, 1, 0}); // y 16..31
    fillNetherChunk(gen, chunk, {0, 1, 0});
    expectChunkBands(chunk, {0, 1, 0}, gen);
    Chunk& chunk2 = world.getOrCreateChunk({0, 2, 0}); // y 32..47
    fillNetherChunk(gen, chunk2, {0, 2, 0});
    expectChunkBands(chunk2, {0, 2, 0}, gen);
    // A chunk at the plateau / air-cavity boundary (y 48..63: the
    // plateau's top 48..52 + the cavity's bottom 53..63).
    Chunk& chunk2b = world.getOrCreateChunk({0, 3, 0}); // y 48..63
    fillNetherChunk(gen, chunk2b, {0, 3, 0});
    expectChunkBands(chunk2b, {0, 3, 0}, gen);
    // The floor chunk (y 0..15: the bedrock floor 0..4 + the sea 5..15).
    Chunk& chunk3 = world.getOrCreateChunk({0, 0, 0});
    fillNetherChunk(gen, chunk3, {0, 0, 0});
    expectChunkBands(chunk3, {0, 0, 0}, gen);
    // The roof chunk (y 112..127: the cavity's top 112..122 + the roof
    // 123..127).
    Chunk& chunk4 = world.getOrCreateChunk({0, 7, 0});
    fillNetherChunk(gen, chunk4, {0, 7, 0});
    expectChunkBands(chunk4, {0, 7, 0}, gen);
}

TEST(NetherGenTest, LavaSeaTopEdge) {
    // The sea's TOP edge (y = 31): the sea's highest level — a source
    // (level 8, the static sea) at 31, the plateau's bottom at 32 (the
    // netherrack terrain the ores ride — the nether wastes' base).
    const NetherGen gen = netherGenFixture();
    World world;
    Chunk& chunk = world.getOrCreateChunk({0, 2, 0}); // y 32..47
    fillNetherChunk(gen, chunk, {0, 2, 0});
    Chunk& below = world.getOrCreateChunk({0, 1, 0}); // y 16..31
    fillNetherChunk(gen, below, {0, 1, 0});
    // y = 31 (local ly = 15 of the below chunk): the sea's top level.
    EXPECT_EQ(below.get(0, 15, 0), BlockID::air());
    EXPECT_EQ(below.fluidAt(0, 15, 0),
              fluidCode(FluidKind::Lava, kNetherLavaSeaLevel));
    // y = 32 (local ly = 0 of the chunk): the plateau's bottom.
    EXPECT_EQ(chunk.get(0, 0, 0), gen.netherrack);
    EXPECT_EQ(chunk.fluidAt(0, 0, 0), 0);
    // The sea is a FULL SOURCE everywhere (level 8 — no gradient, no
    // flow: the static-sea's zero churn, the recon's risk resolution).
    for (int ly = 0; ly < 16; ++ly) {
        for (int lz = 0; lz < 8; ++lz) {
            for (int lx = 0; lx < 8; ++lx) {
                const int wy = 16 + ly;
                if (wy >= kNetherLavaSeaBottom && wy <= kNetherLavaSeaTop) {
                    EXPECT_EQ(below.fluidAt(lx, ly, lz),
                              fluidCode(FluidKind::Lava,
                                        kNetherLavaSeaLevel))
                        << "y=" << wy;
                }
            }
        }
    }
}

TEST(NetherGenTest, BedrockFloorAndRoof) {
    // The bedrock floor (y 0..4) + the bedrock roof (y 123..127) — the
    // five-layer bands (the overworld's GEN-7 floor's solid-band twin).
    const NetherGen gen = netherGenFixture();
    World world;
    Chunk& floor = world.getOrCreateChunk({0, 0, 0});
    fillNetherChunk(gen, floor, {0, 0, 0});
    for (int y = kNetherFloorBedrockBottom; y <= kNetherFloorBedrockTop; ++y) {
        EXPECT_EQ(floor.get(3, y, 5), gen.bedrock) << "floor y=" << y;
    }
    Chunk& roof = world.getOrCreateChunk({0, 7, 0});
    fillNetherChunk(gen, roof, {0, 7, 0});
    for (int y = kNetherRoofBedrockBottom; y <= kNetherRoofBedrockTop; ++y) {
        const int ly = y - 7 * kChunkSize;
        EXPECT_EQ(roof.get(3, ly, 5), gen.bedrock) << "roof y=" << y;
    }
    // The band bounds are the named constants' contract (the kWorldHeight
    // 128 space — the v3.5.0 band revision's edges: the plateau tops out
    // where the air cavity begins, the cavity tops out under the roof).
    EXPECT_EQ(kNetherFloorBedrockTop + 1, kNetherLavaSeaBottom);
    EXPECT_EQ(kNetherLavaSeaTop + 1, kNetherRockBottom);
    EXPECT_EQ(kNetherRockTop + 1, kNetherAirBottom);
    EXPECT_EQ(kNetherAirTop + 1, kNetherRoofBedrockBottom);
    EXPECT_EQ(kNetherFloorBedrockBottom, 0);
    EXPECT_EQ(kNetherFloorBedrockTop, 4);
    EXPECT_EQ(kNetherLavaSeaBottom, 5);
    EXPECT_EQ(kNetherLavaSeaTop, 31);
    EXPECT_EQ(kNetherRockBottom, 32);
    EXPECT_EQ(kNetherRockTop, 52);
    EXPECT_EQ(kNetherAirBottom, 53);
    EXPECT_EQ(kNetherAirTop, 122);
    EXPECT_EQ(kNetherRoofBedrockBottom, 123);
    EXPECT_EQ(kNetherRoofBedrockTop, kWorldHeight - 1);
}

TEST(NetherGenTest, CrossChunkContinuity) {
    // The fill is a function of the WORLD Y alone (the flat slab): two
    // chunks sharing a face (vertical / horizontal neighbors) agree on
    // every shared-boundary cell — the world assembles seamlessly (the
    // WorldGenerator's cross-chunk consistency contract, the flat world's
    // constructive form).
    const NetherGen gen = netherGenFixture();
    World a, b;
    Chunk& ca = a.getOrCreateChunk({0, 2, 0});
    fillNetherChunk(gen, ca, {0, 2, 0});
    Chunk& cb = b.getOrCreateChunk({0, 3, 0}); // the vertical neighbor
    fillNetherChunk(gen, cb, {0, 3, 0});
    // The seam (y = 47 of ca == y = 48-1: ca's top row and cb's bottom
    // row are ADJACENT cells — both netherrack, the plateau's interior).
    for (int lz = 0; lz < kChunkSize; ++lz) {
        for (int lx = 0; lx < kChunkSize; ++lx) {
            EXPECT_EQ(ca.get(lx, 15, lz), gen.netherrack);
            EXPECT_EQ(cb.get(lx, 0, lz), gen.netherrack);
        }
    }
    Chunk& cc = b.getOrCreateChunk({1, 2, 0}); // the horizontal neighbor
    fillNetherChunk(gen, cc, {1, 2, 0});
    for (int ly = 0; ly < kChunkSize; ++ly) {
        for (int lz = 0; lz < kChunkSize; ++lz) {
            EXPECT_EQ(ca.get(15, ly, lz), cc.get(0, ly, lz));
        }
    }
}

TEST(NetherGenTest, NegativeCoordinates) {
    // The fill is column-independent: a chunk at a NEGATIVE x / z holds
    // the same world-Y bands (the flat world's constructive form — the
    // floorDiv discipline's worldgen twin: no lattice walk, no feature
    // that could break at a negative origin).
    const NetherGen gen = netherGenFixture();
    World world;
    Chunk& neg = world.getOrCreateChunk({-3, 2, -7});
    fillNetherChunk(gen, neg, {-3, 2, -7});
    expectChunkBands(neg, {-3, 2, -7}, gen);
    // A negative-Y-adjacent... the world's Y is always >= 0 (the chunks'
    // y in [0, 8)); the negative test is the X/Z space's.
    Chunk& neg2 = world.getOrCreateChunk({-1, 0, 0});
    fillNetherChunk(gen, neg2, {-1, 0, 0});
    expectChunkBands(neg2, {-1, 0, 0}, gen);
}

TEST(NetherGenTest, FillIsPure) {
    // Two fills of the same position (into two fresh chunks) are
    // identical cell-for-cell + fluid-for-fluid (the coordinate-purity —
    // no hidden state, no RNG, the thread-safety the streaming jobs
    // rely on).
    const NetherGen gen = netherGenFixture();
    Chunk a, b;
    fillNetherChunk(gen, a, {4, 3, -2});
    fillNetherChunk(gen, b, {4, 3, -2});
    for (int i = 0; i < Chunk::kVolume; ++i) {
        // The cell arrays are private; the accessors are the interface.
        const int lx = i % kChunkSize;
        const int ly = (i / kChunkSize) % kChunkSize;
        const int lz = i / (kChunkSize * kChunkSize);
        EXPECT_EQ(a.get(lx, ly, lz), b.get(lx, ly, lz)) << "i=" << i;
        EXPECT_EQ(a.fluidAt(lx, ly, lz), b.fluidAt(lx, ly, lz)) << "i=" << i;
    }
    // A different position's fill can differ (the Y bands) — the fill is
    // NOT a constant function: the floor chunk holds bedrock where the
    // plateau chunk holds netherrack (the same local cell, the different
    // world Y).
    Chunk c;
    fillNetherChunk(gen, c, {4, 0, -2}); // the floor chunk (y 0..15)
    EXPECT_EQ(c.get(0, 0, 0), gen.bedrock); // y = 0: the floor
    EXPECT_NE(c.get(0, 0, 0), a.get(0, 0, 0)); // y = 48: the plateau
    // And the roof chunk's bedrock (y = 123: local ly 11 of chunk 7)
    // differs from the cavity's air (y = 112: local ly 0).
    Chunk r;
    fillNetherChunk(gen, r, {4, 7, -2});
    EXPECT_EQ(r.get(0, 11, 0), gen.bedrock);
    EXPECT_EQ(r.get(0, 0, 0), BlockID::air());
}

// ---------------------------------------------------------------------------
// The derived seed (worldSeed ^ kSaltNether — never stored, the parse
// rebuilds it: the xp_seed's deterministic-migration twin).
// ---------------------------------------------------------------------------

TEST(NetherSeedTest, DerivedSeedIsXorOfTheSalt) {
    // The derivation is the XOR (its own inverse shape — the kXpSeedInit
    // precedent): the seed 0 maps to the salt itself, and the salt is a
    // named constant (not a literal at the call site — the unit test's
    // pin is the constant's identity, the S6 content-discipline twin).
    EXPECT_EQ(netherSeed(0), kSaltNether);
    EXPECT_EQ(netherSeed(42), 42 ^ kSaltNether);
    EXPECT_EQ(netherSeed(-7), -7 ^ kSaltNether);
    // A different overworld seed derives a different nether seed (the
    // XOR is a bijection — the worlds never share a generation seed).
    EXPECT_NE(netherSeed(1), netherSeed(2));
    // The salt is non-zero (a zero salt would make the nether's seed the
    // overworld's — the feature-noise correlation the salt discipline
    // exists to break).
    EXPECT_NE(kSaltNether, 0);
}

// ---------------------------------------------------------------------------
// The travel destination's landing pure function (portalTravelDestination
// — the design's D6 step 2/3: the ±16 search, the generation fallback,
// the bottom-center landing + hitbox probe, the orthogonal yaw turn).
// ---------------------------------------------------------------------------

namespace {

// The destination fixture registry (the portal test's obsidian / fire /
// portal / stone + the nether's bedrock — the room probe's solid).
struct DestFixture {
    BlockRegistry blocks;
    BlockID obsidian;
    BlockID fire;
    BlockID portal;
    BlockID stone;
    BlockID bedrock;
};

DestFixture destFixture() {
    DestFixture f;
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
    const std::string bedrock =
        "{\"id\":\"bedrock\",\"properties\":{\"hardness\":0.0,"
        "\"destroyable\":false},"
        "\"model\":{\"textures\":{\"top\":\"bedrock\",\"side\":\"bedrock\","
        "\"bottom\":\"bedrock\"}}}";
    for (const std::string& def : {obsidian, fire, portal, stone, bedrock}) {
        auto r = f.blocks.registerBlock(parseDef(def));
        if (!r.ok()) {
            throw std::runtime_error(r.error().message);
        }
    }
    f.obsidian = f.blocks.idForName("obsidian");
    f.fire = f.blocks.idForName("fire");
    f.portal = f.blocks.idForName("nether_portal");
    f.stone = f.blocks.idForName("stone");
    f.bedrock = f.blocks.idForName("bedrock");
    return f;
}

// Places an ACTIVE 4x5 frame (the corners included — the game's own
// generated portal's shape) with its interior in the portal state: the
// ring (14 obsidian) + the interior (6 portal cells, state = the axis).
void placeActiveFrame(World& world, const DestFixture& f, int axis,
                      int u0, int y0, int plane) {
    for (int u = u0; u < u0 + 4; ++u) {
        const BlockPos p = (axis == 0) ? BlockPos {u, y0, plane}
                                       : BlockPos {plane, y0, u};
        world.setBlock(p.x, p.y, p.z, f.obsidian);
        const BlockPos p2 = (axis == 0) ? BlockPos {u, y0 + 4, plane}
                                        : BlockPos {plane, y0 + 4, u};
        world.setBlock(p2.x, p2.y, p2.z, f.obsidian);
    }
    for (int y = y0 + 1; y <= y0 + 3; ++y) {
        const BlockPos a = (axis == 0) ? BlockPos {u0, y, plane}
                                       : BlockPos {plane, y, u0};
        world.setBlock(a.x, a.y, a.z, f.obsidian);
        const BlockPos b = (axis == 0) ? BlockPos {u0 + 3, y, plane}
                                       : BlockPos {plane, y, u0 + 3};
        world.setBlock(b.x, b.y, b.z, f.obsidian);
    }
    for (int y = y0 + 1; y <= y0 + 3; ++y) {
        for (int u = u0 + 1; u <= u0 + 2; ++u) {
            const BlockPos p = (axis == 0) ? BlockPos {u, y, plane}
                                           : BlockPos {plane, y, u};
            world.setBlock(p.x, p.y, p.z, f.portal);
            world.setBlockState(p.x, p.y, p.z, static_cast<std::uint8_t>(axis));
        }
    }
}

// The frame's interior cells (the tracking set's content).
std::vector<BlockPos> frameInteriorCells(int axis, int u0, int y0, int plane) {
    std::vector<BlockPos> out;
    for (int y = y0 + 1; y <= y0 + 3; ++y) {
        for (int u = u0 + 1; u <= u0 + 2; ++u) {
            out.push_back((axis == 0) ? BlockPos {u, y, plane}
                                      : BlockPos {plane, y, u});
        }
    }
    std::sort(out.begin(), out.end());
    return out;
}

PortalTravelQuery query(BlockPos mapped, int sourceAxis, float sourceYaw,
                        std::vector<BlockPos> cells, const World& world,
                        const DestFixture& f) {
    return PortalTravelQuery {
        mapped, sourceAxis, sourceYaw, std::move(cells), world, f.blocks,
        f.obsidian, f.fire, f.portal, 0.6f, 1.8f};
}

} // namespace

TEST(PortalTravelTest, SearchFindsTheNearestFrame) {
    // The ±16 search (any Y): an active frame at the mapped cell's +5 X /
    // -3 Z / +2 Y offset is found — the landing is the frame's bottom
    // center (the feet on the frame's bottom row), generated = false.
    auto f = destFixture();
    World world;
    world.getOrCreateChunk({0, 0, 0});
    world.getOrCreateChunk({0, 1, 0});
    placeActiveFrame(world, f, /*axis=*/0, /*u0=*/10, /*y0=*/34,
                     /*plane=*/5);
    const BlockPos mapped {15, 36, 8}; // the frame's bottom center is at
    // (u0+1.5, y0+1, plane+0.5) = (11.5, 35, 5.5) — within ±16 of the
    // mapped cell on X (11 vs 15) and Z (5 vs 8).
    const std::vector<BlockPos> cells = frameInteriorCells(0, 10, 34, 5);
    const PortalTravelPlan plan =
        portalTravelDestination(query(mapped, 0, 0.5f, cells, world, f));
    EXPECT_FALSE(plan.generated);
    EXPECT_EQ(plan.frame.axis, 0);
    EXPECT_EQ(plan.frame.lo, 10);
    EXPECT_EQ(plan.frame.y0, 34);
    EXPECT_EQ(plan.destination,
              portalFrameBottomCenter(plan.frame));
    // The yaw is kept (the same axis — no orthogonal turn).
    EXPECT_FLOAT_EQ(plan.yaw, 0.5f);
}

TEST(PortalTravelTest, SearchRadiusIsSixteen) {
    // A frame 16 cells away on X (inclusive) is FOUND; 17 cells away is
    // NOT (the kPortalSearchRadius's edge — the wiki's 32x32 box).
    auto f = destFixture();
    {
        World world;
        world.getOrCreateChunk({0, 0, 0});
        world.getOrCreateChunk({1, 0, 0});
        world.getOrCreateChunk({0, 1, 0});
        world.getOrCreateChunk({1, 1, 0});
        placeActiveFrame(world, f, 0, /*u0=*/0, /*y0=*/34, /*plane=*/0);
        const BlockPos mapped {16, 35, 1}; // the frame's bottom center is
        // at (1.5, 35, 0.5): |1.5 - 16.5| = 15 < 16... the cell
        // distance: the frame's u0 = 0..3, the mapped x = 16 — the
        // bottom center's x = 1.5; 16 - 1 = 15 (the mapped CELL's
        // distance to the frame's nearest cell, 16 - 3 = 13) — inside.
        const std::vector<BlockPos> cells = frameInteriorCells(0, 0, 34, 0);
        const PortalTravelPlan plan =
            portalTravelDestination(query(mapped, 0, 0.0f, cells, world, f));
        EXPECT_FALSE(plan.generated);
    }
    {
        World world;
        world.getOrCreateChunk({0, 0, 0});
        placeActiveFrame(world, f, 0, /*u0=*/0, /*y0=*/34, /*plane=*/0);
        // The mapped cell 17 away: the frame's cells (x 0..3) are all > 16
        // from the mapped x = 17 (17 - 3 = 14 < 16... the search is on the
        // CELL: |c.x - mapped.x| <= 16 — c.x = 3, mapped.x = 17: 14 <= 16
        // → the cell QUALIFIES. To be outside, mapped.x - c.x >= 17 for
        // every cell: c.x max = 3 → mapped.x = 20 (20 - 3 = 17 > 16).
        const BlockPos mapped {20, 35, 1};
        const std::vector<BlockPos> cells = frameInteriorCells(0, 0, 34, 0);
        const PortalTravelPlan plan =
            portalTravelDestination(query(mapped, 0, 0.0f, cells, world, f));
        EXPECT_TRUE(plan.generated);
    }
}

TEST(PortalTravelTest, NearestIncludesY) {
    // Two frames, both in range: the CLOSER one by Euclidean distance
    // (Y included — the wiki rule) wins: frame A is nearer on X but far
    // on Y; frame B is farther on X but at the mapped Y — B's 3D
    // distance is smaller.
    auto f = destFixture();
    World world;
    for (int cy = 0; cy < 3; ++cy) {
        world.getOrCreateChunk({0, cy, 0});
        world.getOrCreateChunk({1, cy, 0});
    }
    // Frame A: axis 0, u0 = 12 (bottom center x = 13.5), y0 = 33 (feet
    // y = 34), plane = 0 (z = 0.5).
    placeActiveFrame(world, f, 0, 12, 33, 0);
    // Frame B: axis 0, u0 = 16 (bottom center x = 17.5), y0 = 35 (feet
    // y = 36), plane = 2 (z = 2.5).
    placeActiveFrame(world, f, 0, 16, 35, 2);
    const BlockPos mapped {16, 36, 3};
    // The mapped POINT (16.5, 36, 3.5):
    //   A's bottom center (13.5, 34, 0.5): d² = 9 + 4 + 9 = 22.
    //   B's bottom center (17.5, 36, 2.5): d² = 1 + 0 + 1 = 2.
    // B wins (the Y-inclusive distance — an X-only nearest would pick A's
    // nearer X... A's nearest cell x = 12..15 vs B's 16..19: A is nearer
    // on X, B on Y — the 3D rule picks B).
    std::vector<BlockPos> cells = frameInteriorCells(0, 12, 33, 0);
    for (const BlockPos& c : frameInteriorCells(0, 16, 35, 2)) {
        cells.push_back(c);
    }
    std::sort(cells.begin(), cells.end());
    const PortalTravelPlan plan =
        portalTravelDestination(query(mapped, 0, 0.0f, cells, world, f));
    EXPECT_FALSE(plan.generated);
    EXPECT_EQ(plan.frame.lo, 16); // frame B
}

TEST(PortalTravelTest, GenerationFallbackAnchorsOnMapped) {
    // No frame in range -> the 4x5 corners-included frame anchored on the
    // mapped cell (the long edge along the SOURCE axis, the bottom row at
    // mapped.y - 1) + the generated flag (the game writes the ring / the
    // 2x3 bedrock platform / the interior).
    auto f = destFixture();
    World world;
    world.getOrCreateChunk({0, 2, 0});
    world.getOrCreateChunk({0, 3, 0});
    const BlockPos mapped {8, 55, 10};
    const PortalTravelPlan plan =
        portalTravelDestination(query(mapped, /*sourceAxis=*/0, 0.7f, {},
                                      world, f));
    EXPECT_TRUE(plan.generated);
    EXPECT_EQ(plan.frame.axis, 0);
    EXPECT_EQ(plan.frame.plane, 10);
    EXPECT_EQ(plan.frame.lo, 7); // mapped.x - 1
    EXPECT_EQ(plan.frame.hi, 10); // lo + 3 (the 4x5's long edge)
    EXPECT_EQ(plan.frame.y0, 54); // mapped.y - 1
    EXPECT_EQ(plan.frame.y1, 58); // y0 + 4 (the 4x5's height)
    // The landing = the generated frame's bottom center.
    EXPECT_EQ(plan.destination, portalFrameBottomCenter(plan.frame));
    EXPECT_FLOAT_EQ(plan.yaw, 0.7f); // the same axis: the yaw is kept
    // The platform (2x3 bedrock — the wiki's forced-generation platform,
    // the design's bedrock ruling): the interior's two long-edge cells x
    // the three across the plane, one below the frame's bottom row.
    const std::vector<BlockPos> platform = portalPlatformPlan(plan.frame);
    ASSERT_EQ(platform.size(), 6u);
    for (const BlockPos& p : platform) {
        EXPECT_EQ(p.y, 53); // y0 - 1
        EXPECT_TRUE(p.x == 8 || p.x == 9); // the interior's long edge
        EXPECT_TRUE(p.z >= 9 && p.z <= 11); // plane - 1 .. plane + 1
    }
    // The axis-1 anchor (the long edge along Z, the plane on X).
    const PortalTravelPlan planZ =
        portalTravelDestination(query(mapped, /*sourceAxis=*/1, 0.7f, {},
                                      world, f));
    EXPECT_TRUE(planZ.generated);
    EXPECT_EQ(planZ.frame.axis, 1);
    EXPECT_EQ(planZ.frame.plane, 8); // mapped.x
    EXPECT_EQ(planZ.frame.lo, 9); // mapped.z - 1
    EXPECT_EQ(planZ.frame.hi, 12);
    // The platform's 2x3 (the axis-1 twin: the z interior x the x across).
    const std::vector<BlockPos> platformZ = portalPlatformPlan(planZ.frame);
    ASSERT_EQ(platformZ.size(), 6u);
    for (const BlockPos& p : platformZ) {
        EXPECT_EQ(p.y, 53);
        EXPECT_TRUE(p.z == 10 || p.z == 11);
        EXPECT_TRUE(p.x >= 7 && p.x <= 9);
    }
}

TEST(PortalTravelTest, GenerationClampsTheBottomRow) {
    // A mapped y of 0 (the world floor): the bottom row clamps to y0 = 0
    // (the mapped.y - 1 = -1 is illegal — the frame's top still fits the
    // world, y1 = 4 < 128).
    auto f = destFixture();
    World world;
    world.getOrCreateChunk({0, 0, 0});
    const BlockPos mapped {2, 0, 2};
    const PortalTravelPlan plan =
        portalTravelDestination(query(mapped, 0, 0.0f, {}, world, f));
    EXPECT_TRUE(plan.generated);
    EXPECT_EQ(plan.frame.y0, 0);
    EXPECT_EQ(plan.frame.y1, 4);
    // The platform at y0 - 1 < 0 is EMPTY (the frame bottomed on the
    // world floor — the platform has nowhere to go).
    EXPECT_TRUE(portalPlatformPlan(plan.frame).empty());
    // The landing's room probe: the bottom center (y = 1) is roomy in the
    // air fixture (the frame's interior is non-solid) — the probe's
    // no-op.
    EXPECT_EQ(plan.destination, portalFrameBottomCenter(plan.frame));
}

TEST(PortalTravelTest, HitboxProbeStepsUp) {
    // The generated fallback against a world whose bottom-center cell is
    // solid (the adjacent geometry's intrusion): the probe steps up to
    // the first roomy level. (The probe's belt domain — for a FOUND
    // frame the interior is air / fire / portal only, none solid, and
    // the bottom center's AABB sits strictly inside the interior (the
    // width-4..23 / height-5..23 bounds), so a found frame's landing is
    // always roomy; the probe's live domain is the GENERATED frame's
    // pre-write world read, the frame's own cells not yet written.)
    auto f = destFixture();
    World world;
    world.getOrCreateChunk({0, 2, 0});
    world.getOrCreateChunk({0, 3, 0});
    // The generated frame (axis 0, mapped {8, 55, 10}) has its bottom
    // center at (8.5, 55, 10.5) — its AABB's cells (8, 55, 10) +
    // (8, 56, 10). A stone at the feet cell blocks the bottom center;
    // the y + 1 level (the cells (8, 56, 10) + (8, 57, 10)) is air.
    world.setBlock(8, 55, 10, f.stone);
    const PortalTravelPlan plan =
        portalTravelDestination(query({8, 55, 10}, /*sourceAxis=*/0, 0.0f,
                                      {}, world, f));
    EXPECT_TRUE(plan.generated);
    EXPECT_EQ(plan.frame.y0, 54);
    // The probe: y = 55 (the bottom center) is blocked, y = 56 roomy.
    // The x = the generated frame's interior center (lo + 2 = 7 + 2 —
    // the 4-wide frame's two interior cells' shared boundary, the
    // portal's exact center).
    EXPECT_FLOAT_EQ(plan.destination.y, 56.0f);
    EXPECT_FLOAT_EQ(plan.destination.x, 9.0f);
    EXPECT_FLOAT_EQ(plan.destination.z, 10.5f);
}

TEST(PortalTravelTest, OrthogonalPortalTurnsTheYaw) {
    // The destination frame's axis ORTHOGONAL to the source's: the yaw
    // turns 90° clockwise (the Minecraft JE rule — kPortalOrthoYawTurn =
    // -pi/2, the project's yaw convention: positive = left). Same axis:
    // the yaw is kept (the design's ruling — the wiki's round-trip
    // double-rotation quirk rides on this).
    auto f = destFixture();
    World world;
    world.getOrCreateChunk({0, 2, 0});
    world.getOrCreateChunk({0, 3, 0});
    placeActiveFrame(world, f, /*axis=*/1, /*u0=*/8, /*y0=*/54,
                     /*plane=*/10); // the z-long frame (axis 1)
    const std::vector<BlockPos> cells = frameInteriorCells(1, 8, 54, 10);
    // Source axis 0 (the x-long overworld portal) -> the nether's axis-1
    // frame: orthogonal -> the turn.
    const PortalTravelPlan turned =
        portalTravelDestination(query({9, 55, 10}, /*sourceAxis=*/0, 0.3f,
                                      cells, world, f));
    EXPECT_NEAR(turned.yaw, 0.3f - static_cast<float>(std::numbers::pi) / 2.0f,
                1e-6f);
    // Source axis 1 (the same as the destination) -> the yaw is kept.
    const PortalTravelPlan kept =
        portalTravelDestination(query({9, 55, 10}, /*sourceAxis=*/1, 0.3f,
                                      cells, world, f));
    EXPECT_FLOAT_EQ(kept.yaw, 0.3f);
}

// ===========================================================================
// V3-2 Part 1 (v3.5.0): the nether content family — the independent biome
// pool (netherBiomeAt), the four generation passes (biome surface / the
// nether ores / the decorations / the pass-order pin), the chunk-order
// independence and the content pins (12 blocks + 2 items + 2 recipes +
// the atlas 136 + the 5 biome files, the real-asset faces).
// ===========================================================================

namespace {

// The test-side roll oracle (the implementation's documented salt stream,
// re-derived here so a drift in either names itself): the 24-bit lattice
// value -> [0, 1) gate (kLattice24Scale, the overworld's shared form).
bool oracleRoll3(int a, int b, int c, std::int64_t salt, float p) {
    const std::uint64_t h =
        hash3(static_cast<std::uint64_t>(static_cast<std::int64_t>(a)),
              static_cast<std::uint64_t>(static_cast<std::int64_t>(b)),
              static_cast<std::uint64_t>(static_cast<std::int64_t>(c)),
              static_cast<std::uint64_t>(salt));
    return static_cast<float>(h >> 40) * kLattice24Scale < p;
}

// The JSON-def formatter (the fixture's "%s" name substitution — the
// block-def family's shared form).
std::string fmtDef(const std::string& def, const std::string& name) {
    std::string out;
    size_t i = 0;
    while (i < def.size()) {
        if (def.compare(i, 2, "%s") == 0) {
            out += name;
            i += 2;
        } else {
            out += def[i];
            ++i;
        }
    }
    return out;
}

// The ore point's oracle (the implementation's documented bit layout: the
// (h >> 12) / (h >> 20) / (h >> 28) in-cell uniform start, the per-ore
// salt stream (t + 1) * kFeatureStep on seed ^ kSaltNetherOre).
struct OrePoint {
    int vx = 0, vy = 0, vz = 0;
    bool placed = false;
    int t = -1; // the ore that wins the cell (-1 = none)
};

OrePoint orePointOracle(const NetherGen& gen, int i, int j, int k) {
    const int s = kNetherOreLattice;
    OrePoint out;
    for (int t = 0; t < 3; ++t) {
        const float p = (t == 0) ? kNetherQuartzOreProbability
                     : (t == 1) ? kNetherGoldOreProbability
                                : kNetherAncientDebrisProbability;
        const std::uint64_t h =
            hash3(static_cast<std::uint64_t>(static_cast<std::int64_t>(i)),
                  static_cast<std::uint64_t>(static_cast<std::int64_t>(j)),
                  static_cast<std::uint64_t>(static_cast<std::int64_t>(k)),
                  static_cast<std::uint64_t>(gen.seed ^ kSaltNetherOre) ^
                      (t + 1) * kFeatureStep);
        if (!(static_cast<float>(h >> 40) * kLattice24Scale < p)) {
            continue;
        }
        out.vx = i * s + static_cast<int>((h >> 12) % s);
        out.vy = j * s + static_cast<int>((h >> 20) % s);
        out.vz = k * s + static_cast<int>((h >> 28) % s);
        if (out.vy < kNetherOreBandBottom || out.vy > kNetherOreBandTop) {
            continue;
        }
        // The netherrack-only predicate: the surface pass runs first, so
        // the cell carries an ore only when the column's surface band does
        // NOT cover (out.vx, out.vy, out.vz) — the v3.5.0 band is the
        // plateau's TOP's downward depth.
        const std::size_t bi =
            netherBiomeAt(gen, out.vx, out.vz);
        const NetherBiome& b = gen.biomes[bi];
        const bool surfaceCovers =
            b.surface != gen.netherrack && b.surface.valid() &&
            out.vy > kNetherRockTop - b.surfaceDepth &&
            out.vy <= kNetherRockTop;
        if (surfaceCovers) {
            continue;
        }
        out.placed = true;
        out.t = t;
        break; // the t-order pin: the first passing ore owns the cell
    }
    return out;
}

// Fills a chunk disk with the content fixture (the region's tests' home).
void fillRegion(World& world, const NetherGen& gen, int x0, int x1, int y0,
                int y1, int z0, int z1) {
    for (int cy = y0; cy <= y1; ++cy) {
        for (int cz = z0; cz <= z1; ++cz) {
            for (int cx = x0; cx <= x1; ++cx) {
                Chunk& c = world.getOrCreateChunk({cx, cy, cz});
                fillNetherChunk(gen, c, {cx, cy, cz});
            }
        }
    }
}

// The lattice column's oracle (the 3a documented bit layout: the hash2
// roll on (the cell index, the cell index, the dedicated salt) — the
// probability gate + the exact height from the (h >> 24) bits).
struct ColumnOracle {
    bool placed = false;
    int baseY = 0;
    int height = 0;
};

ColumnOracle stemColumnOracle(const NetherGen& gen, int wx, int wz) {
    ColumnOracle out;
    if (floorDiv(wx, kNetherStemLattice) * kNetherStemLattice != wx ||
        floorDiv(wz, kNetherStemLattice) * kNetherStemLattice != wz) {
        return out; // not a lattice-4 aligned column
    }
    const NetherBiome& b = gen.biomes[netherBiomeAt(gen, wx, wz)];
    if (!b.stem.valid() || b.columnProbability <= 0.0f) {
        return out;
    }
    const std::int64_t salt = gen.seed ^ kSaltNetherStem;
    const std::uint64_t h = hash2(
        static_cast<std::uint64_t>(static_cast<std::int64_t>(
            floorDiv(wx, kNetherStemLattice))),
        static_cast<std::uint64_t>(static_cast<std::int64_t>(
            floorDiv(wz, kNetherStemLattice))),
        static_cast<std::uint64_t>(salt));
    if (!(static_cast<float>(h >> 40) * kLattice24Scale <
          b.columnProbability)) {
        return out;
    }
    out.placed = true;
    out.baseY = kNetherColumnBaseY; // the plateau's top + 1 (the cavity)
    out.height = kNetherStemHeightMin +
                 static_cast<int>((h >> 24) %
                                  static_cast<std::uint64_t>(
                                      kNetherStemHeightMax -
                                      kNetherStemHeightMin + 1));
    return out;
}

ColumnOracle basaltColumnOracle(const NetherGen& gen, int wx, int wz) {
    ColumnOracle out;
    if (floorDiv(wx, kNetherBasaltLattice) * kNetherBasaltLattice != wx ||
        floorDiv(wz, kNetherBasaltLattice) * kNetherBasaltLattice != wz) {
        return out; // not a lattice-8 aligned column
    }
    const NetherBiome& b = gen.biomes[netherBiomeAt(gen, wx, wz)];
    if (!b.basaltColumns || b.columnProbability <= 0.0f ||
        !gen.basalt.valid()) {
        return out;
    }
    const std::int64_t salt = gen.seed ^ kSaltNetherBasalt;
    const std::uint64_t h = hash2(
        static_cast<std::uint64_t>(static_cast<std::int64_t>(
            floorDiv(wx, kNetherBasaltLattice))),
        static_cast<std::uint64_t>(static_cast<std::int64_t>(
            floorDiv(wz, kNetherBasaltLattice))),
        static_cast<std::uint64_t>(salt));
    if (!(static_cast<float>(h >> 40) * kLattice24Scale <
          b.columnProbability)) {
        return out;
    }
    out.placed = true;
    out.baseY = kNetherColumnBaseY; // the plateau's top + 1 (the cavity)
    out.height = kNetherBasaltHeightMin +
                 static_cast<int>((h >> 24) %
                                  static_cast<std::uint64_t>(
                                      kNetherBasaltHeightMax -
                                      kNetherBasaltHeightMin + 1));
    return out;
}

// The 3b per-cell owner (the documented roll order: soul_soil first, the
// magma last-writer, the glowstone independent) — invalid = no 3b write.
BlockID perCellOwner(const NetherGen& gen, int wx, int wy, int wz) {
    const NetherBiome& b = gen.biomes[netherBiomeAt(gen, wx, wz)];
    if (b.soulFamily) {
        const bool magmaHit =
            wy >= kNetherMagmaBandBottom && wy <= kNetherMagmaBandTop &&
            oracleRoll3(wx, wz, wy, gen.seed ^ kSaltNetherMagma,
                        kNetherMagmaProbability);
        if (magmaHit && gen.magmaBlock.valid()) {
            return gen.magmaBlock;
        }
        const bool soulHit =
            wy >= kNetherSoulSoilBandBottom && wy <= kNetherSoulSoilBandTop &&
            oracleRoll3(wx, wz, wy, gen.seed ^ kSaltNetherSoulSoil,
                        kNetherSoulSoilProbability);
        if (soulHit && gen.soulSoil.valid()) {
            return gen.soulSoil;
        }
    }
    if (wy == kNetherGlowstoneY && gen.glowstone.valid() &&
        oracleRoll3(wx, wz, wy, gen.seed ^ kSaltNetherGlowstone,
                    kNetherGlowstoneProbability)) {
        return gen.glowstone;
    }
    return BlockID::invalid();
}

// The full decoration owner of a cell (3a then 3b — the last writer
// wins, the pass-order pin): invalid = no decoration wrote it.
BlockID decorationOwner(const NetherGen& gen, int wx, int wy, int wz) {
    BlockID owner = BlockID::invalid();
    // 3a: the lattice columns (the stem / the basalt — mutually exclusive
    // by biome, the flags' derivation).
    const ColumnOracle stem = stemColumnOracle(gen, wx, wz);
    if (stem.placed && wy >= stem.baseY && wy < stem.baseY + stem.height) {
        owner = gen.biomes[netherBiomeAt(gen, wx, wz)].stem;
    }
    const ColumnOracle basalt = basaltColumnOracle(gen, wx, wz);
    if (basalt.placed && wy >= basalt.baseY &&
        wy < basalt.baseY + basalt.height) {
        owner = gen.basalt;
    }
    // 3b: the per-cell owner (the LAST pass — it overwrites 3a).
    const BlockID pc = perCellOwner(gen, wx, wy, wz);
    if (pc.valid()) {
        owner = pc;
    }
    return owner;
}

// A column's biome (the fill's per-column selection — the test's oracle
// for the feature gates).
int columnBiome(const NetherGen& gen, int wx, int wz) {
    return static_cast<int>(netherBiomeAt(gen, wx, wz));
}

} // namespace

// ---------------------------------------------------------------------------
// netherBiomeAt (the biomeIndexAt clone over the nether pool).
// ---------------------------------------------------------------------------

TEST(NetherBiomeAtTest, DeterministicAndPoolBounded) {
    const NetherGen gen = netherContentFixture(42);
    // The selection is a pure function of (seed, x, z): repeated calls
    // agree, and the result is always a pool index.
    for (int i = 0; i < 50; ++i) {
        const int x = -100 + 7 * i;
        const int z = 33 * i - 50;
        const std::size_t a = netherBiomeAt(gen, x, z);
        const std::size_t b = netherBiomeAt(gen, x, z);
        EXPECT_EQ(a, b);
        EXPECT_LT(a, gen.biomes.size());
    }
    // A second gen with the SAME seed (a different instance) selects
    // identically (no hidden state).
    const NetherGen gen2 = netherContentFixture(42);
    for (int x = -20; x < 20; x += 3) {
        EXPECT_EQ(netherBiomeAt(gen, x, 11), netherBiomeAt(gen2, x, 11));
    }
}

TEST(NetherBiomeAtTest, DistributionBand100Columns) {
    // The design's distribution pin: 100 columns (a 10x10 grid at the
    // 200-block decorrelation spacing — the biome noise scale 0.02 makes
    // a 200-block step a ~4-noise-unit step, the samples' effective
    // independence) -> each biome's fraction within +/-10 percentage
    // points of its weight ratio (16/22/36/17/9 over the shipped load
    // order basalt/crimson/wastes/soul/warped).
    const NetherGen gen = netherContentFixture(42);
    const int targets[5] = {16, 22, 36, 17, 9}; // registry load order
    int counts[5] = {0, 0, 0, 0, 0};
    for (int gz = 0; gz < 10; ++gz) {
        for (int gx = 0; gx < 10; ++gx) {
            const int x = -900 + 200 * gx;
            const int z = -900 + 200 * gz;
            ++counts[netherBiomeAt(gen, x, z)];
        }
    }
    for (int i = 0; i < 5; ++i) {
        EXPECT_GE(counts[i], targets[i] - 10)
            << "biome " << gen.biomes[i].name;
        EXPECT_LE(counts[i], targets[i] + 10)
            << "biome " << gen.biomes[i].name;
    }
}

TEST(NetherBiomeAtTest, LoadOrderDrivesTheWalk) {
    // Two pools with the SAME weights in a DIFFERENT order select
    // differently on the sampled columns (the cumulative walk is the
    // registry load order, the overworld's twin) — and the single-biome
    // pool is degenerate (always index 0, the walk's total-weight edge).
    BiomeRegistry a, b;
    const std::string w = "{\"id\":\"w\",\"weight\":3,\"surface\":\"s\","
                          "\"subsurface\":\"s\"}";
    const std::string x = "{\"id\":\"x\",\"weight\":1,\"surface\":\"s\","
                          "\"subsurface\":\"s\"}";
    ASSERT_TRUE(a.registerBiome(parseDef(w)).ok());
    ASSERT_TRUE(a.registerBiome(parseDef(x)).ok());
    ASSERT_TRUE(b.registerBiome(parseDef(x)).ok());
    ASSERT_TRUE(b.registerBiome(parseDef(w)).ok());
    // The selection only needs the pool's weights / order (a manual gen —
    // the create() resolution's weights-only face).
    const auto makeGen = [](const BiomeRegistry& pool) {
        NetherGen g;
        g.seed = 7;
        for (const Biome& bb : pool.all()) {
            NetherBiome rb;
            rb.name = bb.name;
            rb.weight = bb.weight;
            g.biomes.push_back(std::move(rb));
        }
        for (const NetherBiome& bb : g.biomes) {
            g.totalWeight += bb.weight;
        }
        return g;
    };
    const NetherGen ga = makeGen(a);
    const NetherGen gb = makeGen(b);
    // Unequal weights (3/1): the cumulative walk's intervals are
    // order-sensitive (the indices differ, not just the names).
    int differ = 0;
    for (int gz = 0; gz < 10; ++gz) {
        for (int gx = 0; gx < 10; ++gx) {
            if (netherBiomeAt(ga, -900 + 200 * gx, -900 + 200 * gz) !=
                netherBiomeAt(gb, -900 + 200 * gx, -900 + 200 * gz)) {
                ++differ;
            }
        }
    }
    EXPECT_GT(differ, 20); // the order feeds the selection
    // The single-biome pool: the walk is degenerate.
    BiomeRegistry single;
    ASSERT_TRUE(single.registerBiome(parseDef(w)).ok());
    const NetherGen gs = makeGen(single);
    for (int xw = -20; xw < 20; xw += 3) {
        EXPECT_EQ(netherBiomeAt(gs, xw, 9), 0u);
    }
}

TEST(NetherBiomeAtTest, SaltAndSeedFeedTheSelection) {
    // The dedicated-salt / derived-seed discipline: a different world seed
    // derives a different nether seed and the selection changes on the
    // sampled columns (the nether's biome layout is NOT locked to the
    // overworld's — the salt independence), and the nether family's salts
    // are mutually distinct (a shared salt would correlate the features).
    const NetherGen g1 = netherContentFixture(100);
    const NetherGen g2 = netherContentFixture(200);
    int differ = 0;
    for (int gz = 0; gz < 10; ++gz) {
        for (int gx = 0; gx < 10; ++gx) {
            if (netherBiomeAt(g1, -900 + 200 * gx, -900 + 200 * gz) !=
                netherBiomeAt(g2, -900 + 200 * gx, -900 + 200 * gz)) {
                ++differ;
            }
        }
    }
    EXPECT_GT(differ, 40); // the majority of the columns differ
    EXPECT_NE(kSaltNetherBiome, 0);
    EXPECT_NE(kSaltNetherBiome, kSaltNether);
    EXPECT_NE(kSaltNetherBiome, kSaltNetherOre);
    EXPECT_NE(kSaltNetherBiome, kSaltNetherStem);
    // The nether's biome salt is not the overworld's kSaltBiome (the
    // dimension-form salt discipline).
    EXPECT_NE(kSaltNetherBiome, kSaltBiome);
}

// ---------------------------------------------------------------------------
// NetherGen::create (the resolution + the S6 named-rejection face).
// ---------------------------------------------------------------------------

TEST(NetherGenCreateTest, ResolvesTheFullContent) {
    const NetherGen gen = netherContentFixture(5);
    // All 14 content ids resolve (the bands + the 12 content blocks).
    EXPECT_TRUE(gen.netherrack.valid());
    EXPECT_TRUE(gen.bedrock.valid());
    EXPECT_TRUE(gen.soulSand.valid());
    EXPECT_TRUE(gen.soulSoil.valid());
    EXPECT_TRUE(gen.crimsonNylium.valid());
    EXPECT_TRUE(gen.warpedNylium.valid());
    EXPECT_TRUE(gen.basalt.valid());
    EXPECT_TRUE(gen.crimsonStem.valid());
    EXPECT_TRUE(gen.warpedStem.valid());
    EXPECT_TRUE(gen.quartzOre.valid());
    EXPECT_TRUE(gen.netherGoldOre.valid());
    EXPECT_TRUE(gen.ancientDebris.valid());
    EXPECT_TRUE(gen.magmaBlock.valid());
    EXPECT_TRUE(gen.glowstone.valid());
    EXPECT_TRUE(gen.valid());
    EXPECT_EQ(gen.biomes.size(), 5u);
    EXPECT_EQ(gen.totalWeight, 100);
    // The feature flags are derived from the biome's OWN data (the
    // data-driven gate): forests carry their nylium's sibling stem, the
    // basalt deltas the column flag, the soul valley the soul family, the
    // wastes nothing.
    const NetherBiome& basalt = gen.biomes[0]; // load order: basalt_deltas
    const NetherBiome& crimson = gen.biomes[1];
    const NetherBiome& wastes = gen.biomes[2];
    const NetherBiome& soul = gen.biomes[3];
    const NetherBiome& warped = gen.biomes[4];
    EXPECT_EQ(basalt.name, "basalt_deltas");
    EXPECT_TRUE(basalt.basaltColumns);
    EXPECT_FALSE(basalt.soulFamily);
    EXPECT_FALSE(basalt.stem.valid());
    EXPECT_EQ(crimson.name, "crimson_forest");
    EXPECT_EQ(crimson.stem, gen.crimsonStem);
    EXPECT_FALSE(crimson.basaltColumns);
    EXPECT_EQ(warped.stem, gen.warpedStem);
    EXPECT_FALSE(warped.basaltColumns);
    EXPECT_EQ(wastes.name, "nether_wastes");
    EXPECT_FALSE(wastes.basaltColumns);
    EXPECT_FALSE(wastes.soulFamily);
    EXPECT_FALSE(wastes.stem.valid());
    EXPECT_TRUE(soul.soulFamily);
    EXPECT_FALSE(soul.basaltColumns);
    EXPECT_FALSE(soul.stem.valid());
    EXPECT_EQ(soul.surfaceDepth, 3);
    EXPECT_EQ(crimson.surfaceDepth, 1);
}

TEST(NetherGenCreateTest, UnknownContentBlockNamed) {
    // A registry missing one of the 14 content names fails LOUDLY, naming
    // the block (the WorldGenerator::create's unknown-block discipline).
    BlockRegistry reg;
    const std::string def =
        "{\"id\":\"%s\",\"model\":{\"textures\":{\"top\":\"%s\","
        "\"side\":\"%s\",\"bottom\":\"%s\"}}}";
    const char* names[14] = {
        "netherrack",     "bedrock",         "soul_sand",
        "soul_soil",      "crimson_nylium",  "warped_nylium",
        "basalt",         "crimson_stem",    "warped_stem",
        "quartz_ore",     "nether_gold_ore", "ancient_debris",
        "magma_block",    "glowstone",
    };
    for (const char* n : names) {
        if (std::string(n) == "magma_block") {
            continue; // the named absence
        }
        auto r = reg.registerBlock(parseDef(fmtDef(def, n)));
        if (!r.ok()) {
            throw std::runtime_error(r.error().message);
        }
    }
    BiomeRegistry none;
    auto gen = NetherGen::create(1, none, reg);
    ASSERT_FALSE(gen.ok());
    EXPECT_NE(gen.error().message.find("magma_block"), std::string::npos);
}

TEST(NetherGenCreateTest, UnknownSurfaceBlockNamed) {
    // A biome naming an absent surface block fails LOUDLY, naming the
    // biome + the block (the S6 "surface unknown block" face).
    BlockRegistry reg;
    auto r = reg.registerBlock(parseDef(
        "{\"id\":\"netherrack\",\"model\":{\"textures\":{\"top\":\"n\","
        "\"side\":\"n\",\"bottom\":\"n\"}}}"));
    if (!r.ok()) {
        throw std::runtime_error(r.error().message);
    }
    for (const char* n : {"bedrock", "soul_sand", "soul_soil",
                          "crimson_nylium", "warped_nylium", "basalt",
                          "crimson_stem", "warped_stem", "quartz_ore",
                          "nether_gold_ore", "ancient_debris",
                          "magma_block", "glowstone"}) {
        auto rr = reg.registerBlock(parseDef(fmtDef(
            "{\"id\":\"%s\",\"model\":{\"textures\":{\"top\":\"n\","
            "\"side\":\"n\",\"bottom\":\"n\"}}}", n)));
        if (!rr.ok()) {
            throw std::runtime_error(rr.error().message);
        }
    }
    BiomeRegistry biomes;
    auto br = biomes.registerBiome(parseDef(
        "{\"id\":\"wastes\",\"weight\":1,\"surface\":\"void_block\","
        "\"subsurface\":\"netherrack\"}"));
    ASSERT_TRUE(br.ok());
    auto gen = NetherGen::create(1, biomes, reg);
    ASSERT_FALSE(gen.ok());
    EXPECT_NE(gen.error().message.find("void_block"), std::string::npos);
    EXPECT_NE(gen.error().message.find("wastes"), std::string::npos);
}

TEST(NetherGenCreateTest, StatefulSurfaceRejected) {
    // The ENT-2 twin: a biome whose surface is a STATEFUL block is
    // rejected by name (worldgen never writes a per-instance state — the
    // nether section's state-derivation completeness premise).
    BlockRegistry reg;
    for (const char* n : {"netherrack", "bedrock", "soul_sand", "soul_soil",
                          "crimson_nylium", "warped_nylium", "basalt",
                          "crimson_stem", "warped_stem", "quartz_ore",
                          "nether_gold_ore", "ancient_debris",
                          "magma_block", "glowstone", "stairish"}) {
        std::string def =
            std::string("{\"id\":\"") + n +
            "\",\"model\":{\"textures\":{\"top\":\"n\",\"side\":\"n\","
            "\"bottom\":\"n\"}}}";
        if (std::string(n) == "stairish") {
            def = "{\"id\":\"stairish\",\"model\":{\"textures\":"
                  "{\"top\":\"n\",\"side\":\"n\",\"bottom\":\"n\"},"
                  "\"state_count\":2,\"state_default\":0}}";
        }
        auto r = reg.registerBlock(parseDef(def));
        if (!r.ok()) {
            throw std::runtime_error(r.error().message);
        }
    }
    BiomeRegistry biomes;
    auto br = biomes.registerBiome(parseDef(
        "{\"id\":\"weird\",\"weight\":1,\"surface\":\"stairish\","
        "\"subsurface\":\"netherrack\"}"));
    ASSERT_TRUE(br.ok());
    auto gen = NetherGen::create(1, biomes, reg);
    ASSERT_FALSE(gen.ok());
    EXPECT_NE(gen.error().message.find("stairish"), std::string::npos);
    EXPECT_NE(gen.error().message.find("per-instance state"),
              std::string::npos);
}

TEST(NetherGenCreateTest, InitTimeFailFastNaming) {
    // The S6 timing pin (v3.5.0 revision): the surface's block-existence
    // + the stateful rejection run at BUILDSCENE INIT — buildScene
    // MC_TRYs exactly this NetherGen::create call (the
    // WorldGenerator::create's init-time twin) and caches the result in
    // m_nether.gen; ensureNether only consumes the cache (the lazy D2 is
    // the nether World, not the content resolution). The error names the
    // biome + the offending surface, so the init's "Initialization
    // failed:" line is diagnostic (a bad nether biome file rejects
    // startup, not the first travel).
    // The unknown-surface form (the b6_3 S6 scene: crimson_forest's
    // surface = 'crimson_wart_block'):
    {
        BlockRegistry reg;
        for (const char* n : {"netherrack", "bedrock", "soul_sand",
                              "soul_soil", "crimson_nylium",
                              "warped_nylium", "basalt", "crimson_stem",
                              "warped_stem", "quartz_ore",
                              "nether_gold_ore", "ancient_debris",
                              "magma_block", "glowstone"}) {
            auto r = reg.registerBlock(parseDef(fmtDef(
                "{\"id\":\"%s\",\"model\":{\"textures\":{\"top\":\"n\","
                "\"side\":\"n\",\"bottom\":\"n\"}}}", n)));
            if (!r.ok()) {
                throw std::runtime_error(r.error().message);
            }
        }
        BiomeRegistry biomes;
        auto br = biomes.registerBiome(parseDef(
            "{\"id\":\"crimson_forest\",\"weight\":22,"
            "\"surface\":\"crimson_wart_block\","
            "\"subsurface\":\"netherrack\"}"));
        ASSERT_TRUE(br.ok());
        auto gen = NetherGen::create(1, biomes, reg);
        ASSERT_FALSE(gen.ok());
        EXPECT_NE(gen.error().message.find("nether biome "
                                           "'crimson_forest'"),
                  std::string::npos);
        EXPECT_NE(gen.error().message.find("unknown surface block "
                                           "'crimson_wart_block'"),
                  std::string::npos);
    }
    // The stateful-surface form (the b6_5 S6 scene: soul_sand_valley's
    // surface = 'nether_portal'):
    {
        BlockRegistry reg;
        for (const char* n : {"netherrack", "bedrock", "soul_sand",
                              "soul_soil", "crimson_nylium",
                              "warped_nylium", "basalt", "crimson_stem",
                              "warped_stem", "quartz_ore",
                              "nether_gold_ore", "ancient_debris",
                              "magma_block", "glowstone", "nether_portal"}) {
            std::string def = fmtDef(
                "{\"id\":\"%s\",\"model\":{\"textures\":{\"top\":\"n\","
                "\"side\":\"n\",\"bottom\":\"n\"}}}", n);
            if (std::string(n) == "nether_portal") {
                def = "{\"id\":\"nether_portal\",\"model\":"
                      "{\"textures\":{\"top\":\"n\",\"side\":\"n\","
                      "\"bottom\":\"n\"},\"state_count\":2,"
                      "\"state_default\":0}}";
            }
            auto r = reg.registerBlock(parseDef(def));
            if (!r.ok()) {
                throw std::runtime_error(r.error().message);
            }
        }
        BiomeRegistry biomes;
        auto br = biomes.registerBiome(parseDef(
            "{\"id\":\"soul_sand_valley\",\"weight\":17,"
            "\"surface\":\"nether_portal\",\"subsurface\":\"netherrack\"}"));
        ASSERT_TRUE(br.ok());
        auto gen = NetherGen::create(1, biomes, reg);
        ASSERT_FALSE(gen.ok());
        EXPECT_NE(gen.error().message.find("soul_sand_valley"),
                  std::string::npos);
        EXPECT_NE(gen.error().message.find("nether_portal"),
                  std::string::npos);
        EXPECT_NE(gen.error().message.find("per-instance state"),
                  std::string::npos);
    }
}

TEST(NetherGenCreateTest, BiomeSchemaRejections) {
    // The S6 face over the nether pool (the shared strict schema): weight
    // 0 / the duplicate id / the unknown-key-free rejection all name
    // themselves (a nether biome file is a biome file — the shared
    // registerBiome's validation applies).
    BiomeRegistry reg;
    auto badWeight = reg.registerBiome(parseDef(
        "{\"id\":\"w\",\"weight\":0,\"surface\":\"s\",\"subsurface\":\"s\"}"));
    EXPECT_FALSE(badWeight.ok());
    EXPECT_NE(badWeight.error().message.find("weight"), std::string::npos);
    ASSERT_TRUE(reg.registerBiome(parseDef(
                     "{\"id\":\"w\",\"weight\":1,\"surface\":\"s\","
                     "\"subsurface\":\"s\"}"))
                    .ok());
    auto dup = reg.registerBiome(parseDef(
        "{\"id\":\"w\",\"weight\":2,\"surface\":\"s\",\"subsurface\":\"s\"}"));
    EXPECT_FALSE(dup.ok());
    EXPECT_NE(dup.error().message.find("duplicate biome id 'w'"),
              std::string::npos);
}

TEST(NetherGenCreateTest, EmptyPoolDegradesToTheSlab) {
    // The fixture degradation form: a content gen with an EMPTY biome pool
    // fills the pure v3.4.0 slab (the content family is inert — the
    // layer-band tests' oracle holds on the content fixture's bands too).
    BlockRegistry reg;
    // Reuse the content fixture's registry via a fresh create with an
    // empty pool: build the 14 blocks the same way, no biomes.
    const std::string def =
        "{\"id\":\"%s\",\"model\":{\"textures\":{\"top\":\"n\",\"side\":\"n\","
        "\"bottom\":\"n\"}}}";
    const char* names[14] = {
        "netherrack",     "bedrock",         "soul_sand",
        "soul_soil",      "crimson_nylium",  "warped_nylium",
        "basalt",         "crimson_stem",    "warped_stem",
        "quartz_ore",     "nether_gold_ore", "ancient_debris",
        "magma_block",    "glowstone",
    };
    for (const char* n : names) {
        auto r = reg.registerBlock(parseDef(fmtDef(def, n)));
        if (!r.ok()) {
            throw std::runtime_error(r.error().message);
        }
    }
    BiomeRegistry none;
    auto full = NetherGen::create(42, none, reg);
    ASSERT_TRUE(full.ok());
    // A bands-only gen (the minimal fixture) fills identically to the
    // empty-pool content gen (the content passes' no-op).
    const NetherGen minimal = netherGenFixture();
    World w1, w2;
    Chunk& c1 = w1.getOrCreateChunk({3, 2, -1});
    fillNetherChunk(*full, c1, {3, 2, -1});
    Chunk& c2 = w2.getOrCreateChunk({3, 2, -1});
    fillNetherChunk(minimal, c2, {3, 2, -1});
    for (int i = 0; i < Chunk::kVolume; ++i) {
        const int lx = i % kChunkSize;
        const int ly = (i / kChunkSize) % kChunkSize;
        const int lz = i / (kChunkSize * kChunkSize);
        EXPECT_EQ(w1.getBlock(3 * kChunkSize + lx, 32 + ly, -16 + lz),
                  w2.getBlock(3 * kChunkSize + lx, 32 + ly, -16 + lz))
            << "i=" << i;
    }
}

// ---------------------------------------------------------------------------
// Pass 1: the biome surface (the slab's bottom band override).
// ---------------------------------------------------------------------------

TEST(NetherSurfaceTest, BandOverridePerColumn) {
    // The per-column rule (the v3.5.0 top-anchored form): the plateau's
    // TOP band (kNetherRockTop - depth + 1, kNetherRockTop] is the biome's
    // surface block (a netherrack surface = the structural no-op). Checked
    // against the column's oracle biome over a chunk spanning the plateau's
    // top + the cavity's bottom (every band cell, all five biome classes).
    const NetherGen gen = netherContentFixture(42);
    World world;
    fillRegion(world, gen, 2, 2, 3, 3, 0, 0); // chunk x[32..47] z[0..15],
                                              // y[48..63] (the band + the
                                              // cavity's bottom)
    const int ox = 32, oz = 0;
    for (int lz = 0; lz < kChunkSize; ++lz) {
        for (int lx = 0; lx < kChunkSize; ++lx) {
            const NetherBiome& b =
                gen.biomes[columnBiome(gen, ox + lx, oz + lz)];
            const bool noOp = (b.surface == gen.netherrack) ||
                              !b.surface.valid();
            // The band's cells (the plateau's top downward).
            for (int d = 0; d < b.surfaceDepth; ++d) {
                const int wy = kNetherRockTop - d; // 52, 51, ...
                if (wy < 48 || wy > 63) {
                    continue; // outside this chunk's y span (depth <= 4 here)
                }
                // A 3b roll (soul_soil / magma) may overwrite the band cell
                // (the pass-order pin) — the surface OR the 3b owner.
                const BlockID got = world.getBlock(ox + lx, wy, oz + lz);
                const BlockID owner = perCellOwner(gen, ox + lx, wy, oz + lz);
                if (noOp) {
                    // The no-op baseline (netherrack) — an ore / a 3b roll
                    // may claim the cell.
                    EXPECT_TRUE(got == gen.netherrack ||
                                got == gen.quartzOre ||
                                got == gen.netherGoldOre ||
                                got == gen.ancientDebris ||
                                (owner.valid() && got == owner))
                        << "lx=" << lx << " lz=" << lz << " y=" << wy;
                } else {
                    EXPECT_TRUE(got == b.surface ||
                                (owner.valid() && got == owner))
                        << "lx=" << lx << " lz=" << lz << " y=" << wy;
                }
            }
        }
    }
    // The cavity's bottom (y = 53): air, except a stem / a basalt column's
    // base cell (the growth base = the plateau's top + 1).
    for (int lz = 0; lz < kChunkSize; ++lz) {
        for (int lx = 0; lx < kChunkSize; ++lx) {
            const BlockID got = world.getBlock(ox + lx, 53, oz + lz);
            const ColumnOracle stem = stemColumnOracle(gen, ox + lx, oz + lz);
            const ColumnOracle basalt = basaltColumnOracle(gen, ox + lx, oz + lz);
            const bool stemBase = stem.placed && 53 >= stem.baseY &&
                                  53 < stem.baseY + stem.height;
            const bool basaltBase =
                basalt.placed && 53 >= basalt.baseY &&
                53 < basalt.baseY + basalt.height;
            EXPECT_TRUE(got == BlockID::air() ||
                        (stemBase && got ==
                             gen.biomes[columnBiome(gen, ox + lx, oz + lz)]
                                 .stem) ||
                        (basaltBase && got == gen.basalt))
                << "cavity base lx=" << lx << " lz=" << lz;
        }
    }
}

TEST(NetherSurfaceTest, EveryBiomeClassAppears) {
    // The five shipped surfaces all carry their block: the selection's
    // existence over the decorrelation grid (all five biomes appear in
    // 100 columns at the fixed seed) + the fill's surface rule on one
    // found column per non-netherrack surface (the plateau's TOP cell,
    // y = 52, is the surface block — the v3.5.0 top-anchored band).
    const NetherGen gen = netherContentFixture(42);
    const BlockID surfaces[3] = {gen.soulSand, gen.crimsonNylium,
                                 gen.warpedNylium};
    const char* names[3] = {"soul_sand", "crimson_nylium", "warped_nylium"};
    for (int s = 0; s < 3; ++s) {
        int fx = 0, fz = 0;
        bool found = false;
        for (int gz = 0; gz < 10 && !found; ++gz) {
            for (int gx = 0; gx < 10 && !found; ++gx) {
                const int x = -900 + 200 * gx;
                const int z = -900 + 200 * gz;
                const NetherBiome& b = gen.biomes[netherBiomeAt(gen, x, z)];
                if (b.surface == surfaces[s]) {
                    fx = x;
                    fz = z;
                    found = true;
                }
            }
        }
        ASSERT_TRUE(found) << names[s];
        // The fill on the found column's chunk: the plateau's top cell
        // (y = kNetherRockTop) is the surface block, unless a 3b roll
        // overwrote it (the pass-order pin — the soul family's band
        // reaches y = 52).
        World world;
        const ChunkPos cp =
            worldToChunk(BlockPos{fx, kNetherRockTop, fz});
        Chunk& c = world.getOrCreateChunk(cp);
        fillNetherChunk(gen, c, cp);
        const BlockID got = world.getBlock(fx, kNetherRockTop, fz);
        const BlockID owner = perCellOwner(gen, fx, kNetherRockTop, fz);
        EXPECT_TRUE(got == surfaces[s] ||
                    (owner.valid() && got == owner))
            << names[s];
    }
}

// ---------------------------------------------------------------------------
// Pass 2: the nether ores (the lattice-8 single-cell rolls).
// ---------------------------------------------------------------------------

TEST(NetherOreTest, LatticeRollPin) {
    // The exact stream pin: a lattice cell whose oracle says "quartz at
    // (vx, vy, vz)" and no decoration overwrites — the filled chunk holds
    // the quartz there (the salt / the bit layout / the t-order all
    // named). Found over the lattice space at the fixed seed (a
    // deterministic hunt — the pin is the stream, not the luck).
    const NetherGen gen = netherContentFixture(42);
    bool pinned = false;
    for (int i = -12; i <= 12 && !pinned; ++i) {
        for (int j = 4; j <= 6 && !pinned; ++j) { // y lattice 32..52
            for (int k = -12; k <= 12 && !pinned; ++k) {
                const OrePoint p = orePointOracle(gen, i, j, k);
                if (!p.placed || p.t != 0) {
                    continue; // pin the QUARTZ stream (t = 0)
                }
                // The decoration's overwrite gate (the pass-order pin: a
                // stem / basalt column or a 3b roll may claim the cell).
                if (decorationOwner(gen, p.vx, p.vy, p.vz).valid()) {
                    continue;
                }
                World world;
                const ChunkPos cp = worldToChunk(BlockPos{p.vx, p.vy, p.vz});
                Chunk& c = world.getOrCreateChunk(cp);
                fillNetherChunk(gen, c, cp);
                EXPECT_EQ(world.getBlock(p.vx, p.vy, p.vz), gen.quartzOre)
                    << "cell (" << p.vx << "," << p.vy << "," << p.vz << ")";
                pinned = true;
            }
        }
    }
    EXPECT_TRUE(pinned); // the fixed seed carries an unoverwritten quartz
}

TEST(NetherOreTest, NetherrackOnlyPredicate) {
    // No ore in the lava sea / the air cavity / the bedrock (the ore band
    // is the plateau only), and no ore INSIDE a column's surface band
    // (the ore pass runs after the surface pass — a surface cell is not
    // netherrack).
    const NetherGen gen = netherContentFixture(42);
    World sea;
    fillRegion(sea, gen, 0, 0, 1, 1, 0, 0); // y 16..47 (the sea's top +
                                            // the plateau's bottom)
    for (int wy = 16; wy <= 31; ++wy) {
        for (int wz = 0; wz < 16; ++wz) {
            for (int wx = 0; wx < 16; ++wx) {
                EXPECT_EQ(sea.getBlock(wx, wy, wz), BlockID::air())
                    << "sea y=" << wy;
            }
        }
    }
    // The cavity / roof region (y 112..127): air up to 122, bedrock
    // 123..127 — no ore anywhere in it.
    World roof;
    fillRegion(roof, gen, 0, 0, 7, 7, 0, 0); // y 112..127
    for (int wy = 112; wy <= 127; ++wy) {
        for (int wz = 0; wz < 16; ++wz) {
            for (int wx = 0; wx < 16; ++wx) {
                const BlockID id = roof.getBlock(wx, wy, wz);
                EXPECT_NE(id, gen.quartzOre) << "cavity/roof y=" << wy;
                EXPECT_NE(id, gen.netherGoldOre) << "cavity/roof y=" << wy;
                EXPECT_NE(id, gen.ancientDebris) << "cavity/roof y=" << wy;
                if (wy >= 123) {
                    EXPECT_EQ(id, gen.bedrock) << "roof y=" << wy;
                } else {
                    EXPECT_TRUE(id == BlockID::air() ||
                                id == gen.glowstone)
                        << "cavity y=" << wy;
                }
            }
        }
    }
    // The surface-band exclusion: every ore cell over a wide plateau
    // region sits BELOW its column's surface band (the top-anchored band
    // occupies y 52-depth+1..52).
    World region;
    fillRegion(region, gen, -1, 1, 2, 3, -1, 1); // y 32..63 (the plateau +
                                                 // the cavity's bottom)
    for (int wy = 32; wy <= 52; ++wy) {
        for (int wz = -16; wz < 32; wz += 1) {
            for (int wx = -16; wx < 32; wx += 1) {
                const BlockID id = region.getBlock(wx, wy, wz);
                const bool isOre = (id == gen.quartzOre) ||
                                   (id == gen.netherGoldOre) ||
                                   (id == gen.ancientDebris);
                if (!isOre) {
                    continue;
                }
                const NetherBiome& b = gen.biomes[columnBiome(gen, wx, wz)];
                const bool inBand =
                    b.surface != gen.netherrack && b.surface.valid() &&
                    wy > kNetherRockTop - b.surfaceDepth &&
                    wy <= kNetherRockTop;
                EXPECT_FALSE(inBand)
                    << "ore (" << wx << "," << wy << "," << wz
                    << ") inside the surface band";
            }
        }
    }
}

TEST(NetherOreTest, BandBoundsAreThePlateau) {
    // The v3.5.0 band revision's constants: the ore band is the WHOLE
    // netherrack plateau (32..52 — the solid terrain), and the fill
    // honors it: no ore in the lava sea's edge (31) nor the cavity's
    // bottom edge (53).
    EXPECT_EQ(kNetherOreBandBottom, kNetherRockBottom);
    EXPECT_EQ(kNetherOreBandBottom, 32);
    EXPECT_EQ(kNetherOreBandTop, kNetherRockTop);
    EXPECT_EQ(kNetherOreBandTop, 52);
    const NetherGen gen = netherContentFixture(42);
    World world;
    fillRegion(world, gen, -8, 7, 2, 3, -8, 7); // 256x256, y 32..63
    // No ore at the band's edges (y = 31 the sea / y = 53 the cavity).
    for (int wy : {31, 53}) {
        for (int wz = -128; wz < 128; wz += 4) {
            for (int wx = -128; wx < 128; wx += 4) {
                const BlockID id = world.getBlock(wx, wy, wz);
                EXPECT_NE(id, gen.quartzOre) << "y=" << wy;
                EXPECT_NE(id, gen.netherGoldOre) << "y=" << wy;
                EXPECT_NE(id, gen.ancientDebris) << "y=" << wy;
            }
        }
    }
    // And the band IS worked: at least one ore cell exists at y = 32 and
    // at y = 52 (the plateau's edges — the full-band shape).
    bool edge32 = false, edge52 = false;
    for (int wy : {32, 52}) {
        for (int wz = -128; wz < 128 && !(edge32 && edge52); ++wz) {
            for (int wx = -128; wx < 128 && !(edge32 && edge52); ++wx) {
                const BlockID id = world.getBlock(wx, wy, wz);
                const bool isOre = (id == gen.quartzOre) ||
                                   (id == gen.netherGoldOre) ||
                                   (id == gen.ancientDebris);
                if (isOre) {
                    if (wy == 32) {
                        edge32 = true;
                    } else {
                        edge52 = true;
                    }
                }
            }
        }
    }
    EXPECT_TRUE(edge32) << "an ore cell at y = 32";
    EXPECT_TRUE(edge52) << "an ore cell at y = 52";
}

TEST(NetherOreTest, AncientDebrisRidesThePlateau) {
    // The v3.5.0 shape: ancient_debris cells exist INSIDE the plateau
    // (the band is the solid terrain 32..52 — no cell above it), pinned
    // over the region at the fixed seed.
    const NetherGen gen = netherContentFixture(42);
    World world;
    fillRegion(world, gen, -8, 7, 2, 3, -8, 7); // 256x256, y 32..63
    bool found = false;
    int foundY = -1;
    for (int wy = kNetherOreBandTop; wy >= kNetherOreBandBottom && !found;
         --wy) {
        for (int wz = -128; wz < 128 && !found; ++wz) {
            for (int wx = -128; wx < 128 && !found; ++wx) {
                if (world.getBlock(wx, wy, wz) == gen.ancientDebris) {
                    found = true;
                    foundY = wy;
                }
            }
        }
    }
    EXPECT_TRUE(found) << "an ancient_debris cell in the plateau";
    if (found) {
        EXPECT_GE(foundY, kNetherOreBandBottom);
        EXPECT_LE(foundY, kNetherOreBandTop);
    }
    // No debris in the cavity (the band's top edge holds).
    for (int wz = -128; wz < 128; wz += 5) {
        for (int wx = -128; wx < 128; wx += 5) {
            EXPECT_NE(world.getBlock(wx, 53, wz), gen.ancientDebris);
            EXPECT_NE(world.getBlock(wx, 63, wz), gen.ancientDebris);
        }
    }
}

TEST(NetherOreTest, OresExistAcrossTheFamily) {
    // The fixed-seed existence face: all three ores appear over the
    // plateau region (the probabilities' non-zero shape).
    const NetherGen gen = netherContentFixture(42);
    World world;
    fillRegion(world, gen, -8, 7, 2, 3, -8, 7); // 256x256, y 32..63
    int quartz = 0, gold = 0, debris = 0;
    for (int wy = kNetherOreBandBottom; wy <= kNetherOreBandTop; ++wy) {
        for (int wz = -128; wz < 128; ++wz) {
            for (int wx = -128; wx < 128; ++wx) {
                const BlockID id = world.getBlock(wx, wy, wz);
                if (id == gen.quartzOre) {
                    ++quartz;
                } else if (id == gen.netherGoldOre) {
                    ++gold;
                } else if (id == gen.ancientDebris) {
                    ++debris;
                }
            }
        }
    }
    EXPECT_GT(quartz, 0);
    EXPECT_GT(gold, 0);
    EXPECT_GT(debris, 0);
}

// ---------------------------------------------------------------------------
// Pass 3: the decorations (the lattice columns + the per-cell rolls).
// ---------------------------------------------------------------------------

TEST(NetherStemTest, ColumnsAlignedGatedAndRanged) {
    // The stem's column-aligned lattice-4 straight columns: the
    // deterministic hunt finds a PLACED column per forest (the aligned
    // column whose biome is the forest and whose roll passes — the wide
    // decorrelation sweep guarantees one at the fixed seed), then the
    // fill matches the oracle's exact base + height: the column sits on
    // the lattice, above the surface band, contiguous, in [4, 9].
    const NetherGen gen = netherContentFixture(42);
    const struct {
        BlockID block;
        const char* what;
    } forests[] = {
        {gen.crimsonStem, "crimson_stem"},
        {gen.warpedStem, "warped_stem"},
    };
    for (const auto& f : forests) {
        int fx = 0, fz = 0;
        ColumnOracle fo;
        bool found = false;
        for (int gz = -120; gz <= 120 && !found; ++gz) {
            for (int gx = -120; gx <= 120 && !found; ++gx) {
                const int x = kNetherStemLattice * gx; // lattice-aligned
                const int z = kNetherStemLattice * gz;
                const NetherBiome& b = gen.biomes[netherBiomeAt(gen, x, z)];
                if (b.stem != f.block) {
                    continue; // the biome gate
                }
                const ColumnOracle o = stemColumnOracle(gen, x, z);
                if (!o.placed) {
                    continue; // the roll missed
                }
                fx = x;
                fz = z;
                fo = o;
                found = true;
            }
        }
        ASSERT_TRUE(found) << f.what << " placed column at the fixed seed";
        World world;
        const ChunkPos cp = worldToChunk(BlockPos{fx, fo.baseY, fz});
        Chunk& c = world.getOrCreateChunk(cp);
        fillNetherChunk(gen, c, cp);
        // The column: the oracle's exact run (base..base+height-1), the
        // lattice alignment, the biome gate, the height's range.
        for (int t = 0; t < fo.height; ++t) {
            EXPECT_EQ(world.getBlock(fx, fo.baseY + t, fz), f.block)
                << f.what << " run t=" << t;
        }
        // Above the column: air (the cavity — the run's top edge).
        EXPECT_EQ(world.getBlock(fx, fo.baseY + fo.height, fz),
                  BlockID::air());
        // Below the column: the plateau's top cell (the nylium surface —
        // the stem roots ABOVE it, y = 52).
        EXPECT_EQ(world.getBlock(fx, fo.baseY - 1, fz),
                  gen.biomes[netherBiomeAt(gen, fx, fz)].surface);
        EXPECT_EQ(fo.baseY, kNetherColumnBaseY);
        EXPECT_EQ(floorDiv(fx, kNetherStemLattice) * kNetherStemLattice, fx);
        EXPECT_EQ(floorDiv(fz, kNetherStemLattice) * kNetherStemLattice, fz);
        EXPECT_GE(fo.height, kNetherStemHeightMin);
        EXPECT_LE(fo.height, kNetherStemHeightMax);
    }
}

TEST(NetherStemTest, NoStemBelowTheBand) {
    // The nylium surface stays intact under the stems: no stem cell at
    // or below y = 52 (the plateau's top — the stem's growth base is
    // 53) over the region.
    const NetherGen gen = netherContentFixture(42);
    World world;
    fillRegion(world, gen, -1, 1, 3, 3, -1, 1); // y 48..63
    for (int wz = -16; wz < 32; ++wz) {
        for (int wx = -16; wx < 32; ++wx) {
            EXPECT_NE(world.getBlock(wx, kNetherRockTop, wz),
                      gen.crimsonStem);
            EXPECT_NE(world.getBlock(wx, kNetherRockTop, wz),
                      gen.warpedStem);
            EXPECT_NE(world.getBlock(wx, kNetherRockTop - 1, wz),
                      gen.crimsonStem);
            EXPECT_NE(world.getBlock(wx, kNetherRockTop - 1, wz),
                      gen.warpedStem);
        }
    }
}

TEST(NetherBasaltTest, ColumnsFromY32RangedDeltasOnly) {
    // The basalt's column-aligned lattice-8 columns: the deterministic
    // hunt finds a PLACED column (the aligned basalt_deltas column whose
    // roll passes), then the fill matches the oracle's exact run: from
    // y = 32 (the slab's bottom — the deltas' signature), contiguous, in
    // [5, 30], the deltas-only gate.
    const NetherGen gen = netherContentFixture(42);
    int fx = 0, fz = 0;
    ColumnOracle fo;
    bool found = false;
    for (int gz = -120; gz <= 120 && !found; ++gz) {
        for (int gx = -120; gx <= 120 && !found; ++gx) {
            const int x = kNetherBasaltLattice * gx; // lattice-aligned
            const int z = kNetherBasaltLattice * gz;
            const NetherBiome& b = gen.biomes[netherBiomeAt(gen, x, z)];
            if (!b.basaltColumns) {
                continue; // the biome gate
            }
            const ColumnOracle o = basaltColumnOracle(gen, x, z);
            if (!o.placed) {
                continue;
            }
            fx = x;
            fz = z;
            fo = o;
            found = true;
        }
    }
    ASSERT_TRUE(found) << "a placed basalt column at the fixed seed";
    World world;
    const ChunkPos cp = worldToChunk(BlockPos{fx, 32, fz});
    for (int cy = 3; cy <= 5; ++cy) { // the column's full run (y 53..82)
        Chunk& c = world.getOrCreateChunk({cp.x, cy, cp.z});
        fillNetherChunk(gen, c, {cp.x, cy, cp.z});
    }
    for (int t = 0; t < fo.height; ++t) {
        EXPECT_EQ(world.getBlock(fx, kNetherColumnBaseY + t, fz),
                  gen.basalt)
            << "run t=" << t;
    }
    EXPECT_EQ(world.getBlock(fx, kNetherColumnBaseY + fo.height, fz),
              BlockID::air()); // the run's top edge (the cavity)
    EXPECT_EQ(world.getBlock(fx, kNetherColumnBaseY - 1, fz),
              gen.biomes[netherBiomeAt(gen, fx, fz)].surface);
    // The plateau's top stays the biome's surface (the basalt's growth
    // base is ABOVE it, y = 52).
    EXPECT_EQ(fo.baseY, kNetherColumnBaseY);
    EXPECT_EQ(kNetherColumnBaseY, kNetherAirBottom);
    EXPECT_GE(fo.height, kNetherBasaltHeightMin);
    EXPECT_LE(fo.height, kNetherBasaltHeightMax);
    // The deltas-only gate (the column's biome is the deltas).
    const NetherBiome& b = gen.biomes[netherBiomeAt(gen, fx, fz)];
    EXPECT_EQ(b.name, "basalt_deltas");
    EXPECT_TRUE(b.basaltColumns);
}

TEST(NetherSoulSoilTest, BandBiomeGateAndRollEquivalence) {
    // The soul_soil's per-cell roll: band [48, 52] (the plateau's top
    // band, the v3.5.0 revision), the soul valley only, and the
    // cell-level equivalence against the roll oracle (the last writer
    // wins: the magma roll overwrites a soul_soil hit). The chunk is
    // HUNTED to a soul-family column first (the deterministic discovery —
    // no luck in the region choice).
    const NetherGen gen = netherContentFixture(42);
    int fx = 0, fz = 0;
    bool found = false;
    for (int gz = -60; gz <= 60 && !found; ++gz) {
        for (int gx = -60; gx <= 60 && !found; ++gx) {
            const NetherBiome& b = gen.biomes[netherBiomeAt(gen, gx, gz)];
            if (b.soulFamily) {
                fx = gx;
                fz = gz;
                found = true;
            }
        }
    }
    ASSERT_TRUE(found) << "a soul-family column at the fixed seed";
    World world;
    const ChunkPos cp = worldToChunk(BlockPos{fx, kNetherRockTop, fz});
    Chunk& c = world.getOrCreateChunk(cp);
    fillNetherChunk(gen, c, cp); // y[48..63]: the band + the cavity's bottom
    const int ox = cp.x * kChunkSize, oz = cp.z * kChunkSize;
    for (int lz = 0; lz < kChunkSize; ++lz) {
        for (int lx = 0; lx < kChunkSize; ++lx) {
            const int wx = ox + lx, wz = oz + lz;
            const NetherBiome& b = gen.biomes[columnBiome(gen, wx, wz)];
            if (!b.soulFamily) {
                continue;
            }
            for (int wy = kNetherSoulSoilBandBottom;
                 wy <= kNetherMagmaBandTop; ++wy) {
                const BlockID got = world.getBlock(wx, wy, wz);
                // The 3b oracle (soul_soil first, magma last-writer):
                const bool soulHit =
                    wy >= kNetherSoulSoilBandBottom &&
                    wy <= kNetherSoulSoilBandTop &&
                    oracleRoll3(wx, wz, wy, gen.seed ^ kSaltNetherSoulSoil,
                                kNetherSoulSoilProbability);
                const bool magmaHit =
                    wy >= kNetherMagmaBandBottom &&
                    wy <= kNetherMagmaBandTop &&
                    oracleRoll3(wx, wz, wy, gen.seed ^ kSaltNetherMagma,
                                kNetherMagmaProbability);
                // 3b is the LAST pass: a hit owns the cell (it overwrites
                // the surface / the ore / the earlier roll — the pass-
                // order pin); a miss wrote nothing (the cell keeps its
                // pre-3b content, which can never be soul_soil / magma —
                // only 3b writes them).
                if (magmaHit) {
                    EXPECT_EQ(got, gen.magmaBlock)
                        << "cell (" << wx << "," << wy << "," << wz << ")";
                } else if (soulHit) {
                    EXPECT_EQ(got, gen.soulSoil)
                        << "cell (" << wx << "," << wy << "," << wz << ")";
                } else {
                    EXPECT_NE(got, gen.soulSoil)
                        << "cell (" << wx << "," << wy << "," << wz << ")";
                    EXPECT_NE(got, gen.magmaBlock)
                        << "cell (" << wx << "," << wy << "," << wz << ")";
                }
            }
        }
    }
}

TEST(NetherMagmaTest, BandAndBiomeGate) {
    // The magma's per-cell roll: band [48, 52] (the v3.5.0 revision), the
    // soul valley only, and it appears at the fixed seed (the existence
    // face — the hunt walks the soul-family columns until a chunk carries
    // a magma cell, the deterministic discovery).
    const NetherGen gen = netherContentFixture(42);
    // The soul-family columns (the wide sweep, the first 24).
    std::vector<std::pair<int, int>> soulCols;
    for (int gz = -500; gz <= 500 && soulCols.size() < 24; ++gz) {
        for (int gx = -500; gx <= 500 && soulCols.size() < 24; ++gx) {
            const NetherBiome& b = gen.biomes[netherBiomeAt(gen, gx, gz)];
            if (b.soulFamily) {
                soulCols.emplace_back(gx, gz);
            }
        }
    }
    ASSERT_FALSE(soulCols.empty());
    int count = 0;
    int edgeChecked = 0;
    for (const auto& col : soulCols) {
        if (count > 0) {
            break;
        }
        World world;
        const ChunkPos cp =
            worldToChunk(BlockPos{col.first, kNetherRockTop, col.second});
        Chunk& c = world.getOrCreateChunk(cp);
        fillNetherChunk(gen, c, cp); // y[48..63]
        const int ox = cp.x * kChunkSize, oz = cp.z * kChunkSize;
        for (int wy = kNetherMagmaBandBottom; wy <= kNetherMagmaBandTop;
             ++wy) {
            for (int lz = 0; lz < kChunkSize; ++lz) {
                for (int lx = 0; lx < kChunkSize; ++lx) {
                    const int wx = ox + lx, wz = oz + lz;
                    if (world.getBlock(wx, wy, wz) == gen.magmaBlock) {
                        ++count;
                        const NetherBiome& b =
                            gen.biomes[columnBiome(gen, wx, wz)];
                        EXPECT_TRUE(b.soulFamily);
                    }
                }
            }
        }
        // The band's edges (y = 47 below / y = 53 above): no magma.
        for (int lz = 0; lz < kChunkSize; ++lz) {
            for (int lx = 0; lx < kChunkSize; ++lx) {
                EXPECT_NE(world.getBlock(ox + lx, 47, oz + lz),
                          gen.magmaBlock);
                EXPECT_NE(world.getBlock(ox + lx, 53, oz + lz),
                          gen.magmaBlock);
            }
        }
        ++edgeChecked;
    }
    EXPECT_GT(count, 0) << "magma appears at the fixed seed";
    EXPECT_GE(edgeChecked, 1);
}

TEST(NetherGlowstoneTest, CeilingRowEquivalence) {
    // The ceiling glowstone's per-cell roll at y = 122 (all biomes — the
    // v3.5.0 revision: the cavity's top row, the roof's underside): the
    // row's cell-level equivalence against the roll oracle (a hit hangs
    // the glowstone in the AIR, a miss stays air — nothing else writes
    // the row: the ores' band tops at 52, the columns' runs end at 82)
    // + the existence face (a 256-cell row at p = 0.04 carries hits).
    const NetherGen gen = netherContentFixture(42);
    World world;
    fillRegion(world, gen, 1, 1, 7, 7, 1, 1); // the roof chunk y 112..127
    const int ox = 16, oz = 16;
    int glow = 0;
    for (int lz = 0; lz < kChunkSize; ++lz) {
        for (int lx = 0; lx < kChunkSize; ++lx) {
            const int wx = ox + lx, wz = oz + lz;
            const bool hit = oracleRoll3(
                wx, wz, kNetherGlowstoneY, gen.seed ^ kSaltNetherGlowstone,
                kNetherGlowstoneProbability);
            const BlockID got = world.getBlock(wx, kNetherGlowstoneY, wz);
            if (hit) {
                EXPECT_EQ(got, gen.glowstone) << "cell (" << wx << "," << wz
                                              << ")";
                ++glow;
            } else {
                // The cell keeps its cavity air (the glowstone's roll
                // missed).
                EXPECT_EQ(got, BlockID::air()) << "cell (" << wx << "," << wz
                                               << ")";
            }
        }
    }
    EXPECT_GT(glow, 0) << "glowstone carries the ceiling row";
    // The cavity's interior (y 112..121) stays air, the roof band itself
    // (y 123..127) stays bedrock.
    for (int lz = 0; lz < kChunkSize; ++lz) {
        for (int lx = 0; lx < kChunkSize; ++lx) {
            for (int wy = 112; wy <= 121; ++wy) {
                EXPECT_EQ(world.getBlock(ox + lx, wy, oz + lz), BlockID::air());
            }
            for (int wy = 123; wy <= 127; ++wy) {
                EXPECT_EQ(world.getBlock(ox + lx, wy, oz + lz), gen.bedrock);
            }
        }
    }
}

TEST(NetherPassOrderTest, DecorationsOverwriteTheEarlierPasses) {
    // The pass-order pin (surface -> ores -> decorations): the per-cell
    // rolls run LAST. (a) A 3b hit owns a SURFACE-BAND cell (the soul
    // sand's plateau-top band y 50..52 — the soul_soil / the magma
    // overwrites the surface); (b) within 3b, the magma roll is the last
    // writer (a co-hit cell holds the magma); (c) the ceiling glowstone
    // hangs from the cavity's AIR (cell-for-cell row rule — a hit is
    // glowstone, a miss is air).
    const NetherGen gen = netherContentFixture(42);
    // (a) + (b): hunt the soul-family columns' band cells for 3b hits —
    // the sweep walks CHUNKS (a filled chunk serves all its soul columns
    // at once) until both targets land (the co-hit's p = 0.05 x 0.01 per
    // cell is rare: the deterministic hunt at the fixed seed).
    int surfaceOverwrites = 0;
    int magmaLastWriters = 0;
    for (int cz = -50;
         cz <= 50 && (surfaceOverwrites < 3 || magmaLastWriters < 1);
         ++cz) {
        for (int cx = -50;
             cx <= 50 && (surfaceOverwrites < 3 || magmaLastWriters < 1);
             ++cx) {
            // The chunk's soul-family columns (skip the chunk if none).
            std::vector<std::pair<int, int>> cols;
            for (int lz = 0; lz < kChunkSize && cols.size() < 64; ++lz) {
                for (int lx = 0; lx < kChunkSize && cols.size() < 64; ++lx) {
                    const int wx = cx * kChunkSize + lx;
                    const int wz = cz * kChunkSize + lz;
                    const NetherBiome& b =
                        gen.biomes[netherBiomeAt(gen, wx, wz)];
                    if (b.soulFamily) {
                        cols.emplace_back(wx, wz);
                    }
                }
            }
            if (cols.empty()) {
                continue;
            }
            World world;
            const ChunkPos cp {cx, 3, cz}; // y[48..63]: the band's span
            Chunk& c = world.getOrCreateChunk(cp);
            fillNetherChunk(gen, c, cp);
            for (const auto& col : cols) {
                const NetherBiome& b =
                    gen.biomes[netherBiomeAt(gen, col.first, col.second)];
                for (int wy = kNetherSoulSoilBandBottom;
                     wy <= kNetherRockTop; ++wy) {
                    // The surface-band cells (the plateau's top downward).
                    if (wy > kNetherRockTop - b.surfaceDepth) {
                        const bool soul = oracleRoll3(
                            col.first, col.second, wy,
                            gen.seed ^ kSaltNetherSoulSoil,
                            kNetherSoulSoilProbability);
                        const bool magma = oracleRoll3(
                            col.first, col.second, wy,
                            gen.seed ^ kSaltNetherMagma,
                            kNetherMagmaProbability);
                        if (magma) {
                            EXPECT_EQ(
                                world.getBlock(col.first, wy, col.second),
                                gen.magmaBlock)
                                << "cell (" << col.first << "," << wy << ","
                                << col.second << ")";
                            if (soul) {
                                ++magmaLastWriters; // (b) the co-hit:
                                                    // magma wins
                            }
                            if (surfaceOverwrites < 3) {
                                ++surfaceOverwrites; // (a) 3b over the
                                                     // surface
                            }
                        } else if (soul) {
                            EXPECT_EQ(
                                world.getBlock(col.first, wy, col.second),
                                gen.soulSoil)
                                << "cell (" << col.first << "," << wy << ","
                                << col.second << ")";
                            if (surfaceOverwrites < 3) {
                                ++surfaceOverwrites;
                            }
                        }
                    }
                }
            }
        }
    }
    EXPECT_GE(surfaceOverwrites, 3)
        << "3b hits over the surface band at the fixed seed";
    EXPECT_GT(magmaLastWriters, 0)
        << "a magma-over-soul_soil co-hit at the fixed seed";
    // (c) the ceiling row (the cavity's air, the roof chunk's y = 122).
    World world;
    fillRegion(world, gen, 1, 1, 7, 7, 1, 1);
    int hits = 0;
    for (int wz = 16; wz < 32; ++wz) {
        for (int wx = 16; wx < 32; ++wx) {
            const bool glow = oracleRoll3(
                wx, wz, kNetherGlowstoneY, gen.seed ^ kSaltNetherGlowstone,
                kNetherGlowstoneProbability);
            const BlockID got = world.getBlock(wx, kNetherGlowstoneY, wz);
            if (glow) {
                EXPECT_EQ(got, gen.glowstone)
                    << "cell (" << wx << "," << wz << ")";
                ++hits;
            } else {
                EXPECT_EQ(got, BlockID::air())
                    << "cell (" << wx << "," << wz << ")";
            }
        }
    }
    EXPECT_GT(hits, 0);
}

// ---------------------------------------------------------------------------
// The fill's invariants (the purity / the order-independence / the bands).
// ---------------------------------------------------------------------------

TEST(NetherFillContentTest, ChunkOrderIndependent) {
    // Two fills of the SAME chunk set in DIFFERENT orders produce
    // identical chunks (the streaming jobs' arbitrary scheduling — no
    // pass reads a neighbor, so the order cannot matter; the pin is the
    // cell-for-cell identity over a 2x2x2 set with the content passes).
    const NetherGen gen = netherContentFixture(42);
    const ChunkPos set[8] = {
        {2, 2, 1}, {3, 2, 1}, {2, 2, 2}, {3, 2, 2},
        {2, 3, 1}, {3, 3, 1}, {2, 3, 2}, {3, 3, 2},
    };
    World a, b;
    for (const ChunkPos& p : set) {
        fillNetherChunk(gen, a.getOrCreateChunk(p), p);
    }
    for (int i = 7; i >= 0; --i) {
        fillNetherChunk(gen, b.getOrCreateChunk(set[i]), set[i]);
    }
    for (const ChunkPos& p : set) {
        for (int i = 0; i < Chunk::kVolume; ++i) {
            const int lx = i % kChunkSize;
            const int ly = (i / kChunkSize) % kChunkSize;
            const int lz = i / (kChunkSize * kChunkSize);
            const int wx = p.x * kChunkSize + lx;
            const int wy = p.y * kChunkSize + ly;
            const int wz = p.z * kChunkSize + lz;
            EXPECT_EQ(a.getBlock(wx, wy, wz), b.getBlock(wx, wy, wz))
                << "cell (" << wx << "," << wy << "," << wz << ")";
            EXPECT_EQ(a.getFluid(wx, wy, wz), b.getFluid(wx, wy, wz))
                << "cell (" << wx << "," << wy << "," << wz << ")";
        }
    }
}

TEST(NetherFillContentTest, FillIsPureWithContent) {
    // Two fills of the same position (the content passes live) are
    // identical cell-for-cell + fluid-for-fluid (the coordinate-purity —
    // the v3.4.0 FillIsPure's content-carrying form).
    const NetherGen gen = netherContentFixture(1234);
    Chunk a, b;
    fillNetherChunk(gen, a, {5, 3, -4});
    fillNetherChunk(gen, b, {5, 3, -4});
    for (int i = 0; i < Chunk::kVolume; ++i) {
        const int lx = i % kChunkSize;
        const int ly = (i / kChunkSize) % kChunkSize;
        const int lz = i / (kChunkSize * kChunkSize);
        EXPECT_EQ(a.get(lx, ly, lz), b.get(lx, ly, lz)) << "i=" << i;
        EXPECT_EQ(a.fluidAt(lx, ly, lz), b.fluidAt(lx, ly, lz)) << "i=" << i;
    }
}

TEST(NetherFillContentTest, NegativeCoordinatesContent) {
    // The content family at NEGATIVE x / z (the floorDiv discipline's
    // worldgen twin — the lattice walks + the hashes carry the negative
    // coordinates exactly like the overworld's): the surface rule holds
    // per column (the plateau's TOP band), the cavity holds (air + the
    // lattice columns), the bands hold.
    const NetherGen gen = netherContentFixture(42);
    World world;
    fillRegion(world, gen, -7, -6, 3, 3, -9, -8); // y[48..63] chunks
    const int ox = -7 * 16, oz = -9 * 16; // the (x,z) corner of the -7,-9
                                          // chunk's world span
    for (int lz = 0; lz < kChunkSize; ++lz) {
        for (int lx = 0; lx < kChunkSize; ++lx) {
            const NetherBiome& b =
                gen.biomes[columnBiome(gen, ox + lx, oz + lz)];
            const bool noOp = (b.surface == gen.netherrack) ||
                              !b.surface.valid();
            const auto isOre = [&](BlockID id) {
                return id == gen.quartzOre || id == gen.netherGoldOre ||
                       id == gen.ancientDebris;
            };
            const auto is3b = [&](BlockID id) {
                return id == gen.soulSoil || id == gen.magmaBlock;
            };
            // The plateau's interior (y 48..49, below the band): the
            // netherrack baseline (an ore / a 3b roll may claim it).
            for (int wy = 48; wy <= 49; ++wy) {
                const BlockID got = world.getBlock(ox + lx, wy, oz + lz);
                EXPECT_TRUE(got == gen.netherrack || isOre(got) ||
                            (b.soulFamily && is3b(got)))
                    << "cell (" << ox + lx << "," << wy << ","
                    << oz + lz << ")";
            }
            // The surface band (the plateau's top downward): the surface
            // (or the 3b's overwrite — the pass-order pin) — the no-op
            // columns keep the baseline (or an ore).
            for (int d = 0; d < b.surfaceDepth; ++d) {
                const int wy = kNetherRockTop - d;
                const BlockID got = world.getBlock(ox + lx, wy, oz + lz);
                if (noOp) {
                    EXPECT_TRUE(got == gen.netherrack || isOre(got) ||
                                is3b(got))
                        << "cell (" << ox + lx << "," << wy << ","
                        << oz + lz << ")";
                } else {
                    EXPECT_TRUE(got == b.surface || is3b(got))
                        << "cell (" << ox + lx << "," << wy << ","
                        << oz + lz << ")";
                }
            }
            // The cavity (y 53..63): air, except a lattice column's run
            // (the stem / the basalt — the 3a owner).
            for (int wy = 53; wy <= 63; ++wy) {
                const BlockID got = world.getBlock(ox + lx, wy, oz + lz);
                const ColumnOracle stem = stemColumnOracle(gen, ox + lx, oz + lz);
                const ColumnOracle basalt =
                    basaltColumnOracle(gen, ox + lx, oz + lz);
                const bool stemRun = stem.placed && wy >= stem.baseY &&
                                     wy < stem.baseY + stem.height;
                const bool basaltRun = basalt.placed && wy >= basalt.baseY &&
                                       wy < basalt.baseY + basalt.height;
                EXPECT_TRUE(got == BlockID::air() ||
                            (stemRun &&
                             got == b.stem) ||
                            (basaltRun && got == gen.basalt))
                    << "cavity cell (" << ox + lx << "," << wy << ","
                    << oz + lz << ")";
            }
        }
    }
}

TEST(NetherFillContentTest, LavaSeaStaysUntouched) {
    // The content family never touches the lava sea: the sea chunk (y
    // 16..31) with the full fixture holds air + the full source, zero
    // content blocks (the band's structural immunity).
    const NetherGen gen = netherContentFixture(42);
    World world;
    fillRegion(world, gen, 0, 0, 1, 1, 0, 0);
    for (int wy = 16; wy <= 31; ++wy) {
        for (int wz = 0; wz < 16; ++wz) {
            for (int wx = 0; wx < 16; ++wx) {
                EXPECT_EQ(world.getBlock(wx, wy, wz), BlockID::air());
                EXPECT_EQ(world.getFluidOfKind(wx, wy, wz, FluidKind::Lava),
                          kNetherLavaSeaLevel);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// The content pins (the REAL assets — the 12 blocks + the 2 items + the 2
// recipes + the atlas 136 + the 5 biome files, the S6 cross-check face).
// ---------------------------------------------------------------------------

namespace {

void realAssets(AssetManager& assets) {
    if (!assets.setRoot("assets").ok()) {
        throw std::runtime_error("run the tests from the repository root");
    }
}

} // namespace

TEST(NetherContentTest, RealTwelveBlocks) {
    // The 12 content blocks' field faces (the design's D3 pin): the
    // hardness / the drops / the sound material (T2 v3.7.0 — the v3.5.0
    // stone / grass reuse deviations' CLOSURE: nether = netherrack's
    // family is the NETHER material, the nylium pair the NYLIUM
    // material (the fieldless grass precedent replaced), the stem pair
    // the STEM material, the soul pair the SAND family, basalt /
    // nether_brick the STONE family — the Minecraft assignments), the
    // statelessness (the ENT-2 premise), the full-cube shape.
    AssetManager assets;
    realAssets(assets);
    BlockRegistry blocks;
    ASSERT_TRUE(blocks.loadDirectory(assets, "data/blocks").ok());
    const struct {
        const char* name;
        float hardness;
        const char* drops;
        const char* sound;
        float walkFactor; // T5 v3.7.0: the walk_speed_factor (1.0 =
                          // absent — the no-slowdown default)
    } want[] = {
        {"nether_brick", 2.0f, "nether_brick", "stone", 1.0f},
        {"magma_block", 1.5f, "magma_block", "nether", 1.0f},
        {"quartz_ore", 3.0f, "quartz", "stone", 1.0f},
        {"nether_gold_ore", 3.0f, "gold_ingot", "stone", 1.0f},
        {"ancient_debris", 10.0f, "ancient_debris", "nether", 1.0f},
        {"soul_sand", 0.5f, "soul_sand", "sand", 0.4f},
        {"soul_soil", 0.5f, "soul_soil", "sand", 1.0f},
        {"crimson_nylium", 0.4f, "crimson_nylium", "nylium", 1.0f},
        {"warped_nylium", 0.4f, "warped_nylium", "nylium", 1.0f},
        {"basalt", 1.2f, "basalt", "stone", 1.0f},
        {"crimson_stem", 0.4f, "crimson_stem", "stem", 1.0f},
        {"warped_stem", 0.4f, "warped_stem", "stem", 1.0f},
    };
    for (const auto& w : want) {
        const Block* b = blocks.findByName(w.name);
        ASSERT_NE(b, nullptr) << w.name;
        EXPECT_FLOAT_EQ(b->properties.hardness, w.hardness) << w.name;
        EXPECT_EQ(b->properties.drops, w.drops) << w.name;
        EXPECT_EQ(b->properties.soundMaterial, w.sound) << w.name;
        // T5 (v3.7.0): the walk_speed_factor — soul_sand carries the
        // Minecraft exact 0.4, every other content block is absent
        // (1.0 — the no-slowdown default, the overworld's twin).
        EXPECT_FLOAT_EQ(b->properties.walkSpeedFactor, w.walkFactor)
            << w.name;
        EXPECT_FALSE(b->hasState()) << w.name; // the ENT-2 premise
        EXPECT_EQ(b->model.shape, BlockModel::Shape::FullCube) << w.name;
    }
    // T3 (v3.7.0): the magma block's light emission (the v3.5.0
    // documented deviation 5's CLOSURE — the Minecraft exact 3: the
    // dim orange glow, the lava-adjacent feel).
    const Block* magma = blocks.findByName("magma_block");
    ASSERT_NE(magma, nullptr);
    EXPECT_EQ(magma->properties.lightEmission, 3);
    // The smelt recipe's data half: netherrack -> nether_brick (the
    // CRAFT-6 block-item inheritance — the item-side pin is below).
    const Block* netherrack = blocks.findByName("netherrack");
    ASSERT_NE(netherrack, nullptr);
    EXPECT_EQ(netherrack->properties.smeltableTo, "nether_brick");
    // The ceiling glowstone is the REUSED shipped block (light 15 — the
    // design's "复用既有 glowstone 块").
    const Block* glow = blocks.findByName("glowstone");
    ASSERT_NE(glow, nullptr);
    EXPECT_EQ(glow->properties.lightEmission, 15);
}

TEST(NetherContentTest, RealTwoItems) {
    // The 2 ore-drop items (the design's D4): quartz is the explicit
    // MATERIAL (no quartz block in v1 — the block's part-2), ancient
    // debris is its BLOCK item (the block's name owns the item space —
    // an explicit same-named file is the buildBlockItems' duplicate
    // rejection, the V2-7 planks precedent). The icon / model texture
    // face + the drop-item resolution (every ore's drops names a
    // registered item — the init cross-check's unit level).
    AssetManager assets;
    realAssets(assets);
    BlockRegistry blocks;
    ASSERT_TRUE(blocks.loadDirectory(assets, "data/blocks").ok());
    ItemRegistry items;
    ASSERT_TRUE(items.buildBlockItems(blocks).ok());
    ASSERT_TRUE(items.loadDirectory(assets, "data/items").ok());
    const Item* quartz = items.findByName("quartz");
    ASSERT_NE(quartz, nullptr);
    EXPECT_EQ(quartz->type, ItemType::Material);
    EXPECT_EQ(quartz->icon, "quartz");
    ASSERT_TRUE(quartz->model.has_value());
    EXPECT_EQ(quartz->model->texture, "quartz");
    EXPECT_TRUE(quartz->model->elements.empty() == false);
    // The icon texture exists on disk (the icon-file discipline).
    EXPECT_TRUE(assets.exists("textures/items/quartz.png"));
    const Item* debris = items.findByName("ancient_debris");
    ASSERT_NE(debris, nullptr);
    EXPECT_EQ(debris->type, ItemType::Block); // the block item (the name
                                              // collision's structural form)
    EXPECT_EQ(debris->icon, ""); // block items render the 3D block cube
    // The ore's drop resolution (the init cross-check's face).
    const Item* gold = items.findByName("gold_ingot");
    ASSERT_NE(gold, nullptr);
    const Block* qo = blocks.findByName("quartz_ore");
    ASSERT_TRUE(items.findByName(qo->properties.drops) != nullptr);
    const Block* ngo = blocks.findByName("nether_gold_ore");
    ASSERT_TRUE(items.findByName(ngo->properties.drops) == gold);
}

TEST(NetherContentTest, RealSmeltAndShapedRecipes) {
    // The 2 recipes (the design's D5): the smelt (netherrack ->
    // nether_brick 1:1 — the block item's smeltableTo, the CRAFT-6
    // inheritance) + the 2x2 shaped (the nether_brick block's recipe).
    AssetManager assets;
    realAssets(assets);
    BlockRegistry blocks;
    ASSERT_TRUE(blocks.loadDirectory(assets, "data/blocks").ok());
    ItemRegistry items;
    ASSERT_TRUE(items.buildBlockItems(blocks).ok());
    ASSERT_TRUE(items.loadDirectory(assets, "data/items").ok());
    // The smelt: the netherrack block item carries the product.
    const Item* netherrackItem = items.findByName("netherrack");
    ASSERT_NE(netherrackItem, nullptr);
    EXPECT_EQ(netherrackItem->smeltableTo, "nether_brick");
    // The product exists (the init cross-check's face: an item named).
    ASSERT_TRUE(items.findByName("nether_brick") != nullptr);
    // The shaped 2x2.
    RecipeRegistry recipes;
    ASSERT_TRUE(recipes.loadDirectory(assets, items, "data/recipes").ok());
    const Recipe* r = recipes.find("nether_brick");
    ASSERT_NE(r, nullptr);
    EXPECT_EQ(r->type, RecipeType::Shaped);
    ASSERT_EQ(r->pattern.size(), 2u);
    EXPECT_EQ(r->pattern[0], "NN");
    EXPECT_EQ(r->pattern[1], "NN");
    ASSERT_EQ(r->key.size(), 1u);
    EXPECT_EQ(r->key.at('N'), items.idForName("nether_brick"));
    EXPECT_EQ(r->output, items.idForName("nether_brick"));
    EXPECT_EQ(r->outputCount, 1);
}

TEST(NetherContentTest, RealBiomesNetherDirectory) {
    // The 5 biome files (the design's D1): the load order (the file-name
    // sort — the registry order), the weights (summing to 100, the MC
    // ratios), the surfaces (cross-checked against the real block
    // registry), and the full NetherGen::create resolution.
    AssetManager assets;
    realAssets(assets);
    BlockRegistry blocks;
    ASSERT_TRUE(blocks.loadDirectory(assets, "data/blocks").ok());
    BiomeRegistry pool;
    ASSERT_TRUE(pool.loadDirectory(assets, "data/biomes_nether").ok());
    ASSERT_EQ(pool.count(), 5u);
    const std::vector<std::pair<std::string, int>> want = {
        {"basalt_deltas", 16},
        {"crimson_forest", 22},
        {"nether_wastes", 36},
        {"soul_sand_valley", 17},
        {"warped_forest", 9},
    };
    int total = 0;
    for (std::size_t i = 0; i < pool.count(); ++i) {
        const Biome& b = pool.all()[i];
        EXPECT_EQ(b.name, want[i].first) << "load order " << i;
        EXPECT_EQ(b.weight, want[i].second) << b.name;
        total += b.weight;
        // The surface cross-check (a real block, stateless).
        const Block* s = blocks.findByName(b.surface);
        ASSERT_NE(s, nullptr) << "surface " << b.surface;
        EXPECT_FALSE(s->hasState()) << b.surface;
    }
    EXPECT_EQ(total, 100);
    auto gen = NetherGen::create(netherSeed(123456789), pool, blocks);
    ASSERT_TRUE(gen.ok()) << gen.error().message;
    // The feature flags over the REAL pool (the data-driven derivation).
    EXPECT_EQ(gen->biomes.size(), 5u);
    EXPECT_TRUE(gen->biomes[0].basaltColumns); // basalt_deltas
    EXPECT_EQ(gen->biomes[1].stem, gen->crimsonStem); // crimson_forest
    EXPECT_FALSE(gen->biomes[2].basaltColumns); // nether_wastes
    EXPECT_FALSE(gen->biomes[2].soulFamily);
    EXPECT_TRUE(gen->biomes[3].soulFamily); // soul_sand_valley
    EXPECT_EQ(gen->biomes[4].stem, gen->warpedStem); // warped_forest
}

TEST(NetherContentTest, RealContentGenCensus) {
    // The content-in-place proof (the deterministic unit twin of the
    // headless smoke's census): the REAL gen (the QUIET config's seed) —
    // every content family is HUNTED through its oracle (a placed column
    // / an unoverwritten ore / a rolling cell), the chunk(s) filled, the
    // family's block asserted in place (the plateau + the cavity are no
    // longer a flat netherrack slab).
    AssetManager assets;
    realAssets(assets);
    BlockRegistry blocks;
    ASSERT_TRUE(blocks.loadDirectory(assets, "data/blocks").ok());
    BiomeRegistry pool;
    ASSERT_TRUE(pool.loadDirectory(assets, "data/biomes_nether").ok());
    auto genR = NetherGen::create(netherSeed(123456789), pool, blocks);
    ASSERT_TRUE(genR.ok()) << genR.error().message;
    const NetherGen gen = *genR;
    World world;
    // Fills on demand (the hunt's found positions' chunks).
    auto fillAt = [&world, &gen](int wx, int wy, int wz) {
        const ChunkPos cp = worldToChunk(BlockPos{wx, wy, wz});
        const auto positions = world.chunkPositions();
        const bool present =
            std::find(positions.begin(), positions.end(), cp) !=
            positions.end();
        if (!present) {
            Chunk& c = world.getOrCreateChunk(cp);
            fillNetherChunk(gen, c, cp);
        }
    };
    // -- the surface family (one found column per biome class) ---------
    const struct {
        const char* biome;
        BlockID surface;
    } surfaces[] = {
        {"soul_sand_valley", gen.soulSand},
        {"crimson_forest", gen.crimsonNylium},
        {"warped_forest", gen.warpedNylium},
    };
    for (const auto& s : surfaces) {
        int fx = 0, fz = 0;
        bool found = false;
        for (int gz = -480; gz <= 480 && !found; gz += 4) {
            for (int gx = -480; gx <= 480 && !found; gx += 4) {
                const NetherBiome& b = gen.biomes[netherBiomeAt(gen, gx, gz)];
                if (b.name == s.biome && b.surface == s.surface) {
                    fx = gx;
                    fz = gz;
                    found = true;
                }
            }
        }
        ASSERT_TRUE(found) << s.biome;
        fillAt(fx, kNetherRockTop, fz);
        // The plateau's TOP cell (y = 52): the surface block, unless a 3b
        // roll overwrote it (the pass-order pin). Either way, a content
        // block is in place (the surface OR the 3b owner).
        const BlockID got = world.getBlock(fx, kNetherRockTop, fz);
        EXPECT_TRUE(got == s.surface || got == gen.soulSoil ||
                    got == gen.magmaBlock)
            << s.biome;
    }
    // -- the stem family (one placed column per forest) ----------------
    for (BlockID stem : {gen.crimsonStem, gen.warpedStem}) {
        int fx = 0, fz = 0;
        bool found = false;
        for (int gz = -480; gz <= 480 && !found; gz += 4) {
            for (int gx = -480; gx <= 480 && !found; gx += 4) {
                const NetherBiome& b = gen.biomes[netherBiomeAt(gen, gx, gz)];
                if (b.stem != stem) {
                    continue;
                }
                if (stemColumnOracle(gen, gx, gz).placed) {
                    fx = gx;
                    fz = gz;
                    found = true;
                }
            }
        }
        ASSERT_TRUE(found) << "a placed stem column ("
                           << (stem == gen.crimsonStem ? "crimson"
                                                      : "warped")
                           << ")";
        fillAt(fx, kNetherAirBottom, fz);
        EXPECT_EQ(world.getBlock(fx, kNetherAirBottom, fz), stem);
    }
    // -- the basalt family (one placed column) --------------------------
    {
        int fx = 0, fz = 0;
        bool found = false;
        for (int gz = -480; gz <= 480 && !found; gz += 8) {
            for (int gx = -480; gx <= 480 && !found; gx += 8) {
                const NetherBiome& b = gen.biomes[netherBiomeAt(gen, gx, gz)];
                if (!b.basaltColumns) {
                    continue;
                }
                if (basaltColumnOracle(gen, gx, gz).placed) {
                    fx = gx;
                    fz = gz;
                    found = true;
                }
            }
        }
        ASSERT_TRUE(found) << "a placed basalt column";
        fillAt(fx, kNetherAirBottom, fz);
        EXPECT_EQ(world.getBlock(fx, kNetherAirBottom, fz), gen.basalt);
    }
    // -- the ore family (one unoverwritten placed cell each) ------------
    const struct {
        int t;
        BlockID block;
        const char* what;
    } ores[] = {
        {0, gen.quartzOre, "quartz_ore"},
        {1, gen.netherGoldOre, "nether_gold_ore"},
        {2, gen.ancientDebris, "ancient_debris"},
    };
    for (const auto& o : ores) {
        bool found = false;
        for (int j = 4; j <= 6 && !found; ++j) { // y lattice 32..52
            for (int k = -24; k <= 24 && !found; ++k) {
                for (int i = -24; i <= 24 && !found; ++i) {
                    const OrePoint p = orePointOracle(gen, i, j, k);
                    if (!p.placed || p.t != o.t) {
                        continue;
                    }
                    if (decorationOwner(gen, p.vx, p.vy, p.vz).valid()) {
                        continue;
                    }
                    fillAt(p.vx, p.vy, p.vz);
                    EXPECT_EQ(world.getBlock(p.vx, p.vy, p.vz), o.block)
                        << o.what;
                    found = true;
                }
            }
        }
        EXPECT_TRUE(found) << "an unoverwritten " << o.what << " cell";
    }
    // -- the 3b family (one rolling cell each) ---------------------------
    {
        // soul_soil: a soul-family cell in the band whose soul roll hits
        // and the magma roll misses.
        bool foundSoul = false;
        for (int wz = -60; wz <= 60 && !foundSoul; ++wz) {
            for (int wx = -60; wx <= 60 && !foundSoul; ++wx) {
                const NetherBiome& b = gen.biomes[netherBiomeAt(gen, wx, wz)];
                if (!b.soulFamily) {
                    continue;
                }
                for (int wy = kNetherSoulSoilBandBottom;
                     wy <= kNetherSoulSoilBandTop && !foundSoul; ++wy) {
                    const bool soul = oracleRoll3(
                        wx, wz, wy, gen.seed ^ kSaltNetherSoulSoil,
                        kNetherSoulSoilProbability);
                    const bool magma = oracleRoll3(
                        wx, wz, wy, gen.seed ^ kSaltNetherMagma,
                        kNetherMagmaProbability);
                    if (soul && !magma) {
                        fillAt(wx, wy, wz);
                        EXPECT_EQ(world.getBlock(wx, wy, wz), gen.soulSoil);
                        foundSoul = true;
                    }
                }
            }
        }
        EXPECT_TRUE(foundSoul) << "a soul_soil cell";
        // magma: a soul-family cell whose magma roll hits.
        bool foundMagma = false;
        for (int wz = -60; wz <= 60 && !foundMagma; ++wz) {
            for (int wx = -60; wx <= 60 && !foundMagma; ++wx) {
                const NetherBiome& b = gen.biomes[netherBiomeAt(gen, wx, wz)];
                if (!b.soulFamily) {
                    continue;
                }
                for (int wy = kNetherMagmaBandBottom;
                     wy <= kNetherMagmaBandTop && !foundMagma; ++wy) {
                    const bool magma = oracleRoll3(
                        wx, wz, wy, gen.seed ^ kSaltNetherMagma,
                        kNetherMagmaProbability);
                    if (magma) {
                        fillAt(wx, wy, wz);
                        EXPECT_EQ(world.getBlock(wx, wy, wz), gen.magmaBlock);
                        foundMagma = true;
                    }
                }
            }
        }
        EXPECT_TRUE(foundMagma) << "a magma_block cell";
        // glowstone: a ceiling-row cell whose roll hits.
        bool foundGlow = false;
        for (int wz = -60; wz <= 60 && !foundGlow; ++wz) {
            for (int wx = -60; wx <= 60 && !foundGlow; ++wx) {
                if (oracleRoll3(wx, wz, kNetherGlowstoneY,
                                gen.seed ^ kSaltNetherGlowstone,
                                kNetherGlowstoneProbability)) {
                    fillAt(wx, kNetherGlowstoneY, wz);
                    EXPECT_EQ(world.getBlock(wx, kNetherGlowstoneY, wz),
                              gen.glowstone);
                    foundGlow = true;
                }
            }
        }
        EXPECT_TRUE(foundGlow) << "a glowstone ceiling cell";
    }
}

TEST(NetherContentTest, AtlasLayoutGrewTo136) {
    // The atlas's census face (the 124 -> 136 tile's zero-shift red line
    // lives in test_atlas's full census; this is the count + the 12 new
    // cells' existence): the grid grew 16x8 -> 16x16 (the new tiles sit
    // in the new rows — no existing tile moved).
    AssetManager assets;
    realAssets(assets);
    auto text = assets.loadText("data/textures/block_atlas.json");
    ASSERT_TRUE(text.ok()) << text.error().message;
    auto def = Json::parse(*text);
    ASSERT_TRUE(def.ok());
    auto layout = AtlasLayout::parse(*def);
    ASSERT_TRUE(layout.ok()) << layout.error().message;
    EXPECT_EQ(layout->columns, 16);
    EXPECT_EQ(layout->rows, 16);
    EXPECT_EQ(layout->tiles.size(), 136u);
    const char* newTiles[12] = {
        "nether_brick",    "magma_block",     "quartz_ore",
        "nether_gold_ore", "ancient_debris",  "soul_sand",
        "soul_soil",       "crimson_nylium",  "warped_nylium",
        "basalt",          "crimson_stem",    "warped_stem",
    };
    for (const char* t : newTiles) {
        EXPECT_TRUE(layout->contains(t)) << t;
    }
}
