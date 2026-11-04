// Data-driven biome definitions.
//
// A Biome is pure data loaded from JSON (assets/data/biomes/*.json, one file
// per biome): which blocks make up the surface, how deep the subsurface
// layer is, how densely trees grow, and how much the terrain height varies
// in that biome.
//
// Selection: world generation maps a low-frequency 2D noise value at a world
// position (in [0, 1)) onto the biomes via cumulative weight. Biomes are
// considered in registry load order (alphabetical file order), which is
// deterministic.
//
// Adding a biome = adding a JSON file (+ block/texture data as needed), no
// core code changes.
#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "core/asset_manager.hpp"
#include "core/error.hpp"
#include "core/json.hpp"

namespace mc {

// One surface decoration (Phase 45): a block placed on the biome's own
// surface (the top terrain layer) at a per-lattice-cell probability.
// Shared by all decoration entries: the tree lattice (worldgen's
// "trees.lattice"), a dedicated salt (independent of the tree rolls), and
// one roll per (cell, entry) pair.
struct BiomeDecoration {
    std::string block;         // block name (resolved at generation setup)
    float probability = 0.0f;  // per-lattice-cell chance, [0, 1]
};

// V2-16: the biome's own worldgen tree — the strict six-key `tree` object
// (the SAME field face as a sapling block's tree object, the
// block_registry parsing precedent). The trunk / leaves NAMES are
// cross-checked against the BlockRegistry at generation setup (World
// Generator::create — the biome surface discipline); the shape vocabulary
// + the integer ranges are checked at data load (treeShapeValid).
struct BiomeTreeSpec {
    std::string trunk;      // the trunk block name (non-empty once validated)
    std::string leaves;     // the canopy block name (non-empty once validated)
    int minHeight = 0;      // [1, 16] once validated
    int heightRange = 0;    // [0, 8] once validated
    int leafRadius = 0;     // [1, 2] once validated (the Oak table reads it)
    std::string shape;      // "oak" / "spruce" / "birch" once validated
};

struct Biome {
    std::string name;
    int weight = 1;                 // relative selection weight (>= 1)
    std::string surface = "grass";  // block name of the topmost layer
    std::string subsurface = "dirt"; // block name below the surface
    int subsurfaceDepth = 3;        // layers below the surface, [1, 16]
    float treeDensity = 0.0f;       // per-tree-lattice-cell chance, [0, 1]
    float heightScale = 1.0f;       // terrain height variation multiplier
    // Optional surface decorations (Phase 45): cactus (desert), flowers
    // (grass biomes). Absent = no decorations.
    std::vector<BiomeDecoration> decorations;
    // V2-16: the biome's own tree. Absent (nullopt) = the global
    // worldgen.json trees section + the hardcoded oak canopy path (the
    // pre-V2-16 behavior, the bit-level-invariant red line for every
    // tree-less biome).
    std::optional<BiomeTreeSpec> tree;
    // V2-16: excluded from the spawn clear zone's cumulative-weight walk
    // (the demo-region structural protection — inside
    // caves.spawn_clear_columns this biome is skipped by the selection,
    // so the zone's mapping keeps the pre-V2-16 total weight bit-for-bit).
    // Strict bool, default false (absent = selectable everywhere).
    bool excludeSpawnArea = false;
};

class BiomeRegistry {
public:
    BiomeRegistry() = default;

    // Parse and register one biome definition.
    Result<void> registerBiome(const Json& def);

    // Load and register every *.json file in `relativeDir` under the asset
    // root (sorted for determinism). Non-const: text loading updates the
    // AssetManager cache.
    Result<void> loadDirectory(AssetManager& assets,
                               const std::string& relativeDir = "data/biomes");

    // All biomes in load order (the order used for cumulative-weight
    // selection).
    const std::vector<Biome>& all() const { return m_biomes; }
    const Biome* findByName(const std::string& name) const;
    std::size_t count() const { return m_biomes.size(); }
    int totalWeight() const;

    void clear() { m_biomes.clear(); }

private:
    std::vector<Biome> m_biomes;
};

} // namespace mc
