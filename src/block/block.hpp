// Data-driven block definitions.
//
// A Block is pure data: properties (physics/rendering behavior), a model
// (which texture on which face, shape) and a stable identity. Core systems
// (world, renderer, player) read these fields — they must never switch on
// block identity. Adding a new block = adding a JSON file + textures, no
// code changes.
//
// BlockState is intentionally not a separate type yet: no block in the
// current set carries per-instance state (facing, attachment, ...). The
// concept is introduced when the first such block appears, together with a
// real consumer (chunk storage).
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace mc {

// Stable handle into a BlockRegistry. Invalid until assigned.
struct BlockID {
    int value = -1;

    constexpr bool valid() const { return value >= 0; }
    constexpr bool operator==(BlockID o) const { return value == o.value; }
    constexpr bool operator!=(BlockID o) const { return value != o.value; }
    constexpr bool operator<(BlockID o) const { return value < o.value; }
    static constexpr BlockID invalid() { return {-1}; }
};

// Behavioral properties. All fields have sensible defaults; JSON files only
// need to override what differs.
struct BlockProperties {
    bool solid = true;           // blocks movement (AABB collision)
    bool opaque = true;          // fully occludes adjacent faces + blocks light
    bool transparent = false;    // rendered in the blending pass
    bool destroyable = true;     // can be broken by the player
    bool fluid = false;          // simulated as a fluid (Phase 9)
    float hardness = 1.0f;       // base break time in seconds (tools refine it)
    int lightEmission = 0;       // block light 0..15 (Phase 7)

    // Item dropped on break, addressed by item id. Empty = drops nothing.
    // Items are Phase 11; until then the convention is to use the block's
    // own id (or a related block's id, e.g. grass -> dirt).
    std::string drops;
};

// Which texture name is applied to each face group.
struct BlockTexture {
    std::string top;
    std::string side;
    std::string bottom;
};

// Rendering shape + face textures.
struct BlockModel {
    enum class Shape {
        FullCube, // 1x1x1
    };

    Shape shape = Shape::FullCube;
    BlockTexture textures;
};

struct Block {
    BlockID id = BlockID::invalid();
    std::string name;
    BlockProperties properties;
    BlockModel model;
};

} // namespace mc
