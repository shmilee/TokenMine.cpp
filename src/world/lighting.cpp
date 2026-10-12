#include "world/lighting.hpp"

#include <algorithm>
#include <cstring>
#include <deque>
#include <functional>
#include <unordered_set>
#include <utility>

namespace mc {

namespace {

const BlockPos kDirections[6] = {
    {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1},
};

// The cost of entering the target cell (a fluid cell — of EITHER kind,
// both attenuate like water — costs kFluidLightAttenuation in every
// direction; air keeps the per-type costs: sky light is free vertically,
// block light costs 1 everywhere).
int enterCost(const Chunk* chunk, int lx, int ly, int lz, bool sky,
              bool vertical) {
    const BlockID id = chunk->get(lx, ly, lz);
    if (id == BlockID::air() && chunk->fluidAt(lx, ly, lz) != 0) {
        return kFluidLightAttenuation;
    }
    return vertical ? (sky ? 0 : 1) : 1;
}

// A fluid cell is a block-light source when its KIND's reference block
// declares light_emission (data-driven: the lava fluid block glows at 15
// like glowstone — the value comes from the fluid block's property, never
// from the cell). 0 for dry cells or kinds whose reference block does not
// glow (water).
int fluidEmission(const BlockRegistry& blocks, std::uint8_t code) {
    if (code == 0) {
        return 0;
    }
    const std::optional<BlockID> id =
        blocks.fluidBlock(fluidKindName(fluidKindOf(code)));
    return id ? blocks.at(*id).properties.lightEmission : 0;
}

// The BFS core, shared by both light types and all entry points.
//
// `queue` is seeded with source cells; `inRegion` decides where values may
// be written. Cells outside the region are read-only boundary conditions —
// their stored light is already correct and must not be modified — but they
// may still be seeded so their light can flow into the region.
//
// Rules: light cannot enter an opaque cell; sky light propagates free
// vertically in air and -1 per horizontal air step, block light -1 per air
// step in every direction, fluid cells cost kFluidLightAttenuation in every
// direction; a cell is updated (and re-queued) only on a strict increase.
void propagateBFS(World& world, const BlockRegistry& blocks, bool sky,
                  std::deque<BlockPos>& queue,
                  const std::function<bool(int, int, int)>& inRegion) {
    while (!queue.empty()) {
        const BlockPos p = queue.front();
        queue.pop_front();
        const std::uint8_t value =
            sky ? world.getSkyLight(p.x, p.y, p.z)
                : world.getBlockLight(p.x, p.y, p.z);
        if (value == 0) {
            continue;
        }
        for (const BlockPos& d : kDirections) {
            const int nx = p.x + d.x;
            const int ny = p.y + d.y;
            const int nz = p.z + d.z;
            if (!inRegion(nx, ny, nz)) {
                continue;
            }
            const BlockPos np{nx, ny, nz};
            const ChunkPos cp = worldToChunk(np);
            Chunk* chunk = world.findChunkMut(cp);
            if (chunk == nullptr) {
                continue; // void: read as open sky / dark, never written
            }
            const BlockPos local = worldToChunkLocal(cp, np);
            const BlockID id = chunk->get(local.x, local.y, local.z);
            if (id != BlockID::air() && blocks.at(id).properties.opaque) {
                continue;
            }
            const int next = value -
                             enterCost(chunk, local.x, local.y, local.z, sky,
                                       d.y != 0);
            if (next <= 0) {
                continue;
            }
            const std::uint8_t current =
                sky ? chunk->skyLightAt(local.x, local.y, local.z)
                    : chunk->blockLightAt(local.x, local.y, local.z);
            if (current < static_cast<std::uint8_t>(next)) {
                if (sky) {
                    chunk->setSkyLightAt(local.x, local.y, local.z,
                                         static_cast<std::uint8_t>(next));
                } else {
                    chunk->setBlockLightAt(local.x, local.y, local.z,
                                           static_cast<std::uint8_t>(next));
                }
                queue.push_back(np);
            }
        }
    }
}

// Sky-light column scan for one (x, z) column: the topmost contiguous run
// of non-opaque cells is open to the sky and reads 15, attenuated by
// kFluidLightAttenuation per fluid cell while scanning down (air carries
// the running light unchanged); everything below the first opaque cell
// reads 0 until the BFS fills it from horizontal neighbors. Missing chunks
// are air (the open run continues) and are never written.
void rescanColumn(World& world, const BlockRegistry& blocks, int x, int z) {
    bool open = true;
    int light = kMaxLight; // sky light available just above the current cell
    for (int y = kWorldHeight - 1; y >= 0; --y) {
        const BlockPos p{x, y, z};
        const ChunkPos cp = worldToChunk(p);
        Chunk* chunk = world.findChunkMut(cp);
        if (chunk == nullptr) {
            continue;
        }
        const BlockPos local = worldToChunkLocal(cp, p);
        std::uint8_t target = 0;
        if (open) {
            const BlockID id = chunk->get(local.x, local.y, local.z);
            if (id != BlockID::air() && blocks.at(id).properties.opaque) {
                open = false; // the opaque cell itself reads 0
            } else {
                if (id == BlockID::air() &&
                    chunk->fluidAt(local.x, local.y, local.z) > 0) {
                    light = std::max(0, light - kFluidLightAttenuation);
                }
                target = static_cast<std::uint8_t>(light);
            }
        }
        const std::uint8_t current = chunk->skyLightAt(local.x, local.y, local.z);
        if (current != target) {
            chunk->setSkyLightAt(local.x, local.y, local.z, target);
        }
    }
}

} // namespace

void Lighting::propagateAll(World& world, const BlockRegistry& blocks) {
    const std::vector<ChunkPos> positions = world.chunkPositions();
    if (positions.empty()) {
        return;
    }

    // 1. Clear block light (sky light is fully overwritten by the column
    //    scan below, so it needs no separate clear).
    for (const ChunkPos& pos : positions) {
        Chunk* chunk = world.findChunkMut(pos);
        chunk->clearBlockLight();
    }

    // 2. Sky column scan over every column that holds at least one chunk.
    std::vector<std::pair<int, int>> columns;
    columns.reserve(positions.size() * kChunkSize * kChunkSize);
    for (const ChunkPos& pos : positions) {
        for (int lx = 0; lx < kChunkSize; ++lx) {
            for (int lz = 0; lz < kChunkSize; ++lz) {
                columns.emplace_back(pos.x * kChunkSize + lx,
                                     pos.z * kChunkSize + lz);
            }
        }
    }
    std::sort(columns.begin(), columns.end());
    columns.erase(std::unique(columns.begin(), columns.end()), columns.end());
    for (const auto& [x, z] : columns) {
        rescanColumn(world, blocks, x, z);
    }

    // 3. Sky BFS seeded with every lit cell.
    std::deque<BlockPos> queue;
    for (const ChunkPos& pos : positions) {
        const BlockPos o = chunkOrigin(pos);
        for (int ly = 0; ly < kChunkSize; ++ly) {
            for (int lz = 0; lz < kChunkSize; ++lz) {
                for (int lx = 0; lx < kChunkSize; ++lx) {
                    const BlockPos p{o.x + lx, o.y + ly, o.z + lz};
                    if (world.getSkyLight(p.x, p.y, p.z) > 0) {
                        queue.push_back(p);
                    }
                }
            }
        }
    }
    propagateBFS(world, blocks, true, queue, [](int, int, int) { return true; });

    // 4. Block light: seed every emission source (the cell keeps its
    //    emission value even when the block is opaque — glowstone) and
    //    propagate.
    for (const ChunkPos& pos : positions) {
        Chunk* chunk = world.findChunkMut(pos);
        const BlockPos o = chunkOrigin(pos);
        for (int ly = 0; ly < kChunkSize; ++ly) {
            for (int lz = 0; lz < kChunkSize; ++lz) {
                for (int lx = 0; lx < kChunkSize; ++lx) {
                    const Block& b = blocks.at(chunk->get(lx, ly, lz));
                    int emission = b.properties.lightEmission;
                    if (emission == 0) {
                        // A fluid cell of a glowing kind (lava).
                        emission =
                            fluidEmission(blocks, chunk->fluidAt(lx, ly, lz));
                    }
                    if (emission > 0) {
                        chunk->setBlockLightAt(lx, ly, lz,
                                               static_cast<std::uint8_t>(emission));
                        queue.push_back({o.x + lx, o.y + ly, o.z + lz});
                    }
                }
            }
        }
    }
    propagateBFS(world, blocks, false, queue,
                 [](int, int, int) { return true; });

    // 5. Full recompute: every chunk's light is considered touched.
    for (const ChunkPos& pos : positions) {
        world.findChunkMut(pos)->markDirty();
    }
}

void Lighting::updateLocal(World& world, const BlockRegistry& blocks,
                           const BlockPos& changed) {
    updateRegion(world, blocks, changed, changed);
}

void Lighting::updateRegion(World& world, const BlockRegistry& blocks,
                            const BlockPos& min, const BlockPos& max) {
    const int R = kLightUpdateRadius;
    // Region shapes: sky light travels vertically without air attenuation,
    // so the sky re-scan covers FULL COLUMNS over the X/Z footprint (the
    // changed box's footprint, expanded by R) - one change can light every
    // block below a broken ceiling; block light dies within 15 steps, so it
    // is re-propagated in the box expanded by R.
    const int sx0 = min.x - R;
    const int sx1 = max.x + R;
    const int sz0 = min.z - R;
    const int sz1 = max.z + R;

    // The re-light rewrites light in stages (the sky rescan clears values
    // that the BFS may restore), so "dirty" must be decided on the NET
    // change: save the light of every chunk the region touches, run the
    // re-light, then mark dirty only chunks that differ.
    struct SavedLight {
        ChunkPos pos;
        std::array<std::uint8_t, Chunk::kVolume> sky {};
        std::array<std::uint8_t, Chunk::kVolume> block {};
    };
    std::vector<SavedLight> saved;
    {
        // The region's chunks: the sky footprint columns pass through all
        // Y-chunk layers; the block box's chunks are a subset of those.
        std::unordered_set<ChunkPos> touched;
        touched.reserve(33 * 33 * 8);
        for (int x = sx0; x <= sx1; ++x) {
            for (int z = sz0; z <= sz1; ++z) {
                const int cx = floorDiv(x, kChunkSize);
                const int cz = floorDiv(z, kChunkSize);
                for (int y = 0; y < kWorldHeight; y += kChunkSize) {
                    touched.insert({cx, y / kChunkSize, cz});
                }
            }
        }
        saved.reserve(touched.size());
        for (const ChunkPos& cp : touched) {
            Chunk* chunk = world.findChunkMut(cp);
            if (chunk == nullptr) {
                continue;
            }
            SavedLight s;
            s.pos = cp;
            std::copy_n(chunk->skyLightData(), Chunk::kVolume, s.sky.begin());
            std::copy_n(chunk->blockLightData(), Chunk::kVolume, s.block.begin());
            saved.push_back(std::move(s));
        }
    }

    // -- Sky light: full columns in the ±R X/Z footprint. -------------------
    // Sky light travels vertically without attenuation, so one block can
    // change an entire column (a broken ceiling lights every block below
    // it) — the re-light must rescan and re-propagate whole columns, not a
    // box around the change.
    for (int x = sx0; x <= sx1; ++x) {
        for (int z = sz0; z <= sz1; ++z) {
            rescanColumn(world, blocks, x, z);
        }
    }
    {
        std::deque<BlockPos> queue;
        // Seed: lit cells inside the footprint plus the adjacent column
        // shell. The shell keeps its old (still correct) values and acts as
        // the boundary source for horizontal propagation.
        for (int x = sx0 - 1; x <= sx1 + 1; ++x) {
            for (int z = sz0 - 1; z <= sz1 + 1; ++z) {
                for (int y = 0; y < kWorldHeight; ++y) {
                    const BlockPos p{x, y, z};
                    if (!world.hasChunk(worldToChunk(p))) {
                        continue;
                    }
                    if (world.getSkyLight(x, y, z) > 0) {
                        queue.push_back(p);
                    }
                }
            }
        }
        propagateBFS(world, blocks, true, queue,
                     [sx0, sx1, sz0, sz1](int x, int, int z) {
                         return x >= sx0 && x <= sx1 &&
                                z >= sz0 && z <= sz1;
                     });
    }

    // -- Block light: the ±R box (block light dies within 15 steps). --------
    const int x0 = sx0;
    const int x1 = sx1;
    const int y0 = std::max(0, min.y - R);
    const int y1 = std::min(kWorldHeight - 1, max.y + R);
    const int z0 = sz0;
    const int z1 = sz1;
    std::deque<BlockPos> queue;

    // Clear the box; re-seed emission sources (a placed/broken glowstone).
    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            for (int z = z0; z <= z1; ++z) {
                const BlockPos p{x, y, z};
                Chunk* chunk = world.findChunkMut(worldToChunk(p));
                if (chunk == nullptr) {
                    continue;
                }
                const BlockPos local = worldToChunkLocal(worldToChunk(p), p);
                int emission =
                    blocks.at(chunk->get(local.x, local.y, local.z))
                        .properties.lightEmission;
                if (emission == 0) {
                    // A fluid cell of a glowing kind (lava).
                    emission = fluidEmission(
                        blocks, chunk->fluidAt(local.x, local.y, local.z));
                }
                const std::uint8_t target =
                    static_cast<std::uint8_t>(emission);
                const std::uint8_t current =
                    chunk->blockLightAt(local.x, local.y, local.z);
                if (current != target) {
                    chunk->setBlockLightAt(local.x, local.y, local.z, target);
                }
                if (target > 0) {
                    queue.push_back(p);
                }
            }
        }
    }
    // Shell: lit cells just outside the box (boundary source).
    for (int y = std::max(0, y0 - 1); y <= std::min(kWorldHeight - 1, y1 + 1);
         ++y) {
        for (int x = x0 - 1; x <= x1 + 1; ++x) {
            for (int z = z0 - 1; z <= z1 + 1; ++z) {
                const bool inside = x >= x0 && x <= x1 && y >= y0 && y <= y1 &&
                                    z >= z0 && z <= z1;
                if (inside) {
                    continue;
                }
                const BlockPos p{x, y, z};
                if (!world.hasChunk(worldToChunk(p))) {
                    continue;
                }
                if (world.getBlockLight(x, y, z) > 0) {
                    queue.push_back(p);
                }
            }
        }
    }
    propagateBFS(world, blocks, false, queue,
                 [x0, x1, y0, y1, z0, z1](int x, int y, int z) {
                     return x >= x0 && x <= x1 && y >= y0 && y <= y1 &&
                            z >= z0 && z <= z1;
                 });

    // Light is baked into mesh vertices: rebuild only the chunks whose
    // light actually changed (net, compared against the saved copy).
    for (const SavedLight& s : saved) {
        Chunk* chunk = world.findChunkMut(s.pos);
        if (std::memcmp(s.sky.data(), chunk->skyLightData(), Chunk::kVolume) !=
                0 ||
            std::memcmp(s.block.data(), chunk->blockLightData(),
                        Chunk::kVolume) != 0) {
            chunk->markDirty();
        }
    }
}

} // namespace mc
