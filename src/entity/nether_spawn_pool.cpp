#include "entity/nether_spawn_pool.hpp"

#include "core/asset_manager.hpp"
#include "core/logger.hpp"

namespace mc {

Result<MobSpawningConfig> loadNetherSpawnPool(AssetManager& assets,
                                              const MobRegistry& types,
                                              const std::string& relative) {
    // The file (the overworld pool's load discipline — loadText names
    // the missing path, the init's "cannot read asset file" line).
    auto text = assets.loadText(relative);
    MC_TRY(text);
    auto parsed = Json::parse(*text);
    MC_TRY(parsed);
    // The shared strict schema (the overworld pool's twin — the cadence
    // fields, the ring, the cap, the weighted {id, weight} list).
    auto cfg = MobSpawningConfig::parse(*parsed);
    MC_TRY(cfg);
    // The vocabulary cross-check (the overworld's "unknown mob"
    // discipline, the ammo / milk item-name pattern — the pool names
    // types in the game's SHARED registry, the single-table ruling).
    for (const SpawnMobEntry& entry : cfg->mobs) {
        if (types.findByName(entry.id) == nullptr) {
            return MC_FAIL(relative + ": unknown mob '" + entry.id +
                           "' (the spawn pool names types in the shared "
                           "mob registry — no data/mobs/" +
                           entry.id + ".json)");
        }
    }
    MC_LOG_INFO("Nether mob spawn pool: " +
                std::to_string(cfg->mobs.size()) + " types, total weight " +
                std::to_string(cfg->totalWeight()));
    return *cfg;
}

} // namespace mc
