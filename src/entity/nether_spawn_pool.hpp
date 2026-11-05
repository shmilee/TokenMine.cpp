// V3-3 (the nether's mob spawn pool): the init-side pure resolution of
// data/mob_spawning_nether.json — the checkWoolDrops / campfire pattern:
// the decision lives here (pure over the asset manager + the SHARED mob
// type table, unit-tested headless), and the game's init calls it once
// (a thin adapter: m_nether.mobs.spawnConfig = *the result).
//
// The nether's MobSystem (the Phase B "Phase C fills it" slot) owns the
// POOL — its cadence / cap / weights / the per-dimension spawn counter.
// The TYPE TABLE stays the game's single registry (m_mobs.mobTypes): a
// mob's data is dimension-agnostic content (the nether's pool names the
// same types the overworld's pool can, and the render caches key on the
// single table's MobID space — the attemptMobSpawn's single-registry
// ruling). The pool's vocabulary cross-checks against THAT table: an
// entry naming an unregistered mob is a content regression (the
// overworld's "unknown mob" discipline, the ammo pattern).
//
// Fail-fast at INIT (the NetherGen::create's S6 timing ruling): a
// missing file, a schema error, or an unknown mob name rejects startup
// with the naming — not the first travel (the nether World's laziness
// is the World, not the content resolution).
#pragma once

#include "core/error.hpp"
#include "core/json.hpp"
#include "entity/mob_registry.hpp"
#include "entity/mob_spawning.hpp"

namespace mc {

class AssetManager;

// Load + parse + cross-check the nether's spawn pool:
//   - the file must exist and be valid JSON (loadText / parse name it);
//   - the shared MobSpawningConfig::parse schema applies (the cadence /
//     ring / cap / the weighted list — the overworld file's twin);
//   - every entry's id must resolve in the SHARED type table (named).
// The result is the pool for m_nether.mobs.spawnConfig (the thin
// adapter's assign).
Result<MobSpawningConfig> loadNetherSpawnPool(
    AssetManager& assets, const MobRegistry& types,
    const std::string& relative = "data/mob_spawning_nether.json");

} // namespace mc
