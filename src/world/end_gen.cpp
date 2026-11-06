#include "world/end_gen.hpp"

#include <cstdlib>

namespace mc {

std::int64_t endSeed(std::int64_t worldSeed) {
    // The netherSeed's twin: the XOR with the dedicated salt (the
    // parse-time rebuild — a load derives it from the stored "seed").
    return worldSeed ^ kSaltEnd;
}

Result<EndGen> EndGen::create(std::int64_t derivedSeed,
                              const BlockRegistry& blocks) {
    // The four 10a names resolved once (the NetherGen::create pattern —
    // the fail-fast: a missing name names the block, the startup
    // rejects). Every one must be STATELESS (the worldgen's ENT-2
    // twin — a per-instance state is a journal the fill cannot write).
    EndGen gen;
    gen.seed = derivedSeed;
    const char* names[4] = {"endstone", "obsidian", "end_portal",
                            "end_portal_frame_eyed"};
    BlockID* slots[4] = {&gen.endstone, &gen.obsidian, &gen.endPortal,
                         &gen.frameEyed};
    for (int i = 0; i < 4; ++i) {
        const BlockID id = blocks.idForName(names[i]);
        if (!id.valid()) {
            return MC_FAIL("end worldgen: missing block '" +
                           std::string(names[i]) +
                           "' (the 10a minimum fill's vocabulary)");
        }
        if (blocks.at(id).hasState()) {
            return MC_FAIL("end worldgen: '" + std::string(names[i]) +
                           "' has a per-instance state — worldgen never "
                           "writes state (the game's setBlock choke point "
                           "is the only state writer, the ENT-2 rule)");
        }
        *slots[i] = id;
    }
    return gen;
}

void fillEndChunk(const EndGen& gen, Chunk& chunk, const ChunkPos& pos) {
    const int ox = pos.x * kChunkSize;
    const int oz = pos.z * kChunkSize;
    const int baseY = pos.y * kChunkSize;
    for (int ly = 0; ly < kChunkSize; ++ly) {
        const int wy = baseY + ly;
        for (int lz = 0; lz < kChunkSize; ++lz) {
            const int wz = oz + lz;
            for (int lx = 0; lx < kChunkSize; ++lx) {
                const int wx = ox + lx;
                // The island plateau (|x|, |z| <= 60, y in [48, 52]).
                const bool onIsland =
                    std::abs(wx) <= kEndIslandHalf &&
                    std::abs(wz) <= kEndIslandHalf &&
                    wy >= kEndIslandBottom && wy <= kEndIslandTop;
                if (onIsland) {
                    chunk.set(lx, ly, lz, gen.endstone);
                }
                // The exit portal at the world origin (body level 53):
                // the 5x5 all-eyed frame ring around the 3x3 active
                // portal center. The y=52 endstone under the center
                // (set by the island pass above) is the standing
                // surface — a player inside the 3x3 has the portal
                // cell around its lower body (the entry's overlap).
                if (wy == kEndExitLevel && std::abs(wx) <= 2 &&
                    std::abs(wz) <= 2) {
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
                // Everything else: the VOID (air, no floor — the fall
                // dies at the kill plane).
            }
        }
    }
}

} // namespace mc
