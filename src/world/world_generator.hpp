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
//   2. cave carving  (GEN-1 spaghetti tubes: the intersection of two
//                     independent 3D noise mid-bands, |n - 0.5| <
//                     tubeThreshold on BOTH fields; surface_guard 0 lets
//                     openings reach the surface, the spawn-clear zone and
//                     the lava-lake exclusion boxes are never carved)
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

// Distinct salts keep the noise fields of independent features (height,
// biome, caves, ores, trees) uncorrelated, so e.g. cave shape never tracks
// the terrain height. Public (not file-local) so the unit tests can pin
// the generated cells to the exact noise predicate. kSaltCave2 (GEN-1) is
// the SECOND cave field: independent of kSaltCave, so the tube predicate
// (the intersection of the two mid-bands) is a decorrelated, winding shape.
inline constexpr std::int64_t kSaltHeight = 0x1A2B3C4D5E6F7A01LL;
inline constexpr std::int64_t kSaltBiome = 0x1A2B3C4D5E6F7A02LL;
inline constexpr std::int64_t kSaltCave = 0x1A2B3C4D5E6F7A03LL;
inline constexpr std::int64_t kSaltOre = 0x1A2B3C4D5E6F7A04LL;
inline constexpr std::int64_t kSaltTree = 0x1A2B3C4D5E6F7A05LL;
// Phase 45: surface decorations (independent of the tree rolls).
inline constexpr std::int64_t kSaltDecoration = 0x1A2B3C4D5E6F7A06LL;
// Phase 93: procedural lava lakes (independent of the other feature rolls).
inline constexpr std::int64_t kSaltLavaLake = 0x1A2B3C4D5E6F7A07LL;
// GEN-1: the second spaghetti-cave noise field.
inline constexpr std::int64_t kSaltCave2 = 0x1A2B3C4D5E6F7A08LL;

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
    // GEN-1: a lava lake resolved at create() time: the lake's parameters +
    // the basin floor Y (the region's lowest terrain minus depth — a pure
    // heightAt read, identical across the chunks the region spans) + the
    // region box. Precomputed once so carveCaves' exclusion check is O(1)
    // per cell and placeLavaLakes does NOT rescan the region's heights per
    // chunk (the floor value is the same it always recomputed).
    struct ResolvedLake {
        WorldgenParams::LavaLake lake;
        int x0 = 0, x1 = -1, z0 = 0, z1 = -1; // the region box (inclusive)
        int floorY = 0;
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
    // Phase 86 (single lake) / Phase 88 (multi-lake list): each configured
    // lava lake is a contained pool carved into the terrain at a fixed
    // location. The basin floor is the region's lowest terrain minus `depth`
    // (a pure heightAt read — deterministic); the terrain above the floor in
    // the region is carved to air and the lowest `fill` levels are poured
    // full lava. The surrounding terrain (outside the region) is >= the region
    // min, so the basin walls always rise above the lava (contained). This
    // loops over every enabled lake in WorldgenParams::lavaLakes. Phase 93:
    // it ALSO places the procedural lakes (m_proceduralLakes, computed in
    // the constructor from the seed — deterministic pseudo-random positions
    // within the configured region). GEN-1: the region box + floor are
    // precomputed (m_lakes, computeLakes) — the per-chunk height rescan is
    // gone (it always recomputed the same pure value).
    void placeLavaLakes(Chunk& chunk, const ChunkPos& pos) const;
    void placeIfAir(Chunk& chunk, const ChunkPos& pos, int wx, int wy, int wz,
                    BlockID id) const;
    // Phase 93: derive the procedural lakes (m_proceduralLakes) from the
    // seed + WorldgenParams::lavaLakesProcedural. Called once in the
    // constructor (a pure function of the seed, so the lakes are the same
    // across runs + chunks).
    void computeProceduralLakes();
    // GEN-1: resolve every enabled lake (the fixed list + the procedural
    // ones) into m_lakes — the region box + the basin floorY (the region's
    // minimum heightAt minus depth). Called once in the constructor, AFTER
    // computeProceduralLakes. carveCaves excludes the boxes above the floor
    // (the basin owns its carve); placeLavaLakes reads the precomputed
    // floor instead of rescanning the region per chunk.
    void computeLakes();

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

    // Phase 93: the procedural lakes (computed in the constructor from the
    // seed + WorldgenParams::lavaLakesProcedural). placeLavaLakes places them
    // in addition to the fixed list (m_params.lavaLakes).
    std::vector<WorldgenParams::LavaLake> m_proceduralLakes;
    // GEN-1: every enabled lake resolved (box + floor) at create() time —
    // the exclusion source for carveCaves and the floor source for
    // placeLavaLakes.
    std::vector<ResolvedLake> m_lakes;
};

} // namespace mc
