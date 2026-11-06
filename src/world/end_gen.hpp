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

#include <cstdint>

#include "world/chunk.hpp"
#include "world/coordinates.hpp"
#include "block/block_registry.hpp"
#include "core/error.hpp"

namespace mc {

// The End seed's dedicated salt (the kSaltNether's twin — the
// independent-salt discipline: a fixed 64-bit constant, the XOR
// derivation its own inverse shape).
inline constexpr std::int64_t kSaltEnd = 0x10D0E11D5E4D73A2LL;

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

// The End's fill content (10a: the framework's minimum standable
// surface + the exit portal — every id must be stateless, the
// NetherGen::create's ENT-2 twin: worldgen never writes a per-instance
// state).
struct EndGen {
    BlockID endstone = BlockID::invalid();
    BlockID obsidian = BlockID::invalid(); // the exit frame's base (REUSED)
    BlockID endPortal = BlockID::invalid();
    BlockID frameEyed = BlockID::invalid(); // the exit frame (all-eyed)
    // The derived seed (the 10b island pass's reserved stream root).
    std::int64_t seed = 0;

    // The buildable predicate (the island's floor + the exit resolve).
    bool valid() const {
        return endstone.valid() && obsidian.valid() && endPortal.valid() &&
               frameEyed.valid();
    }

    // Resolves the fill's content against the block registry (the
    // WorldGenerator::create / NetherGen::create pattern — the
    // fail-fast timing: a missing name rejects STARTUP, not the first
    // travel). The four 10a names: endstone / obsidian / end_portal /
    // end_portal_frame_eyed.
    static Result<EndGen> create(std::int64_t derivedSeed,
                                 const BlockRegistry& blocks);
};

// Fills a freshly created End chunk (the World::ChunkGenerator
// contract: write into the chunk only, never call back into a World).
// A pure function of (gen, pos) — two fills of the same position
// produce identical chunks. The pass order is fixed (island plateau ->
// the exit portal at the origin) and every predicate is a function of
// the coordinates alone (no neighbor-chunk read — the chunk-
// order-independence the streaming jobs rely on).
void fillEndChunk(const EndGen& gen, Chunk& chunk, const ChunkPos& pos);

} // namespace mc
