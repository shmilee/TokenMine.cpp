// V3-1 Phase B (v3.4.0): the nether's world generation — the dedicated
// pure chunk fill (the design's D4: NO biome / ore / cave / tree
// machinery — the Phase B nether is a flat slab world, a PURE FUNCTION
// OF ITS COORDINATES = zero noise, zero randomness, constructive
// determinism; the biome/ore/cave/tree systems stay overworld-only, the
// Phase C content family extends the nether through its own passes).
//
// The layer bands (Minecraft's nether geometry in the project's
// kWorldHeight 128 space — the recon §二.3 wiki-verified set):
//   y ∈ [0, 4]      bedrock (the floor — 5 layers, the overworld's GEN-7
//                   floor's solid-band twin; destroyable false)
//   y ∈ [5, 31]     the LAVA SEA: air cells holding FULL SOURCE lava
//                   (level 8 — a source sea is STATIC under the fluid
//                   simulation: no level gradient, no spread proposal,
//                   zero churn, the recon's "static zero churn" decision
//                   that dissolves the risk surface)
//   y ∈ [32, 122]   netherrack (the solid slab: the terrain surface tops
//                   at y = 122, the portal's landing band)
//   y ∈ [123, 127]  bedrock (the roof — 5 layers, Minecraft's nether
//                   ceiling)
//
// The derived seed: the nether's generation reference is DERIVED from
// the overworld seed (`worldSeed ^ kSaltNether` — the independent-salt
// discipline, the kSaltWeatherThunder / kXpSeedInit precedent). The fill
// itself is coordinate-pure (no noise consumes the seed), so the
// derived seed is the generator's IDENTITY: the save-diff reference and
// the parse-time rebuild read it as a pure function of the stored
// overworld seed (never stored on disk — the xp_seed deterministic
// migration's single-source-of-truth twin).
//
// The Game binds a World::ChunkGenerator callback to
// makeNetherChunkGenerator (the makeChunkGenerator twin): streaming
// generation jobs, first-touch creation, the save diff reference and the
// unload retained diffs all run the same pure fill.
#pragma once

#include <cstdint>

#include "world/chunk.hpp"
#include "world/coordinates.hpp"

namespace mc {

// The nether seed's dedicated salt (the independent-salt discipline — a
// fixed 64-bit constant; the overworld's feature salts live in
// world_generator.hpp, the salt is a NAME, never a literal at the call
// site). XOR (not addition) so the derivation is its own inverse shape
// (the kXpSeedInit's `^` precedent).
inline constexpr std::int64_t kSaltNether = 0x5E73E74E92B1D3A1LL;

// The derived nether seed: a pure function of the overworld seed (the
// parse-time rebuild — a load derives it from the stored "seed" key).
std::int64_t netherSeed(std::int64_t worldSeed);

// The nether fill's resolved content (init name lookup against the block
// registry — the campfireLitId pattern: invalid = the fill refuses to
// write that band, and the nether dimension is structurally unbuildable,
// the P70 family-inert precedent). The names are the data vocabulary
// (hard rule 3: no ids at this layer — the World holds no registry).
struct NetherGen {
    BlockID netherrack = BlockID::invalid();
    BlockID bedrock = BlockID::invalid();

    // True when both bands resolve (the buildable predicate).
    bool valid() const {
        return netherrack.valid() && bedrock.valid();
    }
};

// The nether's layer band bounds (the named-constant discipline — the
// band edges the unit tests pin): the floor bedrock tops out at
// kNetherFloorBedrockTop, the lava sea spans [kNetherLavaSeaBottom,
// kNetherLavaSeaTop], the netherrack slab spans [kNetherRockBottom,
// kNetherRockTop], the roof bedrock spans [kNetherRoofBedrockBottom,
// kNetherRoofBedrockTop] (= kWorldHeight - 1).
inline constexpr int kNetherFloorBedrockBottom = 0;
inline constexpr int kNetherFloorBedrockTop = 4;
inline constexpr int kNetherLavaSeaBottom = 5;
inline constexpr int kNetherLavaSeaTop = 31;
inline constexpr int kNetherRockBottom = 32;
inline constexpr int kNetherRockTop = 122;
inline constexpr int kNetherRoofBedrockBottom = 123;
inline constexpr int kNetherRoofBedrockTop = kWorldHeight - 1; // 127

// The lava sea's source level (kMaxFluidLevel — the full source: the
// static-sea decision).
inline constexpr std::uint8_t kNetherLavaSeaLevel = kMaxFluidLevel;

// Fills a freshly created nether chunk (the World::ChunkGenerator
// contract: write into the chunk only, never call back into a World).
// A pure function of (gen, pos) — two fills of the same position produce
// identical chunks (the WorldGenerator's stateless-determinism contract,
// the thread-safety the streaming generation jobs rely on). `gen` must be
// valid(); an invalid band id leaves that band air / dry (the build
// refuses the nether before any fill runs — the belt over the init
// cross-check).
void fillNetherChunk(const NetherGen& gen, Chunk& chunk, const ChunkPos& pos);

} // namespace mc
