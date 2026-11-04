// V3-1 Phase B (v3.4.0): the nether's world generation — the dedicated
// pure chunk fill (a PURE FUNCTION OF ITS COORDINATES = zero noise, zero
// randomness, constructive determinism; the biome/ore/cave/tree systems
// stay overworld-only, the Phase C content family extends the nether
// through its own passes).
//
// The layer bands (Minecraft's nether geometry in the project's
// kWorldHeight 128 space — the recon §二.3 wiki-verified set; the v3.5.0
// band revision: the solid slab became a 21-layer PLATEAU with a 70-cell
// NAVIGABLE AIR CAVITY above it — the orchestrator's "the nether must be
// flyable" ruling: the portal landings (typical mapped Y ~ 55) sit in the
// cavity, the terrain surface is the plateau's TOP, the ceiling's
// glowstone lights the open space):
//   y ∈ [0, 4]      bedrock (the floor — 5 layers, the overworld's GEN-7
//                   floor's solid-band twin; destroyable false)
//   y ∈ [5, 31]     the LAVA SEA: air cells holding FULL SOURCE lava
//                   (level 8 — a source sea is STATIC under the fluid
//                   simulation: no level gradient, no spread proposal,
//                   zero churn, the recon's "static zero churn" decision
//                   that dissolves the risk surface; below the plateau =
//                   visible when dug through, the Minecraft twin)
//   y ∈ [32, 52]    the NETHERRACK PLATEAU (21 layers — the solid terrain
//                   the ores ride, the biome surfaces top)
//   y ∈ [53, 122]   the AIR CAVITY (70 cells of open, navigable space —
//                   the stems / the basalt columns grow up into it, the
//                   portals float in it, the ceiling's glowstone hangs
//                   from its top row)
//   y ∈ [123, 127]  bedrock (the roof — 5 layers, Minecraft's nether
//                   ceiling)
//
// V3-2 Part 1 (v3.5.0): the content family on the plateau + the cavity —
// four further passes, ALL of them pure functions of (the derived seed,
// the coordinates): chunk-order-independent by construction (no pass
// reads a neighbor chunk — every predicate is a function of the chunk's
// own cells after the earlier passes, which are themselves coordinate-
// pure), the world assembles seamlessly with no cross-chunk bookkeeping
// (the WorldGenerator's lattice-walk contract, the slab's form):
//   1. biome surface  (per-column): netherBiomeAt picks the biome from
//      the INDEPENDENT nether pool (assets/data/biomes_nether — the
//      overworld's biome directory stays untouched), the column's band
//      (kNetherRockTop - surfaceDepth + 1, kNetherRockTop] — the plateau
//      TOP's downward depth) is overridden with the biome's surface block
//      (a netherrack surface = the structural no-op — the wastes / basalt
//      deltas' "depth 0" form, the shared registry's [1, 16] depth range
//      kept intact)
//   2. nether ores    (per lattice-8 cell, the overworld GEN-3's
//      single-cell form — the vein_size 1 precedent): a per-ore roll on
//      the lattice cell's salt stream (the kFeatureStep decorrelation),
//      the in-cell uniform point replaced ONLY when the cell still holds
//      the netherrack baseline (the lava sea / the bedrock / the surface
//      override never carry an ore — the netherrack-only predicate), the
//      band = the plateau
//   3. decorations    (per-column lattice features + per-cell rolls):
//      the forest biomes' stems (the column-aligned lattice-4 straight
//      column, the biome's tree_density as the per-lattice-column
//      probability, grown UP from the plateau's top into the cavity) /
//      the basalt deltas' columns (the lattice-8 column from the plateau
//     's top) / the soul sand valley's soul_soil + magma_block per-cell
//      rolls (the plateau-top band — they are blobs IN the plateau's top
//      layers) / the ceiling glowstone per-cell roll at kNetherAirTop
//      (all biomes — the cavity's top row, the roof's underside)
//   4. pass order     = surface -> ores -> decorations (a later pass may
//      overwrite an earlier pass's cell — the decorations' per-cell rolls
//      replace surface / ore / air cells, the deterministic-order pin)
//
// The derived seed: the nether's generation reference is DERIVED from
// the overworld seed (`worldSeed ^ kSaltNether` — the independent-salt
// discipline, the kSaltWeatherThunder / kXpSeedInit precedent). The
// content passes consume the derived seed through their OWN dedicated
// salts (the kSaltNetherBiome / kSaltNetherOre / ... family — a feature
// never shares the overworld's kSaltBiome / kSaltOre stream: the two
// dimensions' worldgen is uncorrelated by construction, the salt
// discipline's dimension form). The seed is never stored on disk — the
// parse-time rebuild derives it from the stored overworld seed (the
// xp_seed deterministic migration's single-source-of-truth twin).
//
// The Game binds a World::ChunkGenerator callback to
// makeNetherChunkGenerator (the makeChunkGenerator twin): streaming
// generation jobs, first-touch creation, the save diff reference and the
// unload retained diffs all run the same pure fill.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "block/block_registry.hpp"
#include "world/biome.hpp"
#include "world/chunk.hpp"
#include "world/coordinates.hpp"
#include "world/noise.hpp"

namespace mc {

// The nether seed's dedicated salt (the independent-salt discipline — a
// fixed 64-bit constant; the overworld's feature salts live in
// world_generator.hpp, the salt is a NAME, never a literal at the call
// site). XOR (not addition) so the derivation is its own inverse shape
// (the kXpSeedInit's `^` precedent).
inline constexpr std::int64_t kSaltNether = 0x5E73E74E92B1D3A1LL;

// V3-2 Part 1: the nether content family's dedicated salts (the kSaltNether
// family's continuation — each pass consumes the derived seed through its
// own salt, so no nether feature tracks another's, and none tracks the
// overworld's kSaltBiome / kSaltOre / kSaltTree streams).
inline constexpr std::int64_t kSaltNetherBiome = 0x5E73E74E92B1D3A2LL;
inline constexpr std::int64_t kSaltNetherOre = 0x5E73E74E92B1D3A3LL;
inline constexpr std::int64_t kSaltNetherStem = 0x5E73E74E92B1D3A4LL;
inline constexpr std::int64_t kSaltNetherBasalt = 0x5E73E74E92B1D3A5LL;
inline constexpr std::int64_t kSaltNetherSoulSoil = 0x5E73E74E92B1D3A6LL;
inline constexpr std::int64_t kSaltNetherMagma = 0x5E73E74E92B1D3A7LL;
inline constexpr std::int64_t kSaltNetherGlowstone = 0x5E73E74E92B1D3A8LL;

// The derived nether seed: a pure function of the overworld seed (the
// parse-time rebuild — a load derives it from the stored "seed" key).
std::int64_t netherSeed(std::int64_t worldSeed);

// The nether's layer band bounds (the named-constant discipline — the
// band edges the unit tests pin): the floor bedrock tops out at
// kNetherFloorBedrockTop, the lava sea spans [kNetherLavaSeaBottom,
// kNetherLavaSeaTop], the netherrack PLATEAU spans [kNetherRockBottom,
// kNetherRockTop], the AIR CAVITY spans [kNetherAirBottom, kNetherAirTop]
// (the v3.5.0 band revision — the solid slab's upper 70 cells become the
// navigable open space the landings / the columns / the ceiling's
// glowstone live in), the roof bedrock spans [kNetherRoofBedrockBottom,
// kNetherRoofBedrockTop] (= kWorldHeight - 1).
inline constexpr int kNetherFloorBedrockBottom = 0;
inline constexpr int kNetherFloorBedrockTop = 4;
inline constexpr int kNetherLavaSeaBottom = 5;
inline constexpr int kNetherLavaSeaTop = 31;
inline constexpr int kNetherRockBottom = 32;
inline constexpr int kNetherRockTop = 52; // the plateau's top (was 122)
inline constexpr int kNetherAirBottom = 53; // the cavity's bottom (new)
inline constexpr int kNetherAirTop = 122; // the cavity's top (new)
inline constexpr int kNetherRoofBedrockBottom = 123;
inline constexpr int kNetherRoofBedrockTop = kWorldHeight - 1; // 127

// The lava sea's source level (kMaxFluidLevel — the full source: the
// static-sea decision).
inline constexpr std::uint8_t kNetherLavaSeaLevel = kMaxFluidLevel;

// V3-2 Part 1: the biome selection's noise scale — the overworld's
// biomeNoiseScale shipped value (worldgen.json's biomes.noise_scale 0.02):
// same-order biome regions in both dimensions (the nether biomes are the
// overworld biome size, NOT Minecraft's much larger nether regions — the
// flat-slab world's view scale, the documented simplification).
inline constexpr float kNetherBiomeNoiseScale = 0.02f;

// V3-2 Part 1: the content pass's named constants (the lattice /
// probability / height vocabulary — the worldgen.json ore lattice's code
// twin: the nether's worldgen parameters live here, in the nether module,
// because the nether has no worldgen.json section (the data vocabulary is
// the biome pool + the block registry; the part-2 config-ization keeps
// these as the defaults).
//
// The ore pass (per lattice cell — the overworld GEN-3's vein_size 1
// single-cell form):
inline constexpr int kNetherOreLattice = 8;
inline constexpr float kNetherQuartzOreProbability = 0.0376f; // MC exact
inline constexpr float kNetherGoldOreProbability = 0.0208f; // MC exact
inline constexpr float kNetherAncientDebrisProbability = 0.0026f;
// The ore band: the WHOLE netherrack plateau (the v3.5.0 band revision —
// the ores ride the solid terrain [32, 52]; the air cavity above carries
// none).
inline constexpr int kNetherOreBandBottom = kNetherRockBottom;
inline constexpr int kNetherOreBandTop = kNetherRockTop;
//
// The decoration pass:
inline constexpr int kNetherStemLattice = 4; // the column-aligned stem
inline constexpr int kNetherStemHeightMin = 4;
inline constexpr int kNetherStemHeightMax = 9;
inline constexpr int kNetherBasaltLattice = 8; // the column-aligned basalt
inline constexpr int kNetherBasaltHeightMin = 5;
inline constexpr int kNetherBasaltHeightMax = 30;
// The stem / the basalt columns' growth base (the plateau's top + 1 =
// the cavity's bottom — they grow UP into the air, visible from the
// landings).
inline constexpr int kNetherColumnBaseY = kNetherAirBottom; // 53
// The per-cell blob rolls (the soul sand valley's signature features —
// the plateau's TOP band, the v3.5.0 revision):
inline constexpr float kNetherSoulSoilProbability = 0.05f;
inline constexpr int kNetherSoulSoilBandBottom = 48;
inline constexpr int kNetherSoulSoilBandTop = kNetherRockTop; // 52
inline constexpr float kNetherMagmaProbability = 0.01f;
inline constexpr int kNetherMagmaBandBottom = 48;
inline constexpr int kNetherMagmaBandTop = kNetherRockTop; // 52
// The ceiling glowstone (the REUSED shipped glowstone block, light 15)
// at the cavity's top row (the roof's underside — the visible ceiling):
inline constexpr float kNetherGlowstoneProbability = 0.04f;
inline constexpr int kNetherGlowstoneY = kNetherAirTop; // 122

// V3-2 Part 1: a nether biome with its feature content RESOLVED to block
// ids (init name lookup against the block registry — the WorldGenerator's
// ResolvedBiome pattern). The feature flags are derived from the biome's
// OWN data at resolution (no name switch at fill time — the core reads
// the resolved ids / flags, hard rule 3): the forest biomes (a nylium
// surface + a column probability) carry their stem block, the
// basalt-delta biome (a netherrack surface + a column probability) the
// basalt flag, the soul-sand-valley biome (the soul_sand surface) the
// soul-family flag.
struct NetherBiome {
    std::string name;
    int weight = 1; // relative selection weight (>= 1)
    int surfaceDepth = 0; // layers DOWN from kNetherRockTop (the plateau's
                          // top band — the v3.5.0 surface direction)
    float columnProbability = 0.0f; // the biome's tree_density: the
                                    // per-lattice-column feature roll
    BlockID surface = BlockID::invalid(); // the surface block (netherrack
                                          // = the baseline no-op)
    BlockID stem = BlockID::invalid(); // the forest biome's stem block
    bool basaltColumns = false;    // the basalt column pass (lattice 8)
    bool soulFamily = false;       // the soul_soil / magma_block per-cell
                                   // rolls (the soul_sand surface biomes)
};

// The nether fill's resolved content (init name lookup against the block
// registry — the campfireLitId pattern: invalid = the fill refuses to
// write that band, and the nether dimension is structurally unbuildable,
// the P70 family-inert precedent). The names are the data vocabulary
// (hard rule 3: no ids at this layer — the World holds no registry).
struct NetherGen {
    // The v3.4.0 baseline bands (the buildable predicate — the content
    // family degrades gracefully: a content id left invalid (a partial
    // test fixture) makes its own pass inert, the band fill runs on).
    BlockID netherrack = BlockID::invalid();
    BlockID bedrock = BlockID::invalid();
    // V3-2 Part 1: the content blocks (the pass's placement sources —
    // every one of them must be stateless, NetherGen::create's ENT-2
    // twin: worldgen never writes a per-instance state).
    BlockID soulSand = BlockID::invalid();
    BlockID soulSoil = BlockID::invalid();
    BlockID crimsonNylium = BlockID::invalid();
    BlockID warpedNylium = BlockID::invalid();
    BlockID basalt = BlockID::invalid();
    BlockID crimsonStem = BlockID::invalid();
    BlockID warpedStem = BlockID::invalid();
    BlockID quartzOre = BlockID::invalid();
    BlockID netherGoldOre = BlockID::invalid();
    BlockID ancientDebris = BlockID::invalid();
    BlockID magmaBlock = BlockID::invalid();
    BlockID glowstone = BlockID::invalid(); // the REUSED shipped block
    // V3-2 Part 1: the independent nether biome pool (registry load order
    // = the cumulative-weight selection order, the overworld's twin) +
    // the derived seed (the pass's salt-stream root).
    std::vector<NetherBiome> biomes;
    int totalWeight = 0;
    std::int64_t seed = 0;

    // True when the baseline bands resolve (the buildable predicate —
    // the content ids are NOT part of it: a stripped asset set degrades
    // the passes, the slab still builds).
    bool valid() const {
        return netherrack.valid() && bedrock.valid();
    }

    // Resolves the full content (the 14 nether block names — the nether
    // module's data vocabulary, the WorldGenerator::create pattern) + the
    // biome pool (each surface name against the block registry, the
    // stateful-placement source rejected ENT-2 style, the feature flags
    // derived from the biome's own data). Fails with a descriptive error
    // naming the offending definition when a name is missing.
    static Result<NetherGen> create(std::int64_t derivedSeed,
                                    const BiomeRegistry& biomes,
                                    const BlockRegistry& blocks);
};

// V3-2 Part 1: the biome owning the column at (x, z) — the
// WorldGenerator::biomeIndexAt clone over the nether pool: the low-
// frequency valueNoise2 at the column (the kNetherBiomeNoiseScale scale,
// the kSaltNetherBiome-dedicated salt on the derived seed) mapped onto
// the cumulative weight over the biomes in registry load order
// (deterministic). `gen.biomes` must be non-empty (an empty pool makes
// the selection calls no-ops — the caller's guard).
std::size_t netherBiomeAt(const NetherGen& gen, int x, int z);

// Fills a freshly created nether chunk (the World::ChunkGenerator
// contract: write into the chunk only, never call back into a World).
// A pure function of (gen, pos) — two fills of the same position produce
// identical chunks (the WorldGenerator's stateless-determinism contract,
// the thread-safety the streaming generation jobs rely on): the pass
// order is fixed (baseline bands -> biome surface -> ores -> decorations)
// and every predicate is a function of the coordinates + the chunk's own
// cells after the earlier passes (no neighbor-chunk read — the chunk-
// order-independence the streaming jobs' arbitrary scheduling relies on).
// `gen` must be valid(); an invalid band id leaves that band air / dry
// (the build refuses the nether before any fill runs — the belt over the
// init cross-check), an invalid content id / an empty biome pool leaves
// the corresponding pass inert (the family-degradation precedent).
void fillNetherChunk(const NetherGen& gen, Chunk& chunk, const ChunkPos& pos);

} // namespace mc
