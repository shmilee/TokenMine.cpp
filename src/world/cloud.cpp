#include "world/cloud.hpp"

#include "world/coordinates.hpp" // floorDiv
#include "world/noise.hpp"       // splitmix64, hash3

namespace mc {

namespace {

// The golden step (the PITS #4 advance: splitmix64 is a pure finalizer,
// the stream must ADVANCE its state before the mix — the kFeatureStep /
// kGoldenStep discipline).
constexpr std::uint64_t kGoldenStep = 0x9E3779B97F4A7C15ULL;

// Two's-complement reinterpretation (the noise.cpp convention — a
// bijection, so negative cell coordinates are fine).
std::uint64_t asU64(int v) {
    return static_cast<std::uint64_t>(static_cast<std::int64_t>(v));
}

} // namespace

bool cloudMask(int cellX, int cellZ, int t) {
    // The lattice hash decorrelates the cells from each other; the time
    // step advances the state by the golden step before the finalizer
    // (PITS #4). The top bit: 50% density.
    const std::uint64_t state =
        hash3(asU64(cellX), asU64(cellZ), 0, kSaltCloud) +
        static_cast<std::uint64_t>(t) * kGoldenStep;
    return (splitmix64(state) >> 63) == 1;
}

bool cloudSubMask(int cellX, int cellZ, int subX, int subZ, int t) {
    // UX-9a: the sub identity in WORLD 4-block lattice coordinates
    // (cellX*4 + subX, cellZ*4 + subZ) — the pattern is world-anchored,
    // so it never wanders when the camera stands still. The SAME lattice
    // discipline as cloudMask (PITS #4: the golden-step t advance before
    // the splitmix64 finalizer; the asU64 two's-complement convention
    // for negative coordinates) and the SAME time step (region and sub
    // re-roll on the same frame — a whole clump changes together, no
    // sub-clump relative sliding). The mod-4 roll: 75% density.
    const int wx = cellX * (kCloudCellBlocks / kCloudSubCellBlocks) + subX;
    const int wz = cellZ * (kCloudCellBlocks / kCloudSubCellBlocks) + subZ;
    const std::uint64_t state =
        hash3(asU64(wx), asU64(wz), 0, kSaltCloudSub) +
        static_cast<std::uint64_t>(t) * kGoldenStep;
    return splitmix64(state) % kCloudSubDensityDenominator <
           kCloudSubDensityNumerator;
}

int cloudSubHeight(int wx, int wz, int t) {
    // UX-9a: the third, INDEPENDENT level — the column thickness (the
    // CPU oracle for the fragment-shader cloud-column port). The on/off
    // decision is the SAME two-level predicate restated in world
    // coordinates (region = floorDiv(wx, 4), sub = the 0..3 remainder —
    // calling cloudMask ∧ cloudSubMask, so the values cannot drift from
    // the presence field). The height roll itself is a dedicated lattice
    // (kSaltCloudSubHeight — collision-checked against every other salt)
    // with the SAME PITS #4 discipline: the golden-step t advance before
    // the splitmix64 finalizer, the asU64 two's-complement convention
    // for negative coordinates. The mod-3 roll is a uniform {1, 2, 3}
    // (mean 2 blocks of thickness), independent of the on/off rolls
    // (different salts — no tracking).
    const int regionX = floorDiv(wx, kCloudSubCellBlocks);
    const int regionZ = floorDiv(wz, kCloudSubCellBlocks);
    const int subX = wx - regionX * kCloudSubCellBlocks;
    const int subZ = wz - regionZ * kCloudSubCellBlocks;
    if (!cloudMask(regionX, regionZ, t) ||
        !cloudSubMask(regionX, regionZ, subX, subZ, t)) {
        return 0;
    }
    const std::uint64_t state =
        hash3(asU64(wx), asU64(wz), 0, kSaltCloudSubHeight) +
        static_cast<std::uint64_t>(t) * kGoldenStep;
    return 1 + static_cast<int>(splitmix64(state) % 3);
}

int cloudTimeStep(int frameCount) {
    // frameCount >= 0 always (the game's counter); the integer division
    // is the floor for positive values — the pure function of the frame.
    return frameCount / kCloudFramesPerCell;
}

} // namespace mc
