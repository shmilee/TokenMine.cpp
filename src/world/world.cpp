#include "world/world.hpp"

#include <algorithm>

namespace mc {

BlockID World::getBlock(int x, int y, int z) const {
    if (!isWorldYValid(y)) {
        return BlockID::air();
    }
    const BlockPos p{x, y, z};
    const ChunkPos cp = worldToChunk(p);
    const auto it = m_chunks.find(cp);
    if (it == m_chunks.end()) {
        return BlockID::air();
    }
    const BlockPos local = worldToChunkLocal(cp, p);
    return it->second.get(local.x, local.y, local.z);
}

bool World::setBlock(int x, int y, int z, BlockID id) {
    if (!isWorldYValid(y) || !id.valid()) {
        return false;
    }
    const BlockPos p{x, y, z};
    const ChunkPos cp = worldToChunk(p);
    Chunk& chunk = getOrCreateChunk(cp);
    const BlockPos local = worldToChunkLocal(cp, p);
    chunk.set(local.x, local.y, local.z, id);
    markNeighborChunksDirty(local.x, local.y, local.z, cp);
    return true;
}

Chunk& World::getOrCreateChunk(const ChunkPos& pos) {
    auto it = m_chunks.find(pos);
    if (it != m_chunks.end()) {
        return it->second;
    }
    Chunk& chunk = m_chunks.emplace(pos, Chunk()).first->second;
    if (m_generator) {
        m_generator(chunk, pos);
    }
    return chunk;
}

const Chunk* World::findChunk(const ChunkPos& pos) const {
    const auto it = m_chunks.find(pos);
    return it == m_chunks.end() ? nullptr : &it->second;
}

bool World::hasChunk(const ChunkPos& pos) const {
    return m_chunks.count(pos) != 0;
}

std::vector<ChunkPos> World::chunkPositions() const {
    std::vector<ChunkPos> positions;
    positions.reserve(m_chunks.size());
    for (const auto& [pos, chunk] : m_chunks) {
        positions.push_back(pos);
    }
    std::sort(positions.begin(), positions.end());
    return positions;
}

void World::clear() { m_chunks.clear(); }

// A block sitting on a chunk face shares that face with the neighbor
// chunk; the neighbor's mesh may have culling decisions that now change.
void World::markNeighborChunksDirty(int lx, int ly, int lz,
                                    const ChunkPos& pos) {
    const auto mark = [this](const ChunkPos& p) {
        auto it = m_chunks.find(p);
        if (it != m_chunks.end()) {
            it->second.markDirty();
        }
    };
    const int edge = kChunkSize - 1;
    if (lx == 0) {
        mark({pos.x - 1, pos.y, pos.z});
    }
    if (lx == edge) {
        mark({pos.x + 1, pos.y, pos.z});
    }
    if (ly == 0) {
        mark({pos.x, pos.y - 1, pos.z});
    }
    if (ly == edge) {
        mark({pos.x, pos.y + 1, pos.z});
    }
    if (lz == 0) {
        mark({pos.x, pos.y, pos.z - 1});
    }
    if (lz == edge) {
        mark({pos.x, pos.y, pos.z + 1});
    }
}

} // namespace mc
