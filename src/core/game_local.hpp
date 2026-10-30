// Private shared constants/helpers for the game_*.cpp TU family (UX-2 A1).
//
// game.cpp was split into game.cpp / game_init.cpp / game_streaming.cpp
// / game_save.cpp; the file-local items used by >= 2 of those translation
// units live here. They sit in the plain (NOT anonymous) namespace mc so a
// TU that includes this header and uses only some of the items cannot trip
// -Wunused-function under -Werror. Only the game_*.cpp TUs may include
// this header (private; never reference it from anywhere else).

#pragma once

#include <SDL3/SDL.h> // nowSeconds

#include "core/math/mat4.hpp" // Mat4
#include "world/chunk_streaming.hpp" // ColumnPos
#include "world/coordinates.hpp" // kChunkSize

namespace mc {

// Sky objects (sun/moon/stars) sit this far from the world origin — inside
// the far plane (kFar) so they are never clipped.
constexpr double kSkyDistance = 800.0;

// Demo area: 4x4 chunk columns covering world x,z in [-32, 32).
constexpr int kDemoChunkMin = -2;
constexpr int kDemoChunkMax = 1;

// Far-plane cap on the stream disk radius (STRM-1): the effective radius
// is min(config world.view_distance, kStreamCapRadius). The cap is a
// property of the renderer (kFar), not of the user's view — no config
// key.
//
// Derivation (per-lattice-point check against kFar = 1000 blocks = 62.5
// columns): in a quadrant, a column at integer offset (dx, dz) (both >=
// 1) is potentially renderable iff its nearest corner can reach the eye
// — worst case the eye at the corner of its own column facing the target
// — 16*sqrt((dx-1)² + (dz-1)²) < kFar, i.e. (dx-1)² + (dz-1)² < 62.5².
// Verified over the lattice: every potentially-renderable column has
// d = sqrt(dx²+dz²) < 64 (continuous maximum sqrt(2) + 62.5 = 63.91;
// lattice maximum 63.89, the (41, 49) family), and every column with
// d > 64 keeps its nearest corner more than kFar from the eye (closest:
// 1002.01 blocks, the (40, 50) family) — it can never be drawn, so
// streaming it is pure memory waste. Cap 64: a radius-80 config streams
// the 12853-column disk instead of the old 25921-column square.
constexpr int kStreamCapRadius = 64;

constexpr int kCrackStages = 5;

inline double nowSeconds() {
    return static_cast<double>(SDL_GetPerformanceCounter()) /
           SDL_GetPerformanceFrequency();
}

// The column's model matrix: vertex positions bake each chunk's LAYER
// offset (y), so the model translates the column origin in X/Z only
// (baking X/Z into the vertices as well would double-apply it).
inline Mat4 columnModel(const ColumnPos& col) {
    return Mat4::translate({static_cast<float>(col.x * kChunkSize), 0.0f,
                            static_cast<float>(col.z * kChunkSize)});
}

} // namespace mc
