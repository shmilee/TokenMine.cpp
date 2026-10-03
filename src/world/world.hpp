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
#include <functional>
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
    // is invalid. Marks the target chunk — and any neighbor chunk sharing
    // a face with the edited block — dirty.
    bool setBlock(int x, int y, int z, BlockID id);

    // Light queries (0..15), same coordinate conventions as getBlock.
    // Above the world (y >= kWorldHeight) and around the world (a chunk
    // that was never created) the void is open sky: sky 15, block 0.
    // Below the world (y < 0) both read 0.
    std::uint8_t getSkyLight(int x, int y, int z) const;
    std::uint8_t getBlockLight(int x, int y, int z) const;

    // Chunk access.
    Chunk& getOrCreateChunk(const ChunkPos& pos); // runs generator if new
    const Chunk* findChunk(const ChunkPos& pos) const;
    // Non-const access to an existing chunk WITHOUT creating it (the
    // lighting system writes light arrays into chunks it does not create).
    Chunk* findChunkMut(const ChunkPos& pos);
    bool hasChunk(const ChunkPos& pos) const;
    std::size_t chunkCount() const { return m_chunks.size(); }
    // Sorted, for deterministic iteration (rendering, saving, meshing).
    std::vector<ChunkPos> chunkPositions() const;

    void setChunkGenerator(ChunkGenerator gen) { m_generator = std::move(gen); }
    void clear(); // drop all chunks (the generator is kept)

private:
    void markNeighborChunksDirty(int lx, int ly, int lz, const ChunkPos& pos);

    std::unordered_map<ChunkPos, Chunk> m_chunks;
    ChunkGenerator m_generator;
};

} // namespace mc
