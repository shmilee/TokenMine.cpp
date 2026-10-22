#include "world/ore_vein.hpp"

#include <array>

#include "world/noise.hpp"

namespace mc {

namespace {

// The six axis permutations: for local axis i (0 = u / 3-semi-axis,
// 1 = v / 2, 2 = w / 1), the world axis (0 = x, 1 = y, 2 = z) it lies on.
// Central symmetry makes the mirror images redundant — the 3! = 6
// permutations are the full orientation set.
constexpr std::array<std::array<int, 3>, 6> kOreVeinAxisMap = {{
    {0, 1, 2}, // u->x, v->y, w->z
    {1, 0, 2}, // u->y, v->x, w->z
    {2, 1, 0}, // u->z, v->y, w->x
    {0, 2, 1}, // u->x, v->z, w->y
    {1, 2, 0}, // u->y, v->z, w->x
    {2, 0, 1}, // u->z, v->x, w->y
}};

// The 21-cell table in its FIXED order (w, then v, then u ascending —
// every vein of every orientation walks the table the same way).
constexpr std::array<std::array<int, 3>, kOreVeinCellCount>
makeOreVeinCells() {
    std::array<std::array<int, 3>, kOreVeinCellCount> out {};
    int n = 0;
    for (int w = -1; w <= 1; ++w) {
        for (int v = -2; v <= 2; ++v) {
            for (int u = -3; u <= 3; ++u) {
                if (oreVeinCellInEllipse(u, v, w)) {
                    out[n] = {u, v, w};
                    ++n;
                }
            }
        }
    }
    return out;
}

constexpr int countOreVeinCells() {
    int n = 0;
    for (int w = -1; w <= 1; ++w) {
        for (int v = -2; v <= 2; ++v) {
            for (int u = -3; u <= 3; ++u) {
                if (oreVeinCellInEllipse(u, v, w)) {
                    ++n;
                }
            }
        }
    }
    return n;
}

static_assert(countOreVeinCells() == kOreVeinCellCount,
              "the 3x2x1 ellipsoid holds exactly 21 integer cells");

// Reinterpret an (possibly negative) int as 64 bits for hashing (the
// worldgen/growth u64 convention — two's-complement is a bijection).
std::uint64_t asU64(int v) {
    return static_cast<std::uint64_t>(static_cast<std::int64_t>(v));
}

} // namespace

std::vector<BlockPos> planOreVein(std::uint64_t seed, const BlockPos& origin,
                                  int clusters) {
    if (clusters < 1) {
        clusters = 1;
    }
    if (clusters > kMaxOreVeinClusters) {
        clusters = kMaxOreVeinClusters;
    }
    // The PITS #4 advancing stream: state = hash3(origin, salted seed);
    // every draw advances by the golden step, then splitmix64 finalizes
    // (a constant input would otherwise yield a constant stream).
    std::uint64_t state = hash3(asU64(origin.x), asU64(origin.y),
                                asU64(origin.z), seed ^ kOreVeinSalt);
    const auto draw = [&state]() {
        state += kOreVeinFeatureStep;
        return splitmix64(state);
    };
    // Draw 1: the ellipsoid orientation (one of the six permutations).
    const int orientation = static_cast<int>(draw() % kOreVeinAxisMap.size());
    const std::array<int, 3>& axisMap = kOreVeinAxisMap[orientation];
    static const std::array<std::array<int, 3>, kOreVeinCellCount> cells =
        makeOreVeinCells();

    std::vector<BlockPos> out;
    out.reserve(static_cast<std::size_t>(clusters) * kOreVeinCellCount);
    int cx = origin.x;
    int cy = origin.y;
    int cz = origin.z;
    for (int c = 0; c < clusters; ++c) {
        if (c > 0) {
            // Cluster centres walk within kOreVeinClusterSpread of the
            // previous one: three (v % 5 - 2) components (bit-sliced),
            // each in [-2, +2] (the (v%5-2)^3 bound from the design).
            const std::uint64_t v = draw();
            cx += static_cast<int>(v % 5u) - 2;
            cy += static_cast<int>((v >> 3) % 5u) - 2;
            cz += static_cast<int>((v >> 6) % 5u) - 2;
        }
        for (const std::array<int, 3>& local : cells) {
            // Map the local axis frame (u, v, w) onto the world axes
            // (x, y, z) per the orientation (axisMap[i] = the world
            // axis the i-th local axis lies on).
            int woff[3] = {0, 0, 0};
            woff[axisMap[0]] = local[0];
            woff[axisMap[1]] = local[1];
            woff[axisMap[2]] = local[2];
            const int wx = cx + woff[0];
            const int wy = cy + woff[1];
            const int wz = cz + woff[2];
            // Duplicate drop (first occurrence wins): clusters may
            // overlap; the bound is small (<= 168 cells), so a linear
            // scan keeps the function allocation-light.
            bool seen = false;
            for (const BlockPos& p : out) {
                if (p.x == wx && p.y == wy && p.z == wz) {
                    seen = true;
                    break;
                }
            }
            if (!seen) {
                out.push_back(BlockPos {wx, wy, wz});
            }
        }
    }
    return out;
}

} // namespace mc
