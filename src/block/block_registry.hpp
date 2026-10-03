// Registry of all block definitions, populated from JSON data files
// (assets/data/blocks/*.json). One file per block.
//
// BlockID{0} ("air") is a reserved built-in sentinel (the absence of a
// block — no JSON, no textures); data blocks are assigned ids from 1 on.
//
//
// {
//   "id": "grass",
//   "properties": { "hardness": 0.6, "drops": "dirt", ... },  // all optional
//   "model": {
//     "shape": "full_cube",
//     "textures": { "top": "grass_top", "side": "grass_side", "bottom": "dirt" }
//   }
// }
//
// Validation is strict: type errors, out-of-range values, duplicate ids,
// missing textures and opaque+transparent conflicts all produce descriptive
// errors naming the block. Unknown fields are tolerated (with a warning)
// for forward compatibility.
#pragma once

#include <cstddef>
#include <deque>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "block/block.hpp"
#include "core/asset_manager.hpp"
#include "core/error.hpp"
#include "core/json.hpp"

namespace mc {

class BlockRegistry {
public:
    // Creates a registry containing only the built-in air sentinel (id 0).
    BlockRegistry();

    // Parse and register one block definition. Assigns the next free id.
    Result<BlockID> registerBlock(const Json& def);

    // Load and register every *.json file in `relativeDir` under the asset
    // root (sorted for determinism). Non-const: text loading updates the
    // AssetManager cache.
    Result<void> loadDirectory(AssetManager& assets,
                               const std::string& relativeDir = "data/blocks");

    // -- lookups -------------------------------------------------------------
    const Block& at(BlockID id) const; // asserts valid id
    const Block* find(BlockID id) const;
    const Block* findByName(const std::string& name) const;
    BlockID idForName(const std::string& name) const; // invalid() when absent
    // The block that carries the simulated fluid (properties.fluid). The
    // game simulates one fluid (water); the mesher uses this block's
    // textures for every fluid face. nullopt when no fluid block is defined.
    std::optional<BlockID> fluidBlock() const;

    std::size_t count() const { return m_blocks.size(); }
    std::vector<BlockID> allIds() const;

    // Every texture name referenced by any block (for atlas cross-checks).
    std::set<std::string> requiredTextures() const;

    void clear();

private:
    // Parse one properties object into `out` (starts at defaults).
    static Result<void> parseProperties(const Json& def, BlockProperties& out);
    static Result<void> parseModel(const Json& def, BlockModel& out);

    std::deque<Block> m_blocks; // deque: stable references on push
    std::unordered_map<std::string, int> m_names;
};

} // namespace mc
