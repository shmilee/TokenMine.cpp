// V3-10a (v3.23.0) / V3-10b (v3.24.0): the End's world generation —
// the pure chunk fill (world/end_gen: the central plateau + the exit
// portal in 10a; 10b adds the bedrock ocean + the island archipelago +
// the chorus decoration, every pass a coordinate-pure function — zero
// noise in the 10a sense: the lattice rolls are hash-pure, the
// constructive determinism), the derived seed (worldSeed ^ kSaltEnd,
// never stored), the biome pool's single-member selection, the island
// / chorus pure rolls, and the shipped end blocks' data flags
// (stateless, the ENT-2 premise).
#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include "block/block_registry.hpp"
#include "core/asset_manager.hpp"
#include "item/item_registry.hpp"
#include "core/json.hpp"
#include "world/biome.hpp"
#include "world/end_gen.hpp"
#include "world/nether_gen.hpp" // kSaltNether (the independence witness)
#include "world/noise.hpp" // hash3 (the oracle's lattice walk)
#include "world/world.hpp"
#include "world/world_generator.hpp" // kLattice24Scale (the oracle's gate)

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

std::vector<std::string> minimalEndDefs() {
    return {
        "{\"id\":\"endstone\",\"properties\":{\"hardness\":0.5,"
        "\"drops\":\"endstone\"},"
        "\"model\":{\"textures\":{\"top\":\"endstone\",\"side\":\"endstone\","
        "\"bottom\":\"endstone\"}}}",
        "{\"id\":\"obsidian\",\"properties\":{\"hardness\":10.0,"
        "\"destroyable\":false},"
        "\"model\":{\"textures\":{\"top\":\"obsidian\",\"side\":\"obsidian\","
        "\"bottom\":\"obsidian\"}}}",
        "{\"id\":\"end_portal\",\"properties\":{\"hardness\":0,"
        "\"solid\":false,\"opaque\":false,\"transparent\":true,"
        "\"destroyable\":false,\"light_emission\":11,\"end_portal\":true},"
        "\"model\":{\"textures\":{\"top\":\"end_portal\",\"side\":\"end_portal\","
        "\"bottom\":\"end_portal\"}}}",
        "{\"id\":\"end_portal_frame_eyed\",\"properties\":{\"hardness\":0,"
        "\"opaque\":true,\"destroyable\":false,\"end_frame\":true,"
        "\"end_frame_eyed\":true},"
        "\"model\":{\"textures\":{\"top\":\"end_portal_frame_eyed\","
        "\"side\":\"end_portal_frame_eyed\","
        "\"bottom\":\"end_portal_frame_eyed\"}}}",
        // V3-10b: the two content names (the bedrock ocean's block +
        // the chorus's cross).
        "{\"id\":\"bedrock\",\"properties\":{\"hardness\":0.0,"
        "\"destroyable\":false},"
        "\"model\":{\"textures\":{\"top\":\"bedrock\",\"side\":\"bedrock\","
        "\"bottom\":\"bedrock\"}}}",
        "{\"id\":\"chorus_plant\",\"properties\":{\"hardness\":0.0,"
        "\"solid\":false,\"opaque\":false,\"transparent\":true,"
        "\"cutout\":true,\"drops\":\"chorus_fruit\"},"
        "\"model\":{\"shape\":\"cross\",\"textures\":{\"cross\":\"chorus\"}}}",
        // V3-10d: the dragon arena's two blocks (the crystal's
        // non-solid cutout + the explode radius, the egg's non-solid
        // cutout drop — the shipped data's shape).
        "{\"id\":\"end_crystal\",\"properties\":{\"hardness\":0.0,"
        "\"solid\":false,\"opaque\":false,\"transparent\":true,"
        "\"cutout\":true,\"light_emission\":14,\"explode_radius\":3,"
        "\"shatter_on_arrow\":true},"
        "\"model\":{\"textures\":{\"top\":\"end_crystal\",\"side\":"
        "\"end_crystal\",\"bottom\":\"end_crystal\"}}}",
        "{\"id\":\"dragon_egg\",\"properties\":{\"hardness\":0.0,"
        "\"solid\":false,\"opaque\":false,\"transparent\":true,"
        "\"cutout\":true,\"light_emission\":1,"
        "\"drops\":\"dragon_egg\"},"
        "\"model\":{\"textures\":{\"top\":\"dragon_egg\",\"side\":"
        "\"dragon_egg\",\"bottom\":\"dragon_egg\"}}}",
    };
}

// A minimal end fill registry (the six 10b names, the data flags set
// the way the shipped data sets them).
BlockRegistry endRegistry() {
    BlockRegistry reg;
    for (const std::string& def : minimalEndDefs()) {
        auto r = reg.registerBlock(parseDef(def));
        if (!r.ok()) {
            throw std::runtime_error(r.error().message);
        }
    }
    return reg;
}

// The end biome pool fixture (the single end_plains — the shipped
// file's shape).
BiomeRegistry endBiomes() {
    BiomeRegistry biomes;
    const std::string def =
        R"({"id":"end_plains","weight":1,"surface":"endstone","subsurface":"endstone"})";
    auto r = biomes.registerBiome(parseDef(def));
    if (!r.ok()) {
        throw std::runtime_error(r.error().message);
    }
    return biomes;
}

EndGen endGenFixture(std::int64_t seed) {
    const BlockRegistry reg = endRegistry();
    auto r = EndGen::create(seed, endBiomes(), reg);
    if (!r.ok()) {
        throw std::runtime_error(r.error().message);
    }
    return *r;
}

// The island's central-overlap test (the oracle's guardrail twin — the
// fill's islandOverlapsCentralIsland mirrored as the test's oracle).
bool oracleIslandOverlapsCentral(const EndIsland& isle) {
    const int half = kEndIslandHalf;
    return (isle.cx - isle.radius <= half) &&
        (isle.cx + isle.radius >= -half) &&
        (isle.cz - isle.radius <= half) &&
        (isle.cz + isle.radius >= -half);
}

// The expected block of a world cell under the 10b fill (the tests'
// oracle — the pass order mirrored: the bedrock ocean's band, the
// biome plateau's band, the island archipelago (the lattice walk with
// the guardrail + the air-only predicate), the chorus's top-center
// cell, the exit portal LAST. The oracle keeps a running "already
// written" map per cell to reproduce the air-only semantics.)
BlockID oracleCell(const EndGen& gen, int wx, int wy, int wz) {
    // pass 0: the bedrock ocean.
    if (wy >= kEndOceanBottom && wy <= kEndOceanTop) {
        return gen.bedrock;
    }
    // pass 1: the central plateau (the biome's surface — 10b's single
    // member, the endstone no-op).
    if (std::abs(wx) <= kEndIslandHalf && std::abs(wz) <= kEndIslandHalf &&
        wy >= kEndIslandBottom && wy <= kEndIslandTop) {
        const std::size_t biome = endBiomeAt(gen, wx, wz);
        return gen.biomes[biome].surface;
    }
    // pass 2 + 3: the islands (air-only over the passes above) + the
    // chorus (air-only on the top center).
    const int s = kEndIslandLattice;
    const int margin = s + (kEndIslandRadiusMax + kEndIslandTopLayers);
    const int xi0 = floorDiv(wx - margin, s);
    const int xi1 = floorDiv(wx + margin, s);
    const int zi0 = floorDiv(wz - margin, s);
    const int zi1 = floorDiv(wz + margin, s);
    const std::uint64_t saltIsland =
        static_cast<std::uint64_t>(gen.seed ^ kSaltEndIsland);
    const std::uint64_t saltChorus =
        static_cast<std::uint64_t>(gen.seed ^ kSaltEndChorus);
    // The island's write order is the lattice walk's (iz, ix) order —
    // the FIRST island to claim a cell wins (the air-only predicate).
    // Walk in the fill's order (zi0..zi1, xi0..xi1 — the fill's outer
    // loop is iz, inner ix).
    for (int iz = zi0; iz <= zi1; ++iz) {
        for (int ix = xi0; ix <= xi1; ++ix) {
            EndIsland isle;
            if (!endIslandAt(ix, iz, saltIsland, isle)) {
                continue;
            }
            if (oracleIslandOverlapsCentral(isle)) {
                continue;
            }
            const bool chorus = endChorusAt(ix, iz, saltChorus);
            for (int layer = 0; layer < kEndIslandTopLayers + kEndIslandConeLayers;
                 ++layer) {
                const int r = isle.radius - layer;
                if (r < 1) {
                    continue;
                }
                const int layerY = isle.top - layer;
                if (wy == layerY) {
                    const int dx = wx - isle.cx;
                    const int dz = wz - isle.cz;
                    if (dx * dx + dz * dz <= r * r) {
                        return gen.endstone;
                    }
                }
                if (layer == 0 && chorus && wy == isle.top + 1 &&
                    wx == isle.cx && wz == isle.cz) {
                    return gen.chorusPlant;
                }
            }
        }
    }
    // pass 4: the exit portal at the origin (body level 53).
    if (wy == kEndExitLevel && std::abs(wx) <= 2 && std::abs(wz) <= 2) {
        const bool corner = (std::abs(wx) == 2) && (std::abs(wz) == 2);
        if (!corner) {
            if (std::abs(wx) <= 1 && std::abs(wz) <= 1) {
                return gen.endPortal;
            }
            return gen.frameEyed;
        }
    }
    // pass 5 (10d): the dragon arena (the 4 obsidian towers at (±50,
    // ±50) rising y53..58 + the 6 crystals: the 4 tower tops at y59 +
    // the 2 exit crystals on the frame ring's mid-edge tops (±2, y54,
    // 0)). The air-only predicate: none of these cells is claimed by an
    // earlier pass (the plateau is y<=52, the islands are guardrailed
    // out of |x|,|z|<=60, the exit is y53) — so the direct returns hold.
    static const int kTowerCenters[4][2] = {{50, 50}, {-50, 50},
                                            {50, -50}, {-50, -50}};
    for (const auto& tc : kTowerCenters) {
        const int tx = tc[0], tz = tc[1];
        if (std::abs(wx - tx) <= 2 && std::abs(wz - tz) <= 2 &&
            wy >= 53 && wy <= 58) {
            return gen.obsidian;
        }
        if (wx == tx && wz == tz && wy == 59) {
            return gen.endCrystal;
        }
    }
    if (wy == 54 && wz == 0 && (wx == 2 || wx == -2)) {
        return gen.endCrystal;
    }
    return BlockID::air();
}

void expectFillMatchesOracle(const Chunk& chunk, const ChunkPos& pos,
                             const EndGen& gen) {
    const int ox = pos.x * kChunkSize;
    const int oz = pos.z * kChunkSize;
    const int baseY = pos.y * kChunkSize;
    for (int ly = 0; ly < kChunkSize; ++ly) {
        for (int lz = 0; lz < kChunkSize; ++lz) {
            for (int lx = 0; lx < kChunkSize; ++lx) {
                const BlockID want =
                    oracleCell(gen, ox + lx, baseY + ly, oz + lz);
                EXPECT_EQ(chunk.get(lx, ly, lz), want)
                    << "world (" << (ox + lx) << ", " << (baseY + ly) << ", "
                    << (oz + lz) << ")";
            }
        }
    }
}

} // namespace

// ---------------------------------------------------------------------------
// The derived seed (the netherSeed's twin — the XOR with kSaltEnd).
// ---------------------------------------------------------------------------

TEST(EndGenTest, SeedIsTheXorTwin) {
    EXPECT_EQ(endSeed(0), kSaltEnd);
    EXPECT_EQ(endSeed(42), 42 ^ kSaltEnd);
    // The derivation is its own inverse shape (the XOR precedent):
    // endSeed(endSeed(s)) is the world seed.
    EXPECT_EQ(endSeed(endSeed(12345)), 12345);
    // The end's salt is independent of the nether's (a different world
    // seed derivation — no end feature tracks the nether's stream).
    EXPECT_NE(kSaltEnd, kSaltNether);
    // V3-10b: the content salts are independent of each other + of the
    // root (the kSaltNether family's continuation shape).
    EXPECT_NE(kSaltEndIsland, kSaltEndChorus);
    EXPECT_NE(kSaltEndBiome, kSaltEndIsland);
    EXPECT_NE(kSaltEndIsland, kSaltEnd);
}

// ---------------------------------------------------------------------------
// EndGen::create (the fail-fast resolve — the NetherGen::create twin).
// ---------------------------------------------------------------------------

TEST(EndGenTest, CreateResolvesTheEightNames) {
    const BlockRegistry reg = endRegistry();
    auto r = EndGen::create(endSeed(7), endBiomes(), reg);
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_EQ(r->endstone, reg.idForName("endstone"));
    EXPECT_EQ(r->obsidian, reg.idForName("obsidian"));
    EXPECT_EQ(r->endPortal, reg.idForName("end_portal"));
    EXPECT_EQ(r->frameEyed, reg.idForName("end_portal_frame_eyed"));
    EXPECT_EQ(r->bedrock, reg.idForName("bedrock"));
    EXPECT_EQ(r->chorusPlant, reg.idForName("chorus_plant"));
    EXPECT_EQ(r->endCrystal, reg.idForName("end_crystal"));
    EXPECT_EQ(r->dragonEgg, reg.idForName("dragon_egg"));
    EXPECT_EQ(r->seed, endSeed(7));
    ASSERT_EQ(r->biomes.size(), 1u);
    EXPECT_EQ(r->biomes[0].name, "end_plains");
    EXPECT_EQ(r->biomes[0].surface, reg.idForName("endstone"));
    EXPECT_EQ(r->totalWeight, 1);
}

TEST(EndGenTest, CreateNamesTheMissingBlock) {
    // A registry WITHOUT endstone: the first missing name is named
    // (registry resolve order: endstone first).
    BlockRegistry reg = endRegistry();
    reg.clear();
    const std::vector<std::string> defs = minimalEndDefs();
    for (const std::string& def : {defs[1], defs[2], defs[3], defs[4],
                                   defs[5]}) {
        ASSERT_TRUE(reg.registerBlock(parseDef(def)).ok());
    }
    auto r = EndGen::create(1, endBiomes(), reg);
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("endstone"), std::string::npos);
    EXPECT_NE(r.error().message.find("end worldgen"), std::string::npos);
}

TEST(EndGenTest, CreateNamesTheMissingBiomeSurface) {
    // The biome pool names an unknown surface block: the S6 gate's
    // subject (the nether's surface-unknown twin).
    BiomeRegistry biomes;
    const std::string def =
        R"({"id":"bad","weight":1,"surface":"not_a_block","subsurface":"not_a_block"})";
    ASSERT_TRUE(biomes.registerBiome(parseDef(def)).ok());
    auto r = EndGen::create(1, biomes, endRegistry());
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("not_a_block"), std::string::npos);
}

TEST(EndGenTest, CreateRejectsAnEmptyBiomePool) {
    // The empty pool (a stripped biomes_end/ — the loadDirectory's
    // fail-fast twin): the plateau's surface source is missing.
    auto r = EndGen::create(1, BiomeRegistry {}, endRegistry());
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("biomes_end"), std::string::npos);
}

TEST(EndGenTest, CreateRejectsAStatefulFillBlock) {
    // The ENT-2 worldgen rule: a per-instance state is a journal the
    // fill cannot write — a stateful endstone is named.
    BlockRegistry reg = endRegistry();
    reg.clear();
    const std::string statefulEndstone =
        "{\"id\":\"endstone\",\"properties\":{},"
        "\"model\":{\"state_count\":2,\"state_default\":0,"
        "\"textures\":{\"top\":\"e\",\"side\":\"e\",\"bottom\":\"e\"}}}";
    const std::vector<std::string> defs = minimalEndDefs();
    for (const std::string& def :
         {statefulEndstone, defs[1], defs[2], defs[3], defs[4], defs[5]}) {
        ASSERT_TRUE(reg.registerBlock(parseDef(def)).ok());
    }
    auto r = EndGen::create(1, endBiomes(), reg);
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("endstone"), std::string::npos);
    EXPECT_NE(r.error().message.find("per-instance state"),
              std::string::npos);
}

// ---------------------------------------------------------------------------
// V3-10b: the biome selection (the single member's constant walk).
// ---------------------------------------------------------------------------

TEST(EndGenTest, SingleBiomeIsTheConstantSelection) {
    const EndGen gen = endGenFixture(endSeed(3));
    // The one-member pool: every coordinate selects the endstone
    // surface (the cumulative walk's constant, the 10d's seam).
    for (const int coord : {-64, -1, 0, 1, 48, 64, 200}) {
        EXPECT_EQ(gen.biomes[endBiomeAt(gen, coord, -coord)].surface,
                  gen.endstone);
        EXPECT_EQ(gen.biomes[endBiomeAt(gen, coord, coord)].surface,
                  gen.endstone);
    }
}

// ---------------------------------------------------------------------------
// V3-10b: the island / chorus pure rolls (the lattice's determinism +
// the parameter bounds + the guardrail).
// ---------------------------------------------------------------------------

TEST(EndGenTest, IslandRollIsDeterministicAndBounded) {
    const std::uint64_t salt =
        static_cast<std::uint64_t>(endSeed(11) ^ kSaltEndIsland);
    EndIsland a;
    EndIsland b;
    bool any = false;
    for (int iz = -8; iz <= 8; ++iz) {
        for (int ix = -8; ix <= 8; ++ix) {
            const bool ra = endIslandAt(ix, iz, salt, a);
            const bool rb = endIslandAt(ix, iz, salt, b);
            EXPECT_EQ(ra, rb) << "(" << ix << "," << iz << ")";
            if (!ra) {
                continue;
            }
            any = true;
            EXPECT_EQ(a.cx, ix * kEndIslandLattice);
            EXPECT_EQ(a.cz, iz * kEndIslandLattice);
            EXPECT_GE(a.radius, kEndIslandRadiusMin);
            EXPECT_LE(a.radius, kEndIslandRadiusMax);
            EXPECT_GE(a.top, kEndIslandTopMin);
            EXPECT_LE(a.top, kEndIslandTopMax);
        }
    }
    // The lattice's density: at p = 0.15 over 289 cells, at least one
    // island exists (the deterministic witness — a zero count would
    // mean the roll's stream is dead).
    EXPECT_TRUE(any);
}

TEST(EndGenTest, IslandGuardrailProtectsTheCentralIsland) {
    // The lattice cells whose island bbox overlaps the central square
    // are all guarded (the fill skips them — the exit + the platform's
    // protection). The inner lattice region (|ix|, |iz| such that the
    // island's center lies within half + the max radius of the origin)
    // is ALWAYS guarded: center (c), radius >= 5, bbox [c-12, c+12]
    // overlaps [-60, 60] whenever |c| <= 72 → |ix| <= 4 (c = 16*ix).
    const std::uint64_t salt =
        static_cast<std::uint64_t>(endSeed(11) ^ kSaltEndIsland);
    for (int iz = -4; iz <= 4; ++iz) {
        for (int ix = -4; ix <= 4; ++ix) {
            EndIsland isle;
            if (!endIslandAt(ix, iz, salt, isle)) {
                continue;
            }
            EXPECT_TRUE(oracleIslandOverlapsCentral(isle))
                << "lattice (" << ix << "," << iz << "): the inner "
                   "island must be guarded";
        }
    }
}

TEST(EndGenTest, ChorusRollIsIndependentOfTheIslandRoll) {
    // The two salts decorrelate: there exist lattice cells where the
    // island rolls but the chorus does not, and (for another salt pair)
    // the chorus's answer is independent — the stream discipline's
    // witness over the deterministic lattice.
    const std::uint64_t saltIsland =
        static_cast<std::uint64_t>(endSeed(23) ^ kSaltEndIsland);
    const std::uint64_t saltChorus =
        static_cast<std::uint64_t>(endSeed(23) ^ kSaltEndChorus);
    int islandYesChorusNo = 0;
    int islandYesChorusYes = 0;
    for (int iz = -8; iz <= 8; ++iz) {
        for (int ix = -8; ix <= 8; ++ix) {
            EndIsland isle;
            if (!endIslandAt(ix, iz, saltIsland, isle)) {
                continue;
            }
            if (endChorusAt(ix, iz, saltChorus)) {
                ++islandYesChorusYes;
            } else {
                ++islandYesChorusNo;
            }
        }
    }
    // At p_island 0.15 x p_chorus 0.6 over ~43 islands, BOTH outcomes
    // exist (a monochrome chorus would mean the rolls track).
    EXPECT_GT(islandYesChorusYes, 0);
    EXPECT_GT(islandYesChorusNo, 0);
}

// ---------------------------------------------------------------------------
// The 10b fill (the oracle-checked chunks — the bedrock ocean's band,
// the plateau, the islands' cross-chunk continuity, the exit).
// ---------------------------------------------------------------------------

TEST(EndGenTest, BedrockOceanBandChunk) {
    // A chunk at the ocean's band (y 0..15 holds the y[0,4] bedrock):
    // the whole dimension's floor, under the central island too.
    const EndGen gen = endGenFixture(endSeed(1));
    World world;
    Chunk& chunk = world.getOrCreateChunk({0, 0, 0});
    fillEndChunk(gen, chunk, {0, 0, 0});
    for (int ly = 0; ly < 5; ++ly) {
        for (int lz = 0; lz < kChunkSize; ++lz) {
            for (int lx = 0; lx < kChunkSize; ++lx) {
                EXPECT_EQ(chunk.get(lx, ly, lz), gen.bedrock)
                    << "world (" << lx << ", " << ly << ", " << lz << ")";
            }
        }
    }
    // Above the ocean: not bedrock (the void / the content passes).
    for (int lz = 0; lz < kChunkSize; lz += 5) {
        for (int lx = 0; lx < kChunkSize; lx += 5) {
            EXPECT_NE(chunk.get(lx, 5, lz), gen.bedrock);
        }
    }
}

TEST(EndGenTest, IslandPlateauChunk) {
    // Chunk (0, 3, 0): y 48..63 — the plateau's five layers at the
    // chunk edge + the exit level 53 + the void above (+ the islands'
    // air-only writes + the ocean's absence at this height).
    const EndGen gen = endGenFixture(endSeed(1));
    World world;
    Chunk& chunk = world.getOrCreateChunk({0, 3, 0});
    fillEndChunk(gen, chunk, {0, 3, 0});
    expectFillMatchesOracle(chunk, {0, 3, 0}, gen);
}

TEST(EndGenTest, IslandEdgeChunk) {
    // Chunk (3, 3, 3): x 48..63 — the plateau boundary |x| <= 60 cuts
    // the chunk (x 48..60 plateau, x 61..63 void) + the outer islands'
    // first reach (the archipelago's edge).
    const EndGen gen = endGenFixture(endSeed(1));
    World world;
    Chunk& chunk = world.getOrCreateChunk({3, 3, 3});
    fillEndChunk(gen, chunk, {3, 3, 3});
    expectFillMatchesOracle(chunk, {3, 3, 3}, gen);
}

TEST(EndGenTest, OuterIslandChunk) {
    // Chunk (5, 2, 5): x 80..95 — beyond the plateau: the archipelago's
    // home (the lattice's outer cells roll the islands over the void;
    // the oracle's lattice walk covers them).
    const EndGen gen = endGenFixture(endSeed(1));
    World world;
    Chunk& chunk = world.getOrCreateChunk({5, 2, 5});
    fillEndChunk(gen, chunk, {5, 2, 5});
    expectFillMatchesOracle(chunk, {5, 2, 5}, gen);
}

TEST(EndGenTest, CrossChunkFillIsIdentical) {
    // The R1 contract: two fills of the same position into two fresh
    // chunks are identical — the lattice walk's extension makes an
    // island straddling the boundary write every piece in every chunk
    // it touches, identically (the chunk-order-independence the
    // streaming jobs' arbitrary scheduling relies on).
    const EndGen gen = endGenFixture(endSeed(1));
    World a;
    World b;
    const ChunkPos pos {2, 3, -1};
    fillEndChunk(gen, a.getOrCreateChunk(pos), pos);
    fillEndChunk(gen, b.getOrCreateChunk(pos), pos);
    const Chunk& ca = a.getOrCreateChunk(pos);
    const Chunk& cb = b.getOrCreateChunk(pos);
    for (int ly = 0; ly < kChunkSize; ++ly) {
        for (int lz = 0; lz < kChunkSize; ++lz) {
            for (int lx = 0; lx < kChunkSize; ++lx) {
                EXPECT_EQ(ca.get(lx, ly, lz), cb.get(lx, ly, lz));
            }
        }
    }
}

TEST(EndGenTest, CentralRegionStaysIslandFree) {
    // The R2 guardrail's concrete read (over the FILL, not the oracle):
    // the central square's interior carries ONLY the plateau (y 48..
    // 52), the exit (y 53), the ocean (y 0..4) and the void — an
    // island's disk is inside its bbox, and every bbox-overlapping
    // island is guarded, so no island cell (endstone at the island
    // band's off-plateau heights, or a chorus) can reach inside the
    // square. The sample grid covers the island band y 44..56.
    const EndGen gen = endGenFixture(endSeed(1));
    World world;
    for (int wz = -58; wz <= 58; wz += 13) {
        for (int wx = -58; wx <= 58; wx += 13) {
            for (int wy = 44; wy <= 56; ++wy) {
                const ChunkPos cp = worldToChunk(BlockPos {wx, wy, wz});
                fillEndChunk(gen, world.getOrCreateChunk(cp), cp);
                const BlockPos local =
                    worldToChunkLocal(cp, BlockPos {wx, wy, wz});
                const BlockID got = world.getBlock(wx, wy, wz);
                (void)local;
                if (wy == kEndExitLevel && std::abs(wx) <= 2 &&
                    std::abs(wz) <= 2) {
                    continue; // the exit's cells (the 10a's exact shape)
                }
                if (wy >= kEndIslandBottom && wy <= kEndIslandTop) {
                    EXPECT_EQ(got, gen.endstone)
                        << "the plateau's band at (" << wx << "," << wy
                        << "," << wz << ")";
                } else {
                    EXPECT_EQ(got, BlockID::air())
                        << "an island cell inside the square at (" << wx
                        << "," << wy << "," << wz << ")";
                }
            }
        }
    }
}

TEST(EndGenTest, ExitPortalCellsAtTheOrigin) {
    // The 5x5 ring at the body level: the 12 frame cells all EYED, the
    // 3x3 center all end_portal, the four corners void air (the 10a
    // worldgen ships the ACTIVE exit — the return portal). The exit is
    // the LAST pass: no island / chorus cell overwrites it (the
    // guardrail's belt + the order's double-gate).
    const EndGen gen = endGenFixture(endSeed(1));
    World world;
    for (int cz = -1; cz <= 0; ++cz) {
        for (int cx = -1; cx <= 0; ++cx) {
            const ChunkPos pos {cx, 3, cz};
            fillEndChunk(gen, world.getOrCreateChunk(pos), pos);
        }
    }
    for (int dz = -2; dz <= 2; ++dz) {
        for (int dx = -2; dx <= 2; ++dx) {
            const int wx = dx;
            const int wz = dz;
            const bool onEdge = (std::abs(dx) == 2) || (std::abs(dz) == 2);
            const bool corner = (std::abs(dx) == 2) && (std::abs(dz) == 2);
            const BlockID got = world.getBlock(wx, kEndExitLevel, wz);
            if (corner) {
                EXPECT_EQ(got, BlockID::air()) << "(" << wx << "," << wz << ")";
            } else if (onEdge) {
                EXPECT_EQ(got, gen.frameEyed) << "(" << wx << "," << wz << ")";
            } else {
                EXPECT_EQ(got, gen.endPortal) << "(" << wx << "," << wz << ")";
            }
        }
    }
    // The standing surface under the center: the plateau's top layer.
    EXPECT_EQ(world.getBlock(0, kEndIslandTop, 0), gen.endstone);
}

TEST(EndGenTest, IslandConstantsPinned) {
    // The 10a's geometry (the framework's invariants — the travel's
    // landing + the exit's level).
    EXPECT_EQ(kEndIslandHalf, 60);
    EXPECT_EQ(kEndIslandBottom, 48);
    EXPECT_EQ(kEndIslandTop, 52);
    EXPECT_EQ(kEndExitLevel, 53);
    // The 10b's content parameters (the oracle's vocabulary — a
    // constant edit re-runs every fill).
    EXPECT_EQ(kEndIslandLattice, 16);
    EXPECT_FLOAT_EQ(kEndIslandProbability, 0.15f);
    EXPECT_EQ(kEndIslandRadiusMin, 5);
    EXPECT_EQ(kEndIslandRadiusMax, 12);
    EXPECT_EQ(kEndIslandTopMin, 44);
    EXPECT_EQ(kEndIslandTopMax, 56);
    EXPECT_EQ(kEndIslandTopLayers, 2);
    EXPECT_EQ(kEndIslandConeLayers, 2);
    EXPECT_EQ(kEndOceanBottom, 0);
    EXPECT_EQ(kEndOceanTop, 4);
    EXPECT_FLOAT_EQ(kEndChorusProbability, 0.6f);
}

// ---------------------------------------------------------------------------
// The SHIPPED data (the real assets — the flags + the statelessness +
// the biome pool + the create against the real registry).
// ---------------------------------------------------------------------------

TEST(ShippedEndBlocks, DataFlagsPinned) {
    // The REAL assets (data/blocks/end*.json + end_stone_bricks +
    // chorus_plant): the three data predicates exactly where they
    // belong (end_portal on the portal only, end_frame on both frames,
    // end_frame_eyed on the eyed frame only) — and all the end fill
    // blocks stateless (the worldgen's ENT-2 premise; the chorus's
    // cross + the bricks' full cube are the 10b's two new stateless
    // members).
    AssetManager assets;
    ASSERT_TRUE(assets.setRoot("assets").ok())
        << "run the tests from the repository root";
    BlockRegistry blocks;
    ASSERT_TRUE(blocks.loadDirectory(assets, "data/blocks").ok());
    struct Expect {
        const char* name;
        bool endPortal;
        bool endFrame;
        bool endFrameEyed;
        bool stateless;
    };
    const Expect expects[] = {
        {"endstone", false, false, false, true},
        {"end_stone_bricks", false, false, false, true},
        {"chorus_plant", false, false, false, true},
        {"end_portal", true, false, false, true},
        {"end_portal_frame", false, true, false, true},
        {"end_portal_frame_eyed", false, true, true, true},
        // the pre-V3-10a blocks keep the false defaults (the bit-
        // identical structural root; nether_portal is stateful by its
        // own axis design — the stateless premise is the END fill's).
        {"nether_portal", false, false, false, false},
        {"netherrack", false, false, false, false},
    };
    for (const Expect& e : expects) {
        const BlockID id = blocks.idForName(e.name);
        ASSERT_TRUE(id.valid()) << e.name;
        const Block& b = blocks.at(id);
        if (e.stateless) {
            EXPECT_FALSE(b.hasState()) << e.name;
        }
        EXPECT_EQ(b.properties.endPortal, e.endPortal) << e.name;
        EXPECT_EQ(b.properties.endFrame, e.endFrame) << e.name;
        EXPECT_EQ(b.properties.endFrameEyed, e.endFrameEyed) << e.name;
    }
    // The biome pool (the real data/biomes_end/ — the single end_plains).
    BiomeRegistry biomes;
    ASSERT_TRUE(biomes.loadDirectory(assets, "data/biomes_end").ok());
    ASSERT_EQ(biomes.count(), 1u);
    EXPECT_EQ(biomes.all()[0].name, "end_plains");
    EXPECT_EQ(biomes.all()[0].surface, std::string("endstone"));
    // EndGen::create against the REAL registry + pool (the init
    // fail-fast's shipped-data witness — the 10b's six names resolve).
    auto r = EndGen::create(endSeed(42), biomes, blocks);
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_TRUE(r->valid());
    EXPECT_TRUE(r->bedrock.valid());
    EXPECT_TRUE(r->chorusPlant.valid());
}

TEST(ShippedEndItems, PearlAndFruitPinned) {
    // The REAL items (data/items/ender_pearl.json + chorus_fruit.json):
    // the pearl's throw channel (the throws_teleport flag + the two
    // sound names + the MC-exact stack 16) + the fruit's food values
    // (the apple's shape, the MC's 2 hunger / 0.0 saturation).
    AssetManager assets;
    ASSERT_TRUE(assets.setRoot("assets").ok())
        << "run the tests from the repository root";
    BlockRegistry blocks;
    ASSERT_TRUE(blocks.loadDirectory(assets, "data/blocks").ok());
    ItemRegistry items;
    ASSERT_TRUE(items.buildBlockItems(blocks).ok());
    ASSERT_TRUE(items.loadDirectory(assets, "data/items").ok());
    const ItemID pearl = items.idForName("ender_pearl");
    ASSERT_TRUE(pearl.valid());
    const Item& p = items.at(pearl);
    EXPECT_TRUE(p.throwsTeleport);
    EXPECT_EQ(p.throwSound, "ender_pearl_throw");
    EXPECT_EQ(p.teleportSound, "ender_pearl_teleport");
    EXPECT_EQ(p.maxStack, 16);
    const ItemID fruit = items.idForName("chorus_fruit");
    ASSERT_TRUE(fruit.valid());
    const Item& f = items.at(fruit);
    EXPECT_FALSE(f.throwsTeleport);
    EXPECT_EQ(f.restoreHunger, 2);
    ASSERT_TRUE(f.saturation.has_value());
    EXPECT_FLOAT_EQ(*f.saturation, 0.0f);
}

// ---------------------------------------------------------------------------
// V3-10d: the dragon arena (pass 4 — the 4 obsidian towers + the 6
// crystals, the air-only belt's frame/plateau protection).
// ---------------------------------------------------------------------------

TEST(EndGenTest, DragonArenaCellsArePinned) {
    const EndGen gen = endGenFixture(endSeed(1));
    World world;
    // The arena's chunk range: the towers' (±50, ±50) 5x5 footprints
    // span chunks -4..3 on both axes (the floorDiv's negative read —
    // -52 is chunk -4, -48 is chunk -3), the exit pair spans -1..0.
    for (int cz = -4; cz <= 3; ++cz) {
        for (int cx = -4; cx <= 3; ++cx) {
            const ChunkPos pos {cx, 3, cz};
            fillEndChunk(gen, world.getOrCreateChunk(pos), pos);
        }
    }
    // The 6 crystal slots (the kEndCrystalSlots' single source — the
    // game's census reads these exact cells).
    const int crystals[6][3] = {{50, 59, 50}, {-50, 59, 50},
                                 {50, 59, -50}, {-50, 59, -50},
                                 {2, 54, 0},    {-2, 54, 0}};
    for (const int* c : crystals) {
        EXPECT_EQ(world.getBlock(c[0], c[1], c[2]), gen.endCrystal)
            << "(" << c[0] << "," << c[1] << "," << c[2] << ")";
    }
    // The 4 towers' 5x5 obsidian columns (y 53..58 — the crystal's
    // y59 sits on the column's apex).
    const int towers[4][2] = {{50, 50}, {-50, 50}, {50, -50}, {-50, -50}};
    for (const int* t : towers) {
        for (int tx = t[0] - 2; tx <= t[0] + 2; ++tx) {
            for (int tz = t[1] - 2; tz <= t[1] + 2; ++tz) {
                for (int ty = 53; ty <= 58; ++ty) {
                    EXPECT_EQ(world.getBlock(tx, ty, tz), gen.obsidian)
                        << "(" << tx << "," << ty << "," << tz << ")";
                }
            }
        }
    }
    // The air-only belt's witness: the exit's FRAME (y53) is untouched
    // by pass 4 (the exit crystal sits at y54, the frame's top face).
    EXPECT_EQ(world.getBlock(2, 53, 0), gen.frameEyed);
    EXPECT_EQ(world.getBlock(-2, 53, 0), gen.frameEyed);
    // The egg is NOT worldgen (the defeat's marker — only the ritual's
    // m_dead frame writes it, the reload's no-respawn gate's source).
    EXPECT_EQ(world.getBlock(0, 54, 0), BlockID::air());
    // The fire's write target is air at generation (the game's section
    // 2's setBlock precondition — the zero-churn's read).
    EXPECT_EQ(world.getBlock(2, 55, 0), BlockID::air());
    EXPECT_EQ(world.getBlock(50, 60, 50), BlockID::air());
}
