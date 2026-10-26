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

int cloudTimeStep(int frameCount) {
    // frameCount >= 0 always (the game's counter); the integer division
    // is the floor for positive values — the pure function of the frame.
    return frameCount / kCloudFramesPerCell;
}

void cloudCellVertices(int cellX, int cellZ, std::vector<MeshVertex>& out) {
    const float x0 = static_cast<float>(cellX * kCloudCellBlocks);
    const float x1 = x0 + static_cast<float>(kCloudCellBlocks);
    const float z0 = static_cast<float>(cellZ * kCloudCellBlocks);
    const float z1 = z0 + static_cast<float>(kCloudCellBlocks);
    const float y = static_cast<float>(kCloudHeight);
    const Vec3 corners[4] = {{x0, y, z0}, {x1, y, z0}, {x1, y, z1},
                             {x0, y, z1}};
    out.reserve(out.size() + 4);
    for (const Vec3& c : corners) {
        MeshVertex v;
        v.position = c;
        // Normal +Y on every corner: the vertex shader's faceShade 1.0
        // (top face) for both rendered orientations — the beam pattern
        // (culling off, no winding dependence).
        v.normal = {0.0f, 1.0f, 0.0f};
        v.skyLight = 0.0f;
        v.blockLight = 1.0f; // unlit white (the clouds never darken)
        out.push_back(v);
    }
}

void buildCloudMesh(int camCellX, int camCellZ, int t,
                    std::vector<MeshVertex>& vertices,
                    std::vector<std::uint32_t>& indices) {
    vertices.clear();
    indices.clear();
    const int half = kCloudWindowCells / 2; // 12: 24 cells per side
    for (int dz = -half; dz < half; ++dz) {
        for (int dx = -half; dx < half; ++dx) {
            const int cx = camCellX + dx;
            const int cz = camCellZ + dz;
            if (!cloudMask(cx, cz, t)) {
                continue;
            }
            const std::uint32_t base =
                static_cast<std::uint32_t>(vertices.size());
            cloudCellVertices(cx, cz, vertices);
            // Two triangles, the (0,1,2,3) quad traced (the winding is
            // irrelevant — culling is off for the draw, the beam pattern).
            indices.push_back(base + 0);
            indices.push_back(base + 1);
            indices.push_back(base + 2);
            indices.push_back(base + 0);
            indices.push_back(base + 2);
            indices.push_back(base + 3);
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
