// PRE-4 / UX-9a: the drifting sky cloud layer (Minecraft's grid
// clouds).
//
// The clouds sit at y = kCloudHeight over the WORLD 4-block SUB lattice
// (kCloudSubCellBlocks — the block grain), grouped into 16-block REGION
// cells (kCloudCellBlocks — the kChunkSize alignment: the region grid IS
// the chunk grid at cloud height). One cell of drift every
// kCloudFramesPerCell frames (100 s at 60 fps = the Minecraft "one cell
// per in-game 10 minutes" rhythm).
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
// A THIRD, INDEPENDENT level gives the columns their THICKNESS (UX-9a):
// cloudSubHeight(wx, wz, t) is a uniform {1, 2, 3} block height for
// every ON sub-cell (0 for OFF sub-cells), rolled with the dedicated
// kSaltCloudSubHeight lattice on the SAME time step (a whole clump
// changes shape together).
//
// RENDERING (UX-9a): the pattern is computed PER PIXEL by a dedicated
// shader program (assets/shaders/cloud.vert + cloud.frag) that the host
// draws as ONE kCloudQuadSize x kCloudQuadSize world-space quad at
// kCloudHeight, translated per draw to the camera-anchored 4-block
// lattice origin. The GLSL predicates are an EXACT port of the functions
// below (the project's 3.3 core context has no 64-bit integers — the
// CPU's uint64_t is emulated as uvec2 (lo, hi) pairs in cloud.frag), so
// THIS FILE IS THE CPU ORACLE: the shader must reproduce cloudMask ∧
// cloudSubMask pixel-for-pixel (tests/test_cloud.cpp pins the field; the
// gate's A/B protocol pins the image). The old per-camera-window Mesh
// rebuild is gone: the iGPU's ~140 ms per MB of buffer update made every
// 16-block crossing hitch, while the window edge popped in. The quad is
// uploaded ONCE at init and never again — zero upload, zero camera
// drift, zero pop-in. The thickness in the fragment is a constant-width
// boundary band (cloud.frag documents the approximation); the height
// field is the CPU-side data for the future true-3D (height-aware) port.
//
// Everything here is a PURE function of (cell coordinates, time step):
// no state, no allocation, thread-safe.
//
// PITS #4 discipline: the lattice hashes (hash3 with the dedicated
// kSaltCloud / kSaltCloudSub / kSaltCloudSubHeight) decorrelate the cells
// from each other; the time step advances the state by the golden step
// before the splitmix64 finalizer (the finalizer alone is not a
// progression).
#pragma once

#include <cstdint>

namespace mc {

// The cloud plane's height (world y). kWorldHeight = 128 keeps it inside
// the world (the Minecraft clouds sit below the build ceiling, the same
// place). A named constant — no config key (the zero-config discipline).
inline constexpr int kCloudHeight = 108;
// One REGION cell's side length in blocks (kChunkSize-aligned).
inline constexpr int kCloudCellBlocks = 16;
// UX-9a: one SUB cell's side length in blocks (kCloudCellBlocks /
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
// UX-9a: the one-time cloud QUAD's side length in blocks. The host
// translates it to 4*floorDiv(floor(eye), 4) - kCloudQuadSize/2 in x and
// z, so the quad spans kCloudQuadSize/2 = 8192 blocks on each side of
// the camera's sub cell: far beyond the 70-degree frustum's reach at
// cloud height even at maximum elevation (the edge's largest elevation
// angle, 0.19 degrees, is sub-pixel — invisible), and 8192 is a multiple
// of 4 so the anchor stays on the world sub lattice for every eye
// position. 16384 = 2^14 keeps every corner coordinate (a multiple of 4,
// at most |eye| + 24576) far inside float32's exact-integer bound (2^24);
// the per-pixel evaluation x*0.25 stays cell-exact for |eye| < 2^26.
inline constexpr int kCloudQuadSize = 16384;
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
// The dedicated salt for the cloud SUB-CELL HEIGHT lattice (UX-9a, the
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
// (The CPU oracle for cloud.frag's regionOn — an exact GLSL port.)
bool cloudMask(int cellX, int cellZ, int t);

// The cloud's SUB-cell presence inside region cell (cellX, cellZ): the
// world 4-block lattice point (cellX*4 + subX, cellZ*4 + subZ) is on at
// 75% (the splitmix64 % 4 < kCloudSubDensityNumerator roll, UX-9a). The
// sub identity is WORLD-anchored (the pattern does not wander when the
// camera stands still) and uses the SAME time step as cloudMask (region
// and sub re-roll on the same frame — a whole clump changes together, no
// sub-clump relative sliding). Pure: same arguments -> same answer,
// across runs and threads. (The CPU oracle for cloud.frag's subOn.)
bool cloudSubMask(int cellX, int cellZ, int subX, int subZ, int t);

// UX-9a: the cloud SUB-CELL THICKNESS at world 4-block lattice
// coordinates (wx, wz) in time step t — the CPU oracle for the
// fragment-shader cloud-column port. 0 when the sub is OFF (its region
// cell off, or its sub roll off — the SAME two-level decision as
// cloudMask ∧ cloudSubMask, restated in world coordinates), otherwise a
// uniform {1, 2, 3} block column height (the splitmix64 mod-3 roll, mean
// 2 — the dedicated kSaltCloudSubHeight lattice, re-rolled on the SAME
// time step so a whole clump changes shape together; the height never
// tracks the on/off layers). Pure: same arguments -> same answer, across
// runs and threads. (Not evaluated by the current fragment — the
// boundary-band approximation documents the stand-in; the data is kept
// for the future true-3D port.)
int cloudSubHeight(int wx, int wz, int t);

// The frame-locked drift time step: frameCount / kCloudFramesPerCell.
// The integer division keeps the drift a pure function of the frame;
// monotone non-decreasing in frameCount (period kCloudFramesPerCell).
int cloudTimeStep(int frameCount);

} // namespace mc
