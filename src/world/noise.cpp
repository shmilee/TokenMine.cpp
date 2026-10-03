#include "world/noise.hpp"

#include <cmath>

namespace mc {

std::uint64_t hash2(std::uint64_t a, std::uint64_t b, std::uint64_t seed) {
    // Perturb the coordinates so that (a, b, seed) permutations cannot
    // collide on the first mix; then avalanche the whole state.
    std::uint64_t h = seed;
    h ^= splitmix64(a + 0x9e3779b97f4a7c15ULL);
    h = (h + 0xbf58476d1ce4e5b9ULL) ^
         (splitmix64(b + 0x94d049bb133111ebULL) >> 1);
    return splitmix64(h);
}

std::uint64_t hash3(std::uint64_t a, std::uint64_t b, std::uint64_t c,
                    std::uint64_t seed) {
    std::uint64_t h = seed;
    h ^= splitmix64(a + 0x9e3779b97f4a7c15ULL);
    h = (h + 0xbf58476d1ce4e5b9ULL) ^
         (splitmix64(b + 0x94d049bb133111ebULL) >> 1);
    h = (h + 0x27d4eb2f165667c5ULL) ^
         (splitmix64(c + 0x2545f4914f6cdd1dULL) >> 1);
    return splitmix64(h);
}

namespace {

// Coordinates are hashed as unsigned 64-bit. Negative values are fine:
// two's-complement reinterpretation is a bijection.
inline std::uint64_t asU64(int v) {
    return static_cast<std::uint64_t>(static_cast<std::int64_t>(v));
}

inline float smooth(float t) { return t * t * (3.0f - 2.0f * t); }

// 24-bit lattice value in [0, 1).
inline float lattice2(int x, int y, std::int64_t seed) {
    const std::uint64_t h = hash2(asU64(x), asU64(y),
                                  static_cast<std::uint64_t>(seed));
    return static_cast<float>(h >> 40) * (1.0f / 16777216.0f);
}

inline float lattice3(int x, int y, int z, std::int64_t seed) {
    const std::uint64_t h =
        hash3(asU64(x), asU64(y), asU64(z), static_cast<std::uint64_t>(seed));
    return static_cast<float>(h >> 40) * (1.0f / 16777216.0f);
}

} // namespace

float valueNoise2(float x, float y, std::int64_t seed) {
    const int xi = static_cast<int>(std::floorf(x));
    const int yi = static_cast<int>(std::floorf(y));
    const float tx = smooth(x - static_cast<float>(xi));
    const float ty = smooth(y - static_cast<float>(yi));

    const float v00 = lattice2(xi, yi, seed);
    const float v10 = lattice2(xi + 1, yi, seed);
    const float v01 = lattice2(xi, yi + 1, seed);
    const float v11 = lattice2(xi + 1, yi + 1, seed);

    const float a = v00 + (v10 - v00) * tx;
    const float b = v01 + (v11 - v01) * tx;
    return a + (b - a) * ty;
}

float valueNoise3(float x, float y, float z, std::int64_t seed) {
    const int xi = static_cast<int>(std::floorf(x));
    const int yi = static_cast<int>(std::floorf(y));
    const int zi = static_cast<int>(std::floorf(z));
    const float tx = smooth(x - static_cast<float>(xi));
    const float ty = smooth(y - static_cast<float>(yi));
    const float tz = smooth(z - static_cast<float>(zi));

    const float v000 = lattice3(xi, yi, zi, seed);
    const float v100 = lattice3(xi + 1, yi, zi, seed);
    const float v010 = lattice3(xi, yi + 1, zi, seed);
    const float v110 = lattice3(xi + 1, yi + 1, zi, seed);
    const float v001 = lattice3(xi, yi, zi + 1, seed);
    const float v101 = lattice3(xi + 1, yi, zi + 1, seed);
    const float v011 = lattice3(xi, yi + 1, zi + 1, seed);
    const float v111 = lattice3(xi + 1, yi + 1, zi + 1, seed);

    const float c00 = v000 + (v100 - v000) * tx;
    const float c10 = v010 + (v110 - v010) * tx;
    const float c01 = v001 + (v101 - v001) * tx;
    const float c11 = v011 + (v111 - v011) * tx;

    const float a = c00 + (c10 - c00) * ty;
    const float b = c01 + (c11 - c01) * ty;
    return a + (b - a) * tz;
}

float fbm2(float x, float y, std::int64_t seed, int octaves) {
    if (octaves < 1) {
        octaves = 1;
    }
    if (octaves > 32) {
        octaves = 32;
    }
    float sum = 0.0f;
    float weight = 1.0f;
    float total = 0.0f;
    float fx = x;
    float fy = y;
    for (int i = 0; i < octaves; ++i) {
        sum += valueNoise2(fx, fy, seed + i * 1013904223ll) * weight;
        total += weight;
        weight *= 0.5f;
        fx *= 2.0f;
        fy *= 2.0f;
    }
    return sum / total;
}

} // namespace mc
