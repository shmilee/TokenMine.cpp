// Coordinate types and conversions for the voxel world.
//
// Strict separation of the three coordinate spaces (a core project rule):
//   - world coords : BlockPos,  in block units, unbounded in X/Z,
//                    y in [0, kWorldHeight). Negative X/Z are legal.
//   - chunk coords : ChunkPos,  index of a kChunkSize^3 chunk in the grid.
//   - local coords : plain ints in [0, kChunkSize), relative to a chunk origin.
//
// All world<->chunk conversions go through this header; nothing else in the
// codebase does the arithmetic.
//
// Negative-coordinate correctness: C++ integer division truncates toward
// zero, so -1 / 16 == 0 — which would put block x=-1 into chunk 0. Chunk
// assignment must use floor division instead: floorDiv(-1, 16) == -1.
#pragma once

#include <cstddef>
#include <cstdint>

#include <unordered_map>

#include "block/block.hpp"

namespace mc {

inline constexpr int kChunkSize = 16;
// World is a slab: unlimited in X/Z, finite in Y.
inline constexpr int kWorldHeight = 128;

struct BlockPos {
    int x = 0;
    int y = 0;
    int z = 0;

    bool operator==(const BlockPos& o) const {
        return x == o.x && y == o.y && z == o.z;
    }
    bool operator!=(const BlockPos& o) const { return !(*this == o); }
    bool operator<(const BlockPos& o) const {
        if (x != o.x) {
            return x < o.x;
        }
        if (y != o.y) {
            return y < o.y;
        }
        return z < o.z;
    }
};

struct ChunkPos {
    int x = 0;
    int y = 0;
    int z = 0;

    bool operator==(const ChunkPos& o) const {
        return x == o.x && y == o.y && z == o.z;
    }
    bool operator!=(const ChunkPos& o) const { return !(*this == o); }
    bool operator<(const ChunkPos& o) const {
        if (x != o.x) {
            return x < o.x;
        }
        if (y != o.y) {
            return y < o.y;
        }
        return z < o.z;
    }
};

// Floor division for a positive divisor: the correct chunk index for
// negative coordinates. (b must be > 0.)
inline int floorDiv(int a, int b) {
    int q = a / b;
    int r = a % b;
    if (r < 0) {
        --q;
    }
    return q;
}

// World block -> the chunk that contains it.
inline ChunkPos worldToChunk(const BlockPos& p) {
    return {floorDiv(p.x, kChunkSize), floorDiv(p.y, kChunkSize),
            floorDiv(p.z, kChunkSize)};
}

// Chunk -> world position of its (0,0,0) corner.
inline BlockPos chunkOrigin(const ChunkPos& c) {
    return {c.x * kChunkSize, c.y * kChunkSize, c.z * kChunkSize};
}

// Local coords must be in [0, kChunkSize).
inline BlockPos chunkLocalToWorld(const ChunkPos& c, int lx, int ly, int lz) {
    const BlockPos o = chunkOrigin(c);
    return {o.x + lx, o.y + ly, o.z + lz};
}

// World -> local within chunk `c` (result in [0, kChunkSize) when `p` is
// inside the chunk). Subtraction is negative-safe; floorDiv is not needed
// here because the chunk is already known.
inline BlockPos worldToChunkLocal(const ChunkPos& c, const BlockPos& p) {
    const BlockPos o = chunkOrigin(c);
    return {p.x - o.x, p.y - o.y, p.z - o.z};
}

inline bool isWorldYValid(int y) { return y >= 0 && y < kWorldHeight; }

} // namespace mc

// -- std::hash specializations ------------------------------------------------
// Deterministic mixing (boost-style); quality is fine for a spatial map.
namespace std {

template <> struct hash<mc::ChunkPos> {
    std::size_t operator()(const mc::ChunkPos& p) const noexcept {
        std::size_t h = 0;
        const auto mix = [&h](int v) {
            h ^= std::hash<int> {}(v) + 0x9e3779b97f4a7c15ULL + (h << 6) +
                 (h >> 2);
        };
        mix(p.x);
        mix(p.y);
        mix(p.z);
        return h;
    }
};

template <> struct hash<mc::BlockPos> {
    std::size_t operator()(const mc::BlockPos& p) const noexcept {
        std::size_t h = 0;
        const auto mix = [&h](int v) {
            h ^= std::hash<int> {}(v) + 0x9e3779b97f4a7c15ULL + (h << 6) +
                 (h >> 2);
        };
        mix(p.x);
        mix(p.y);
        mix(p.z);
        return h;
    }
};

} // namespace std
