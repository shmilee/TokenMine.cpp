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
    int veinSize = 8;         // blocks per vein, [1, 32]
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
    std::string waterBlock = "water";

    // Biome selection (section "biomes").
    float biomeNoiseScale = 0.02f;

    // Caves (section "caves").
    bool cavesEnabled = true;
    float caveNoiseScale = 0.06f;
    float caveThreshold = 0.68f; // carve where the 3D noise exceeds this
    int caveMinY = 2;            // never carve below this Y
    int surfaceGuard = 4;        // never carve within this many layers of surface

    // Ores (section "ores").
    int oreLattice = 8; // world-space lattice cell size for vein placement
    std::vector<OreConfig> ores;

    // Trees (section "trees").
    int treeLattice = 4;  // candidate tree positions lie on this lattice
    std::string treeTrunk = "wood";
    std::string treeLeaves = "leaves";
    int treeMinHeight = 4; // trunk height in blocks
    int treeHeightRange = 3; // trunk height varies by +0..this

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
