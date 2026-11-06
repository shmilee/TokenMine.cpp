// The End's worldgen (V3-10a, the nether_gen's second consumption —
// the dimension's fill as a coordinate-pure function: zero noise,
// constructionally deterministic, the chunk-order-independence the
// streaming jobs' arbitrary scheduling relies on).
//
// The 10a minimal form (the 10b content family — the plains / the
// bedrock ocean / the island archipelago — replaces this fill): the
// CENTRAL ISLAND, a 120x120 endstone plateau (x, z in [-60, 60],
// y in [48, 52], the surface top 52 / feet 53) + the EXIT PORTAL at
// the world origin (the 5x5 frame ring — all-eyed, the active read —
// at body level y=53 around the 3x3 end_portal center, the y=52
// endstone under it the standing surface) + the VOID (air, no floor —
// the fall dies at the kill plane, the "world's edge" death line).
//
// The derived End seed: a pure function of the overworld seed (the
// netherSeed twin — the parse-time rebuild, never stored). 10a
// consumes no noise (the fill is band-constant); the seed is the
// 10b island pass's reserved stream root (the kSaltNetherBiome
// precedent: the derived seed is carried even while inert).
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "block/block_registry.hpp"
#include "core/error.hpp"
#include "world/biome.hpp"
#include "world/chunk.hpp"
#include "world/coordinates.hpp"

namespace mc {

// The End seed's dedicated salt (the kSaltNether's twin — the
// independent-salt discipline: a fixed 64-bit constant, the XOR
// derivation its own inverse shape).
inline constexpr std::int64_t kSaltEnd = 0x10D0E11D5E4D73A2LL;

// V3-10b: the End content family's dedicated salts (the kSaltNether
// family's continuation — each pass consumes the derived seed through
// its own salt, so no end feature tracks another's, and none tracks
// the overworld's / the nether's streams).
inline constexpr std::int64_t kSaltEndIsland = 0x10D0E11D5E4D73A3LL;
inline constexpr std::int64_t kSaltEndChorus = 0x10D0E11D5E4D73A4LL;
inline constexpr std::int64_t kSaltEndBiome = 0x10D0E11D5E4D73A5LL;

// V3-10b: the island archipelago's parameters (the lattice-16 roll —
// the nether ore lattice's twin spacing; every parameter a named
// constant, the unit tests' oracle).
inline constexpr int kEndIslandLattice = 16;
inline constexpr float kEndIslandProbability = 0.15f;
inline constexpr int kEndIslandRadiusMin = 5;
inline constexpr int kEndIslandRadiusMax = 12;
// The island's top surface level band (the central island's feet 53
// sightline — the fog's 8..48 range sees the near islands, the recon's
// R7 read).
inline constexpr int kEndIslandTopMin = 44;
inline constexpr int kEndIslandTopMax = 56;
// The island's shape (the MC floating-island read): the top TWO layers
// at the full radius + the bottom cone's two layers (r-1, r-2).
inline constexpr int kEndIslandTopLayers = 2;
inline constexpr int kEndIslandConeLayers = 2;
// The bedrock ocean's band (the nether floor's mirror — the whole
// dimension's floor, the MC end's bedrock sea's single-band read; the
// 10a's void's escape surface, the pearl's landing belt).
inline constexpr int kEndOceanBottom = 0;
inline constexpr int kEndOceanTop = 4;
// The chorus decoration's per-island roll (the island top center, the
// MC's "chorus on the island's top" read).
inline constexpr float kEndChorusProbability = 0.6f;

// The derived End seed (the netherSeed twin).
std::int64_t endSeed(std::int64_t worldSeed);

// The island's bounds (the named-constant discipline — the band edges
// the unit tests pin): the plateau spans x, z in [-kEndIslandHalf,
// +kEndIslandHalf] and y in [kEndIslandBottom, kEndIslandTop] (5 layers,
// the surface top = kEndIslandTop + 1 = 53 feet). The exit portal's
// frame / portal cells sit at kEndExitLevel (= 53, the BODY level —
// the player standing on the y=52 endstone has the portal cell around
// its lower body, the overlap the entry trigger reads).
inline constexpr int kEndIslandHalf = 60;
inline constexpr int kEndIslandBottom = 48;
inline constexpr int kEndIslandTop = 52;
inline constexpr int kEndExitLevel = 53;

// V3-10b: the End's biome (the NetherBiome's minimum shape — 10b ships
// a SINGLE biome, end_plains: the surface's override source + the
// 10d's second-biome seam. The weight / cumulative selection is the
// netherBiomeAt's shape, inert at one member).
struct EndBiome {
    std::string name;
    int weight = 1;
    BlockID surface = BlockID::invalid();
};

// The End's fill content (10a: the framework's minimum standable
// surface + the exit portal; 10b: the bedrock ocean + the island
// archipelago + the chorus decoration — every id must be stateless,
// the NetherGen::create's ENT-2 twin: worldgen never writes a
// per-instance state).
struct EndGen {
    BlockID endstone = BlockID::invalid();
    BlockID obsidian = BlockID::invalid(); // the exit frame's base (REUSED)
    BlockID endPortal = BlockID::invalid();
    BlockID frameEyed = BlockID::invalid(); // the exit frame (all-eyed)
    // V3-10b: the content family (the pass's placement sources — a
    // missing one degrades its pass to inert, the band fill runs on,
    // the NetherGen's graceful-degradation twin).
    BlockID bedrock = BlockID::invalid(); // the REUSED shipped block
    BlockID chorusPlant = BlockID::invalid();
    // V3-10b: the end biome pool (the biomes_end/ load — registry order
    // = the cumulative-weight selection order; 10b ships one member,
    // the 10d seam).
    std::vector<EndBiome> biomes;
    int totalWeight = 0;
    // The derived seed (the content passes' stream root).
    std::int64_t seed = 0;

    // The buildable predicate (the island's floor + the exit resolve —
    // the content ids are NOT part of it: a stripped asset set degrades
    // the passes, the plateau still builds, the NetherGen's twin).
    bool valid() const {
        return endstone.valid() && obsidian.valid() && endPortal.valid() &&
               frameEyed.valid();
    }

    // Resolves the fill's content against the block registry + the
    // biome pool (the NetherGen::create's second-consumption shape —
    // the fail-fast timing: a missing name rejects STARTUP, not the
    // first travel). The six 10b names: the four 10a minimums +
    // bedrock + chorus_plant; every biome's surface against the
    // registry (stateful rejected ENT-2 style). An EMPTY biome pool is
    // a reject (the plateau's surface source — the loadDirectory's
    // fail-fast twin, the S6 gate's subject).
    static Result<EndGen> create(std::int64_t derivedSeed,
                                 const BiomeRegistry& biomes,
                                 const BlockRegistry& blocks);
};

// V3-10b: the end biome selection (the netherBiomeAt's clone over the
// end pool — the low-frequency noise mapped onto the cumulative weight
// in registry load order; one member is the constant selection, the
// 10d's second biome's seam).
std::size_t endBiomeAt(const EndGen& gen, int x, int z);

// V3-10b: the island's roll (the lattice cell's pure decision — the
// nether stem/basalt lattice's twin shape): the probability gate on
// the dedicated salt, the radius / the top from the hash's bits.
// false = no island at this lattice cell.
struct EndIsland {
    int cx = 0; // the island center (the lattice cell's origin)
    int cz = 0;
    int radius = 0;
    int top = 0; // the top layer's level
};
bool endIslandAt(int latticeX, int latticeZ, std::uint64_t salt,
                 EndIsland& out);
// The chorus's per-island roll (the dedicated salt — a true island
// carries the plant at its top center when the roll passes).
bool endChorusAt(int latticeX, int latticeZ, std::uint64_t salt);

// Fills a freshly created End chunk (the World::ChunkGenerator
// contract: write into the chunk only, never call back into a World).
// A pure function of (gen, pos) — two fills of the same position
// produce identical chunks. The pass order is fixed (island plateau ->
// the exit portal at the origin) and every predicate is a function of
// the coordinates alone (no neighbor-chunk read — the chunk-
// order-independence the streaming jobs rely on).
void fillEndChunk(const EndGen& gen, Chunk& chunk, const ChunkPos& pos);

} // namespace mc
