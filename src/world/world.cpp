#include "world/world.hpp"

#include <algorithm>

#include "world/lighting.hpp" // kMaxLight

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

std::uint8_t World::getSkyLight(int x, int y, int z) const {
    if (y < 0) {
        return 0; // below the world: nothing to light
    }
    if (y >= kWorldHeight) {
        return kMaxLight; // open sky above the world
    }
    const BlockPos p{x, y, z};
    const ChunkPos cp = worldToChunk(p);
    const auto it = m_chunks.find(cp);
    if (it == m_chunks.end()) {
        return kMaxLight; // void is open sky
    }
    const BlockPos local = worldToChunkLocal(cp, p);
    return it->second.skyLightAt(local.x, local.y, local.z);
}

std::uint8_t World::getBlockLight(int x, int y, int z) const {
    if (!isWorldYValid(y)) {
        return 0;
    }
    const BlockPos p{x, y, z};
    const ChunkPos cp = worldToChunk(p);
    const auto it = m_chunks.find(cp);
    if (it == m_chunks.end()) {
        return 0; // void emits no block light
    }
    const BlockPos local = worldToChunkLocal(cp, p);
    return it->second.blockLightAt(local.x, local.y, local.z);
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
    // A block displaces any fluid in the cell (placing into water).
    if (chunk.fluidAt(local.x, local.y, local.z) != 0) {
        chunk.setFluidAt(local.x, local.y, local.z, 0);
    }
    markNeighborChunksDirty(local.x, local.y, local.z, cp);
    return true;
}

std::uint8_t World::getFluid(int x, int y, int z) const {
    if (!isWorldYValid(y)) {
        return 0;
    }
    const BlockPos p{x, y, z};
    const ChunkPos cp = worldToChunk(p);
    const auto it = m_chunks.find(cp);
    if (it == m_chunks.end()) {
        return 0; // void is dry
    }
    const BlockPos local = worldToChunkLocal(cp, p);
    return fluidLevelOf(it->second.fluidAt(local.x, local.y, local.z));
}

std::uint8_t World::getFluidOfKind(int x, int y, int z,
                                   FluidKind kind) const {
    if (!isWorldYValid(y)) {
        return 0;
    }
    const BlockPos p{x, y, z};
    const ChunkPos cp = worldToChunk(p);
    const auto it = m_chunks.find(cp);
    if (it == m_chunks.end()) {
        return 0; // void is dry
    }
    const BlockPos local = worldToChunkLocal(cp, p);
    const std::uint8_t code = it->second.fluidAt(local.x, local.y, local.z);
    return fluidKindOf(code) == kind ? fluidLevelOf(code) : 0;
}

std::optional<FluidKind> World::getFluidKind(int x, int y, int z) const {
    if (!isWorldYValid(y)) {
        return std::nullopt;
    }
    const BlockPos p{x, y, z};
    const ChunkPos cp = worldToChunk(p);
    const auto it = m_chunks.find(cp);
    if (it == m_chunks.end()) {
        return std::nullopt; // void is dry
    }
    const BlockPos local = worldToChunkLocal(cp, p);
    const std::uint8_t code = it->second.fluidAt(local.x, local.y, local.z);
    return code == 0 ? std::optional<FluidKind>()
                     : std::optional<FluidKind>(fluidKindOf(code));
}

bool World::setFluid(int x, int y, int z, FluidKind kind,
                     std::uint8_t level) {
    if (!isWorldYValid(y) || level > kMaxFluidLevel) {
        return false;
    }
    const BlockPos p{x, y, z};
    const ChunkPos cp = worldToChunk(p);
    Chunk* chunk = findChunkMut(cp);
    if (chunk == nullptr) {
        return false; // no chunk: the void stays dry (never created here)
    }
    const BlockPos local = worldToChunkLocal(cp, p);
    const std::uint8_t code = chunk->fluidAt(local.x, local.y, local.z);
    if (level == 0) {
        // A zero level clears ONLY this kind (the other kind, if any,
        // stays). A no-op when this kind is not present at a positive
        // level.
        if (code == 0 || fluidKindOf(code) != kind) {
            return false;
        }
    } else {
        if (chunk->get(local.x, local.y, local.z) != BlockID::air()) {
            return false; // a block occupies the cell
        }
        if (fluidKindOf(code) == kind && fluidLevelOf(code) == level) {
            return false; // this kind's level is unchanged
        }
        // A positive level replaces any other kind in the cell.
    }
    chunk->setFluidAt(local.x, local.y, local.z, fluidCode(kind, level));
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

bool World::insertChunk(const ChunkPos& pos, Chunk&& chunk) {
    auto result = m_chunks.emplace(pos, std::move(chunk));
    if (!result.second) {
        return false; // the chunk already exists; nothing was inserted
    }
    // The chunk (a fresh Chunk is dirty by construction) and every
    // face-sharing existing neighbor need a re-mesh: the neighbors'
    // boundary faces were decided against the void before the new chunk
    // existed.
    const auto mark = [this](const ChunkPos& p) {
        auto it = m_chunks.find(p);
        if (it != m_chunks.end()) {
            it->second.markDirty();
        }
    };
    mark({pos.x - 1, pos.y, pos.z});
    mark({pos.x + 1, pos.y, pos.z});
    mark({pos.x, pos.y - 1, pos.z});
    mark({pos.x, pos.y + 1, pos.z});
    mark({pos.x, pos.y, pos.z - 1});
    mark({pos.x, pos.y, pos.z + 1});
    return true;
}

const Chunk* World::findChunk(const ChunkPos& pos) const {
    const auto it = m_chunks.find(pos);
    return it == m_chunks.end() ? nullptr : &it->second;
}

Chunk* World::findChunkMut(const ChunkPos& pos) {
    auto it = m_chunks.find(pos);
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
