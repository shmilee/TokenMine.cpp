#pragma once
// GEN-4 cave-fragmentation calibration probe (v1.0 task 19/31).
//
// User report: "caves are too many and too fragmented, small holes show up
// constantly". The cave predicate is fully data-driven (GEN-1 spaghetti
// tubes: the intersection of two 3D value-noise mid-bands), so the fix is a
// single-round parameter recalibration — zero core changes. This probe:
//
//   1. generates a fixed 32x32-column x full-height sample with the REAL
//      shipped assets (the worldgen.json terrain + biomes), overriding ONLY
//      caves.noise_scale (cs) + caves.tube_threshold (t);
//   2. marks the carved cells in the carve-eligible band
//      [caveMinY, surfaceHeight] (the same band carveCaves writes, with the
//      same topExclusive expression);
//   3. computes 26-neighbour connected-component statistics:
//        - carve rate      = carved cells / band cells
//        - component count, volume p50/p90/max
//        - POCKET FRACTION = the share of carved cells that live in
//          components with < kCaveSmallVolume cells (the user's pain metric:
//          the small dead-end pockets the double-band intersection shreds
//          off the main tubes at the band edges)
//        - surface openings = carved cells on the surface row (the
//          surface_guard 0 entrances — the baseline-view impact input)
//
// Sample region: world x [kCaveSampleX0, kCaveSampleX0 + kCaveSampleCols) x
// z [kCaveSampleZ0, +kCaveSampleCols) = chunks 7..8 x 0..1. It sits OUTSIDE
// the spawn clear zone (|col| <= 2 → world < 48) and OUTSIDE every shipped
// lava-lake box for any seed (the fixed lakes reach x <= 59 with their
// exclusion margin, the procedural region [60, 90]^2 with max radius 3 +
// margin 1 reaches x <= 94; the sample starts at x = 112), so the
// statistics measure pure cave carving.
//
// Determinism: a pure function of (assets, cs, t, seed) — no new random
// source; the same inputs reproduce the same statistics (pinned in the
// gtest). The heavy full-grid table lives in the standalone driver
// (cave_probe_main.cpp), not the suite.

#include <algorithm>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "block/block_registry.hpp"
#include "core/asset_manager.hpp"
#include "core/error.hpp"
#include "world/biome.hpp"
#include "world/chunk.hpp"
#include "world/coordinates.hpp"
#include "world/world_generator.hpp"
#include "world/worldgen_config.hpp"

namespace mc {

// A component with fewer cells than this is a "pocket" (the user pain).
// Strict inequality: a 7-cell hole counts, an 8-cell one does not.
inline constexpr int kCaveSmallVolume = 8;

// The calibration sample (see the header comment for the region choice).
inline constexpr int kCaveSampleCols = 32;
inline constexpr int kCaveSampleX0 = 112;
inline constexpr int kCaveSampleZ0 = 0;

// 26-neighbour connected-component statistics over a carved grid.
struct CaveStats {
    long bandCells = 0;        // the carve-eligible band cells (denominator)
    long carvedCells = 0;      // the carved (air, dry) cells
    double carveRate = 0.0;    // carvedCells / bandCells
    int componentCount = 0;    // the 26-neighbour connected components
    double volumeP50 = 0.0;    // sorted components[v.size() / 2]
    double volumeP90 = 0.0;    // sorted components[v.size() * 9 / 10]
    int volumeMax = 0;         // the largest component
    long smallCells = 0;       // carved cells in components < kCaveSmallVolume
    double smallCellFraction = 0.0; // smallCells / carvedCells
    long surfaceOpenings = 0;  // carved cells on the surface row (y == surfaceHeight)
};

// A carved-cell sample of a generated region.
// Layout: carved[index(rx, y, rz)] != 0 = carved, with rx in [0, nx),
// rz in [0, nz), y in [0, ny) (the region's relative x / world y / z).
struct CaveSample {
    int nx = 0, ny = 0, nz = 0;
    std::vector<std::uint8_t> carved;
    long bandCells = 0;
    long carvedCells = 0;
    long surfaceOpenings = 0;

    std::size_t index(int x, int y, int z) const {
        return (std::size_t(x) * std::size_t(nz) + std::size_t(z)) *
                   std::size_t(ny) +
               std::size_t(y);
    }
};

// Build the carved grid of the band [caveMinY, surfaceHeight] (mirroring
// carveCaves' topExclusive: guard > 0 → h - guard, guard 0 → h + 1) over
// the chunk-aligned column box [x0, x0 + nx) x [z0, z0 + nz), all y layers.
// Generates the full chunk group itself (ore/tree/decoration/lake passes
// included — they never write into the band outside the lake boxes, which
// the sample region avoids by construction).
inline CaveSample buildCaveSample(const WorldGenerator& gen,
                                  const WorldgenParams& params, int x0,
                                  int z0, int nx, int nz) {
    CaveSample s;
    s.nx = nx;
    s.ny = kWorldHeight;
    s.nz = nz;
    s.carved.assign(static_cast<std::size_t>(nx) * std::size_t(nz) *
                        static_cast<std::size_t>(kWorldHeight),
                    0);
    std::map<ChunkPos, Chunk> chunks;
    for (int cx = x0 / kChunkSize; cx <= (x0 + nx - 1) / kChunkSize; ++cx) {
        for (int cz = z0 / kChunkSize; cz <= (z0 + nz - 1) / kChunkSize;
             ++cz) {
            for (int cy = 0; cy * kChunkSize < kWorldHeight; ++cy) {
                const ChunkPos cp{cx, cy, cz};
                gen.generateChunk(chunks[cp], cp);
            }
        }
    }
    const auto isCarved = [&](int x, int y, int z) {
        const ChunkPos cp = worldToChunk(BlockPos{x, y, z});
        const auto it = chunks.find(cp);
        if (it == chunks.end()) {
            return false;
        }
        const BlockPos local = worldToChunkLocal(cp, BlockPos{x, y, z});
        return it->second.get(local.x, local.y, local.z) == BlockID::air() &&
               it->second.fluidAt(local.x, local.y, local.z) == 0;
    };
    const int guard = params.surfaceGuard;
    for (int x = x0; x < x0 + nx; ++x) {
        for (int z = z0; z < z0 + nz; ++z) {
            const int h = gen.surfaceHeight(x, z);
            const int topExclusive = (guard > 0) ? (h - guard) : (h + 1);
            const int yLo = std::max(params.caveMinY, 0);
            const int yHi = std::min(topExclusive, kWorldHeight) - 1;
            for (int y = yLo; y <= yHi; ++y) {
                ++s.bandCells;
                if (!isCarved(x, y, z)) {
                    continue;
                }
                s.carved[s.index(x - x0, y, z - z0)] = 1;
                ++s.carvedCells;
                if (y == h) {
                    ++s.surfaceOpenings;
                }
            }
        }
    }
    return s;
}

// The flood-fill statistics (26 neighbours: face + edge + corner — a cave
// reachable diagonally is one cave). The quantiles follow the project
// convention (SpaghettiTubeWidth): the sorted element at size/2 (p50) and
// size*9/10 (p90), no interpolation.
inline CaveStats caveStats(const CaveSample& sample) {
    CaveStats out;
    out.bandCells = sample.bandCells;
    out.carvedCells = sample.carvedCells;
    out.surfaceOpenings = sample.surfaceOpenings;
    if (sample.bandCells > 0) {
        out.carveRate = static_cast<double>(sample.carvedCells) /
                        static_cast<double>(sample.bandCells);
    }
    if (sample.carvedCells > 0) {
        const int nx = sample.nx, ny = sample.ny, nz = sample.nz;
        const std::size_t yznz =
            static_cast<std::size_t>(ny) * static_cast<std::size_t>(nz);
        std::vector<std::uint8_t> visited(sample.carved.size(), 0);
        std::vector<std::size_t> stack;
        std::vector<int> volumes;
        for (int rx = 0; rx < nx; ++rx) {
            for (int rz = 0; rz < nz; ++rz) {
                for (int y = 0; y < ny; ++y) {
                    const std::size_t start = sample.index(rx, y, rz);
                    if (!sample.carved[start] || visited[start]) {
                        continue;
                    }
                    // One component: iterative DFS, deterministic scan order.
                    long volume = 0;
                    visited[start] = 1;
                    stack.push_back(start);
                    while (!stack.empty()) {
                        const std::size_t c = stack.back();
                        stack.pop_back();
                        ++volume;
                        const int cy = static_cast<int>(c % ny);
                        const int cz = static_cast<int>((c / ny) % nz);
                        const int cx = static_cast<int>(c / yznz);
                        for (int dx = -1; dx <= 1; ++dx) {
                            for (int dy = -1; dy <= 1; ++dy) {
                                for (int dz = -1; dz <= 1; ++dz) {
                                    if (dx == 0 && dy == 0 && dz == 0) {
                                        continue;
                                    }
                                    const int ax = cx + dx;
                                    const int ay = cy + dy;
                                    const int az = cz + dz;
                                    if (ax < 0 || ax >= nx || ay < 0 ||
                                        ay >= ny || az < 0 || az >= nz) {
                                        continue;
                                    }
                                    const std::size_t j =
                                        sample.index(ax, ay, az);
                                    if (sample.carved[j] && !visited[j]) {
                                        visited[j] = 1;
                                        stack.push_back(j);
                                    }
                                }
                            }
                        }
                    }
                    volumes.push_back(static_cast<int>(volume));
                }
            }
        }
        out.componentCount = static_cast<int>(volumes.size());
        if (!volumes.empty()) {
            std::sort(volumes.begin(), volumes.end());
            out.volumeP50 = volumes[volumes.size() / 2];
            out.volumeP90 = volumes[volumes.size() * 9 / 10];
            out.volumeMax = volumes.back();
            for (const int v : volumes) {
                if (v < kCaveSmallVolume) {
                    out.smallCells += v;
                }
            }
        }
        out.smallCellFraction = static_cast<double>(out.smallCells) /
                                static_cast<double>(sample.carvedCells);
    }
    return out;
}

// The REAL-asset probe world (the Gen3World pattern): the shipped
// worldgen.json with the cave noise_scale / tube_threshold overridden per
// probe run.
class CaveProbeWorld {
public:
    // Load the shipped assets (run from the repository root).
    bool load() {
        if (!m_assets.setRoot("assets").ok()) {
            return false;
        }
        if (!m_blocks.loadDirectory(m_assets, "data/blocks").ok()) {
            return false;
        }
        if (!m_biomes.loadDirectory(m_assets, "data/biomes").ok()) {
            return false;
        }
        auto p = WorldgenParams::load(m_assets);
        if (!p.ok()) {
            return false;
        }
        m_params = *p;
        return true;
    }

    // One probe run: the shipped worldgen.json with caves.noise_scale = cs
    // and caves.tube_threshold = t over the chunk-aligned column box
    // [x0, x0 + cols) x [z0, z0 + cols) (cols a multiple of kChunkSize;
    // the standard sample = kCaveSampleX0 / kCaveSampleZ0 /
    // kCaveSampleCols), the full statistics.
    Result<CaveStats> probe(float cs, float t, std::int64_t seed, int x0 =
                                                                 kCaveSampleX0,
                            int z0 = kCaveSampleZ0,
                            int cols = kCaveSampleCols) const {
        if (cols % kChunkSize != 0 || cols < kChunkSize) {
            return MC_FAIL(std::string("probe sample cols must be a "
                                       "multiple of the chunk size, got ") +
                           std::to_string(cols));
        }
        WorldgenParams p = m_params;
        p.caveNoiseScale = cs;
        p.caveTubeThreshold = t;
        auto gen = WorldGenerator::create(seed, p, m_biomes, m_blocks);
        if (!gen.ok()) {
            return MC_FAIL(gen.error().message);
        }
        const CaveSample sample = buildCaveSample(*gen, p, x0, z0, cols, cols);
        return caveStats(sample);
    }

    // The SHIPPED cave tuning (worldgen.json as it stands).
    const WorldgenParams& params() const { return m_params; }

private:
    AssetManager m_assets;
    BlockRegistry m_blocks;
    BiomeRegistry m_biomes;
    WorldgenParams m_params;
};

} // namespace mc
