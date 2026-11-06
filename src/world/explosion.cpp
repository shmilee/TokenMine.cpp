#include "world/explosion.hpp"

namespace mc {

int explosionDamage(double dist, double radius, double baseDamage) {
    // The blast's bound: at (or past) the radius the damage is zero —
    // the check first (a radius <= 0 / baseDamage <= 0 blast is a
    // no-op, the named-constant defense).
    if (radius <= 0.0 || baseDamage <= 0.0 || dist >= radius) {
        return 0;
    }
    const double frac = 1.0 - dist / radius;
    return static_cast<int>(std::ceil(frac * baseDamage));
}

ExplosionScan scanExplosion(const World& world, const BlockRegistry& blocks,
                            const Vec3& center, double radius) {
    ExplosionScan out;
    if (radius <= 0.0) {
        return out; // the no-op blast (the damage's twin)
    }
    // The center's cell (the floor convention — the coordinates' twin:
    // a cell's center is (x + 0.5, y + 0.5, z + 0.5)).
    const int cx = static_cast<int>(std::floor(center.x));
    const int cy = static_cast<int>(std::floor(center.y));
    const int cz = static_cast<int>(std::floor(center.z));
    const int r = static_cast<int>(std::ceil(radius));
    // The bounding box, deterministic x/y/z loop order (the world
    // journal's order — an identical run scans identically). The
    // World getter's documented air fallback covers the y-range edges
    // + the never-created chunks (no bounds check needed).
    for (int x = cx - r; x <= cx + r; ++x) {
        for (int y = cy - r; y <= cy + r; ++y) {
            for (int z = cz - r; z <= cz + r; ++z) {
                const double dx = static_cast<double>(x) + 0.5 -
                                  static_cast<double>(center.x);
                const double dy = static_cast<double>(y) + 0.5 -
                                  static_cast<double>(center.y);
                const double dz = static_cast<double>(z) + 0.5 -
                                  static_cast<double>(center.z);
                const double dist = std::sqrt(dx * dx + dy * dy + dz * dz);
                if (dist > radius) {
                    continue; // outside the blast's sphere
                }
                const BlockID id = world.getBlock(x, y, z);
                if (id == BlockID::air()) {
                    continue; // air (and fluids — the block layer only)
                }
                const Block& b = blocks.at(id);
                // The local intensity: the blast's full reach at the
                // center, 0 at the edge (the resistance's opponent).
                const double local = radius - dist;
                if (b.properties.destroyable &&
                    static_cast<double>(b.properties.blastResistance) <
                        local) {
                    out.broken.push_back(BlockPos {x, y, z});
                } else if (b.properties.flamable) {
                    // The unbroken flammable catches (the broken branch
                    // wins a cell the flame would otherwise reach —
                    // there is nothing left to burn).
                    out.ignited.push_back(BlockPos {x, y, z});
                }
            }
        }
    }
    return out;
}

} // namespace mc
