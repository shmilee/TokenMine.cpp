// V3-10c (the End's mob spawn pool): the init-side pure resolution of
// data/mob_spawning_end.json — the loadNetherSpawnPool's THIRD
// consumption (the Phase B "10c fills it" slot's consumer, the two-
// phase precedent: 10a shipped the empty m_end.mobs slot, 10c ships
// the file). The decision lives here (pure over the asset manager +
// the SHARED mob type table, unit-tested headless), and the game's
// init calls it once (a thin adapter: m_end.mobs.spawnConfig = *the
// result).
//
// The End's MobSystem owns the POOL (the nether's member-for-member
// twin — the cadence / cap / weights / the per-dimension spawn
// counter); the TYPE TABLE stays the game's single registry
// (m_mobs.mobTypes — the dimension-agnostic content ruling). The pool's
// vocabulary cross-checks against THAT table (an entry naming an
// unregistered mob is a content regression, the nether's discipline).
//
// Fail-fast at INIT (the NetherGen's S6 timing ruling's twin): a
// missing file, a schema error, or an unknown mob name rejects startup
// with the naming — not the first travel (the End World's laziness is
// the World, not the content resolution).
#pragma once

#include "core/error.hpp"
#include "core/json.hpp"
#include "entity/mob_registry.hpp"
#include "entity/mob_spawning.hpp"

namespace mc {

class AssetManager;

// Load + parse + cross-check the End's spawn pool (the
// loadNetherSpawnPool's shape verbatim over the End's file):
//   - the file must exist and be valid JSON (loadText / parse name it);
//   - the shared MobSpawningConfig::parse schema applies (the cadence /
//     ring / cap / the weighted list — the nether file's twin);
//   - every entry's id must resolve in the SHARED type table (named).
// The result is the pool for m_end.mobs.spawnConfig (the thin
// adapter's assign). 10c ships the SINGLE-SPECIES pool (enderman —
// the End's only mob; the 10d dragon's era re-discusses the overworld
// / nether pools' enderman entries, the recon's R11 boundary).
Result<MobSpawningConfig> loadEndSpawnPool(
    AssetManager& assets, const MobRegistry& types,
    const std::string& relative = "data/mob_spawning_end.json");

} // namespace mc
