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

bool isOpaqueAt(const World& world, const BlockRegistry& blocks, int x,
                int y, int z) {
    return blocks.at(world.getBlock(x, y, z)).properties.opaque;
}

// The BFS core, shared by both light types and both entry points.
//
// `queue` is seeded with source cells; `inRegion` decides where values may
// be written. Cells outside the region are read-only boundary conditions —
// their stored light is already correct and must not be modified — but they
// may still be seeded so their light can flow into the region.
//
// Rules: light cannot enter an opaque cell; sky light propagates free
// vertically and -1 per horizontal step, block light -1 per step in every
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
            const int attenuation = (d.y != 0) ? (sky ? 0 : 1) : 1;
            const int next = value - attenuation;
            if (next <= 0) {
                continue;
            }
            if (isOpaqueAt(world, blocks, nx, ny, nz)) {
                continue;
            }
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
// of non-opaque cells reads 15 (open to the sky), everything below reads 0
// until the BFS fills it from horizontal neighbors. Missing chunks are air
// (the open run continues) and are never written.
void rescanColumn(World& world, const BlockRegistry& blocks, int x, int z) {
    bool open = true;
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
            if (isOpaqueAt(world, blocks, x, y, z)) {
                open = false; // the opaque cell itself reads 0
            } else {
                target = static_cast<std::uint8_t>(kMaxLight);
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
                    const int emission = b.properties.lightEmission;
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
                           const BlockPos& c) {
    const int R = kLightUpdateRadius;

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
        for (int x = c.x - R; x <= c.x + R; ++x) {
            for (int z = c.z - R; z <= c.z + R; ++z) {
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
    for (int x = c.x - R; x <= c.x + R; ++x) {
        for (int z = c.z - R; z <= c.z + R; ++z) {
            rescanColumn(world, blocks, x, z);
        }
    }
    {
        std::deque<BlockPos> queue;
        // Seed: lit cells inside the footprint plus the adjacent column
        // shell. The shell keeps its old (still correct) values and acts as
        // the boundary source for horizontal propagation.
        const int a = R + 1;
        for (int x = c.x - a; x <= c.x + a; ++x) {
            for (int z = c.z - a; z <= c.z + a; ++z) {
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
                     [c, R](int x, int, int z) {
                         return x >= c.x - R && x <= c.x + R &&
                                z >= c.z - R && z <= c.z + R;
                     });
    }

    // -- Block light: the ±R box (block light dies within 15 steps). --------
    const int x0 = c.x - R;
    const int x1 = c.x + R;
    const int y0 = std::max(0, c.y - R);
    const int y1 = std::min(kWorldHeight - 1, c.y + R);
    const int z0 = c.z - R;
    const int z1 = c.z + R;
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
                const int emission =
                    blocks.at(chunk->get(local.x, local.y, local.z))
                        .properties.lightEmission;
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
