#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <climits>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "block/block_registry.hpp"
#include "core/asset_manager.hpp"
#include "world/biome.hpp"
#include "world/noise.hpp"
#include "world/ore_vein.hpp"
#include "world/world_generator.hpp"
#include "world/worldgen_config.hpp"

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
          "bedrock", "coal_ore", "iron_ore", "gold_ore", "diamond_ore",
          // Phase 45: decoration sources + the lapis ore vein.
          "lapis_ore", "cactus", "allium", "tulip", "cornflower"}) {
        auto r = reg.registerBlock(parseJson(blockDef(name)));
        if (!r.ok()) {
            ADD_FAILURE() << "block '" << name << "': " << r.error().message;
        }
    }
    // ENT-2: a STATEFUL block (the Stairs shape = hasState) — the init
    // cross-check tests point a worldgen placement source at it. Nothing
    // else references it (no behavior change for the existing tests).
    auto s = reg.registerBlock(parseJson(
        R"({"id":"stone_stairs","model":{"shape":"stairs","textures":{"top":"t","side":"t","bottom":"t"}}})"));
    if (!s.ok()) {
        ADD_FAILURE() << "block 'stone_stairs': " << s.error().message;
    }
    return reg;
}

BiomeRegistry makeBiomes() {
    BiomeRegistry reg;
    // Registration order mimics the sorted asset file order (desert,
    // forest, plains) and is what cumulative-weight selection walks.
    for (const char* def : {
             // Phase 45: surface decorations (deliberately dense so the
             // tests see them in a 32x32 region).
             R"({"id":"desert","weight":2,"surface":"sand","subsurface":"sand",)"
             R"("subsurface_depth":4,"tree_density":0.0,"height_scale":0.9,)"
             R"("decorations":[{"block":"cactus","probability":0.35}]})",
             R"({"id":"forest","weight":4,"surface":"grass","subsurface":"dirt",)"
             R"("subsurface_depth":3,"tree_density":0.12,"height_scale":1.1,)"
             R"("decorations":[{"block":"allium","probability":0.35},)"
             R"({"block":"tulip","probability":0.35},)"
             R"({"block":"cornflower","probability":0.35}]})",
             R"({"id":"plains","weight":4,"surface":"grass","subsurface":"dirt",)"
             R"("subsurface_depth":3,"tree_density":0.03,"height_scale":1.0,)"
             R"("decorations":[{"block":"allium","probability":0.35},)"
             R"({"block":"tulip","probability":0.35},)"
             R"({"block":"cornflower","probability":0.35}]})",
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
    // Phase 45: the lapis vein (mirrors the asset's max_y 32).
    OreConfig lapis;
    lapis.block = "lapis_ore";
    lapis.probability = 0.08f;
    lapis.veinSize = 6;
    lapis.maxY = 32;
    p.ores = {coal, iron, lapis};
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

// ---------------------------------------------------------------------------
// GEN-1 (spaghetti caves) helpers.
//
// The origin Region (cols 0..1) sits INSIDE the spawn clear zone (|col| <=
// 2) — uncarved by design — so the cave tests generate their own regions
// at cols 4+ (world x,z >= 64), outside the zone.
// ---------------------------------------------------------------------------

// The GEN-1 cave tuning (calibrated at seed 123456789: 5.38% of the
// subsurface band carved, ~95 surface openings per 64x64, p90 straight
// run width 6/5, clear zone + lake boxes unscathed, 1.18 ms/col). This
// fixture value pins the PREDICATE/GEOMETRY behaviour below (it is a
// legal cs/t pair, independent of what is shipped); the SHIPPED tuning
// is pinned in test_worldgen_config.cpp (content) and test_cave_probe.cpp
// (GEN-4 calibration conclusion).
WorldgenParams calibratedCaveParams() {
    WorldgenParams p = makeParams();
    p.caveNoiseScale = 0.09f;
    p.caveTubeThreshold = 0.06f;
    p.surfaceGuard = 0;
    p.caveSpawnClearColumns = 2;
    return p;
}

// Generate all y-layers of the columns [cx0, cx1] x [cz0, cz1].
std::map<ChunkPos, Chunk> genColumns(const WorldGenerator& gen, int cx0,
                                     int cx1, int cz0, int cz1) {
    std::map<ChunkPos, Chunk> chunks;
    for (int cx = cx0; cx <= cx1; ++cx) {
        for (int cz = cz0; cz <= cz1; ++cz) {
            for (int cy = 0; cy * kChunkSize < kWorldHeight; ++cy) {
                gen.generateChunk(chunks[{cx, cy, cz}], {cx, cy, cz});
            }
        }
    }
    return chunks;
}

BlockID blockAt(const std::map<ChunkPos, Chunk>& chunks, int x, int y,
                int z) {
    if (y < 0 || y >= kWorldHeight) {
        return BlockID::air();
    }
    const ChunkPos cp = worldToChunk(BlockPos{x, y, z});
    const auto it = chunks.find(cp);
    if (it == chunks.end()) {
        return BlockID::air();
    }
    const auto local = worldToChunkLocal(cp, BlockPos{x, y, z});
    return it->second.get(local.x, local.y, local.z);
}

std::uint8_t fluidAt(const std::map<ChunkPos, Chunk>& chunks, int x, int y,
                     int z) {
    if (y < 0 || y >= kWorldHeight) {
        return 0;
    }
    const ChunkPos cp = worldToChunk(BlockPos{x, y, z});
    const auto it = chunks.find(cp);
    if (it == chunks.end()) {
        return 0;
    }
    const auto local = worldToChunkLocal(cp, BlockPos{x, y, z});
    return it->second.fluidAt(local.x, local.y, local.z);
}

// A carved cell: air with no fluid (the open sky above the surface and the
// underwater fill are air too, but callers restrict to the subsurface band
// / dry columns where those do not occur).
bool carvedAt(const std::map<ChunkPos, Chunk>& chunks, int x, int y, int z) {
    return blockAt(chunks, x, y, z) == BlockID::air() &&
           fluidAt(chunks, x, y, z) == 0;
}

// ---------------------------------------------------------------------------
// GEN-5 (floating tree guard) helpers.
//
// The guard skips a tree whose trunk column's surface cell is cave-carved
// (the shipped surface_guard 0 = the carving reaches the surface cell
// itself). The scans below work WITHOUT generating chunks for every
// candidate: placeTrees' per-column decision is pure (the biome density
// roll on the tree lattice + the dry-column check), and the carve check is
// caveCarvesCell — so a "floating tree" case is a lattice column whose roll
// passes, that is dry, and whose surface cell the predicate carves.
// ---------------------------------------------------------------------------

// The fixture biome tree densities (makeBiomes): the ResolvedBiome is
// private, so placeTrees' roll is recomputed from the public biome name.
float gen5TreeDensity(const WorldGenerator& gen, int wx, int wz) {
    const std::string& name = gen.biomeNameAt(wx, wz);
    if (name == "forest") {
        return 0.12f;
    }
    if (name == "plains") {
        return 0.03f;
    }
    return 0.0f; // the desert plants no trees
}

// placeTrees' per-column roll, its exact expression (the kSaltTree salt).
bool gen5RollPasses(const WorldGenerator& gen, int wx, int wz) {
    const float density = gen5TreeDensity(gen, wx, wz);
    if (density <= 0.0f) {
        return false;
    }
    const std::uint64_t h =
        hash2(static_cast<std::uint64_t>(static_cast<std::int64_t>(wx)),
              static_cast<std::uint64_t>(static_cast<std::int64_t>(wz)),
              static_cast<std::uint64_t>(gen.seed() ^ kSaltTree));
    return static_cast<float>(h >> 40) * kLattice24Scale < density;
}

// The tree footprint (trunk ± 2 = the generator's canopy radius) straddles
// a chunk boundary exactly when the trunk (a multiple of the lattice 4)
// sits on a chunk's west edge (wx % 16 == 0 → wx - 2 is in the previous
// chunk) or its north edge (wz % 16 == 0 → wz - 2 is).
bool gen5FootprintCrosses(int wx, int wz) {
    return (wx % kChunkSize) == 0 || (wz % kChunkSize) == 0;
}

struct Gen5Case {
    std::int64_t seed = -1;
    int wx = 0, wz = 0, hgt = 0;
};

// The scan region: world x [64, 96) x z [0, 64) (lattice step 4) — OUTSIDE
// the spawn clear zone (|chunk col| <= 2 → world < 48), which never carves
// and would mask the guard; the fixture has no lakes.
constexpr int kGen5X0 = 64, kGen5X1 = 96, kGen5Z0 = 0, kGen5Z1 = 64;

// Every y-layer of the chunks covering trunk ± 2 (the canopy radius).
void gen5GenerateBox(const WorldGenerator& gen,
                     std::map<ChunkPos, Chunk>& chunks, int wx, int wz) {
    const int cx0 = floorDiv(wx - 2, kChunkSize);
    const int cx1 = floorDiv(wx + 2, kChunkSize);
    const int cz0 = floorDiv(wz - 2, kChunkSize);
    const int cz1 = floorDiv(wz + 2, kChunkSize);
    for (int cx = cx0; cx <= cx1; ++cx) {
        for (int cz = cz0; cz <= cz1; ++cz) {
            for (int cy = 0; cy * kChunkSize < kWorldHeight; ++cy) {
                const ChunkPos cp{cx, cy, cz};
                if (chunks.find(cp) == chunks.end()) {
                    gen.generateChunk(chunks[cp], cp);
                }
            }
        }
    }
}

// The first seed + tree column (roll passes, dry) with the requested
// surface-carve flag and footprint-cross flag; an empty case (seed -1)
// when none within maxSeeds.
Gen5Case findGen5Case(const GenSetup& s, bool wantCarved, bool wantCross,
                      int maxSeeds) {
    const int t = s.params.treeLattice;
    for (std::int64_t seed = 1; seed <= maxSeeds; ++seed) {
        auto res = WorldGenerator::create(seed, s.params, s.biomes, s.blocks);
        if (!res.ok()) {
            continue;
        }
        const WorldGenerator& gen = *res;
        for (int wx = kGen5X0; wx < kGen5X1; wx += t) {
            for (int wz = kGen5Z0; wz < kGen5Z1; wz += t) {
                if (!gen5RollPasses(gen, wx, wz)) {
                    continue;
                }
                const int hgt = gen.surfaceHeight(wx, wz);
                if (hgt <= s.params.seaLevel) {
                    continue;
                }
                const bool carved = gen.caveCarvesCell(wx, hgt, wz, hgt);
                if (carved != wantCarved ||
                    gen5FootprintCrosses(wx, wz) != wantCross) {
                    continue;
                }
                return {seed, wx, wz, hgt};
            }
        }
    }
    return {};
}

// A CROSSING case validated on generated chunks: for each seed the
// validation world is built with `validateCavesOn`, the candidate (roll
// passes, dry, carved flag `wantCarved` on the caves-on predicate,
// footprint crosses) must have its tree actually planted THERE (the trunk
// contiguous from the surface) and its seam cell — the canopy cell one
// block past the boundary edge, at the trunk top level (written by EXACTLY
// this tree: the nearest other trunk is 4 lattice steps away, past the
// radius-2 canopy) — must hold leaves. A terrain hill swallows the leaf
// and would mask the cross-chunk assertion, so such candidates are
// skipped. `requireNeighborsUnguarded` (used for the caves-on POSITIVE
// case) additionally rejects a candidate when a face/diagonal neighbor
// lattice column is also guarded: the neighbor's skipped leaves would
// pollute the "only THIS tree's contribution is gone" argument.
Gen5Case findGen5CrossCase(const GenSetup& s, bool wantCarved, int maxSeeds,
                           bool validateCavesOn,
                           bool requireNeighborsUnguarded) {
    const int t = s.params.treeLattice;
    const BlockID woodId = s.blocks.idForName("wood");
    const BlockID leavesId = s.blocks.idForName("leaves");
    for (std::int64_t seed = 1; seed <= maxSeeds; ++seed) {
        WorldgenParams pVal = s.params;
        if (!validateCavesOn) {
            pVal.cavesEnabled = false;
        }
        auto resVal = WorldGenerator::create(seed, pVal, s.biomes, s.blocks);
        if (!resVal.ok()) {
            continue;
        }
        const WorldGenerator& genVal = *resVal;
        // The carve predicate + the neighbor guard check are properties of
        // the SHIPPED (caves-on) world, independent of the validation
        // world's caves flag.
        auto resOn = WorldGenerator::create(seed, s.params, s.biomes,
                                            s.blocks);
        if (!resOn.ok()) {
            continue;
        }
        const WorldGenerator& genOn = *resOn;
        for (int wx = kGen5X0; wx < kGen5X1; wx += t) {
            for (int wz = kGen5Z0; wz < kGen5Z1; wz += t) {
                if (!gen5RollPasses(genOn, wx, wz)) {
                    continue;
                }
                const int hgt = genOn.surfaceHeight(wx, wz);
                if (hgt <= s.params.seaLevel) {
                    continue;
                }
                if (genOn.caveCarvesCell(wx, hgt, wz, hgt) != wantCarved) {
                    continue;
                }
                if (!gen5FootprintCrosses(wx, wz)) {
                    continue;
                }
                if (requireNeighborsUnguarded) {
                    bool neighborsClear = true;
                    for (int dx = -t; dx <= t && neighborsClear; dx += t) {
                        for (int dz = -t; dz <= t && neighborsClear;
                             dz += t) {
                            if (dx == 0 && dz == 0) {
                                continue;
                            }
                            const int nx = wx + dx, nz = wz + dz;
                            if (nx < kGen5X0 || nx >= kGen5X1 ||
                                nz < kGen5Z0 || nz >= kGen5Z1) {
                                continue;
                            }
                            const int nh = genOn.surfaceHeight(nx, nz);
                            if (nh > s.params.seaLevel &&
                                gen5RollPasses(genOn, nx, nz) &&
                                genOn.caveCarvesCell(nx, nh, nz, nh)) {
                                neighborsClear = false;
                            }
                        }
                    }
                    if (!neighborsClear) {
                        continue;
                    }
                }
                std::map<ChunkPos, Chunk> chunks;
                gen5GenerateBox(genVal, chunks, wx, wz);
                // The trunk: contiguous wood from the surface cell up.
                int bottom = -1, top = -1;
                for (int y = hgt + 1; y < kWorldHeight; ++y) {
                    if (blockAt(chunks, wx, y, wz) == woodId) {
                        if (bottom < 0) {
                            bottom = y;
                        }
                        top = y;
                    } else {
                        break;
                    }
                }
                if (bottom != hgt + 1 ||
                    top - hgt < s.params.treeMinHeight ||
                    top - hgt > s.params.treeMinHeight +
                                    s.params.treeHeightRange) {
                    continue; // the tree is not planted as expected
                }
                // The seam cell: one past the boundary edge, at the trunk
                // top level (a full-radius canopy layer, never a corner).
                const int sx = (wx % kChunkSize == 0) ? wx - 1 : wx;
                const int sz = (wx % kChunkSize == 0) ? wz : wz - 1;
                if (blockAt(chunks, sx, top, sz) != leavesId) {
                    continue; // terrain swallowed the seam leaf
                }
                return {seed, wx, wz, hgt};
            }
        }
    }
    return {};
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
    // GEN-1 migration: the pre-GEN-1 scan region [0,32)² lies INSIDE the
    // spawn clear zone (|col| <= 2 — uncarved by design), so the scan moves
    // to cols 4..5 (x,z in [64,96)), outside the zone. The band now runs
    // up to the surface (surface_guard 0) and the assertions are
    // recalibrated to the spaghetti density (~5% of the band).
    GenSetup s;
    s.params = calibratedCaveParams();
    auto res = WorldGenerator::create(42, s.params, s.biomes, s.blocks);
    ASSERT_TRUE(res.ok()) << res.error().message;
    const WorldGenerator& gen = *res;
    const auto chunks = genColumns(gen, 4, 5, 4, 5);
    int caveBlocks = 0;
    int belowSurface = 0;
    for (int x = 64; x < 96; ++x) {
        for (int z = 64; z < 96; ++z) {
            const int h = gen.surfaceHeight(x, z);
            for (int y = s.params.caveMinY; y <= h; ++y) {
                ++belowSurface;
                if (carvedAt(chunks, x, y, z)) {
                    ++caveBlocks;
                }
            }
        }
    }
    EXPECT_GT(belowSurface, 1000);
    // Caves are a meaningful fraction of the subsurface band ...
    EXPECT_GT(caveBlocks, 1000);
    // ... but not all of it.
    EXPECT_LT(caveBlocks, belowSurface / 2);
}

TEST(WorldGeneratorTest, CaveMatchesPurePredicate) {
    // Per-bit pin: a cell at or below the surface is carved IFF it is in
    // the band [caveMinY, h] (surface_guard 0 includes the surface cell),
    // and BOTH noise mid-bands hold (|n - 0.5| < tube_threshold on the
    // kSaltCave AND the kSaltCave2 fields). Chunk {4,0,0} (x 64..79,
    // z 0..15) is outside the clear zone, and the fixture has no lakes, so
    // the predicate is exactly the generator's carve decision.
    GenSetup s;
    s.params = calibratedCaveParams();
    const std::int64_t seed = 123456789;
    auto res = WorldGenerator::create(seed, s.params, s.biomes, s.blocks);
    ASSERT_TRUE(res.ok()) << res.error().message;
    const WorldGenerator& gen = *res;
    const ChunkPos pos{4, 0, 0};
    Chunk chunk;
    gen.generateChunk(chunk, pos);
    const auto cellIsCarved = [&](int x, int y, int z) {
        const auto local = worldToChunkLocal(pos, BlockPos{x, y, z});
        return chunk.get(local.x, local.y, local.z) == BlockID::air() &&
               chunk.fluidAt(local.x, local.y, local.z) == 0;
    };
    const float cs = s.params.caveNoiseScale;
    const float t = s.params.caveTubeThreshold;
    int checked = 0;
    for (int lx = 0; lx < kChunkSize; ++lx) {
        for (int lz = 0; lz < kChunkSize; ++lz) {
            const int x = pos.x * kChunkSize + lx;
            const int z = pos.z * kChunkSize + lz;
            const int h = gen.surfaceHeight(x, z);
            for (int ly = 0; ly < kChunkSize; ++ly) {
                const int y = pos.y * kChunkSize + ly;
                if (y > h) {
                    continue; // open sky: out of scope
                }
                const int topExclusive = s.params.surfaceGuard > 0
                                             ? h - s.params.surfaceGuard
                                             : h + 1;
                bool expectCarved = false;
                if (y >= s.params.caveMinY && y < topExclusive) {
                    const float n1 =
                        valueNoise3(static_cast<float>(x) * cs,
                                    static_cast<float>(y) * cs,
                                    static_cast<float>(z) * cs,
                                    seed ^ kSaltCave);
                    const float n2 =
                        valueNoise3(static_cast<float>(x) * cs,
                                    static_cast<float>(y) * cs,
                                    static_cast<float>(z) * cs,
                                    seed ^ kSaltCave2);
                    expectCarved = std::fabs(n1 - 0.5f) < t &&
                                   std::fabs(n2 - 0.5f) < t;
                }
                const bool carved = cellIsCarved(x, y, z);
                if (expectCarved) {
                    EXPECT_TRUE(carved) << "x=" << x << " y=" << y << " z=" << z;
                } else {
                    EXPECT_FALSE(carved) << "x=" << x << " y=" << y << " z=" << z;
                }
                ++checked;
            }
        }
    }
    EXPECT_GT(checked, 4000);
}

TEST(WorldGeneratorTest, SpaghettiTubeWidth) {
    // The carved runs are THIN tubes, not rooms: over the calibrated
    // sample region (cols 4..8 x 0..4, the probe area) the straight carved
    // runs have p90 <= 8 and a clear majority in [2, 8]. The occasional
    // long straight stretch (max ~17, a tube running along the scan
    // direction) is the spaghetti shape, not a blob.
    GenSetup s;
    s.params = calibratedCaveParams();
    const std::int64_t seed = 123456789;
    auto res = WorldGenerator::create(seed, s.params, s.biomes, s.blocks);
    ASSERT_TRUE(res.ok()) << res.error().message;
    const WorldGenerator& gen = *res;
    const auto chunks = genColumns(gen, 4, 8, 0, 4);
    const int X0 = 64, X1 = 143, Z0 = 0, Z1 = 79;
    std::vector<int> hRuns, vRuns;
    for (int z = Z0; z <= Z1; ++z) {
        for (int x = X0; x <= X1; ++x) {
            const int h = gen.surfaceHeight(x, z);
            if (h < s.params.seaLevel) {
                continue; // dry columns only
            }
            int run = 0;
            for (int y = s.params.caveMinY; y <= h; ++y) {
                if (carvedAt(chunks, x, y, z)) {
                    ++run;
                } else if (run > 0) {
                    vRuns.push_back(run);
                    run = 0;
                }
            }
            if (run > 0) {
                vRuns.push_back(run);
            }
        }
    }
    const int kMaxBandY = kWorldHeight - 16; // heightAt's upper clamp
    for (int y = s.params.caveMinY; y <= kMaxBandY; ++y) {
        for (int z = Z0; z <= Z1; ++z) {
            int run = 0;
            for (int x = X0; x <= X1; ++x) {
                const int h = gen.surfaceHeight(x, z);
                const bool cand = h >= s.params.seaLevel && y <= h;
                if (cand && carvedAt(chunks, x, y, z)) {
                    ++run;
                } else if (run > 0) {
                    hRuns.push_back(run);
                    run = 0;
                }
            }
            if (run > 0) {
                hRuns.push_back(run);
            }
        }
    }
    ASSERT_GE(hRuns.size(), 500u);
    ASSERT_GE(vRuns.size(), 500u);
    const auto p90 = [](const std::vector<int>& runs) {
        std::vector<int> sorted = runs;
        std::sort(sorted.begin(), sorted.end());
        return sorted[sorted.size() * 9 / 10];
    };
    const auto fracInRange = [](const std::vector<int>& runs) {
        long in = 0;
        for (int r : runs) {
            if (r >= 2 && r <= 8) {
                ++in;
            }
        }
        return 100.0 * static_cast<double>(in) / runs.size();
    };
    EXPECT_LE(p90(hRuns), 8);
    EXPECT_LE(p90(vRuns), 8);
    EXPECT_GE(fracInRange(hRuns), 50.0);
    EXPECT_GE(fracInRange(vRuns), 50.0);
}

TEST(WorldGeneratorTest, SpawnClearZoneUnscathed) {
    // The clear zone (|col| <= 2, the spawn candidate columns {-1,0}² plus
    // margin) is NEVER carved: with caves on vs off the zone generates
    // byte-identical (terrain/ores/trees/lakes run the same in both).
    GenSetup s;
    s.params = calibratedCaveParams();
    const std::int64_t seed = 123456789;
    auto onRes = WorldGenerator::create(seed, s.params, s.biomes, s.blocks);
    ASSERT_TRUE(onRes.ok()) << onRes.error().message;
    WorldgenParams offParams = s.params;
    offParams.cavesEnabled = false;
    auto offRes =
        WorldGenerator::create(seed, offParams, s.biomes, s.blocks);
    ASSERT_TRUE(offRes.ok()) << offRes.error().message;
    int diffs = 0;
    for (int cx = -2; cx <= 2; ++cx) {
        for (int cz = -2; cz <= 2; ++cz) {
            for (int cy = 0; cy * kChunkSize < kWorldHeight; ++cy) {
                Chunk a, b;
                onRes->generateChunk(a, {cx, cy, cz});
                offRes->generateChunk(b, {cx, cy, cz});
                for (int i = 0; i < Chunk::kVolume; ++i) {
                    if (a.data()[i] != b.data()[i]) {
                        ++diffs;
                    }
                }
            }
        }
    }
    EXPECT_EQ(diffs, 0) << "the spawn clear zone must never be cave-carved";
}

TEST(WorldGeneratorTest, LavaLakeExclusion) {
    // A lake's exclusion box (the region above its floor) is NEVER
    // cave-carved: caves on vs off are byte-identical INSIDE the box,
    // differ OUTSIDE it (caves still work around the lake), and the basin
    // geometry matches the recomputed region-min-minus-depth (the
    // precomputed ResolvedLake floor, the per-chunk rescan's value).
    GenSetup s;
    s.params = calibratedCaveParams();
    WorldgenParams::LavaLake lake;
    lake.centerX = 55;
    lake.centerZ = 45;
    lake.radius = 3;
    lake.depth = 3;
    lake.fill = 2;
    s.params.lavaLakes = {lake};
    const std::int64_t seed = 123456789;
    auto onRes = WorldGenerator::create(seed, s.params, s.biomes, s.blocks);
    ASSERT_TRUE(onRes.ok()) << onRes.error().message;
    WorldgenParams offParams = s.params;
    offParams.cavesEnabled = false;
    auto offRes =
        WorldGenerator::create(seed, offParams, s.biomes, s.blocks);
    ASSERT_TRUE(offRes.ok()) << offRes.error().message;

    // The recomputed floor (region min heightAt - depth).
    const WorldGenerator& gen = *onRes;
    int minH = INT_MAX;
    for (int x = 52; x <= 58; ++x) {
        for (int z = 42; z <= 48; ++z) {
            minH = std::min(minH, gen.surfaceHeight(x, z));
        }
    }
    const int floorY = minH - lake.depth;
    const auto inBox = [&](int x, int y, int z) {
        return x >= 52 && x <= 58 && z >= 42 && z <= 48 && y > floorY;
    };

    // The lake spans cols 3 (x 48..63) x 2..3 (z 32..63); generate both.
    const auto onChunks = genColumns(gen, 2, 3, 2, 3);
    int boxDiff = 0, outsideDiff = 0;
    for (const auto& [pos, a] : onChunks) {
        Chunk b;
        offRes->generateChunk(b, pos);
        for (int i = 0; i < Chunk::kVolume; ++i) {
            if (a.data()[i] == b.data()[i]) {
                continue;
            }
            const int lx = i % kChunkSize;
            const int lz = (i / kChunkSize) % kChunkSize;
            const int ly = i / (kChunkSize * kChunkSize);
            const int wx = pos.x * kChunkSize + lx;
            const int wz = pos.z * kChunkSize + lz;
            const int wy = pos.y * kChunkSize + ly;
            if (inBox(wx, wy, wz)) {
                ++boxDiff;
            } else {
                ++outsideDiff;
            }
        }
    }
    EXPECT_EQ(boxDiff, 0) << "no cave carving inside the lake exclusion box";
    EXPECT_GT(outsideDiff, 100) << "caves still work outside the box";

    // The basin: carved to air exactly above the recomputed floor ...
    for (int x = 52; x <= 58; ++x) {
        for (int z = 42; z <= 48; ++z) {
            for (int y = floorY + 1; y <= minH; ++y) {
                EXPECT_EQ(blockAt(onChunks, x, y, z), BlockID::air())
                    << "x=" << x << " y=" << y << " z=" << z;
            }
        }
    }
    // ... and the topmost lava of the centre column sits `fill` levels up.
    int topLava = -1;
    for (int y = 1; y < kWorldHeight; ++y) {
        if (fluidKindOf(fluidAt(onChunks, 55, y, 45)) == FluidKind::Lava) {
            topLava = y;
        }
    }
    EXPECT_EQ(topLava, floorY + lake.fill);
}

TEST(WorldGeneratorTest, CaveOpeningsReachSurface) {
    // surface_guard 0: cave openings REACH the surface (carved surface
    // cells) — over the calibrated sample region there are plenty.
    // Control: the same chunk with surface_guard 4 (the pre-GEN-1 band,
    // the surface cell excluded) carves no surface.
    GenSetup s;
    s.params = calibratedCaveParams();
    const std::int64_t seed = 123456789;
    auto res = WorldGenerator::create(seed, s.params, s.biomes, s.blocks);
    ASSERT_TRUE(res.ok()) << res.error().message;
    const WorldGenerator& gen = *res;
    const auto chunks = genColumns(gen, 4, 8, 0, 4);
    int mouths = 0;
    for (int x = 64; x <= 143; ++x) {
        for (int z = 0; z <= 79; ++z) {
            const int h = gen.surfaceHeight(x, z);
            if (h < s.params.seaLevel) {
                continue; // dry columns only
            }
            if (carvedAt(chunks, x, h, z)) {
                ++mouths;
            }
        }
    }
    EXPECT_GE(mouths, 3) << "the calibrated region must open to the surface";

    GenSetup s2;
    s2.params = calibratedCaveParams();
    s2.params.surfaceGuard = 4;
    auto res2 = WorldGenerator::create(seed, s2.params, s2.biomes, s2.blocks);
    ASSERT_TRUE(res2.ok()) << res2.error().message;
    Chunk c;
    res2->generateChunk(c, {4, 0, 0});
    int surfaceCarved = 0;
    for (int lx = 0; lx < kChunkSize; ++lx) {
        for (int lz = 0; lz < kChunkSize; ++lz) {
            const int x = 64 + lx;
            const int h = res2->surfaceHeight(x, lz);
            if (h < s2.params.seaLevel || h >= kChunkSize) {
                continue; // dry + the surface cell is in this chunk
            }
            if (c.get(lx, h, lz) == BlockID::air()) {
                ++surfaceCarved;
            }
        }
    }
    EXPECT_EQ(surfaceCarved, 0)
        << "guard 4 never carves the surface cell (the pre-GEN-1 band)";
}

TEST(WorldGeneratorTest, CavesCrossSeedDifferSameSeedStable) {
    // The cave field is seed-derived (like the lakes): the same seed
    // regenerates the region byte-identically, a different seed changes
    // the layout.
    GenSetup s;
    s.params = calibratedCaveParams();
    const std::int64_t seed = 123456789;
    auto aRes = WorldGenerator::create(seed, s.params, s.biomes, s.blocks);
    ASSERT_TRUE(aRes.ok()) << aRes.error().message;
    auto bRes = WorldGenerator::create(seed, s.params, s.biomes, s.blocks);
    ASSERT_TRUE(bRes.ok()) << bRes.error().message;
    auto cRes = WorldGenerator::create(seed + 1, s.params, s.biomes, s.blocks);
    ASSERT_TRUE(cRes.ok()) << cRes.error().message;
    const auto a = genColumns(*aRes, 4, 8, 0, 4);
    const auto b = genColumns(*bRes, 4, 8, 0, 4);
    const auto c = genColumns(*cRes, 4, 8, 0, 4);
    int sameSeedDiff = 0, crossSeedDiff = 0;
    for (const auto& [pos, chunk] : a) {
        const Chunk& ob = b.at(pos);
        const Chunk& oc = c.at(pos);
        for (int i = 0; i < Chunk::kVolume; ++i) {
            if (chunk.data()[i] != ob.data()[i]) {
                ++sameSeedDiff;
            }
            if (chunk.data()[i] != oc.data()[i]) {
                ++crossSeedDiff;
            }
        }
    }
    EXPECT_EQ(sameSeedDiff, 0) << "the same seed regenerates identical caves";
    EXPECT_GT(crossSeedDiff, 500) << "a different seed changes the caves";
}

TEST(WorldGeneratorTest, OresUnaffectedByCaves) {
    // Ores still replace deep stone only: with caves on, every ore cell is
    // the SAME ore cell in the caves-off world (caves can only REMOVE ore
    // sites where they carve air — never create or move them) — and the
    // cave region still holds plenty of ore.
    GenSetup s;
    s.params = calibratedCaveParams();
    const std::int64_t seed = 123456789;
    auto onRes = WorldGenerator::create(seed, s.params, s.biomes, s.blocks);
    ASSERT_TRUE(onRes.ok()) << onRes.error().message;
    WorldgenParams offParams = s.params;
    offParams.cavesEnabled = false;
    auto offRes =
        WorldGenerator::create(seed, offParams, s.biomes, s.blocks);
    ASSERT_TRUE(offRes.ok()) << offRes.error().message;
    const auto on = genColumns(*onRes, 4, 8, 0, 4);
    const auto off = genColumns(*offRes, 4, 8, 0, 4);
    const BlockID coal = s.blocks.idForName("coal_ore");
    const BlockID iron = s.blocks.idForName("iron_ore");
    const BlockID lapis = s.blocks.idForName("lapis_ore");
    int oreCells = 0, mismatched = 0;
    for (const auto& [pos, chunk] : on) {
        for (int i = 0; i < Chunk::kVolume; ++i) {
            const BlockID id = chunk.data()[i];
            if (id != coal && id != iron && id != lapis) {
                continue;
            }
            ++oreCells;
            if (off.at(pos).data()[i] != id) {
                ++mismatched;
            }
        }
    }
    EXPECT_GT(oreCells, 1000) << "the cave region still holds plenty of ore";
    EXPECT_EQ(mismatched, 0) << "no ore cell is created or moved by caves";
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

// -- GEN-5 (floating tree guard) ---------------------------------------------
//
// carveCaves runs before placeTrees and with the shipped surface_guard 0
// its openings reach the surface cell itself: a tree planted on a
// carved-out column floats over the hole (the user report). The guard
// skips the whole tree when the trunk column's surface cell is carved —
// the SAME pure predicate carveCaves uses, so the verdict is identical in
// every chunk whose margin covers the column (no half-placed canopy at a
// chunk boundary).

TEST(WorldGeneratorTest, CaveCarvesCellMatchesOldInlinePredicate) {
    // GEN-5 extraction identity: the pure predicate is the pre-GEN-5 inline
    // carve decision, bit-for-bit — same band (guard 0 → the surface cell
    // itself is carve-eligible), same lake boxes, same double mid-band
    // with the n2 short-circuit. Chunk {4,0,0} (x 64..79, z 0..15) is
    // outside the spawn clear zone; the fixture has no lakes, so the
    // predicate is band + mid-bands. Every cell is checked.
    GenSetup s;
    s.params = calibratedCaveParams();
    const std::int64_t seed = 123456789;
    auto res = WorldGenerator::create(seed, s.params, s.biomes, s.blocks);
    ASSERT_TRUE(res.ok()) << res.error().message;
    const WorldGenerator& gen = *res;
    const float cs = s.params.caveNoiseScale;
    const float t = s.params.caveTubeThreshold;
    const ChunkPos pos{4, 0, 0};
    int checked = 0;
    int carved = 0;
    for (int lx = 0; lx < kChunkSize; ++lx) {
        for (int lz = 0; lz < kChunkSize; ++lz) {
            const int x = pos.x * kChunkSize + lx;
            const int z = pos.z * kChunkSize + lz;
            const int h = gen.surfaceHeight(x, z);
            for (int ly = 0; ly < kChunkSize; ++ly) {
                const int y = pos.y * kChunkSize + ly;
                // The OLD inline decision (the CaveMatchesPurePredicate
                // form): in-band + both mid-bands hold.
                const int topExclusive =
                    s.params.surfaceGuard > 0 ? h - s.params.surfaceGuard
                                               : h + 1;
                bool expectCarved = false;
                if (y >= s.params.caveMinY && y < topExclusive) {
                    const float n1 =
                        valueNoise3(static_cast<float>(x) * cs,
                                    static_cast<float>(y) * cs,
                                    static_cast<float>(z) * cs,
                                    seed ^ kSaltCave);
                    if (std::fabs(n1 - 0.5f) < t) {
                        const float n2 =
                            valueNoise3(static_cast<float>(x) * cs,
                                        static_cast<float>(y) * cs,
                                        static_cast<float>(z) * cs,
                                        seed ^ kSaltCave2);
                        expectCarved = std::fabs(n2 - 0.5f) < t;
                    }
                }
                const bool got = gen.caveCarvesCell(x, y, z, h);
                EXPECT_EQ(got, expectCarved) << "x=" << x << " y=" << y
                                              << " z=" << z;
                if (got) {
                    ++carved;
                }
                ++checked;
            }
        }
    }
    EXPECT_EQ(checked, kChunkSize * kChunkSize * kChunkSize);
    EXPECT_GT(carved, 100) << "the calibrated region does carve";

    // The spawn clear zone branch: a zone chunk is never carved (the
    // predicate must agree, cell by cell, over its whole band).
    const ChunkPos zone{0, 0, 0};
    int zoneChecked = 0;
    for (int lx = 0; lx < kChunkSize; ++lx) {
        for (int lz = 0; lz < kChunkSize; ++lz) {
            const int x = zone.x * kChunkSize + lx;
            const int z = zone.z * kChunkSize + lz;
            const int h = gen.surfaceHeight(x, z);
            for (int y = s.params.caveMinY; y <= h; ++y) {
                EXPECT_FALSE(gen.caveCarvesCell(x, y, z, h))
                    << "spawn zone carved x=" << x << " y=" << y
                    << " z=" << z;
                ++zoneChecked;
            }
        }
    }
    EXPECT_GT(zoneChecked, 1000);

    // The cavesEnabled branch: a generator with caves off never carves.
    WorldgenParams off = s.params;
    off.cavesEnabled = false;
    auto resOff = WorldGenerator::create(seed, off, s.biomes, s.blocks);
    ASSERT_TRUE(resOff.ok()) << resOff.error().message;
    const WorldGenerator& genOff = *resOff;
    const int hOff = genOff.surfaceHeight(70, 8);
    EXPECT_FALSE(genOff.caveCarvesCell(70, hOff, 8, hOff));
    EXPECT_FALSE(genOff.caveCarvesCell(70, hOff - 2, 8, hOff));
}

TEST(WorldGeneratorTest, CarveCavesWritesExactlyCaveCarvesCell) {
    // GEN-5: carveCaves now calls the pure predicate per cell (the per-
    // chunk early returns are kept). For every band cell of chunk {4,0,0}
    // (a dry column's band holds no tree/ore/decoration cells — they all
    // live above the surface or replace deep stone): the chunk holds air
    // IFF the predicate carves it. Together with
    // CaveCarvesCellMatchesOldInlinePredicate (predicate == old formula)
    // and CaveMatchesPurePredicate (chunk == old formula, unchanged), this
    // pins carveCaves' behavior identity across the extraction on the
    // same seed / params / grid.
    GenSetup s;
    s.params = calibratedCaveParams();
    const std::int64_t seed = 123456789;
    auto res = WorldGenerator::create(seed, s.params, s.biomes, s.blocks);
    ASSERT_TRUE(res.ok()) << res.error().message;
    const WorldGenerator& gen = *res;
    const ChunkPos pos{4, 0, 0};
    Chunk chunk;
    gen.generateChunk(chunk, pos);
    const auto cellIsCarved = [&](int x, int y, int z) {
        const auto local = worldToChunkLocal(pos, BlockPos{x, y, z});
        return chunk.get(local.x, local.y, local.z) == BlockID::air() &&
               chunk.fluidAt(local.x, local.y, local.z) == 0;
    };
    int checked = 0;
    int carved = 0;
    for (int lx = 0; lx < kChunkSize; ++lx) {
        for (int lz = 0; lz < kChunkSize; ++lz) {
            const int x = pos.x * kChunkSize + lx;
            const int z = pos.z * kChunkSize + lz;
            const int h = gen.surfaceHeight(x, z);
            for (int ly = 0; ly < kChunkSize; ++ly) {
                const int y = pos.y * kChunkSize + ly;
                if (y > h) {
                    continue; // open sky: out of the carve band
                }
                const bool expectCarved = gen.caveCarvesCell(x, y, z, h);
                const bool carvedCell = cellIsCarved(x, y, z);
                EXPECT_EQ(carvedCell, expectCarved) << "x=" << x << " y=" << y
                                                    << " z=" << z;
                if (expectCarved) {
                    ++carved;
                }
                ++checked;
            }
        }
    }
    EXPECT_GT(checked, 4000);
    EXPECT_GT(carved, 100) << "the calibrated region does carve";
}

TEST(WorldGeneratorTest, TreesOnIntactSurfaceStillGrow) {
    // GEN-5 negative case (regression): a tree column whose surface cell
    // is NOT cave-carved must still be planted exactly as before — the
    // guard fires only on the carved case. Includes a boundary-crossing
    // column: BOTH chunks complete the seamless tree (the seam cell one
    // past the boundary edge is written by the neighbor chunk).
    GenSetup s;
    s.params = calibratedCaveParams();
    const BlockID woodId = s.blocks.idForName("wood");
    const BlockID leavesId = s.blocks.idForName("leaves");

    const Gen5Case plain = findGen5Case(s, /*wantCarved=*/false,
                                        /*wantCross=*/false, 400);
    ASSERT_GE(plain.seed, 0) << "no intact-surface tree column in 400 seeds";
    {
        auto res = WorldGenerator::create(plain.seed, s.params, s.biomes,
                                          s.blocks);
        ASSERT_TRUE(res.ok()) << res.error().message;
        std::map<ChunkPos, Chunk> chunks;
        gen5GenerateBox(*res, chunks, plain.wx, plain.wz);
        // The surface cell is intact (the guard must not fire) ...
        EXPECT_NE(blockAt(chunks, plain.wx, plain.hgt, plain.wz),
                  BlockID::air());
        // ... and the trunk is planted exactly as before: contiguous wood
        // right above the surface, min..max length.
        int bottom = -1;
        for (int y = plain.hgt + 1; y < kWorldHeight; ++y) {
            if (blockAt(chunks, plain.wx, y, plain.wz) == woodId) {
                bottom = y;
                break;
            }
        }
        ASSERT_EQ(bottom, plain.hgt + 1);
        int top = bottom;
        while (top + 1 < kWorldHeight &&
               blockAt(chunks, plain.wx, top + 1, plain.wz) == woodId) {
            ++top;
        }
        EXPECT_GE(top - plain.hgt, s.params.treeMinHeight);
        EXPECT_LE(top - plain.hgt,
                  s.params.treeMinHeight + s.params.treeHeightRange);
    }

    const Gen5Case cross = findGen5CrossCase(s, /*wantCarved=*/false, 400,
                                             /*validateCavesOn=*/true,
                                             /*requireNeighborsUnguarded=*/
                                             false);
    ASSERT_GE(cross.seed, 0)
        << "no boundary-crossing intact tree column in 400 seeds";
    {
        auto res = WorldGenerator::create(cross.seed, s.params, s.biomes,
                                          s.blocks);
        ASSERT_TRUE(res.ok()) << res.error().message;
        std::map<ChunkPos, Chunk> chunks;
        gen5GenerateBox(*res, chunks, cross.wx, cross.wz);
        int bottom = -1;
        for (int y = cross.hgt + 1; y < kWorldHeight; ++y) {
            if (blockAt(chunks, cross.wx, y, cross.wz) == woodId) {
                bottom = y;
                break;
            }
        }
        ASSERT_EQ(bottom, cross.hgt + 1);
        int top = bottom;
        while (top + 1 < kWorldHeight &&
               blockAt(chunks, cross.wx, top + 1, cross.wz) == woodId) {
            ++top;
        }
        // The seam cell lies in the NEIGHBOR chunk (past the boundary
        // edge) and still holds leaves: the neighbor chunk planted its
        // canopy half (the guard fired only for a carved surface — this
        // one is intact).
        const int sx = (cross.wx % kChunkSize == 0) ? cross.wx - 1 : cross.wx;
        const int sz = (cross.wx % kChunkSize == 0) ? cross.wz : cross.wz - 1;
        EXPECT_NE(worldToChunk(BlockPos{sx, top, sz}),
                  worldToChunk(BlockPos{cross.wx, 0, cross.wz}))
            << "the case must cross a chunk boundary";
        EXPECT_EQ(blockAt(chunks, sx, top, sz), leavesId);
    }
}

TEST(WorldGeneratorTest, TreesSkipCaveCarvedSurfaceColumns) {
    // GEN-5 positive case: a carved-surface column is skipped — the
    // floating tree the user reported. Proven by A/B against the SAME
    // seed with caves disabled (the guard's only input): caves-off plants
    // the trunk, caves-on must have none. The cross-chunk sub-case adds:
    // the seam cell past the boundary edge is a leaf in caves-off but air
    // in caves-on (the neighbor chunk skipped its canopy half — the same
    // pure verdict in both chunks), with no neighbor tree guarded (so
    // ONLY this tree's contribution is gone from the box).
    GenSetup s;
    s.params = calibratedCaveParams();
    const BlockID woodId = s.blocks.idForName("wood");
    const BlockID leavesId = s.blocks.idForName("leaves");

    const Gen5Case plain = findGen5Case(s, /*wantCarved=*/true,
                                        /*wantCross=*/false, 400);
    ASSERT_GE(plain.seed, 0) << "no carved-surface tree column in 400 seeds";
    {
        auto resOn = WorldGenerator::create(plain.seed, s.params, s.biomes,
                                            s.blocks);
        ASSERT_TRUE(resOn.ok()) << resOn.error().message;
        WorldgenParams pOff = s.params;
        pOff.cavesEnabled = false;
        auto resOff = WorldGenerator::create(plain.seed, pOff, s.biomes,
                                             s.blocks);
        ASSERT_TRUE(resOff.ok()) << resOff.error().message;
        std::map<ChunkPos, Chunk> on, off;
        gen5GenerateBox(*resOn, on, plain.wx, plain.wz);
        gen5GenerateBox(*resOff, off, plain.wx, plain.wz);
        // The surface cell is cave-carved in the caves-on world (the
        // floating condition) and intact in the caves-off world.
        EXPECT_EQ(blockAt(on, plain.wx, plain.hgt, plain.wz), BlockID::air());
        EXPECT_NE(blockAt(off, plain.wx, plain.hgt, plain.wz), BlockID::air());
        // The guard fired: no wood anywhere in the trunk column (only this
        // tree can write wood there — the next trunk is 4 lattice steps
        // away) ...
        int woodOn = 0;
        for (int y = plain.hgt + 1; y < kWorldHeight; ++y) {
            woodOn += static_cast<int>(blockAt(on, plain.wx, y, plain.wz) ==
                                       woodId);
        }
        EXPECT_EQ(woodOn, 0);
        // ... and no leaves in the column's tree band either (the canopy
        // is skipped as a whole; a neighbor canopy cannot reach this
        // column — radius 2 < lattice 4).
        int leavesOn = 0;
        const int bandTop = plain.hgt + s.params.treeMinHeight +
                            s.params.treeHeightRange + 2;
        for (int y = plain.hgt + 1; y <= bandTop; ++y) {
            leavesOn += static_cast<int>(
                blockAt(on, plain.wx, y, plain.wz) == leavesId);
        }
        EXPECT_EQ(leavesOn, 0);
        // The caves-off world plants the same column right above the
        // intact surface: the A/B diff is the guard.
        int woodOff = 0;
        for (int y = plain.hgt + 1; y < kWorldHeight; ++y) {
            if (blockAt(off, plain.wx, y, plain.wz) == woodId) {
                ++woodOff;
            } else {
                break;
            }
        }
        EXPECT_GE(woodOff, s.params.treeMinHeight);
        EXPECT_LE(woodOff, s.params.treeMinHeight + s.params.treeHeightRange);
    }

    const Gen5Case cross = findGen5CrossCase(s, /*wantCarved=*/true, 400,
                                             /*validateCavesOn=*/false,
                                             /*requireNeighborsUnguarded=*/
                                             true);
    ASSERT_GE(cross.seed, 0)
        << "no boundary-crossing carved tree column in 400 seeds";
    {
        auto resOn = WorldGenerator::create(cross.seed, s.params, s.biomes,
                                            s.blocks);
        ASSERT_TRUE(resOn.ok()) << resOn.error().message;
        WorldgenParams pOff = s.params;
        pOff.cavesEnabled = false;
        auto resOff = WorldGenerator::create(cross.seed, pOff, s.biomes,
                                             s.blocks);
        ASSERT_TRUE(resOff.ok()) << resOff.error().message;
        std::map<ChunkPos, Chunk> on, off;
        gen5GenerateBox(*resOn, on, cross.wx, cross.wz);
        gen5GenerateBox(*resOff, off, cross.wx, cross.wz);
        // The surface cell is carved (the floating condition) ...
        EXPECT_EQ(blockAt(on, cross.wx, cross.hgt, cross.wz), BlockID::air());
        // ... and the whole tree is gone from the caves-on box: no wood
        // (the trunk column is empty; no other trunk lies in trunk ± 2)
        // and no leaves from the skipped canopy — the seam cell one past
        // the boundary edge is air there, while the caves-off world holds
        // it as a leaf (the neighbor chunk's half was planted).
        int woodOn = 0;
        for (const auto& [cp, chunk] : on) {
            for (int i = 0; i < Chunk::kVolume; ++i) {
                woodOn += static_cast<int>(chunk.data()[i] == woodId);
            }
        }
        EXPECT_EQ(woodOn, 0);
        // The trunk top level in the caves-off world (the seam's height).
        int topOff = -1;
        for (int y = cross.hgt + 1; y < kWorldHeight; ++y) {
            if (blockAt(off, cross.wx, y, cross.wz) == woodId) {
                topOff = y;
            } else {
                break;
            }
        }
        ASSERT_GE(topOff, cross.hgt + s.params.treeMinHeight);
        const int sx = (cross.wx % kChunkSize == 0) ? cross.wx - 1 : cross.wx;
        const int sz = (cross.wx % kChunkSize == 0) ? cross.wz : cross.wz - 1;
        EXPECT_NE(worldToChunk(BlockPos{sx, topOff, sz}),
                  worldToChunk(BlockPos{cross.wx, 0, cross.wz}))
            << "the case must cross a chunk boundary";
        EXPECT_EQ(blockAt(off, sx, topOff, sz), leavesId)
            << "caves-off: the neighbor chunk planted its canopy half";
        EXPECT_EQ(blockAt(on, sx, topOff, sz), BlockID::air())
            << "caves-on: the neighbor chunk skipped its canopy half "
               "(same pure verdict as the trunk's chunk)";
    }
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

// ENT-2: the init cross-check — worldgen places NO stateful block (a
// per-instance state has no worldgen writer: the game's setBlock choke
// point is the only one, and the save journal derives the state array
// from the block journal on exactly this premise). The placement sources
// are named and rejected at create().

TEST(WorldGeneratorTest, CreateFailsOnStatefulBiomeSurface) {
    GenSetup s;
    BiomeRegistry biomes;
    ASSERT_TRUE(biomes.registerBiome(parseJson(
        R"({"id":"x","surface":"stone_stairs","subsurface":"dirt"})"))
                    .ok());
    auto r = WorldGenerator::create(42, s.params, biomes, s.blocks);
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("stone_stairs"), std::string::npos);
    EXPECT_NE(r.error().message.find("per-instance state"),
              std::string::npos);
}

TEST(WorldGeneratorTest, CreateFailsOnStatefulOre) {
    GenSetup s;
    s.params.ores.clear();
    OreConfig bad;
    bad.block = "stone_stairs";
    bad.probability = 0.1f;
    bad.veinSize = 4;
    bad.maxY = 16;
    s.params.ores.push_back(bad);
    auto r = WorldGenerator::create(42, s.params, s.biomes, s.blocks);
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("stone_stairs"), std::string::npos);
    EXPECT_NE(r.error().message.find("per-instance state"),
              std::string::npos);
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

// -- surface decorations (Phase 45) ------------------------------------------

// Scan the region for decoration cells and verify the placement contract:
// exactly one above the surface cell, on the biome's own surface block,
// above sea level, on the tree lattice.
TEST(WorldGeneratorTest, DecorationsSitOnBiomeSurface) {
    Region r(42);
    const BlockID cactusId = r.setup.blocks.idForName("cactus");
    const BlockID alliumId = r.setup.blocks.idForName("allium");
    const BlockID tulipId = r.setup.blocks.idForName("tulip");
    const BlockID cornflowerId =
        r.setup.blocks.idForName("cornflower");
    int flowers = 0;
    for (int x = 0; x < 32; ++x) {
        for (int z = 0; z < 32; ++z) {
            for (int y = 1; y < kWorldHeight; ++y) {
                const BlockID id = r.at(x, y, z);
                if (id == cactusId) {
                    continue; // the desert is covered by its own test
                }
                if (id != alliumId && id != tulipId && id != cornflowerId) {
                    continue;
                }
                // The cell below is the biome's own surface block ...
                const Biome* biome =
                    r.setup.biomes.findByName(r.gen->biomeNameAt(x, z));
                ASSERT_NE(biome, nullptr);
                EXPECT_EQ(r.at(x, y - 1, z),
                          r.setup.blocks.idForName(biome->surface));
                // ... and sits above sea level (no underwater decorations).
                EXPECT_GT(y - 1, r.setup.params.seaLevel);
                // ... and the decoration cell itself is on the tree lattice.
                EXPECT_EQ(x % r.setup.params.treeLattice, 0);
                EXPECT_EQ(z % r.setup.params.treeLattice, 0);
                ++flowers;
            }
        }
    }
    EXPECT_GT(flowers, 0);
}

// Seed 28 carries a large dry desert in the 32x32 region (the seed-42
// region has no desert columns at all), so the cactus path gets its own
// coverage: cacti appear, and only on sand.
TEST(WorldGeneratorTest, CactusGrowsOnDesertSurface) {
    Region r(28);
    const BlockID cactusId = r.setup.blocks.idForName("cactus");
    const BlockID sandId = r.setup.blocks.idForName("sand");
    int cactus = 0;
    for (int x = 0; x < 32; ++x) {
        for (int z = 0; z < 32; ++z) {
            for (int y = 1; y < kWorldHeight; ++y) {
                if (r.at(x, y, z) != cactusId) {
                    continue;
                }
                EXPECT_EQ(r.at(x, y - 1, z), sandId);
                EXPECT_EQ(y, r.gen->surfaceHeight(x, z) + 1);
                ++cactus;
            }
        }
    }
    EXPECT_GT(cactus, 0);
}

// A decoration is exactly ONE above the terrain surface height (h+1) —
// never floating higher. Trees win the h+1 cell (placeTrees runs first,
// placeIfAir never overwrites), so a trunk column simply has no
// decoration.
TEST(WorldGeneratorTest, DecorationsAreExactlyOneAboveSurface) {
    Region r(42);
    const BlockID cactusId = r.setup.blocks.idForName("cactus");
    const BlockID alliumId = r.setup.blocks.idForName("allium");
    const BlockID tulipId = r.setup.blocks.idForName("tulip");
    const BlockID cornflowerId = r.setup.blocks.idForName("cornflower");
    int checked = 0;
    for (int x = 0; x < 32; ++x) {
        for (int z = 0; z < 32; ++z) {
            for (int y = 1; y < kWorldHeight; ++y) {
                const BlockID id = r.at(x, y, z);
                if (id != cactusId && id != alliumId && id != tulipId &&
                    id != cornflowerId) {
                    continue;
                }
                EXPECT_EQ(y, r.gen->surfaceHeight(x, z) + 1);
                ++checked;
            }
        }
    }
    EXPECT_GT(checked, 0);
}

TEST(WorldGeneratorTest, LapisOresAppearAndRespectMaxY) {
    Region r(42);
    const BlockID lapisId = r.setup.blocks.idForName("lapis_ore");
    int lapis = 0;
    int aboveMax = 0;
    for (int x = 0; x < 32; ++x) {
        for (int z = 0; z < 32; ++z) {
            for (int y = 1; y < kWorldHeight; ++y) {
                if (r.at(x, y, z) == lapisId) {
                    ++lapis;
                    if (y > 32) {
                        ++aboveMax;
                    }
                }
            }
        }
    }
    EXPECT_GT(lapis, 0);
    EXPECT_EQ(aboveMax, 0);
}

TEST(WorldGeneratorTest, CreateFailsOnUnknownDecorationBlock) {
    BiomeRegistry biomes;
    auto r = biomes.registerBiome(parseJson(
        R"({"id":"x","surface":"grass","subsurface":"dirt",)"
        R"("decorations":[{"block":"nope","probability":0.1}]})"));
    ASSERT_TRUE(r.ok()) << r.error().message;
    auto created =
        WorldGenerator::create(42, makeParams(), biomes, makeBlocks());
    ASSERT_FALSE(created.ok());
    EXPECT_NE(created.error().message.find("nope"), std::string::npos);
}

// Phase 86: the lava lake — a contained pool of lava carved into the terrain
// at a fixed location. The region min-height floor (depth below) is carved to
// air, `fill` full-lava levels are poured on the floor, and the surrounding
// terrain (outside the region) stays solid (no lava) — the basin is
// contained. Deterministic (pure heightAt).
TEST(WorldGeneratorTest, Phase86LavaLakeIsContained) {
    GenSetup setup;
    // Phase 88: a LIST of lakes inside the 2x2 region (x,z in [0,32)): lake 0
    // centre (20,20) radius 3 (region 17..23), lake 1 centre (10,10) radius 2
    // (region 8..12). Both must be present + contained (the multi-lake list).
    WorldgenParams::LavaLake a;
    a.centerX = 20;
    a.centerZ = 20;
    a.radius = 3;
    a.depth = 4;
    a.fill = 2;
    WorldgenParams::LavaLake b;
    b.centerX = 10;
    b.centerZ = 10;
    b.radius = 2;
    b.depth = 3;
    b.fill = 2;
    setup.params.lavaLakes = {a, b};

    auto res = WorldGenerator::create(7, setup.params, setup.biomes,
                                      setup.blocks);
    ASSERT_TRUE(res.ok()) << res.error().message;
    WorldGenerator gen = std::move(*res);
    std::map<ChunkPos, Chunk> chunks;
    for (int cx = 0; cx <= 1; ++cx) {
        for (int cz = 0; cz <= 1; ++cz) {
            for (int cy = 0; cy * kChunkSize < kWorldHeight; ++cy) {
                gen.generateChunk(chunks[{cx, cy, cz}], {cx, cy, cz});
            }
        }
    }
    auto fluidAt = [&](int x, int y, int z) -> std::uint8_t {
        const BlockPos p {x, y, z};
        const ChunkPos cp = worldToChunk(p);
        const auto it = chunks.find(cp);
        if (it == chunks.end()) {
            return 0;
        }
        const BlockPos local = worldToChunkLocal(cp, p);
        return it->second.fluidAt(local.x, local.y, local.z);
    };
    auto blockAt = [&](int x, int y, int z) -> BlockID {
        const BlockPos p {x, y, z};
        const ChunkPos cp = worldToChunk(p);
        const auto it = chunks.find(cp);
        if (it == chunks.end()) {
            return BlockID::air();
        }
        const BlockPos local = worldToChunkLocal(cp, p);
        return it->second.get(local.x, local.y, local.z);
    };

    // (1) There IS lava in the lake region.
    int lavaCells = 0;
    for (int x = 17; x <= 23; ++x) {
        for (int z = 17; z <= 23; ++z) {
            for (int y = 1; y < kWorldHeight; ++y) {
                if (fluidKindOf(fluidAt(x, y, z)) == FluidKind::Lava) {
                    ++lavaCells;
                }
            }
        }
    }
    EXPECT_GT(lavaCells, 0) << "the lake should contain lava";

    // (2) The lava is CONTAINED: no lava outside the region (a ring just
    // outside the 17..23 basin).
    int outsideLava = 0;
    for (int x = 15; x <= 25; ++x) {
        for (int z = 15; z <= 25; ++z) {
            if (x >= 17 && x <= 23 && z >= 17 && z <= 23) {
                continue; // inside the region
            }
            for (int y = 1; y < kWorldHeight; ++y) {
                if (fluidKindOf(fluidAt(x, y, z)) == FluidKind::Lava) {
                    ++outsideLava;
                }
            }
        }
    }
    EXPECT_EQ(outsideLava, 0) << "the basin walls contain the lava";

    // (3) Determinism: a second generation has the lava in the SAME cells.
    std::map<ChunkPos, Chunk> chunks2;
    for (int cx = 0; cx <= 1; ++cx) {
        for (int cz = 0; cz <= 1; ++cz) {
            for (int cy = 0; cy * kChunkSize < kWorldHeight; ++cy) {
                gen.generateChunk(chunks2[{cx, cy, cz}], {cx, cy, cz});
            }
        }
    }
    bool same = true;
    for (int x = 15; x <= 25; ++x) {
        for (int z = 15; z <= 25; ++z) {
            for (int y = 1; y < kWorldHeight; ++y) {
                const BlockPos p {x, y, z};
                const ChunkPos cp = worldToChunk(p);
                const BlockPos local = worldToChunkLocal(cp, p);
                const std::uint8_t a = chunks.at(cp).fluidAt(local.x, local.y,
                                                             local.z);
                const std::uint8_t b = chunks2.at(cp).fluidAt(local.x, local.y,
                                                              local.z);
                if (a != b) {
                    same = false;
                }
            }
        }
    }
    EXPECT_TRUE(same) << "the lake is deterministic";

    // (4) In the centre column, the cell just above the topmost lava is air
    // (the carved basin headspace).
    int topLavaY = -1;
    for (int y = 1; y < kWorldHeight; ++y) {
        if (fluidKindOf(fluidAt(20, y, 20)) == FluidKind::Lava) {
            topLavaY = y;
        }
    }
    ASSERT_GE(topLavaY, 0) << "the centre column should hold lava";
    EXPECT_EQ(blockAt(20, topLavaY + 1, 20), BlockID::air())
        << "the basin headspace above the lava is carved to air";

    // (5) Phase 88 multi-lake: the SECOND lake (centre (10,10), region 8..12)
    // also holds lava — the list places every configured lake.
    int lake2Lava = 0;
    for (int x = 8; x <= 12; ++x) {
        for (int z = 8; z <= 12; ++z) {
            for (int y = 1; y < kWorldHeight; ++y) {
                if (fluidKindOf(fluidAt(x, y, z)) == FluidKind::Lava) {
                    ++lake2Lava;
                }
            }
        }
    }
    EXPECT_GT(lake2Lava, 0) << "the second lake should also contain lava";
}

// Phase 90: a SURFACE lava lake (depth == fill) puts its lava top at the
// region's minimum ORIGINAL terrain height (minH) — flush with the rim, so
// the P58 burn pass scorches the flammable rim at runtime. A buried lake
// (depth > fill) puts the lava top BELOW minH (sealed under headspace), so
// its rim is not 6-adjacent to the lava and does not burn. This test pins
// the surface property: the topmost lava == the region's original min height.
TEST(WorldGeneratorTest, Phase90SurfaceLakeLavaTopAtRegionMin) {
    // World A: the original terrain (no lake) over the 2x2 region.
    Region rA(7);
    const int x0 = 8, x1 = 12, z0 = 8, z1 = 12; // lake region (centre 10,10 R2)
    auto surfA = [&](int x, int z) {
        for (int y = kWorldHeight - 1; y >= 1; --y) {
            const BlockID b = rA.at(x, y, z);
            if (b.valid() && b != BlockID::air() &&
                rA.setup.blocks.at(b).properties.solid) {
                return y;
            }
        }
        return -1;
    };
    int minH = INT_MAX;
    for (int x = x0; x <= x1; ++x)
        for (int z = z0; z <= z1; ++z) minH = std::min(minH, surfA(x, z));
    ASSERT_GE(minH, 1) << "the region should have terrain";

    // World B: the same terrain + a SURFACE lake (depth == fill == 2).
    GenSetup setupB;
    WorldgenParams::LavaLake s;
    s.enabled = true;
    s.centerX = 10;
    s.centerZ = 10;
    s.radius = 2;
    s.depth = 2;
    s.fill = 2; // depth == fill -> surface lake
    setupB.params.lavaLakes = {s};
    auto resB = WorldGenerator::create(7, setupB.params, setupB.biomes,
                                       setupB.blocks);
    ASSERT_TRUE(resB.ok()) << resB.error().message;
    std::map<ChunkPos, Chunk> b;
    for (int cx = 0; cx <= 1; ++cx)
        for (int cz = 0; cz <= 1; ++cz)
            for (int cy = 0; cy * kChunkSize < kWorldHeight; ++cy)
                resB->generateChunk(b[{cx, cy, cz}], {cx, cy, cz});
    auto lavaTopB = [&](int x, int z) {
        int top = -1;
        for (int y = 1; y < kWorldHeight; ++y) {
            const auto it = b.find(worldToChunk(BlockPos {x, y, z}));
            if (it == b.end()) continue;
            const BlockPos local = worldToChunkLocal(it->first, BlockPos {x, y, z});
            if (fluidKindOf(it->second.fluidAt(local.x, local.y, local.z)) ==
                FluidKind::Lava) {
                top = y;
            }
        }
        return top;
    };
    int hTop = -1;
    for (int x = x0; x <= x1; ++x)
        for (int z = z0; z <= z1; ++z) hTop = std::max(hTop, lavaTopB(x, z));
    ASSERT_GE(hTop, 1) << "the surface lake should hold lava";
    EXPECT_EQ(hTop, minH)
        << "a surface lake (depth == fill) puts its lava top at the region's "
           "original minimum height (flush with the rim, so the P58 burn "
           "pass scorches it)";

    // Control: a BURIED lake (depth > fill) puts the lava top BELOW minH.
    GenSetup setupC;
    WorldgenParams::LavaLake buried;
    buried.enabled = true;
    buried.centerX = 10;
    buried.centerZ = 10;
    buried.radius = 2;
    buried.depth = 4;
    buried.fill = 2; // depth > fill -> buried (lava top below minH)
    setupC.params.lavaLakes = {buried};
    auto resC = WorldGenerator::create(7, setupC.params, setupC.biomes,
                                       setupC.blocks);
    ASSERT_TRUE(resC.ok()) << resC.error().message;
    std::map<ChunkPos, Chunk> c;
    for (int cx = 0; cx <= 1; ++cx)
        for (int cz = 0; cz <= 1; ++cz)
            for (int cy = 0; cy * kChunkSize < kWorldHeight; ++cy)
                resC->generateChunk(c[{cx, cy, cz}], {cx, cy, cz});
    int cTop = -1;
    for (int x = x0; x <= x1; ++x)
        for (int z = z0; z <= z1; ++z) {
            for (int y = 1; y < kWorldHeight; ++y) {
                const auto it = c.find(worldToChunk(BlockPos {x, y, z}));
                if (it == c.end()) continue;
                const BlockPos local =
                    worldToChunkLocal(it->first, BlockPos {x, y, z});
                if (fluidKindOf(it->second.fluidAt(local.x, local.y, local.z)) ==
                    FluidKind::Lava) {
                    cTop = std::max(cTop, y);
                }
            }
        }
    ASSERT_GE(cTop, 1) << "the buried lake should hold lava";
    EXPECT_LT(cTop, minH)
        << "a buried lake (depth > fill) puts its lava top below the region "
           "minimum (sealed, rim not adjacent to the lava)";
}

// Phase 93: the PROCEDURAL lake distribution — `count` lakes placed at
// deterministic (seeded) pseudo-random positions within the region. The
// lakes are generated (lava present in the region), deterministic (two
// generations -> same positions), and seed-dependent (different seeds ->
// different positions).
TEST(WorldGeneratorTest, Phase93ProceduralLakesAreDeterministic) {
    auto lavaCols = [&](std::int64_t seed) -> std::set<int> {
        GenSetup setup;
        WorldgenParams::LavaLakeProcedural pd;
        pd.enabled = true;
        pd.count = 5;
        pd.minRadius = 1;
        pd.maxRadius = 2;
        pd.minDepth = 2;
        pd.maxDepth = 3;
        pd.minFill = 1;
        pd.maxFill = 2;
        pd.regionMinX = 4;
        pd.regionMinZ = 4;
        pd.regionMaxX = 20;
        pd.regionMaxZ = 20;
        setup.params.lavaLakesProcedural = pd;
        auto res = WorldGenerator::create(seed, setup.params, setup.biomes,
                                          setup.blocks);
        if (!res.ok()) {
            ADD_FAILURE() << res.error().message;
            return {};
        }
        std::map<ChunkPos, Chunk> ch;
        for (int cx = 0; cx <= 1; ++cx)
            for (int cz = 0; cz <= 1; ++cz)
                for (int cy = 0; cy * kChunkSize < kWorldHeight; ++cy)
                    res->generateChunk(ch[{cx, cy, cz}], {cx, cy, cz});
        std::set<int> cols;
        for (int x = 0; x < 32; ++x)
            for (int z = 0; z < 32; ++z)
                for (int y = 1; y < kWorldHeight; ++y) {
                    const auto it = ch.find(worldToChunk(BlockPos {x, y, z}));
                    if (it == ch.end()) continue;
                    const BlockPos local =
                        worldToChunkLocal(it->first, BlockPos {x, y, z});
                    if (fluidKindOf(it->second.fluidAt(local.x, local.y,
                                                        local.z)) ==
                        FluidKind::Lava) {
                        cols.insert(x * 32 + z);
                        break;
                    }
                }
        return cols;
    };
    // (1) The lakes are generated (lava present in the region).
    const std::set<int> colsA = lavaCols(7);
    EXPECT_GE(colsA.size(), 3)
        << "the procedural lakes should generate (>=3 lava columns)";
    // (2) Deterministic: a second generation (same seed) -> same columns.
    const std::set<int> colsB = lavaCols(7);
    EXPECT_EQ(colsA, colsB) << "the procedural lakes are deterministic";
    // (3) Seed-dependent: a different seed -> different columns.
    const std::set<int> colsC = lavaCols(9);
    EXPECT_NE(colsA, colsC)
        << "the procedural lakes are seed-derived (different across seeds)";
}

// -- GEN-3 (ore vein shape): the ellipsoid vein pins ------------------------
//
// The calibration probe region (docs/archive/phase-gen3.md): seed
// 123456789, columns cx 4..11 x cz 0..7 (world x[64,192) z[0,128)), full
// height, REAL assets. The pinned counts below are the single-round
// calibration final (shape switch + the probability re-tune); the old
// random-walk baselines (coal 3206 / iron 1908 / diamond 142 / gravel
// 1007) stay as the soft density-band references (the diamond residual
// is recorded in docs/KNOWN_ISSUES).

namespace {

constexpr std::int64_t kGen3ProbeSeed = 123456789;
constexpr int kGen3CX0 = 4, kGen3CX1 = 11, kGen3CZ0 = 0, kGen3CZ1 = 7;
constexpr int kGen3Layers = kWorldHeight / kChunkSize;

struct Gen3World {
    AssetManager assets;
    BlockRegistry blocks;
    BiomeRegistry biomes;
    WorldgenParams params;
    std::optional<WorldGenerator> gen;
    std::map<ChunkPos, Chunk> chunks;
    // The 6 shipped vein entries, in worldgen.json order.
    std::array<const char*, 6> oreNames = {
        {"coal_ore", "iron_ore", "gold_ore", "diamond_ore", "lapis_ore",
         "gravel"}};

    bool load() {
        if (!assets.setRoot("assets").ok()) {
            return false;
        }
        if (!blocks.loadDirectory(assets, "data/blocks").ok()) {
            return false;
        }
        if (!biomes.loadDirectory(assets).ok()) {
            return false;
        }
        auto wp = WorldgenParams::load(assets);
        if (!wp.ok()) {
            return false;
        }
        params = *wp;
        auto g =
            WorldGenerator::create(kGen3ProbeSeed, params, biomes, blocks);
        if (!g.ok()) {
            return false;
        }
        gen = std::move(*g);
        return true;
    }
    void generateRegion(int cx0, int cx1, int cz0, int cz1) {
        for (int cx = cx0; cx <= cx1; ++cx) {
            for (int cz = cz0; cz <= cz1; ++cz) {
                for (int cy = 0; cy < kGen3Layers; ++cy) {
                    gen->generateChunk(chunks[{cx, cy, cz}], {cx, cy, cz});
                }
            }
        }
    }
    int countOre(const char* name) const {
        const BlockID id = blocks.idForName(name);
        int n = 0;
        for (const auto& [pos, chunk] : chunks) {
            for (int i = 0; i < Chunk::kVolume; ++i) {
                n += chunk.data()[i] == id;
            }
        }
        return n;
    }
};

} // namespace

TEST(WorldGeneratorTest, Gen3ShippedOreVeinCellCountsPinned) {
    Gen3World w;
    ASSERT_TRUE(w.load()) << "run the tests from the repository root";
    w.generateRegion(kGen3CX0, kGen3CX1, kGen3CZ0, kGen3CZ1);
    // The calibration final (probe region, single round): exact pins.
    // GEN-4 re-pin (cave noise_scale 0.09 → 0.05): caves carve a different
    // cell set, so the deep-stone cells available to ore placement shift
    // slightly (ores never replace carved cells) — the shape switch +
    // probability re-tune contract is unchanged.
    const int coal = w.countOre("coal_ore");
    const int iron = w.countOre("iron_ore");
    const int diamond = w.countOre("diamond_ore");
    const int gravel = w.countOre("gravel");
    EXPECT_EQ(coal, 3448); // old baseline 3206 (ratio 1.075)
    EXPECT_EQ(iron, 1684); // old baseline 1908 (ratio 0.883)
    EXPECT_EQ(diamond, 103); // old baseline 142 (ratio 0.725 — recorded)
    EXPECT_EQ(gravel, 1087); // old baseline 1007 (ratio 1.079)
    // The soft density band vs the old random-walk baseline: drift
    // guard (a shape/parameter regression moves the density out of band
    // long before the exact pin's value changes world meaningfully).
    EXPECT_GE(coal, 0.75 * 3206);
    EXPECT_LE(coal, 1.3 * 3206);
    EXPECT_GE(iron, 0.75 * 1908);
    EXPECT_LE(iron, 1.3 * 1908);
    EXPECT_GE(gravel, 0.75 * 1007);
    EXPECT_LE(gravel, 1.3 * 1007);
    // Diamond: the single-round calibration residual landed BELOW the
    // 0.8 band (recorded, docs/KNOWN_ISSUES) — the guard is widened for
    // it (still drift-protective).
    EXPECT_GE(diamond, 0.5 * 142);
    EXPECT_LE(diamond, 1.3 * 142);
}

TEST(WorldGeneratorTest, Gen3OreVeinsRespectMaxY) {
    Gen3World w;
    ASSERT_TRUE(w.load()) << "run the tests from the repository root";
    w.generateRegion(kGen3CX0, kGen3CX1, kGen3CZ0, kGen3CZ1);
    const int maxys[] = {80, 56, 36, 20, 32, 80}; // the shipped max_y set
    for (int t = 0; t < 6; ++t) {
        const BlockID id = w.blocks.idForName(w.oreNames[t]);
        int above = 0;
        for (const auto& [pos, chunk] : w.chunks) {
            for (int lx = 0; lx < kChunkSize; ++lx) {
                for (int ly = 0; ly < kChunkSize; ++ly) {
                    for (int lz = 0; lz < kChunkSize; ++lz) {
                        const int wy = pos.y * kChunkSize + ly;
                        if (wy > maxys[t] && chunk.get(lx, ly, lz) == id) {
                            ++above;
                        }
                    }
                }
            }
        }
        EXPECT_EQ(above, 0) << w.oreNames[t];
    }
}

TEST(WorldGeneratorTest, Gen3OreReplacesDeepOnly) {
    Gen3World w;
    ASSERT_TRUE(w.load()) << "run the tests from the repository root";
    w.generateRegion(kGen3CX0, kGen3CX1, kGen3CZ0, kGen3CZ1);
    // placeOre's contract (unchanged by GEN-3): an ore cell is a DEEP
    // stone cell — never the y=0 bottom layer, never the surface /
    // subsurface band (the fill's h - subsurfaceDepth line), and never a
    // cave-carved cell (an ore there would sit in air).
    int checked = 0;
    for (int t = 0; t < 6; ++t) {
        const BlockID id = w.blocks.idForName(w.oreNames[t]);
        for (const auto& [pos, chunk] : w.chunks) {
            for (int lx = 0; lx < kChunkSize; ++lx) {
                for (int ly = 0; ly < kChunkSize; ++ly) {
                    for (int lz = 0; lz < kChunkSize; ++lz) {
                        if (chunk.get(lx, ly, lz) != id) {
                            continue;
                        }
                        const int x = pos.x * kChunkSize + lx;
                        const int y = pos.y * kChunkSize + ly;
                        const int z = pos.z * kChunkSize + lz;
                        const int h = w.gen->surfaceHeight(x, z);
                        const Biome* biome =
                            w.biomes.findByName(w.gen->biomeNameAt(x, z));
                        const int depth =
                            biome ? biome->subsurfaceDepth : 0;
                        EXPECT_GE(y, 1) << "ore in the bottom layer";
                        EXPECT_LT(y, h - depth)
                            << "ore in the surface/subsurface band";
                        ++checked;
                    }
                }
            }
        }
    }
    EXPECT_GT(checked, 5000) << "the region holds plenty of ore";
}

TEST(WorldGeneratorTest, Gen3OreCrossChunkSeamConsistent) {
    // The vein plan is a pure function of the WORLD-coordinate lattice
    // (the start + the planOreVein seed), so a chunk's ore cells are
    // identical generated alone or inside a neighbor group — a vein
    // crossing the boundary lands its cells in both chunks consistently
    // (the seam contract; the ellipsoid can now reach 3 blocks further
    // than the old 1-step walk's first ring).
    Gen3World w;
    ASSERT_TRUE(w.load()) << "run the tests from the repository root";
    const ChunkPos group[4] = {{4, 4, 0}, {5, 4, 0}, {4, 4, 1}, {5, 4, 1}};
    w.generateRegion(4, 5, 0, 1);
    for (const ChunkPos& cp : group) {
        Chunk solo;
        w.gen->generateChunk(solo, cp);
        for (int t = 0; t < 6; ++t) {
            const BlockID id = w.blocks.idForName(w.oreNames[t]);
            std::set<BlockPos> inGroup, inSolo;
            for (int lx = 0; lx < kChunkSize; ++lx) {
                for (int ly = 0; ly < kChunkSize; ++ly) {
                    for (int lz = 0; lz < kChunkSize; ++lz) {
                        if (w.chunks[cp].get(lx, ly, lz) == id) {
                            inGroup.insert(
                                BlockPos {cp.x * kChunkSize + lx,
                                          cp.y * kChunkSize + ly,
                                          cp.z * kChunkSize + lz});
                        }
                        if (solo.get(lx, ly, lz) == id) {
                            inSolo.insert(
                                BlockPos {cp.x * kChunkSize + lx,
                                          cp.y * kChunkSize + ly,
                                          cp.z * kChunkSize + lz});
                        }
                    }
                }
            }
            EXPECT_EQ(inGroup, inSolo) << "chunk " << cp.x << "," << cp.y
                                       << "," << cp.z << " ore "
                                       << w.oreNames[t];
        }
    }
}

TEST(WorldGeneratorTest, Gen3GravelCellsMatchEllipsoidPlans) {
    // The CaveMatchesPurePredicate pattern for GEN-3: with the SHIPPED
    // params (K = 1, the re-tuned probability), a region cell holds
    // gravel IFF the world-coordinate lattice plan places it there — the
    // probability gate + the in-cell start + planOreVein(salt, start, 1)
    // — and the cell is uncarved deep stone (the maxY band + the
    // deep-only filter are placeOre's, unchanged). A 4x4-column region
    // (16 columns: the predicate scan is cheap enough to be exact).
    Gen3World w;
    ASSERT_TRUE(w.load()) << "run the tests from the repository root";
    const int cx0 = 4, cx1 = 7, cz0 = 0, cz1 = 3;
    w.generateRegion(cx0, cx1, cz0, cz1);
    const int x0 = cx0 * kChunkSize, x1 = (cx1 + 1) * kChunkSize;
    const int z0 = cz0 * kChunkSize, z1 = (cz1 + 1) * kChunkSize;
    // The gravel entry (its lattice index = its worldgen.json order).
    int t = -1;
    for (std::size_t i = 0; i < w.params.ores.size(); ++i) {
        if (w.params.ores[i].block == "gravel") {
            t = static_cast<int>(i);
        }
    }
    ASSERT_GE(t, 0) << "shipped worldgen.json must name the gravel vein";
    const OreConfig& gravelCfg = w.params.ores[static_cast<std::size_t>(t)];
    const std::uint64_t salt =
        static_cast<std::uint64_t>(kGen3ProbeSeed ^ kSaltOre) ^
        static_cast<std::uint64_t>(t + 1) * kFeatureStep;
    // The lattice box any of the region's chunks scans (margin = s + 3 +
    // 2*(K-1) = 11 at the shipped K = 1).
    const int s = w.params.oreLattice;
    const int margin = s + kOreVeinMaxAxis +
                       kOreVeinClusterSpread * (gravelCfg.veinSize - 1);
    const int xi0 = floorDiv(x0 - margin, s);
    const int xi1 = floorDiv(x1 + margin - 1, s);
    const int yi0 = floorDiv(0 - margin, s);
    const int yi1 = floorDiv(kWorldHeight + margin - 1, s);
    const int zi0 = floorDiv(z0 - margin, s);
    const int zi1 = floorDiv(z1 + margin - 1, s);
    std::set<BlockPos> planned;
    for (int i = xi0; i <= xi1; ++i) {
        for (int j = yi0; j <= yi1; ++j) {
            for (int k = zi0; k <= zi1; ++k) {
                const std::uint64_t h = hash3(
                    static_cast<std::uint64_t>(static_cast<std::int64_t>(i)),
                    static_cast<std::uint64_t>(static_cast<std::int64_t>(j)),
                    static_cast<std::uint64_t>(static_cast<std::int64_t>(k)),
                    salt);
                if (static_cast<float>(h >> 40) * kLattice24Scale >=
                    gravelCfg.probability) {
                    continue;
                }
                const BlockPos start {
                    i * s + static_cast<int>((h >> 12) % static_cast<std::uint64_t>(s)),
                    j * s + static_cast<int>((h >> 20) % static_cast<std::uint64_t>(s)),
                    k * s + static_cast<int>((h >> 28) % static_cast<std::uint64_t>(s))};
                for (const BlockPos& cell :
                     planOreVein(salt, start, gravelCfg.veinSize)) {
                    if (cell.x < x0 || cell.x >= x1 || cell.y < 0 ||
                        cell.y >= kWorldHeight || cell.z < z0 ||
                        cell.z >= z1 || cell.y < 1 ||
                        cell.y > gravelCfg.maxY) {
                        continue; // the region / placeOre's y band
                    }
                    planned.insert(cell);
                }
            }
        }
    }
    const BlockID gravelId = w.blocks.idForName("gravel");
    const BlockID deepId = w.blocks.idForName(w.params.deepBlock);
    int inPlan = 0, checked = 0;
    for (int cx = cx0; cx <= cx1; ++cx) {
        for (int cy = 0; cy < kGen3Layers; ++cy) {
            for (int cz = cz0; cz <= cz1; ++cz) {
                const Chunk& chunk = w.chunks[{cx, cy, cz}];
                for (int lx = 0; lx < kChunkSize; ++lx) {
                    for (int ly = 0; ly < kChunkSize; ++ly) {
                        for (int lz = 0; lz < kChunkSize; ++lz) {
                            const BlockPos cell {cx * kChunkSize + lx,
                                                 cy * kChunkSize + ly,
                                                 cz * kChunkSize + lz};
                            const BlockID content = chunk.get(lx, ly, lz);
                            const bool isGravel = content == gravelId;
                            const bool isDeep = content == deepId;
                            const bool inPlanned = planned.count(cell) != 0;
                            if (isGravel) {
                                EXPECT_TRUE(inPlanned)
                                    << "gravel outside every ellipsoid plan "
                                    << cell.x << "," << cell.y << ","
                                    << cell.z;
                                ++inPlan;
                            }
                            if (inPlanned && isDeep) {
                                EXPECT_TRUE(isGravel)
                                    << "planned deep cell not gravel "
                                    << cell.x << "," << cell.y << ","
                                    << cell.z;
                            }
                            ++checked;
                        }
                    }
                }
            }
        }
    }
    EXPECT_GT(inPlan, 100) << "the region holds plenty of planned gravel";
    EXPECT_GT(checked, 200000);
}
