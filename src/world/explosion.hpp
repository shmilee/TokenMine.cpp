// V3-14a: the explosion core — the blast's pure world math (the ghast's
// V3-14b fireball and the --explode gate seam share it). No GL, no SDL,
// no Game state: the scan reads the World through its documented
// air-fallback getters, so it is unit-testable on a fixture world
// (the fire_spread pure-function model).
//
// The blast's three effects (the V3-14a scope, the BACKLOG's
// "伤害衰减/破块/点火"):
//   1. DAMAGE — the linear falloff (explosionDamage): full at the
//      center, zero at the radius; the game applies it to the player +
//      the mobs (the game's side — this header owns the curve only).
//   2. BREAK  — a cell is broken when it is in range, solid,
//      destroyable, and its data `blast_resistance` loses to the
//      blast's LOCAL intensity (radius - distance: full at the center,
//      0 at the edge). Bedrock / obsidian-class blocks survive by their
//      data (hard rule 3: the core reads the property, never a block
//      id).
//   3. IGNITE — an in-range, unbroken, data-`flamable` cell catches
//      fire (the fire spread's predicate reused; the deterministic 100%
//      — the Minecraft creeper's 25% roll is a documented deviation).
// Fluids are untouched (the block layer only — the documented
// simplification); broken blocks drop NOTHING (the Minecraft twin —
// the break's journal + relight come from the game's setBlock choke
// point, not from this scan).
#pragma once

#include <cmath>
#include <cstddef>
#include <vector>

#include "core/math/vec3.hpp"
#include "world/coordinates.hpp"
#include "world/world.hpp"
#include "block/block_registry.hpp"

namespace mc {

// The blast's defaults (named constants — the magic-number ban): the
// classic TNT-class radius + the center damage (12 = 60% of the 20-HP
// bar, the integer read).
inline constexpr double kExplosionDefaultRadius = 3.0;
inline constexpr double kExplosionBaseDamage = 12.0;

// The damage falloff (pure): the Euclidean distance from the blast's
// center to the hurt point. dist >= radius = zero (the blast's bound);
// otherwise ceil((1 - dist/radius) * baseDamage) — the integer HP
// read (the fall-damage floor twin, deterministic for identical runs).
int explosionDamage(double dist, double radius, double baseDamage);

// The blast's cell scan (pure read). The bounding box (2r+1)^3 around
// the center's cell, in the deterministic x/y/z loop order (the world
// journal's order — identical runs scan identically); a cell is in
// range when its CENTER's distance (the +0.5 cell-center convention)
// is <= radius. `broken` = the cells the game sets to air; `ignited`
// = the cells the game sets to fire (disjoint: the broken branch wins
// a cell the flame would otherwise reach).
struct ExplosionScan {
    std::vector<BlockPos> broken;
    std::vector<BlockPos> ignited;
};

ExplosionScan scanExplosion(const World& world, const BlockRegistry& blocks,
                            const Vec3& center, double radius);

// The --explode CLI's parsed spec (the GameArgs' optional payload —
// nullopt = no blast, the no-input run's structural-immunity root).
struct ExplosionSpec {
    Vec3 center {0.0f, 0.0f, 0.0f};
    double radius = kExplosionDefaultRadius;
};

} // namespace mc
