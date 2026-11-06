// The End's worldgen (V3-10a, the nether_gen's second consumption —
// the dimension's fill as a coordinate-pure function: zero noise in
// 10a, constructionally deterministic; V3-10b adds the content family
// — the bedrock ocean + the island archipelago + the chorus
// decoration, every pass a pure function of (the derived seed, the
// coordinates): the chunk-order-independence + the cross-chunk
// continuity hold by construction, the lattice-walk contract).
#include "world/end_gen.hpp"

#include <algorithm>
#include <cstdint>

#include "world/noise.hpp" // hash3 + valueNoise2 (the nether_gen's twin)
#include "world/world_generator.hpp" // kLattice24Scale (the overworld
                                     // lattice-roll vocabulary)

namespace mc {

namespace {

// Reinterpret an (possibly negative) lattice index as 64 bits for
// hashing (the nether_gen's u64 twin — two's-complement bijection,
// the floorDiv discipline's hash form).
std::uint64_t u64(int v) {
    return static_cast<std::uint64_t>(static_cast<std::int64_t>(v));
}

// The 24-bit lattice / per-cell roll -> [0, 1) probability gate (the
// nether_gen's rollBelow verbatim — the shared form).
bool rollBelow(std::uint64_t h, float probability) {
    return static_cast<float>(h >> 40) * kLattice24Scale < probability;
}

// The island's footprint test (the central-island guardrail — the
// recon's R2: an island whose bounding box overlaps the central
// square |x|,|z| <= kEndIslandHalf is skipped: the exit portal's
// origin region + the entry platform stay island-free).
bool islandOverlapsCentralIsland(const EndIsland& isle) {
    const int half = kEndIslandHalf;
    return (isle.cx - isle.radius <= half) && (isle.cx + isle.radius >= -half) &&
        (isle.cz - isle.radius <= half) && (isle.cz + isle.radius >= -half);
}

// Writes the island's cells into the chunk (the per-cell air-only
// predicate — the plateau / the ocean / the exit / the platform are
// all non-air, so they self-protect; the recon's R1: the chunk's
// lattice walk extends by the lattice + the radius margin, so an
// island straddling a chunk boundary writes every piece of it, in
// every chunk it touches, identically).
void writeIsland(const EndGen& gen, const EndIsland& isle, Chunk& chunk,
                 const ChunkPos& pos, bool chorus) {
    const int ox = pos.x * kChunkSize;
    const int oz = pos.z * kChunkSize;
    const int baseY = pos.y * kChunkSize;
    // The layers: the top kEndIslandTopLayers at the full radius, the
    // cone's kEndIslandConeLayers at r-1, r-2 (the MC floating island's
    // read — flat top, tapered bottom).
    for (int layer = 0; layer < kEndIslandTopLayers + kEndIslandConeLayers;
         ++layer) {
        const int r = isle.radius - layer;
        if (r < 1) {
            continue; // the cone tapers out (r-2 can be 3 — still a disk)
        }
        const int wy = isle.top - layer;
        if (wy < 0 || wy >= kWorldHeight) {
            continue;
        }
        const int ly = wy - baseY;
        if (ly < 0 || ly >= kChunkSize) {
            continue;
        }
        for (int dz = -r; dz <= r; ++dz) {
            const int wz = isle.cz + dz;
            const int lz = wz - oz;
            if (lz < 0 || lz >= kChunkSize) {
                continue;
            }
            for (int dx = -r; dx <= r; ++dx) {
                if (dx * dx + dz * dz > r * r) {
                    continue; // the disk (the circle, not the square)
                }
                const int wx = isle.cx + dx;
                const int lx = wx - ox;
                if (lx < 0 || lx >= kChunkSize) {
                    continue;
                }
                if (chunk.get(lx, ly, lz) != BlockID::air()) {
                    continue; // the air-only predicate (self-protection)
                }
                chunk.set(lx, ly, lz, gen.endstone);
            }
        }
        // The chorus (the island's top center — the top layer only,
        // one cell, the MC's "chorus on the island top" read).
        if (layer == 0 && chorus) {
            const int lz = isle.cz - oz;
            const int lx = isle.cx - ox;
            if (lz >= 0 && lz < kChunkSize && lx >= 0 && lx < kChunkSize) {
                const int wy = isle.top + 1;
                if (wy >= 0 && wy < kWorldHeight) {
                    const int ly = wy - baseY;
                    if (ly >= 0 && ly < kChunkSize &&
                        chunk.get(lx, ly, lz) == BlockID::air()) {
                        chunk.set(lx, ly, lz, gen.chorusPlant);
                    }
                }
            }
        }
    }
}

} // namespace

std::int64_t endSeed(std::int64_t worldSeed) {
    // The netherSeed's twin: the XOR with the dedicated salt (the
    // parse-time rebuild — a load derives it from the stored "seed").
    return worldSeed ^ kSaltEnd;
}

std::size_t endBiomeAt(const EndGen& gen, int x, int z) {
    // The netherBiomeAt's clone over the end pool (the low-frequency
    // noise mapped onto the cumulative weight, registry load order).
    // 10b's single member: the constant selection (the noise still
    // consumes the dedicated salt — the 10d's second biome inherits
    // the working stream, no re-salting).
    if (gen.biomes.empty()) {
        return 0;
    }
    const float n = valueNoise2(
        static_cast<float>(x) * 0.02f, static_cast<float>(z) * 0.02f,
        gen.seed ^ kSaltEndBiome);
    const int target =
        static_cast<int>(n * static_cast<float>(gen.totalWeight));
    int acc = 0;
    for (std::size_t i = 0; i < gen.biomes.size(); ++i) {
        acc += gen.biomes[i].weight;
        if (target < acc) {
            return i;
        }
    }
    return gen.biomes.size() - 1;
}

bool endIslandAt(int latticeX, int latticeZ, std::uint64_t salt,
                 EndIsland& out) {
    // The lattice cell's pure decision (the nether stem lattice's
    // shape): the probability gate first (the cheap reject), then the
    // radius / the top from the hash's bits (the deterministic
    // per-island parameters — the same lattice cell always yields the
    // same island in every chunk that walks it, the R1 contract).
    const std::uint64_t h = hash3(u64(latticeX), u64(0), u64(latticeZ), salt);
    if (!rollBelow(h, kEndIslandProbability)) {
        return false;
    }
    out.cx = latticeX * kEndIslandLattice;
    out.cz = latticeZ * kEndIslandLattice;
    // The modulus BEFORE the int cast (the >> 24 slice is up to 2^40 —
    // a raw int cast would truncate to a signed 32-bit value and the
    // % would take a negative remainder: the gate's off-band radius).
    out.radius = kEndIslandRadiusMin +
        static_cast<int>((h >> 24) %
                         (kEndIslandRadiusMax - kEndIslandRadiusMin + 1));
    out.top = kEndIslandTopMin +
        static_cast<int>((h >> 32) %
                         (kEndIslandTopMax - kEndIslandTopMin + 1));
    return true;
}

bool endChorusAt(int latticeX, int latticeZ, std::uint64_t salt) {
    // The chorus's dedicated salt (the island's hash is NOT reused —
    // the decorrelation discipline: the plant never tracks the island
    // size / position stream). The lattice-y bit 1 separates it from
    // the island's (lattice-y 0) hash space.
    const std::uint64_t h = hash3(u64(latticeX), u64(1), u64(latticeZ), salt);
    return rollBelow(h, kEndChorusProbability);
}

Result<EndGen> EndGen::create(std::int64_t derivedSeed,
                              const BiomeRegistry& biomes,
                              const BlockRegistry& blocks) {
    // The six 10b names resolved once (the NetherGen::create pattern —
    // the fail-fast: a missing name names the block, the startup
    // rejects). Every one must be STATELESS (the worldgen's ENT-2
    // twin — a per-instance state is a journal the fill cannot write).
    EndGen gen;
    gen.seed = derivedSeed;
    const char* names[6] = {"endstone", "obsidian", "end_portal",
                            "end_portal_frame_eyed", "bedrock",
                            "chorus_plant"};
    BlockID* slots[6] = {&gen.endstone, &gen.obsidian, &gen.endPortal,
                         &gen.frameEyed, &gen.bedrock, &gen.chorusPlant};
    for (int i = 0; i < 6; ++i) {
        const BlockID id = blocks.idForName(names[i]);
        if (!id.valid()) {
            return MC_FAIL("end worldgen: missing block '" +
                           std::string(names[i]) +
                           "' (the 10b content fill's vocabulary)");
        }
        if (blocks.at(id).hasState()) {
            return MC_FAIL("end worldgen: '" + std::string(names[i]) +
                           "' has a per-instance state — worldgen never "
                           "writes state (the game's setBlock choke point "
                           "is the only state writer, the ENT-2 rule)");
        }
        *slots[i] = id;
    }
    // The biome pool (the NetherGen::create's twin): at least one
    // biome (the plateau's surface source — the S6 gate's subject) +
    // every surface resolved against the registry (stateful rejected
    // ENT-2 style — a surface with a per-instance state is a journal
    // the fill cannot write).
    if (biomes.count() == 0) {
        return MC_FAIL("end worldgen: the biome pool 'data/biomes_end' "
                       "is empty (the plateau's surface needs at least "
                       "one biome)");
    }
    for (const Biome& b : biomes.all()) {
        EndBiome rb;
        rb.name = b.name;
        rb.weight = b.weight;
        rb.surface = blocks.idForName(b.surface);
        if (!rb.surface.valid()) {
            return MC_FAIL("end biome '" + b.name +
                           "' names an unknown surface block '" +
                           b.surface + "'");
        }
        if (blocks.at(rb.surface).hasState()) {
            return MC_FAIL("end biome '" + b.name + "' has a stateful "
                           "surface block '" + b.surface +
                           "' — worldgen never writes state (the ENT-2 "
                           "rule)");
        }
        gen.biomes.push_back(std::move(rb));
    }
    for (const EndBiome& b : gen.biomes) {
        gen.totalWeight += b.weight;
    }
    return gen;
}

void fillEndChunk(const EndGen& gen, Chunk& chunk, const ChunkPos& pos) {
    const int ox = pos.x * kChunkSize;
    const int oz = pos.z * kChunkSize;
    const int baseY = pos.y * kChunkSize;

    // -- pass 0 (10b): the bedrock ocean (the whole dimension's floor,
    //    y in [0, 4] — the nether floor band's verbatim mirror, the
    //    MC end's bedrock sea's single-band read; the fall's escape
    //    surface, the pearl's landing belt).
    for (int ly = 0; ly < kChunkSize; ++ly) {
        const int wy = baseY + ly;
        if (wy < kEndOceanBottom || wy > kEndOceanTop) {
            continue;
        }
        for (int lz = 0; lz < kChunkSize; ++lz) {
            for (int lx = 0; lx < kChunkSize; ++lx) {
                if (gen.bedrock.valid()) {
                    chunk.set(lx, ly, lz, gen.bedrock);
                }
            }
        }
    }

    // -- pass 1: the central plateau (the 10a's band, the biome's
    //    surface — 10b's single biome end_plains is a no-op override
    //    over the endstone baseline, the nether wastes' precedent;
    //    the 10d's second biome inherits the per-column selection).
    for (int ly = 0; ly < kChunkSize; ++ly) {
        const int wy = baseY + ly;
        if (wy < kEndIslandBottom || wy > kEndIslandTop) {
            continue;
        }
        for (int lz = 0; lz < kChunkSize; ++lz) {
            const int wz = oz + lz;
            if (std::abs(wz) > kEndIslandHalf) {
                continue;
            }
            for (int lx = 0; lx < kChunkSize; ++lx) {
                const int wx = ox + lx;
                if (std::abs(wx) > kEndIslandHalf) {
                    continue;
                }
                const std::size_t biome = endBiomeAt(gen, wx, wz);
                if (biome < gen.biomes.size()) {
                    chunk.set(lx, ly, lz, gen.biomes[biome].surface);
                }
            }
        }
    }

    // -- pass 2 (10b): the island archipelago (the lattice-16 walk —
    //    the R1 cross-chunk contract: the range extends by the lattice
    //    + the radius margin, so every island the chunk can touch is
    //    walked, and each writes its own cells air-only).
    if (gen.endstone.valid()) {
        const int s = kEndIslandLattice;
        const int margin = s + (kEndIslandRadiusMax + kEndIslandTopLayers);
        const int xi0 = floorDiv(ox - margin, s);
        const int xi1 = floorDiv(ox + kChunkSize - 1 + margin, s);
        const int zi0 = floorDiv(oz - margin, s);
        const int zi1 = floorDiv(oz + kChunkSize - 1 + margin, s);
        const std::uint64_t saltIsland =
            static_cast<std::uint64_t>(gen.seed ^ kSaltEndIsland);
        const std::uint64_t saltChorus =
            static_cast<std::uint64_t>(gen.seed ^ kSaltEndChorus);
        for (int iz = zi0; iz <= zi1; ++iz) {
            for (int ix = xi0; ix <= xi1; ++ix) {
                EndIsland isle;
                if (!endIslandAt(ix, iz, saltIsland, isle)) {
                    continue;
                }
                if (islandOverlapsCentralIsland(isle)) {
                    continue; // the R2 guardrail (the exit + the platform)
                }
                writeIsland(gen, isle, chunk, pos,
                            endChorusAt(ix, iz, saltChorus));
            }
        }
    }

    // -- pass 3 (10a): the exit portal at the world origin (body level
    //    53) — the LAST pass (the 5x5's cells are the exit's, never an
    //    island's: the guardrail + the air-only predicate double-gate,
    //    but the order keeps the 10a's exact cells regardless).
    for (int ly = 0; ly < kChunkSize; ++ly) {
        const int wy = baseY + ly;
        if (wy != kEndExitLevel) {
            continue;
        }
        for (int lz = 0; lz < kChunkSize; ++lz) {
            const int wz = oz + lz;
            if (std::abs(wz) > 2) {
                continue;
            }
            for (int lx = 0; lx < kChunkSize; ++lx) {
                const int wx = ox + lx;
                if (std::abs(wx) > 2) {
                    continue;
                }
                const bool corner = (std::abs(wx) == 2) &&
                                    (std::abs(wz) == 2);
                if (corner) {
                    continue; // the 5x5 ring's missing corners
                }
                if (std::abs(wx) <= 1 && std::abs(wz) <= 1) {
                    chunk.set(lx, ly, lz, gen.endPortal);
                } else {
                    chunk.set(lx, ly, lz, gen.frameEyed);
                }
            }
        }
    }
}

} // namespace mc
