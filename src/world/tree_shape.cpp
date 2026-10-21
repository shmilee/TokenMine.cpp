#include "world/tree_shape.hpp"

#include <cstdlib>

#include "world/noise.hpp"

namespace mc {

namespace {

// The golden-ratio step (PITS #4): the roll state must ADVANCE by this
// before the splitmix64 finalizer — the finalizer alone is not a
// progression (a constant input would yield a constant stream).
constexpr std::uint64_t kGoldenStep = 0x9E3779B97F4A7C15ULL;

// Dedicated salt for the tree height roll: the rolls never track the
// worldgen tree lattice (kSaltTree), the crop rolls, or the sapling
// growth rolls (their own salt, world/growth).
constexpr std::uint64_t kSaltTreeGrowth = 0x1A2B3C4D5E6F7A09ULL;

// Reinterpret an (possibly negative) int as 64 bits for hashing (two's
// complement is a bijection — the worldgen/growth u64 convention).
std::uint64_t asU64(int v) {
    return static_cast<std::uint64_t>(static_cast<std::int64_t>(v));
}

// One canopy layer: the offset above the root + the square's radius.
struct CanopyLayer {
    int offset;
    int radius;
};

// The shape's layer table (the header's silhouette description). `height`
// is the trunk length T; r is the spec's leaf_radius (the Oak table only).
std::vector<CanopyLayer> canopyLayers(const TreeSpec& spec, int height) {
    switch (spec.shape) {
        case TreeShape::Oak:
            // The worldgen's placeTrees canopy (the unit test pins the
            // cell-for-cell isomorphism): two full-radius layers around
            // the trunk top, then the smaller cap on very top.
            return {{height - 2, spec.leafRadius},
                    {height - 1, spec.leafRadius},
                    {height, 1}};
        case TreeShape::Spruce:
            // The bottom-wide cone: a full radius-2 skirt two below the
            // top, then three radius-1 layers up to one above the top.
            return {{height - 2, 2}, {height - 1, 1}, {height, 1},
                    {height + 1, 1}};
        case TreeShape::Birch:
            // The slim small crown: three radius-1 layers from one below
            // the top to one above it.
            return {{height - 1, 1}, {height, 1}, {height + 1, 1}};
    }
    return {};
}

} // namespace

bool treeShapeValid(std::string_view name) {
    return name == "oak" || name == "spruce" || name == "birch";
}

TreeShape treeShapeFromName(std::string_view name) {
    if (name == "spruce") {
        return TreeShape::Spruce;
    }
    if (name == "birch") {
        return TreeShape::Birch;
    }
    return TreeShape::Oak; // "oak" (the only remaining validated name)
}

std::vector<TreeCell> planTree(int height, const TreeSpec& spec) {
    std::vector<TreeCell> cells;
    if (height < 1) {
        return cells; // malformed (the validated domain is [1, 16+8])
    }
    // The trunk column: the root (the sapling cell) through i = height-1.
    cells.reserve(static_cast<std::size_t>(height) + 20);
    for (int i = 0; i < height; ++i) {
        cells.push_back(TreeCell{{0, i, 0}, true});
    }
    // The canopy layers (table order): a square of leaves at each (dx, dz)
    // within the radius — the (0, 0) centre skipped while the trunk still
    // occupies it (offset <= height - 1), the square's corners rounded
    // when the radius is 2 (the placeTrees rule).
    for (const CanopyLayer& layer : canopyLayers(spec, height)) {
        for (int dx = -layer.radius; dx <= layer.radius; ++dx) {
            for (int dz = -layer.radius; dz <= layer.radius; ++dz) {
                if (dx == 0 && dz == 0 && layer.offset <= height - 1) {
                    continue; // trunk cell (already planned)
                }
                if (layer.radius == 2 && std::abs(dx) == layer.radius &&
                    std::abs(dz) == layer.radius) {
                    continue; // round the corners
                }
                cells.push_back(
                    TreeCell{{dx, layer.offset, dz}, false});
            }
        }
    }
    return cells;
}

int treeHeightRoll(std::uint64_t seed, const BlockPos& pos,
                   const TreeSpec& spec) {
    // Position-hash the root (decorrelates trees from each other — the
    // crop-roll pattern), seed with the dedicated salt, ADVANCE (PITS #4),
    // finalize, and take the residue over the range width.
    const std::uint64_t state =
        hash3(asU64(pos.x), asU64(pos.y), asU64(pos.z),
              seed ^ static_cast<std::uint64_t>(kSaltTreeGrowth));
    return spec.minHeight +
           static_cast<int>(
               splitmix64(state + kGoldenStep) %
               static_cast<std::uint64_t>(spec.heightRange + 1));
}

TreeSpec treeSpecFromBlock(const Block& block) {
    TreeSpec spec;
    const auto& t = *block.properties.tree;
    spec.minHeight = t.minHeight;
    spec.heightRange = t.heightRange;
    spec.leafRadius = t.leafRadius;
    spec.shape = treeShapeFromName(t.shape);
    // trunk / leaves stay invalid: the ids are resolved by the game's
    // init (the block names are validated against the registry).
    return spec;
}

} // namespace mc
