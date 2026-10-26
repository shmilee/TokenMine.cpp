// Top-level game object: owns configuration, assets, window, input, camera,
// renderer and the voxel world; runs the main loop.
//
// Block interaction: a DDA voxel raycast from the eye selects a block
// (reach 5). Holding left button breaks the target (if
// properties.destroyable) over a data-driven duration (1.5 s × hardness /
// the selected item's break speed — see breakDurationSeconds); right click
// places the selected inventory slot's block item on the hit face (if the
// target cell is placeable and does not overlap the player) and consumes
// one from that stack. The hotbar is the first Inventory::kHotbarSize slots
// of the player's inventory (starting contents from
// assets/data/hotbar.json, Tab to cycle the selection, Q drops one from the
// selected slot). Broken blocks drop ItemEntity pickups that the player
// collects by walking over them. Each chunk owns up to two GPU meshes
// (opaque + transparent/blended); a chunk's meshes are rebuilt lazily when
// it is marked dirty (first meshing or a setBlock on/next to it).
#pragma once

#include <array>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "block/block_registry.hpp"
#include "core/asset_manager.hpp"
#include "core/camera.hpp"
#include "core/config.hpp"
#include "core/difficulty.hpp"
#include "core/error.hpp"
#include "core/food.hpp"
#include "core/input.hpp"
#include "core/logger.hpp"
#include "core/save.hpp"
#include "core/sound.hpp"
#include "core/status_effect.hpp"
#include "core/thread_pool.hpp"
#include "core/window.hpp"
#include "entity/entity.hpp"
#include "entity/item_entity.hpp"
#include "entity/item_mesh.hpp"
#include "entity/mob_entity.hpp"
#include "entity/mob_registry.hpp"
#include "entity/mob_spawning.hpp"
#include "entity/particle.hpp"
#include "entity/projectile.hpp"
#include "ui/debug_overlay.hpp"
#include "ui/font.hpp"
#include "ui/inventory_ui.hpp"
#include "item/campfire.hpp"
#include "item/crafting_grid.hpp"
#include "item/furnace.hpp"
#include "item/inventory.hpp"
#include "item/item_registry.hpp"
#include "item/recipe.hpp"
#include "player/drown.hpp"
#include "player/player.hpp"
#include "renderer/mesh.hpp"
#include "renderer/renderer.hpp"
#include "renderer/shader.hpp"
#include "renderer/texture.hpp"
#include "renderer/texture_atlas.hpp"
#include "world/ambience.hpp"
#include "world/biome.hpp"
#include "world/chunk.hpp"
#include "world/chunk_mesher.hpp"
#include "world/chunk_streaming.hpp"
#include "world/cloud.hpp"
#include "world/day_cycle.hpp"
#include "world/fluid.hpp"
#include "world/lighting.hpp"
#include "world/raycast.hpp"
#include "world/tree_shape.hpp"
#include "world/world.hpp"
#include "world/world_generator.hpp"

namespace mc {

// PRE-5: the three world-geometry render passes (the classic Minecraft
// order). Opaque: depth write on, no blend (the GL defaults, unchanged).
// Cutout: the `cutout` blocks (leaves, cross plants) + the pure-cutout
// item drops — drawn with alpha DISCARD active (u_discardEnabled) AND
// depth write on, so the texture holes write no depth (the visibility
// fix). Blend: the remaining translucent blocks (glass, the pane, the
// fluids) + the translucent items — blended, depth write OFF (a
// translucent surface must not occlude what is behind it). With
// render.alpha_test off, the cutout section loses its discard, the blend
// section keeps the depth write (the GL default), and the pipeline is
// bit-identical to pre-PRE-5 (see the game's render()).
enum class RenderPass {
    Opaque,
    Cutout,
    Blend,
};

struct GameArgs {
    std::filesystem::path configPath = "config/config.json";
    int frames = 0;        // 0 = run until closed
    std::int64_t seed = -1; // -1 = use config value
    double time = -1;      // -1 = use config start_time_of_day (else [0, 1))
    LogLevel logLevel = LogLevel::Info;
    std::filesystem::path screenshotPath; // if set: save after the last frame
    // Save slot (Phase 13): --save <slot>. At startup loads
    // data/saves/<slot>.json when it exists (unless newWorld); on a clean
    // exit writes it back. Unset = no save/load at all (the default —
    // pre-Phase-13 behavior, so existing runs are unaffected).
    std::optional<std::string> saveSlot;
    // --new: ignore an existing save for the slot (fresh world); the slot
    // is overwritten on exit. Requires saveSlot.
    bool newWorld = false;
    // --save-full-diff (PIPE-2): force the legacy full-world diff at exit
    // (diffWorld + the stair world scan) instead of the journal path —
    // the in-process equivalence oracle and the escape hammer. A debug
    // flag: no config key, no save-format effect.
    bool saveFullDiff = false;
};

class Game {
public:
    // Which crafting panel (Phase 17) is open (None = nothing open).
    enum class OpenPanel {
        None,      // no panel
        Inventory, // E: 2x2 grid + the shapeless recipe list
        Table,     // right-clicked crafting table: the 3x3 grid
        Furnace,   // Phase 40: right-clicked furnace: the smelting GUI
    };

    // The crafting UIs (Phase 15/17) + the furnace UI (Phase 40) hold
    // references to the inventory, the crafting grids and the registries,
    // so they are constructed here (after all of them, in declaration
    // order). Phase 44: the furnace UI binds its state per panel opening
    // (per-block furnace states — see openFurnacePanel). CRAFT-1: the
    // inventory UIs bind their grid per panel opening too — the E panel
    // to the player's own grid (bound once, in the constructor body), the
    // table panel to the clicked block's per-block grid (bound on every
    // open — see openTablePanel).
    Game() : m_inventoryUi(m_inventory, m_items, m_recipes),
             m_tableUi(m_inventory, m_items, m_recipes),
             m_furnaceUi(m_inventory, m_items) {
        m_inventoryUi.bind(m_inventoryGrid);
    }
    ~Game();

    Result<void> init(const GameArgs& args);
    // Runs the main loop. Returns the process exit code (0 = clean exit).
    int run();

    const Config& config() const { return m_config; }
    AssetManager& assets() { return m_assets; }
    const World& world() const { return m_world; }

private:
    // CPU geometry of one chunk (the mesher's output, kept in CPU memory
    // so a column's merged GL mesh can be rebuilt when any of its eight
    // chunks re-meshes). Phase 99.9.
    // GPU geometry of one COLUMN (all eight vertical chunks merged into
    // one opaque + one transparent sub-mesh). 8x fewer draw calls and
    // buffer objects than per-chunk meshes: at view distance 80 the
    // per-chunk submission was ~10-30 us/draw on the Intel iGPU and grew
    // with the ring (the vd80 render collapse). Vertices bake the chunk's
    // world offset, so the column model translates in X/Z only.
    struct ColumnMesh {
        std::optional<Mesh> opaque;
        // PRE-5: the cutout sub-mesh (leaves, cross plants — the discard +
        // depth-write pass); the transparent one below is the
        // translucent-BLEND sub-mesh (glass, the pane, the fluids).
        std::optional<Mesh> cutout;
        std::optional<Mesh> transparent;
    };

    // One screen-space draw batch (Phase 15 UI): all quads/glyphs with the
    // same color + texture are merged into a single non-indexed mesh, so
    // the whole UI is a handful of draw calls.
    struct UiBatch {
        std::vector<MeshVertex> vertices;
        Vec4 color {1.0f, 1.0f, 1.0f, 1.0f};
        bool textured = false;
        const Texture* texture = nullptr;
    };

    Result<void> buildScene();
    void update(double dt);
    void render();
    // Rebuilds the GPU mesh of every column whose merged GL mesh is stale
    // (m_columnRebuildPending). PIPE-1: the mesher's COMPUTE no longer runs
    // here — worker mesh jobs (postMeshJobs / integrateMeshJobs) refresh
    // the chunk's CPU geometry (m_chunkMeshData) against a private snapshot
    // World; what remains on the main thread is the column-merged GL
    // upload (rebuildColumnMesh — the GL context is main-thread only).
    Result<void> updateChunkMeshes();
    // Concatenates the column's chunk geometries (baking each chunk's
    // world offset into the vertices, remapping the indices) and uploads
    // the merged opaque + transparent sub-meshes.
    Result<void> rebuildColumnMesh(const ColumnPos& col);

    // Chunk streaming (Phase 14). integrateStreamedColumns runs after the
    // pool flush: worker-generated columns are inserted into the World
    // (insertChunk marks the chunk + face-sharing neighbors dirty, so the
    // next updateChunkMeshes re-meshes them and the boundary). The two
    // post* methods run last in update, after every main-thread World
    // write: from then on until the next flush the workers own the window.
    void integrateStreamedColumns();
    // Collects the completed lighting jobs (after the pool flush):
    // STREAMING job columns (LightJob::fluid = false) leave
    // m_unlitColumns, so the next updateChunkMeshes builds their meshes
    // with the propagated light (the one-and-only mesh); FLUID catch-up
    // job columns (99.11, fluid = true) leave m_fluidLightPending (the
    // set was already drained at post time — the reap's erase is the
    // contract belt, and m_unlitColumns is never touched: fluid columns
    // are not unlit columns). Every job's net-changed chunks (collected
    // job-locally by the workers, 99.10) join the World's dirty set
    // here — the main-thread-writer window, no worker in flight.
    void collectLitColumns();
    // Decides the columns to stream around the player (trigger + radius,
    // see chunk_streaming) and posts up to kStreamColumnsPerFrame
    // generation jobs to the pool.
    void streamChunks();
    // Finds chunks that appeared this frame (streamed-in or created by the
    // placement path) and posts their worker lighting jobs (99.10): up to
    // TWO per frame — two pending columns whose light regions are
    // bit-level independent (planLightJobs, world/light_job.hpp: the
    // write footprint is the column box ±kLightUpdateRadius and the
    // block-light BFS reads one more column ring, so a column gap >=
    // kLightParallelColumnGap on either axis separates them) get one job
    // each and run in parallel on two workers; a dependent pair (or 3+
    // columns) stays one job, boxes sequential (the pre-99.10 behavior).
    // The chunks render one frame dark and are re-meshed with the
    // correct light the frame after the job completes (deterministic,
    // frame-locked by the flush).
    void postLightJobs();
    // 99.11: posts the budgeted fluid catch-up light jobs (the worker
    // twin of the old synchronous bounding-box re-light in the fluid
    // tick — that call was the frame's entire cost in the shipped ocean,
    // step-0: 130..661 s per 300-frame window). Runs LAST in update,
    // after postLightJobs: the budget (planFluidLightJobs: at most
    // kFluidLightColumnsPerFrame columns per frame from
    // m_fluidLightPending in set order, skipping any candidate whose
    // light region is not lightRegionsIndependent of a streaming job's
    // column — the 99.10 parallel safety is per-plan, and a shared
    // window would let two workers touch one chunk's light arrays) is
    // partitioned like planLightJobs (independent pair -> two parallel
    // jobs, dependent pair -> one sequential job). The taken columns
    // leave m_fluidLightPending at POST time (an in-flight column is
    // never re-taken; a re-churn re-adds it after the job lands). Each
    // job runs updateRegion(columnLightBox) with newTerrain = true
    // (the 99.9 no-op skip would strand a fluid-opened horizontal light
    // path dark — the streaming jobs pay the same cost) and collects
    // its net-changed chunks job-locally (dirtyOut; collectLitColumns
    // applies them at the flush — the fluid columns' chunks get
    // re-meshed with the new light, the 99.4 lit-pop-in discipline).
    // A settled world changes nothing: the pending set stays empty and
    // this function is a no-op (structural immunity for the base gate).
    void postFluidLightJobs();
    // PIPE-1: posts up to kMaxMeshBacklog worker mesh jobs for the nearest
    // eligible dirty columns (selection: world/mesh_job.hpp). Each job
    // snapshots the column's 3x3 column neighborhood into its own World
    // (taken after every main-thread World write of this frame) and meshes
    // the column's 8 chunks on a worker. Runs in update AFTER streamChunks
    // (gen posts) and BEFORE postLightJobs — the post order gen -> mesh ->
    // light is what keeps this frame's lighting job the only live-World
    // writer after the snapshots (see the pipeline contract above).
    void postMeshJobs();
    // PIPE-1: collects the completed mesh jobs (after the pool flush,
    // BEFORE integrateStreamedColumns — a stream insert marks neighbor
    // columns dirty, which must not be swallowed by a mesh integrate's
    // clear-dirty): the 8 ChunkMeshData outputs move into m_chunkMeshData
    // (whole-column replace), the 8 chunks clear their dirty flag (at
    // integrate, not post: an edit after this frame's integrate re-marks
    // the chunk and re-queues it), and the column joins
    // m_columnRebuildPending for the GL upload. FIFO = deterministic.
    void integrateMeshJobs();
    // Unloads the loaded columns far behind the player (the memory leak
    // fix: streaming only ever ADDED chunks — a radius-80 session grew to
    // 11k+ chunks / several GB and kept rising). Runs right after
    // integrateStreamedColumns (the pool is flushed: no worker in flight),
    // the same safe main-thread-writer point. A column survives when it is
    // within view_distance + kUnloadHysteresis, part of the protected 4x4
    // demo area (respawn safety), player-edited (m_editedColumns — the
    // save records its diff and must keep the chunks), tracked by per-block
    // state (furnace/campfire/crop/beacon/fire — the save cross-checks
    // those entries against the block edits), or awaiting a generation job
    // (m_streamPending). Unloading erases the chunks (eraseChunk re-meshes
    // the boundary neighbors), frees the GL mesh buffers, and drops the lit
    // set entries.
    void unloadFarColumns();
    Result<void> takeScreenshot(const std::filesystem::path& path);

    // Draw the sun, moon and starfield (unlit billboarded quads, blended).
    // Called between the opaque and transparent chunk passes.
    void drawSkyObjects(const Mat4& view, const Mat4& projection,
                        int viewportHeight);

    // Pointer-lock state machine + block interaction (called from update).
    void handleInteraction(double dt);
    // The block the crosshair is aiming at (raycast from the eye, within
    // kReach), if any.
    std::optional<BlockHit> targetBlock() const;
    // Hold-to-break state machine (Phase 11): advances the progress while
    // the crosshair stays on the same destroyable block and breaks it when
    // the data-driven duration elapses (see breakDurationSeconds).
    void updateBreaking(double dt);
    void resetBreaking();
    // Mob targeting + attacking (Phase 20): the mob under the crosshair
    // (the nearer of the block hit and the mob AABB hits wins — blocks
    // occlude), and the swing state machine (one hit per kSwingInterval
    // while the button stays down on the mob: data damage + knockback).
    struct MobHit {
        MobEntity* mob;
        float distance;
    };
    std::optional<MobHit> targetMob() const;
    void updateAttacking(double dt, MobEntity& mob);
    // Melee damage of the selected item (data; bare hand = 1.0).
    float selectedAttackDamage() const;
    // Bow launching (Phase 21, charge + ammo Phase 24): the selected
    // item's launch data (data 'shoot_speed' / 'shoot_damage'; 0 = the
    // selected item is not a launcher). While a launcher is selected the
    // RIGHT button draws (a press starts the charge when ammo is
    // available — the click is consumed, no interact/place; the hold
    // accumulates draw time, capped at kBowFullDrawSeconds) and the
    // RELEASE fires one arrow with the charge's power (core/bow.hpp),
    // consuming one ammo item (data 'ammo'; the lowest-index slot that
    // holds it). The LEFT button is the bow's melee swing (the item's
    // data attack_damage) on a mob under the crosshair; a launcher never
    // mines.
    float selectedShootSpeed() const;
    float selectedShootDamage() const;
    // True when the selected launcher can fire: no 'ammo' data = always;
    // otherwise at least one ammo item in the inventory.
    bool ammoAvailable() const;
    void updateBowDraw(double dt);
    // Fires one arrow with the charge's power (see updateBowDraw),
    // consuming one ammo item; a draw with no ammo left is cancelled
    // silently. Resets the draw state.
    void fireBow();
    // Cancels an in-progress draw (item switch, unlock, panel, ...).
    void resetBowDraw();
    // The single choke point for every in-game block change (break,
    // place, dye recolor): the raw world setBlock (which marks the chunk
    // dirty) plus, Phase 42, releasing any STUCK arrow whose box overlaps
    // the changed cell (it falls out and rejoins flight physics).
    void setBlock(int x, int y, int z, BlockID id);
    // Performs the break: air + relight + data-driven drop (ItemEntity).
    // The caller guarantees the hit is destroyable.
    void breakBlock(const BlockHit& hit);
    // Places one from the selected hotbar slot (block items only) and
    // consumes it from the stack.
    void placeBlock();
    // Eats one from the selected hotbar slot when it holds food (Phase 23)
    // and the player is not at full hunger: restores the item's data
    // restore_hunger, consumes the stack entry, and plays the eat sound.
    // Returns true when the click was consumed by eating (food selected,
    // including the no-op case at full hunger), so the caller skips the
    // normal right-click (interact/place) behavior for that click.
    bool tryEat();
    // Drinks one from the selected hotbar slot when it holds a drinkable
    // container (Phase 43, item data 'drink_time'): arms the shared eat
    // channel with the item's DATA-DRIVEN duration. Completing it consumes
    // one, returns the item's 'returns_item' container to the inventory
    // and clears the player's status effects (v1: the hurt flash). The
    // drink starts only when the returned container would fit (a full
    // inventory is a no-op, the click is still consumed). Returns true
    // when the click was consumed by drinking.
    bool tryDrink();
    // Milks a milkable mob (Phase 43, mob data 'milk_with' / 'milk_gives'):
    // the crosshair must be on a mob whose 'milk_with' names the selected
    // item (melee targeting: kReach + block occlusion). Consumes one held
    // item, adds one 'milk_gives' item; the mob is unharmed (v1). Returns
    // true when the click was consumed (crosshair on the mob, including
    // the no-milk case of a full inventory).
    bool tryMilk();
    // Fluid bucket (Phase 47): when the selected slot holds a bucket side
    // (item data 'collects_fluid' or 'pours_fluid'; the pair is
    // cross-checked at init): COLLECT — the bucket-target ray (kReach,
    // first non-air-block-or-fluid cell) must land on a level-8 (source)
    // fluid cell: the source is removed and one held item is swapped for
    // the companion (the full bucket). POUR — the placement-target cell
    // (face neighbor of the hit block) must be air without fluid and not
    // overlapping the player: a level-8 source is placed and the item
    // swaps to the empty bucket. Both play the "splash" sound and consume
    // the click (a bucket never places a block); a no-action case
    // (flowing water, occupied cell, no target) falls through to the
    // normal right-click (placeBlock is a no-op for a material).
    bool tryFluidBucket();
    // Armor (SURV-1): the right-click channel of the armor pieces. A
    // HELD piece equips onto its data slot (an occupied slot SWAPS the
    // piece with the held one — Minecraft); an EMPTY hand unequips the
    // FIRST occupied slot in the fixed order head -> chest -> legs ->
    // feet (repeated right-clicks strip the whole set, deterministic).
    // The click is consumed whenever the hand holds a piece (armor is a
    // material — placeBlock would be a no-op anyway — and the channel
    // owns the right button) or a piece is unequipped; nothing in hand
    // + nothing worn falls through to the next channel (the sneak-
    // rotate can still fire on an empty hand). Returns true when the
    // click was consumed.
    bool tryArmor();
    // Advances the eat/drink channel in real dt (Phase 28, drinkable
    // extension Phase 43): interrupts (damage, death, unlock, panel,
    // selection change) and the terminal events (complete = consume +
    // food restore OR drink return-container + clear status effects,
    // early release = cancel).
    void updateEatChannel(double dt);
    void resetEatChannel();
    // Clears the player's status effects on a completed drink (Phase 43,
    // completed by the Phase 46 system): resets the red damage veil
    // (m_hurtFlash) and removes every NEGATIVE active effect (Minecraft
    // milk; the positive effects, like regeneration, survive). Fall
    // damage is deliberately NOT cleared (Minecraft parity: it is not a
    // status effect).
    void clearStatusEffects();
    // Advances the player's active status effects by REAL dt (Phase 46,
    // the survival-timer class): durations count down, tick effects fire
    // their damage (the min_health floor — poison never kills) / healing
    // per interval, and expired effects are removed. A no-op while the
    // player is dead or has no active effects.
    void updateEffects(double dt);
    // Phase 64 fire: the real-dt burn-out countdown for every burning
    // fire block (a fire that reaches zero becomes air — the burn leaves
    // no char, like the Phase 58 lava burn) + the water extinguish (a
    // fire with a water fluid 6-adjacent is put out instantly — water
    // beats fire, the Phase 57 water-beats-lava rule). Runs every frame
    // after the fluids tick (so the newly burned cells from the Phase 58
    // pass are registered and the new water is visible). A no-op when
    // the world holds no fire. Phase 66 adds, on the same pass: the
    // per-tick fire-ember particle emission (fixed-frame step, gated on
    // m_fireParticles) and the death burst on burn-out/water extinguish.
    void updateFires(double dt);
    // Phase 66 fire spread (Fire v2): every fire.spread_interval_frames
    // frame, each live fire cell rolls its 5 spread neighbors (4
    // horizontal + above — never downward, Minecraft) for a `flamable`
    // block (fire.spread_chance per roll, world/fire_spread) and ignites
    // the passes with setBlock(FIRE) (the choke point registers the new
    // fire's full life). Runs every frame after updateFires; a no-op
    // without fires or without the fire block.
    void updateFireSpread();
    // Phase 83 beacon: re-derive the power level of EVERY tracked beacon
    // (world/beacon — a pure pyramid scan of the `beacon_base` layers
    // below it) and, for each level >= 1, apply its NAMED effect to the
    // player while the player is within `beaconRange` blocks of the
    // beacon (the duration refreshes every frame, the fire-source pattern
    // — standing in range keeps it active). Also fills `m_beaconActive`
    // (the level>=1 beacons) for the beam renderer. A no-op while the
    // world holds no beacon (structural immunity).
    void updateBeacons();
    // PRE-4 ambience: the day/night/cave ambient sound (the frame-locked
    // schedule, world/ambience): once every kAmbienceIntervalFrames the
    // pure roll of (world seed, frame, context) decides, and a pass plays
    // the context's sound 2D (the ambience is global — never
    // spatialized) + the "Ambience: <name>" log line (the E2E evidence).
    // Audio never enters the frame state (the CRAFT-3 discipline — the
    // sound is heard, never seen or saved). The context reads the
    // player's feet cell's RAW sky light (0 = cave, priority) + the
    // dayFactor split. audio.ambience (code default true) gates the
    // pass; OFF = zero events, zero sounds (the gate's "new-off"
    // control).
    void updateAmbience();
    // Phase 72 (the two-component model): set the player's absorption
    // pool to the active effects' total `absorb_hp`. The pool is a pure
    // function of the effects AT EVERY SYNC POINT (a refreshed layer
    // restores the full sum — Minecraft's attribute-recalculation
    // semantics); between sync points damage drains it (the drain
    // persists until the next sync point or the expiry). Called at every
    // effect change (eat-grant, mob on_hit_effect, expiry in
    // updateEffects, milk clear, death clear). The SAVE LOAD does not
    // call this: the format-12 pool is authoritative (it may sit below
    // the total) and the format-11 migration derives it (applySave).
    void syncAbsorption();
    // SURV-4 (potions): the single choke point for applying a NAMED status
    // effect to the player. Resolves the effect (init cross-checks every
    // grant source names an existing effect, so idForName cannot fail),
    // applies it with the RESET semantics (an already-active instance is
    // re-timed, the tick phase kept), and settles any INSTANT action the
    // effect data carries (a positive instant_hp = heal, a negative one =
    // damage with the armor EXEMPTED — Minecraft's magic/potion damage, the
    // SURV-2 drowning bypass twin). Effects without an instant action are
    // unchanged (the pre-SURV-4 apply sites behave identically through the
    // helper). The CALLER owns the surrounding syncAbsorption() (the pool
    // is a pure function of the effects at every sync point).
    void applyPlayerEffect(const std::string& name, double duration);
    // Cycles the hotbar selection; resets break progress (the selected
    // tool changed, Minecraft behavior).
    void cycleHotbar();
    // Drops one from the selected hotbar slot at the player's feet (Q).
    void dropSelectedItem();
    // Effective break speed of the selected item against `target`
    // (Phase 67): tools use their data break_speed — times the
    // fast_against multiplier when the target block is one the tool is
    // fast against (effectiveBreakSpeed); hand (empty slot) and block
    // items break at 1.0.
    float selectedBreakSpeed(const Block& target) const;

    // Entity simulation (Phase 10): per-entity physics, then the item merge
    // pass and despawn (removal is deferred so the merge order — and
    // therefore the merge result — is deterministic).
    void updateEntities(double dt);
    // Dropped-item rendering: the items belonging to the given pass
    // (opaque / cutout / blend — PRE-5's three-way routing), bobbing +
    // spinning around their rest position (frame-counter driven — see
    // kItemBobPeriodFrames).
    void drawItemEntities(const Mat4& view, const Mat4& projection,
                          RenderPass pass);
    // The cached full-cube mesh for a dropped block (built lazily on first
    // render; the geometry is cached, only the draw transform varies).
    const Mesh& itemMesh(BlockID id);
    // CRAFT-2: the cached box-model mesh for a model item (built lazily on
    // first render, like itemMesh; the geometry is static per item).
    const Mesh& itemModelMesh(ItemID id);

    // Mob rendering + spawning (Phase 20): the mobs are drawn after the
    // item entities with their cached box-model mesh (translate by the
    // mob's bottom-center, rotateY by its facing). attemptMobSpawn runs on
    // the data cadence (see mob_spawning) and pushes one MobEntity at the
    // deterministic scan's cell; it is a no-op unless the spawn config
    // allows it (cap, cadence, first frame, enabled flag).
    //
    // ENT-1 limb swing: a mob that is actively swinging (config on + the
    // type has swing data + the phase != 0) draws a per-entity DYNAMIC
    // mesh instead: the frame's vertices (the swing parts rotated about
    // their pivots, the rest verbatim — buildSwingFrame) rewrite the VBO
    // (glBufferSubData); phase 0 / no swing data / config off take the
    // STATIC per-type cache (the fast path = structural immunity).
    void attemptMobSpawn();
    void drawMobs(const Mat4& view, const Mat4& projection);
    const Mesh& mobMesh(MobID id);
    const Mesh& swingMobMesh(const MobEntity& mob, const Mob& def);
    const SwingModel& swingModel(MobID id);

    // Arrow rendering (Phase 21): the arrows (the P10 Projectiles the bow
    // launches) draw after the mobs in the opaque pass with the shared
    // box mesh, oriented along their velocity (model = translate *
    // rotateY(yaw) * rotateX(pitch), local -Z = travel direction, see
    // arrowOrientation), in the basic shader's solid-color path
    // (u_useTexture 0).
    void drawArrows(const Mat4& view, const Mat4& projection);
    const Mesh& arrowMesh();

    // 3D held hand (Phase 42, the Minecraft hand): while the pointer is
    // locked and no panel is open, the selected item renders as a view-
    // anchored 3D model in the bottom-right of the frustum — the block
    // cube for block items (the dropped-item cube mesh, atlas-lit), a
    // flat icon quad for the other icon items (always bright). While the
    // bow is drawn the hand pulls back with the charge (the draw
    // indicator, replacing the Phase 24 2D icon pull); while food is
    // being eaten it rises toward the face with the eat progress. Pure
    // rendering — no state, and gated on the pointer lock, so a no-input
    // run renders the pre-Phase-42 image (byte-identical baseline).
    void drawHeldHand(const Mat4& view, const Mat4& projection);
    const Mesh& handIconMesh();
    // Phase 71: the item cooking on each campfire (the non-empty slots)
    // as a small crossed icon quad above the block (the cross-block
    // convention — view-independent, opaque pass, depth-occluded like
    // everything else). No-op without a cooking campfire (a no-input run
    // never renders it — baseline byte-identical).
    void drawCampfireCookItems(const Mat4& view, const Mat4& projection);
    // Phase 83 beacon beam: the translucent light column rising from every
    // ACTIVE (level>=1) beacon to the sky, drawn in the blending pass (the
    // depth test occludes it behind terrain like the other world geometry).
    // A no-op without an active beacon (baseline byte-identical); gated on
    // the `beacon.beam` config (the E2E control, the fire.particles
    // pattern).
    void drawBeaconBeams(const Mat4& view, const Mat4& projection);
    // PRE-4 clouds: the drifting translucent cloud layer at
    // y = kCloudHeight (the 16-block cell grid, world/cloud), drawn in
    // the blending pass with the basic program's unlit-white quads (the
    // beam pattern: u_useTexture = 0 + u_alpha, culling off, the depth
    // test occludes it behind terrain). The cached mesh is rebuilt ONLY
    // when the camera's cloud cell or the drift time step changes
    // (no-input = static from the first draw; walking = one rebuild per
    // 16 blocks — the 99.x discipline, no per-frame VBO upload).
    // render.clouds (code default true) gates the draw; OFF = zero mesh,
    // zero draw (the v0.27.0 pipeline, bit-identical).
    void drawClouds(const Mat4& view, const Mat4& projection);

    // -- Phase 15 UI / audio / particles ------------------------------------
    // E/F3/F5 handling (F3 + F5 work in any pointer state; E opens the
    // inventory only from the captured state and closes it from the free
    // state, ESC closes any open panel too).
    void handleUiKeys();
    // The shapeless recipes in registry order (the E panel's craft list —
    // shaped recipes are crafted in the grids, not from the list).
    std::vector<const Recipe*> shapelessRecipes() const;
    // Click routing while a crafting panel is open (Phase 17): shapeless
    // craft rows (the E panel only), then the output slot, a grid cell or
    // an inventory slot (the active panel's InventoryUI).
    void handlePanelClicks();
    // Opens the given panel (releasing the pointer) / closes the open one.
    void openPanel(OpenPanel panel);
    void closePanel();
    // Phase 44: opens the furnace panel for the block at `pos` — binds the
    // panel's UI to that block's state (created on demand; a furnace block
    // always owns one — see setBlock).
    void openFurnacePanel(const BlockPos& pos);
    // The state of the furnace block at `pos` (default-constructs one when
    // absent — defensive: the setBlock invariant makes that unreachable).
    FurnaceState& furnaceAt(const BlockPos& pos);
    // CRAFT-1: opens the table panel for the block at `pos` — binds the
    // panel's UI to that block's per-block grid (created on demand; a
    // crafting_table block always owns one — see setBlock). The
    // openFurnacePanel twin (Phase 44).
    void openTablePanel(const BlockPos& pos);
    // The grid of the table block at `pos` (default-constructs one when
    // absent — defensive: the setBlock invariant makes that unreachable).
    CraftingGrid& tableAt(const BlockPos& pos);
    // Phase 71: the right-click campfire cooking interaction (the
    // kInteractCampfire branch of the right-click chain): a selected
    // smeltable item places one unit on an EMPTY slot (consumed from the
    // hand); an empty hand takes the slot's item back (Minecraft: the take
    // needs a free hand); every other combination is a no-op. The click is
    // consumed either way (a campfire is never placed through this
    // branch).
    void tryCampfireCook(const BlockPos& pos);
    // The state of the campfire block at `pos` (default-constructs one
    // when absent — defensive: the setBlock invariant makes that
    // unreachable).
    CampfireCookState& campfireAt(const BlockPos& pos);
    // Footstep sound timer (moving + on ground, pointer captured).
    void updateSoundTimers(double dt);
    // SAVE-1: the auto-save pass (REAL dt, the survival-timer class —
    // hard rule 8: the accumulator is not frame state, it is never saved
    // and never touches the deterministic frame, the audio-timer class).
    // Gated on a --save slot (structurally inert without one) and the
    // configured interval (save.auto_save_seconds, 0 = off); when the
    // interval elapses it writes the full save (writeSave, atomic) — a
    // success zeros the accumulator, a failure keeps it armed for the
    // NEXT frame's retry (ERROR log; a broken save path must not drop
    // the save window).
    void updateAutoSave(double dt);
    // Phase 16 survival polling: a health drop drives the hurt flash +
    // sound, a death releases the pointer (the death screen is rendered
    // from the player's state) and plays the death sound, and R (while
    // dead) respawns the player.
    void updateSurvival(double dt);
    // World-space overlay: the break crack on the target block + the
    // target outline (drawn in the blending pass, depth test on).
    void drawWorldOverlay(const Mat4& view, const Mat4& projection);
    // Particle batch for one pass (dynamic mesh, rebuilt per frame;
    // split by the block's pass — opaque / cutout / blend — like the
    // item entities, PRE-5).
    void drawParticles(const Mat4& view, const Mat4& projection,
                       RenderPass pass);
    // The whole screen-space pass: hotbar + crosshair + held item (while
    // captured), the inventory/craft panel (while open), the F3 overlay
    // (while toggled). Runs with depth test + culling off; every element
    // is gated on a key/pointer state, so the no-input baseline renders
    // nothing extra.
    void renderUi();
    // The UI geometry space in whole "UI pixels" (Phase 34): the window's
    // LOGICAL size divided by the ui.scale config key, rounded. Every
    // screen-space element (hotbar, hearts, panels, F3, death screen,
    // held item) is laid out in this space, and uiProjection() stretches
    // it onto the drawable — the GPU does the upscaling (NEAREST textures
    // stay crisp). At dppx 1 with ui.scale 1.0 the space is exactly the
    // drawable pixels (the no-input baseline).
    struct UiSize {
        int width = 0;
        int height = 0;
    };
    UiSize uiSize() const;
    // The mouse position in UI pixels: SDL delivers LOGICAL coordinates,
    // which divide by ui.scale (the drawable stretch is a uniform scale).
    std::pair<int, int> uiMouse() const;
    // Screen-space ortho projection (y-down: y = 0 is the window top).
    Mat4 uiProjection(int w, int h) const;
    // Uploads one UI vertex batch and draws it with the UI shader.
    void drawUiBatch(const std::vector<MeshVertex>& vertices,
                     const Mat4& projection, const Texture& texture,
                     bool textured, const Vec4& color);

    // Save/load (Phase 13), opt-in via --save <slot>. The file
    // data/saves/<slot>.json holds the seed + the cells that differ from
    // that seed's generation + the full game state (see core/save.hpp).
    std::filesystem::path savePath() const;
    // Gathers the current state into a SaveGame (world diff + player +
    // inventory + entities + time).
    Result<SaveGame> collectSave() const;
    // Serializes collectSave() to the slot file (logs the result).
    // SAVE-1: the write is ATOMIC (writeAtomicFile: same-directory temp
    // + rename) — a crash mid-write cannot corrupt the slot. byAutoSave
    // picks the log line ("World auto-saved to" for the periodic pass,
    // "World saved to" for the exit / F5 passes — the E2E asserts on
    // the line).
    Result<void> writeSave(bool byAutoSave = false) const;
    // Reads + strictly parses the slot file (cross-checked against the
    // registries and the world seed).
    Result<SaveGame> readSaveFile(const std::filesystem::path& path) const;
    // Applies a parsed save: cell edits + relight, player, inventory
    // (replacing the hotbar.json fill), entities, day-cycle time.
    Result<void> applySave(const SaveGame& save);
    // No-op unless --save is set, the slot is not --new, and the file
    // exists; then reads + applies it.
    Result<void> tryLoadSave();
    // Rebuilds the crop-growth tracking set from the world: every cell
    // whose block has a non-empty growsTo (Phase 48). Runs at the end of
    // buildScene — after a save load, whose block edits restore the
    // planted crops (no save-format field: the block id IS the stage).
    void rescanCrops();
    // GEN-2: rebuilds the sapling tracking set from the world — every cell
    // whose block carries a `tree` object (the rescanCrops twin; the
    // stage is NOT rescanned, it lives in the world's state byte and
    // survives in the format-15 save segment). Runs at the end of
    // buildScene — after the save load, whose block edits restore the
    // planted saplings.
    void rescanSaplings();
    // GEN-2: places a grown tree at a COMPLETED sapling cell: the
    // planTree cells (the rolled trunk height) through the setBlock
    // choke point — trunk column first (the sapling cell becomes the
    // trunk base), canopy after — then ONE lighting updateRegion over the
    // tree's bounding box (the canopy shadow, LGT-2 makes it visible) and
    // the tracking entry drops (the choke point already erased it when
    // the root cell became trunk — this is the defensive belt).
    void placeGrownTree(const BlockPos& saplingPos, int height);
    // Rebuilds m_flammableChunks from the world (end of buildScene: the
    // generated demo area, or the save's restored chunks).
    void rescanFlammableChunks();
    // Phase 83 beacon: rebuilds the beacon tracking set from the world —
    // every cell whose block has a non-empty beaconEffect. Runs at the end
    // of buildScene (like rescanCrops; no save-format field: the block is
    // a beacon, the level is derived each frame).
    void rescanBeacons();
    // The pure generator as a World chunk callback — used at world
    // creation AND as the generation reference for the save diffs (the
    // full diffWorld and the PIPE-2 journalDiff).
    World::ChunkGenerator makeChunkGenerator() const;

    GameArgs m_args;
    Config m_config;
    float m_uiScale = 1.0f; // ui.scale (Phase 34); 1.0 = pixel-exact
    AssetManager m_assets;
    Window m_window;
    Input m_input;
    Camera m_camera;
    Renderer m_renderer;

    // Data-driven content.
    BlockRegistry m_blocks;
    BiomeRegistry m_biomes;
    TextureAtlas m_atlas;

    // Voxel world + per-chunk GPU meshes.
    World m_world;
    std::unordered_map<ChunkPos, ChunkMeshData> m_chunkMeshData;
    std::unordered_map<ColumnPos, ColumnMesh> m_columnMeshes;
    // Columns whose merged GL mesh is stale (a chunk mesh landed this
    // frame — integrated from a worker mesh job, PIPE-1): rebuilt
    // (GL upload) at the end of updateChunkMeshes.
    std::set<ColumnPos> m_columnRebuildPending;

    // Seeded world generator (built in buildScene). Optional: the game only
    // touches it after init succeeded.
    std::optional<WorldGenerator> m_generator;

    // The first-person actor (built in buildScene after the world exists;
    // it holds references to the world and the block registry).
    std::optional<Player> m_player;

    // Chunk streaming (Phase 14) + worker meshing (PIPE-1): worker pool
    // for out-of-area terrain and off-main-thread remeshing. The
    // per-frame pipeline (see update) makes this race-free:
    //   flush -> integrate (main writes) -> interaction/fluids (main writes)
    //   -> GL rebuild + render (main reads only) -> post jobs in the order
    //   gen -> mesh -> light. Workers run from the posts until the next
    //   flush; a job therefore touches shared state only while the main
    //   thread is in its read-only window.
    //   - generation jobs touch no shared state at all (the generator is a
    //     pure function; results land in the job's own scratch chunks);
    //   - mesh jobs touch no shared state at all (the mesher runs on the
    //     job's OWN snapshot World — a private 3x3 column neighborhood
    //     copied at post time, after every main-thread World write of the
    //     frame and before the lighting job runs; results land in the
    //     job's own ChunkMeshData outputs);
    //   - lighting jobs write light arrays + the dirty flag, which the main
    //     thread never writes in its read-only window (the next frame's
    //     mesh snapshot is taken after the light lands; the light job also
    //     re-marks the chunks it changed dirty, so a mesh posted before the
    //     light re-queues after it — the selection rules in
    //     world/mesh_job.hpp).
    std::unique_ptr<ThreadPool> m_pool;
    // One in-flight generation job: a whole column
    // (kVerticalChunksPerColumn scratch chunks). `done` is written by the
    // worker and read by the main thread only after pool->flush()
    // (happens-before via the pool's mutex).
    struct StreamJob {
        ColumnPos column;
        std::unique_ptr<Chunk[]> chunks;
        bool done = false;
    };
    // In-flight generation jobs (deque: the heap-allocated StreamJob never
    // moves, so worker lambdas may capture it by raw pointer).
    std::deque<std::unique_ptr<StreamJob>> m_streamJobs;
    // Up to two in-flight lighting jobs per frame (99.10): the pending
    // columns are partitioned by planLightJobs — two columns whose light
    // regions are bit-level independent (write footprint + the
    // block-light boundary-read ring, world/light_job.hpp) get one job
    // EACH and run in parallel on two workers; a dependent pair (or 3+
    // columns) falls back to ONE job running its boxes sequentially (the
    // pre-99.10 behavior, bit-identical). The jobs write light arrays of
    // DISJOINT chunks only, and collect their net-changed chunk
    // positions in `dirtyChunks` instead of mutating the World's shared
    // dirty set (two parallel markChunkDirty calls would race);
    // collectLitColumns applies them at the next flush. The done flag +
    // columns let the main thread clear m_unlitColumns there (see
    // collectLitColumns).
    struct LightJob {
        std::vector<std::array<BlockPos, 2>> boxes;
        std::set<ColumnPos> columns;
        std::set<ChunkPos> dirtyChunks;
        bool done = false;
        // 99.11: the job's kind — false = a streaming job (its columns
        // leave m_unlitColumns at reap: the lit-gate is new-terrain
        // only), true = a fluid catch-up job (its columns are already
        // lit and left m_fluidLightPending at POST time; the reap's
        // erase is the contract belt — see collectLitColumns). The two
        // kinds never touch each other's bookkeeping: a newly integrated
        // column can have its fluid churned before its streaming job
        // lands, and the streaming reap must NOT drop that fluid pending
        // entry (its re-light would be lost); a fluid reap must not
        // touch m_unlitColumns (fluid columns are not unlit).
        bool fluid = false;
    };
    std::deque<std::unique_ptr<LightJob>> m_lightJobs;
    // One in-flight MESH job (PIPE-1): the target column's 3x3 column
    // neighborhood snapshotted into a throwaway World (insertSnapshotChunk,
    // after every main-thread World write of the posting frame), plus the
    // job's own 8 ChunkMeshData outputs (y-layer order). `done` is written
    // by the worker and read by the main thread only after pool->flush()
    // (happens-before via the pool's mutex). The worker never touches the
    // live World (the generation-job pattern).
    struct MeshJob {
        World snap;
        ColumnPos column;
        std::vector<ChunkMeshData> meshes; // sized kVerticalChunksPerColumn
        bool done = false;
    };
    // In-flight mesh jobs (deque: the heap-allocated MeshJob never moves,
    // so worker lambdas may capture it by raw pointer).
    std::deque<std::unique_ptr<MeshJob>> m_meshJobs;
    // Columns whose mesh job is in flight (not yet integrated). Same
    // paradigm as m_streamPending: they are excluded from re-posting
    // (nextMeshColumns) and protected from unloading.
    std::set<ColumnPos> m_meshPending;
    // Columns whose light has NOT propagated yet (integrated this session
    // or first-touch created, their lighting job still in flight). Their
    // chunks are NOT meshed until the job completes: the old design meshed
    // them at light 0 (black faces) and re-meshed after the light landed —
    // with the remesh budget that re-mesh could lag by the whole dirty
    // backlog, so freshly loaded terrain showed black faces for seconds
    // (99.2 walking-run report). Meshing once, lit (pop-in), is also less
    // work; PIPE-1 enforces the same rule in the worker selection (a
    // column in this set never gets a mesh job).
    std::set<ColumnPos> m_unlitColumns;
    // Columns with a generation job in flight (not yet in the World).
    std::set<ColumnPos> m_streamPending;
    // Columns the player has MODIFIED (the setBlock choke point — break /
    // place / rotate / growth / burn / campfire, plus the fluid-bucket
    // scoop and pour paths). Their diff is recorded in the save, so the
    // chunks must survive until the save is written: never unloaded.
    std::set<ColumnPos> m_editedColumns;
    // Streaming hysteresis (see chunk_streaming): engaged when the player
    // reaches the edge of the loaded area, disengaged when the player is
    // deep inside it again. While engaged, the full radius disk around
    // the player is filled (the capped view_distance becomes visible
    // terrain).
    bool m_streamingEngaged = false;
    // The streaming radius in chunk columns (config world.view_distance),
    // UNCLAMPED: the logs and the F3 overlay report this raw value; the
    // geometric consumers (stream disk, unload band) clamp it to
    // kStreamCapRadius (STRM-1 far-plane cap).
    int m_streamRadius = 8;
    // The loaded columns (incremental mirror of the World's chunk map —
    // streaming decides on THIS instead of scanning every chunk: the
    // scan is ~9k-200k map entries + a sort per frame at full ring).
    // Maintained by buildScene (initial), integrateStreamedColumns (+),
    // unloadFarColumns (-), and the setBlock choke point (+ a column it
    // completed for a first-touch edit). A column is "loaded" when ANY
    // chunk of it exists — the ensureColumn invariant makes "any" and
    // "all" equivalent.
    std::set<ColumnPos> m_loadedColumns;
    // Total columns unloaded since session start (the periodic INFO
    // progress line reports it — proof the unload pass is working).
    std::size_t m_unloadedColumnsTotal = 0;
    // The save journal (PIPE-2): the session's cell-edit history as
    // position sets, fed by the World's cell-mutation observer (installed
    // at the top of buildScene — BEFORE tryLoadSave, so the save load's
    // replay seeds it automatically) and evicted per column by
    // unloadFarColumns. At exit the save diff (journalDiff) + the stair
    // array derive from it instead of the O(N) world scans; when it is
    // invalid (the kSaveJournalMaxEntries cap was exceeded) or
    // --save-full-diff is set, collectSave falls back to the legacy
    // full diffWorld + stair scan.
    SaveJournal m_saveJournal;
    // STRM-2: the retained diffs of UNLOADED columns — a column that left
    // the world while the session modified it (the pure-fluid churn: far
    // lava flow / water film, the fire burn, the water-lava reaction —
    // the writes that go through World::setBlock/setFluid directly and so
    // never reach the m_editedColumns protection; their columns stay
    // unloadable by design). unloadFarColumns snapshots the column's
    // journal diff (retainColumnDiff, BEFORE the chunk erase) into this
    // map; integrateStreamedColumns re-applies a re-streamed column's
    // diff (blocks first, then fluids — the applyWorldEdits order) and
    // erases the entry; collectSave merges every entry into the exit
    // save (BOTH paths — the journal diff and the legacy/oracle full
    // diff). INVARIANT (the no-duplicate-cells proof of the merge): a
    // column is in at most ONE of the save journal and this map — unload
    // snapshots before it evicts the journal entries, integrate applies
    // before it erases the entry (the observer re-journals the re-
    // applied writes, so the next unload snapshots again: the self-
    // similar loop). The retained value is DIFF ENTRIES (a few to a few
    // dozen cells for a fluid column), not chunks — the memory stays
    // bounded over a long wandering session. Empty columns are never
    // stored.
    std::map<ColumnPos, RetainedColumnDiff> m_retainedColumns;
    // Chunks that (ever, since load/integration) hold a FLAMABLE block —
    // the burn pass's gate (Phase 99.8): a lava chunk walks only when it
    // or a face neighbor is in this set. Over-approximation by design:
    // a burn removes flammability without pruning the entry (exact on the
    // safe side — a miss would leave a neighbor unburned).
    std::set<ChunkPos> m_flammableChunks;
    // Columns that appeared since the last postLightJobs (integrated or
    // first-touch created): each gets one worker lighting job for its
    // full column box. Consumed (cleared) by postLightJobs. Replaces the
    // old m_litChunks "every covered chunk" set, whose only job was
    // detecting new chunks by a full per-frame scan.
    std::set<ColumnPos> m_lightPendingColumns;
    // 99.11: columns whose fluid tick changed cells since the last
    // fluid catch-up post (the coast film cascade: the old code re-lit
    // the bounding box of ALL changed cells synchronously on the main
    // thread — step-0: 130..661 s per 300-frame window in the shipped
    // ocean). The set merges every changed cell of a column into one
    // entry (changedCellColumns); the budget (postFluidLightJobs:
    // kFluidLightColumnsPerFrame per frame, worker jobs on the 99.10
    // infrastructure) drains it in set order — frame-locked, and the
    // per-column convergence argument (light_job.hpp) bounds the lag:
    // the coast churn rate exceeds the budget, so the set saturates at
    // the coast column count. Unloaded columns are evicted by
    // unloadFarColumns (no unbounded growth). A column may briefly sit
    // in BOTH this set and m_unlitColumns (a freshly integrated column
    // whose fluid churns before its streaming job lands) — the sets are
    // independent bookkeeping (the LightJob::fluid flag at reap: each
    // job drains only its own set), and window exclusivity (a fluid
    // candidate never independent of a streaming column is skipped this
    // frame) keeps the two re-light passes serialized, never parallel.
    std::set<ColumnPos> m_fluidLightPending;

    // Non-player entities (Phase 10): dropped items + projectiles. The
    // game is the sole owner — it updates them each frame, merges items,
    // and removes the despawned.
    std::vector<std::unique_ptr<Entity>> m_entities;
    // Dropped-item meshes: one full-cube mesh per block id (built lazily).
    std::unordered_map<BlockID, Mesh, BlockIDHash> m_itemMeshes;
    // 3D held-hand icon quad (Phase 42): the shared flat unit quad the
    // non-block items' 2D icons render through (built lazily, one for all
    // items — the per-item texture is bound at draw time).
    Mesh m_handIconMesh;

    // Mobs (Phase 20): the data-driven types (assets/data/mobs) + the
    // spawn config (assets/data/mob_spawning.json), the enable flag
    // (config mobs.enabled, default true), the per-type box-model meshes
    // (built lazily) and the spawn counter (feeds the per-mob RNG seeds,
    // so the same runs spawn the same mobs with the same seeds).
    MobRegistry m_mobTypes;
    MobSpawningConfig m_spawnConfig;
    bool m_mobsEnabled = true;
    // Difficulty (Phase 25): config mobs.difficulty (default "normal"),
    // strict-parsed; scales the hostile spawn cadence, the spawn-protection
    // radius and the hostile damage (core/difficulty.hpp). m_spawnPoint is
    // the WORLD spawn (the fresh-world spawn, before save load) — the
    // center of the spawn-protection circle.
    Difficulty m_difficulty = Difficulty::Normal;
    Vec3 m_spawnPoint {0.5f, 0.0f, 0.5f};
    std::unordered_map<MobID, Mesh, MobIDHash> m_mobMeshes;
    // ENT-1: the limb-swing config (mobs.limb_swing, default true; false
    // = the v0.9.0 static-gait baseline — the mobs.enabled pattern) + the
    // per-type swing geometry (built lazily on a swinging type's first
    // frame: the CPU copy of the static mesh + the per-part phase
    // sin/cos) + the per-entity dynamic meshes (allocated on the mob's
    // first swing frame, freed in the updateEntities despawn rebuild —
    // the entity pointer is stable: m_entities holds unique_ptrs, and
    // neither push_back nor the erase-rebuild moves the pointees) + the
    // frame-vertex scratch (one buffer shared by all mobs: drawMobs is
    // single-threaded on the game/GL thread).
    bool m_limbSwing = true;
    std::unordered_map<MobID, SwingModel, MobIDHash> m_swingModels;
    std::unordered_map<const MobEntity*, Mesh> m_swingMeshes;
    std::vector<MeshVertex> m_swingFrameVerts;
    std::uint64_t m_spawnCounter = 0;
    // Swing cooldown (seconds; Phase 20): one attack hit per
    // kSwingInterval while the left button is held on a mob. Reset to 0
    // on release / when the aim leaves the mob (the next swing is
    // immediate).
    double m_swingCooldown = 0.0;
    // Bow draw (Phase 24): true while the right button is held with a
    // launcher selected; m_bowDrawSeconds accumulates the draw in real dt
    // (capped at kBowFullDrawSeconds). Reset by fireBow/resetBowDraw and
    // by every state change that interrupts a draw (unlock, panel, item
    // switch).
    bool m_bowDrawing = false;
    double m_bowDrawSeconds = 0.0;
    // Eat channel (Phase 28, core/food.hpp): the Minecraft-style 1.6 s
    // eat animation — right-clicking with food selected starts the
    // channel (m_eatSlot/m_eatItem capture the slot and item; completion
    // consumes one and restores hunger + saturation). Interrupted by
    // damage, death, unlock, panel, selection change, early release, or a
    // left-button swing (none of these consume the food).
    EatChannel m_eatChannel;
    std::size_t m_eatSlot = 0;
    ItemID m_eatItem = ItemID::invalid();
    float m_eatStartHealth = -1.0f; // damage during the channel cancels it
    // The shared arrow mesh (Phase 21; built lazily on first render).
    Mesh m_arrowMesh;

    // Item/inventory system (Phase 11): item registry (implicit block
    // items + explicit tools/materials from assets/data/items), the
    // player's inventory (hotbar = first Inventory::kHotbarSize slots,
    // starting contents from assets/data/hotbar.json) and the selected
    // hotbar slot. Crafting recipes (Phase 12) are loaded and validated at
    // startup; the in-game crafting UI/trigger is Phase 15.
    ItemRegistry m_items;
    Inventory m_inventory;
    std::size_t m_selectedSlot = 0;
    RecipeRegistry m_recipes;

    // Hold-to-break state (Phase 11): the cell being broken and how long
    // it has been held (seconds). Reset on target change, pointer unlock,
    // selection change, and completion.
    std::optional<BlockPos> m_breakTarget;
    double m_breakProgress = 0.0;
    // Last progress value at which a dig sound played (Phase 15).
    double m_lastDigProgress = 0.0;
    // Footstep timer (seconds since the last step sound; Phase 15).
    double m_stepTimer = 0.0;

    // Phase 15 UI: unlit screen-space shader + the generated font/crack
    // textures, the 2D icons of the explicit items (indexed by ItemID
    // value; empty entry = no icon), the target-block outline (line
    // mesh) and one crack overlay cube per stage.
    ShaderProgram m_uiShader;
    Texture m_fontTexture;
    Texture m_crackTexture;
    std::vector<Texture> m_itemIcons;
    // CRAFT-2: the per-item model textures (indexed by ItemID value, the
    // m_itemIcons pattern; invalid entry = no model) + the cached box-
    // model meshes (built lazily on first render, sized at init).
    std::vector<Texture> m_itemModelTextures;
    std::vector<Mesh> m_itemModelMeshes;
    // PRE-5: the per-item cutout census (indexed by ItemID value, parallel
    // to m_itemModelTextures; false entry = no model / not a pure cutout).
    // Computed at init from the item's model-texture PNG (isPureCutoutImage
    // on the decoded alpha channel) — the pure-cutout items (alpha set
    // {0, 255}) render their world drops in the CUTOUT pass (discard +
    // depth write, visible through the leaf holes); the items with a
    // translucent icon stay in the BLEND pass. Shipped icons are all
    // pure cutout (the census is content data, not code policy).
    std::vector<bool> m_itemCutout;
    Mesh m_outlineMesh;
    std::array<Mesh, 5> m_crackMeshes;
    // Phase 15 state (all default-off: the no-input baseline renders no
    // UI at all, keeping the Phase 14 screenshots byte-identical).
    bool m_debugOverlay = false;

    // Phase 17 crafting: the E panel's persistent crafting grid (its 2x2
    // uses the top-left 4 cells of the 9-cell model) and the open-panel
    // state machine. The grid survives a close/reopen (see
    // docs/KNOWN_ISSUES) and is saved (format 3). CRAFT-1: the crafting
    // TABLE's grid is no longer a single global — it is per-block (one
    // grid per crafting_table block, the m_tables map below).
    CraftingGrid m_inventoryGrid;
    InventoryUI m_inventoryUi;
    InventoryUI m_tableUi;
    OpenPanel m_openPanel = OpenPanel::None;
    bool panelOpen() const { return m_openPanel != OpenPanel::None; }

    // CRAFT-1: the per-block crafting-table grids — one CraftingGrid per
    // crafting_table block in the world (created on placement, empty;
    // dropped when the cell stops holding a table — the container's
    // items are lost, the furnace-break semantics; the invariant the
    // format-19 save cross-checks against the block edits, exactly like
    // the furnace states). std::map: position-sorted, so the save's write
    // order is deterministic. The table panel's UI is bound to the
    // clicked block's grid on every open (openTablePanel).
    std::map<BlockPos, CraftingGrid> m_tables;

    // Phase 40/44 furnace: PER-BLOCK smelting states — one FurnaceState
    // per furnace block in the world (created on placement, dropped when
    // the cell stops holding a furnace; the invariant the format-7 save
    // cross-checks). std::map: position-sorted, so the per-frame ticks
    // iterate deterministically. m_openFurnacePos is valid while
    // m_openPanel == OpenPanel::Furnace (the panel's UI is bound to its
    // state).
    std::map<BlockPos, FurnaceState> m_furnaces;
    BlockPos m_openFurnacePos {};
    FurnaceUI m_furnaceUi;

    // Phase 64 fire: the remaining real-dt life of EVERY burning fire
    // block in the world (created on placement/burn, dropped when the cell
    // stops holding a fire; the invariant the format-10 save
    // cross-checks). std::map: position-sorted, so the per-frame burn-out
    // countdown iterates deterministically. m_fireId is the registry id of
    // the "fire" block (the burn target; the init cross-check guarantees
    // it whenever a flamable block exists); m_fireLifetime is the
    // config-driven burn-out duration (fire.lifetime_seconds, default 4).
    std::map<BlockPos, double> m_fires;
    BlockID m_fireId = BlockID::invalid();
    double m_fireLifetime = 4.0;
    // Phase 66 fire spread + embers (Fire v2): the spread roll cadence
    // (fire.spread_interval_frames, default 6 = the 0.1 s Minecraft tick
    // rhythm) and per-neighbor chance (fire.spread_chance, default 0.05
    // = the Minecraft normal value); m_fireParticles gates the ember
    // emission (fire.particles, default true — a no-input run without
    // fires is inert either way, and it is the E2E control).
    int m_fireSpreadInterval = 6;
    double m_fireSpreadChance = 0.05;
    bool m_fireParticles = true;

    // Phase 71 campfire (the family in Phase 75): PER-BLOCK cooking
    // states — one CampfireCookState per campfire-FAMILY block in the
    // world (lit or extinguished: water puts the lit one out into the
    // extinguished block, which KEEPS its state — the items stay on the
    // unlit logs; created on placement, dropped when the cell stops
    // holding a family block; the invariant the format-13 save
    // cross-checks, exactly like the furnace states). std::map:
    // position-sorted, so the per-frame ticks iterate deterministically.
    std::map<BlockPos, CampfireCookState> m_campfires;
    // Phase 75: the campfire family ids (init-resolved; invalid = the
    // family-dependent paths are structurally inert, the P70 pattern).
    BlockID m_campfireLitId = BlockID::invalid();
    BlockID m_campfireExtId = BlockID::invalid();

    // Phase 48 crop growth: the cells holding a block with a non-empty
    // growsTo property (the growth stage is the block id itself — no
    // per-instance state, no save field). Maintained through the setBlock
    // choke point (like the furnace states) and rebuilt from the world by
    // rescanCrops() after load. std::map: position-sorted, so the per-
    // frame growth ticks iterate deterministically.
    std::map<BlockPos, BlockID> m_crops;

    // GEN-2 tree growth: the cells holding a SAPLING (a block with a
    // `tree` object — the growth P48 pattern, the m_crops twin: maintained
    // through the setBlock choke point, rebuilt by rescanSaplings() after
    // load). The GROWTH STAGE is NOT stored here — it is the cell's
    // per-instance state (ENT-2, the format-15 `states` save segment);
    // this set only tracks WHICH cells roll. std::map: position-sorted,
    // so the per-frame sapling ticks iterate deterministically.
    std::map<BlockPos, BlockID> m_saplings;
    // GEN-2: the RESOLVED tree specs (the sapling's `tree` names ->
    // BlockIDs, the init-resolved m_campfireLitId pattern — the registry
    // load validates the names exist, so this map is the placement's
    // fast path). Keyed by the sapling's block id; empty = no sapling
    // content (the structurally-inert pattern).
    std::map<BlockID, TreeSpec> m_treeSpecs;

    // Phase 83 beacon: the cells holding a block with a non-empty
    // beaconEffect property (a beacon — the growth P48 pattern: maintained
    // through the setBlock choke point, rebuilt by rescanBeacons() after
    // load). The power level is NOT stored here — it is re-derived from
    // the world every frame (world/beacon), so there is no save-format
    // field. std::map: position-sorted, so the per-frame pass iterates
    // deterministically.
    std::map<BlockPos, BlockID> m_beacons;
    // Phase 83 beacon: this frame's ACTIVE beacons (level >= 1), filled by
    // updateBeacons() and read by drawBeaconBeams() — the (x, y, z) of the
    // beacon block plus its power level (the beam is taller at higher
    // levels, Minecraft).
    struct BeaconBeam {
        int x, y, z, level;
    };
    std::vector<BeaconBeam> m_beaconActive;
    // Phase 83 beacon config: the effect range in blocks
    // (beacon.range_blocks, default 50, >0 — a player within Chebyshev
    // distance <= range of the beacon cell receives the effect) and the
    // beam visibility gate (beacon.beam, default true — a no-input run
    // without beacons is inert either way, the E2E control the fire.
    // particles pattern).
    int m_beaconRange = 50;
    bool m_beaconBeam = true;
    // Phase 83 beacon: the shared translucent beam column mesh (a thin box
    // from y=0 to y=1, scaled + positioned per beacon by the model matrix)
    // and its resolved effect id (init; -1 = no beacon effect, the path is
    // structurally inert — the m_fireEffectId pattern).
    Mesh m_beaconBeamMesh;
    int m_beaconEffectId = -1;

    // PRE-4 clouds: the cached cloud-layer mesh (the full camera window,
    // world/buildCloudMesh) + its rebuild key — the camera's cloud cell
    // and the drift time step. The mesh is rebuilt only when the key
    // changes (cloudMeshNeedsRebuild, the pure dirty flag): a no-input
    // run rebuilds once on the first draw and is static thereafter
    // (the ×2 RAW byte-identity), walking rebuilds once per 16 blocks.
    // render.clouds OFF never touches any of this (the v0.27.0 pipeline
    // bit-identical).
    Mesh m_cloudMesh;
    bool m_cloudMeshValid = false;
    int m_cloudMeshCellX = 0;
    int m_cloudMeshCellZ = 0;
    int m_cloudMeshT = 0;

    // SAVE-1: the auto-save cadence (save.auto_save_seconds — a CODE
    // default; the committed config carries no save section, the
    // fire/mobs/beacon config-key discipline) + the real-dt accumulator
    // (NOT frame state, hard rule 8: never saved, never affects the
    // deterministic frame — the audio-timer class). 0 = off (the
    // pre-SAVE-1 behavior); without a --save slot the timer is
    // structurally inert (updateAutoSave's gate).
    double m_autoSaveInterval = kAutoSaveDefaultSeconds;
    double m_autoSaveAccum = 0.0;

    // Phase 15 particles + audio: the break bursts (deterministic seeds:
    // world seed mixed with an increasing per-break counter) and the sound
    // bank (gracefully silent when no audio device exists).
    ParticleSystem m_particles;
    std::uint64_t m_particleBreakCounter = 0;
    SoundBank m_sound;

    // Phase 16 survival: the health heart strip (48x16, three 16x16 tiles:
    // full/half/empty) and the damage-flash + event polling state. The
    // player owns the health; the game polls it each frame (a drop = hurt
    // flash + sound, a death = pointer release + sound; R respawns).
    Texture m_healthTexture;
    double m_hurtFlash = 0.0; // seconds remaining of the red flash
    float m_prevHealth = -1.0f; // -1 = first frame (no hurt event)
    // Phase 23 hunger: the drumstick strip (same 48x16 three-tile layout as
    // the health strip). The player owns the hunger; the game only draws it
    // and feeds it (tryEat).
    Texture m_hungerTexture;
    // SURV-2: the oxygen bubble strip (a SINGLE 16x16 tile — no half
    // state; the fill count is the number of drawn icons, which
    // replaces the hunger row while the air is below full) + the
    // player's drowning state (the pure step lives in player/drown,
    // the unit tests own it — the campfire pattern).
    Texture m_oxygenTexture;
    DrownState m_drown;
    // Phase 46 status effects: the data-driven definitions
    // (assets/data/effects) + the player's active instances (core/
    // status_effect.hpp) + the 16 px icon strip (assets/textures/
    // effects.png; the effect's 'icon' field indexes its tile). The game
    // owns the instances and their real-dt tick; the HUD draws one icon
    // per active effect (left-aligned on the heart row).
    EffectRegistry m_effects;
    StatusEffects m_playerEffects;
    // Phase 57: the ignites_on_lava effect (the fire) — its id, or -1
    // when no registered effect carries the flag (init cross-checks that
    // AT MOST ONE does). (Re-)applied every frame while the player is in
    // the lava; cleared by water (the extinguished_by_water flag).
    int m_fireEffectId = -1;
    Texture m_effectTexture;

    ShaderProgram m_basicShader;

    // Day/night cycle (config-driven; advanced by a fixed per-frame step so
    // headless runs stay byte-reproducible — see kDayStepPerFrame).
    DayCycle m_dayCycle;
    // Unlit billboarded sky sprites (sun, moon, stars), drawn with
    // m_skyShader between the opaque and transparent chunk passes.
    ShaderProgram m_skyShader;
    Mesh m_starMesh;     // kStarCount quads on the upper hemisphere
    Mesh m_skyPointMesh; // one quad at the origin (offset per draw)

    int m_frameCount = 0;
    double m_fpsSmoothed = 0.0;
    double m_lastTitleUpdate = 0.0;
    bool m_initialized = false;
};

} // namespace mc
