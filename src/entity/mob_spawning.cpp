#include "entity/mob_spawning.hpp"

#include <algorithm>
#include <cmath>

#include "core/logger.hpp"
#include "world/fluid.hpp"
#include "world/lighting.hpp"

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
                       "non-empty array of {\"id\", \"weight\"?} objects");
    }
    for (std::size_t i = 0; i < mobsJson->size(); ++i) {
        const Json& m = mobsJson->at(i);
        // Phase 39: the legacy plain-string form is rejected (migration
        // guard, the Phase 28 drops precedent) — entries are objects.
        if (m.isString()) {
            return MC_FAIL("mob spawning: 'mobs[" + std::to_string(i) +
                           "]' is a plain string — the Phase 39 schema "
                           "requires an object {\"id\": \"...\"}");
        }
        if (!m.isObject()) {
            return MC_FAIL("mob spawning: 'mobs[" + std::to_string(i) +
                           "]' must be an object {\"id\", \"weight\"?}");
        }
        for (const auto& [key, value] : m.members()) {
            if (key != "id" && key != "weight") {
                MC_LOG_WARN("mob spawning: unknown field 'mobs[" +
                            std::to_string(i) + "]." + key + "' ignored");
            }
        }
        const Json* id = m.find("id");
        if (id == nullptr || !id->isString() || id->asString().empty()) {
            return MC_FAIL("mob spawning: 'mobs[" + std::to_string(i) +
                           "].id' is required and must be a non-empty "
                           "string");
        }
        SpawnMobEntry entry;
        entry.id = id->asString();
        const Json* weight = m.find("weight");
        if (weight != nullptr) {
            if (!weight->isNumber() || weight->asDouble() !=
                                            std::floor(weight->asDouble()) ||
                weight->asDouble() < 1.0 || weight->asDouble() > 10000.0) {
                return MC_FAIL("mob spawning: 'mobs[" + std::to_string(i) +
                               "].weight' must be an integer in [1, 10000]");
            }
            entry.weight = static_cast<int>(weight->asDouble());
        }
        for (const SpawnMobEntry& prev : out.mobs) {
            if (prev.id == entry.id) {
                return MC_FAIL("mob spawning: duplicate mob id '" +
                               entry.id + "' in 'mobs'");
            }
        }
        out.mobs.push_back(std::move(entry));
    }
    return out;
}

int MobSpawningConfig::totalWeight() const {
    int total = 0;
    for (const SpawnMobEntry& e : mobs) {
        total += e.weight;
    }
    return total;
}

std::size_t MobSpawningConfig::pickIndex(std::uint64_t v) const {
    // Walk the cumulative weights: entry i owns the half-open range
    // [sum_{<i}, sum_{<=i}). With all weights 1 this is exactly v (the
    // caller reduced v < list.size()), i.e. the legacy pick % size.
    std::uint64_t cum = 0;
    for (std::size_t i = 0; i < mobs.size(); ++i) {
        cum += static_cast<std::uint64_t>(mobs[i].weight);
        if (v < cum) {
            return i;
        }
    }
    return mobs.size() - 1; // unreachable for v < totalWeight()
}

int currentSpawnLight(const World& world, int x, int y, int z,
                      float dayFactor, int nightLevel) {
    // LGT-3 + UX-1: the sky contribution is the SAME effective level the
    // renderer shows — the affine retreat of the stored (noon) field
    // (effectiveSkyLight with the configured floor nightLevel,
    // world/lighting.hpp), not the pre-LGT-3 scale floor(stored *
    // dayFactor): the spawn gate is the SEEN light (the Minecraft
    // semantics). At noon (dayFactor 1) the two are value-identical for
    // every stored level and every floor; deep night differs — the floor
    // (default kNightSkyLevel 7: open sky 15 -> 7, never 0) IS the
    // feature (the 10-27 user verdict), which the shipped hostile
    // threshold (spawn_max_light 7) pairs with: night open sky 7 spawns,
    // a torch circle 14 is safe, day open sky 15 does not. The pairing
    // rule holds for any configured floor (see the header). Block light
    // is time-independent.
    const int skyNow =
        effectiveSkyLight(world.getSkyLight(x, y, z), dayFactor, nightLevel);
    const int block = static_cast<int>(world.getBlockLight(x, y, z));
    return std::max(skyNow, block);
}

float spawnFloorTop(const World& world, const BlockRegistry& blocks,
                    int x, int y, int z) {
    // The legacy floor top is the cell top (1.0) for EVERY shape — the
    // only V2-17 change is the stateful slab (see the header): state 0 =
    // kSlabHalf, state 1 / >= 2 = 1.0. A stateless slab (the bed family),
    // stairs and full cubes never read a state -> the exact legacy value
    // (the bit-level red line).
    const BlockID id = world.getBlock(x, y, z);
    if (id == BlockID::air() || !id.valid()) {
        return 1.0f;
    }
    const Block& b = blocks.at(id);
    if (b.model.shape != BlockModel::Shape::Slab || !b.hasState()) {
        return 1.0f;
    }
    const int st = world.getBlockState(x, y, z);
    const int s = (st != Chunk::kBlockStateUnset) ? st : b.stateDefault();
    return (s == 1 || s >= 2) ? 1.0f : kSlabHalf;
}

std::optional<BlockPos> findMobSpawnCell(const World& world,
                                         const BlockRegistry& blocks,
                                         float minRadius, float maxRadius,
                                         int cx, int cz, int playerFloorY,
                                         float width, float height,
                                         float dayFactor, int spawnMaxLight,
                                         const Vec3& playerMin,
                                         const Vec3& playerMax,
                                         const std::vector<Vec3>& mobPositions,
                                         float protectionRadius,
                                         float protectionX,
                                         float protectionZ,
                                         int nightLevel,
                                         bool spawnsInAir,
                                         bool walksOnLava) {
    // Cells the mob's AABB occupies above the floor top: the AABB spans
    // [floorTop, floorTop + height), which strictly overlaps the cells
    // floorTop .. floorTop + ceil(height - eps) - 1. (The in-air branch
    // reuses this as the box's cell count: the bat's 0.5 box = 1 cell.)
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

            // Spawn protection (Phase 25): no natural hostile spawn within
            // protectionRadius (horizontal) of the world spawn point
            // (passive mobs pass 0 — exempt). Checked first: it is the
            // cheapest rejection and the policy is about the cell itself.
            if (protectionRadius > 0.0f) {
                const float pdx = static_cast<float>(x) + 0.5f - protectionX;
                const float pdz = static_cast<float>(z) + 0.5f - protectionZ;
                if (pdx * pdx + pdz * pdz < protectionRadius *
                                                  protectionRadius) {
                    continue;
                }
            }

            // V2-14: the in-air branch (the bat — the data flag). The
            // FLOOR BAND IS SKIPPED (any Y, the Minecraft bat cave
            // semantics): the scan walks the column TOP-DOWN and takes
            // the first cell whose airCells-deep box is air with no
            // fluid and passes the light gate (the night floor 7 > the
            // bat's 3 keeps it out of the open sky — caves only; the
            // light gate is the effective "stone ceiling" filter).
            // Top-down = the highest dark air = the spawn under the
            // ceiling (the bat's hang look). No floor, no headroom
            // above the box (the box IS the airCells cells).
            if (spawnsInAir) {
                for (int y = kWorldHeight - airCells; y >= 0; --y) {
                    // The box's cells must be air with no fluid.
                    bool open = true;
                    for (int i = 0; i < airCells; ++i) {
                        const int yy = y + i;
                        if (world.getBlock(x, yy, z) != BlockID::air() ||
                            world.getFluid(x, yy, z) != 0) {
                            open = false;
                            break;
                        }
                    }
                    if (!open) {
                        continue;
                    }
                    // Light gate (the day/night rule): the CURRENT
                    // effective light of the box's floor cell must not
                    // exceed the mob's spawn_max_light (the bat's 3 =
                    // caves only: night open sky 7 > 3, day 15 > 3).
                    if (currentSpawnLight(world, x, y, z, dayFactor,
                                          nightLevel) > spawnMaxLight) {
                        continue;
                    }
                    // Spawn point (bottom-center, feet at the box floor
                    // cell's bottom) and its AABB.
                    const float sx = static_cast<float>(x) + 0.5f;
                    const float sy = static_cast<float>(y);
                    const float sz = static_cast<float>(z) + 0.5f;
                    const float halfW = width / 2.0f;
                    const Vec3 smin {sx - halfW, sy, sz - halfW};
                    const Vec3 smax {sx + halfW, sy + height, sz + halfW};
                    // No overlap with the player (strict AABB test, the
                    // floor branch's twin).
                    if (smin.x < playerMax.x && smax.x > playerMin.x &&
                        smin.y < playerMax.y && smax.y > playerMin.y &&
                        smin.z < playerMax.z && smax.z > playerMin.z) {
                        continue;
                    }
                    // Clearance: no existing mob too close (horizontal,
                    // the floor branch's twin).
                    bool clear = true;
                    for (const Vec3& mp : mobPositions) {
                        const float ddx = mp.x - sx;
                        const float ddz = mp.z - sz;
                        if (ddx * ddx + ddz * ddz <
                            kSpawnClearance * kSpawnClearance) {
                            clear = false;
                            break;
                        }
                    }
                    if (!clear) {
                        continue;
                    }
                    return BlockPos {x, y, z};
                }
                continue;  // no air cell in this column: next column
            }

            // V3-3: the LAVA-SURFACE branch (the strider — the data flag).
            // The walker's floor is the lava's surface plane: the FLOOR
            // BAND IS SKIPPED (the lava sea sits far below the player's
            // floor — the nether plateau's feet vs the sea's top, the
            // kSpawnFloorDelta band would never reach it). The scan walks
            // the column TOP-DOWN and takes the first LAVA SOURCE cell
            // (level 8) whose top is a free surface: the airCells above
            // are air with no fluid (headroom — covered lava is not a
            // walkable top) and the cell below holds no fluid of a
            // DIFFERENT kind (the second-fluid rejection — a lava film
            // over water is not a surface a walker can stand on; lava
            // below is the sea itself, the accepted case). The light
            // gate / player-overlap /
            // clearance / protection checks are the floor branch's twins
            // (the feet cell = y + 1; the protection ran at the top of
            // this column's loop). The returned cell is the LAVA CELL
            // ITSELF — the game's spawnY = cell.y + spawnFloorTop = the
            // cell's top (the block there is air: a fluid is not a block,
            // so spawnFloorTop's exact +1.0).
            if (walksOnLava) {
                for (int y = kWorldHeight - 1 - airCells; y >= 0; --y) {
                    if (world.getFluidOfKind(x, y, z, FluidKind::Lava) !=
                        kMaxFluidLevel) {
                        continue;
                    }
                    // The second-fluid rejection (the design's 3-form):
                    // the cell below holds a fluid of a DIFFERENT kind
                    // (the lava film over water — not a walkable top).
                    // LAVA below is the sea itself (the nether lava sea's
                    // 5-31 all-source column: the topmost L8 is the sea's
                    // top, the walker's floor — the single fluid, the
                    // accepted case).
                    if (y > 0) {
                        const std::optional<FluidKind> below =
                            world.getFluidKind(x, y - 1, z);
                        if (below.has_value() && *below != FluidKind::Lava) {
                            continue;
                        }
                    }
                    bool open = true;
                    for (int i = 1; i <= airCells; ++i) {
                        if (world.getBlock(x, y + i, z) != BlockID::air() ||
                            world.getFluid(x, y + i, z) != 0) {
                            open = false;
                            break;
                        }
                    }
                    if (!open) {
                        continue;
                    }
                    // Light gate (the floor branch's twin): the CURRENT
                    // effective light of the feet cell (y + 1) must not
                    // exceed the mob's spawn_max_light. The nether is
                    // skyless (the stored sky 0) — the gate reads the
                    // block light (the magma's 3, the glowstone's 15).
                    if (currentSpawnLight(world, x, y + 1, z, dayFactor,
                                          nightLevel) > spawnMaxLight) {
                        continue;
                    }
                    // Spawn point (bottom-center, feet on the lava's top)
                    // and its AABB (the floor branch's twin, +1.0).
                    const float sx = static_cast<float>(x) + 0.5f;
                    const float sy = static_cast<float>(y) + 1.0f;
                    const float sz = static_cast<float>(z) + 0.5f;
                    const float halfW = width / 2.0f;
                    const Vec3 smin {sx - halfW, sy, sz - halfW};
                    const Vec3 smax {sx + halfW, sy + height, sz + halfW};
                    // No overlap with the player (the floor branch's twin).
                    if (smin.x < playerMax.x && smax.x > playerMin.x &&
                        smin.y < playerMax.y && smax.y > playerMin.y &&
                        smin.z < playerMax.z && smax.z > playerMin.z) {
                        continue;
                    }
                    // Clearance: no existing mob too close (the floor
                    // branch's twin).
                    bool clear = true;
                    for (const Vec3& mp : mobPositions) {
                        const float ddx = mp.x - sx;
                        const float ddz = mp.z - sz;
                        if (ddx * ddx + ddz * ddz <
                            kSpawnClearance * kSpawnClearance) {
                            clear = false;
                            break;
                        }
                    }
                    if (!clear) {
                        continue;
                    }
                    return BlockPos {x, y, z};
                }
                continue;  // no lava surface in this column: next column
            }

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

            // Light gate (the day/night rule): the CURRENT effective light
            // of the cell the mob would stand in (feet cell) must not
            // exceed the mob's spawn_max_light. At deep night the sky
            // contribution is the configured floor nightLevel (UX-1;
            // default 7 = the LGT-3 moonlight floor), so dark-only mobs
            // may spawn under open sky when the floor admits them; by
            // day it is the full stored level, so they may not.
            if (currentSpawnLight(world, x, floorY + 1, z, dayFactor,
                                  nightLevel) > spawnMaxLight) {
                continue;
            }

            // Spawn point (bottom-center) and its AABB. V2-17: the feet
            // rest on the floor TOP (spawnFloorTop — a stateful slab floor
            // tops out at its solid region, not the cell top; every other
            // shape = the exact legacy + 1.0).
            const float sx = static_cast<float>(x) + 0.5f;
            const float sy =
                static_cast<float>(floorY) +
                spawnFloorTop(world, blocks, x, floorY, z);
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
