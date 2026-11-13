// Private shared constants/helpers for the game_*.cpp TU family (UX-2 A1).
//
// game.cpp was split (and finally retired in A2) into
// game_frame.cpp / game_interaction.cpp / game_entities.cpp /
// game_render.cpp / game_ui.cpp / game_init.cpp / game_streaming.cpp
// / game_save.cpp; the file-local items used by >= 2 of those translation
// units live here. They sit in the plain (NOT anonymous) namespace mc so a
// TU that includes this header and uses only some of the items cannot trip
// -Wunused-function under -Werror. Only the game_*.cpp TUs may include
// this header (private; never reference it from anywhere else).

#pragma once

#include <SDL3/SDL.h> // nowSeconds

#include "core/math/mat4.hpp" // Mat4
#include "world/chunk_streaming.hpp" // ColumnPos
#include "world/coordinates.hpp" // kChunkSize

namespace mc {

// Sky objects (sun/moon/stars) sit this far from the world origin — inside
// the far plane (kFar) so they are never clipped.
constexpr double kSkyDistance = 800.0;

// Demo area: 4x4 chunk columns covering world x,z in [-32, 32).
constexpr int kDemoChunkMin = -2;
constexpr int kDemoChunkMax = 1;

// Far-plane cap on the stream disk radius (STRM-1): the effective radius
// is min(config world.view_distance, kStreamCapRadius). The cap is a
// property of the renderer (kFar), not of the user's view — no config
// key.
//
// Derivation (per-lattice-point check against kFar = 1000 blocks = 62.5
// columns): in a quadrant, a column at integer offset (dx, dz) (both >=
// 1) is potentially renderable iff its nearest corner can reach the eye
// — worst case the eye at the corner of its own column facing the target
// — 16*sqrt((dx-1)² + (dz-1)²) < kFar, i.e. (dx-1)² + (dz-1)² < 62.5².
// Verified over the lattice: every potentially-renderable column has
// d = sqrt(dx²+dz²) < 64 (continuous maximum sqrt(2) + 62.5 = 63.91;
// lattice maximum 63.89, the (41, 49) family), and every column with
// d > 64 keeps its nearest corner more than kFar from the eye (closest:
// 1002.01 blocks, the (40, 50) family) — it can never be drawn, so
// streaming it is pure memory waste. Cap 64: a radius-80 config streams
// the 12853-column disk instead of the old 25921-column square.
constexpr int kStreamCapRadius = 64;

constexpr int kCrackStages = 5;

// V3-20: the menu's panorama strip's shape (the --panorama generator's
// window's override + the stitch's belt — the game_init / the
// game_render's two TUs' single source: the strip = 8 views of
// 640x360 = 5120x360, the menu's background's slow pan's asset).
constexpr int kPanoramaViews = 8;
constexpr int kPanoramaViewWidth = 640;
constexpr int kPanoramaViewHeight = 360;
// V3-21 → V3-22: the panorama's per-view HORIZONTAL FOV = 360 / views
// — the views must TILE the azimuth's circle exactly (no overlap, no
// gap). Mat4::perspective's first argument is the VERTICAL FOV: the
// vertical's twin = 2·atan(tan(h/2)·H/W), computed in render's
// effective-FOV's face. The v3.48 gate-catch (the user's 11-13
// second report): the tile-exact's value was fed to the vertical's
// argument — each view then covered a 72.7-degree azimuth, the 8
// views' 27.7-degree overlap = the every-boundary's scene-cut's root
// (the grass's / the tree's misalignment the user saw).
constexpr float kPanoramaHfovDegrees = 360.0f / kPanoramaViews;

inline double nowSeconds() {
    return static_cast<double>(SDL_GetPerformanceCounter()) /
           SDL_GetPerformanceFrequency();
}

// The column's model matrix: vertex positions bake each chunk's LAYER
// offset (y), so the model translates the column origin in X/Z only
// (baking X/Z into the vertices as well would double-apply it).
inline Mat4 columnModel(const ColumnPos& col) {
    return Mat4::translate({static_cast<float>(col.x * kChunkSize), 0.0f,
                            static_cast<float>(col.z * kChunkSize)});
}


// Bow (Phase 21): the arrow spawns this far in front of the eye (the
// launch origin never sits inside the camera near plane).
constexpr float kArrowSpawnOffset = 0.3f;

// V3-10b (the ender pearl): the throw's launch speed in blocks/second
// (the Minecraft 1.5 b/t EXACT value — the arrow's 15 b/s is the
// project's documented 1/4-scale deviation, a pearl at 7.5 would fly
// ~3 blocks: unhittable, the ruling's absolute value) + the throw's
// cooldown in frames (the Minecraft 1 s = 20 ticks frame-locked; the
// session state, the fly-toggle precedent class).
constexpr float kPearlLaunchSpeed = 30.0f;
constexpr int kPearlThrowCooldownFrames = 60;
// The landing's uniform offset RADIUS in blocks (the Minecraft 2.5
// EXACT value — the sphere-uniform roll's bound).
constexpr float kPearlTeleportOffset = 2.5f;

// V3-10c (the enderman's carry): the PLACE cadence in FRAMES (the
// Minecraft 1/2000-tick read at the 60 fps step = 6000 frames ~ 100 s —
// the frame-mod's zero-stream discipline: the global frame, the shared
// cadence, the deterministic; the recon's R6's named constant). The
// pickup is every frame (the MC 1/20-tick read — no constant).
constexpr int kCarryPlaceFrames = 6000;
// The carried cube's draw scale (the Minecraft 0.9 read over the
// 1.0 item cube — the overhead block's project scale).
constexpr float kCarryBlockScale = 0.9f;

// V3-10d (the ender dragon): the death ritual's ascent (the D10's
// compressed MC 10-s read): the 60-frame flight to the perch + the
// 180-frame hover = the 240 total, the removal frame (the game pass
// drives the corpse, the despawn pass's delay gate reads the bound —
// the two TUs share the named constants here).
constexpr int kDragonAscentFlyFrames = 60;
constexpr int kDragonAscentTotalFrames = 240;
// The fireball's impact CLOUD (the MC linger read's project scale — the
// breath's 3-s data twin's fixed constants: the impact's 5-s duration +
// the 2.5-block radius, the D11's damage-by-cloud read).
constexpr double kDragonFireballCloudDuration = 5.0;
constexpr float kDragonFireballCloudRadius = 2.5f;

// V3-12c1 (the nameplate's projection): the camera's fixed constants
// (the game_render's file-local trio, moved to the shared header —
// the nameplate's world->screen read must use the SAME fov / near /
// far as the 3D pass, the projection's single source).
inline constexpr float kFovDegrees = 70.0f;
inline constexpr float kNear = 0.1f;
inline constexpr float kFar = 1000.0f;

// V3-10d: the XP burst's orb count + the every-5-frames release
// cadence (the MC 155-tick/5-tick read's frame scale; 22 x 9 + 1 x 2 =
// 200 — the first kill's project-scale XP).
constexpr int kDragonXpOrbs = 23;
constexpr int kDragonXpOrbValue = 9;
constexpr int kDragonXpLastOrbValue = 2;
constexpr int kDragonXpReleaseFrames = 5;

// V3-10d2 (the summon / the re-kill): the EGG session respawn's
// deadline in FRAMES (the player-broken egg rewrites at (0,54,0) after
// 60 s — the MC ~1-min read; SESSION state, a reload resets the timer
// = D7's documented deviation; the summon-sequence's egg REMOVAL is a
// player break NOT — it clears the pending timer, the MC read).
constexpr int kEggRespawnFrames = 3600;
// The SUMMON sequence's countdown in FRAMES (the 4 face crystals'
// presence gate — breaking any one during it cancels; the sequence's
// completion leaves the arena egg-free + tower-complete, and the 10d1
// spawn gate's 60-frame delay then brings the dragon back — the MC's
// "crystals, then the dragon" read).
constexpr int kDragonSummonCountdownFrames = 60;
// The RE-KILL's XP (one orb's value — the MC 500/12000 ratio's 1/24
// over the project's 200 = 8, the D5's read; the first kill's 200
// burst is the 10d1's scheduler, this is its re-kill's single orb).
constexpr int kDragonRekillXpValue = 8;

} // namespace mc
