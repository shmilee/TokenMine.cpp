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
