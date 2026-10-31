// PRE-4: the drifting sky cloud layer (Minecraft's grid clouds).
//
// The clouds are a FLAT translucent layer of world-space quads at
// y = kCloudHeight over a 16-block cell grid (kCloudCellBlocks — the
// kChunkSize alignment: the cloud grid IS the chunk grid at cloud
// height). One cell of drift every kCloudFramesPerCell frames (100 s at
// 60 fps = the Minecraft "one cell per in-game 10 minutes" rhythm).
//
// The presence field is TWO-LEVEL (UX-9a, the "blocky clouds" report:
// PRE-4's flat 50% region cells read as smooth paper sheets): a 16-block
// REGION cell is on at 50% (the splitmix64 top bit — the cloud clump
// domain), and each on region cell is subdivided into 4x4 4-block SUB
// cells, each independently on at 75% (the splitmix64 mod-4 roll — the
// block grain). Total coverage = 50% x 75% = 37.5% (the 30-45% blocky
// target band). The sub pattern is anchored to the WORLD 4-block lattice
// (cellX*4 + subX, cellZ*4 + subZ) and re-rolls on the SAME time step as
// the region layer, so a whole clump changes together (no sub-clump
// relative sliding) and standing still never wanders the texture.
//
// A THIRD, INDEPENDENT level gives the clouds their THICKNESS (UX-9b):
// cloudSubHeight(wx, wz, t) is a uniform {1, 2, 3} block column height
// for every ON sub-cell (0 for OFF sub-cells), rolled with the dedicated
// kSaltCloudSubHeight lattice on the SAME time step (a whole clump
// changes shape together). It is a pure field — the CPU oracle for the
// world-anchored fragment-shader cloud-column port (the mesh path stays
// the UX-9a flat sheet until that lands).
//
// Everything here is a PURE function of (cell coordinates, time step):
// no state, no allocation beyond the mesh builders' outputs,
// thread-safe.
//
// PITS #4 discipline: the lattice hashes (hash3 with the dedicated
// kSaltCloud / kSaltCloudSub / kSaltCloudSubHeight) decorrelate the cells
// from each other; the time step advances the state by the golden step
// before the splitmix64 finalizer (the finalizer alone is not a
// progression).
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
// UX-9a: one sub-cell's side length in blocks (kCloudCellBlocks /
// kCloudSubCellBlocks = 4 sub-cells per region side — the 4-block block
// grain of the cloud edge).
inline constexpr int kCloudSubCellBlocks = 4;
// UX-9a: the sub-cell on-density (splitmix64 % 4 < 3 = 3/4 on — the 75%
// grain inside each cloud region; region 50% x sub 75% = 37.5% total
// coverage). Named constant — no config key.
inline constexpr std::uint32_t kCloudSubDensityNumerator = 3;
inline constexpr std::uint32_t kCloudSubDensityDenominator = 4;
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

// The dedicated salt for the cloud REGION lattice hash (the kCropRollSalt
// pattern — no other system hashes with it).
inline constexpr std::uint64_t kSaltCloud = 0xC10D05A1ULL;
// The dedicated salt for the cloud SUB-cell lattice hash (UX-9a). A
// sibling of kSaltCloud in a different value family (kSaltCloud =
// 0xC10D05A1; the sub salt = 0x5B8C92E7): collision-checked against
// every kSalt* / k*RollSalt / kAmbienceSalt in src/ (0x1A2B3C4D5E6F7A0x
// worldgen family, 0xC1B1D57E7AE0C9Ax growth, 0x... particle/fire/chance
// rolls, 0xA1B65445 ambience) — no other system hashes with it, and the
// two cloud layers never track each other.
inline constexpr std::uint64_t kSaltCloudSub = 0x5B8C92E7ULL;
// The dedicated salt for the cloud SUB-CELL HEIGHT lattice (UX-9b, the
// "cloud column thickness" level — the CPU oracle for the fragment-
// shader cloud column port). A sibling of kSaltCloud / kSaltCloudSub in
// the same 32-bit value family (0xC10D05A1 / 0x5B8C92E7 / 0x9C2E4B71):
// collision-checked against every kSalt* / k*RollSalt / kAmbienceSalt in
// src/ — no other system hashes with it in either the 32-bit domain or
// the zero-extended 64-bit domain, and the height layer never tracks the
// on/off layers (three independent rolls, one per level).
inline constexpr std::uint64_t kSaltCloudSubHeight = 0x9C2E4B71ULL;

// The cloud's presence at lattice cell (cellX, cellZ) in time step t:
// true = a cloud tile covers the cell. 50% density (the splitmix64 top
// bit). Pure: same arguments -> same answer, across runs and threads.
bool cloudMask(int cellX, int cellZ, int t);

// The cloud's SUB-cell presence inside region cell (cellX, cellZ): the
// world 4-block lattice point (cellX*4 + subX, cellZ*4 + subZ) is on at
// 75% (the splitmix64 % 4 < kCloudSubDensityNumerator roll, UX-9a). The
// sub identity is WORLD-anchored (the pattern does not wander when the
// camera stands still) and uses the SAME time step as cloudMask (region
// and sub re-roll on the same frame — a whole clump changes together, no
// sub-clump relative sliding). Pure: same arguments -> same answer,
// across runs and threads.
bool cloudSubMask(int cellX, int cellZ, int subX, int subZ, int t);

// UX-9b: the cloud SUB-CELL THICKNESS at world 4-block lattice
// coordinates (wx, wz) in time step t — the CPU oracle for the
// fragment-shader cloud-column port. 0 when the sub is OFF (its region
// cell off, or its sub roll off — the SAME two-level decision as
// cloudMask ∧ cloudSubMask, restated in world coordinates), otherwise a
// uniform {1, 2, 3} block column height (the splitmix64 mod-3 roll, mean
// 2 — the dedicated kSaltCloudSubHeight lattice, re-rolled on the SAME
// time step so a whole clump changes shape together; the height never
// tracks the on/off layers). Pure: same arguments -> same answer, across
// runs and threads.
int cloudSubHeight(int wx, int wz, int t);

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

// The world-space vertices of ONE cloud sub-cell: a kCloudSubCellBlocks
// x kCloudSubCellBlocks flat quad at y = kCloudHeight, the same corner
// order / normals / lightmap convention as cloudCellVertices (the shared
// emitter — UX-9a). Appends to `out`.
void cloudSubCellVertices(int cellX, int cellZ, int subX, int subZ,
                          std::vector<MeshVertex>& out);

// The FULL window mesh (UX-9a two-level grain: one flat 4x4 quad per ON
// SUB-cell of every ON region cell in the kCloudWindowCells x
// kCloudWindowCells window centered on the camera's cloud cell): indexed
// triangles — 4 vertices + 6 indices per quad, in window-scan order (z
// outer, x inner; sub z outer / sub x inner inside a cell). Pure function
// of its arguments (the game rebuilds the cached Mesh from it; the
// rebuild key — camera cell + t — is unchanged by the sub level).
void buildCloudMesh(int camCellX, int camCellZ, int t,
                    std::vector<MeshVertex>& vertices,
                    std::vector<std::uint32_t>& indices);

// The cached mesh's rebuild predicate (the pure dirty flag): true when
// the mesh is not valid yet or the camera cell / the time step moved
// since the last build.
bool cloudMeshNeedsRebuild(bool valid, int cellX, int cellZ, int t,
                           int newCellX, int newCellZ, int newT);

} // namespace mc
