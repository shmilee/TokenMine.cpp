#include "world/lighting.hpp"

#include <algorithm>
#include <cassert>
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

// The light opacity of ONE cell — the single definition of the unified
// attenuation (LGT-2, the leaf light attenuation): a fluid cell of EITHER
// kind costs kFluidLightAttenuation in every direction (the reference
// block's light_opacity is NOT read for fluids — the Phase 51 documented
// simplification); a dry cell costs its block's light_opacity (derived
// from opaque at data load: opaque -> 15, non-opaque -> 0, an explicit
// `light_opacity` overrides — see BlockRegistry).
int cellOpacity(const BlockRegistry& blocks, const Chunk* chunk, int lx,
                int ly, int lz) {
    const BlockID id = chunk->get(lx, ly, lz);
    if (id == BlockID::air() && chunk->fluidAt(lx, ly, lz) != 0) {
        return kFluidLightAttenuation;
    }
    return blocks.at(id).properties.lightOpacity;
}

// The cost of entering the target cell (LGT-2, the unified attenuation):
// the cell's opacity in the given direction — sky light is free ONLY
// vertically through a zero-opacity cell (air); every other entry costs
// at least 1 (glass 1, leaves 2, a fluid cell kFluidLightAttenuation in
// every direction).
int enterCost(const BlockRegistry& blocks, const Chunk* chunk, int lx,
              int ly, int lz, bool sky, bool vertical) {
    const int o = cellOpacity(blocks, chunk, lx, ly, lz);
    return vertical ? (sky ? o : std::max(1, o)) : std::max(1, o);
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
// Rules (LGT-2): light cannot enter a dry cell whose block fully blocks
// light (light_opacity 15 — for content without an explicit field exactly
// the opaque set); entering any other cell costs its opacity in the given
// direction: sky light free vertically through a zero-opacity cell,
// otherwise at least 1 per step (air horizontal 1, glass 1, leaves 2), a
// fluid cell of either kind always kFluidLightAttenuation in every
// direction; a cell is updated (and re-queued) only on a strict increase.
//
// The worklist algorithm converges to the same light field regardless of
// queue order (monotone strict-increase updates), so seeding a chunk-level
// scan (Phase 99.9) is result-identical to the old block-level scan.
//
// Hot path: a BFS neighbor stays in the same chunk 15 of 16 times, so the
// World's chunk lookup is cached (last-chunk) — the per-cell unordered_map
// find of the pre-99.9 code was the dominant cost of a lighting box.
void propagateBFS(World& world, const BlockRegistry& blocks, bool sky,
                  std::deque<BlockPos>& queue,
                  const std::function<bool(int, int, int)>& inRegion) {
    Chunk* last = nullptr;
    ChunkPos lastCp {-2000000000, -1, -2000000000};
    const auto chunkAt = [&](const BlockPos& p) -> Chunk* {
        const ChunkPos cp = worldToChunk(p);
        if (cp == lastCp) {
            return last;
        }
        last = world.findChunkMut(cp);
        lastCp = cp;
        return last;
    };
    while (!queue.empty()) {
        const BlockPos p = queue.front();
        queue.pop_front();
        const ChunkPos cp = worldToChunk(p);
        const BlockPos local = worldToChunkLocal(cp, p);
        Chunk* chunk = chunkAt(p);
        if (chunk == nullptr) {
            continue;
        }
        const std::uint8_t value =
            sky ? chunk->skyLightAt(local.x, local.y, local.z)
                : chunk->blockLightAt(local.x, local.y, local.z);
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
            Chunk* nchunk = chunkAt(np);
            if (nchunk == nullptr) {
                continue; // void: read as open sky / dark, never written
            }
            const BlockPos nlocal = worldToChunkLocal(worldToChunk(np), np);
            const BlockID id = nchunk->get(nlocal.x, nlocal.y, nlocal.z);
            // LGT-2: light cannot ENTER a dry cell whose block fully
            // blocks light (opacity 15 — for content without an explicit
            // field exactly the opaque set). The cell's OWN value is
            // untouched (an emission source keeps it, the glowstone
            // pattern); a fluid cell never blocks (its opacity is the
            // fixed kFluidLightAttenuation).
            if (id != BlockID::air() &&
                cellOpacity(blocks, nchunk, nlocal.x, nlocal.y, nlocal.z) ==
                    kMaxLight) {
                continue;
            }
            const int next = value -
                             enterCost(blocks, nchunk, nlocal.x, nlocal.y,
                                       nlocal.z, sky, d.y != 0);
            if (next <= 0) {
                continue;
            }
            const std::uint8_t current =
                sky ? nchunk->skyLightAt(nlocal.x, nlocal.y, nlocal.z)
                    : nchunk->blockLightAt(nlocal.x, nlocal.y, nlocal.z);
            if (current < static_cast<std::uint8_t>(next)) {
                if (sky) {
                    nchunk->setSkyLightAt(nlocal.x, nlocal.y, nlocal.z,
                                          static_cast<std::uint8_t>(next));
                } else {
                    nchunk->setBlockLightAt(nlocal.x, nlocal.y, nlocal.z,
                                            static_cast<std::uint8_t>(next));
                }
                queue.push_back(np);
            }
        }
    }
}

// Sky-light column scan for one (x, z) column: the topmost contiguous run
// of cells that do not fully block light (light_opacity < 15) is open to
// the sky and reads the source level, attenuated by each cell's OPACITY
// while scanning down (LGT-2: air 0, glass 1, leaves 2, a fluid cell the
// fixed kFluidLightAttenuation); everything below the first full-opacity
// cell reads 0 until the BFS fills it from horizontal neighbors. Missing
// chunks are air (the open run continues) and are never written.
//
// `sourceLevel` (LGT-3 seam): the open-sky level the run starts from.
// Production always passes kMaxLight (the noon field — the default via
// propagateAll); the confluence test drives S < kMaxLight to pin that
// the propagated field is affine in the source level.
void rescanColumn(World& world, const BlockRegistry& blocks, int x, int z,
                  int sourceLevel) {
    bool open = true;
    int light = sourceLevel; // sky light just above the current cell
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
            // LGT-2: a full-opacity dry cell (15 — for content without an
            // explicit field exactly the opaque set) cuts the open run and
            // itself reads 0; every other cell attenuates by its opacity
            // (a fluid cell: the fixed kFluidLightAttenuation — the
            // pre-LGT-2 formula).
            const int o =
                cellOpacity(blocks, chunk, local.x, local.y, local.z);
            if (o == kMaxLight) {
                open = false; // the full-opacity cell itself reads 0
            } else {
                light = std::max(0, light - o);
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

// LGT-3: the source level S behind a day factor is kNightSkyLevel +
// (kMaxLight - kNightSkyLevel) * dayFactor (deep night 0 -> the
// kNightSkyLevel moonlight floor, noon 1 -> kMaxLight), and the retreat
// distance is kMaxLight - S = (kMaxLight - kNightSkyLevel) * (1 - df).
// The integer version floors it (whole light levels — the save/spawn
// semantics); see the header for the affine-field argument.
std::uint8_t effectiveSkyLight(std::uint8_t stored, float dayFactor) {
    const int delta = static_cast<int>(std::floor(static_cast<double>(
        kMaxLight - kNightSkyLevel) *
        (1.0 - static_cast<double>(dayFactor))));
    const int value = static_cast<int>(stored) - delta;
    return static_cast<std::uint8_t>(value > 0 ? value : 0);
}

// `stored` is the shader's 0..1-normalized sky value (light level /
// kMaxLight): the exact mirror of the basic.frag sky term, the retreat
// in normalized units = kNightRetreatScale * (1 - dayFactor).
float effectiveSkyLightF(float stored, float dayFactor) {
    return std::max(
        0.0f,
        stored - kNightRetreatScale * (1.0f - dayFactor));
}

void Lighting::propagateAll(World& world, const BlockRegistry& blocks,
                            int skySourceLevel) {
    // LGT-3 seam contract: the sky source reads a level in
    // [0, kMaxLight] (production passes the default kMaxLight).
    assert(skySourceLevel >= 0 && skySourceLevel <= kMaxLight);
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
        rescanColumn(world, blocks, x, z, skySourceLevel);
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
        world.markChunkDirty(pos);
    }
}

void Lighting::updateLocal(World& world, const BlockRegistry& blocks,
                           const BlockPos& changed) {
    updateRegion(world, blocks, changed, changed);
}

void Lighting::updateRegion(World& world, const BlockRegistry& blocks,
                            const BlockPos& min, const BlockPos& max,
                            bool newTerrain, std::set<ChunkPos>* dirtyOut) {
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
    // box around the change. The rescan covers the FULL ±R region (the
    // original semantics): a column whose pre-insertion light rode a path
    // THROUGH the now-filled void keeps a stale high value unless its
    // column is re-asserted to its vertical-only value here (the box
    // footprint alone regressed InsertedTwoChunkColumnRelight — the dark
    // region beyond the box never got its sources re-seeded). Chunk-level
    // scan (Phase 99.9): one chunk lookup per 16x16x16 cell block instead
    // of one per CELL (the old rescanColumn did a findChunkMut per cell,
    // ~12 ms/box). Per-column state (open run, running light) carries
    // across the Y-chunk layers exactly as before, so the per-cell
    // sequence — and the result — is identical.
    long rescanWrites = 0; // Phase 99.9: zero writes -> skip the sky BFS
    {
        constexpr int kLayers = kWorldHeight / kChunkSize;
        const int fcx0 = floorDiv(sx0, kChunkSize), fcx1 = floorDiv(sx1, kChunkSize);
        const int fcz0 = floorDiv(sz0, kChunkSize), fcz1 = floorDiv(sz1, kChunkSize);
        for (int cx = fcx0; cx <= fcx1; ++cx) {
            for (int cz = fcz0; cz <= fcz1; ++cz) {
                for (int lx = 0; lx < kChunkSize; ++lx) {
                    for (int lz = 0; lz < kChunkSize; ++lz) {
                        const int wx = cx * kChunkSize + lx;
                        const int wz = cz * kChunkSize + lz;
                        if (wx < sx0 || wx > sx1 || wz < sz0 || wz > sz1) {
                            continue; // outside the ±R footprint: untouched
                        }
                        bool open = true;
                        int light = kMaxLight; // sky light just above y top
                        for (int cy = kLayers - 1; cy >= 0; --cy) {
                            const Chunk* chunk = world.findChunk({cx, cy, cz});
                            if (chunk == nullptr) {
                                continue; // void: open run continues, no write
                            }
                            for (int ly = kChunkSize - 1; ly >= 0; --ly) {
                                std::uint8_t target = 0;
                                if (open) {
                                    // LGT-2: the same per-cell sequence as
                                    // rescanColumn — a full-opacity dry
                                    // cell cuts the run (reads 0), every
                                    // other cell attenuates by its opacity
                                    // (fluid: the fixed
                                    // kFluidLightAttenuation).
                                    const int o =
                                        cellOpacity(blocks, chunk, lx, ly, lz);
                                    if (o == kMaxLight) {
                                        open = false; // full opacity reads 0
                                    } else {
                                        light = std::max(0, light - o);
                                        target = static_cast<std::uint8_t>(light);
                                    }
                                }
                                const std::uint8_t current =
                                    chunk->skyLightAt(lx, ly, lz);
                                if (current != target) {
                                    const_cast<Chunk*>(chunk)->setSkyLightAt(
                                        lx, ly, lz, target);
                                    ++rescanWrites;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    if (rescanWrites == 0 && !newTerrain) {
        // No-op skip (Phase 99.9): the rescan re-asserted every column's
        // vertical values and found them unchanged, and the box holds no
        // freshly inserted terrain. No cell's sky light differs from the
        // old fixed point, and a change to EXISTING terrain can only LOWER
        // sky values, so no horizontal propagation can raise any cell —
        // the seed scan + BFS would be pure re-proof. (newTerrain forces
        // the propagation: a new column can be born dark — under a
        // ceiling — and still need light FLOWED IN from its lit neighbor
        // columns, even though its own rescan wrote nothing.)
    } else {
        std::deque<BlockPos> queue;
        // Seed: EVERY lit cell of the ±R region (the original semantics,
        // chunk-level since Phase 99.9 — one lookup per 16^3 block). The
        // worklist BFS propagates only from QUEUED cells, and a box
        // change can invalidate the light of any column within the
        // propagation distance (the pre-insertion field may have ridden
        // paths through the now-filled void), so the full lit region is
        // the required source set — reduced seeds (changed cells + shell)
        // left darkened regions unre-lit (test regression). The seed
        // ORDER differs from the pre-99.9 block loop; the BFS is a
        // monotone strict-increase worklist, so the converged field is
        // identical regardless of order.
        constexpr int kLayers = kWorldHeight / kChunkSize;
        const int scx0 = floorDiv(sx0, kChunkSize),
                  scx1 = floorDiv(sx1, kChunkSize);
        const int scz0 = floorDiv(sz0, kChunkSize),
                  scz1 = floorDiv(sz1, kChunkSize);
        for (int cx = scx0; cx <= scx1; ++cx) {
            for (int cz = scz0; cz <= scz1; ++cz) {
                for (int cy = 0; cy < kLayers; ++cy) {
                    const Chunk* chunk = world.findChunk({cx, cy, cz});
                    if (chunk == nullptr) {
                        continue;
                    }
                    for (int ly = 0; ly < kChunkSize; ++ly) {
                        for (int lz = 0; lz < kChunkSize; ++lz) {
                            for (int lx = 0; lx < kChunkSize; ++lx) {
                                const int wx = cx * kChunkSize + lx;
                                const int wz = cz * kChunkSize + lz;
                                if (wx < sx0 || wx > sx1 || wz < sz0 ||
                                    wz > sz1) {
                                    continue;
                                }
                                if (chunk->skyLightAt(lx, ly, lz) > 0) {
                                    queue.push_back(
                                        {wx, cy * kChunkSize + ly, wz});
                                }
                            }
                        }
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
    // Chunk-level scan (Phase 99.9): one lookup per 16^3 cell block, then
    // direct array access — the old loop did a findChunkMut per cell.
    {
        const int bcx0 = floorDiv(x0, kChunkSize), bcx1 = floorDiv(x1, kChunkSize);
        const int bcy0 = floorDiv(y0, kChunkSize), bcy1 = floorDiv(y1, kChunkSize);
        const int bcz0 = floorDiv(z0, kChunkSize), bcz1 = floorDiv(z1, kChunkSize);
        for (int cx = bcx0; cx <= bcx1; ++cx) {
            for (int cy = bcy0; cy <= bcy1; ++cy) {
                for (int cz = bcz0; cz <= bcz1; ++cz) {
                    Chunk* chunk = world.findChunkMut({cx, cy, cz});
                    if (chunk == nullptr) {
                        continue;
                    }
                    for (int ly = 0; ly < kChunkSize; ++ly) {
                        for (int lz = 0; lz < kChunkSize; ++lz) {
                            for (int lx = 0; lx < kChunkSize; ++lx) {
                                const int wx = cx * kChunkSize + lx;
                                const int wy = cy * kChunkSize + ly;
                                const int wz = cz * kChunkSize + lz;
                                if (wx < x0 || wx > x1 || wy < y0 ||
                                    wy > y1 || wz < z0 || wz > z1) {
                                    continue;
                                }
                                int emission =
                                    blocks.at(chunk->get(lx, ly, lz))
                                        .properties.lightEmission;
                                if (emission == 0) {
                                    // A fluid cell of a glowing kind (lava).
                                    emission = fluidEmission(
                                        blocks, chunk->fluidAt(lx, ly, lz));
                                }
                                const std::uint8_t target =
                                    static_cast<std::uint8_t>(emission);
                                const std::uint8_t current =
                                    chunk->blockLightAt(lx, ly, lz);
                                if (current != target) {
                                    chunk->setBlockLightAt(lx, ly, lz, target);
                                }
                                if (target > 0) {
                                    queue.push_back({wx, wy, wz});
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    // Shell: lit cells just outside the box (boundary source). Chunk-level
    // (Phase 99.9) — the box chunk range plus one layer in each direction.
    {
        const int hcx0 = floorDiv(x0 - 1, kChunkSize),
                  hcx1 = floorDiv(x1 + 1, kChunkSize);
        const int hcy0 = floorDiv(std::max(0, y0 - 1), kChunkSize),
                  hcy1 =
                      floorDiv(std::min(kWorldHeight - 1, y1 + 1), kChunkSize);
        const int hcz0 = floorDiv(z0 - 1, kChunkSize),
                  hcz1 = floorDiv(z1 + 1, kChunkSize);
        for (int cx = hcx0; cx <= hcx1; ++cx) {
            for (int cy = hcy0; cy <= hcy1; ++cy) {
                for (int cz = hcz0; cz <= hcz1; ++cz) {
                    const Chunk* chunk = world.findChunk({cx, cy, cz});
                    if (chunk == nullptr) {
                        continue;
                    }
                    for (int ly = 0; ly < kChunkSize; ++ly) {
                        for (int lz = 0; lz < kChunkSize; ++lz) {
                            for (int lx = 0; lx < kChunkSize; ++lx) {
                                const int wx = cx * kChunkSize + lx;
                                const int wy = cy * kChunkSize + ly;
                                const int wz = cz * kChunkSize + lz;
                                const bool inside =
                                    wx >= x0 && wx <= x1 && wy >= y0 &&
                                    wy <= y1 && wz >= z0 && wz <= z1;
                                if (inside) {
                                    continue;
                                }
                                if (chunk->blockLightAt(lx, ly, lz) > 0) {
                                    queue.push_back({wx, wy, wz});
                                }
                            }
                        }
                    }
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
    // Through the World API (flag AND dirty set): a bare chunk markDirty
    // is invisible to the set-driven mesh pass — the chunk would keep its
    // light-0 mesh (black faces on freshly lit terrain, 99.2 user report
    // from the walking perf run). 99.10: the worker path collects the
    // positions in `dirtyOut` instead — two parallel lighting jobs must
    // not mutate the shared dirty set concurrently; the main thread
    // applies them via markChunkDirty after the pool flush.
    for (const SavedLight& s : saved) {
        Chunk* chunk = world.findChunkMut(s.pos);
        if (std::memcmp(s.sky.data(), chunk->skyLightData(), Chunk::kVolume) !=
                0 ||
            std::memcmp(s.block.data(), chunk->blockLightData(),
                        Chunk::kVolume) != 0) {
            if (dirtyOut != nullptr) {
                dirtyOut->insert(s.pos);
            } else {
                world.markChunkDirty(s.pos);
            }
        }
    }
}

} // namespace mc
