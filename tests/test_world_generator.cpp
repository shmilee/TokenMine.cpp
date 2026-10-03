#include <gtest/gtest.h>

#include <algorithm>
#include <map>
#include <optional>
#include <set>
#include <string>

#include "block/block_registry.hpp"
#include "world/biome.hpp"
#include "world/world_generator.hpp"

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

std::string blockDef(const std::string& id) {
    return R"({"id":")" + id +
           R"(","model":{"textures":{"top":"t","side":"t","bottom":"t"}}})";
}

// The block content set the generator needs (mirrors assets/data/blocks).
BlockRegistry makeBlocks() {
    BlockRegistry reg;
    for (const char* name :
         {"stone", "dirt", "grass", "sand", "water", "wood", "leaves",
          "bedrock", "coal_ore", "iron_ore", "gold_ore", "diamond_ore"}) {
        auto r = reg.registerBlock(parseJson(blockDef(name)));
        if (!r.ok()) {
            ADD_FAILURE() << "block '" << name << "': " << r.error().message;
        }
    }
    return reg;
}

BiomeRegistry makeBiomes() {
    BiomeRegistry reg;
    // Registration order mimics the sorted asset file order (desert,
    // forest, plains) and is what cumulative-weight selection walks.
    for (const char* def : {
             R"({"id":"desert","weight":2,"surface":"sand","subsurface":"sand",)"
             R"("subsurface_depth":4,"tree_density":0.0,"height_scale":0.9})",
             R"({"id":"forest","weight":4,"surface":"grass","subsurface":"dirt",)"
             R"("subsurface_depth":3,"tree_density":0.12,"height_scale":1.1})",
             R"({"id":"plains","weight":4,"surface":"grass","subsurface":"dirt",)"
             R"("subsurface_depth":3,"tree_density":0.03,"height_scale":1.0})",
         }) {
        auto r = reg.registerBiome(parseJson(def));
        if (!r.ok()) {
            ADD_FAILURE() << r.error().message;
        }
    }
    return reg;
}

WorldgenParams makeParams() {
    WorldgenParams p;
    // Mirror assets/data/worldgen.json: the y=0 layer is bedrock.
    p.bottomBlock = "bedrock";
    OreConfig coal;
    coal.block = "coal_ore";
    coal.probability = 0.5f;
    coal.veinSize = 8;
    coal.maxY = 80;
    OreConfig iron;
    iron.block = "iron_ore";
    iron.probability = 0.3f;
    iron.veinSize = 8;
    iron.maxY = 56;
    p.ores = {coal, iron};
    return p;
}

struct GenSetup {
    BlockRegistry blocks = makeBlocks();
    BiomeRegistry biomes = makeBiomes();
    WorldgenParams params = makeParams();
};

// A world with one generator, pre-generated over a 2x2x8-chunk region
// (world x,z in [0, 32), all y layers) — enough to observe cross-chunk
// features (trees, veins) near the x/z = 16 boundaries.
struct Region {
    GenSetup setup;
    std::optional<WorldGenerator> gen;
    std::map<ChunkPos, Chunk> chunks;

    explicit Region(std::int64_t seed) {
        auto r = WorldGenerator::create(seed, setup.params, setup.biomes,
                                        setup.blocks);
        if (!r.ok()) {
            ADD_FAILURE() << r.error().message;
            return;
        }
        gen = std::move(*r);
        for (int cx = 0; cx <= 1; ++cx) {
            for (int cz = 0; cz <= 1; ++cz) {
                for (int cy = 0; cy * kChunkSize < kWorldHeight; ++cy) {
                    gen->generateChunk(chunks[{cx, cy, cz}], {cx, cy, cz});
                }
            }
        }
    }

    // Block at world coordinates (missing chunks read as air).
    BlockID at(int x, int y, int z) const {
        if (y < 0 || y >= kWorldHeight) {
            return BlockID::air();
        }
        const BlockPos p{x, y, z};
        const ChunkPos cp = worldToChunk(p);
        const auto it = chunks.find(cp);
        if (it == chunks.end()) {
            return BlockID::air();
        }
        const BlockPos local = worldToChunkLocal(cp, p);
        return it->second.get(local.x, local.y, local.z);
    }

    // Fluid level at world coordinates (missing chunks read as dry).
    std::uint8_t fluidAt(int x, int y, int z) const {
        if (y < 0 || y >= kWorldHeight) {
            return 0;
        }
        const BlockPos p{x, y, z};
        const ChunkPos cp = worldToChunk(p);
        const auto it = chunks.find(cp);
        if (it == chunks.end()) {
            return 0;
        }
        const BlockPos local = worldToChunkLocal(cp, p);
        return it->second.fluidAt(local.x, local.y, local.z);
    }
};

// Find the trunk base (world y of the bottommost wood block) of the column
// at (x, z), or -1 when the column has no wood.
int findTrunkBase(const Region& r, int x, int z) {
    const BlockID woodId = r.setup.blocks.idForName("wood");
    for (int y = 1; y < kWorldHeight; ++y) {
        if (r.at(x, y, z) == woodId) {
            return y;
        }
    }
    return -1;
}

} // namespace

// -- seed reproducibility ----------------------------------------------------

TEST(WorldGeneratorTest, SameSeedProducesIdenticalChunks) {
    Region a(42);
    Region b(42);
    ASSERT_EQ(a.chunks.size(), b.chunks.size());
    for (const auto& [pos, chunk] : a.chunks) {
        const auto it = b.chunks.find(pos);
        ASSERT_NE(it, b.chunks.end());
        const Chunk& other = it->second;
        for (int i = 0; i < Chunk::kVolume; ++i) {
            EXPECT_EQ(chunk.data()[i], other.data()[i])
                << "chunk " << pos.x << "," << pos.y << "," << pos.z
                << " cell " << i;
        }
    }
}

TEST(WorldGeneratorTest, DifferentSeedsProduceDifferentChunks) {
    Region a(1);
    Region b(2);
    int diffs = 0;
    for (const auto& [pos, chunk] : a.chunks) {
        const Chunk& other = b.chunks.at(pos);
        for (int i = 0; i < Chunk::kVolume; ++i) {
            if (chunk.data()[i] != other.data()[i]) {
                ++diffs;
            }
        }
    }
    EXPECT_GT(diffs, 1000);
}

TEST(WorldGeneratorTest, GeneratingTheSameChunkTwiceIsStable) {
    Region r(42);
    Chunk a;
    Chunk b;
    r.gen->generateChunk(a, {0, 0, 0});
    r.gen->generateChunk(b, {0, 0, 0});
    const Chunk& pregenerated = r.chunks.at({0, 0, 0});
    for (int i = 0; i < Chunk::kVolume; ++i) {
        EXPECT_EQ(a.data()[i], b.data()[i]);
        EXPECT_EQ(a.data()[i], pregenerated.data()[i]);
    }
}

TEST(WorldGeneratorTest, NegativeCoordinateChunksAreReproducible) {
    GenSetup s;
    auto res = WorldGenerator::create(7, s.params, s.biomes, s.blocks);
    ASSERT_TRUE(res.ok()) << res.error().message;
    Chunk a;
    Chunk b;
    res->generateChunk(a, {-1, 0, -1});
    res->generateChunk(b, {-1, 0, -1});
    for (int i = 0; i < Chunk::kVolume; ++i) {
        EXPECT_EQ(a.data()[i], b.data()[i]);
    }

    // The column at the world corner (-16, -16) sits at local (0, *, 0) of
    // that chunk and must have the same structure as any other column.
    const int h = res->surfaceHeight(-16, -16);
    const BlockID bedrockId = s.blocks.idForName("bedrock");
    const BlockPos p{-16, 0, -16};
    const BlockPos local = worldToChunkLocal({-1, 0, -1}, p);
    EXPECT_EQ(local.x, 0);
    EXPECT_EQ(local.z, 0);
    EXPECT_EQ(a.get(local.x, 0, local.z), bedrockId);
    EXPECT_GE(h, 8);
    EXPECT_LE(h, kWorldHeight - 16);
}

// -- terrain structure -------------------------------------------------------

TEST(WorldGeneratorTest, SurfaceHeightWithinBounds) {
    Region r(42);
    for (int x = -32; x < 32; ++x) {
        for (int z = -32; z < 32; ++z) {
            const int h = r.gen->surfaceHeight(x, z);
            EXPECT_GE(h, 8);
            EXPECT_LE(h, kWorldHeight - 16);
        }
    }
}

TEST(WorldGeneratorTest, ColumnStructureMatchesBiome) {
    Region r(42);
    const BlockID stoneId = r.setup.blocks.idForName("stone");
    const BlockID coalId = r.setup.blocks.idForName("coal_ore");
    const BlockID ironId = r.setup.blocks.idForName("iron_ore");
    const BlockID bedrockId = r.setup.blocks.idForName("bedrock");
    int checked = 0;
    for (int x = 0; x < 32; x += 4) {
        for (int z = 0; z < 32; z += 4) {
            const int h = r.gen->surfaceHeight(x, z);
            const bool desert = r.gen->biomeNameAt(x, z) == "desert";
            const BlockID surfaceId =
                r.setup.blocks.idForName(desert ? "sand" : "grass");
            const BlockID subsurfId =
                r.setup.blocks.idForName(desert ? "sand" : "dirt");
            const int depth = desert ? 4 : 3;

            // y=0 is the bottom layer, untouched by caves/ores.
            EXPECT_EQ(r.at(x, 0, z), bedrockId);

            if (h >= r.setup.params.seaLevel) {
                // Dry column: surface, subsurface band, deep stone (an ore
                // may legitimately replace the sampled stone cell), air well
                // above (past the maximum tree height).
                EXPECT_EQ(r.at(x, h, z), surfaceId);
                for (int y = h - 1; y >= h - depth && y >= 1; --y) {
                    EXPECT_EQ(r.at(x, y, z), subsurfId);
                }
                // First deep-stone layer: an ore may legitimately replace
                // it, and in the desert biome (deeper subsurface) a cave
                // may have carved it.
                const int stoneY = h - depth - 1;
                if (stoneY >= 1) {
                    const BlockID deep = r.at(x, stoneY, z);
                    EXPECT_TRUE(deep == stoneId || deep == coalId ||
                                deep == ironId || deep == BlockID::air());
                }
                EXPECT_EQ(r.at(x, h + 9, z), BlockID::air());
            } else {
                // Underwater column: fluid (full level) from the surface up
                // to the sea level — the block stays air — and air above.
                for (int y = h + 1; y <= r.setup.params.seaLevel; ++y) {
                    EXPECT_EQ(r.at(x, y, z), BlockID::air());
                    EXPECT_EQ(r.fluidAt(x, y, z), kMaxFluidLevel);
                }
                EXPECT_EQ(r.at(x, r.setup.params.seaLevel + 1, z),
                          BlockID::air());
                EXPECT_EQ(r.fluidAt(x, r.setup.params.seaLevel + 1, z), 0);
            }
            ++checked;
        }
    }
    EXPECT_EQ(checked, 64); // 8x8 sample grid
}

TEST(WorldGeneratorTest, UnderwaterColumnsExist) {
    Region r(42);
    // Wide scan: a small area can legitimately sit on one side of the
    // sea level, but the world must mix land and water.
    int underwater = 0;
    const int kSpan = 192; // x,z in [-96, 96)
    for (int x = -96; x < 96; ++x) {
        for (int z = -96; z < 96; ++z) {
            if (r.gen->surfaceHeight(x, z) < r.setup.params.seaLevel) {
                ++underwater;
            }
        }
    }
    EXPECT_GT(underwater, 0);
    EXPECT_LT(underwater, kSpan * kSpan); // not the whole area
}

TEST(WorldGeneratorTest, CavesCarveBelowSurface) {
    Region r(42);
    int caveBlocks = 0;
    int belowSurface = 0;
    for (int x = 0; x < 32; ++x) {
        for (int z = 0; z < 32; ++z) {
            const int h = r.gen->surfaceHeight(x, z);
            for (int y = r.setup.params.caveMinY;
                 y < h - r.setup.params.surfaceGuard; ++y) {
                ++belowSurface;
                if (r.at(x, y, z) == BlockID::air()) {
                    ++caveBlocks;
                }
            }
        }
    }
    EXPECT_GT(belowSurface, 1000);
    // Caves are a meaningful fraction of the subsurface, not all of it.
    EXPECT_GT(caveBlocks, 100);
    EXPECT_LT(caveBlocks, belowSurface);
}

// -- ores --------------------------------------------------------------------

TEST(WorldGeneratorTest, OresAppearAndRespectMaxY) {
    Region r(42);
    const BlockID coalId = r.setup.blocks.idForName("coal_ore");
    const BlockID ironId = r.setup.blocks.idForName("iron_ore");
    int coal = 0;
    int iron = 0;
    int coalAboveMax = 0;
    int ironAboveMax = 0;
    for (int x = 0; x < 32; ++x) {
        for (int z = 0; z < 32; ++z) {
            for (int y = 1; y < kWorldHeight; ++y) {
                const BlockID id = r.at(x, y, z);
                if (id == coalId) {
                    ++coal;
                    if (y > 80) {
                        ++coalAboveMax;
                    }
                } else if (id == ironId) {
                    ++iron;
                    if (y > 56) {
                        ++ironAboveMax;
                    }
                }
            }
        }
    }
    EXPECT_GT(coal, 10);
    EXPECT_GT(iron, 0);
    EXPECT_EQ(coalAboveMax, 0);
    EXPECT_EQ(ironAboveMax, 0);
}

// -- trees -------------------------------------------------------------------

TEST(WorldGeneratorTest, TreesGrowOnLand) {
    Region r(42);
    const BlockID woodId = r.setup.blocks.idForName("wood");
    const BlockID leavesId = r.setup.blocks.idForName("leaves");
    int wood = 0;
    int leaves = 0;
    for (int x = 0; x < 32; ++x) {
        for (int z = 0; z < 32; ++z) {
            for (int y = 0; y < kWorldHeight; ++y) {
                const BlockID id = r.at(x, y, z);
                if (id == woodId) {
                    ++wood;
                } else if (id == leavesId) {
                    ++leaves;
                }
            }
        }
    }
    EXPECT_GT(wood, 10);
    EXPECT_GT(leaves, 30);
}

TEST(WorldGeneratorTest, TreeTrunksStartAtSurfaceAndAreContiguous) {
    Region r(42);
    const BlockID woodId = r.setup.blocks.idForName("wood");
    int checked = 0;
    for (int x = 0; x < 32; ++x) {
        for (int z = 0; z < 32; ++z) {
            const int bottom = findTrunkBase(r, x, z);
            if (bottom < 0) {
                continue;
            }
            const int h = r.gen->surfaceHeight(x, z);
            // The trunk starts right above the surface ...
            EXPECT_EQ(bottom, h + 1);
            // ... and runs up for min_height .. min_height + height_range.
            int top = bottom;
            while (top + 1 < kWorldHeight && r.at(x, top + 1, z) == woodId) {
                ++top;
            }
            EXPECT_GE(top - h, r.setup.params.treeMinHeight);
            EXPECT_LE(top - h,
                      r.setup.params.treeMinHeight +
                          r.setup.params.treeHeightRange);
            ++checked;
        }
    }
    EXPECT_GT(checked, 0);
}

// The margin mechanism: a tree whose trunk sits exactly on the x=16 chunk
// boundary must be completed by BOTH neighboring chunks. The leaf cell at
// (15, h+trunk, wz) can only be written by chunk (0,*,0), whose candidate
// set extends past its own bounds to the trunk at x=16; the trunk itself
// lives entirely in chunk (1,*,0).
TEST(WorldGeneratorTest, TreeStraddlingChunkBoundaryIsSeamless) {
    const GenSetup setup = GenSetup {};
    const BlockID woodId = setup.blocks.idForName("wood");
    const BlockID leavesId = setup.blocks.idForName("leaves");

    // Find a seed where a trunk base sits at world x=16 and the
    // boundary-crossing leaf cell is present.
    int seed = -1;
    for (int s = 1; s <= 400 && seed < 0; ++s) {
        Region r(s);
        for (int z = 0; z < 32 && seed < 0; ++z) {
            const int bottom = findTrunkBase(r, 16, z);
            if (bottom < 0) {
                continue;
            }
            const int h = r.gen->surfaceHeight(16, z);
            if (bottom != h + 1) {
                continue;
            }
            int top = bottom;
            while (top + 1 < kWorldHeight && r.at(16, top + 1, z) == woodId) {
                ++top;
            }
            const int trunk = top - h;
            if (trunk < setup.params.treeMinHeight ||
                trunk > setup.params.treeMinHeight +
                            setup.params.treeHeightRange) {
                continue;
            }
            if (r.at(15, h + trunk, z) == leavesId) {
                seed = s;
            }
        }
    }
    ASSERT_NE(seed, -1) << "no boundary tree found in 400 seeds";

    Region r(seed);
    int wz = -1;
    int h = -1;
    int trunk = -1;
    for (int z = 0; z < 32 && wz < 0; ++z) {
        const int bottom = findTrunkBase(r, 16, z);
        if (bottom < 0) {
            continue;
        }
        const int hh = r.gen->surfaceHeight(16, z);
        if (bottom != hh + 1) {
            continue;
        }
        int top = bottom;
        while (top + 1 < kWorldHeight && r.at(16, top + 1, z) == woodId) {
            ++top;
        }
        const int t = top - hh;
        if (t < setup.params.treeMinHeight ||
            t > setup.params.treeMinHeight + setup.params.treeHeightRange) {
            continue;
        }
        if (r.at(15, hh + t, z) == leavesId) {
            wz = z;
            h = hh;
            trunk = t;
        }
    }
    ASSERT_GE(wz, 0) << "boundary tree disappeared on re-generation";

    // 1) The trunk is contiguous and lives in chunk (1,*,0).
    for (int ty = 1; ty <= trunk; ++ty) {
        EXPECT_EQ(r.at(16, h + ty, wz), woodId);
    }
    // 2) The crossing canopy cell (owned by chunk (0,*,0)) is present.
    EXPECT_EQ(r.at(15, h + trunk, wz), leavesId);
    // 3) The cap on top of the trunk is present.
    EXPECT_EQ(r.at(16, h + trunk + 1, wz), leavesId);
}

// -- biomes ------------------------------------------------------------------

TEST(WorldGeneratorTest, BiomeSelectionCoversAllBiomes) {
    Region r(42);
    std::set<std::string> seen;
    for (int x = 0; x <= 512; x += 4) {
        for (int z = 0; z <= 512; z += 4) {
            seen.insert(r.gen->biomeNameAt(x, z));
        }
    }
    EXPECT_EQ(seen.size(), 3u);
}

TEST(WorldGeneratorTest, DesertColumnsHaveNoTrees) {
    Region r(42);
    for (int x = 0; x < 32; ++x) {
        for (int z = 0; z < 32; ++z) {
            if (r.gen->biomeNameAt(x, z) != "desert") {
                continue;
            }
            // Desert tree density is 0: no trunk can be rooted in the
            // column. (Trunks are single-column, so this also catches any
            // overhang bug that would plant wood in a desert column.)
            EXPECT_EQ(findTrunkBase(r, x, z), -1)
                << "desert column " << x << "," << z << " hosts a trunk";
        }
    }
    // The generated region may contain no desert at all; a wide scan proves
    // the biome actually occurs in this world.
    std::set<std::string> seen;
    for (int x = -128; x < 128; x += 4) {
        for (int z = -128; z < 128; z += 4) {
            seen.insert(r.gen->biomeNameAt(x, z));
        }
    }
    EXPECT_TRUE(seen.count("desert") != 0)
        << "no desert columns in the 256x256 scan";
}

// -- construction validation --------------------------------------------------

TEST(WorldGeneratorTest, CreateFailsWithoutBiomes) {
    GenSetup s;
    BiomeRegistry empty;
    auto r = WorldGenerator::create(1, s.params, empty, s.blocks);
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("biome"), std::string::npos);
}

TEST(WorldGeneratorTest, CreateFailsOnUnknownBiomeBlock) {
    GenSetup s;
    BiomeRegistry biomes;
    ASSERT_TRUE(biomes
                    .registerBiome(parseJson(
                        R"({"id":"x","surface":"obsidian","subsurface":"dirt"})"))
                    .ok());
    auto r = WorldGenerator::create(1, s.params, biomes, s.blocks);
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("obsidian"), std::string::npos);
}

TEST(WorldGeneratorTest, CreateFailsOnUnknownOreBlock) {
    GenSetup s;
    s.params.ores.clear();
    OreConfig bad;
    bad.block = "uranium_ore";
    bad.probability = 0.1f;
    bad.veinSize = 4;
    bad.maxY = 16;
    s.params.ores.push_back(bad);
    auto r = WorldGenerator::create(1, s.params, s.biomes, s.blocks);
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("uranium_ore"), std::string::npos);
}

TEST(WorldGeneratorTest, CavesCanBeDisabled) {
    GenSetup s;
    s.params.cavesEnabled = false;
    auto res = WorldGenerator::create(42, s.params, s.biomes, s.blocks);
    ASSERT_TRUE(res.ok()) << res.error().message;
    Chunk chunk;
    res->generateChunk(chunk, {0, 0, 0});
    // Chunk (0,0,0) covers world y in [0, 16) (local y == world y). With
    // caves off there are no air pockets below the surface, in dry or
    // underwater columns alike.
    int airPockets = 0;
    for (int lx = 0; lx < kChunkSize; ++lx) {
        for (int lz = 0; lz < kChunkSize; ++lz) {
            const int h = res->surfaceHeight(lx, lz);
            for (int ly = 1; ly < kChunkSize; ++ly) {
                if (chunk.get(lx, ly, lz) != BlockID::air()) {
                    continue;
                }
                const int wy = ly;
                if (h >= s.params.seaLevel) {
                    // Dry column: air is legitimate only above the surface.
                    if (wy < h) {
                        ++airPockets;
                    }
                } else if (wy <= s.params.seaLevel) {
                    // Underwater column: the fill is solid/fluid up to the
                    // sea level, so any air cell not holding fluid is a
                    // pocket.
                    if (chunk.fluidAt(lx, ly, lz) == 0) {
                        ++airPockets;
                    }
                }
            }
        }
    }
    EXPECT_EQ(airPockets, 0);
}
