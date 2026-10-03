#include "world/world_generator.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <string>
#include <vector>

#include "core/logger.hpp"
#include "world/noise.hpp"

namespace mc {

namespace {

// Distinct salts keep the noise fields of independent features (height,
// biome, caves, ores, trees) uncorrelated, so e.g. cave shape never tracks
// the terrain height.
constexpr std::int64_t kSaltHeight = 0x1A2B3C4D5E6F7A01LL;
constexpr std::int64_t kSaltBiome = 0x1A2B3C4D5E6F7A02LL;
constexpr std::int64_t kSaltCave = 0x1A2B3C4D5E6F7A03LL;
constexpr std::int64_t kSaltOre = 0x1A2B3C4D5E6F7A04LL;
constexpr std::int64_t kSaltTree = 0x1A2B3C4D5E6F7A05LL;

// Per-feature mixing constant (per ore type, per vein-walk step).
constexpr std::uint64_t kFeatureStep = 0x9E3779B97F4A7C15ULL;

// 2^24, the resolution of the 24-bit lattice values.
constexpr float kLattice24Scale = 1.0f / 16777216.0f;

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
        rb.subsurface = blocks.idForName(b.subsurface);
        if (!rb.subsurface.valid()) {
            return MC_FAIL("biome '" + b.name +
                           "': unknown subsurface block '" + b.subsurface +
                           "'");
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
        {"terrain water block", params.waterBlock, &gen.m_water},
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
                } else if (h < m_params.seaLevel && wy <= m_params.seaLevel) {
                    // The column is underwater: fill air up to the sea level.
                    id = m_water;
                }
                chunk.set(lx, ly, lz, id);
            }
        }
    }
}

void WorldGenerator::carveCaves(Chunk& chunk, const ChunkPos& pos,
                                const std::array<int, kChunkSize * kChunkSize>& heights) const {
    if (!m_params.cavesEnabled) {
        return;
    }
    const int ox = pos.x * kChunkSize;
    const int oz = pos.z * kChunkSize;
    const int y0 = pos.y * kChunkSize;
    const float cs = m_params.caveNoiseScale;
    for (int lx = 0; lx < kChunkSize; ++lx) {
        for (int lz = 0; lz < kChunkSize; ++lz) {
            const int h =
                heights[static_cast<std::size_t>(lx) * kChunkSize +
                        static_cast<std::size_t>(lz)];
            for (int ly = 0; ly < kChunkSize; ++ly) {
                const int wy = y0 + ly;
                // Keep a floor at the bottom and a shell under the surface.
                if (wy < m_params.caveMinY ||
                    wy >= h - m_params.surfaceGuard) {
                    continue;
                }
                const float n = valueNoise3(static_cast<float>(ox + lx) * cs,
                                            static_cast<float>(wy) * cs,
                                            static_cast<float>(oz + lz) * cs,
                                            m_seed ^ kSaltCave);
                if (n > m_params.caveThreshold) {
                    chunk.set(lx, ly, lz, BlockID::air());
                }
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
    // A vein started in a lattice cell can reach this chunk only if it
    // starts within veinSize blocks of it (the walk is at most that long),
    // so scan cells from `margin` = maxVeinSize + one cell past each edge.
    int maxVeinSize = 0;
    for (const ResolvedOre& ore : m_ores) {
        maxVeinSize = std::max(maxVeinSize, ore.veinSize);
    }
    const int margin = maxVeinSize + s;
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
                    // Random walk: each step is a pure function of the
                    // previous state, so the whole vein is reproducible.
                    std::uint64_t state = h;
                    for (int step = 0; step < ore.veinSize; ++step) {
                        state = splitmix64(state ^ kFeatureStep ^ u64(step));
                        const int dx = static_cast<int>(state % 3u) - 1;
                        const int dy = static_cast<int>((state >> 2) % 3u) - 1;
                        const int dz = static_cast<int>((state >> 4) % 3u) - 1;
                        placeOre(chunk, pos, vx, vy, vz, ore);
                        vx += dx;
                        vy += dy;
                        vz += dz;
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

} // namespace mc
