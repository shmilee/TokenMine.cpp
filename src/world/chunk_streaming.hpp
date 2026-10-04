// Chunk streaming (Phase 14): the pure decision logic for which chunk
// columns to generate as the player explores beyond the pre-generated
// area. No World, no threads — the Game wires the results into the
// thread pool and the World.
//
// A "column" is a vertical stack of the kVerticalChunksPerColumn chunks
// sharing one (x, z) chunk coordinate. Streaming loads whole columns so a
// terrain column either exists or it does not (no partial-column cliffs).
//
// Hysteresis (see nearestMissingDistance): the Game engages streaming
// when the player comes within kStreamTriggerDistance (1) of a missing
// column inside the radius ring, and disengages only when the full
// radius ring around the player is loaded (the distance sentinel).
// While engaged it keeps posting the nearest missing columns — so an
// exploring player ends up with the view_distance ring around them,
// while an idle player in the center of the loaded area (2 columns from
// its edge) never engages. That keeps the initial area and every
// stationary run job-free, which keeps headless baselines
// byte-reproducible.
#pragma once

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

// Chebyshev distance between columns (square rings).
int columnDistance(const ColumnPos& a, const ColumnPos& b);

// The columns to stream: every column inside `radius` (Chebyshev,
// inclusive) of `player` that is not in `loaded`, sorted by
// (distance, x, z) — nearest first, deterministic.
std::vector<ColumnPos> missingColumns(const ColumnPos& player,
                                      const std::vector<ColumnPos>& loaded,
                                      int radius);

// The Chebyshev distance from `player` to the nearest column that is
// missing and inside the radius ring; radius + 1 (the sentinel) when the
// whole ring is loaded. The hysteresis input: engage at distance <=
// kStreamTriggerDistance, disengage at the sentinel.
int nearestMissingDistance(const ColumnPos& player,
                           const std::vector<ColumnPos>& loaded, int radius);

// True when a not-yet-engaged streamer should engage: at least one
// missing column inside the radius ring is within `triggerDistance`
// (Chebyshev, inclusive) of the player's column.
bool streamingActive(const ColumnPos& player,
                     const std::vector<ColumnPos>& loaded, int radius,
                     int triggerDistance);

} // namespace mc
