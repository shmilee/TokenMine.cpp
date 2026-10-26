// PRE-4: the drifting sky cloud layer (Minecraft's grid clouds, the
// minimal version).
//
// The clouds are a FLAT translucent layer of world-space quads at
// y = kCloudHeight over a 16-block cell grid (kCloudCellBlocks — the
// kChunkSize alignment: the cloud grid IS the chunk grid at cloud
// height). One cell of drift every kCloudFramesPerCell frames (100 s at
// 60 fps = the Minecraft "one cell per in-game 10 minutes" rhythm).
//
// Everything here is a PURE function of (cell coordinates, time step):
// no state, no allocation beyond the mesh builders' outputs,
// thread-safe. The presence field is 50% density (the splitmix64 top
// bit — the salt-and-pepper grid look, the documented deviation from
// Minecraft's noise field: no cloud clump shapes).
//
// PITS #4 discipline: the lattice hash (hash3 with the dedicated
// kSaltCloud) decorrelates the cells from each other; the time step
// advances the state by the golden step before the splitmix64
// finalizer (the finalizer alone is not a progression).
//
// The game caches one Mesh and rebuilds it ONLY when the camera's cloud
// cell or the time step changes (the 99.x discipline — no per-frame VBO
// upload: a no-input run is static from its first draw, walking costs
// one rebuild per 16 blocks).
#pragma once

#include <cstdint>
#include <vector>

#include "renderer/mesh.hpp" // MeshVertex

namespace mc {

// The cloud plane's height (world y). kWorldHeight = 128 keeps it inside
// the world (the Minecraft clouds sit below the build ceiling, the same
// place). A named constant — no config key (the zero-config discipline).
inline constexpr int kCloudHeight = 108;
// One cloud cell's side length in blocks (kChunkSize-aligned).
inline constexpr int kCloudCellBlocks = 16;
// The frames one cell of drift takes (100 s at 60 fps). The time step is
// the integer division of the frame count — a pure function of the frame
// (hard rule 8), so same-frame-count runs are bit-reproducible.
inline constexpr int kCloudFramesPerCell = 6000;
// The camera-centered window of cloud cells per side the mesh covers
// (kCloudWindowCells * kCloudCellBlocks = 384 blocks — far beyond the
// 70-degree frustum's reach at cloud height; the window edge's pop-in
// is hundreds of blocks away, the documented deviation).
inline constexpr int kCloudWindowCells = 24;
// The cloud quad's translucency (0..1). 0.55 = the Minecraft low-res
// cloud look (the white tiles read as clouds, the sky stays visible
// through them). A named constant — no config key.
inline constexpr float kCloudAlpha = 0.55f;

// The dedicated salt for the cloud lattice hash (the kCropRollSalt
// pattern — no other system hashes with it).
inline constexpr std::uint64_t kSaltCloud = 0xC10D05A1ULL;

// The cloud's presence at lattice cell (cellX, cellZ) in time step t:
// true = a cloud tile covers the cell. 50% density (the splitmix64 top
// bit). Pure: same arguments -> same answer, across runs and threads.
bool cloudMask(int cellX, int cellZ, int t);

// The frame-locked drift time step: frameCount / kCloudFramesPerCell.
// The integer division keeps the drift a pure function of the frame;
// monotone non-decreasing in frameCount (period kCloudFramesPerCell).
int cloudTimeStep(int frameCount);

// The world-space vertices of ONE cloud cell: a kCloudCellBlocks x
// kCloudCellBlocks flat quad at y = kCloudHeight, in the corner order
// (x0, z0), (x1, z0), (x1, z1), (x0, z1). Every corner carries normal
// +Y (the vertex shader's top-face shade 1.0 — the beacon beam pattern,
// so both rendered faces are equally bright) and skyLight 0 /
// blockLight 1.0 (the unlit-white rule — the clouds do not darken with
// the day cycle, the documented deviation). Appends to `out`.
void cloudCellVertices(int cellX, int cellZ, std::vector<MeshVertex>& out);

// The FULL window mesh (one flat quad per CLOUD cell of the
// kCloudWindowCells x kCloudWindowCells window centered on the camera's
// cloud cell): indexed triangles — 4 vertices + 6 indices per quad, in
// window-scan order (z outer, x inner). Pure function of its arguments
// (the game rebuilds the cached Mesh from it).
void buildCloudMesh(int camCellX, int camCellZ, int t,
                    std::vector<MeshVertex>& vertices,
                    std::vector<std::uint32_t>& indices);

// The cached mesh's rebuild predicate (the pure dirty flag): true when
// the mesh is not valid yet or the camera cell / the time step moved
// since the last build.
bool cloudMeshNeedsRebuild(bool valid, int cellX, int cellZ, int t,
                           int newCellX, int newCellZ, int newT);

} // namespace mc
