// Chunk: a 16x16x16 block volume — the unit of storage, generation and
// meshing (Minecraft's "chunk section").
//
// Storage: a dense array of BlockID where id 0 (BlockID::air()) means
// "no block". Coordinates are local, in [0, kSize); all world<->local
// conversion lives in world/coordinates.hpp.
//
// A fresh chunk is dirty (never meshed). set()/fill() mark it dirty so
// the mesher rebuilds its GPU mesh.
#pragma once

#include <array>
#include <cstddef>

#include "block/block.hpp"
#include "world/coordinates.hpp"

namespace mc {

class Chunk {
public:
    static constexpr int kSize = kChunkSize; // 16
    static constexpr int kVolume = kSize * kSize * kSize;

    Chunk(); // filled with air, marked dirty

    // Local coordinates, each in [0, kSize). Out of range is a bug (assert).
    BlockID get(int x, int y, int z) const;
    void set(int x, int y, int z, BlockID id); // marks dirty

    void fill(BlockID id); // marks dirty
    void clear();          // all air, marks dirty

    bool dirty() const { return m_dirty; }
    void markDirty() { m_dirty = true; }
    void clearDirty() { m_dirty = false; }

    // Raw access for bulk readers (the mesher).
    const BlockID* data() const { return m_blocks.data(); }
    static std::size_t index(int x, int y, int z);

private:
    static bool inBounds(int c) { return c >= 0 && c < kSize; }

    std::array<BlockID, kVolume> m_blocks;
    bool m_dirty = true;
};

} // namespace mc
