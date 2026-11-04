// V3-1 Phase B (v3.4.0): the nether's pure chunk fill (see nether_gen.hpp
// for the layer bands + the derived-seed discipline).
//
// V3-2 Part 1 (v3.5.0): the content family — the four passes (biome
// surface -> ores -> decorations, over the v3.4.0 baseline bands), every
// one a pure function of (the derived seed, the coordinates): the
// chunk-order-independence + the cross-chunk continuity hold by
// construction (the WorldGenerator's lattice-walk contract, the slab's
// form — no pass reads a neighbor chunk).
#include "world/nether_gen.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>

#include "world/world_generator.hpp" // kFeatureStep + kLattice24Scale (the
                                     // overworld lattice-roll vocabulary)

namespace mc {

std::int64_t netherSeed(std::int64_t worldSeed) {
    return worldSeed ^ kSaltNether;
}

namespace {

// Reinterpret an (possibly negative) int / lattice index as 64 bits for
// hashing. Two's-complement reinterpretation is a bijection, so this is
// a valid deterministic mapping for negative coordinates (the
// world_generator.cpp's u64 twin, the floorDiv discipline's hash form).
std::uint64_t u64(int v) {
    return static_cast<std::uint64_t>(static_cast<std::int64_t>(v));
}

// Is the world cell inside the chunk (the world_generator.cpp's
// insideChunk twin — the per-pass write gate).
bool cellInChunk(const ChunkPos& pos, int wx, int wy, int wz) {
    if (wy < 0 || wy >= kWorldHeight) {
        return false;
    }
    return worldToChunk(BlockPos{wx, wy, wz}) == pos;
}

// The 24-bit lattice / per-cell roll -> [0, 1) probability value (the
// world_generator.cpp's `h >> 40 * kLattice24Scale` gate, the overworld
// ore / tree / decoration rolls' shared form).
bool rollBelow(std::uint64_t h, float probability) {
    return static_cast<float>(h >> 40) * kLattice24Scale < probability;
}

} // namespace

Result<NetherGen> NetherGen::create(std::int64_t derivedSeed,
                                    const BiomeRegistry& biomes,
                                    const BlockRegistry& blocks) {
    NetherGen gen;
    gen.seed = derivedSeed;
    // The 14 nether block names (the nether module's data vocabulary —
    // the WorldGenerator::create's "worldgen <what>: unknown block"
    // naming discipline).
    const struct {
        const char* name;
        BlockID* out;
    } content[] = {
        {"netherrack", &gen.netherrack},
        {"bedrock", &gen.bedrock},
        {"soul_sand", &gen.soulSand},
        {"soul_soil", &gen.soulSoil},
        {"crimson_nylium", &gen.crimsonNylium},
        {"warped_nylium", &gen.warpedNylium},
        {"basalt", &gen.basalt},
        {"crimson_stem", &gen.crimsonStem},
        {"warped_stem", &gen.warpedStem},
        {"quartz_ore", &gen.quartzOre},
        {"nether_gold_ore", &gen.netherGoldOre},
        {"ancient_debris", &gen.ancientDebris},
        {"magma_block", &gen.magmaBlock},
        {"glowstone", &gen.glowstone},
    };
    // ENT-2 twin: the nether's worldgen places NO stateful block (the
    // save's state derivation's completeness premise — every stateful
    // nether cell, the player's portals, is block-edited this session).
    for (const auto& c : content) {
        *c.out = blocks.idForName(c.name);
        if (!c.out->valid()) {
            return MC_FAIL("nether content: unknown block '" +
                           std::string(c.name) + "'");
        }
        if (blocks.at(*c.out).hasState()) {
            return MC_FAIL("nether content block '" + std::string(c.name) +
                           "' has a per-instance state — worldgen never "
                           "writes state (the game's setBlock choke point "
                           "is the only state writer)");
        }
    }
    for (const Biome& b : biomes.all()) {
        NetherBiome rb;
        rb.name = b.name;
        rb.weight = b.weight;
        rb.surfaceDepth = b.subsurfaceDepth;
        rb.columnProbability = b.treeDensity;
        rb.surface = blocks.idForName(b.surface);
        if (!rb.surface.valid()) {
            return MC_FAIL("nether biome '" + b.name +
                           "': unknown surface block '" + b.surface + "'");
        }
        if (blocks.at(rb.surface).hasState()) {
            return MC_FAIL("nether biome '" + b.name + "' surface block '" +
                           b.surface +
                           "' has a per-instance state — worldgen never "
                           "writes state (the game's setBlock choke point "
                           "is the only state writer)");
        }
        // The feature flags (derived from the biome's OWN resolved data —
        // the fill reads the flags, never a biome name): the forest
        // biomes are the nylium-surface ones (their stem is the nylium's
        // sibling — the data pairing), the basalt columns are the
        // netherrack-surface biome carrying a column probability, the
        // soul family is the soul_sand-surface biome.
        if (rb.surface == gen.crimsonNylium) {
            rb.stem = gen.crimsonStem;
        } else if (rb.surface == gen.warpedNylium) {
            rb.stem = gen.warpedStem;
        }
        if (rb.surface == gen.netherrack && rb.columnProbability > 0.0f) {
            rb.basaltColumns = true;
        }
        if (rb.surface == gen.soulSand) {
            rb.soulFamily = true;
        }
        gen.biomes.push_back(std::move(rb));
    }
    for (const NetherBiome& b : gen.biomes) {
        gen.totalWeight += b.weight;
    }
    return gen;
}

std::size_t netherBiomeAt(const NetherGen& gen, int x, int z) {
    // The WorldGenerator::biomeIndexAt clone over the nether pool: the
    // low-frequency noise in [0, 1) mapped onto the cumulative weight,
    // the biomes in registry load order (deterministic). The dedicated
    // kSaltNetherBiome on the DERIVED seed — the nether's selection never
    // tracks the overworld's kSaltBiome stream (the salt discipline's
    // dimension form), and the overworld's spawn-clear-zone exclusion has
    // no nether twin (the zone is an overworld demo-region feature).
    const float n = valueNoise2(
        static_cast<float>(x) * kNetherBiomeNoiseScale,
        static_cast<float>(z) * kNetherBiomeNoiseScale,
        gen.seed ^ kSaltNetherBiome);
    const int target =
        static_cast<int>(n * static_cast<float>(gen.totalWeight));
    int acc = 0;
    for (std::size_t i = 0; i < gen.biomes.size(); ++i) {
        acc += gen.biomes[i].weight;
        if (target < acc) {
            return i;
        }
    }
    // Safety fallthrough (target < totalWeight, so the walk returns first
    // for any valid selection): the last biome in load order.
    return gen.biomes.size() - 1;
}

void fillNetherChunk(const NetherGen& gen, Chunk& chunk, const ChunkPos& pos) {
    const int ox = pos.x * kChunkSize;
    const int oz = pos.z * kChunkSize;
    const int baseY = pos.y * kChunkSize;

    // -- pass 0: the baseline bands (the v3.5.0 band revision: the 21-
    // layer netherrack plateau + the 70-cell air cavity) ------------------
    for (int ly = 0; ly < kChunkSize; ++ly) {
        const int wy = baseY + ly;
        for (int lz = 0; lz < kChunkSize; ++lz) {
            for (int lx = 0; lx < kChunkSize; ++lx) {
                if (wy <= kNetherFloorBedrockTop) {
                    // The bedrock floor (y ∈ [0, 4]).
                    if (gen.bedrock.valid()) {
                        chunk.set(lx, ly, lz, gen.bedrock);
                    }
                } else if (wy <= kNetherLavaSeaTop) {
                    // The lava sea (y ∈ [5, 31]): an air cell holding a
                    // full source (level 8) — the static sea (no level
                    // gradient, no spread, zero churn under the fluid
                    // simulation — the recon's risk-surface ③
                    // resolution).
                    chunk.setFluidAt(lx, ly, lz,
                                     fluidCode(FluidKind::Lava,
                                               kNetherLavaSeaLevel));
                } else if (wy <= kNetherRockTop) {
                    // The netherrack plateau (y ∈ [32, 52]).
                    if (gen.netherrack.valid()) {
                        chunk.set(lx, ly, lz, gen.netherrack);
                    }
                } else if (wy <= kNetherAirTop) {
                    // The air cavity (y ∈ [53, 122]): left AIR (the fresh
                    // chunk's default — the navigable open space).
                } else {
                    // The bedrock roof (y ∈ [123, 127]).
                    if (gen.bedrock.valid()) {
                        chunk.set(lx, ly, lz, gen.bedrock);
                    }
                }
            }
        }
    }

    // The column's biome (computed once, shared by the surface / the
    // lattice-feature / the per-cell passes — the overworld's per-column
    // resolvedBiomeAt precedent). An empty pool leaves the whole content
    // family inert (the v3.4.0 slab — the fixture degradation form).
    const bool hasPool = !gen.biomes.empty();
    std::array<int, kChunkSize * kChunkSize> colBiome {};
    if (hasPool) {
        for (int lz = 0; lz < kChunkSize; ++lz) {
            for (int lx = 0; lx < kChunkSize; ++lx) {
                colBiome[static_cast<std::size_t>(lx) * kChunkSize +
                         static_cast<std::size_t>(lz)] =
                    static_cast<int>(netherBiomeAt(gen, ox + lx, oz + lz));
            }
        }
    }

    // -- pass 1: the biome surface (per-column, the plateau's TOP band) --
    // The column's band (kNetherRockTop - surfaceDepth + 1, kNetherRockTop]
    // is overridden with the biome's surface block (the overworld's
    // surface-layer rule, the v3.5.0 top-anchored form: the terrain's
    // surface is the plateau's top, the solid below). A netherrack
    // surface is a structural no-op (the wastes / the basalt deltas'
    // depth-0 form).
    if (hasPool && gen.netherrack.valid()) {
        for (int lz = 0; lz < kChunkSize; ++lz) {
            for (int lx = 0; lx < kChunkSize; ++lx) {
                const NetherBiome& b =
                    gen.biomes[colBiome[static_cast<std::size_t>(lx) *
                                            kChunkSize +
                                        static_cast<std::size_t>(lz)]];
                if (b.surface == gen.netherrack || !b.surface.valid()) {
                    continue;
                }
                for (int d = 0; d < b.surfaceDepth; ++d) {
                    const int wy = kNetherRockTop - d;
                    if (wy < baseY || wy >= baseY + kChunkSize) {
                        continue;
                    }
                    const int ly = wy - baseY;
                    if (chunk.get(lx, ly, lz) == gen.netherrack) {
                        chunk.set(lx, ly, lz, b.surface);
                    }
                }
            }
        }
    }

    // -- pass 2: the nether ores (per lattice-8 cell, netherrack-only) --
    // The overworld GEN-3's single-cell form (vein_size 1): a per-ore
    // roll on the lattice cell's dedicated salt stream (the kFeatureStep
    // decorrelation — an ore never tracks another's), the in-cell uniform
    // point replaced ONLY when the cell still holds the netherrack
    // baseline (the surface override / the bedrock / the lava sea never
    // carry an ore — the lava sea's cell is air + fluid, the surface's
    // is the biome block).
    if (gen.netherrack.valid()) {
        const int s = kNetherOreLattice;
        const int yLo = std::max(baseY, kNetherOreBandBottom);
        const int yHi = std::min(baseY + kChunkSize - 1, kNetherOreBandTop);
        if (yLo <= yHi) {
            const int xi0 = floorDiv(ox - (s - 1), s);
            const int xi1 = floorDiv(ox + kChunkSize - 1, s);
            const int yi0 = floorDiv(yLo - (s - 1), s);
            const int yi1 = floorDiv(yHi, s);
            const int zi0 = floorDiv(oz - (s - 1), s);
            const int zi1 = floorDiv(oz + kChunkSize - 1, s);
            const struct {
                BlockID block;
                float probability;
            } ores[] = {
                {gen.quartzOre, kNetherQuartzOreProbability},
                {gen.netherGoldOre, kNetherGoldOreProbability},
                {gen.ancientDebris, kNetherAncientDebrisProbability},
            };
            for (int i = xi0; i <= xi1; ++i) {
                for (int j = yi0; j <= yi1; ++j) {
                    for (int k = zi0; k <= zi1; ++k) {
                        for (std::size_t t = 0; t < 3; ++t) {
                            if (!ores[t].block.valid()) {
                                continue;
                            }
                            const std::uint64_t salt =
                                static_cast<std::uint64_t>(
                                    gen.seed ^ kSaltNetherOre) ^
                                (t + 1) * kFeatureStep;
                            const std::uint64_t h = hash3(u64(i), u64(j),
                                                          u64(k), salt);
                            if (!rollBelow(h, ores[t].probability)) {
                                continue;
                            }
                            // The in-cell uniform point (the overworld
                            // vein start's bit layout, the single cell).
                            const int vx =
                                i * s + static_cast<int>((h >> 12) % u64(s));
                            const int vy =
                                j * s + static_cast<int>((h >> 20) % u64(s));
                            const int vz =
                                k * s + static_cast<int>((h >> 28) % u64(s));
                            if (vy < kNetherOreBandBottom ||
                                vy > kNetherOreBandTop) {
                                continue;
                            }
                            if (!cellInChunk(pos, vx, vy, vz)) {
                                continue;
                            }
                            const int lx = vx - ox;
                            const int ly = vy - baseY;
                            const int lz = vz - oz;
                            if (chunk.get(lx, ly, lz) != gen.netherrack) {
                                continue; // the netherrack-only predicate
                            }
                            chunk.set(lx, ly, lz, ores[t].block);
                        }
                    }
                }
            }
        }
    }

    // -- pass 3: the decorations (the last pass — it may overwrite the
    // surface / the ore cells: the deterministic-order pin) --------------
    if (hasPool) {
        // 3a: the lattice columns (the stem's lattice 4 / the basalt's
        // lattice 8 — the column-aligned points, one roll per lattice
        // cell, the height from the roll's low bits — the overworld
        // placeTrees' (h >> 24) % range convention).
        for (int s : {kNetherStemLattice, kNetherBasaltLattice}) {
            const bool stemPass = (s == kNetherStemLattice);
            const std::uint64_t salt =
                static_cast<std::uint64_t>(gen.seed ^
                                           (stemPass ? kSaltNetherStem
                                                    : kSaltNetherBasalt));
            const int hMin =
                stemPass ? kNetherStemHeightMin : kNetherBasaltHeightMin;
            const int hMax =
                stemPass ? kNetherStemHeightMax : kNetherBasaltHeightMax;
            const int x0 = floorDiv(ox, s);
            const int x1 = floorDiv(ox + kChunkSize - 1, s);
            const int z0 = floorDiv(oz, s);
            const int z1 = floorDiv(oz + kChunkSize - 1, s);
            for (int i = x0; i <= x1; ++i) {
                for (int j = z0; j <= z1; ++j) {
                    const int wx = i * s; // the column-aligned point
                    const int wz = j * s;
                    const NetherBiome& b =
                        gen.biomes[netherBiomeAt(gen, wx, wz)];
                    if (stemPass) {
                        // The forest biomes: the stem column UP from the
                        // plateau's top into the cavity (rooted on the
                        // nylium's top row — y = 52 stays the surface).
                        if (!b.stem.valid() ||
                            b.columnProbability <= 0.0f) {
                            continue;
                        }
                    } else {
                        if (!b.basaltColumns ||
                            b.columnProbability <= 0.0f ||
                            !gen.basalt.valid()) {
                            continue;
                        }
                    }
                    const std::uint64_t h = hash2(u64(i), u64(j), salt);
                    if (!rollBelow(h, b.columnProbability)) {
                        continue;
                    }
                    const int height = hMin +
                                       static_cast<int>(
                                           (h >> 24) %
                                           static_cast<std::uint64_t>(
                                               hMax - hMin + 1));
                    // The growth base: the plateau's top + 1 (the cavity's
                    // bottom) — the column rises UP into the air (the
                    // v3.5.0 band revision: visible from the landings).
                    const int base = kNetherColumnBaseY;
                    const BlockID fill = stemPass ? b.stem : gen.basalt;
                    for (int t = 0; t < height; ++t) {
                        const int wy = base + t;
                        if (wy > kNetherAirTop) {
                            break;
                        }
                        if (!cellInChunk(pos, wx, wy, wz)) {
                            continue;
                        }
                        chunk.set(wx - ox, wy - baseY, wz - oz, fill);
                    }
                }
            }
        }

        // 3b: the per-cell rolls (the soul family's plateau-top blobs +
        // the ceiling glowstone — one independent roll per (cell, feature)
        // pair, the overworld placeDecorations' per-entry precedent: the
        // feature's dedicated salt keeps the rolls uncorrelated). The
        // pass-order pin: like the lattice columns, the per-cell features
        // may OVERWRITE the surface / the ore cells (a later pass wins —
        // the magma roll over a soul_soil cell, the soil over the soul
        // sand's surface band); the ceiling glowstone hangs from the
        // cavity's air (the v3.5.0 band revision: y = 122 is open space,
        // the block IS the ceiling feature). The soul / magma bands are
        // inside the plateau, so the lava sea / the bedrock / the open
        // cavity interior are never touched by them.
        const int yLo = std::max(baseY, kNetherSoulSoilBandBottom);
        const int yHi = std::min(baseY + kChunkSize - 1, kNetherAirTop);
        for (int ly = yLo - baseY; ly <= yHi - baseY; ++ly) {
            const int wy = baseY + ly;
            for (int lz = 0; lz < kChunkSize; ++lz) {
                for (int lx = 0; lx < kChunkSize; ++lx) {
                    const int wx = ox + lx;
                    const int wz = oz + lz;
                    const NetherBiome& b =
                        gen.biomes[colBiome[static_cast<std::size_t>(lx) *
                                                kChunkSize +
                                            static_cast<std::size_t>(lz)]];
                    if (b.soulFamily) {
                        if (gen.soulSoil.valid() &&
                            wy >= kNetherSoulSoilBandBottom &&
                            wy <= kNetherSoulSoilBandTop &&
                            rollBelow(
                                hash3(u64(wx), u64(wz), u64(wy),
                                      static_cast<std::uint64_t>(
                                          gen.seed ^ kSaltNetherSoulSoil)),
                                kNetherSoulSoilProbability)) {
                            chunk.set(lx, ly, lz, gen.soulSoil);
                        }
                        if (gen.magmaBlock.valid() &&
                            wy >= kNetherMagmaBandBottom &&
                            wy <= kNetherMagmaBandTop &&
                            rollBelow(
                                hash3(u64(wx), u64(wz), u64(wy),
                                      static_cast<std::uint64_t>(
                                          gen.seed ^ kSaltNetherMagma)),
                                kNetherMagmaProbability)) {
                            chunk.set(lx, ly, lz, gen.magmaBlock);
                        }
                    }
                    if (wy == kNetherGlowstoneY && gen.glowstone.valid() &&
                        rollBelow(hash3(u64(wx), u64(wz), u64(wy),
                                      static_cast<std::uint64_t>(
                                          gen.seed ^ kSaltNetherGlowstone)),
                                  kNetherGlowstoneProbability)) {
                        chunk.set(lx, ly, lz, gen.glowstone);
                    }
                }
            }
        }
    }
}

} // namespace mc
