// World: an unbounded (X/Z) x finite (Y in [0, kWorldHeight)) voxel space
// organized into 16^3 chunks.
//
// Responsibilities:
//  - block access by world coordinates, crossing chunk boundaries
//  - lazy chunk creation with an optional generation callback (Phase 4
//    plugs real terrain generation in here)
//  - dirty tracking for cross-chunk face culling: a setBlock on a chunk
//    boundary marks the neighboring chunk dirty as well, so both meshes
//    get rebuilt with a consistent cull decision.
//
// Out-of-world reads (y outside the valid range, or a chunk that was
// never created) return air — the "void" around the world is empty.
//
// Reference stability: a Chunk& returned by getOrCreateChunk is invalidated
// by any later World mutation that can rehash the chunk map (setBlock in
// another chunk, getOrCreateChunk). Use it within one logical operation.
#pragma once

#include <cstddef>
#include <cstdint>
#include <array>
#include <functional>
#include <set>
#include <unordered_map>
#include <vector>

#include "block/block.hpp"
#include "world/chunk.hpp"
#include "world/coordinates.hpp"

namespace mc {

class World {
public:
    // Fills a freshly created chunk; may leave it all-air. Must not call
    // back into this World (no re-entrant chunk creation).
    using ChunkGenerator = std::function<void(Chunk&, const ChunkPos&)>;

    // World coordinates. y outside [0, kWorldHeight) reads as air.
    BlockID getBlock(int x, int y, int z) const;
    // Returns false (and changes nothing) when y is out of range or the id
    // is invalid. Placing a block displaces any fluid in the cell (the
    // fluid reads 0 afterwards). Marks the target chunk — and any neighbor
    // chunk sharing a face with the edited block — dirty.
    bool setBlock(int x, int y, int z, BlockID id);

    // Fluid level of EITHER kind (0..kMaxFluidLevel; 0 = no fluid). y
    // outside [0, kWorldHeight) or a chunk that was never created reads 0
    // (the void is dry).
    std::uint8_t getFluid(int x, int y, int z) const;
    // Fluid level of ONE kind (0 when the cell is dry or holds the other
    // kind). Same coordinate conventions as getFluid.
    std::uint8_t getFluidOfKind(int x, int y, int z, FluidKind kind) const;
    // The kind present in the cell, or nullopt (dry). Same conventions.
    std::optional<FluidKind> getFluidKind(int x, int y, int z) const;
    // Sets the fluid of ONE kind in an existing chunk. Returns false (and
    // changes nothing) when y is out of range, the level is out of range,
    // the chunk was never created (the void is never created here), a
    // positive level targets a cell that holds a block (fluid only fills
    // air cells — a block displaces fluid instead), or the kind's level is
    // unchanged. A positive level REPLACES any other kind in the cell; a
    // zero level clears only this kind (the other kind, if any, stays).
    // Marks the target chunk and face-sharing neighbors dirty, like
    // setBlock.
    bool setFluid(int x, int y, int z, FluidKind kind, std::uint8_t level);

    // Stair facing (Phase 94): the per-instance orientation of a stair cell
    // (0-3: north/east/south/west). Same coordinate conventions as getFluid:
    // y outside [0, kWorldHeight) or a chunk that was never created reads 0
    // (the void / uncreated reads the default north). Written by the game
    // (the setBlock choke point, the placement, the rotation); read by the
    // collision (Entity::solidTopHeight) + the mesh (emitStairs).
    std::uint8_t getStairFacing(int x, int y, int z) const;
    // Sets the facing in an existing chunk. Returns false (and changes
    // nothing) when y is out of range, the facing is out of range, or the
    // chunk was never created. Marks the target chunk dirty (a mesh rebuild).
    bool setStairFacing(int x, int y, int z, std::uint8_t facing);

    // Light queries (0..15), same coordinate conventions as getBlock.
    // Above the world (y >= kWorldHeight) and around the world (a chunk
    // that was never created) the void is open sky: sky 15, block 0.
    // Below the world (y < 0) both read 0.
    std::uint8_t getSkyLight(int x, int y, int z) const;
    std::uint8_t getBlockLight(int x, int y, int z) const;

    // Chunk access.
    Chunk& getOrCreateChunk(const ChunkPos& pos); // runs generator if new
    // Insert a pre-built chunk (Phase 14 streaming: worker threads fill a
    // scratch chunk with the generator, the main thread integrates it).
    // The chunk must not exist yet and must not be dirty. Marks the chunk
    // and every face-sharing existing neighbor dirty: their boundary
    // faces were meshed against the void, and the new neighbor's content
    // changes their cull decision (the reverse direction is correct from
    // the start because the new chunk meshes after insertion).
    bool insertChunk(const ChunkPos& pos, Chunk&& chunk);
    // The inverse of insertChunk: drop an existing chunk (false when absent)
    // and mark the six face-sharing SURVIVING neighbors dirty — their
    // boundary faces were culled against the chunk that is now void (a
    // missing chunk reads air). Only safe when no worker job is in flight
    // (the streaming unload pass runs right after the pool flush, where the
    // main thread is the sole World writer).
    bool eraseChunk(const ChunkPos& pos);
    const Chunk* findChunk(const ChunkPos& pos) const;
    // Non-const access to an existing chunk WITHOUT creating it (the
    // lighting system writes light arrays into chunks it does not create).
    Chunk* findChunkMut(const ChunkPos& pos);
    bool hasChunk(const ChunkPos& pos) const;
    // Chunks that currently hold FLUID of the given kind (any level).
    // The fluid simulation's four per-frame passes (reaction, burn,
    // water, lava) iterate THESE instead of every chunk: a full-ring
    // world holds ~9k-200k chunks of which only a few dozen contain
    // fluid, and the all-chunk scans were the dominant frame cost.
    // Maintained by setFluid / setBlock (drain) / insertChunk /
    // eraseChunk / getOrCreateChunk (generation places sea water and
    // lava lakes) / clear.
    const std::set<ChunkPos>& fluidChunks(FluidKind kind) const {
        return m_fluidChunks[static_cast<std::size_t>(kind)];
    }
    // Sorted, like chunkPositions (deterministic iteration order).
    std::vector<ChunkPos> fluidChunkPositions(FluidKind kind) const;
    // Chunks currently marked dirty (content changed, mesh pending). The
    // mesh pass iterates THIS set instead of every chunk (a full-ring
    // world is ~9k-200k chunks; the dirty set is player-scale).
    const std::set<ChunkPos>& dirtyChunks() const { return m_dirtyChunks; }
    // The mesh pass calls this after rebuilding the chunk's mesh (clears
    // the chunk's own flag AND drops it from the set).
    void clearChunkDirty(const ChunkPos& pos);
    // Marks an existing chunk dirty through BOTH the chunk's own flag and
    // the World's dirty set. The lighting pipeline writes light arrays
    // into chunks directly (findChunkMut) and signals the re-mesh with
    // this — a bare chunk markDirty would be invisible to the set-driven
    // mesh pass (the chunk keeps its light-0 mesh: black faces on freshly
    // lit terrain).
    void markChunkDirty(const ChunkPos& pos);
    // True when the column holds ALL its chunks. Streaming loads and
    // integrates complete columns; a single-chunk creation (the
    // placement path, a save edit) must be topped up to keep the column
    // complete — the streaming loader counts a column as loaded when ANY
    // chunk exists, so a partial column's missing chunks are never
    // generated (a permanent void hole around the edit).
    bool columnComplete(int cx, int cz) const;
    // Creates (running the generator) every missing chunk of the column.
    // Idempotent: a complete column costs 8 map lookups and nothing else.
    void ensureColumn(int cx, int cz);
    std::size_t chunkCount() const { return m_chunks.size(); }
    // Sorted, for deterministic iteration (rendering, saving, meshing).
    std::vector<ChunkPos> chunkPositions() const;

    void setChunkGenerator(ChunkGenerator gen) { m_generator = std::move(gen); }
    void clear(); // drop all chunks (the generator is kept)

private:
    void markNeighborChunksDirty(int lx, int ly, int lz, const ChunkPos& pos);
    // Fluid-set maintenance: add the chunk (it now holds the kind), or
    // prune it when the last cell of the kind left (a kVolume scan, only
    // on drains — rare).
    void addFluidChunk(const ChunkPos& pos, FluidKind kind);
    void pruneFluidChunk(const Chunk& chunk, const ChunkPos& pos,
                         FluidKind kind);
    void scanFluidChunks(const Chunk& chunk, const ChunkPos& pos);

    std::unordered_map<ChunkPos, Chunk> m_chunks;
    // Chunks marked dirty (a mirror of each Chunk's own flag — the mesh
    // pass iterates this instead of every chunk).
    std::set<ChunkPos> m_dirtyChunks;
    // Indexed by FluidKind (Water = 1, Lava = 2; slot 0 unused).
    std::array<std::set<ChunkPos>, 3> m_fluidChunks;
    ChunkGenerator m_generator;
};

} // namespace mc
