#include "entity/item_light.hpp"

#include <cmath>

#include "entity/item_entity.hpp"
#include "world/lighting.hpp" // kMaxLight
#include "world/world.hpp"

namespace mc {

std::pair<float, float> itemDropLight(const World& world,
                                      const ItemEntity& item) {
    // The AABB center: position is the bottom-center (Entity convention),
    // the center is height/2 above it. The light is sampled at the cell
    // the CENTER sits in (the Minecraft "entity cell" — a drop straddling
    // a cell boundary takes the center's cell, not a blend of both).
    const Vec3 p = item.position();
    const int cx = static_cast<int>(std::floor(p.x));
    const int cy = static_cast<int>(std::floor(p.y + item.height() * 0.5f));
    const int cz = static_cast<int>(std::floor(p.z));
    // World getters' documented fallback covers y out of range and
    // never-created chunks (open sky / void / below the world).
    const float sky =
        static_cast<float>(world.getSkyLight(cx, cy, cz)) /
        static_cast<float>(kMaxLight);
    const float block =
        static_cast<float>(world.getBlockLight(cx, cy, cz)) /
        static_cast<float>(kMaxLight);
    return {sky, block};
}

} // namespace mc
