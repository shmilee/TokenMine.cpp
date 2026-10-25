// GEN-4 standalone cave-fragmentation calibration driver (the zero-tuning-
// round grid; NOT part of the unit suite — the full table is ~1 minute of
// chunk generation):
//
//   build/bin/cave_probe                      → 9-combo x 3-seed grid,
//                                               the standard 32x32 sample
//   build/bin/cave_probe <cols>               → the grid on a larger
//                                               cols x cols sample
//                                               (cross-check)
//   build/bin/cave_probe <cs> <t> <seed>      → a single combo
//   build/bin/cave_probe <cs> <t> <seed> <cols>
//
// Run from the repository root (the assets are relative). Selection rule
// (the design's): carve rate in the 4-6% band (primary seed 123456789),
// then the lowest pocket share, then the fewest components, ties broken by
// the smallest deviation from the current cs = 0.09. The direction must
// hold on all three seeds.

#include <chrono>
#include <cfloat>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "cave_probe.hpp"

using namespace mc;

namespace {

void printRow(float cs, float t, std::int64_t seed, int cols,
              const CaveStats& s) {
    const double rate = 100.0 * s.carveRate;
    const double pockets = 100.0 * s.smallCellFraction;
    std::printf("cs=%.2f t=%.2f seed=%-10lld cols=%-3d  rate=%5.3f%%  "
                "carved=%6ld band=%7ld  comps=%4d  p50=%5.0f p90=%5.0f "
                "max=%5d  pockets=%5.2f%%  small=%5ld  openings=%4ld\n",
                static_cast<double>(cs), static_cast<double>(t),
                static_cast<long long>(seed), cols, rate, s.carvedCells,
                s.bandCells, s.componentCount, s.volumeP50, s.volumeP90,
                s.volumeMax, pockets, s.smallCells, s.surfaceOpenings);
}

} // namespace

int main(int argc, char** argv) {
    CaveProbeWorld w;
    if (!w.load()) {
        std::fprintf(stderr,
                     "cave_probe: asset load failed (run from the repo root)\n");
        return 1;
    }
    const std::vector<float> cses = {0.09f, 0.07f, 0.06f, 0.05f};
    const std::vector<float> ts = {0.06f, 0.07f, 0.08f};
    const std::vector<std::int64_t> seeds = {123456789, 9, 42};
    if (argc == 4 || argc == 5) {
        char* end1 = nullptr;
        char* end2 = nullptr;
        char* end3 = nullptr;
        const float cs = std::strtof(argv[1], &end1);
        const float t = std::strtof(argv[2], &end2);
        const long seed = std::strtol(argv[3], &end3, 10);
        const int cols = (argc == 5) ? std::atoi(argv[4]) : kCaveSampleCols;
        if (end1 != argv[1] + std::string(argv[1]).size() ||
            end2 != argv[2] + std::string(argv[2]).size() ||
            end3 != argv[3] + std::string(argv[3]).size()) {
            std::fprintf(stderr,
                         "cave_probe: bad arguments <cs> <t> <seed> [cols]\n");
            return 1;
        }
        auto r = w.probe(cs, t, seed, kCaveSampleX0, kCaveSampleZ0, cols);
        if (!r.ok()) {
            std::fprintf(stderr, "cave_probe: %s\n", r.error().describe().c_str());
            return 1;
        }
        printRow(cs, t, seed, cols, *r);
        return 0;
    }
    if (argc == 2 || argc == 1) {
        const int cols = (argc == 2) ? std::atoi(argv[1]) : kCaveSampleCols;
        if (cols % kChunkSize != 0 || cols < kChunkSize) {
            std::fprintf(stderr,
                         "cave_probe: cols must be a multiple of %d\n",
                         kChunkSize);
            return 1;
        }
        std::printf("GEN-4 cave calibration — real assets, sample "
                    "x[112,+%d) x z[0,+%d), 26-neighbour components\n",
                    cols, cols);
        std::printf("legacy (GEN-1 shipped) = cs 0.09 t 0.06\n");
        const auto wallStart = std::chrono::steady_clock::now();
        for (const float cs : cses) {
            for (const float t : ts) {
                for (const std::int64_t seed : seeds) {
                    auto r = w.probe(cs, t, seed, kCaveSampleX0, kCaveSampleZ0,
                                     cols);
                    if (!r.ok()) {
                        std::fprintf(stderr,
                                     "probe cs=%.2f t=%.2f seed=%lld: %s\n",
                                     static_cast<double>(cs),
                                     static_cast<double>(t),
                                     static_cast<long long>(seed),
                                     r.error().describe().c_str());
                        return 1;
                    }
                    printRow(cs, t, seed, cols, *r);
                }
            }
        }
        const auto totalMs = std::chrono::duration_cast<
            std::chrono::milliseconds>(std::chrono::steady_clock::now() -
                                       wallStart)
                                 .count();
        std::printf("total %ld ms (cols=%d)\n", totalMs, cols);
        return 0;
    }
    if (argc != 1 && argc != 2) {
        std::fprintf(stderr,
                     "usage: cave_probe [<cols>]\n"
                     "       cave_probe <cs> <t> <seed> [cols]\n"
                     "  no args = the full 9-combo x 3-seed grid (32x32)\n");
        return 1;
    }
    return 0;
}
