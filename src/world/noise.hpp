// Deterministic value noise (2D/3D) + fBm, built on a 64-bit integer hash.
//
// Everything here is a pure function of (coordinates, seed): no state, no
// allocation, no floating-point accumulation across calls. That makes
// world generation reproducible from a seed alone (a core Phase 4
// requirement) and thread-safe (Phase 14 can generate chunks in parallel).
//
// Determinism notes:
//  - lattice hashes use 64-bit integer arithmetic only (splitmix64 finalizer);
//  - interpolation uses float with a fixed evaluation order.
// Values are in [0, 1).
#pragma once

#include <cstdint>

namespace mc {

// splitmix64 finalizer: avalanche-mixes a 64-bit integer.
constexpr std::uint64_t splitmix64(std::uint64_t x) {
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
}

// Mixes two 64-bit values and a seed into a uniformly distributed 64-bit
// hash. Commutative in (a, b) is NOT required; (a, b) and (b, a) differ.
std::uint64_t hash2(std::uint64_t a, std::uint64_t b, std::uint64_t seed);
std::uint64_t hash3(std::uint64_t a, std::uint64_t b, std::uint64_t c,
                    std::uint64_t seed);

// 2D value noise: smooth interpolation of hashed lattice points.
// `x`, `y` may be any float (typically worldCoord * scale). Result [0, 1).
float valueNoise2(float x, float y, std::int64_t seed);

// 3D value noise. Result [0, 1).
float valueNoise3(float x, float y, float z, std::int64_t seed);

// Fractal Brownian motion over valueNoise2: `octaves` (1..32) layers with
// lacunarity 2 and gain 0.5, normalized back to [0, 1).
float fbm2(float x, float y, std::int64_t seed, int octaves = 4);

} // namespace mc
