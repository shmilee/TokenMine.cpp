// Chunk streaming (Phase 14, STRM-1): the pure decision logic for which
// chunk columns to generate as the player explores beyond the
// pre-generated area. No World, no threads — the Game wires the results
// into the thread pool and the World.
//
// A "column" is a vertical stack of the kVerticalChunksPerColumn chunks
// sharing one (x, z) chunk coordinate. Streaming loads whole columns so a
// terrain column either exists or it does not (no partial-column cliffs).
//
// The stream ring is a DISK (STRM-1): the columns whose squared
// Euclidean offset from the player's column is dx² + dz² <= radius² —
// not the old (2R+1)² Chebyshev square. The square wasted memory in the
// corners beyond the renderer's far plane (kFar = 1000 blocks); the Game
// clamps the radius to kStreamCapRadius (64), the exact disk that covers
// every potentially-renderable column (per-lattice-point derivation on
// the constant in core/game.cpp). All distances here are SQUARED and
// pure integer (no floating point) so the predicate stays deterministic
// and cheap.
//
// Hysteresis (see nearestMissingDistanceSq): the Game engages streaming
// when the player comes within kStreamTriggerDistance squared (1²) of a
// missing column inside the disk, and disengages only when the full
// disk around the player is loaded (the (radius+1)² sentinel).
// While engaged it keeps posting the nearest missing columns — so an
// exploring player ends up with the capped view_distance disk around
// them, while an idle player in the center of the loaded area (2
// columns from its edge) never engages. That keeps the initial area and
// every stationary run job-free, which keeps headless baselines
// byte-reproducible.
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace mc {

// A column address: (x, z) chunk coordinates (the vertical axis is
// implicit — a column spans the whole world height). Lexicographic
// (x, z) order for std::set/std::map.
struct ColumnPos {
    int x = 0;
    int z = 0;

    bool operator<(const ColumnPos& o) const {
        return x != o.x ? x < o.x : z < o.z;
    }
    bool operator==(const ColumnPos& o) const {
        return x == o.x && z == o.z;
    }
};

// Squared Euclidean distance between columns: dx² + dz² (pure integer —
// no floating point; comparing squares needs no sqrt).
std::int64_t columnDistanceSq(const ColumnPos& a, const ColumnPos& b);

// The "whole disk loaded" sentinel: (radius + 1)² — strictly greater
// than the largest in-disk squared distance (radius²), so it can never
// be confused with a real missing column.
std::int64_t missingSentinelDistanceSq(int radius);

// The columns to stream: every column inside `radius` (squared
// Euclidean, inclusive: dx² + dz² <= radius²) of `player` that is not in
// `loaded`, sorted by (distanceSq, x, z) — nearest first, deterministic.
std::vector<ColumnPos> missingColumns(const ColumnPos& player,
                                      const std::vector<ColumnPos>& loaded,
                                      int radius);

// The squared distance from `player` to the nearest column that is
// missing and inside the disk; missingSentinelDistanceSq(radius) when
// the whole disk is loaded. The hysteresis input: engage at distanceSq
// <= trigger², disengage at the sentinel.
std::int64_t nearestMissingDistanceSq(const ColumnPos& player,
                                      const std::vector<ColumnPos>& loaded,
                                      int radius);

// Exact number of columns in the disk of `radius`: the count of integer
// (dx, dz) pairs with dx² + dz² <= radius². The log "N/M" total and the
// memory-budget math (a radius-64 disk is 12853 columns).
std::size_t diskColumnCount(int radius);

// True when a not-yet-engaged streamer should engage: at least one
// missing column inside the disk is within `triggerDistance` squared
// (dx² + dz² <= triggerDistance²) of the player's column.
bool streamingActive(const ColumnPos& player,
                     const std::vector<ColumnPos>& loaded, int radius,
                     int triggerDistance);

} // namespace mc

// Deterministic mixing (the coordinates.hpp ChunkPos/BlockPos style).
namespace std {

template <> struct hash<mc::ColumnPos> {
    std::size_t operator()(const mc::ColumnPos& p) const noexcept {
        std::size_t h = 0;
        const auto mix = [&h](int v) {
            h ^= std::hash<int> {}(v) + 0x9e3779b97f4a7c15ULL + (h << 6) +
                 (h >> 2);
        };
        mix(p.x);
        mix(p.z);
        return h;
    }
};

} // namespace std
