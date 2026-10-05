#include "entity/mob_spawning.hpp"

#include <algorithm>
#include <cmath>

#include "core/logger.hpp"

namespace mc {

namespace {

Result<void> readIntField(const Json& def, const char* key, int& out, int lo,
                          int hi) {
    const Json* v = def.find(key);
    if (v == nullptr || !v->isNumber()) {
        return MC_FAIL("mob spawning: '" + std::string(key) +
                       "' is required and must be an integer");
    }
    const double d = v->asDouble();
    if (d != std::floor(d)) {
        return MC_FAIL("mob spawning: '" + std::string(key) +
                       "' must be an integer");
    }
    out = static_cast<int>(d);
    if (out < lo || out > hi) {
        return MC_FAIL("mob spawning: '" + std::string(key) + " must be in [" +
                       std::to_string(lo) + ", " + std::to_string(hi) +
                       "], got " + std::to_string(out));
    }
    return {};
}

Result<void> readFloatField(const Json& def, const char* key, float& out) {
    const Json* v = def.find(key);
    if (v == nullptr || !v->isNumber()) {
        return MC_FAIL("mob spawning: '" + std::string(key) +
                       "' is required and must be a number");
    }
    out = static_cast<float>(v->asDouble());
    if (out <= 0.0f) {
        return MC_FAIL("mob spawning: '" + std::string(key) + "' must be > 0, "
                       "got " + std::to_string(out));
    }
    return {};
}

} // namespace

Result<MobSpawningConfig> MobSpawningConfig::parse(const Json& def) {
    if (!def.isObject()) {
        return MC_FAIL("mob spawning config must be a JSON object");
    }
    for (const auto& [key, value] : def.members()) {
        if (key != "check_every_frames" && key != "first_spawn_frame" &&
            key != "max_mobs" && key != "min_radius" &&
            key != "max_radius" && key != "despawn_distance" &&
            key != "mobs") {
            MC_LOG_WARN("mob spawning: unknown field '" + key + "' ignored");
        }
    }

    MobSpawningConfig out;
    auto r = readIntField(def, "check_every_frames", out.checkEveryFrames, 1,
                          100000);
    MC_TRY(r);
    r = readIntField(def, "first_spawn_frame", out.firstSpawnFrame, 0,
                     1000000);
    MC_TRY(r);
    r = readIntField(def, "max_mobs", out.maxMobs, 1, 1000);
    MC_TRY(r);
    r = readFloatField(def, "min_radius", out.minRadius);
    MC_TRY(r);
    r = readFloatField(def, "max_radius", out.maxRadius);
    MC_TRY(r);
    if (out.minRadius >= out.maxRadius) {
        return MC_FAIL("mob spawning: 'min_radius' must be < 'max_radius'");
    }
    r = readFloatField(def, "despawn_distance", out.despawnDistance);
    MC_TRY(r);
    if (out.despawnDistance <= out.maxRadius) {
        return MC_FAIL("mob spawning: 'despawn_distance' must be > "
                       "'max_radius' (otherwise every spawn despawns)");
    }

    const Json* mobsJson = def.find("mobs");
    if (mobsJson == nullptr || !mobsJson->isArray() ||
        mobsJson->size() == 0) {
        return MC_FAIL("mob spawning: 'mobs' is required and must be a "
                       "non-empty array of mob names");
    }
    for (std::size_t i = 0; i < mobsJson->size(); ++i) {
        const Json& m = mobsJson->at(i);
        if (!m.isString() || m.asString().empty()) {
            return MC_FAIL("mob spawning: 'mobs[" + std::to_string(i) +
                           "]' must be a non-empty string");
        }
        out.mobs.push_back(m.asString());
    }
    return out;
}

std::optional<BlockPos> findMobSpawnCell(const World& world,
                                         const BlockRegistry& blocks,
                                         float minRadius, float maxRadius,
                                         int cx, int cz, int playerFloorY,
                                         float width, float height,
                                         const Vec3& playerMin,
                                         const Vec3& playerMax,
                                         const std::vector<Vec3>& mobPositions) {
    // Cells the mob's AABB occupies above the floor top: the AABB spans
    // [floorTop, floorTop + height), which strictly overlaps the cells
    // floorTop .. floorTop + ceil(height - eps) - 1.
    const int airCells =
        std::max(1, static_cast<int>(std::ceil(height - 1e-6f)));
    const int ring = static_cast<int>(std::ceil(maxRadius));
    const float minSq = minRadius * minRadius;
    const float maxSq = maxRadius * maxRadius;

    for (int dz = -ring; dz <= ring; ++dz) {
        for (int dx = -ring; dx <= ring; ++dx) {
            const float distSq =
                static_cast<float>(dx * dx + dz * dz);
            if (distSq < minSq || distSq > maxSq) {
                continue;
            }
            const int x = cx + dx;
            const int z = cz + dz;

            // Floor: the HIGHEST solid cell within kSpawnFloorDelta of the
            // player's floor (scanning downward = the surface the mob would
            // stand on; far cliffs/valleys fall outside the band).
            int floorY = -1;
            const int top = playerFloorY + kSpawnFloorDelta;
            const int bottom =
                std::max(0, playerFloorY - kSpawnFloorDelta);
            for (int y = std::min(top, kWorldHeight - airCells - 1);
                 y >= bottom && floorY < 0; --y) {
                if (blocks.at(world.getBlock(x, y, z)).properties.solid) {
                    floorY = y;
                }
            }
            if (floorY < 0) {
                continue;
            }

            // Headroom: the cells above the floor top must be air with no
            // fluid (a mob cannot spawn inside a roof or underwater).
            bool open = true;
            for (int i = 1; i <= airCells; ++i) {
                const int y = floorY + i;
                if (world.getBlock(x, y, z) != BlockID::air() ||
                    world.getFluid(x, y, z) != 0) {
                    open = false;
                    break;
                }
            }
            if (!open) {
                continue;
            }

            // Spawn point (bottom-center) and its AABB.
            const float sx = static_cast<float>(x) + 0.5f;
            const float sy = static_cast<float>(floorY) + 1.0f;
            const float sz = static_cast<float>(z) + 0.5f;
            const float halfW = width / 2.0f;
            const Vec3 smin {sx - halfW, sy, sz - halfW};
            const Vec3 smax {sx + halfW, sy + height, sz + halfW};

            // No overlap with the player (strict AABB test, same
            // convention as the entity physics).
            if (smin.x < playerMax.x && smax.x > playerMin.x &&
                smin.y < playerMax.y && smax.y > playerMin.y &&
                smin.z < playerMax.z && smax.z > playerMin.z) {
                continue;
            }

            // Clearance: no existing mob too close (horizontal distance).
            bool clear = true;
            for (const Vec3& mp : mobPositions) {
                const float ddx = mp.x - sx;
                const float ddz = mp.z - sz;
                if (ddx * ddx + ddz * ddz < kSpawnClearance * kSpawnClearance) {
                    clear = false;
                    break;
                }
            }
            if (!clear) {
                continue;
            }
            return BlockPos {x, floorY, z};
        }
    }
    return std::nullopt;
}

} // namespace mc
