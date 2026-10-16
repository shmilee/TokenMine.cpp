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
    const std::uint8_t displaced = chunk.fluidAt(local.x, local.y, local.z);
    if (displaced != 0) {
        chunk.setFluidAt(local.x, local.y, local.z, 0);
        pruneFluidChunk(chunk, cp, fluidKindOf(displaced));
    }
    // A block change can UNSTICK fluid: support removed under a settled
    // (cold) sheet must still fall — force the target and its six
    // face-sharing chunks (the ones that hold fluid) through one
    // fluid-simulation walk on the next tick (Phase 99.8 activity gating).
    for (FluidKind kind : {FluidKind::Water, FluidKind::Lava}) {
        chunk.invalidateFluidWalk(kind);
        const auto invalidate = [this, kind](const ChunkPos& q) {
            if (m_fluidChunks[static_cast<std::size_t>(kind)].count(q) != 0) {
                auto it = m_chunks.find(q);
                if (it != m_chunks.end()) {
                    it->second.invalidateFluidWalk(kind);
                }
            }
        };
        invalidate({cp.x - 1, cp.y, cp.z});
        invalidate({cp.x + 1, cp.y, cp.z});
        invalidate({cp.x, cp.y - 1, cp.z});
        invalidate({cp.x, cp.y + 1, cp.z});
        invalidate({cp.x, cp.y, cp.z - 1});
        invalidate({cp.x, cp.y, cp.z + 1});
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
    if (level > 0) {
        addFluidChunk(cp, kind);
    } else {
        pruneFluidChunk(*chunk, cp, kind);
    }
    markNeighborChunksDirty(local.x, local.y, local.z, cp);
    return true;
}

std::uint8_t World::getStairFacing(int x, int y, int z) const {
    if (!isWorldYValid(y)) {
        return Chunk::kStairFacingUnset; // the void reads "unset"
    }
    const BlockPos p{x, y, z};
    const ChunkPos cp = worldToChunk(p);
    const auto it = m_chunks.find(cp);
    if (it == m_chunks.end()) {
        return Chunk::kStairFacingUnset; // uncreated reads "unset"
    }
    const BlockPos local = worldToChunkLocal(cp, p);
    return it->second.stairFacingAt(local.x, local.y, local.z);
}

bool World::setStairFacing(int x, int y, int z, std::uint8_t facing) {
    if (!isWorldYValid(y) || facing > 3) {
        return false;
    }
    const BlockPos p{x, y, z};
    const ChunkPos cp = worldToChunk(p);
    Chunk* chunk = findChunkMut(cp);
    if (chunk == nullptr) {
        return false; // no chunk (the void is never created here)
    }
    const BlockPos local = worldToChunkLocal(cp, p);
    if (chunk->stairFacingAt(local.x, local.y, local.z) == facing) {
        return false; // unchanged
    }
    chunk->setStairFacing(local.x, local.y, local.z, facing);
    markNeighborChunksDirty(local.x, local.y, local.z, cp);
    return true;
}

Chunk& World::getOrCreateChunk(const ChunkPos& pos) {
    auto it = m_chunks.find(pos);
    if (it != m_chunks.end()) {
        return it->second;
    }
    Chunk& chunk = m_chunks.emplace(pos, Chunk()).first->second;
    // A fresh chunk is dirty by construction: the mesh pass must visit it.
    m_dirtyChunks.insert(pos);
    if (m_generator) {
        m_generator(chunk, pos);
    }
    // Generated terrain can carry fluid (sea water, lava lakes):
    // register the new chunk for the fluid simulation's chunk sets.
    scanFluidChunks(chunk, pos);
    return chunk;
}

bool World::insertChunk(const ChunkPos& pos, Chunk&& chunk) {
    auto result = m_chunks.emplace(pos, std::move(chunk));
    if (!result.second) {
        return false; // the chunk already exists; nothing was inserted
    }
    // The incoming chunk may carry fluid (generated sea water, a lava
    // lake): register it for the fluid simulation's chunk sets.
    scanFluidChunks(result.first->second, pos);
    // The chunk (a fresh Chunk is dirty by construction) and every
    // face-sharing existing neighbor need a re-mesh: the neighbors'
    // boundary faces were decided against the void before the new chunk
    // existed.
    m_dirtyChunks.insert(pos);
    const auto mark = [this](const ChunkPos& p) {
        auto it = m_chunks.find(p);
        if (it != m_chunks.end()) {
            it->second.markDirty();
            m_dirtyChunks.insert(p);
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

bool World::eraseChunk(const ChunkPos& pos) {
    auto it = m_chunks.find(pos);
    if (it == m_chunks.end()) {
        return false; // nothing to drop
    }
    // The chunk leaves the fluid simulation's chunk sets (its fluid, if
    // any, is dropped with it).
    m_fluidChunks[static_cast<std::size_t>(FluidKind::Water)].erase(pos);
    m_fluidChunks[static_cast<std::size_t>(FluidKind::Lava)].erase(pos);
    // The six face-sharing SURVIVING neighbors re-expose their boundary
    // faces (the erased chunk now reads as the air void): re-mesh them.
    const auto mark = [this](const ChunkPos& p) {
        auto jt = m_chunks.find(p);
        if (jt != m_chunks.end()) {
            jt->second.markDirty();
            m_dirtyChunks.insert(p);
        }
    };
    mark({pos.x - 1, pos.y, pos.z});
    mark({pos.x + 1, pos.y, pos.z});
    mark({pos.x, pos.y - 1, pos.z});
    mark({pos.x, pos.y + 1, pos.z});
    mark({pos.x, pos.y, pos.z - 1});
    mark({pos.x, pos.y, pos.z + 1});
    m_chunks.erase(it);
    m_dirtyChunks.erase(pos);
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

bool World::columnComplete(int cx, int cz) const {
    constexpr int kVertical = kWorldHeight / kChunkSize;
    for (int cy = 0; cy < kVertical; ++cy) {
        if (m_chunks.find({cx, cy, cz}) == m_chunks.end()) {
            return false;
        }
    }
    return true;
}

void World::ensureColumn(int cx, int cz) {
    constexpr int kVertical = kWorldHeight / kChunkSize;
    for (int cy = 0; cy < kVertical; ++cy) {
        getOrCreateChunk({cx, cy, cz});
    }
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

void World::clear() {
    m_chunks.clear();
    m_fluidChunks = {};
    m_dirtyChunks.clear();
}

void World::clearChunkDirty(const ChunkPos& pos) {
    auto it = m_chunks.find(pos);
    if (it != m_chunks.end()) {
        it->second.clearDirty();
    }
    m_dirtyChunks.erase(pos);
}

void World::markChunkDirty(const ChunkPos& pos) {
    auto it = m_chunks.find(pos);
    if (it == m_chunks.end()) {
        return; // a missing chunk is not meshed; nothing to flag
    }
    it->second.markDirty();
    m_dirtyChunks.insert(pos);
}

std::vector<ChunkPos> World::fluidChunkPositions(FluidKind kind) const {
    const std::set<ChunkPos>& s =
        m_fluidChunks[static_cast<std::size_t>(kind)];
    return {s.begin(), s.end()};
}

void World::addFluidChunk(const ChunkPos& pos, FluidKind kind) {
    m_fluidChunks[static_cast<std::size_t>(kind)].insert(pos);
}

void World::pruneFluidChunk(const Chunk& chunk, const ChunkPos& pos,
                            FluidKind kind) {
    // The cell that just lost the kind is known (the caller drained it);
    // the question is whether ANY other cell still holds it. A kVolume
    // scan on drains only (rare; a few microseconds).
    const std::uint8_t* fluid = chunk.fluidData();
    for (int i = 0; i < Chunk::kVolume; ++i) {
        if (fluidKindOf(fluid[i]) == kind && fluidLevelOf(fluid[i]) > 0) {
            return; // the chunk still holds the kind
        }
    }
    m_fluidChunks[static_cast<std::size_t>(kind)].erase(pos);
}

void World::scanFluidChunks(Chunk& chunk, const ChunkPos& pos) {
    const std::uint8_t* fluid = chunk.fluidData();
    bool water = false;
    bool lava = false;
    for (int i = 0; i < Chunk::kVolume; ++i) {
        if (fluid[i] == 0) {
            continue;
        }
        if (fluidKindOf(fluid[i]) == FluidKind::Water) {
            water = true;
        } else {
            lava = true;
        }
    }
    if (water) {
        addFluidChunk(pos, FluidKind::Water);
        chunk.setFluidHot(FluidKind::Water); // never simulated: first tick walks it
    }
    if (lava) {
        addFluidChunk(pos, FluidKind::Lava);
        chunk.setFluidHot(FluidKind::Lava); // never simulated: first tick walks it
    }
}

// A block sitting on a chunk face shares that face with the neighbor
// chunk; the neighbor's mesh may have culling decisions that now change.
void World::markNeighborChunksDirty(int lx, int ly, int lz,
                                    const ChunkPos& pos) {
    // The edited chunk itself (every caller wrote to it first) plus the
    // face-sharing neighbors on chunk edges — all into the dirty set the
    // mesh pass iterates.
    m_dirtyChunks.insert(pos);
    const auto mark = [this](const ChunkPos& p) {
        auto it = m_chunks.find(p);
        if (it != m_chunks.end()) {
            it->second.markDirty();
            m_dirtyChunks.insert(p);
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
