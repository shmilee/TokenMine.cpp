// V3-10a (v3.23.0): the End's world generation — the pure chunk fill
// (world/end_gen: the 120x120 endstone plateau + the origin exit portal,
// a coordinate-pure function — zero noise, zero randomness, the
// nether_gen's second consumption) + the derived seed (worldSeed ^
// kSaltEnd, never stored) + the shipped end blocks' data flags (the
// end_portal / end_frame / end_frame_eyed predicates, stateless).
#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include "block/block_registry.hpp"
#include "core/asset_manager.hpp"
#include "core/json.hpp"
#include "world/end_gen.hpp"
#include "world/nether_gen.hpp" // kSaltNether (the independence witness)
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
    };
}

// A minimal end fill registry (endstone + obsidian + end_portal +
// end_portal_frame_eyed — the four 10a names, the data flags set the
// way the shipped data sets them).
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

EndGen endGenFixture(std::int64_t seed) {
    const BlockRegistry reg = endRegistry();
    EndGen gen;
    gen.seed = seed;
    gen.endstone = reg.idForName("endstone");
    gen.obsidian = reg.idForName("obsidian");
    gen.endPortal = reg.idForName("end_portal");
    gen.frameEyed = reg.idForName("end_portal_frame_eyed");
    return gen;
}

// The expected block of a world cell under the 10a fill (the tests'
// oracle — the island plateau band + the origin exit ring/center at the
// body level; everything else void air).
BlockID expectedCell(const EndGen& gen, int wx, int wy, int wz) {
    const bool onIsland = std::abs(wx) <= kEndIslandHalf &&
        std::abs(wz) <= kEndIslandHalf && wy >= kEndIslandBottom &&
        wy <= kEndIslandTop;
    if (onIsland) {
        return gen.endstone;
    }
    if (wy == kEndExitLevel && std::abs(wx) <= 2 && std::abs(wz) <= 2) {
        const bool corner = (std::abs(wx) == 2) && (std::abs(wz) == 2);
        if (corner) {
            return BlockID::air();
        }
        if (std::abs(wx) <= 1 && std::abs(wz) <= 1) {
            return gen.endPortal;
        }
        return gen.frameEyed;
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
                    expectedCell(gen, ox + lx, baseY + ly, oz + lz);
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
}

// ---------------------------------------------------------------------------
// EndGen::create (the fail-fast resolve — the NetherGen::create twin).
// ---------------------------------------------------------------------------

TEST(EndGenTest, CreateResolvesTheFourNames) {
    const BlockRegistry reg = endRegistry();
    auto r = EndGen::create(endSeed(7), reg);
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_EQ(r->endstone, reg.idForName("endstone"));
    EXPECT_EQ(r->obsidian, reg.idForName("obsidian"));
    EXPECT_EQ(r->endPortal, reg.idForName("end_portal"));
    EXPECT_EQ(r->frameEyed, reg.idForName("end_portal_frame_eyed"));
    EXPECT_EQ(r->seed, endSeed(7));
}

TEST(EndGenTest, CreateNamesTheMissingBlock) {
    // A registry WITHOUT endstone: the first missing name is named
    // (registry resolve order: endstone first).
    BlockRegistry reg;
    const std::vector<std::string> defs = minimalEndDefs();
    for (const std::string& def : {defs[1], defs[2], defs[3]}) {
        ASSERT_TRUE(reg.registerBlock(parseDef(def)).ok());
    }
    auto r = EndGen::create(1, reg);
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("endstone"), std::string::npos);
    EXPECT_NE(r.error().message.find("end worldgen"), std::string::npos);
}

TEST(EndGenTest, CreateRejectsAStatefulFillBlock) {
    // The ENT-2 worldgen rule: a per-instance state is a journal the
    // fill cannot write — a stateful endstone is named.
    BlockRegistry reg;
    const std::string statefulEndstone =
        "{\"id\":\"endstone\",\"properties\":{},"
        "\"model\":{\"state_count\":2,\"state_default\":0,"
        "\"textures\":{\"top\":\"e\",\"side\":\"e\",\"bottom\":\"e\"}}}";
    const std::vector<std::string> defs = minimalEndDefs();
    for (const std::string& def :
         {statefulEndstone, defs[1], defs[2], defs[3]}) {
        ASSERT_TRUE(reg.registerBlock(parseDef(def)).ok());
    }
    auto r = EndGen::create(1, reg);
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("endstone"), std::string::npos);
    EXPECT_NE(r.error().message.find("per-instance state"),
              std::string::npos);
}

// ---------------------------------------------------------------------------
// The island plateau band (the oracle-checked fill).
// ---------------------------------------------------------------------------

TEST(EndGenTest, IslandPlateauChunk) {
    const EndGen gen = endGenFixture(endSeed(1));
    World world;
    // Chunk (0, 3, 0): y 48..63 — the island's five layers at the chunk
    // top edge (y 48..52) + the exit level 53 + the open void above.
    Chunk& chunk = world.getOrCreateChunk({0, 3, 0});
    fillEndChunk(gen, chunk, {0, 3, 0});
    expectFillMatchesOracle(chunk, {0, 3, 0}, gen);
}

TEST(EndGenTest, IslandEdgeChunk) {
    // Chunk (3, 3, 3): x 48..63 — the island boundary |x| <= 60 cuts
    // the chunk (x 48..60 plateau, x 61..63 void).
    const EndGen gen = endGenFixture(endSeed(1));
    World world;
    Chunk& chunk = world.getOrCreateChunk({3, 3, 3});
    fillEndChunk(gen, chunk, {3, 3, 3});
    expectFillMatchesOracle(chunk, {3, 3, 3}, gen);
}

TEST(EndGenTest, NegativeCoordChunk) {
    // Chunk (-4, 3, -4): x, z -64..-49 — the negative-boundary floorDiv
    // twin (|x| <= 60 still holds down to -60).
    const EndGen gen = endGenFixture(endSeed(1));
    World world;
    Chunk& chunk = world.getOrCreateChunk({-4, 3, -4});
    fillEndChunk(gen, chunk, {-4, 3, -4});
    expectFillMatchesOracle(chunk, {-4, 3, -4}, gen);
}

TEST(EndGenTest, VoidBeyondTheIsland) {
    // Chunk (5, 3, 0): x 80..95 — beyond |x| <= 60: all air (the void,
    // no floor).
    const EndGen gen = endGenFixture(endSeed(1));
    World world;
    Chunk& chunk = world.getOrCreateChunk({5, 3, 0});
    fillEndChunk(gen, chunk, {5, 3, 0});
    for (int ly = 0; ly < kChunkSize; ++ly) {
        for (int lz = 0; lz < kChunkSize; ++lz) {
            for (int lx = 0; lx < kChunkSize; ++lx) {
                EXPECT_EQ(chunk.get(lx, ly, lz), BlockID::air());
            }
        }
    }
}

TEST(EndGenTest, FillIsChunkOrderIndependent) {
    // The pure-function contract: two fills of the same position into
    // two fresh chunks are identical (the streaming jobs' arbitrary
    // scheduling premise).
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

TEST(EndGenTest, ExitPortalCellsAtTheOrigin) {
    // The 5x5 ring at the body level: the 12 frame cells all EYED, the
    // 3x3 center all end_portal, the four corners void air (the 10a
    // worldgen ships the ACTIVE exit — the return portal).
    const EndGen gen = endGenFixture(endSeed(1));
    World world;
    // The origin ring spans x, z -2..2 = the four chunks at the
    // negative/positive quadrant boundary (fill them all).
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
    // The standing surface under the center: the island's top layer.
    EXPECT_EQ(world.getBlock(0, kEndIslandTop, 0), gen.endstone);
}

TEST(EndGenTest, IslandConstantsPinned) {
    EXPECT_EQ(kEndIslandHalf, 60);
    EXPECT_EQ(kEndIslandBottom, 48);
    EXPECT_EQ(kEndIslandTop, 52);
    EXPECT_EQ(kEndExitLevel, 53);
}

// ---------------------------------------------------------------------------
// The SHIPPED data (the real assets — the flags + the statelessness).
// ---------------------------------------------------------------------------

TEST(ShippedEndBlocks, DataFlagsPinned) {
    // The REAL assets (data/blocks/end*.json): the three data predicates
    // exactly where they belong (end_portal on the portal only, end_frame
    // on both frames, end_frame_eyed on the eyed frame only) — and all
    // four fill blocks stateless (the worldgen's ENT-2 premise).
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
        bool stateless; // the worldgen fill vocabulary must be stateless
    };
    const Expect expects[] = {
        {"endstone", false, false, false, true},
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
    // EndGen::create against the REAL registry (the init fail-fast's
    // shipped-data witness).
    auto r = EndGen::create(endSeed(42), blocks);
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_TRUE(r->valid());
}
