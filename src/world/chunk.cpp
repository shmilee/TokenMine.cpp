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

std::size_t Chunk::index(int x, int y, int z) {
    return (static_cast<std::size_t>(y) * kSize + static_cast<std::size_t>(z)) *
               kSize +
           static_cast<std::size_t>(x);
}

} // namespace mc
