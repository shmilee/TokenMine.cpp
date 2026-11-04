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
#include <string>
#include <vector>

#include "block/block_registry.hpp"
#include "core/json.hpp"
#include "core/portal.hpp"
#include "player/player.hpp"
#include "world/nether_gen.hpp"
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

// The nether fill's content registry (netherrack + bedrock — the two
// band blocks; the lava is a fluid, not a block).
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
    return NetherGen {reg.idForName("netherrack"), reg.idForName("bedrock")};
}

// The nether's band at a world Y (the fill's layer rule — the test's
// expectation oracle, independent of the fill's band walk): the floor
// bedrock / the lava sea / the netherrack slab / the roof bedrock.
enum class NetherBand { FloorBedrock, LavaSea, Netherrack, RoofBedrock };
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
    // A chunk at the lava-sea / netherrack boundary (world y 24..39
    // holds both the sea's top (31) and the slab's bottom (32)).
    World world;
    Chunk& chunk = world.getOrCreateChunk({0, 1, 0}); // y 16..31
    fillNetherChunk(gen, chunk, {0, 1, 0});
    expectChunkBands(chunk, {0, 1, 0}, gen);
    Chunk& chunk2 = world.getOrCreateChunk({0, 2, 0}); // y 32..47
    fillNetherChunk(gen, chunk2, {0, 2, 0});
    expectChunkBands(chunk2, {0, 2, 0}, gen);
    // The floor chunk (y 0..15: the bedrock floor 0..4 + the sea 5..15).
    Chunk& chunk3 = world.getOrCreateChunk({0, 0, 0});
    fillNetherChunk(gen, chunk3, {0, 0, 0});
    expectChunkBands(chunk3, {0, 0, 0}, gen);
    // The roof chunk (y 112..127: the slab's top 112..122 + the roof 123
    // ..127).
    Chunk& chunk4 = world.getOrCreateChunk({0, 7, 0});
    fillNetherChunk(gen, chunk4, {0, 7, 0});
    expectChunkBands(chunk4, {0, 7, 0}, gen);
}

TEST(NetherGenTest, LavaSeaTopEdge) {
    // The sea's TOP edge (y = 31): the sea's highest level — a source
    // (level 8, the static sea) at 31, the slab's bottom at 32 (the
    // netherrack the player stands on — the nether wastes' surface).
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
    // y = 32 (local ly = 0 of the chunk): the slab's bottom.
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
    // 128 space — the recon §二.3's verified geometry).
    EXPECT_EQ(kNetherFloorBedrockTop + 1, kNetherLavaSeaBottom);
    EXPECT_EQ(kNetherLavaSeaTop + 1, kNetherRockBottom);
    EXPECT_EQ(kNetherRockTop + 1, kNetherRoofBedrockBottom);
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
    // row are ADJACENT cells — both netherrack, the slab's interior).
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
    // slab chunk holds netherrack (the same local cell, the different
    // world Y).
    Chunk c;
    fillNetherChunk(gen, c, {4, 0, -2}); // the floor chunk (y 0..15)
    EXPECT_EQ(c.get(0, 0, 0), gen.bedrock); // y = 0: the floor
    EXPECT_NE(c.get(0, 0, 0), a.get(0, 0, 0)); // y = 48: the slab
    // And the roof chunk's bedrock (y = 123: local ly 11 of chunk 7)
    // differs from the slab's netherrack (y = 112: local ly 0).
    Chunk r;
    fillNetherChunk(gen, r, {4, 7, -2});
    EXPECT_EQ(r.get(0, 11, 0), gen.bedrock);
    EXPECT_EQ(r.get(0, 0, 0), gen.netherrack);
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
