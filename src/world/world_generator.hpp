// Stateless, seed-deterministic procedural world generator (Phase 4).
//
// generateChunk(chunk, pos) is a pure function of (seed, params, biomes,
// resolved block ids, pos): two generators built with the same seed produce
// identical chunks for the same position, and no state is shared between
// chunk generations. That gives us:
//   - seed reproducibility (the core Phase 4 requirement)
//   - free thread safety (Phase 14 can generate chunks in parallel)
//
// Features that can cross chunk boundaries (ore veins, trees) are placed
// from a world-space lattice: for a chunk, every lattice cell within an
// extended range (the chunk plus the feature radius) is considered, and
// only the blocks that fall inside the target chunk are written. Neighbor
// chunks compute the same features independently, so the assembled world is
// seamless with no cross-chunk bookkeeping.
//
// Generation order per chunk:
//   1. terrain fill  (y=0 bottom / deep stone / subsurface / surface;
//                     columns below the sea level are filled with the full
//                     fluid level — water lives in the chunk's fluid array,
//                     not the block array)
//   2. cave carving  (3D noise threshold, strictly below the surface)
//   3. ore veins     (replace deep stone only, so caves stay carved)
//   4. trees         (replace air only, so they never destroy terrain)
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "block/block.hpp"
#include "block/block_registry.hpp"
#include "core/error.hpp"
#include "world/biome.hpp"
#include "world/chunk.hpp"
#include "world/coordinates.hpp"
#include "world/worldgen_config.hpp"

namespace mc {

class WorldGenerator {
public:
    // Resolves every block name (biome surfaces/subsurfaces, ore and tree
    // blocks, terrain fills) against `blocks`. Fails with a descriptive
    // error naming the offending definition when a name is missing.
    static Result<WorldGenerator> create(std::int64_t seed,
                                         const WorldgenParams& params,
                                         const BiomeRegistry& biomes,
                                         const BlockRegistry& blocks);

    // Fill a freshly created chunk. Must not call back into a World (the
    // generator never creates or mutates other chunks).
    void generateChunk(Chunk& chunk, const ChunkPos& pos) const;

    // Terrain surface height (topmost terrain layer) of the column at
    // (x, z). Ignores caves (always below the surface) and trees.
    // Intended for camera / player spawning.
    int surfaceHeight(int x, int z) const;

    // Name of the biome owning the column at (x, z) (logging/tests).
    const std::string& biomeNameAt(int x, int z) const;

    std::int64_t seed() const { return m_seed; }
    const WorldgenParams& params() const { return m_params; }

private:
    // A biome decoration entry with its block resolved to an id (Phase 45).
    struct ResolvedDecoration {
        BlockID block = BlockID::invalid();
        float probability = 0.0f;
    };
    struct ResolvedBiome {
        std::string name;
        int weight = 1;
        BlockID surface = BlockID::invalid();
        BlockID subsurface = BlockID::invalid();
        int subsurfaceDepth = 3;
        float treeDensity = 0.0f;
        float heightScale = 1.0f;
        // Surface decorations (Phase 45): resolved block names -> ids.
        std::vector<ResolvedDecoration> decorations;
    };
    struct ResolvedOre {
        BlockID block = BlockID::invalid();
        float probability = 0.0f;
        int veinSize = 8;
        int maxY = 80;
    };

    WorldGenerator() = default;

    int heightAt(int x, int z) const;
    std::size_t biomeIndexAt(int x, int z) const;
    const ResolvedBiome& resolvedBiomeAt(int x, int z) const;

    void fillTerrain(Chunk& chunk, const ChunkPos& pos,
                     const std::array<int, kChunkSize * kChunkSize>& heights) const;
    void carveCaves(Chunk& chunk, const ChunkPos& pos,
                    const std::array<int, kChunkSize * kChunkSize>& heights) const;
    void placeOres(Chunk& chunk, const ChunkPos& pos) const;
    void placeOre(Chunk& chunk, const ChunkPos& pos, int wx, int wy, int wz,
                  const ResolvedOre& ore) const;
    void placeTrees(Chunk& chunk, const ChunkPos& pos) const;
    // Surface decorations (Phase 45): per-lattice-cell rolls on the tree
    // lattice, one independent roll per (cell, entry) pair; a decoration
    // lands one above the surface cell — only when that cell still holds
    // the biome's surface block (caves may have eaten it) and it is above
    // sea level. Runs AFTER placeTrees, so trunks/canopies win collisions
    // (placeIfAir never overwrites).
    void placeDecorations(Chunk& chunk, const ChunkPos& pos) const;
    void placeIfAir(Chunk& chunk, const ChunkPos& pos, int wx, int wy, int wz,
                    BlockID id) const;

    std::int64_t m_seed = 0;
    WorldgenParams m_params;
    std::vector<ResolvedBiome> m_biomes; // registry load order
    int m_totalWeight = 0;
    std::vector<ResolvedOre> m_ores;

    // Resolved terrain fill blocks (data-driven names, see WorldgenParams).
    BlockID m_bottom = BlockID::invalid();
    BlockID m_deep = BlockID::invalid();
    BlockID m_trunk = BlockID::invalid();
    BlockID m_leaves = BlockID::invalid();
};

} // namespace mc
