// Lighting: Minecraft-style 0..15 sky light and block light, stored per
// chunk (see Chunk) and queried through World::getSkyLight/getBlockLight.
//
// Propagation model (Minecraft light opacity, LGT-2 — the leaf light
// attenuation):
//  - Each DRY cell attenuates light by its block's `light_opacity` (0..15,
//    Minecraft's scale: 15 = fully blocks light, 2 = leaves, 1 = glass,
//    0 = air / non-opaque plants). The property is DERIVED from `opaque`
//    at data load when the JSON key is absent (opaque -> 15, non-opaque ->
//    0), so field-less content reproduces the pre-LGT-2 lighting bit-for-
//    bit; an explicit `light_opacity` overrides the derivation. A FLUID
//    cell of either kind ALWAYS costs kFluidLightAttenuation (Minecraft's
//    water value) in every direction — the field is not read for fluids
//    (a documented simplification) — a lake bottom is dim in proportion
//    to the water depth.
//  - Sky light: every column's topmost contiguous run of non-full-opacity
//    cells reads 15 (open to the sky), attenuated per cell while scanning
//    down; light then propagates through non-full-opacity cells — free
//    vertically in a zero-opacity cell (air), otherwise at least 1 per
//    step (the cell's opacity).
//  - Block light: every block with properties.lightEmission > 0 is a
//    source (its own cell holds the emission value, even if fully
//    blocking — the glowstone pattern); light propagates with the same
//    per-cell costs in all directions.
//  - The void around the world is open sky (sky 15, block 0) and dry.
//
// Three entry points share one BFS core:
//  - propagateAll: recompute everything (startup, new chunk created).
//  - updateLocal: after a single block change, re-light the ±radius region
//    around it (a one-cell box, see updateRegion).
//  - updateRegion: after a batch of cell changes (a fluid tick), re-light
//    the region around their bounding box.
//    Region shape: sky light travels vertically without air attenuation,
//    so the sky re-scan covers FULL COLUMNS over the box's X/Z footprint
//    (expanded by the radius); block light decays within 15 steps, so it
//    is re-propagated in the box expanded by the radius. Cells outside the
//    region keep their previous values, which remain correct, and serve as
//    read-only boundary sources for the propagation. Only chunks whose
//    light actually changed (net) are marked dirty.
#pragma once

#include <cstdint>
#include <set>

#include "block/block_registry.hpp"
#include "core/config.hpp"
#include "core/error.hpp"
#include "world/coordinates.hpp"
#include "world/world.hpp"

namespace mc {

// Highest light value (Minecraft's light scale).
inline constexpr int kMaxLight = 15;
// Re-light radius after cell changes: max light range (15) plus a one-cell
// boundary margin.
inline constexpr int kLightUpdateRadius = 16;
// Light lost when entering a fluid cell (Minecraft's water value), applied
// in every direction for both sky and block light.
inline constexpr int kFluidLightAttenuation = 3;
// LGT-3 (10-27 user verdict: an all-black deep night is unplayable) +
// UX-1 (the floor is now a RECOVERABLE setting): Minecraft's moonlight
// FLOOR — open-sky light at midnight. This is the CODE DEFAULT of the
// config key `render.night_sky_level` (an integer in [0, kMaxLight];
// absent = this value = the pre-UX-1 v1.34.0 pipeline bit-for-bit;
// 0 = the true all-black night, kMaxLight = no retreat at all). For a
// floor L the sky source level never retreats below it: S = L +
// (kMaxLight - L) * dayFactor (noon 15, deep night L), and the retreat
// distance is (kMaxLight - L) * (1 - dayFactor).
// The shipped hostile mobs pair their spawn threshold to the DEFAULT
// floor (spawn_max_light 7 — MC: hostiles spawn at light <= 7; night
// open sky 7 spawns, a torch circle 14 is safe, day open sky 15 does
// not — the gameplay loop is preserved). The pairing rule holds for any
// configured floor L: the open-sky deep-night level IS L, so at L = 0
// night open sky 0 <= 7 still spawns, at L = 15 it does not — the user's
// choice (the gate S2 narrative in the LGT-3 docs).
inline constexpr int kNightSkyLevel = 7;
// The deep-night retreat in the shader's 0..1-normalized sky values at
// the DEFAULT floor: (kMaxLight - kNightSkyLevel) / kMaxLight = 8/15.
// nightRetreatScale(kNightSkyLevel) is bit-identical to this constant
// (the same expression); the basic.frag coefficient is the u_nightRetreat
// uniform, set once at init from the configured floor (UX-1).
inline constexpr float kNightRetreatScale =
    static_cast<float>(kMaxLight - kNightSkyLevel) /
    static_cast<float>(kMaxLight);
// UX-1: the ambient floor of the presentation light formula (basic.frag:
// rgb = base * (ambient + (1 - ambient) * light * faceShade)) — the
// single definition (the shader side is the u_ambient uniform, set once
// at init). At any nonzero floor the host uploads this value (the GLSL
// 0.03 literal, bit-for-bit); for the zero floor it uploads 0.0 — a
// true all-black night carries no residual glow.
inline constexpr float kAmbientFloor = 0.03f;

class Lighting {
public:
    // Recompute sky and block light for every existing chunk. O(world
    // size); the demo area takes a few tens of milliseconds.
    //
    // `skySourceLevel` (LGT-3, the night re-propagation seam): the light
    // level the open-sky source reads. Production ALWAYS passes the
    // default kMaxLight — the stored field is the noon field — so the
    // parameter is a test-only seam. The day cycle drives S within
    // [kNightSkyLevel, kMaxLight] (the moonlight floor); the confluence
    // test exercises values below the floor (down to 0) to pin the
    // mathematical property. The propagated sky field is AFFINE in the
    // source level (every sky source shares one level and the per-cell
    // costs are source-independent): the source-S field equals
    // max(0, S - minCost(x)) cell by cell, which is exactly
    // max(0, stored(x) - (kMaxLight - S)) of the noon field (lossless —
    // see effectiveSkyLight). The confluence test
    // (AffineTransformMatchesDirectPropagation) pins this by
    // propagating the same world at several S and comparing against the
    // affine transform of the S = kMaxLight field.
    static void propagateAll(World& world, const BlockRegistry& blocks,
                             int skySourceLevel = kMaxLight);

    // Re-light the region whose light can have changed after a single
    // block change at `changed` (break or place). Every chunk whose light
    // actually changed is marked dirty (its mesh carries per-face light);
    // chunks that came out identical are left untouched.
    static void updateLocal(World& world, const BlockRegistry& blocks,
                            const BlockPos& changed);

    // Re-light the region whose light can have changed after a batch of
    // cell changes inside the [min, max] box (a fluid tick passes the
    // bounding box of its changed cells; a single cell is min == max).
    // Same net-change dirty semantics as updateLocal.
    //
    // `newTerrain` (Phase 99.9): true when the box covers a freshly
    // INSERTED chunk (the streaming pipeline's column job). Then the sky
    // propagation always runs, even if the rescan's vertical re-assert
    // wrote nothing (a new column can be dark from the start — under a
    // ceiling — and still need its light FLOWED IN from the lit neighbor
    // columns). False (the default: fluid relights, block edits) enables
    // the no-op skip: rescan wrote nothing => the sky field is at its old
    // fixed point => the seed scan + BFS are pure re-proof.
    //
    // `dirtyOut` (99.10): when non-null, the net-changed chunk positions
    // are collected here INSTEAD of marking the World's shared dirty set
    // (markChunkDirty). The streaming lighting jobs run two such regions
    // in parallel on two workers — concurrent markChunkDirty calls would
    // race on the shared set — and the main thread applies the collected
    // positions after the pool flush (Game::collectLitColumns). Null
    // (the default: fluid relights, block edits, tree growth) marks the
    // World exactly as before.
    static void updateRegion(World& world, const BlockRegistry& blocks,
                             const BlockPos& min, const BlockPos& max,
                             bool newTerrain = false,
                             std::set<ChunkPos>* dirtyOut = nullptr);
};

// LGT-3 (the night light retreat) + UX-1 (the configurable floor): the
// EFFECTIVE sky light at a day/night factor. The stored field is the
// noon field (source level kMaxLight); the sky field is affine in its
// source level S (see propagateAll), and S is a pure function of the
// existing day cycle and the deep-night floor `nightLevel` (default
// kNightSkyLevel = 7, the Minecraft moonlight floor — the pre-UX-1
// constant): S = nightLevel + (kMaxLight - nightLevel) * dayFactor
// (deep night 0 -> S = the floor, noon 1 -> S 15). The effective level
// is the stored level minus the retreat distance (kMaxLight - S),
// clamped at 0:
//
//   effective = max(0, stored - (kMaxLight - nightLevel) * (1 - dayFactor))
//
// At dayFactor 1 it is the identity for ANY floor (noon, the stored
// field itself); at 0 it is the floor (open sky 15 -> nightLevel, and
// every stored level below kMaxLight - nightLevel retreats to 0 — at
// the default floor deep night is never all black; at nightLevel 0 it
// IS all black). The lit region therefore shrinks toward the cave
// mouths / canopy gaps along the stored = (kMaxLight - S) contour as
// night falls — the Minecraft-style retreat — WITHOUT any
// re-propagation: the renderer (basic.frag, the normalized form of
// exactly this formula, via the u_nightRetreat uniform) and the spawn
// gate (currentSpawnLight) evaluate it per cell.
//
// The integer version takes the STORED level (0..15, the save/spawn
// semantics) and floors the retreat distance (whole light levels). The
// float version is the exact mirror of the shader's sky term: it takes
// the 0..1-NORMALIZED stored value (the mesh's v_skyLight = level /
// kMaxLight) and applies the fractional retreat nightRetreatScale(
// nightLevel) * (1 - dayFactor) — (8/15) * (1 - dayFactor) at the
// default floor (bit-identical to kNightRetreatScale) — a superset of
// Minecraft's quantized levels (a documented deviation). The two agree
// exactly at dayFactor 0 (the floor: 15 -> nightLevel, 1 -
// nightRetreatScale -> nightLevel/15) and 1 (the identity) and differ
// by less than one level through the ramps.
std::uint8_t effectiveSkyLight(std::uint8_t stored, float dayFactor,
                               int nightLevel = kNightSkyLevel);
float effectiveSkyLightF(float stored, float dayFactor,
                         int nightLevel = kNightSkyLevel);

// UX-1: the deep-night sky floor as a STRICT config read
// (render.night_sky_level). Absent = kNightSkyLevel (the code default,
// the pre-UX-1 pipeline bit-for-bit); present = a JSON number that is
// an integer value in [0, kMaxLight] (0 = the all-black night,
// kMaxLight = no retreat at all). A non-number / non-integer /
// non-finite / out-of-range value is named and rejected (the SAVE-1
// strict-read discipline — the Config getters would silently fall back
// on a type mismatch, which must not happen for a user-visible knob).
Result<int> readNightSkyLevel(const Config& config);

// UX-1: the deep-night retreat distance in the shader's 0..1-normalized
// sky units for a floor of `level`: (kMaxLight - level) / kMaxLight.
// At level = kNightSkyLevel this is bit-identical to kNightRetreatScale
// (the same expression, the same float operations).
float nightRetreatScale(int level);

// UX-1: the ambient floor for a floor of `level`: kAmbientFloor at any
// nonzero level, and 0.0 for level = 0 — a true all-black night
// carries no residual glow (the pair with nightRetreatScale: the two
// values the host uploads to the basic shader once at init).
float ambientFloorForLevel(int level);

} // namespace mc
