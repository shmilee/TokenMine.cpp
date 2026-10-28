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
#include <optional>
#include <set>
#include <unordered_map>
#include <vector>

#include "block/block.hpp"
#include "world/chunk.hpp"
#include "world/coordinates.hpp"

namespace mc {

// Cell mutation (PIPE-2, the save journal): one cell a World-level writer
// changed. The World is the SOLE choke point for post-generation cell
// writes (every path — the game's setBlock choke point, the fluid sim
// passes, the burn pass, the save replay — goes through World::setBlock /
// setFluid; the generator and streaming inserts write FRESH content,
// which is never a journal entry), so an observer on the World sees the
// session's whole edit history.
struct CellMutation {
    BlockPos pos;
    BlockID block = BlockID::invalid(); // invalid = no block change
    // nullopt = no fluid change; otherwise the cell's fluid code AFTER
    // the write (0 = drained). The displacement trap: a setBlock into a
    // fluid cell drains it via the internal chunk.setFluidAt (which
    // bypasses setFluid), so setBlock reports the drain HERE — the
    // "double report" (block + level-0 fluid in one mutation).
    std::optional<std::uint8_t> fluidCode;
};

using CellMutationObserver = std::function<void(const CellMutation&)>;

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

    // Per-block state (Phase 94 stair facing, generalized in ENT-2): the
    // per-instance state of a stateful cell (a stair's orientation 0-3; any
    // model.state_count block's state 0..count-1). Same coordinate
    // conventions as getFluid: y outside [0, kWorldHeight) or a chunk that
    // was never created reads the sentinel 255 (kBlockStateUnset) — the
    // collision (solidTopHeight) + the mesh (emitStairs) + the save
    // derivation fall back to the block's state default
    // (Block::stateDefault). Written by the game (the setBlock choke point,
    // the placement, the rotation); read by the collision + the mesh + the
    // save.
    std::uint8_t getBlockState(int x, int y, int z) const;
    // Sets the state in an existing chunk. Returns false (and changes
    // nothing) when y is out of range, the state is 255 (the UNSET
    // sentinel — the World API never writes it; the sentinel is what a
    // fresh cell holds), or the chunk was never created. This call
    // validates < 255 + y + existence + no-change ONLY (the World holds no
    // registry): the [0, stateCount) domain is enforced at the three
    // writers — the game's setBlock choke point (the block's default), the
    // rotation ((s+1) % stateCount) and the save parse (per-entry range
    // against the registry). Marks the target chunk dirty (a mesh rebuild).
    bool setBlockState(int x, int y, int z, std::uint8_t state);

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
    // Snapshot insertion (PIPE-1): place a pre-built chunk into a
    // THROWAWAY snapshot World with none of the live-World bookkeeping —
    // no fluid-set scan, no dirty marking, no neighbor marking. The
    // snapshot holds the meshed column's 3x3 column neighborhood and is
    // read by a worker mesher, then destroyed; none of the snapshot's
    // indexes (dirty set, fluid sets) are ever consulted. The copy
    // carries the cell content + light + fluid + stair facing
    // bit-for-bit, so the mesher's reads (block/fluid/light/stair) are
    // identical to the live world's; out-of-snapshot columns read the
    // same void an empty live world reads (the same World class).
    void insertSnapshotChunk(const ChunkPos& pos, const Chunk& chunk);
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
    // Chunks the fluid simulation may WALK this tick, per kind (PIPE-3):
    // the 99.8 activity flags' set twin. Every site that sets a chunk's
    // fluidHot / fluidWalkPending also inserts here (the six insertion
    // points are exhaustive over the flag-setting sites); the set may
    // conservatively over-insert (a setBlock on a dry chunk inserts its
    // target unconditionally — the walk is then a no-op), and set semantics
    // make re-insertion idempotent. The per-kind simulation pass iterates
    // THIS instead of the all-holding-chunks scan, double-checks the chunk
    // flags (belt: a stale entry with both flags false is consumed and
    // skipped), and the walk consumes the entry together with the flags
    // (consumeFluidActive). Purged by eraseChunk / clear.
    const std::set<ChunkPos>& fluidActiveChunks(FluidKind kind) const {
        return m_fluidActive[static_cast<std::size_t>(kind)];
    }
    // Consume the chunk's entry (the walk's flag-clearing site): a no-op
    // when absent (a chunk visited as a neighbor holds no entry).
    void consumeFluidActive(const ChunkPos& pos, FluidKind kind) {
        m_fluidActive[static_cast<std::size_t>(kind)].erase(pos);
    }
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
    // PIPE-2: installs the cell-mutation observer (the m_generator
    // callback paradigm; an empty observer = no notifications). Called
    // at the TOP of buildScene — before the save load's replay — so the
    // session's edit history (the save journal) is complete at exit.
    void setCellMutationObserver(CellMutationObserver observer) {
        m_observer = std::move(observer);
    }
    void clear(); // drop all chunks (the generator is kept)

private:
    void markNeighborChunksDirty(int lx, int ly, int lz, const ChunkPos& pos);
    // Fluid-set maintenance: add the chunk (it now holds the kind), or
    // prune it when the last cell of the kind left (a kVolume scan, only
    // on drains — rare).
    void addFluidChunk(const ChunkPos& pos, FluidKind kind);
    void pruneFluidChunk(const Chunk& chunk, const ChunkPos& pos,
                         FluidKind kind);
    void scanFluidChunks(Chunk& chunk, const ChunkPos& pos);
    // New-chunk fluid activity wake (the 99.8 streaming-boundary hole):
    // a chunk that just APPEARED changes what its fluid neighbors can do
    // — a spread proposal the void just rejected now has a solid target,
    // a generated sea meets a settled neighbor — but a settled (cold)
    // fluid chunk never re-heats on its own, so force one fluid-
    // simulation walk of every chunk among {self + the six face-sharing
    // neighbors} that exists and holds the kind (the setBlock UNSTICK
    // pass's mirror: there the block change is the event, here the
    // chunk's appearance is). Bounded cost: it fires only on chunk
    // creation (the streaming cadence — a few columns per frame), and a
    // settled walk re-reads its cells and re-goes-cold. Called by
    // getOrCreateChunk / insertChunk AFTER scanFluidChunks (so the new
    // chunk's own holding is already registered); insertSnapshotChunk is
    // excluded (the snapshot World's maintenance indexes are never read).
    void wakeFluidChunks(const ChunkPos& pos);

    std::unordered_map<ChunkPos, Chunk> m_chunks;
    // Chunks marked dirty (a mirror of each Chunk's own flag — the mesh
    // pass iterates this instead of every chunk).
    std::set<ChunkPos> m_dirtyChunks;
    // Indexed by FluidKind (Water = 1, Lava = 2; slot 0 unused).
    std::array<std::set<ChunkPos>, 4> m_fluidChunks;
    // Per-kind fluid-active chunk sets (PIPE-3; see fluidActiveChunks).
    std::array<std::set<ChunkPos>, 4> m_fluidActive {};
    ChunkGenerator m_generator;
    CellMutationObserver m_observer;
};

} // namespace mc
