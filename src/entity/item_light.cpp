#include "entity/item_light.hpp"

#include <cmath>

#include "entity/item_entity.hpp"
#include "world/lighting.hpp" // kMaxLight
#include "world/world.hpp"

namespace mc {

BlockPos entityCenterCell(const Vec3& position, float height) {
    return {static_cast<int>(std::floor(position.x)),
            static_cast<int>(std::floor(position.y + height * 0.5f)),
            static_cast<int>(std::floor(position.z))};
}

std::pair<float, float> itemDropLight(const World& world,
                                      const ItemEntity& item) {
    // The light is sampled at the cell the AABB CENTER sits in (the
    // Minecraft "entity cell" — a drop straddling a cell boundary takes
    // the center's cell, not a blend of both).
    const BlockPos cell = entityCenterCell(item.position(), item.height());
    // World getters' documented fallback covers y out of range and
    // never-created chunks (open sky / void / below the world).
    const float sky =
        static_cast<float>(world.getSkyLight(cell.x, cell.y, cell.z)) /
        static_cast<float>(kMaxLight);
    const float block =
        static_cast<float>(world.getBlockLight(cell.x, cell.y, cell.z)) /
        static_cast<float>(kMaxLight);
    return {sky, block};
}

} // namespace mc
