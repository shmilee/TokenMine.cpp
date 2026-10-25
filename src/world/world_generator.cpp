#include "world/world_generator.hpp"

#include <algorithm>
#include <array>
#include <climits>
#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <string>
#include <vector>

#include "core/logger.hpp"
#include "world/noise.hpp"
#include "world/ore_vein.hpp" // GEN-3: planOreVein + the margin constants

namespace mc {

namespace {

// Trees extend at most this many blocks horizontally from their trunk.
constexpr int kTreeLeafRadius = 2;

// Reinterpret an (possibly negative) int as 64 bits for hashing.
// Two's-complement reinterpretation is a bijection, so this is a valid
// deterministic mapping for negative coordinates.
std::uint64_t u64(int v) {
    return static_cast<std::uint64_t>(static_cast<std::int64_t>(v));
}

bool insideChunk(const ChunkPos& pos, int wx, int wy, int wz) {
    if (wy < 0 || wy >= kWorldHeight) {
        return false;
    }
    return worldToChunk(BlockPos{wx, wy, wz}) == pos;
}

} // namespace

Result<WorldGenerator> WorldGenerator::create(std::int64_t seed,
                                              const WorldgenParams& params,
                                              const BiomeRegistry& biomes,
                                              const BlockRegistry& blocks) {
    if (biomes.count() == 0) {
        return MC_FAIL("world generation requires at least one biome "
                       "(assets/data/biomes/*.json)");
    }

    WorldGenerator gen;
    gen.m_seed = seed;
    gen.m_params = params;

    // ENT-2: the per-block state invariant — worldgen places NO stateful
    // block. A per-instance state has no worldgen writer (the game's
    // setBlock choke point is the only one), and the save journal derives
    // the state array from the BLOCK journal on exactly this premise
    // (every stateful cell was block-edited this session) — a generator
    // that placed a stateful block would silently lose its state on a
    // journal save. The placement sources: the biome surfaces /
    // subsurfaces / decorations, the ore veins, the tree trunks / leaves
    // (checked at each resolution below, named).
    const auto rejectStateful = [&](const char* what, const std::string& name,
                                    BlockID id) -> Result<void> {
        if (blocks.at(id).hasState()) {
            return MC_FAIL(std::string("worldgen ") + what + " '" + name +
                           "' has a per-instance state — worldgen never "
                           "writes state (the game's setBlock choke point "
                           "is the only state writer)");
        }
        return {};
    };

    for (const Biome& b : biomes.all()) {
        ResolvedBiome rb;
        rb.name = b.name;
        rb.weight = b.weight;
        rb.subsurfaceDepth = b.subsurfaceDepth;
        rb.treeDensity = b.treeDensity;
        rb.heightScale = b.heightScale;
        rb.surface = blocks.idForName(b.surface);
        if (!rb.surface.valid()) {
            return MC_FAIL("biome '" + b.name + "': unknown surface block '" +
                           b.surface + "'");
        }
        MC_TRY(rejectStateful("biome surface block", b.surface, rb.surface));
        rb.subsurface = blocks.idForName(b.subsurface);
        if (!rb.subsurface.valid()) {
            return MC_FAIL("biome '" + b.name +
                           "': unknown subsurface block '" + b.subsurface +
                           "'");
        }
        MC_TRY(rejectStateful("biome subsurface block", b.subsurface,
                              rb.subsurface));
        // Surface decorations (Phase 45): resolve each block name.
        for (const BiomeDecoration& d : b.decorations) {
            ResolvedDecoration rd;
            rd.probability = d.probability;
            rd.block = blocks.idForName(d.block);
            if (!rd.block.valid()) {
                return MC_FAIL("biome '" + b.name +
                               "': unknown decoration block '" + d.block +
                               "'");
            }
            MC_TRY(rejectStateful("biome decoration block", d.block,
                                  rd.block));
            rb.decorations.push_back(std::move(rd));
        }
        gen.m_totalWeight += rb.weight;
        gen.m_biomes.push_back(std::move(rb));
    }

    for (const OreConfig& o : params.ores) {
        ResolvedOre ro;
        ro.probability = o.probability;
        ro.veinSize = o.veinSize;
        ro.maxY = o.maxY;
        ro.block = blocks.idForName(o.block);
        if (!ro.block.valid()) {
            return MC_FAIL("worldgen ore: unknown block '" + o.block + "'");
        }
        MC_TRY(rejectStateful("ore", o.block, ro.block));
        gen.m_ores.push_back(std::move(ro));
    }

    struct FillBlock {
        const char* what;
        const std::string& name;
        BlockID* out;
    };
    const FillBlock fills[] = {
        {"terrain bottom block", params.bottomBlock, &gen.m_bottom},
        {"terrain deep block", params.deepBlock, &gen.m_deep},
        {"tree trunk block", params.treeTrunk, &gen.m_trunk},
        {"tree leaves block", params.treeLeaves, &gen.m_leaves},
    };
    for (const FillBlock& f : fills) {
        *f.out = blocks.idForName(f.name);
        if (!f.out->valid()) {
            return MC_FAIL(std::string("worldgen ") + f.what +
                           ": unknown block '" + f.name + "'");
        }
    }
    // ENT-2: the tree trunks / leaves are worldgen placement sources too
    // (the terrain fills — bottom / deep — are not listed among the
    // stateful-feature sources by design: they are the world's bedrock and
    // deep stone, which the project's stateful roadmap (facing / top-bottom
    // / sapling stages) never puts there; the real-asset test
    // SaveJournalTest.RealWorldgenPlacesNoStatefulBlock pins the whole
    // generated region empirically).
    MC_TRY(rejectStateful("tree trunk block", params.treeTrunk, gen.m_trunk));
    MC_TRY(rejectStateful("tree leaves block", params.treeLeaves,
                          gen.m_leaves));
    // Phase 93: derive the procedural lakes (a pure function of the seed, so
    // they are the same across runs + chunks).
    gen.computeProceduralLakes();
    // GEN-1: resolve every enabled lake into an exclusion box + floor (the
    // cave carve skips the basins; the lake pass reads the precomputed
    // floor instead of rescanning the region per chunk).
    gen.computeLakes();
    return gen;
}

std::size_t WorldGenerator::biomeIndexAt(int x, int z) const {
    // Low-frequency noise in [0, 1) mapped onto cumulative weight over the
    // biomes in registry load order (deterministic).
    const float n = valueNoise2(static_cast<float>(x) * m_params.biomeNoiseScale,
                                static_cast<float>(z) * m_params.biomeNoiseScale,
                                m_seed ^ kSaltBiome);
    const int target = static_cast<int>(n * static_cast<float>(m_totalWeight));
    int acc = 0;
    for (std::size_t i = 0; i < m_biomes.size(); ++i) {
        acc += m_biomes[i].weight;
        if (target < acc) {
            return i;
        }
    }
    return m_biomes.size() - 1;
}

const WorldGenerator::ResolvedBiome&
WorldGenerator::resolvedBiomeAt(int x, int z) const {
    return m_biomes[biomeIndexAt(x, z)];
}

int WorldGenerator::heightAt(int x, int z) const {
    const float n = fbm2(static_cast<float>(x) * m_params.heightNoiseScale,
                         static_cast<float>(z) * m_params.heightNoiseScale,
                         m_seed ^ kSaltHeight, m_params.heightOctaves);
    const float h = static_cast<float>(m_params.baseHeight) +
                    (n - 0.5f) * 2.0f *
                    static_cast<float>(m_params.heightAmplitude) *
                    resolvedBiomeAt(x, z).heightScale;
    const int height = static_cast<int>(std::lroundf(h));
    constexpr int kMinHeight = 8;
    const int kMaxHeight = kWorldHeight - 16;
    if (height < kMinHeight) {
        return kMinHeight;
    }
    if (height > kMaxHeight) {
        return kMaxHeight;
    }
    return height;
}

int WorldGenerator::surfaceHeight(int x, int z) const {
    return heightAt(x, z);
}

const std::string& WorldGenerator::biomeNameAt(int x, int z) const {
    return m_biomes[biomeIndexAt(x, z)].name;
}

void WorldGenerator::generateChunk(Chunk& chunk, const ChunkPos& pos) const {
    // Per-column terrain heights, computed once and shared by the terrain
    // fill and the cave carving (they must agree on where the surface is).
    std::array<int, kChunkSize * kChunkSize> heights {};
    const int ox = pos.x * kChunkSize;
    const int oz = pos.z * kChunkSize;
    for (int lx = 0; lx < kChunkSize; ++lx) {
        for (int lz = 0; lz < kChunkSize; ++lz) {
            heights[static_cast<std::size_t>(lx) * kChunkSize +
                    static_cast<std::size_t>(lz)] = heightAt(ox + lx, oz + lz);
        }
    }

    fillTerrain(chunk, pos, heights);
    carveCaves(chunk, pos, heights);
    placeOres(chunk, pos);
    placeTrees(chunk, pos);
    placeDecorations(chunk, pos); // Phase 45: after trees (trunks win)
    placeLavaLakes(chunk, pos); // Phase 86/88: the contained lava pool(s)
}

void WorldGenerator::fillTerrain(Chunk& chunk, const ChunkPos& pos,
                                 const std::array<int, kChunkSize * kChunkSize>& heights) const {
    const int ox = pos.x * kChunkSize;
    const int oz = pos.z * kChunkSize;
    const int y0 = pos.y * kChunkSize;
    for (int lx = 0; lx < kChunkSize; ++lx) {
        for (int lz = 0; lz < kChunkSize; ++lz) {
            const int h =
                heights[static_cast<std::size_t>(lx) * kChunkSize +
                        static_cast<std::size_t>(lz)];
            const ResolvedBiome& b = resolvedBiomeAt(ox + lx, oz + lz);
            for (int ly = 0; ly < kChunkSize; ++ly) {
                const int wy = y0 + ly;
                BlockID id = BlockID::air();
                if (wy == 0) {
                    id = m_bottom;
                } else if (wy < h) {
                    id = (wy < h - b.subsurfaceDepth) ? m_deep : b.subsurface;
                } else if (wy == h) {
                    id = b.surface;
                }
                chunk.set(lx, ly, lz, id);
                if (h < m_params.seaLevel && wy > h && wy <= m_params.seaLevel) {
                    // The column is underwater: fill the full fluid level up
                    // to the sea level (water lives in the fluid array; the
                    // block stays air).
                    chunk.setFluidAt(lx, ly, lz,
                                     fluidCode(FluidKind::Water,
                                               kMaxFluidLevel));
                }
            }
        }
    }
}

bool WorldGenerator::caveCarvesCell(int wx, int wy, int wz, int height) const {
    if (!m_params.cavesEnabled) {
        return false;
    }
    // GEN-1: the spawn clear zone — a chunk whose column is within
    // `spawn_clear_columns` of the world origin on BOTH axes is never
    // carved (the spawn candidate columns, {-1,0}², must keep intact dry
    // ground). A per-chunk early return: a chunk is wholly in or out of
    // the zone (the zone is column-aligned).
    const ChunkPos cp = worldToChunk(BlockPos{wx, wy, wz});
    if (std::abs(cp.x) <= m_params.caveSpawnClearColumns &&
        std::abs(cp.z) <= m_params.caveSpawnClearColumns) {
        return false;
    }
    // GEN-1: surface_guard 0 lifts the shell below the surface — cave
    // openings may reach the surface cell itself (the shipped value, the
    // Minecraft cave entrances). guard > 0 keeps the pre-GEN-1 band (never
    // within `guard` layers of the surface, the surface cell excluded).
    const int guard = m_params.surfaceGuard;
    const int topExclusive = (guard > 0) ? (height - guard) : (height + 1);
    // Keep a floor at the bottom and (guard > 0) a shell under the surface.
    if (wy < m_params.caveMinY || wy >= topExclusive) {
        return false;
    }
    // GEN-1: the lava-lake exclusion boxes — the basin owns its carve (the
    // lake pass, which runs LATER, carves the same region anyway; skipping
    // it here keeps the basin geometry exact and is what makes the box
    // observable: caves-on vs caves-off differ only OUTSIDE the box).
    for (const ResolvedLake& rl : m_lakes) {
        if (wx >= rl.x0 && wx <= rl.x1 && wz >= rl.z0 && wz <= rl.z1 &&
            wy > rl.floorY) {
            return false;
        }
    }
    const float cs = m_params.caveNoiseScale;
    const float t = m_params.caveTubeThreshold;
    // GEN-1: the spaghetti-tube predicate — the intersection of
    // TWO independent noise fields' mid-bands, |n - 0.5| < t on
    // BOTH (the intersection of two thin 3D bands is a network
    // of winding tubes, the Minecraft 1.18 cave shape — the old
    // single-noise high-region produced blobs). The second field
    // is evaluated ONLY when the first is in-band (same
    // predicate, less work).
    const float n1 = valueNoise3(static_cast<float>(wx) * cs,
                                 static_cast<float>(wy) * cs,
                                 static_cast<float>(wz) * cs,
                                 m_seed ^ kSaltCave);
    if (std::fabs(n1 - 0.5f) >= t) {
        return false;
    }
    const float n2 = valueNoise3(static_cast<float>(wx) * cs,
                                 static_cast<float>(wy) * cs,
                                 static_cast<float>(wz) * cs,
                                 m_seed ^ kSaltCave2);
    return std::fabs(n2 - 0.5f) < t;
}

void WorldGenerator::carveCaves(Chunk& chunk, const ChunkPos& pos,
                                const std::array<int, kChunkSize * kChunkSize>& heights) const {
    if (!m_params.cavesEnabled) {
        return;
    }
    // GEN-1: the spawn clear zone — a chunk whose column is within
    // `spawn_clear_columns` of the world origin on BOTH axes is never
    // carved (the spawn candidate columns, {-1,0}², must keep intact dry
    // ground). A per-chunk early return: a chunk is wholly in or out of
    // the zone (the zone is column-aligned).
    if (std::abs(pos.x) <= m_params.caveSpawnClearColumns &&
        std::abs(pos.z) <= m_params.caveSpawnClearColumns) {
        return;
    }
    const int ox = pos.x * kChunkSize;
    const int oz = pos.z * kChunkSize;
    const int y0 = pos.y * kChunkSize;
    for (int lx = 0; lx < kChunkSize; ++lx) {
        for (int lz = 0; lz < kChunkSize; ++lz) {
            const int h =
                heights[static_cast<std::size_t>(lx) * kChunkSize +
                        static_cast<std::size_t>(lz)];
            const int wx = ox + lx;
            const int wz = oz + lz;
            for (int ly = 0; ly < kChunkSize; ++ly) {
                const int wy = y0 + ly;
                // GEN-5: the per-cell carve decision — the pure predicate
                // (cavesEnabled / spawn zone / band / lake boxes / double
                // mid-band) moved out of this loop bit-for-bit, so
                // placeTrees' floating-tree guard calls the SAME decision
                // from a neighbor chunk without reading any chunk data.
                if (!caveCarvesCell(wx, wy, wz, h)) {
                    continue;
                }
                chunk.set(lx, ly, lz, BlockID::air());
            }
        }
    }
}

void WorldGenerator::placeOres(Chunk& chunk, const ChunkPos& pos) const {
    if (m_ores.empty()) {
        return;
    }
    const int s = m_params.oreLattice;
    const int ox = pos.x * kChunkSize;
    const int oz = pos.z * kChunkSize;
    const int y0 = pos.y * kChunkSize;
    // GEN-3: a vein started in a lattice cell is K ellipsoid clusters —
    // the first centred on the in-cell uniform start, each next within
    // kOreVeinClusterSpread of the previous, and each ellipsoid reaching
    // kOreVeinMaxAxis from its centre. So a vein can reach this chunk
    // only if it starts within `margin` = lattice + max semi-axis +
    // (K-1) cluster steps past each edge.
    int maxClusters = 1;
    for (const ResolvedOre& ore : m_ores) {
        maxClusters = std::max(maxClusters, ore.veinSize);
    }
    const int margin = s + kOreVeinMaxAxis +
                       kOreVeinClusterSpread * (maxClusters - 1);
    const int xi0 = floorDiv(ox - margin, s);
    const int xi1 = floorDiv(ox + kChunkSize + margin - 1, s);
    const int yi0 = floorDiv(y0 - margin, s);
    const int yi1 = floorDiv(y0 + kChunkSize + margin - 1, s);
    const int zi0 = floorDiv(oz - margin, s);
    const int zi1 = floorDiv(oz + kChunkSize + margin - 1, s);

    for (int i = xi0; i <= xi1; ++i) {
        for (int j = yi0; j <= yi1; ++j) {
            for (int k = zi0; k <= zi1; ++k) {
                for (std::size_t t = 0; t < m_ores.size(); ++t) {
                    const ResolvedOre& ore = m_ores[t];
                    const std::uint64_t salt =
                        static_cast<std::uint64_t>(m_seed ^ kSaltOre) ^
                        (t + 1) * kFeatureStep;
                    const std::uint64_t h = hash3(u64(i), u64(j), u64(k), salt);
                    if (static_cast<float>(h >> 40) * kLattice24Scale >=
                        ore.probability) {
                        continue;
                    }
                    // Vein start: a uniform point inside the cell.
                    int vx = i * s + static_cast<int>((h >> 12) % u64(s));
                    int vy = j * s + static_cast<int>((h >> 20) % u64(s));
                    int vz = k * s + static_cast<int>((h >> 28) % u64(s));
                    // GEN-3: the vein is a cluster of MC 3x2x1 ellipsoids
                    // (the retired 26-neighbor random walk): K = veinSize
                    // ellipsoids planned by a pure function of the SAME
                    // per-ore salt stream + the in-cell start (the PITS #4
                    // advancing stream inside — a new vein-dedicated salt).
                    // Every planned cell goes through the unchanged
                    // placeOre (the y bounds / inside-chunk / deep-only
                    // filters); the world-coordinate start keeps the
                    // cross-chunk seam exact (each chunk plans the same
                    // vein and writes only its own cells).
                    for (const BlockPos& cell :
                         planOreVein(salt, {vx, vy, vz}, ore.veinSize)) {
                        placeOre(chunk, pos, cell.x, cell.y, cell.z, ore);
                    }
                }
            }
        }
    }
}

void WorldGenerator::placeOre(Chunk& chunk, const ChunkPos& pos, int wx,
                              int wy, int wz, const ResolvedOre& ore) const {
    if (wy < 1 || wy > ore.maxY || !insideChunk(pos, wx, wy, wz)) {
        return;
    }
    const int lx = wx - pos.x * kChunkSize;
    const int ly = wy - pos.y * kChunkSize;
    const int lz = wz - pos.z * kChunkSize;
    // Only replace deep stone: caves (air) and surface layers stay intact.
    if (chunk.get(lx, ly, lz) != m_deep) {
        return;
    }
    chunk.set(lx, ly, lz, ore.block);
}

void WorldGenerator::placeTrees(Chunk& chunk, const ChunkPos& pos) const {
    const int s = m_params.treeLattice;
    const int ox = pos.x * kChunkSize;
    const int oz = pos.z * kChunkSize;
    // A tree extends up to kTreeLeafRadius blocks horizontally from its
    // trunk, so consider lattice candidates within that margin: a tree
    // straddling a chunk boundary is completed by every neighbor chunk.
    const int margin = kTreeLeafRadius + 1;
    const int wx0 = floorDiv(ox - margin, s) * s;
    const int wz0 = floorDiv(oz - margin, s) * s;
    const int limit = ox + kChunkSize + margin;

    for (int wx = wx0; wx < limit; wx += s) {
        for (int wz = wz0; wz < limit; wz += s) {
            const ResolvedBiome& b = resolvedBiomeAt(wx, wz);
            if (b.treeDensity <= 0.0f) {
                continue;
            }
            const std::uint64_t h =
                hash2(u64(wx), u64(wz),
                      static_cast<std::uint64_t>(m_seed ^ kSaltTree));
            if (static_cast<float>(h >> 40) * kLattice24Scale >=
                b.treeDensity) {
                continue;
            }
            const int hgt = heightAt(wx, wz);
            if (hgt <= m_params.seaLevel) {
                continue; // no trees underwater
            }
            // GEN-5: the floating-tree guard — the trunk column's surface
            // cell must still be terrain. carveCaves runs FIRST, and with
            // the shipped surface_guard 0 its openings reach the surface
            // cell itself: planting here would float the whole tree over
            // the hole (the placeDecorations guard is the same rule —
            // "a flower would float over the hole" — trees were the missed
            // case). The decision is the SAME pure predicate carveCaves
            // uses, so it reads no chunk data: every chunk whose margin
            // covers this column recomputes the same verdict and skips the
            // whole tree (a half-placed canopy straddling a chunk boundary
            // is impossible).
            if (caveCarvesCell(wx, hgt, wz, hgt)) {
                continue;
            }
            const int trunk = m_params.treeMinHeight +
                              static_cast<int>(
                                  (h >> 24) %
                                  static_cast<std::uint64_t>(
                                      m_params.treeHeightRange + 1));
            for (int ty = 1; ty <= trunk; ++ty) {
                placeIfAir(chunk, pos, wx, hgt + ty, wz, m_trunk);
            }
            // Canopy: two full-radius layers around the trunk top, then a
            // smaller cap on very top.
            for (int dy = trunk - 1; dy <= trunk + 1; ++dy) {
                const int radius = (dy == trunk + 1) ? 1 : kTreeLeafRadius;
                for (int dx = -radius; dx <= radius; ++dx) {
                    for (int dz = -radius; dz <= radius; ++dz) {
                        if (dx == 0 && dz == 0 && dy <= trunk) {
                            continue; // trunk cell
                        }
                        if (radius == kTreeLeafRadius &&
                            std::abs(dx) == radius && std::abs(dz) == radius) {
                            continue; // round the corners
                        }
                        placeIfAir(chunk, pos, wx + dx, hgt + dy, wz + dz,
                                   m_leaves);
                    }
                }
            }
        }
    }
}

void WorldGenerator::placeDecorations(Chunk& chunk, const ChunkPos& pos) const {
    const int s = m_params.treeLattice;
    const int ox = pos.x * kChunkSize;
    const int oz = pos.z * kChunkSize;
    // Decorations occupy exactly one cell (no margin), so lattice candidates
    // are the cells of this chunk itself.
    const int wx0 = floorDiv(ox, s) * s;
    const int wz0 = floorDiv(oz, s) * s;
    const int limit = ox + kChunkSize;

    for (int wx = wx0; wx < limit; wx += s) {
        for (int wz = wz0; wz < limit; wz += s) {
            const ResolvedBiome& b = resolvedBiomeAt(wx, wz);
            if (b.decorations.empty()) {
                continue;
            }
            const int hgt = heightAt(wx, wz);
            if (hgt <= m_params.seaLevel) {
                continue; // no decorations underwater (trees' rule)
            }
            // The decoration lands one ABOVE the surface cell. That surface
            // cell must still hold the biome's own surface block: carveCaves
            // may have eaten it (a flower would float over the hole), and a
            // different surface (e.g. sand at a biome edge) is not this
            // biome's ground. The surface cell is in the same column, so it
            // is in this chunk exactly when the decoration cell is — except
            // when hgt+1 leaves the chunk's y range, which placeIfAir
            // rejects anyway (documented: a decoration straddling a chunk's
            // top y boundary is dropped, like the topmost tree-ring cells).
            const int lsy = hgt - pos.y * kChunkSize;
            const int lsx = wx - ox;
            const int lsz = wz - oz;
            if (lsy < 0 || lsy >= kChunkSize || lsx < 0 || lsx >= kChunkSize ||
                lsz < 0 || lsz >= kChunkSize) {
                continue;
            }
            if (chunk.get(lsx, lsy, lsz) != b.surface) {
                continue; // cave opening / not this biome's ground
            }
            // One independent roll per (cell, entry) pair: the entry index
            // is a third hash input (hash3), the salt is dedicated, so
            // decoration rolls never track the tree rolls.
            for (std::size_t i = 0; i < b.decorations.size(); ++i) {
                const std::uint64_t h =
                    hash3(static_cast<std::uint64_t>(wx),
                          static_cast<std::uint64_t>(wz),
                          static_cast<std::uint64_t>(i),
                          static_cast<std::uint64_t>(
                              m_seed ^ kSaltDecoration));
                if (static_cast<float>(h >> 40) * kLattice24Scale <
                    b.decorations[i].probability) {
                    placeIfAir(chunk, pos, wx, hgt + 1, wz,
                               b.decorations[i].block);
                }
            }
        }
    }
}

void WorldGenerator::placeIfAir(Chunk& chunk, const ChunkPos& pos, int wx,
                                int wy, int wz, BlockID id) const {
    if (!insideChunk(pos, wx, wy, wz)) {
        return;
    }
    const int lx = wx - pos.x * kChunkSize;
    const int ly = wy - pos.y * kChunkSize;
    const int lz = wz - pos.z * kChunkSize;
    // Never overwrite existing terrain: trees grow into open air only, so
    // overlapping canopies merge and hills are preserved.
    if (chunk.get(lx, ly, lz) == BlockID::air()) {
        chunk.set(lx, ly, lz, id);
    }
}

void WorldGenerator::computeProceduralLakes() {
    // Phase 93: derive the procedural lakes from the seed. A state-advancing
    // splitmix64 stream (PITS #4: advance with the golden step, then mix)
    // yields, for each lake i, a deterministic (x, z, radius, depth, fill)
    // drawn from the configured ranges. The lakes are the same across runs +
    // chunks (a pure function of the seed).
    m_proceduralLakes.clear();
    const auto& pd = m_params.lavaLakesProcedural;
    if (!pd.enabled || pd.count <= 0) {
        return;
    }
    const std::uint64_t base =
        static_cast<std::uint64_t>(m_seed) ^
        static_cast<std::uint64_t>(kSaltLavaLake);
    const int xSpan = pd.regionMaxX - pd.regionMinX + 1;
    const int zSpan = pd.regionMaxZ - pd.regionMinZ + 1;
    const int rSpan = pd.maxRadius - pd.minRadius + 1;
    const int dSpan = pd.maxDepth - pd.minDepth + 1;
    const int fSpan = pd.maxFill - pd.minFill + 1;
    for (int i = 0; i < pd.count; ++i) {
        // A fresh state per lake (seed-advanced by i * golden), then 5
        // advances for (x, z, radius, depth, fill).
        std::uint64_t state = splitmix64(base + std::uint64_t(i) * kFeatureStep);
        WorldgenParams::LavaLake lake;
        lake.enabled = true;
        lake.centerX = pd.regionMinX +
                       static_cast<int>((splitmix64(state += kFeatureStep) >> 32) %
                                        std::uint64_t(xSpan));
        lake.centerZ = pd.regionMinZ +
                       static_cast<int>((splitmix64(state += kFeatureStep) >> 32) %
                                        std::uint64_t(zSpan));
        lake.radius = pd.minRadius +
                      static_cast<int>((splitmix64(state += kFeatureStep) >> 40) %
                                       std::uint64_t(rSpan));
        lake.depth = pd.minDepth +
                     static_cast<int>((splitmix64(state += kFeatureStep) >> 40) %
                                      std::uint64_t(dSpan));
        lake.fill = pd.minFill +
                    static_cast<int>((splitmix64(state += kFeatureStep) >> 40) %
                                     std::uint64_t(fSpan));
        m_proceduralLakes.push_back(lake);
    }
}

void WorldGenerator::computeLakes() {
    // GEN-1: resolve every enabled lake (the fixed list + the procedural
    // ones) into its region box + basin floor. The floor is the region's
    // lowest terrain minus `depth` — a pure heightAt read, computed ONCE
    // here (placeLavaLakes previously recomputed it per chunk, and
    // carveCaves' exclusion boxes need it without a rescan). The value is
    // identical to the old per-chunk rescan (same pure function).
    m_lakes.clear();
    const auto resolve = [this](const WorldgenParams::LavaLake& lake) {
        if (!lake.enabled) {
            return;
        }
        ResolvedLake rl;
        rl.lake = lake;
        rl.x0 = lake.centerX - lake.radius;
        rl.x1 = lake.centerX + lake.radius;
        rl.z0 = lake.centerZ - lake.radius;
        rl.z1 = lake.centerZ + lake.radius;
        int minH = INT_MAX;
        for (int x = rl.x0; x <= rl.x1; ++x) {
            for (int z = rl.z0; z <= rl.z1; ++z) {
                minH = std::min(minH, heightAt(x, z));
            }
        }
        rl.floorY = minH - lake.depth;
        m_lakes.push_back(std::move(rl));
    };
    for (const auto& lake : m_params.lavaLakes) {
        resolve(lake);
    }
    for (const auto& lake : m_proceduralLakes) {
        resolve(lake);
    }
}

void WorldGenerator::placeLavaLakes(Chunk& chunk, const ChunkPos& pos) const {
    // Phase 88: loop over every configured lake (Phase 86 shipped a single
    // lake; the list generalizes it). Phase 93: the procedural lakes are
    // placed in addition to the fixed list. Each lake is independent (its
    // own region/floor); overlapping lakes carve the same cells
    // (deterministic). GEN-1: the region box + floor come from m_lakes
    // (precomputed in create — the same pure heightAt value the per-chunk
    // rescan always recomputed).
    const int ox = pos.x * kChunkSize;
    const int oz = pos.z * kChunkSize;
    const int y0 = pos.y * kChunkSize;
    for (const ResolvedLake& rl : m_lakes) {
        const WorldgenParams::LavaLake& lake = rl.lake;
        const int x0 = rl.x0, x1 = rl.x1, z0 = rl.z0, z1 = rl.z1;
        const int floorY = rl.floorY;
        for (int lx = 0; lx < kChunkSize; ++lx) {
            for (int lz = 0; lz < kChunkSize; ++lz) {
                const int wx = ox + lx;
                const int wz = oz + lz;
                if (wx < x0 || wx > x1 || wz < z0 || wz > z1) {
                    continue;
                }
                for (int ly = 0; ly < kChunkSize; ++ly) {
                    const int wy = y0 + ly;
                    if (wy <= floorY) {
                        continue; // below the floor: keep the terrain
                    }
                    // Carve the terrain above the floor to air (the basin).
                    if (chunk.get(lx, ly, lz) != BlockID::air()) {
                        chunk.set(lx, ly, lz, BlockID::air());
                    }
                    // Pour full lava in the lowest `fill` levels on the floor.
                    if (wy <= floorY + lake.fill) {
                        chunk.setFluidAt(lx, ly, lz,
                                         fluidCode(FluidKind::Lava,
                                                   kMaxFluidLevel));
                    }
                }
            }
        }
    }
}

} // namespace mc
