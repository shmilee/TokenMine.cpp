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
#include <optional>
#include <string>

#include "block/block.hpp"
#include "world/coordinates.hpp"

namespace mc {

// Fluid level range: 0 = no fluid, 1..7 = flowing, 8 = full/source.
// The project simulates TWO fluids (water and lava); the encoded array
// value separates them (below). A level > 0 means the cell holds that
// much fluid and no block (a block displaces fluid, see World).
inline constexpr std::uint8_t kMaxFluidLevel = 8;

// Fluid kind vocabulary (core, like the BlockID::air sentinel): the two
// simulated fluids, with the names data files carry (the block's 'fluid'
// property, the bucket item fields) and save files persist.
enum class FluidKind : std::uint8_t { Water = 1, Lava = 2 };

// Encoded fluid byte (Chunk::m_fluid): 0 = no fluid;
// 1..kMaxFluidLevel = water level 1..8;
// kMaxFluidLevel+1..2*kMaxFluidLevel = lava level 1..8. A cell holds at
// most ONE kind (World::setFluid replaces the other kind in the cell).
inline std::uint8_t fluidCode(FluidKind kind, std::uint8_t level) {
    return level == 0 ? 0
                      : (kind == FluidKind::Water ? level
                                                  : kMaxFluidLevel + level);
}
inline std::uint8_t fluidLevelOf(std::uint8_t code) {
    return code <= kMaxFluidLevel ? code : code - kMaxFluidLevel;
}
inline FluidKind fluidKindOf(std::uint8_t code) {
    return code <= kMaxFluidLevel ? FluidKind::Water : FluidKind::Lava;
}
inline const char* fluidKindName(FluidKind kind) {
    return kind == FluidKind::Water ? "water" : "lava";
}
inline std::optional<FluidKind> fluidKindForName(const std::string& name) {
    if (name == "water") {
        return FluidKind::Water;
    }
    if (name == "lava") {
        return FluidKind::Lava;
    }
    return std::nullopt;
}

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

    // -- Fluid (encoded byte, see fluidCode: 0 = dry, water 1..8, --------
    // lava 9..16; written by the fluid simulation, read by the
    // mesher/lighting/player). A fresh chunk is dry.
    std::uint8_t fluidAt(int x, int y, int z) const;
    void setFluidAt(int x, int y, int z, std::uint8_t code); // marks dirty
    void clearFluid();
    const std::uint8_t* fluidData() const { return m_fluid.data(); }

    // -- Fluid activity flags (Phase 99.8) -------------------------------
    // The fluid simulation walks a chunk only while it is HOT (one of its
    // cells' fluid changed on the last applied tick — a moving front, a
    // fresh source spreading) or PENDING (a block change under/beside it
    // may have unstuck settled fluid, e.g. support removed). A settled
    // fluid (a static pool, a static flow gradient) changes nothing, so
    // it goes cold and costs zero per tick — the Minecraft cell-queue
    // property at chunk granularity. The walk CONSUMES the hot flag
    // (changes applied after the walk re-heat via the next setFluid).
    bool fluidHot(FluidKind kind) const {
        return m_fluidHot[static_cast<std::size_t>(kind)];
    }
    void setFluidHot(FluidKind kind) {
        m_fluidHot[static_cast<std::size_t>(kind)] = true;
    }
    void clearFluidHot(FluidKind kind) {
        m_fluidHot[static_cast<std::size_t>(kind)] = false;
    }
    bool fluidWalkPending(FluidKind kind) const {
        return m_fluidWalkPending[static_cast<std::size_t>(kind)];
    }
    void clearFluidWalkPending(FluidKind kind) {
        m_fluidWalkPending[static_cast<std::size_t>(kind)] = false;
    }
    // Force one fluid-simulation walk of this chunk (even cold): a block
    // change may have unstuck settled fluid (support removed under a
    // sheet).
    void invalidateFluidWalk(FluidKind kind) {
        m_fluidWalkPending[static_cast<std::size_t>(kind)] = true;
    }

    // -- Per-block state (Phase 94 stair facing, generalized in ENT-2):
    // the per-instance state of a stateful block (a stair's orientation:
    // 0-3 north/east/south/west; any model.state_count block's state:
    // 0..count-1). This is the project's first PER-BLOCK STATE (a
    // breakthrough of the "no per-instance state" philosophy): the state
    // lives in the World (a per-cell byte), so the collision (Entity) +
    // the mesh (ChunkMesher) — which read World — can see it. A fresh cell
    // is kBlockStateUnset (255 — the value the stateCount domain
    // [1, 255] never reaches) — the collision / mesh / save derivation
    // fall back to the block's state DEFAULT (Block::stateDefault) when
    // unset. Written by the game (the setBlock choke point, the placement,
    // the rotation), read by the collision + mesh + save.
    static constexpr std::uint8_t kBlockStateUnset = 255;
    std::uint8_t stateAt(int x, int y, int z) const;
    void setState(int x, int y, int z, std::uint8_t state); // dirty
    void clearState();

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
    std::array<std::uint8_t, kVolume> m_fluid {}; // encoded (see fluidCode)
    std::array<std::uint8_t, kVolume> m_blockState {}; // Phase 94 / ENT-2
    bool m_dirty = true;
    // Fluid activity flags (Phase 99.8), indexed by FluidKind (slot 0
    // unused: Water = 1, Lava = 2).
    std::array<bool, 3> m_fluidHot {false, false, false};
    std::array<bool, 3> m_fluidWalkPending {false, false, false};
};

} // namespace mc
