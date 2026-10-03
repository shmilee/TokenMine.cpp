// Chunk: a 16x16x16 block volume — the unit of storage, generation and
// meshing (Minecraft's "chunk section").
//
// Storage: a dense array of BlockID where id 0 (BlockID::air()) means
// "no block", two light arrays (sky and block light, 0..15 per cell) and a
// fluid level array (0..kMaxFluidLevel per cell; see World::setFluid).
// Coordinates are local, in [0, kSize); all world<->local
// conversion lives in world/coordinates.hpp.
//
// A fresh chunk is dirty (never meshed). set()/fill() mark it dirty so
// the mesher rebuilds its GPU mesh. Light values are computed by the
// lighting system (world/lighting.hpp); a fresh chunk reads as 0/0 until
// propagated.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "block/block.hpp"
#include "world/coordinates.hpp"

namespace mc {

// Fluid level range: 0 = no fluid, 1..7 = flowing, 8 = full/source.
// The game simulates one fluid (water); a level > 0 means the cell holds
// that much fluid and no block (a block displaces fluid, see World).
inline constexpr std::uint8_t kMaxFluidLevel = 8;

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

    // -- Light (0..15, written by the lighting system, read by the mesher) --
    std::uint8_t skyLightAt(int x, int y, int z) const;
    std::uint8_t blockLightAt(int x, int y, int z) const;
    void setSkyLightAt(int x, int y, int z, std::uint8_t value);
    void setBlockLightAt(int x, int y, int z, std::uint8_t value);
    void clearSkyLight();
    void clearBlockLight();

    // -- Fluid (0..kMaxFluidLevel; written by the fluid simulation, --------
    // read by the mesher/lighting/player). A fresh chunk is dry.
    std::uint8_t fluidAt(int x, int y, int z) const;
    void setFluidAt(int x, int y, int z, std::uint8_t level); // marks dirty
    void clearFluid();
    const std::uint8_t* fluidData() const { return m_fluid.data(); }

    // Raw array access (kVolume bytes each) for bulk readers/comparators.
    const std::uint8_t* skyLightData() const { return m_skyLight.data(); }
    const std::uint8_t* blockLightData() const { return m_blockLight.data(); }

    // Raw access for bulk readers (the mesher).
    const BlockID* data() const { return m_blocks.data(); }
    static std::size_t index(int x, int y, int z);

private:
    static bool inBounds(int c) { return c >= 0 && c < kSize; }

    std::array<BlockID, kVolume> m_blocks;
    std::array<std::uint8_t, kVolume> m_skyLight {};
    std::array<std::uint8_t, kVolume> m_blockLight {};
    std::array<std::uint8_t, kVolume> m_fluid {};
    bool m_dirty = true;
};

} // namespace mc
