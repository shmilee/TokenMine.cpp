// V3-1 Phase B (v3.4.0): the nether's pure chunk fill (see nether_gen.hpp
// for the layer bands + the derived-seed discipline).
#include "world/nether_gen.hpp"

namespace mc {

std::int64_t netherSeed(std::int64_t worldSeed) {
    return worldSeed ^ kSaltNether;
}

void fillNetherChunk(const NetherGen& gen, Chunk& chunk, const ChunkPos& pos) {
    // The fill is a function of the WORLD Y alone (the flat slab: every
    // column at a given Y is identical — the constructive-determinism
    // root: no lattice walk, no feature radius, no noise, so the
    // cross-chunk continuity + the negative-coordinate safety hold by
    // construction, the WorldGenerator's contract's flat-world form).
    const int baseY = pos.y * kChunkSize;
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
                    // The netherrack slab (y ∈ [32, 122]).
                    if (gen.netherrack.valid()) {
                        chunk.set(lx, ly, lz, gen.netherrack);
                    }
                } else {
                    // The bedrock roof (y ∈ [123, 127]).
                    if (gen.bedrock.valid()) {
                        chunk.set(lx, ly, lz, gen.bedrock);
                    }
                }
            }
        }
    }
}

} // namespace mc
