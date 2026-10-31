#include "world/cloud.hpp"

#include "world/noise.hpp" // splitmix64, hash3

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

// The shared flat-quad emitter (UX-9a): 4 corners of a `size`-wide quad
// at (x0, z0), y = kCloudHeight, in the corner order (x0, z0), (x1, z0),
// (x1, z1), (x0, z1). Every corner carries normal +Y (the vertex
// shader's top-face shade 1.0 — the beam pattern, both rendered faces
// equally bright) and skyLight 0 / blockLight 1.0 (unlit white — the
// clouds never darken). Both cloudCellVertices (16x16) and
// cloudSubCellVertices (4x4) call it — the per-field values are the
// single source, so the two quads are convention-identical.
void emitQuad(float x0, float z0, float size, std::vector<MeshVertex>& out) {
    const float x1 = x0 + size;
    const float z1 = z0 + size;
    const float y = static_cast<float>(kCloudHeight);
    const Vec3 corners[4] = {{x0, y, z0}, {x1, y, z0}, {x1, y, z1},
                             {x0, y, z1}};
    out.reserve(out.size() + 4);
    for (const Vec3& c : corners) {
        MeshVertex v;
        v.position = c;
        v.normal = {0.0f, 1.0f, 0.0f};
        v.skyLight = 0.0f;
        v.blockLight = 1.0f; // unlit white (the clouds never darken)
        out.push_back(v);
    }
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

int cloudTimeStep(int frameCount) {
    // frameCount >= 0 always (the game's counter); the integer division
    // is the floor for positive values — the pure function of the frame.
    return frameCount / kCloudFramesPerCell;
}

void cloudCellVertices(int cellX, int cellZ, std::vector<MeshVertex>& out) {
    emitQuad(static_cast<float>(cellX * kCloudCellBlocks),
             static_cast<float>(cellZ * kCloudCellBlocks),
             static_cast<float>(kCloudCellBlocks), out);
}

void cloudSubCellVertices(int cellX, int cellZ, int subX, int subZ,
                          std::vector<MeshVertex>& out) {
    emitQuad(static_cast<float>(cellX * kCloudCellBlocks +
                                subX * kCloudSubCellBlocks),
             static_cast<float>(cellZ * kCloudCellBlocks +
                                subZ * kCloudSubCellBlocks),
             static_cast<float>(kCloudSubCellBlocks), out);
}

void buildCloudMesh(int camCellX, int camCellZ, int t,
                    std::vector<MeshVertex>& vertices,
                    std::vector<std::uint32_t>& indices) {
    vertices.clear();
    indices.clear();
    const int half = kCloudWindowCells / 2; // 12: 24 cells per side
    const int subs = kCloudCellBlocks / kCloudSubCellBlocks; // 4 per side
    for (int dz = -half; dz < half; ++dz) {
        for (int dx = -half; dx < half; ++dx) {
            const int cx = camCellX + dx;
            const int cz = camCellZ + dz;
            if (!cloudMask(cx, cz, t)) {
                continue;
            }
            // UX-9a: the two-level grain — one flat 4x4 quad per ON
            // sub-cell, sub z outer / sub x inner (the window's z-major
            // scan order continued inside the cell).
            for (int sz = 0; sz < subs; ++sz) {
                for (int sx = 0; sx < subs; ++sx) {
                    if (!cloudSubMask(cx, cz, sx, sz, t)) {
                        continue;
                    }
                    const std::uint32_t base =
                        static_cast<std::uint32_t>(vertices.size());
                    cloudSubCellVertices(cx, cz, sx, sz, vertices);
                    // Two triangles, the (0,1,2,3) quad traced (the
                    // winding is irrelevant — culling is off for the
                    // draw, the beam pattern).
                    indices.push_back(base + 0);
                    indices.push_back(base + 1);
                    indices.push_back(base + 2);
                    indices.push_back(base + 0);
                    indices.push_back(base + 2);
                    indices.push_back(base + 3);
                }
            }
        }
    }
}

bool cloudMeshNeedsRebuild(bool valid, int cellX, int cellZ, int t,
                           int newCellX, int newCellZ, int newT) {
    if (!valid) {
        return true; // the first draw builds the mesh
    }
    return cellX != newCellX || cellZ != newCellZ || t != newT;
}

} // namespace mc
