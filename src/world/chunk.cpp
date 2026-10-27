#include "world/chunk.hpp"

#include <algorithm>
#include <cassert>

namespace mc {

Chunk::Chunk() { fill(BlockID::air()); }

BlockID Chunk::get(int x, int y, int z) const {
    assert(inBounds(x) && inBounds(y) && inBounds(z));
    return m_blocks[index(x, y, z)];
}

void Chunk::set(int x, int y, int z, BlockID id) {
    assert(inBounds(x) && inBounds(y) && inBounds(z));
    assert(id.valid());
    m_blocks[index(x, y, z)] = id;
    m_dirty = true;
}

void Chunk::fill(BlockID id) {
    std::fill(m_blocks.begin(), m_blocks.end(), id);
    clearFluid(); // a cell holds a block XOR fluid
    clearState(); // a fresh cell holds no per-instance state
    m_dirty = true;
}

void Chunk::clear() { fill(BlockID::air()); }

std::uint8_t Chunk::skyLightAt(int x, int y, int z) const {
    assert(inBounds(x) && inBounds(y) && inBounds(z));
    return m_skyLight[index(x, y, z)];
}

std::uint8_t Chunk::blockLightAt(int x, int y, int z) const {
    assert(inBounds(x) && inBounds(y) && inBounds(z));
    return m_blockLight[index(x, y, z)];
}

void Chunk::setSkyLightAt(int x, int y, int z, std::uint8_t value) {
    assert(inBounds(x) && inBounds(y) && inBounds(z));
    m_skyLight[index(x, y, z)] = value;
}

void Chunk::setBlockLightAt(int x, int y, int z, std::uint8_t value) {
    assert(inBounds(x) && inBounds(y) && inBounds(z));
    m_blockLight[index(x, y, z)] = value;
}

void Chunk::clearSkyLight() { std::fill(m_skyLight.begin(), m_skyLight.end(), 0); }

void Chunk::clearBlockLight() {
    std::fill(m_blockLight.begin(), m_blockLight.end(), 0);
}

std::uint8_t Chunk::fluidAt(int x, int y, int z) const {
    assert(inBounds(x) && inBounds(y) && inBounds(z));
    return m_fluid[index(x, y, z)];
}

void Chunk::setFluidAt(int x, int y, int z, std::uint8_t code) {
    assert(inBounds(x) && inBounds(y) && inBounds(z));
    assert(code <= 2 * kMaxFluidLevel); // encoded: water 1..8, lava 9..16
    const std::uint32_t i = static_cast<std::uint32_t>(index(x, y, z));
    const std::uint8_t old = m_fluid[i];
    m_fluid[i] = code;
    m_dirty = true;
    // Fluid activity (Phase 99.8): a cell change heats BOTH the old and
    // the new kind (the neighbor flows of either may change next tick).
    if (code != old) {
        if (old != 0) {
            m_fluidHot[static_cast<std::size_t>(fluidKindOf(old))] = true;
        }
        if (code != 0) {
            m_fluidHot[static_cast<std::size_t>(fluidKindOf(code))] = true;
        }
        // Cell list (PIPE-3): push exactly when the cell newly BECOMES a
        // member of the kind (0 -> n and a kind change). A same-kind level
        // change keeps its existing entry; a drain removes none (the stale
        // entry stays until the walk's filter / compaction — it may already
        // be gone after a compaction, which is a no-op). Invariant: a live
        // fluid cell => its kind's list holds at least one entry (possibly
        // duplicated or stale-mixed).
        if (code != 0 &&
            (old == 0 || fluidKindOf(old) != fluidKindOf(code))) {
            m_fluidCells[static_cast<std::size_t>(fluidKindOf(code))]
                .push_back(i);
        }
    }
}

void Chunk::clearFluid() {
    std::fill(m_fluid.begin(), m_fluid.end(), 0);
    m_fluidHot = {true, true, true}; // everything that flowed is stale
    // A reset chunk holds no fluid: the cell lists go with it (they must
    // not carry entries from the chunk's previous content — the streaming
    // scratch regeneration path relies on the reset).
    for (std::vector<std::uint32_t>& v : m_fluidCells) {
        v.clear();
    }
}

std::uint8_t Chunk::stateAt(int x, int y, int z) const {
    assert(inBounds(x) && inBounds(y) && inBounds(z));
    return m_blockState[index(x, y, z)];
}

void Chunk::setState(int x, int y, int z, std::uint8_t state) {
    assert(inBounds(x) && inBounds(y) && inBounds(z));
    assert(state != kBlockStateUnset); // the sentinel is the UNSET value
    m_blockState[index(x, y, z)] = state;
    m_dirty = true;
}

void Chunk::clearState() {
    // A fresh cell holds no per-instance state (the sentinel). The
    // collision / mesh / save derivation fall back to the block's state
    // default (Block::stateDefault) when unset.
    std::fill(m_blockState.begin(), m_blockState.end(), kBlockStateUnset);
}

std::size_t Chunk::index(int x, int y, int z) {
    return (static_cast<std::size_t>(y) * kSize + static_cast<std::size_t>(z)) *
               kSize +
           static_cast<std::size_t>(x);
}

} // namespace mc
