// World generation parameters, loaded from assets/data/worldgen.json.
//
// The file is organized in optional sections (terrain / biomes / caves /
// ores / trees). Missing sections or keys keep the documented defaults;
// wrong types are hard errors (the same policy as the block registry).
//
// All blocks (terrain fills, water, ore, tree parts) are referenced by name
// and resolved against the BlockRegistry when a WorldGenerator is built —
// adding an ore to the world = adding a JSON entry + block definition +
// texture, no core code changes.
#pragma once

#include <string>
#include <vector>

#include "core/asset_manager.hpp"
#include "core/error.hpp"
#include "core/json.hpp"

namespace mc {

// One ore vein distribution entry ("ores.veins" in worldgen.json).
struct OreConfig {
    std::string block;       // block name (resolved against the registry)
    float probability = 0.2f; // fraction of lattice cells hosting a vein, [0, 1]
    int veinSize = 8;         // GEN-3: ellipsoid clusters K, [1, 8] (was walk steps [1,32])
    int maxY = 80;            // veins never appear above this world Y
};

struct WorldgenParams {
    // Terrain (section "terrain").
    int baseHeight = 40;          // center of the height range
    int heightAmplitude = 24;     // height varies by +/- this amount
    int seaLevel = 34;            // water fills columns up to this Y
    float heightNoiseScale = 0.02f;
    int heightOctaves = 4;
    std::string bottomBlock = "stone"; // the y=0 layer
    std::string deepBlock = "stone";   // the world's deep bedrock
    // GEN-7: the multi-layer bedrock floor. Layers y < bedrockLayers are
    // the bottom floor: EVEN layers (y 0, 2, ...) are always the bottom
    // block, ODD layers (y 1, 3, ...) are the bottom block where a
    // per-cell deterministic 50% roll hits (world_generator's
    // bedrockLayerRoll, the Minecraft bedrock floor rule). 1 (default) =
    // the legacy single y=0 layer (bit-for-bit).
    int bedrockLayers = 1;

    // GEN-8: mountain passes (the terrain features interacting with each
    // other — the Minecraft "passes" rule). A COARSE ridge mask
    // (fbm2 at `noiseScale`, its own salt) marks the mountain ranges;
    // where the mask exceeds `threshold` the base height field is raised
    // by up to `amplitude` (linear in the mask's excess), so adjacent
    // peaks merge into ranges with saddles ("passes") between them where
    // the mask dips. The bonus is a PURE function of the noise fields
    // (no chunk data) — cross-chunk consistent by construction, and it
    // only ever RAISES (passes never dig). Disabled (default) = the
    // legacy height field bit-for-bit. Section "terrain.passes".
    //
    // (The original local-peak design — strict 3x3 maxima of the base
    // field — was dropped in 10-27 gate 122 prep: at the shipped noise
    // scale 0.02 the rounded dome tops are 5-10-wide PLATEAUs with no
    // apex (the 123456789 probe had zero peaks in 64x64); the mask is
    // the robust MC-faithful rule.)
    struct Passes {
        bool enabled = false;
        float noiseScale = 0.008f;  // (0, 0.05]: the ridge mask frequency
        // (0, 1): mask level where the bonus starts. The 3-octave fbm2
        // mask spans ~0.07..0.88, so the threshold is a population
        // selector: shipped 0.55 raises ~27% of the world's columns
        // (up to `amplitude` at the mask peaks) into broad ranges.
        float threshold = 0.55f;
        int amplitude = 6;          // [0, 64]: the bonus at mask = 1.0
    };
    Passes passes;

    // Biome selection (section "biomes").
    float biomeNoiseScale = 0.02f;

    // Caves (section "caves"). GEN-1: spaghetti tubes — the intersection
    // of two independent 3D noise mid-bands (|n - 0.5| < tubeThreshold on
    // BOTH fields) yields thin winding tubes instead of the old
    // single-noise blobs.
    bool cavesEnabled = true;
    float caveNoiseScale = 0.06f;
    // LEGACY (pre-GEN-1 "carve where the 3D noise exceeds this"): still
    // parsed and range-validated so old configs load, but the generator
    // does NOT read it (the section warns about the retired key).
    float caveThreshold = 0.68f;
    // GEN-1: carve where |n - 0.5| < this on BOTH cave noise fields
    // (0.04..0.12 are the typical spaghetti range; 0 = no caves).
    float caveTubeThreshold = 0.06f;
    int caveMinY = 2; // never carve below this Y
    // 0 = cave openings may reach the surface (GEN-1: the shipped value,
    // Minecraft cave entrances); > 0 = never carve within this many layers
    // of the surface (the pre-GEN-1 behavior).
    int surfaceGuard = 4;
    // GEN-1: chunks whose column is within this many of the world origin on
    // BOTH axes (|col.x| <= N && |col.z| <= N) are never carved — the
    // spawn candidate columns ({-1,0}²) must keep intact dry ground.
    int caveSpawnClearColumns = 2;

    // Ores (section "ores").
    int oreLattice = 8; // world-space lattice cell size for vein placement
    std::vector<OreConfig> ores;

    // Trees (section "trees").
    int treeLattice = 4;  // candidate tree positions lie on this lattice
    std::string treeTrunk = "wood";
    std::string treeLeaves = "leaves";
    int treeMinHeight = 4; // trunk height in blocks
    int treeHeightRange = 3; // trunk height varies by +0..this

    // Lava lakes (section "lava_lakes", Phase 86 single lake, Phase 88
    // multi-lake list): each is a contained pool of lava carved into the
    // terrain at a fixed location — a (2R+1)^2 square basin centred at
    // (centerX, centerZ), the terrain below the floor carved to air and
    // `fill` full-lava levels poured on the floor. The lake sits at a FIXED
    // floor Y (the region's lowest terrain minus depth, independent of the
    // local surface) so it is deterministic; the surrounding terrain is
    // higher, so the lava is contained. Lakes are placed OUTSIDE the spawn's
    // default facing so the no-input baseline is unaffected.
    struct LavaLake {
        bool enabled = true;
        int centerX = 0;
        int centerZ = 0;
        int radius = 4;   // the basin is (2R+1)^2 = 9x9 at R=4
        int depth = 4;    // the basin floor is this far below the region's
                          // lowest terrain (heightAt)
        int fill = 2;     // full-lava levels poured on the basin floor
    };
    std::vector<LavaLake> lavaLakes;

    // Phase 93: the PROCEDURAL lake distribution (in addition to the fixed
    // list above). When enabled, `count` lakes are placed at deterministic
    // (seeded) pseudo-random positions within the region, each with a random
    // radius/depth/fill drawn from the ranges — the project's "random
    // distribution" for the lava lakes (Minecraft scatters lava lakes across
    // the world). The region is kept OUTSIDE the spawn's default facing so
    // the no-input baseline is unaffected (the deterministic positions are
    // seed-derived, so a given seed always yields the same lakes).
    struct LavaLakeProcedural {
        bool enabled = false;
        int count = 0;             // number of procedural lakes (0..64)
        int minRadius = 1, maxRadius = 4;
        int minDepth = 1, maxDepth = 4;
        int minFill = 1, maxFill = 2;
        // The placement region (world x/z bounds, inclusive).
        int regionMinX = 0, regionMinZ = 0;
        int regionMaxX = 0, regionMaxZ = 0;
    };
    LavaLakeProcedural lavaLakesProcedural;

    // Parse a worldgen definition. Strict about types, permissive about
    // missing keys (defaults above).
    static Result<WorldgenParams> parse(const Json& def);

    // Load + parse the worldgen file. Fails if the file is missing or the
    // JSON is invalid.
    static Result<WorldgenParams> load(AssetManager& assets,
                                       const std::string& relativePath =
                                           "data/worldgen.json");
};

} // namespace mc
