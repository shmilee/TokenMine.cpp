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

std::size_t Chunk::index(int x, int y, int z) {
    return (static_cast<std::size_t>(y) * kSize + static_cast<std::size_t>(z)) *
               kSize +
           static_cast<std::size_t>(x);
}

} // namespace mc
