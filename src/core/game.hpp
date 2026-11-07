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
#include "core/bed.hpp"
#include "core/camera.hpp"
#include "core/config.hpp"
#include "core/demo.hpp"
#include "core/difficulty.hpp"
#include "core/enchanting.hpp"
#include "core/error.hpp"
#include "core/fly.hpp"
#include "core/food.hpp"
#include "core/input.hpp"
#include "net/net_proto.hpp"
#include "net/net_session.hpp"
#include "core/end_portal.hpp"
#include "core/logger.hpp"
#include "core/portal.hpp"
#include "core/save.hpp"
#include "core/sound.hpp"
#include "core/status_effect.hpp"
#include "core/thread_pool.hpp"
#include "core/third_person.hpp"
#include "core/tutorial.hpp"
#include "core/weather.hpp"
#include "core/window.hpp"
#include "entity/entity.hpp"
#include "entity/exp_orb.hpp"
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
#include "ui/ui_scale.hpp"
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
#include "world/explosion.hpp"
#include "world/ao.hpp"
#include "world/end_gen.hpp"
#include "world/lighting.hpp"
#include "world/nether_gen.hpp"
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
    // V2-4: --weather <clear|rain> — the initial weather state (nullopt =
    // not given = the default first-segment-CLEAR; true = rain — the
    // opening segment runs the fixed kRainMin frames, then the world-
    // seed streams take over; false = clear — behaviorally the default).
    // It touches the initial state ONLY, never the stream sequence
    // (same world seed, same weather after the opening segment).
    std::optional<bool> weather;
    // V3-14a: --explode <x> <y> <z> [radius] — a single deterministic
    // blast at the run's start (the gate/test seam's --weather twin:
    // the V3-14a explosion has no in-game trigger yet — the ghast's
    // V3-14b fireball is its first consumer; the seam exercises the
    // same Game::explodeAt the ghast will call). nullopt = no blast
    // (the no-input run's structural-immunity root).
    std::optional<ExplosionSpec> explode;
    // V3-10d: --dragon-damage <n> — a single deterministic damage to the
    // End dragon (the gate/test seam's --explode twin: the boss's kill
    // loop's combat trigger is the coupled moving-target demo, the seam
    // exercises the same damage() the combat would, deterministically).
    // Applied the first frame the dragon exists (the spawn's frame);
    // nullopt = no damage (the no-seam run's structural-immunity root).
    std::optional<double> dragonDamage;
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
    // SAVE-3: save slot management — headless, STANDALONE (at most one
    // per run, and never combined with a game-run flag; parseArgs
    // enforces both). The operation runs on data/saves before the game
    // boots, prints the result, and exits.
    bool listSaves = false;
    std::optional<std::pair<std::string, std::string>> copySave; // src, dst
    std::optional<std::pair<std::string, std::string>> renameSave;
    std::optional<std::string> deleteSave;
    // V3-9: the demo family (main.cpp enforces the mutual exclusion:
    // at most one of the three per run). demo = the --demo <file.dmo>
    // replay (main LOADS + PARSES the file headless before the Game
    // boots and fills demoFile; the world spec is applied to seed /
    // saveSlot there, the demo is the start-point authority).
    // demoAuthor = the --demo-author <timeline.json> STANDALONE
    // headless run (main expands + writes the .dmo and exits — the
    // game never boots). demoRecord = the --demo-record [out.dmo]
    // mode (normal gameplay + the Input boundary tap; the exit saves
    // the .dmo; demoRecordOut empty = data/demos/rec_<ts>.dmo).
    std::filesystem::path demo;
    // v3.13.0: the --demo-force ESCAPE — bypasses the replay's SIM
    // version gate only (the schema's strictness stays; a WARN is
    // printed when the forced sim differs). Requires --demo.
    bool demoForce = false;
    std::optional<DemoFile> demoFile;
    // V3-12b: the NET family (main.cpp enforces the mutual exclusion:
    // one of --listen / --connect per run, never with the demo family;
    // --connect x --save is rejected, R7; --listen x --save is rejected
    // (the 12b net session is the FRESH-world shape — the save form is
    // the 12c reconnect snapshot). --script = the gate arm's
    // deterministic input source (the demo timeline's fourth consumer —
    // its 'world' key rides along for the shape's strictness, ignored
    // in net: the WELCOME's spec is the world's authority).
    std::uint16_t listenPort = 0; // --listen <port> (the host)
    std::string connectHost;      // --connect <host:port> (the client)
    std::uint16_t connectPort = 0;
    std::filesystem::path script; // --script <timeline.json>
    std::optional<DemoFile> scriptFile;
    std::filesystem::path demoAuthor;
    bool demoRecord = false;
    std::filesystem::path demoRecordOut;
};

// V3-12b: the state hash FNV primitive (net/net_proto.hpp).
struct Fnv64;

class Game {
    // V3-12b: forward declaration — the PlayerSession's full definition
    // lands later in the class (it absorbs the PanelState / SessionStats
    // nested types), but the ×N method declarations reference it by
    // reference (an incomplete type is legal in a reference parameter).
    struct PlayerSession;

public:
    // V3-12b: the state hash (net_proto.cpp's netStateHash) reads the
    // full sim truth — the desync detector's friend (the field list is
    // the design §2.3's, the FNV primitive is net_proto's). The feeder
    // friend names the PlayerSession's private nested type (the hash's
    // per-player field list).
    friend std::uint64_t netStateHash(const Game& g);
    friend void netFeedPlayer(struct Fnv64& h, std::size_t i,
                              const Player& p, const PlayerSession& s);

    // V3-1 Phase B (D1): the two dimensions (the fixed pair — a >2
    // dimension future is the structural re-merge point, the design's
    // documented deviation 1: the incremental parallel members, not the
    // Dimension-x2 reorganization). The overworld is the pre-Phase-B
    // world (every member unchanged); the nether is the parallel
    // NetherDim family (lazy — D2).
    enum class Dimension { Overworld, Nether, End };

    // Which crafting panel (Phase 17) is open (None = nothing open).
    enum class OpenPanel {
        None,      // no panel
        Inventory, // E: 2x2 grid + the shapeless recipe list
        Table,     // right-clicked crafting table: the 3x3 grid
        Furnace,   // Phase 40: right-clicked furnace: the smelting GUI
        Enchant,   // V2-12: right-clicked enchanting table: the 3-offer GUI
    };

    // The camera mode (UX-3/UX-4, V2-1): the player's world-space body
    // renders ONLY in third person (the body is invisible in first person,
    // the Minecraft convention). First is the default — every UX-4 change
    // is gated on m_cameraMode, so a first-person (no-input) run stays
    // bit-identical. UX-4's C key (locked pointer only) cycles it at run
    // time (V2-1: the Minecraft F5 order First -> ThirdBack -> ThirdFront
    // -> First); the mode is session state, never saved (a loaded world
    // starts in first person, the Minecraft convention).
    enum class CameraMode {
        First,     // the default: no player body (byte-identical baseline)
        ThirdBack, // the body renders behind the camera (UX-4)
        // V2-1: the camera in front of the player, looking at it — the
        // player's front is visible (the Minecraft F5 front view).
        ThirdFront,
    };

    // The panel UIs (Phase 15/17/40 — UX-2 B4's m_panel) hold references
    // to the inventory and the registries, so they are constructed in
    // m_panel's initializer (after all of them, in declaration order).
    // Phase 44: the furnace UI binds its state per panel opening
    // (per-block furnace states — see openFurnacePanel). CRAFT-1: the
    // inventory UIs bind their grid per panel opening too — the E panel
    // to the player's own grid (bound once, in the constructor body), the
    // table panel to the clicked block's per-block grid (bound on every
    // open — see openTablePanel).
    // V3-12b: the session family is created FIRST (one primary session —
    // the reference aliases below bind into it; a net session ADDS the
    // remote player's session at the connect, the aliases never move —
    // they stay the primary's storage, design §3.1). The panel's
    // grid-bind moved into the PlayerSession constructor (per-session).
    Game() : m_sessions(makeSessions(m_items, m_recipes)),
             m_inventory(m_sessions[0]->inventory),
             m_selectedSlot(m_sessions[0]->selectedSlot),
             m_interaction(m_sessions[0]->interaction),
             m_playerEffects(m_sessions[0]->effects),
             m_drown(m_sessions[0]->drown),
             m_hurtFlash(m_sessions[0]->hurtFlash),
             m_prevHealth(m_sessions[0]->prevHealth),
             m_pearlThrowCooldownFrames(m_sessions[0]->pearlThrowCooldownFrames),
             m_ridingMount(m_sessions[0]->ridingMount),
             m_sessionStats(m_sessions[0]->sessionStats),
             m_stats(m_sessions[0]->stats),
             m_tutorial(m_sessions[0]->tutorial),
             m_tutorialEnabled(m_sessions[0]->tutorialEnabled),
             m_tutorialToastFrames(m_sessions[0]->tutorialToastFrames),
             m_lastSpacePressTime(m_sessions[0]->lastSpacePressTime) {
    }
    ~Game();

    Result<void> init(const GameArgs& args);
    // Runs the main loop. Returns the process exit code (0 = clean exit).
    // V3-9: the loop is BRANCHED on the demo mode — Off / Record run
    // runGameplayLoop (the pre-V3-9 loop verbatim + the record's dt
    // append, flag-gated); Replay runs runDemoReplay (the audience
    // owns the SDL queue, the demo's dts are the only time source, the
    // wall clock only paces the rendering). Three flags absent =
    // runGameplayLoop = the bit-identical normal run (the structural
    // immunity root).
    int run();
    // The gameplay loop (Off / Record) — the pre-V3-9 main loop body,
    // verbatim (the dt = the wall clock, clamped kMaxDt).
    void runGameplayLoop(double lastFrame);
    // The demo replay loop (see run()). The audience Input pumps the
    // SDL queue (the single-consumer discipline — the player's Input
    // is demo-fed, never pumped); the demo frame's events inject into
    // the player's Input in file order; dt = the demo's dts; the
    // 0.5x hold renders twice (the second render-only — no update, no
    // m_frameCount); the audience's whitelist (orbit / speed / pause /
    // view mode / quit-from-panel) never touches the simulation.
    void runDemoReplay();
    // The per-frame camera sync (the pre-V3-9 update()'s camera block,
    // extracted VERBATIM — the player's state + the session's camera
    // mode / zoom; update() calls it at the same point, the bit-
    // identical normal run) + the V3-9 replay's audience orbit (the
    // render-only offset, re-resolved around the eye — see the
    // implementation). The replay's paused / hold renders call it too
    // (the orbit stays live without the simulation).
    void syncCamera();
    // The aim ray's direction (every world-facing ray: the break /
    // attack / mob / bow / bucket / slab hit point). The normal path
    // = the camera's forward (bit-identical — the pre-V3-9
    // expression); the replay = the mode's BASE view (no audience
    // orbit — the orbit is render-only, the demo's aim stays the
    // player's faithful view, the input-routing red line).
    Vec3 aimForward() const;

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

    // V3-1 Phase B: the nested types DimCtx references by value /
    // reference before their definitions (the complete-class context
    // covers member function bodies, not nested class definitions — the
    // BeaconStream pattern's forward-declaration twin).
    struct StreamingState;
    struct MobSystem;

    // V3-1 Phase B (D1): the streaming/meshing DIMENSION CONTEXT — the
    // reference bundle the (parameterized) streaming functions run over.
    // The overworld binds its existing members (m_world / m_streaming /
    // m_chunkMeshData / ... — zero behavior change: the overworld call
    // sites pass the same data the pre-Phase-B functions read directly);
    // the nether binds its parallel NetherDim family. One pool is shared
    // (the ctx binds the overworld's m_streaming.pool for BOTH — the
    // per-frame flush barrier then covers every job of both dimensions,
    // and the inactive dimension's zero-post steady state keeps the
    // shared pool uncontended, the design's "稳态零争用" ruling). The
    // protectStates callback is the dimension's per-block state maps'
    // unload protection (the overworld's seven-map family; the nether's
    // portal tracking set) — the demo-area protection is the flag (the
    // overworld only; the nether has no demo area).
    struct DimCtx {
        World& world;
        StreamingState& streaming;
        ThreadPool& pool;
        std::unordered_map<ChunkPos, ChunkMeshData>& chunkMeshData;
        std::unordered_map<ColumnPos, ColumnMesh>& columnMeshes;
        std::set<ColumnPos>& columnRebuildPending;
        std::set<ChunkPos>& flammableChunks;
        std::set<ColumnPos>& lightPendingColumns;
        std::set<ColumnPos>& fluidLightPending;
        // The dimension's generation reference (the retain-diff's fresh
        // comparison — the makeChunkGenerator / makeNetherChunkGenerator
        // closures).
        World::ChunkGenerator generator;
        // The unload pass's per-block state protection (see the struct).
        std::function<void(std::set<ColumnPos>&)> protectStates;
        bool protectDemoArea = false;
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

    // V3-12b: the starter inventory's per-session fill (buildScene's
    // hotbar.json block, extracted — the net connect's player-2's gift).
    Result<void> fillStarterInventory(PlayerSession& s);
    Result<void> buildScene();
    void update(double dt);
    void render();
    // Rebuilds the GPU mesh of every column whose merged GL mesh is stale
    // (ctx.columnRebuildPending). PIPE-1: the mesher's COMPUTE no longer runs
    // here — worker mesh jobs (postMeshJobs / integrateMeshJobs) refresh
    // the chunk's CPU geometry (ctx.chunkMeshData) against a private snapshot
    // World; what remains on the main thread is the column-merged GL
    // upload (rebuildColumnMesh — the GL context is main-thread only).
    // V3-1 Phase B: parameterized over the dimension context (the
    // overworld / nether each merge their own columns).
    Result<void> updateChunkMeshes(const DimCtx& ctx);
    // Concatenates the column's chunk geometries (baking each chunk's
    // world offset into the vertices, remapping the indices) and uploads
    // the merged opaque + transparent sub-meshes.
    Result<void> rebuildColumnMesh(const DimCtx& ctx, const ColumnPos& col);

    // Chunk streaming (Phase 14). integrateStreamedColumns runs after the
    // pool flush: worker-generated columns are inserted into the World
    // (insertChunk marks the chunk + face-sharing neighbors dirty, so the
    // next updateChunkMeshes re-meshes them and the boundary). The two
    // post* methods run last in update, after every main-thread World
    // write: from then on until the next flush the workers own the window.
    void integrateStreamedColumns(const DimCtx& ctx);
    // Collects the completed lighting jobs (after the pool flush):
    // STREAMING job columns (LightJob::fluid = false) leave
    // m_streaming.unlitColumns, so the next updateChunkMeshes builds their
    // meshes
    // with the propagated light (the one-and-only mesh); FLUID catch-up
    // job columns (99.11, fluid = true) leave m_fluidLightPending (the
    // set was already drained at post time — the reap's erase is the
    // contract belt, and m_streaming.unlitColumns is never touched: fluid
    // columns
    // are not unlit columns). Every job's net-changed chunks (collected
    // job-locally by the workers, 99.10) join the World's dirty set
    // here — the main-thread-writer window, no worker in flight.
    void collectLitColumns(const DimCtx& ctx);
    // Decides the columns to stream around the player (trigger + radius,
    // see chunk_streaming) and posts up to kStreamColumnsPerFrame
    // generation jobs to the pool.
    void streamChunks(const DimCtx& ctx);
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
    void postLightJobs(const DimCtx& ctx);
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
    void postFluidLightJobs(const DimCtx& ctx);
    // PIPE-1: posts up to kMaxMeshBacklog worker mesh jobs for the nearest
    // eligible dirty columns (selection: world/mesh_job.hpp). Each job
    // snapshots the column's 3x3 column neighborhood into its own World
    // (taken after every main-thread World write of this frame) and meshes
    // the column's 8 chunks on a worker. Runs in update AFTER streamChunks
    // (gen posts) and BEFORE postLightJobs — the post order gen -> mesh ->
    // light is what keeps this frame's lighting job the only live-World
    // writer after the snapshots (see the pipeline contract above).
    void postMeshJobs(const DimCtx& ctx);
    // PIPE-1: collects the completed mesh jobs (after the pool flush,
    // BEFORE integrateStreamedColumns — a stream insert marks neighbor
    // columns dirty, which must not be swallowed by a mesh integrate's
    // clear-dirty): the 8 ChunkMeshData outputs move into m_chunkMeshData
    // (whole-column replace), the 8 chunks clear their dirty flag (at
    // integrate, not post: an edit after this frame's integrate re-marks
    // the chunk and re-queues it), and the column joins
    // m_columnRebuildPending for the GL upload. FIFO = deterministic.
    void integrateMeshJobs(const DimCtx& ctx);
    // Unloads the loaded columns far behind the player (the memory leak
    // fix: streaming only ever ADDED chunks — a radius-80 session grew to
    // 11k+ chunks / several GB and kept rising). Runs right after
    // integrateStreamedColumns (the pool is flushed: no worker in flight),
    // the same safe main-thread-writer point. A column survives when it is
    // within view_distance + kUnloadHysteresis, part of the protected 4x4
    // demo area (respawn safety), player-edited (m_streaming.editedColumns
    // — the
    // save records its diff and must keep the chunks), tracked by per-block
    // state (furnace/campfire/crop/beacon/fire — the save cross-checks
    // those entries against the block edits), or awaiting a generation job
    // (m_streaming.streamPending). Unloading erases the chunks (eraseChunk
    // re-meshes
    // the boundary neighbors), frees the GL mesh buffers, and drops the lit
    // set entries.
    void unloadFarColumns(const DimCtx& ctx);
    Result<void> takeScreenshot(const std::filesystem::path& path);

    // Draw the sun, moon and starfield (unlit billboarded quads, blended).
    // Called between the opaque and transparent chunk passes.
    void drawSkyObjects(const Mat4& view, const Mat4& projection,
                        int viewportHeight);

    // Pointer-lock state machine + block interaction (called from update).
    void handleInteraction(Player& p, PlayerSession& s, const Input& in, double dt);
    // The block the crosshair is aiming at (raycast from the eye, within
    // aimReach), if any.
    std::optional<BlockHit> targetBlock() const;
    // V2-1: the aim ray's reach budget. First / ThirdBack: the legacy
    // kReach (bit-identical — the structural-immunity root of the UX-8g
    // red line). ThirdFront: the third-person distance + kReach (V2-19:
    // the wheel-zoomed session m_thirdPersonDistance, default
    // kThirdPersonDistance — the front camera sits that far in front of
    // the eye (it IS the player's visual eye in that view), so the eye
    // ray needs the D compensation to keep the full Minecraft reach at
    // any zoom, the MC front-view convention). Defined in
    // game_interaction (next to kReach).
    float aimReach() const;
    // Hold-to-break state machine (Phase 11): advances the progress while
    // the crosshair stays on the same destroyable block and breaks it when
    // the data-driven duration elapses (see breakDurationSeconds).
    void updateBreaking(Player& p, PlayerSession& s, double dt);
    void resetBreaking(PlayerSession& s);
    // Mob targeting + attacking (Phase 20): the mob under the crosshair
    // (the nearer of the block hit and the mob AABB hits wins — blocks
    // occlude), and the swing state machine (one hit per kSwingInterval
    // while the button stays down on the mob: data damage + knockback).
    struct MobHit {
        MobEntity* mob;
        float distance;
    };
        Vec3 playerAimForwardFor(const Player& p) const;
    std::optional<BlockHit> targetBlockFor(const Player& p) const;
    std::optional<MobHit> targetMobFor(const Player& p) const;
std::optional<MobHit> targetMob() const;
    void updateAttacking(Player& p, PlayerSession& s, double dt, MobEntity& mob);
    // Melee damage of the selected item (data; bare hand = 1.0). V2-12:
    // includes the held SHARPNESS bonus (the (L+1) points).
    float selectedAttackDamageFor(PlayerSession& s) const;
    // V2-12 (protection): the armor protection BONUS points (4 * min(20,
    // the sum of the protection levels over the four worn pieces) — 0 when
    // no protection is worn). The game pushes this to the player each frame
    // (setProtectionBonus); damage() adds it to the equipped points before
    // the shared kArmorMaxPoints cap.
    int armorProtectionPoints(const Player& p) const;
    // V2-12 (unbreaking): apply `wear` to `stack`, respecting its unbreaking
    // enchantment (the per-point skip roll — the state from the world seed,
    // the stack's damage BEFORE this event, and the item id). Returns true
    // exactly when the stack BROKE (the applyDurability result); false when
    // the wear was fully skipped or the stack survived. The armor wear runs
    // through Player::damage (which holds no registry — the pushed-level
    // pattern), so this is for the TOOL / bow / igniter sites only.
    bool wearItem(ItemStack& stack, float wear);
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
    float selectedShootSpeedFor(PlayerSession& s) const;
    float selectedShootDamage() const;
    // True when the selected launcher can fire: no 'ammo' data = always;
    // otherwise at least one ammo item in the inventory.
    bool ammoAvailableFor(PlayerSession& s) const;
    void updateBowDraw(double dt);
    void updateBowDrawFor(Player& p, PlayerSession& s, const Input& in, double dt);
    // Fires one arrow with the charge's power (see updateBowDraw),
    // consuming one ammo item; a draw with no ammo left is cancelled
    // silently. Resets the draw state.
    void fireBow(Player& p, PlayerSession& s);
    // Cancels an in-progress draw (item switch, unlock, panel, ...).
    void resetBowDraw(PlayerSession& s);
    // The single choke point for every in-game block change (break,
    // place, dye recolor): the raw world setBlock (which marks the chunk
    // dirty) plus, Phase 42, releasing any STUCK arrow whose box overlaps
    // the changed cell (it falls out and rejoins flight physics).
    // V3-1 Phase B: the legacy name addresses the PLAYER's dimension
    // (currentWorld — the pre-Phase-B bit-identical path while the
    // player is in the overworld); setBlockInWorld is the dimension-
    // explicit form the travel pipeline's generated-portal writes use
    // (the target dimension — the player is still in the source when the
    // frame is written).
    void setBlock(int x, int y, int z, BlockID id);
    void setBlockInWorld(World& world, int x, int y, int z, BlockID id);
    // V3-1 Phase A: the portal frame event over the setBlock choke point
    // (the furnaces / campfires bookkeeping family's transition twin):
    // an obsidian or fire placement / destruction runs the bounded frame
    // scan (core/portal's scanPortalFrames) + the pure transition
    // decision (portalTransitionDecision) and applies the plans —
    // activation (the interior's air + fire cells -> the portal block in
    // the frame's axis state, one updateRegion over the frame box, the
    // "Portal activated at (x, y, z) (WxH)" log) and deactivation (the
    // interior's portals -> air, the "Portal deactivated" log). Idempotent
    // (a repeated event finds no new transition); structurally inert on
    // every non-obsidian / non-fire event and without the portal +
    // obsidian blocks (the base run's immunity root). V3-1 Phase B:
    // dimension-explicit (the frame event runs over the dimension the
    // edit landed in — the nether's portals build / deactivate there).
    void checkPortalTransition(World& world, BlockID oldId, BlockID newId,
                               int x, int y, int z);
    // V3-1 Phase A: applies the activation plan of a complete frame
    // (called by checkPortalTransition; the pure plan + relight + log).
    void activatePortal(World& world, const PortalFrame& frame);
    // V3-1 Phase A: applies the deactivation plan of a broken frame
    // (called by checkPortalTransition).
    void deactivatePortal(World& world, const PortalFrame& frame);
    // Performs the break: air + relight + data-driven drop (ItemEntity).
    // The caller guarantees the hit is destroyable.
    void breakBlock(Player& p, PlayerSession& s, const BlockHit& hit);
    // Places one from the selected hotbar slot (block items only) and
    // consumes it from the stack.
    void placeBlock(Player& p, PlayerSession& s);
    // Eats one from the selected hotbar slot when it holds food (Phase 23)
    // and the player is not at full hunger: restores the item's data
    // restore_hunger, consumes the stack entry, and plays the eat sound.
    // Returns true when the click was consumed by eating (food selected,
    // including the no-op case at full hunger), so the caller skips the
    // normal right-click (interact/place) behavior for that click.
    bool tryEatFor(Player& p, PlayerSession& s);
    // Drinks one from the selected hotbar slot when it holds a drinkable
    // container (Phase 43, item data 'drink_time'): arms the shared eat
    // channel with the item's DATA-DRIVEN duration. Completing it consumes
    // one, returns the item's 'returns_item' container to the inventory
    // and clears the player's status effects (v1: the hurt flash). The
    // drink starts only when the returned container would fit (a full
    // inventory is a no-op, the click is still consumed). Returns true
    // when the click was consumed by drinking.
    bool tryDrinkFor(Player& p, PlayerSession& s);
    // Milks a milkable mob (Phase 43, mob data 'milk_with' / 'milk_gives'):
    // the crosshair must be on a mob whose 'milk_with' names the selected
    // item (melee targeting: kReach + block occlusion). Consumes one held
    // item, adds one 'milk_gives' item; the mob is unharmed (v1). Returns
    // true when the click was consumed (crosshair on the mob, including
    // the no-milk case of a full inventory).
    bool tryMilk(Player& p, PlayerSession& s);
    // V2-15 (sheep): shears a wool mob — the crosshair on a mob whose
    // data carries 'wool_drops', the selected item carrying the 'shears'
    // data flag, and the instance still WOOLLY (the bare state is a
    // no-op that still consumes the click, the failed-milk rule):
    // the mob's own wool drop entry's count (1-3, the per-mob stream
    // roll) of the INSTANCE-COLOR wool drops at the mob (a ground item,
    // the Q-drop precedent — zero velocity + the pickup delay), the mob
    // enters the bare regrow window (kSheepRegrowFrames), the shears
    // wear kShearWear (the successful-action wear, the kIgniteWear twin),
    // and the "shear" cue plays spatialized at the mob. Returns true
    // when the click was consumed (crosshair on the target mob, the
    // bare no-op included).
    bool tryShearFor(Player& p, PlayerSession& s);
    // V2-15 (sheep): dyes a wool mob's instance wool — the crosshair on
    // a 'wool_drops' mob, the selected item a DYE (dyeColor non-empty —
    // the data flag, the block recolor's predicate), the instance still
    // WOOLLY (Minecraft: a sheared mob refuses the dye — the click is
    // consumed): the instance color becomes the dye's color, one dye is
    // consumed, the "cloth" cue plays (the block recolor's sound twin).
    // Returns true when the click was consumed (crosshair on the target
    // mob, the bare refusal included).
    bool tryDyeMobFor(Player& p, PlayerSession& s);
    // V3-14c (piglin): the barter — the crosshair on a 'barter_input'
    // mob, the selected item == the data's barter input name (the data
    // predicate, no species switch): one held input is consumed and the
    // rolled output drops at the mob (ground items, the shear's
    // precedent: zero velocity + pickup delay + the magnet), the
    // "barter" cue plays at the mob. The mob is NEVER damaged (a neutral
    // barter does not enrage — the milk non-enrage's structural twin).
    // Returns true when the crosshair is on a bartering mob (the click
    // is consumed even when the held item is wrong — the failed-milk
    // rule); false = fall through (a non-barter mob, the legacy
    // bit-identical).
    bool tryBarter(Player& p, PlayerSession& s);
    // V3-14d (strider riding): the ride pass — the per-frame choke point
    // (the dismount conditions + the carry snap + the steer push). Runs
    // after the input handlers (a panel-open / a Space / a LeftControl
    // dismounts THIS frame) and before the player's update (the snap
    // owns the pose the update then bypasses) and the entities' update
    // (the steer is pushed before the mount's update). One null check
    // in every no-ride session (the structural-immunity root).
    void updateRide();
    // V3-14d: the mount entry (the tryMilk paradigm, the RMB chain's
    // tryBarter twin): the crosshair on a 'rideable' mob, the player
    // not already riding / flying. The click is consumed on a rideable
    // target (the failed-milk rule); a non-rideable target falls
    // through to the legacy block interact (the bit-identical path).
    bool tryRide(Player& p, PlayerSession& s);
    // V3-14d: the dismount exit (every dismount funnels here — the
    // Space / LeftControl keys, the ride pass's conditions: the mount
    // dead, the player dead, the flight toggle, the panel-open, the
    // mount in water, the portal arm).
    void dismountRide();
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
    bool tryFluidBucketFor(Player& p, PlayerSession& s);
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
    bool tryArmorFor(Player& p, PlayerSession& s);
    // Advances the eat/drink channel in real dt (Phase 28, drinkable
    // extension Phase 43): interrupts (damage, death, unlock, panel,
    // selection change) and the terminal events (complete = consume +
    // food restore OR drink return-container + clear status effects,
    // early release = cancel).
    void updateEatChannel(Player& p, PlayerSession& s, const Input& in, double dt);
    void resetEatChannel(PlayerSession& s);
    // Clears the player's status effects on a completed drink (Phase 43,
    // completed by the Phase 46 system): resets the red damage veil
    // (m_hurtFlash) and removes every NEGATIVE active effect (Minecraft
    // milk; the positive effects, like regeneration, survive). Fall
    // damage is deliberately NOT cleared (Minecraft parity: it is not a
    // status effect).
    void clearStatusEffectsFor(PlayerSession& s);
    // Advances the player's active status effects by REAL dt (Phase 46,
    // the survival-timer class): durations count down, tick effects fire
    // their damage (the min_health floor — poison never kills) / healing
    // per interval, and expired effects are removed. A no-op while the
    // player is dead or has no active effects.
    void updateEffects(double dt);
    // Phase 64 fire: the real-dt burn-out countdown for every burning
    // fire block (a fire that reaches zero becomes air — the burn leaves
    // no char, like the Phase 58 lava burn) + the extinguish (a fire with
    // a water fluid 6-adjacent is put out instantly — water beats fire,
    // the Phase 57 water-beats-lava rule; V2-4: the rain disjunct through
    // the shared extinguishedByWaterOrRain predicate). Runs every frame
    // after the fluids tick (so the newly burned cells from the Phase 58
    // pass are registered and the new water is visible). A no-op when
    // the world holds no fire. Phase 66 adds, on the same pass: the
    // per-tick fire-ember particle emission (fixed-frame step, gated on
    // m_states.fireParticles) and the death burst on burn-out/water
    // extinguish.
    void updateFires(double dt);
    // CRAFT-5: the lit-furnace mouth flames — every
    // kFurnaceParticleEveryNFrames frame, each furnace with a live burn
    // emits a rising ember batch from the cell above the block (the P66
    // spawnFire reused; gated on m_states.furnaceFlames — zero emissions
    // when off, no furnaces, or no fire block for the texture).
    void updateFurnaceFlames();
    // V3-15: the portal steam emission (the frame-locked box scan, the
    // fire-particle seed contract with the dedicated salt).
    void updatePortalSteam();
    // PRE-8: the burning-mob flames — every kMobFlameEveryNFrames frame,
    // each BURNING mob (MobEntity::burning, the P57/P73 fire system's
    // state) emits a rising ember batch from its AABB-center cell (the
    // P66 spawnFire reused; the embers are SELF-LIT — block 1.0, the
    // combustion lights them at deep night, the center cell's sky as
    // the ambient term — unlike CRAFT-5's furnace-mouth embers, which
    // live in the air above the furnace and are environment-lit; the
    // documented difference: a furnace's flame pokes out, a mob's flame
    // covers it). Gated on m_states.mobFlames — zero emissions when off,
    // no burning mob, or no fire block for the texture.
    void updateMobFlames();
    // Phase 66 fire spread (Fire v2): every fire.spread_interval_frames
    // frame, each live fire cell rolls its 5 spread neighbors (4
    // horizontal + above — never downward, Minecraft) for a `flamable`
    // block (fire.spread_chance per roll, world/fire_spread) and ignites
    // the passes with setBlock(FIRE) (the choke point registers the new
    // fire's full life). Runs every frame after updateFires; a no-op
    // without fires or without the fire block.
    void updateFireSpread();
    // V3-14a: the explosion's apply seam (the pure scan's game-side
    // twin, world/explosion): applies the blast to the player's
    // dimension — the broken cells to air (the setBlock choke point's
    // journal/relight + the debris's seed-contract particles, no
    // drops), the ignited cells to fire, the boom sound, the center's
    // ember flash, and the falloff damage to the player + the mobs
    // (the creative's gate is inside Player::damage). `sourceName`
    // names the exploder for the death line ("" = the unattributed
    // blast — the --explode seam; the ghast's V3-14b blast passes its
    // id). The frame-locked world + the seed-contract particles keep
    // an identical run byte-identical.
    void explodeAt(const Vec3& center, double radius,
                   const std::string& sourceName);
    // Phase 83 beacon: re-derive the power level of EVERY tracked beacon
    // (world/beacon — a pure pyramid scan of the `beacon_base` layers
    // below it) and, for each level >= 1, apply its NAMED effect to the
    // player while the player is within `beaconRange` blocks of the
    // beacon (the duration refreshes every frame, the fire-source pattern
    // — standing in range keeps it active). Also fills
    // `m_states.beaconActive`
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
    // V3-1 Phase A: the portal entry countdown (the frame-locked class —
    // hard rule 8, the day-cycle step; the pure step is
    // core/portal's portalEntryStep, the unit tests own it). The player's
    // AABB overlapping a portal cell arms kPortalEntryFrames; leaving
    // cancels; a panel opened mid-countdown auto-closes (the Minecraft
    // portal rule — no GUI in a portal, the E2E scene pin); completion
    // with kPortalTravelEnabled = false (Phase A) is the explicit no-op
    // stub: the "Nether ready at (x, y, z)" info log (the 8:1 mapped
    // coordinate, Phase B's travel input) + zero displacement + the
    // countdown cleared (re-entry re-counts — no cooldown, the Phase A
    // semantics pin). A no-input run without portals never arms it
    // (structural immunity).
    void updatePortalEntry();
    // V3-1 Phase B: the portal frame the player's AABB currently overlaps
    // (the travel's source — the deterministic cell scan + the
    // portalValidFrame predicate; nullopt = no valid frame).
    std::optional<PortalFrame> playerPortalFrame() const;
    // V3-1 Phase B: the real dimension switch (the completed countdown's
    // consumer — see the declaration comment near the NetherDim family).
    // V3-1 Phase B: the overworld's world pass (the pre-Phase-B furnace /
    // campfire / crop / sapling code moved verbatim — the time-capsule
    // gate's overworld half, frozen while the player is in the nether).
    void updateOverworldWorldPass();
    // V3-1 Phase B: the overworld's fluid / moisture / fire pass (the
    // pre-Phase-B code moved verbatim — the time-capsule gate's second
    // half).
    void updateOverworldFluidPass(double dt);
    // V3-1 Phase B: the nether's fluid pass (the overworld section's
    // dimension-bound twin — the static source lava sea's zero churn).
    void updateNetherFluidTick();
    // V3-1 Phase A: the portal ambient sound (the bat squeak's per-frame
    // roll, spatialized at the portal cell center): every tracked portal
    // within kPortalAmbientRadiusBlocks of the player's feet rolls once
    // per frame — the (seed, frame, cell) pure stream's
    // 1 / (kPortalAmbientIntervalSeconds × 60) probability. Audio never
    // enters the frame state (hard rule 8); a no-input run without
    // portals never arms it (the structural-immunity root).
    void updatePortalAmbience();
    // V2-4 weather: the frame-locked state-machine step (the fixed 1/60
    // day-cycle class, hard rule 8 — the pure logic in core/weather, the
    // unit tests own it). The game owns the streams + the event effects:
    // the verbatim log lines ("Weather: rain started" / "Weather: rain
    // stopped" / "Thunder" — the E2E evidence), the thunder sound (2D
    // centered, never spatialized — the distant-thunder decision; audio
    // never enters the frame state, the PRE-4 discipline) and the
    // thunder frame's presentation pulse state. render.weather OFF =
    // this never runs (the machine stays at its initial clear state —
    // zero interaction, zero drops, the v2.3.0 pipeline bit-identical).
    // Runs FIRST in update (before every raining consumer: the campfire
    // put-out, the crop growth, the fire put-out, the effect extinguish,
    // the mobs' setRaining feed).
    void updateWeather();
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
    void syncAbsorptionFor(Player& p, PlayerSession& s);
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
    // SURV-6: `tier` (0-based; the grant's tier — 0 = the pre-SURV-6
    // value, the trailing default keeps every legacy call site
    // bit-identical).
    void applyPlayerEffect(const std::string& name, double duration,
                           int tier = 0);
    void applyPlayerEffectFor(Player& p, PlayerSession& s,
                              const std::string& name, double duration,
                              int tier);
    // UX-7 / V2-3: the SINGLE choke point for tutorial events — every
    // game-world event of the closed vocabulary (walk / jump / eat /
    // break.<block> / place.<block> / craft.<item> / kill.<mob> /
    // interact.<block> / pickup.<item> / smelt.<item>) funnels through
    // here, one call site per event class (the tutorial logic never
    // scatters into the call sites).
    // Gated on m_tutorialEnabled ∧ !m_tutorial.done (a no-input run
    // emits no events — the base gate's structural immunity — and a
    // done/skipped/disabled tutorial never re-runs the machine): the
    // body is the thin adapter over the pure tutorialApply (the logic
    // is unit-test-owned in core/tutorial). The completion flip (this
    // call made the state done) arms the toast window + fires the
    // one-shot info log (the E2E evidence — the PRE-4 ambience-log
    // pattern). Frame-locked: only called from update-frame code paths
    // (hard rule 8).
    void emitTutorialEvent(const std::string& name, int n,
                    Player* actor = nullptr);
    // Cycles the hotbar selection; resets break progress (the selected
    // tool changed, Minecraft behavior).
    void cycleHotbar(PlayerSession& s);
    // Drops one from the selected hotbar slot at the player's feet (Q).
    void dropSelectedItem(Player& p, PlayerSession& s);
    // Effective break speed of the selected item against `target`
    // (Phase 67): tools use their data break_speed — times the
    // fast_against multiplier when the target block is one the tool is
    // fast against (effectiveBreakSpeed); hand (empty slot) and block
    // items break at 1.0.
    float selectedBreakSpeedFor(PlayerSession& s, const Block& target) const;

    // Entity simulation (Phase 10): per-entity physics, then the item merge
    // pass and despawn (removal is deferred so the merge order — and
    // therefore the merge result — is deterministic). V2-11: the XP orbs
    // run in the same list — their physics take the real-dt rest branch
    // (the item twin), the magnet pass and the pickup pass each add a
    // dynamic_cast branch (isolated — the item paths stay bit-level
    // unchanged), and the despawn pass spawns a dead mob's data `xp` orb
    // at its drop position (xp 0 = no grant).
    void updateEntities(double dt);
    // V2-11 (XP): the single spawn point for every XP grant (the three
    // call sites — the smelt-complete event, the mob-death despawn, the
    // break completion — each pass its data-validated value + position):
    // one ExpOrbEntity at rest at `pos` (zero velocity — the item
    // convention; the value is clamped into 1..9 by the entity's
    // constructor, the belt over the data discipline). `value <= 0`
    // spawns nothing (the "0 = no grant" data gate, the belt).
    void spawnExpOrb(int value, Vec3 pos);
    // Dropped-item rendering: the items belonging to the given pass
    // (opaque / cutout / blend — PRE-5's three-way routing), bobbing +
    // spinning around their rest position (frame-counter driven — see
    // kItemBobPeriodFrames).
    void drawItemEntities(const Mat4& view, const Mat4& projection,
                          RenderPass pass);
    // V2-11 (XP): the experience orbs — the small solid-green cubes in
    // the opaque pass after the arrows (the basic shader's solid-color
    // path, the arrows' precedent; the per-draw light override samples
    // each orb's cell — the itemDropLight pattern: the orb does not
    // EMIT light, the MC orb lights nothing). Zero orbs = zero draws
    // (the lazy mesh is never built — a no-input run's structural
    // immunity; the HUD bar is the documented pixel exception).
    void drawExpOrbs(const Mat4& view, const Mat4& projection);
    // The cached solid orb-cube mesh (built lazily on first render, the
    // mobShadowMesh pattern; the geometry is static — one shared mesh,
    // the per-orb transform varies).
    const Mesh& expOrbMesh();
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
    // V3-10c: the carried block's overhead cube (the enderman's 10c-lite
    // visual read — the 0.9 cube above the head, the mob's facing
    // rotation, the itemMesh's lazy per-block cube + the block's own
    // texture: zero new geometry). The pass routes by the block's
    // render class (the item drop's twin: endstone opaque, the chorus
    // cutout — the shipped carriable set's two passes), so each cube
    // draws exactly once in its pipeline. A mob without a carried block
    // (the bit root) never reaches the mesh build.
    void drawCarriedBlocks(const Mat4& view, const Mat4& projection,
                           RenderPass pass);
    // V2-15: the wool variant's render builders (the sheep — the
    // data-flag gated; see drawMobs). `woolVariantMesh`: the per-entity
    // STATIC variant mesh (the model's wool box — kWoolBoxName — rebuilt
    // with the mob's current variant tile, the swingMeshes lazy/despawn
    // twin). `variantSwingModel`: the per-(type, variant-tile) swing
    // geometry (the swingModels twin). The swingMobMesh overload takes
    // the caller-built SwingModel (the variant's base).
    const Mesh& woolVariantMesh(const MobEntity& mob, const Mob& def);
    const SwingModel& variantSwingModel(MobID id, const std::string& tile);
    const Mesh& swingMobMesh(const MobEntity& mob, const Mob& def,
                             const SwingModel& sm);
    // V2-6: the blob shadows — the flat double-sided translucent-black
    // quad under each mob (and the player, THIRD PERSON only — first
    // person has no self-shadow, the MC look). Pure rendering: the
    // geometry is a function of the entities' current positions
    // (entity/blob_shadow.hpp) — zero new frame state. Drawn in the
    // blending pass (depth write off, depth test on: hills occlude a
    // shadow, the ground surface hosts it). A no-input run has no mobs
    // (the spawn config's firstSpawnFrame) and is first person = zero
    // draws = the v2.5.0 pipeline bit-identical.
    void drawMobShadows(const Mat4& view, const Mat4& projection);
    const Mesh& mobShadowMesh();

    // UX-3: the player's world-space body — the shared mob box-model
    // pipeline (data/player_body.json parsed through MobRegistry::
    // parseModel) rendered in THIRD PERSON ONLY (m_cameraMode != First;
    // UX-3 ships First = a no-op on every run, the structural-immunity
    // root). First person: the held hand (drawHeldHand) is untouched.
    // The swing (arms + legs, the player's swing phase) draws the
    // per-frame DYNAMIC mesh exactly like a swinging mob; the rest pose
    // (phase 0) draws the STATIC cache.
    void drawPlayerBody(const Mat4& view, const Mat4& projection);
    const Mesh& playerBodyMesh();
    const Mesh& playerBodySwingMesh();
    const SwingModel& playerBodySwingModel();

    // Arrow rendering (Phase 21): the arrows (the P10 Projectiles the bow
    // launches) draw after the mobs in the opaque pass with the shared
    // box mesh, oriented along their velocity (model = translate *
    // rotateY(yaw) * rotateX(pitch), local -Z = travel direction, see
    // arrowOrientation), in the basic shader's solid-color path
    // (u_useTexture 0). PRE-9: each arrow is THREE parts drawn in order
    // (the brown shaft, the gray tip, the white fletching) — three
    // single-color draws per arrow, the classic MC arrow.
    void drawArrows(const Mat4& view, const Mat4& projection);
    // The three shared arrow-part meshes (Phase 21; PRE-9 three-part;
    // built lazily on first render, idempotent).
    void buildArrowMeshes();
    // V3-14b: the fireball's glow-cube mesh (the arrow parts' lazy-
    // build twin — idempotent, built on first render).
    void buildFireballMesh();

    // 3D held hand (Phase 42, the Minecraft hand): while the pointer is
    // locked and no panel is open, the selected item renders as a view-
    // anchored 3D model in the bottom-right of the frustum — the block
    // cube for block items (the dropped-item cube mesh, atlas-lit), a
    // flat icon quad for the other icon items (always bright). While the
    // bow is drawn the hand pulls back with the charge (the draw
    // indicator, replacing the Phase 24 2D icon pull); while food is
    // being eaten it rises toward the face with the eat progress. V2-6:
    // the FIRST-PERSON ARM (the player's data-model 'arm_r' box, the
    // Steve-skin pa_arm tile — zero new texture) draws under the item,
    // for an empty hand too (the bare skin hand); it is static (no
    // walking swing — the documented deviation) but follows the item's
    // bow/eat offset. Pure rendering — no state, and gated on the
    // pointer lock, so a no-input run renders the pre-Phase-42 image
    // (byte-identical baseline; the arm's lazy mesh is never built).
    void drawHeldHand(const Mat4& view, const Mat4& projection);
    const Mesh& handIconMesh();
    // V2-6: the first-person arm's mesh — the player model's 'arm_r' box
    // (data/player_body.json, init-validated to exist) re-centered at
    // the origin through the shared box-model pipeline (makeMobModel-
    // Geometry: the chunk-mesher-verified UVs, full sky light baked =
    // the always-bright hand, the mob convention). Built LAZILY on the
    // first locked first-person frame (a no-input run never builds it —
    // structural immunity).
    const Mesh& firstPersonArmMesh();
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
    // V2-4 rain: the static 300-drop field (the cloud program's fourth
    // precedent — a dedicated shader program, a one-time VBO upload, zero
    // per-frame upload: the vertex shader re-derives each drop's
    // position from the frame-bounded fall offset + the player-anchored
    // 16^3 box origin, the CPU oracle rainDropPosition in core/
    // weather). Drawn in the blending pass AFTER the clouds, BEFORE the
    // HUD (the depth test occludes the drops behind terrain; culling off
    // for the double-sided bar quads; blending is the caller's state —
    // the drawClouds discipline). RAIN only: clear weather (the default
    // runs, the first segment >= kClearMin frames) or render.weather OFF
    // = zero drops (the v2.3.0 pipeline bit-identical, the structural
    // immunity).
    void drawRain(const Mat4& view, const Mat4& projection);
    // V2-4: the dayFactor channel's SINGLE convergence point (the design
    // decision "dayFactor 消费点单收口"): the base day/night factor,
    // RAIN-dimmed by kRainSkyDim, plus the thunder's 5-frame brightness
    // pulse (thunderPulse on the last strike frame). Every per-frame
    // consumer (the u_dayFactor uniform at the top of render(), the
    // drawBeaconBeams per-draw override) reads this one value; the sky
    // clear + the fog color consume the twin weather-adjusted sky.
    // Weather off / clear returns m_dayCycle.dayFactor() bit-for-bit
    // (the IEEE identities: x * 1.0f == x, x + 0.0f == x).
    float weatherDayFactor() const;

    // -- Phase 15 UI / audio / particles ------------------------------------
    // E/F3/F5 handling (F3 + F5 work in any pointer state; E opens the
    // inventory only from the captured state and closes it from the free
    // state, ESC closes any open panel too).
    void handleUiKeys();
    // The shapeless recipes in registry order (the E panel's craft list —
    // shaped recipes are crafted in the grids, not from the list).
    std::vector<const Recipe*> shapelessRecipes() const;
    // CRAFT-4: the shaped recipes in registry order (the table panel's
    // HINT list — display-only rows with the mini pattern; crafted in
    // the grid, never from the list).
    std::vector<const Recipe*> shapedRecipes() const;
    // Click routing while a crafting panel is open (Phase 17): shapeless
    // craft rows (the E panel only), then the output slot, a grid cell or
    // an inventory slot (the active panel's InventoryUI).
    void handlePanelClicks(Player& p, PlayerSession& s, const Input& in);
    // Opens the given panel (releasing the pointer) / closes the open one.
    void openPanel(OpenPanel panel);
    void openPanelFor(Player& p, PlayerSession& s, OpenPanel panel);
    void closePanel();
    void closePanelFor(Player& p, PlayerSession& s);
    // Phase 44: opens the furnace panel for the block at `pos` — binds the
    // panel's UI to that block's state (created on demand; a furnace block
    // always owns one — see setBlock).
    void openFurnacePanel(const BlockPos& pos);
    void openFurnacePanelFor(Player& p, PlayerSession& s, const BlockPos& pos);
    // The state of the furnace block at `pos` (default-constructs one when
    // absent — defensive: the setBlock invariant makes that unreachable).
    FurnaceState& furnaceAt(const BlockPos& pos);
    // CRAFT-1: opens the table panel for the block at `pos` — binds the
    // panel's UI to that block's per-block grid (created on demand; a
    // crafting_table block always owns one — see setBlock). The
    // openFurnacePanel twin (Phase 44).
    void openTablePanel(const BlockPos& pos);
    void openTablePanelFor(Player& p, PlayerSession& s, const BlockPos& pos);
    // The grid of the table block at `pos` (default-constructs one when
    // absent — defensive: the setBlock invariant makes that unreachable).
    CraftingGrid& tableAt(const BlockPos& pos);
    // V2-12: opens the enchanting panel for the block at `pos` — clears the
    // session item/lapis slots, re-rolls the (empty) offers, binds the UI.
    // The openTablePanel twin (the session-state variant: no per-block
    // state — the offers are derived on demand).
    void openEnchantPanel(const BlockPos& pos);
    void openEnchantPanelFor(Player& p, PlayerSession& s, const BlockPos& pos);
    // V2-12: re-derives the panel's offers from the CURRENT item slot (the
    // deterministic roll — bookshelfCount around the table + the item's
    // class/enchantability + the player's xpSeed). Called on open and after
    // any item-slot change. An empty item slot clears the offers.
    void regenEnchantOffers();
    void regenEnchantOffersFor(Player& p, PlayerSession& s);
    // V2-12: applies the offer at `row` (0..2) to the item slot when the
    // four gates pass (item has a class + not yet enchanted + level >=
    // xpCost + lapis >= lapisCost): sets the enchants, deducts the whole
    // XP levels (the remainder kept), deducts the lapis, advances the
    // xpSeed, plays "craft", and clears the offers. Returns true when
    // applied (the game's click routing uses the cue for nothing else —
    // the sound is played here).
    bool applyEnchantOffer(int row);
    bool applyEnchantOfferFor(Player& p, PlayerSession& s, int row);
    // V2-12: return one enchant panel slot's stack to the inventory (a full
    // inventory drops it at the player's feet). The closePanel cleanup.
    void returnEnchantSlot(ItemStack& stack);
    void returnEnchantSlotFor(Player& p, PlayerSession& s, ItemStack& stack);
    // Phase 71: the right-click campfire cooking interaction (the
    // kInteractCampfire branch of the right-click chain): a selected
    // smeltable item places one unit on an EMPTY slot (consumed from the
    // hand); an empty hand takes the slot's item back (Minecraft: the take
    // needs a free hand); every other combination is a no-op. The click is
    // consumed either way (a campfire is never placed through this
    // branch).
    void tryCampfireCook(const BlockPos& pos);
    void tryCampfireCookFor(PlayerSession& s, const BlockPos& pos);
    // The state of the campfire block at `pos` (default-constructs one
    // when absent — defensive: the setBlock invariant makes that
    // unreachable).
    CampfireCookState& campfireAt(const BlockPos& pos);
    // V2-13 (the bed sleep, core/bed): the right-click on a bed FAMILY
    // block (the caller guarantees the kDyeBaseBed property — the family
    // check, never a color id) and a NON-dye held item (a held dye is the
    // Phase-41 recolor's, which falls through). The three gates, in
    // order (the pure predicates own the boundaries, core/bed):
    //   b — the player's feet within kSleepMaxDistance (3, the Minecraft
    //       value) of the bed cell's center;
    //   c — no HOSTILE mob (the spawn-protection's data flag, the same
    //       check) in the 8x5 box around the bed;
    //   a — the night window (the day-cycle's dusk->dawn span).
    // b or c failing = the whole action is refused (no spawn point —
    // the documented deviation 6). Passed: the respawn point is the bed
    // cell's center (Minecraft: the right-click sets it, night or day);
    // inside the window the sleep completes IMMEDIATELY (the documented
    // deviation 2 — no 101-tick entry animation): the time jumps to
    // kDawnStart (the 06:00 dawn), a RAINING weather resets to CLEAR
    // (the Minecraft "sleeping clears the rain" rule — a clear sky is
    // untouched; the V2-4 machine's pure initial state, a fresh CLEAR
    // segment off the same world-seed streams), the "sleep" cue plays
    // (spatialized at the bed) and the "sleep" tutorial event emits
    // (the closed vocabulary's new bare word — no series JSON change).
    // The click is consumed either way (no placement fall-through).
    void trySleepFor(Player& p, const BlockHit& hit);
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
    // V2-13 (the respawn flow, core/bed): the R key's single choke point
    // (the game side of the death screen — updateSurvival calls this
    // instead of the player's respawn() directly). The RESPAWN-TIME bed
    // validation (the Minecraft rule — only at respawn, the coordinate
    // memory survives the bed's break/recolor/reorientation in between):
    // the remembered spawn (the player's m_spawnPos — the bed cell's
    // center from the last sleep / point-set, or the world spawn) is
    // checked by the pure bedRespawnValid (the cell holds a bed family
    // block + the player's AABB fits); a valid bed respawns AT the bed,
    // an invalid one (no bed, a blocked headroom) falls back to the
    // WORLD spawn (m_mobs.spawnPoint — captured at init, before the
    // save load, the spawn-protection anchor). The fallback writes the
    // world spawn back (setSpawnPos): a broken bed therefore restores
    // the world spawn PERMANENTLY (the Minecraft rule — the coordinate
    // memory is re-pinned to what the respawn actually used).
    void respawnPlayer();
    void respawnPlayerFor(Player& p, PlayerSession& s);
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
    std::pair<int, int> uiMouseFor(const Input& in) const;
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
    // GEN-9: rebuilds the moisture tracking set from the world — every
    // cell whose block carries a model.moisture_top variant (the
    // rescanSaplings twin; the wetness is NOT rescanned into storage —
    // it is derived at mesh time, the map value is seeded with the first
    // evaluation so a just-loaded farm only re-meshes on a real flip).
    // Runs at the end of buildScene — after the save load, whose block
    // edits restore the placed farmland.
    void rescanMoistureBlocks();
    // GEN-9: the moisture convergence sweep — re-evaluates the wetness of
    // every tracked moisture cell (position-sorted, the soilIsWet pure
    // predicate over the LIVE fluid field) and marks the cell's chunk
    // dirty ONLY when the wetness flipped. Runs on a frame-locked cadence
    // (kMoistureRefreshEveryNFrames) because the mesh-time derivation can
    // bake STALE wetness: the worker mesher reads the fluid field
    // cross-chunk, and a farmland chunk meshed before its water neighbor
    // streamed in bakes dry until this sweep re-meshes it. Deterministic;
    // a no-op while the tracked set is empty (the fresh world / the perf
    // scenes fast-exit).
    void refreshMoistureStates();
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
    // V3-1 Phase A: the portal load-time frame validity (the Minecraft
    // update semantics + the PITS 383① two-way cross-check discipline):
    // rebuilds the portal tracking set (m_states.portals) from the world,
    // and every portal cell that does not sit strictly inside a complete
    // frame (a hand-edited save's orphan) is set to air. Runs at the end
    // of buildScene (like rescanBeacons; no save-format field: the portal
    // is a plain stateful block, the format-15 states segment carries its
    // axis — zero format). A fresh world finds nothing (worldgen rejects
    // stateful blocks — the structural-immunity root).
    void rescanPortals();
    // The pure generator as a World chunk callback — used at world
    // creation AND as the generation reference for the save diffs (the
    // full diffWorld and the PIPE-2 journalDiff).
    World::ChunkGenerator makeChunkGenerator() const;

    // ------------------------------------------------------------------
    // V3-1 Phase B (v3.4.0): the nether dimension + the real travel.
    // ------------------------------------------------------------------
    // The player's dimension (the streaming / entity / interaction /
    // render / save discriminator). Overworld is the default (the pre-
    // Phase-B bit-identical path — every dimension-gated expression
    // reduces to the legacy overworld one while it holds); it flips on
    // the travel pipeline (travelPortal) and the load's player rebind.
    Dimension m_playerDimension = Dimension::Overworld;
    bool playerInNether() const {
        return m_playerDimension == Dimension::Nether;
    }
    // V3-10a: the WORLD->dimension discriminator (the setBlockInWorld
    // choke point's pointer test, the recon's risk ② redo: with a
    // third world the `&w != &m_world` binary breaks — an explicit
    // identity check over the engaged dimensions, overworld the
    // default; unengaged dimensions are skipped, so a stray pointer
    // can only ever be the overworld).
    Dimension dimensionOfWorld(const World& w) const;
    // V3-10a: the End dimension's reader (the playerInNether's twin — the
    // 33 dispatch sites ternary through the three predicates).
    bool playerInEnd() const { return m_playerDimension == Dimension::End; }
    bool inOverworld() const { return m_playerDimension == Dimension::Overworld; }
    // The dimension the player (and therefore the interaction, the
    // raycast, the entity list, the streaming and the render) currently
    // addresses. The overworld returns m_world — the pre-Phase-B
    // bit-identical path (the structural-immunity root: a no-travel
    // session never touches the nether family).
    World& currentWorld();
    const World& currentWorld() const;
    // The dimension's entity list (the player crosses; the drops /
    // arrows / orbs it spawns and collects ride the CURRENT dimension —
    // D3: the lists are per-dimension, the inactive dimension's entities
    // freeze (no tick, the references stay valid — the design's ruling)).
    std::vector<std::unique_ptr<Entity>>& currentEntities();
    const std::vector<std::unique_ptr<Entity>>& currentEntities() const;
    // The dimension's portal tracking set (the setBlock choke point +
    // the rescan + the ambience + the travel search all read it — the
    // overworld's m_states.portals / the nether's m_nether.portals).
    const std::map<BlockPos, BlockID>& portalsOf(Dimension dim) const;
    std::map<BlockPos, BlockID>& portalsOf(Dimension dim);
    // The dimension's streaming context (the DimCtx bind — the overworld
    // family / the nether family; the shared pool for all three).
    DimCtx overworldCtx();
    DimCtx netherCtx();
    // V3-10a: the End's streaming context (the netherCtx's third member —
    // the protectStates = the End portal set, the shared pool).
    DimCtx endCtx();
    // V3-10a: the End's ambient light floor (the recon's §5 point 6 —
    // the Minecraft End's ambient_light 0.25: the 10a island has NO
    // light sources, so without it the plateau is untestable black; the
    // overworld / nether keep their config-derived values bit-for-bit).
    // The set points: init + every dimension flip frame (travelEnd /
    // the respawn rebind / the load's player rebind) — the shader
    // uniform u_ambient, zero per-frame cost (a GL state set, the
    // per-run constant's shape).
    static constexpr float kAmbientEnd = 0.25f;
    // The per-dimension u_ambient apply (the flip frame's presentation
    // set — the End reads kAmbientEnd, every other dimension the
    // config-derived floor, the init's one-time set's per-dimension
    // generalization).
    void applyDimensionAmbient();
    // D2 (the laziness): the nether is constructed on first need — the
    // first travel, a load whose nether section is non-empty or whose
    // player sits in the nether (before ANY chunk / mesh / journal /
    // streaming job / streaming memory exists, the structural-immunity
    // strongest form). Builds the skyless World (D5), binds the pure
    // nether fill as its generator callback + the save-diff reference,
    // installs the nether's cell-mutation observer (the nether journal),
    // and copies the streaming radius (the shared config value — no new
    // config key). Idempotent (a second call is a no-op).
    void ensureNether();
    // The nether World (valid after ensureNether; the accessors belt-
    // check — every consumer runs only once the nether is engaged).
    World& netherWorld();
    // The nether's generation reference (the fillNetherChunk closure —
    // the makeChunkGenerator twin: streaming jobs, first-touch creation,
    // the save-diff reference and the unload retained diffs all run it).
    World::ChunkGenerator makeNetherChunkGenerator() const;
    // The dimension's generation SEED (the overworld's stored seed / the
    // nether's derived one — the drop-scatter / ambience / tutorial
    // decorrelation streams read the CURRENT dimension's seed, the
    // xp_seed single-source-of-truth twin: the nether's is derived,
    // never stored).
    std::int64_t currentSeed() const;
    // D6 step 3 (the synchronous catch-up): the 3x3 column disk around
    // (cx, cz) of the given dimension — every missing column generated
    // synchronously (the first-touch precedent amplified: ensureColumn +
    // loadedColumns, the ~ms one-time cost) + ONE synchronous relight
    // over the disk's bounding box (newTerrain — the nether's skyless
    // source 0 through the rescan). Runs in the main-thread-writer
    // window (the travel frame / the load), so no window conflict (the
    // design's risk-surface ② ruling).
    void catchUpDisk(Dimension dim, int cx, int cz);
    // D6 (the single-frame frame-locked travel pipeline): the completed
    // standing countdown's consumer — the 8:1 mapping, the target
    // ±16 portal search (the per-dimension tracking set, O(set size),
    // no world scan), the generation fallback (the 4x5 corners-included
    // frame + the 2x3 bedrock platform through the setBlockInWorld
    // choke point — the journal / editedColumns protection rides
    // along), the catch-up disk, the player's world rebind (setWorld) +
    // restorePosition (the fall pin — the Phase 36 load twin: the
    // in-air landing takes no free fall damage) + camera, the dimension
    // flip (the streaming center re-engages the target dimension next
    // frame; the source dimension's streaming freezes — zero post, the
    // steady-state zero contention), the cooldown arm (the destination
    // inside-portal ping-pong guard), the two travel sounds (the
    // trigger on entry — the arming frame — and the travel fade here)
    // and the "Traveled to <dim> at (x,y,z) from (sx,sy,sz)" info log.
    // No loading screen, no transition frame (the design's deviation 7):
    // the switch is one deterministic frame.
    void travelPortal(const PortalFrame& sourceFrame);
    // The load-time nether portal frame validity (the rescanPortals
    // twin over the nether world — the PITS 383① two-way cross-check:
    // a portal cell not strictly inside a complete frame is an orphan
    // -> air; the tracking set rebuilds). Runs after the nether's edit
    // replay at load (and is a no-op while the nether is unengaged).
    void rescanNetherPortals();

    // ------------------------------------------------------------------
    // V3-10a: the End dimension (the nether family's third member — the
    // lazy world + the entry/exit travel pipeline + the embed-eye
    // interaction). The travelPortal / updatePortalEntry originals are
    // NEVER touched (the nether path's bit-identical red line).
    // ------------------------------------------------------------------
    // The End's lazy construction (the ensureNether's twin — the skyless
    // World + the pure fillEndChunk generator + the end journal observer
    // + the streaming radius; idempotent; the init's EndGen::create
    // fail-fast feeds the cache this consumes).
    void ensureEnd();
    // The End World (valid after ensureEnd; the belt-checked accessor,
    // the netherWorld's twin).
    World& endWorld();
    // The End's generation reference (the fillEndChunk closure — the
    // makeNetherChunkGenerator's third member).
    World::ChunkGenerator makeEndChunkGenerator() const;
    // The End's fluid pass (the updateNetherFluidTick's dimension-bound
    // twin — 10a's worldgen holds no fluid; a player-poured bucket
    // settles the same way, the static-sea zero-churn discipline).
    void updateEndFluidTick();
    // The end portal's entry countdown (the updatePortalEntry's OW-side
    // twin: the player's AABB overlaps a TRACKED end_portal cell in the
    // OVERWORLD — the 60f kPortalEntryFrames count, the GUI-mutex auto-
    // close, the ride dismount; the arm plays the portal_trigger).
    void updateEndPortalEntry();
    // V3-10d (the ender dragon): the boss's per-frame GAMEPLAY pass
    // (the updateEndPortalEntry's dimension-twin, called every frame
    // after it — the single choke point over the dragon's whole
    // arena logic, all gated on playerInEnd() so a no-End session is
    // one bool check, the structural-immunity root). Runs, in order:
    //   1. the first-visit SPAWN schedule (the 60-frame delay over the
    //   no-dragon + no-egg + mobs-enabled gate — the dragon's singleton
    //   appears high over the exit and settles into the orbit);
    //   2. the CRYSTAL census (the 6 fixed slots' live count -> the
    //   dragon's setCrystalsAlive feed + the crystalDelta flag) + the
    //   crystal FIRE (each live crystal's cell+1y = a permanent end
    //   fire block, the D9 closed set's zero-churn read) + the HEAL
    //   cadence (the dragon +1 HP / 30 frames from its nearest live
    //   crystal in 32, the data-order tie-break);
    //   3. the CONTACT damage pass (the contact_damage > 0 mob's
    //   strict-AABB player overlap, the 1-s session cooldown — the
    //   wing sweep);
    //   4. the BREATH CLOUD damage pass (the player's overlap of a
    //   live cloud -> the dragon_breath effect's 30-frame re-grant);
    //   5. the ASCENSION ritual driver (the dead boss's corpse flight
    //   to the perch, the 200-XP burst scheduler, the dragon_death
    //   cue at the ritual's end — the m_dead frame's egg/XP bookkeeping
    //   ran at the death, the R14's 0-frame-gap ruling).
    void updateDragonGameplay();
    // V3-10d: the dragon's singleton SPAWN (the attemptMobSpawn's
    // twin over the fixed point — the seed's kSaltDragon xor, the
    // setCrystalsAlive / setDragonAnchors feeds, the shoot callback's
    // dragon fireball wiring, the growl/breath seams).
    void spawnDragon();
    // V3-10d: the CRYSTAL shatter (the breakBlock's explode hook's
    // caller-over — the arrow sweep's direct path): set the cell air
    // (the journal's choke), relight, detonate the block's data
    // explode_radius over its center (the no-chain structural
    // property — the blast's setBlock never re-enters this function),
    // and the crystal_break_damage's heal-source self-harm (the
    // dragon's 10 when a HEALING crystal shatters, the R4's frame
    // ruling). Returns true when a live crystal was shattered.
    bool shatterCrystalAt(int x, int y, int z);
    // V3-10d: the dragon's two RENDER seams (the crystal->dragon HEAL
    // beam's blend quad + the breath CLOUDS' blend discs — both the
    // drawBeaconBeams / drawParticles blend-pass twins, called from
    // the render's blend section; a no-dragon session is one null /
    // empty check each, the structural-immunity root).
    void drawDragonBeam(const Mat4& view, const Mat4& projection);
    void drawBreathClouds(const Mat4& view, const Mat4& projection);
    // The dragon FX's lazy unit column mesh (the heal beam's geometry,
    // the breath cloud's scaled-flat disc — the m_mobShadowMesh's lazy
    // twin, built on the first FX frame).
    const Mesh& dragonFxMesh();
    // The End's real travel (the travelPortal's twin, NO 8:1 mapping —
    // the 1:1 independent origin): the OW->End leg writes the entry
    // platform (the 5x5 obsidian at (40,52,0), the per-entry rewrite —
    // the journal's first-visit diff, idempotent thereafter) and lands
    // at the fixed point facing west; the End->OW leg (the origin exit)
    // returns to the overworld's spawn (the bed-validated respawn
    // precedent). Both legs: catchUpDisk + setWorld + flip + the 900f
    // cooldown + the travel sound (reused — zero new wav, deviation).
    void travelEnd();
    // The load-time End portal frame validity (the rescanNetherPortals
    // twin — the entry's OW ring: an end_portal cell not strictly inside
    // a complete 12-eyeful ring is an orphan -> air; the tracking set
    // rebuilds. Runs after the end's edit replay at load).
    void rescanEndPortals();
    // V3-10b: the pearl's landing seam (the stuck pass's consumer — a
    // stuck pearl teleports the THROWER to the landing point): the
    // 3x3 catch-up disk around the target, the uniform 2.5-block
    // offset roll (the particle counter's deterministic stream — the
    // frame-locked contract), the hasRoomAt probe with the +1..+3
    // up-step (the portal's landing-probe twin), the restorePosition's
    // fall re-anchor (the pearl cancels the fall damage, the MC twin),
    // the dismount (a rider throws, the MC 1.10 twin) + the teleport
    // cue + the DEBUG log (the E2E evidence). The probe's total failure
    // (a sealed pocket) swallows the pearl WITHOUT the teleport (the
    // documented deviation — the MC always teleports).
    void pearlLanding(Projectile* pearl);
    // V3-10c (the enderman): the teleport's GAME side — the callback the
    // mob's roll invokes with its fixed candidate array (the stream
    // contract is the roll's; this is pure world I/O). Probes the
    // candidates in order (the catch-up disk + the downward hasRoomAt /
    // fluid landing probe — the pearlLanding's twin over the 16-cell
    // depth) and lands the first roomy one (the setPosition + the mob's
    // finishTeleport re-anchor + the landing cue + the DEBUG line); the
    // total failure swallows (the mob stays put). Runs in the update's
    // main-thread-writer window (the flush barrier ran at the top), so
    // the disk + the writes are safe.
    bool executeMobTeleport(
        MobEntity* mob, const std::array<TeleportCandidate,
                                         kTeleportAttempts>& candidates);
    // The embed-eye interaction (V3-10a's A-lite activation): the
    // crosshair on an end_frame cell, the selected item an ender_eye —
    // one eye is consumed, the cell becomes the EYED frame, and the
    // ring's scan activates the center (all 12 eyed + center air -> the
    // 3x3 end_portal, the destroyable-false frames' no-deactivation
    // path, deviation). The click is consumed on a frame cell (the
    // failed-milk rule); a non-frame target falls through (the
    // bit-identical legacy path).
    bool tryEyePortal(Player& p, PlayerSession& s);
    // V3-10b (the ender pearl): the TELEPORT-THROW — the crosshair is
    // the aim (the aimForward twin), the selected item carries the data
    // predicate throws_teleport, the cooldown is 0 and the player is
    // alive: one pearl is consumed (creative consumes none — the V3-11a
    // no-consumption twin) and the projectile launches from the eye
    // ahead along the aim at kPearlLaunchSpeed WITH the thrower's
    // velocity added (the MC 1.9+ twin). The click is consumed on a
    // throw (the cooldown's window included — a rapid re-click inside
    // the 60f window is the silent no-op, the failed-milk rule's
    // cooldown shape); a non-throwable item returns false (the legacy
    // bit-identical fall-through). The landing's teleport is the
    // pearlLanding seam (the stuck pass's twin branch).
    bool tryThrowPearl(Player& p, PlayerSession& s);

    // V3-10a: the ender_eye item's resolved id (the init name lookup —
    // invalid = the embed interaction is structurally inert, the
    // absent-asset twin).
    ItemID m_enderEyeId = ItemID::invalid();
    // V3-10b: the pearl's throw cooldown moved into the PlayerSession
    // (V3-12b's per-player family — the frame-locked session state, the
    // kPearlThrowCooldownFrames window after a throw; 0 = ready, never
    // saved, the fly-toggle / portal-cooldown precedent class).

    GameArgs m_args;
    Config m_config;
    float m_uiScale = 1.0f; // ui.scale (Phase 34); 1.0 = pixel-exact
    // ui.font_scale (UX-8e): the text-only zoom applied in renderUi —
    // 1.0 = the bit-exact v1.44.0 baseline (s * 1.0 is IEEE-exact).
    // An init-time constant like m_uiScale (never a frame state, so it
    // carries no save-format field).
    double m_fontScale = kFontScaleDefault;
    AssetManager m_assets;
    Window m_window;
    Input m_input;
    Camera m_camera;
    Renderer m_renderer;

    // Data-driven content.
    BlockRegistry m_blocks;
    BiomeRegistry m_biomes;
    // V3-2 Part 1: the nether's INDEPENDENT biome pool (assets/data/
    // biomes_nether — the D1: the overworld's m_biomes stays untouched,
    // the nether's selection walks this pool over the derived seed).
    BiomeRegistry m_netherBiomes;
    // V3-10b: the End's biome pool (data/biomes_end/ — 10b ships the
    // single end_plains, the 10d's second-biome seam; the nether pool's
    // member shape).
    BiomeRegistry m_endBiomes;
    TextureAtlas m_atlas;

    // Voxel world + per-chunk GPU meshes.
    World m_world;
    std::unordered_map<ChunkPos, ChunkMeshData> m_chunkMeshData;
    std::unordered_map<ColumnPos, ColumnMesh> m_columnMeshes;
    // Columns whose merged GL mesh is stale (a chunk mesh landed this
    // frame — integrated from a worker mesh job, PIPE-1): rebuilt
    // (GL upload) at the end of updateChunkMeshes.
    std::set<ColumnPos> m_columnRebuildPending;

    // V3-12b (R2): the shared SIM clock (seconds): incremented every
    // frame by the frame's dt (solo: the local wall dt, net: the
    // host-broadcast dt — the two machines accumulate the identical
    // sequence). The wall-clock double-tap windows (the Space fly
    // toggle, the inventory double-click collect) read this in NET
    // mode; the solo path keeps nowSeconds() verbatim (the bit-level
    // red line — the accumulator is written, never read, in solo).
    double m_simTime = 0.0;

    // Seeded world generator (built in buildScene). Optional: the game only
    // touches it after init succeeded.
    std::optional<WorldGenerator> m_generator;

    // V3-12b (the Player multi-ification, design §3.1): the PLAYER LIST
    // — every player's body (position / orientation / health / hunger /
    // XP: the Player-intrinsic state). Index 0 = the PRIMARY (the host
    // player on every machine — the R1 red line: every "the player" sim
    // anchor is primary-indexed, a local-relative anchor is the double-
    // machine divergence root). m_localSlot = the LOCAL machine's own
    // player (0 in solo / on the host, 1 on the client — the camera /
    // HUD / death screen read localPlayer()). Solo: size 1, primary ==
    // local — the bit-identical red line.
    std::vector<std::unique_ptr<Player>> m_players;
    int m_localSlot = 0;
    // V3-12b (R9): the valid spawn candidates collected at init (the
    // net connect's player-2 spawn reads index 1 — the
    // spawnPositionForIndex's source).
    std::vector<Vec3> m_spawnCandidates;
    // The first-person actor (built in buildScene after the world exists;
    // it holds references to the world and the block registry). V3-12b:
    // a cached PRIMARY alias (m_players[0].get(), set at the single
    // construction site) — the 354-site m_player reference family keeps
    // compiling unchanged and points at the host player in every net
    // session (the "the player" sim-semantic anchors, design §3.1's
    // pass-ownership table). nullptr before init (the pre-V3-12b
    // std::optional's empty state, the has_value checks' twin).
    Player* m_player = nullptr;
    // The dual-alias accessors (design §3.1): primary = index 0 (every
    // machine's shared definition — the sim anchors), local = the
    // machine's own player (camera / HUD / death screen / the first-
    // person arm). Solo they return the same object (the bit-level
    // root); a null list (pre-init) returns nullptr / defaults.
    Player* primaryPlayer() const {
        return m_players.empty() ? nullptr : m_players[0].get();
    }
    Player* localPlayer() const {
        return m_players.empty() ? nullptr
             : m_players[static_cast<std::size_t>(m_localSlot)].get();
    }
    // V3-12b (R9): the i-th valid spawn candidate (the init's collected
    // list; the index wraps when the world has fewer candidates than
    // players — the single-player world's shape). -safe: an empty list
    // returns the world origin surface (the init's spawnPos fallback).
    Vec3 spawnPositionForIndex(std::size_t i) const {
        if (m_spawnCandidates.empty()) {
            return Vec3 {0.5f,
                         m_generator ? m_generator->surfaceHeight(0, 0) + 1.0f
                                     : 64.0f,
                         0.5f};
        }
        return m_spawnCandidates[i % m_spawnCandidates.size()];
    }

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
    // The streaming family's job TYPES (UX-2 B2): pure types with no
    // construction-order effect — declared before StreamingState because
    // its job deque members are std::deque of std::unique_ptr of them,
    // and a nested type is visible to a nested struct only when declared
    // first (the B1 BeaconBeam pattern).
    // One in-flight generation job: a whole column
    // (kVerticalChunksPerColumn scratch chunks). `done` is written by the
    // worker and read by the main thread only after pool->flush()
    // (happens-before via the pool's mutex).
    struct StreamJob {
        ColumnPos column;
        std::unique_ptr<Chunk[]> chunks;
        bool done = false;
    };
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
    // columns let the main thread clear m_streaming.unlitColumns there
    // (see collectLitColumns).
    struct LightJob {
        std::vector<std::array<BlockPos, 2>> boxes;
        std::set<ColumnPos> columns;
        std::set<ChunkPos> dirtyChunks;
        bool done = false;
        // 99.11: the job's kind — false = a streaming job (its columns
        // leave m_streaming.unlitColumns at reap: the lit-gate is
        // new-terrain only), true = a fluid catch-up job (its columns are
        // already lit and left m_fluidLightPending at POST time; the
        // reap's erase is the contract belt — see collectLitColumns).
        // The two kinds never touch each other's bookkeeping: a newly
        // integrated column can have its fluid churned before its
        // streaming job lands, and the streaming reap must NOT drop that
        // fluid pending entry (its re-light would be lost); a fluid reap
        // must not touch m_streaming.unlitColumns (fluid columns are not
        // unlit).
        bool fluid = false;
    };
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
    // The streaming/worker family (UX-2 B2): everything the chunk
    // streaming pipeline maintains — the worker pool, the in-flight job
    // deques (gen / mesh / light), the pending-column bookkeeping sets,
    // the streaming hysteresis + radius state, the loaded/unload mirror
    // and progress counter, and the save journal + retained-column diffs
    // (the PIPE-2 / STRM-2 exit-save O(edits) machinery). Collected so a
    // NEW streaming subsystem adds one member here and writes
    // m_streaming.xxx explicitly, instead of scattering a member across
    // the streaming bookkeeping. Member order = the original declaration
    // order of the family (construction order = initialization order =
    // behavior red line). m_flammableChunks / m_lightPendingColumns /
    // m_fluidLightPending stay OUT — the burn + light bookkeeping
    // (cross-domain, the B-segment residual).
    struct StreamingState {
        std::unique_ptr<ThreadPool> pool;
        // In-flight generation jobs (deque: the heap-allocated StreamJob
        // never moves, so worker lambdas may capture it by raw pointer).
        std::deque<std::unique_ptr<StreamJob>> streamJobs;
        std::deque<std::unique_ptr<LightJob>> lightJobs;
        // In-flight mesh jobs (deque: the heap-allocated MeshJob never
        // moves, so worker lambdas may capture it by raw pointer).
        std::deque<std::unique_ptr<MeshJob>> meshJobs;
        // Columns whose mesh job is in flight (not yet integrated). Same
        // paradigm as streamPending: they are excluded from re-posting
        // (nextMeshColumns) and protected from unloading.
        std::set<ColumnPos> meshPending;
        // Columns whose light has NOT propagated yet (integrated this
        // session or first-touch created, their lighting job still in
        // flight). Their chunks are NOT meshed until the job completes:
        // the old design meshed them at light 0 (black faces) and
        // re-meshed after the light landed — with the remesh budget that
        // re-mesh could lag by the whole dirty backlog, so freshly loaded
        // terrain showed black faces for seconds (99.2 walking-run
        // report). Meshing once, lit (pop-in), is also less work; PIPE-1
        // enforces the same rule in the worker selection (a column in
        // this set never gets a mesh job).
        std::set<ColumnPos> unlitColumns;
        // Columns with a generation job in flight (not yet in the World).
        std::set<ColumnPos> streamPending;
        // Columns the player has MODIFIED (the setBlock choke point —
        // break / place / rotate / growth / burn / campfire, plus the
        // fluid-bucket scoop and pour paths). Their diff is recorded in
        // the save, so the chunks must survive until the save is written:
        // never unloaded.
        std::set<ColumnPos> editedColumns;
        // Streaming hysteresis (see chunk_streaming): engaged when the
        // player reaches the edge of the loaded area, disengaged when the
        // player is deep inside it again. While engaged, the full radius
        // disk around the player is filled (the capped view_distance
        // becomes visible terrain).
        bool streamingEngaged = false;
        // The streaming radius in chunk columns (config
        // world.view_distance), UNCLAMPED: the logs and the F3 overlay
        // report this raw value; the geometric consumers (stream disk,
        // unload band) clamp it to kStreamCapRadius (STRM-1 far-plane
        // cap).
        int streamRadius = 8;
        // The loaded columns (incremental mirror of the World's chunk map
        // — streaming decides on THIS instead of scanning every chunk:
        // the scan is ~9k-200k map entries + a sort per frame at full
        // ring). Maintained by buildScene (initial),
        // integrateStreamedColumns (+), unloadFarColumns (-), and the
        // setBlock choke point (+ a column it completed for a first-touch
        // edit). A column is "loaded" when ANY chunk of it exists — the
        // ensureColumn invariant makes "any" and "all" equivalent.
        std::set<ColumnPos> loadedColumns;
        // Total columns unloaded since session start (the periodic INFO
        // progress line reports it — proof the unload pass is working).
        std::size_t unloadedColumnsTotal = 0;
        // The save journal (PIPE-2): the session's cell-edit history as
        // position sets, fed by the World's cell-mutation observer
        // (installed at the top of buildScene — BEFORE tryLoadSave, so
        // the save load's replay seeds it automatically) and evicted per
        // column by unloadFarColumns. At exit the save diff (journalDiff)
        // + the stair array derive from it instead of the O(N) world
        // scans; when it is invalid (the kSaveJournalMaxEntries cap was
        // exceeded) or --save-full-diff is set, collectSave falls back to
        // the legacy full diffWorld + stair scan.
        SaveJournal saveJournal;
        // STRM-2: the retained diffs of UNLOADED columns — a column that
        // left the world while the session modified it (the pure-fluid
        // churn: far lava flow / water film, the fire burn, the
        // water-lava reaction — the writes that go through
        // World::setBlock/setFluid directly and so never reach the
        // editedColumns protection; their columns stay unloadable by
        // design). unloadFarColumns snapshots the column's journal diff
        // (retainColumnDiff, BEFORE the chunk erase) into this map;
        // integrateStreamedColumns re-applies a re-streamed column's diff
        // (blocks first, then fluids — the applyWorldEdits order) and
        // erases the entry; collectSave merges every entry into the exit
        // save (BOTH paths — the journal diff and the legacy/oracle full
        // diff). INVARIANT (the no-duplicate-cells proof of the merge):
        // a column is in at most ONE of the save journal and this map —
        // unload snapshots before it evicts the journal entries,
        // integrate applies before it erases the entry (the observer
        // re-journals the re-applied writes, so the next unload snapshots
        // again: the self-similar loop). The retained value is DIFF
        // ENTRIES (a few to a few dozen cells for a fluid column), not
        // chunks — the memory stays bounded over a long wandering
        // session. Empty columns are never stored.
        std::map<ColumnPos, RetainedColumnDiff> retainedColumns;
    };
    StreamingState m_streaming;
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
    // in BOTH this set and m_streaming.unlitColumns (a freshly integrated
    // column
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

    // The mob family (UX-2 B3): everything the mob subsystem maintains —
    // the data-driven type registry (assets/data/mobs) + the spawn config
    // (assets/data/mob_spawning.json) + the enable flag (config
    // mobs.enabled), the difficulty (config mobs.difficulty) + the WORLD
    // spawn point (the hostile spawn-protection circle center), the
    // per-type box-model meshes (built lazily), the ENT-1 limb-swing
    // assets (config mobs.limb_swing + per-type swing geometry + per-
    // entity dynamic meshes + frame-vertex scratch), and the spawn counter
    // (feeds the per-mob RNG seeds, so the same runs spawn the same mobs
    // with the same seeds). Consumer sites: game_init (the registry /
    // spawn config / difficulty / flag load + the cross-checks + the world
    // spawn capture), game_entities (the spawn cadence gate +
    // attemptMobSpawn + the despawn distance + drawMobs / mobMesh /
    // swingMobMesh / swingModel + the despawn-rebuild swing-mesh erase),
    // game_save (the SURV-7 stats vocabulary cross-check) and game_ui
    // (the F3 difficulty line). Collected so a NEW mob-system feature adds
    // one member here and writes m_mobs.xxx explicitly, instead of
    // scattering a member across the mob bookkeeping. Member order = the
    // original declaration order of the family (construction order =
    // initialization order = behavior red line). m_handIconMesh /
    // m_interaction stay OUT — the held-hand icon (Phase 42) and the
    // player input-channel (UX-2 B4, game_interaction) families.
    struct MobSystem {
        // Mobs (Phase 20): the data-driven types (assets/data/mobs) + the
        // spawn config (assets/data/mob_spawning.json), the enable flag
        // (config mobs.enabled, default true), the per-type box-model meshes
        // (built lazily) and the spawn counter (feeds the per-mob RNG seeds,
        // so the same runs spawn the same mobs with the same seeds).
        MobRegistry mobTypes;
        MobSpawningConfig spawnConfig;
        bool mobsEnabled = true;
        // Difficulty (Phase 25): config mobs.difficulty (default "normal"),
        // strict-parsed; scales the hostile spawn cadence, the spawn-protection
        // radius and the hostile damage (core/difficulty.hpp). spawnPoint is
        // the WORLD spawn (the fresh-world spawn, before save load) — the
        // center of the spawn-protection circle.
        Difficulty difficulty = Difficulty::Normal;
        Vec3 spawnPoint {0.5f, 0.0f, 0.5f};
        std::unordered_map<MobID, Mesh, MobIDHash> mobMeshes;
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
        bool limbSwing = true;
        std::unordered_map<MobID, SwingModel, MobIDHash> swingModels;
        std::unordered_map<const MobEntity*, Mesh> swingMeshes;
        std::vector<MeshVertex> swingFrameVerts;
        // V2-15: the WOOL VARIANT render assets (the sheep — the data-
        // flag gated: only a wool_drops mob ever touches these; every
        // other mob's render path is bit-identical, the structural
        // immunity root). `woolVariants`: the per-entity LAST APPLIED
        // variant tile (the re-upload trigger — a shear / dye / regrow
        // changes the tile, so the entity's dynamic swing mesh (the
        // swingMeshes entry) is dropped and re-uploaded from the
        // variant's base, and the static variant mesh is rebuilt).
        // `woolVariantMeshes`: the per-entity STATIC variant mesh (the
        // swingMeshes twin — lazy build, despawn free).
        // `variantSwingModels`: the per-(type, variant-tile) swing
        // geometry (the swingModels twin, the key extended with the
        // tile: the model's wool box rebuilt with the variant's UVs).
        std::unordered_map<const MobEntity*, std::string> woolVariants;
        std::unordered_map<const MobEntity*, Mesh> woolVariantMeshes;
        std::unordered_map<std::string, SwingModel> variantSwingModels;
        std::uint64_t spawnCounter = 0;
    };
    MobSystem m_mobs;

    // V3-1 Phase B (D1/D2): the NETHER dimension — the parallel named
    // family (the overworld's members are UNTOUCHED: m_world /
    // m_streaming / m_chunkMeshData / m_columnMeshes / m_entities /
    // m_flammableChunks / m_lightPendingColumns / m_fluidLightPending /
    // m_states all stay the overworld's — the zero-diff red line, the
    // gate's structural-immunity root). Lazy (D2): the World is nullopt
    // until ensureNether (the first travel / a load with a non-empty
    // nether section or a nether player) — before that there is no nether
    // chunk, no mesh, no journal, no streaming job, no streaming memory
    // (the strongest form). The pool is NOT a member: the nether's
    // streaming posts to the SHARED overworld pool (netherCtx binds
    // *m_streaming.pool — the flush barrier covers both dimensions'
    // jobs, and the inactive dimension's zero-post steady state keeps it
    // uncontended). Member order: all default-constructed, no cross-reads
    // (the construction-order red line).
    struct NetherDim {
        // The nether's world (SKYLESS — D5: the void / world top read
        // sky 0, the rescan top source 0; the day/night shader term and
        // the night floor are structurally inert there).
        std::optional<World> world;
        // The fill's resolved content. Populated at INIT (buildScene's
        // NetherGen::create — the fail-fast timing ruling, the
        // WorldGenerator::create's twin: a bad biome surface rejects
        // startup, not the first travel). ensureNether only CONSUMES
        // this cache (the lazy D2 is the nether World — the chunks /
        // meshes / journal / streaming — not the content resolution).
        // Invalid = structurally impossible post-init (the belt the
        // ensureNether's named-abort reads).
        NetherGen gen;
        // The nether's streaming family (the overworld's m_streaming
        // twin — the same StreamingState struct, its own instance; the
        // pool member stays null, the shared pool is the overworld's).
        StreamingState streaming;
        // The nether's per-chunk CPU geometry + the per-column GL meshes
        // (the overworld's m_chunkMeshData / m_columnMeshes twins).
        std::unordered_map<ChunkPos, ChunkMeshData> chunkMeshData;
        std::unordered_map<ColumnPos, ColumnMesh> columnMeshes;
        std::set<ColumnPos> columnRebuildPending;
        // The nether's cross-domain bookkeeping (the overworld's
        // m_flammableChunks / m_lightPendingColumns / m_fluidLightPending
        // twins — the burn pass's gate / the streaming light jobs'
        // pending set / the fluid catch-up's pending set).
        std::set<ChunkPos> flammableChunks;
        std::set<ColumnPos> lightPendingColumns;
        std::set<ColumnPos> fluidLightPending;
        // The nether's entity list (D3: the player crosses, the nether's
        // drops / arrows / orbs live here — Phase B spawns no mobs, the
        // empty pool below; the list holds the player-thrown content).
        std::vector<std::unique_ptr<Entity>> entities;
        // The nether's mob system (Phase B: the EMPTY pool — no mobs
        // data, the structural-immunity root; Phase C fills it).
        // V3-3: the pool IS filled (data/mob_spawning_nether.json,
        // parsed at init — the loadNetherSpawnPool's fail-fast; the
        // spawn gate fast-exits on the empty config only for a
        // stripped asset set). The type table is the game's SHARED
        // registry (m_mobs.mobTypes — the single-table ruling: a mob's
        // data is dimension-agnostic content; this member's mobTypes
        // stays default-empty, never loaded). The live entities are
        // the dimension's list above (D3: the mobs are born and tick
        // here — the inactive dimension's freeze is the time-capsule).
        MobSystem mobs;
        // The nether's portal tracking set (the overworld's
        // m_states.portals twin — the choke point / the rescan / the
        // ambience / the travel search read it; position-sorted
        // std::map, deterministic iteration).
        std::map<BlockPos, BlockID> portals;
    };
    NetherDim m_nether;
    // V3-10a: the END dimension — the NetherDim's third member (the
    // 12-point mirror: world (skyless) / gen (EndGen, init fail-fast) /
    // streaming family / mesh data / bookkeeping sets / entities / the
    // mobs empty pool (10c's two-phase slot) / the exit portal tracking
    // set). Same laziness (ensureEnd on first travel / load) — a
    // no-End session never allocates any of it (the structural-immunity
    // strongest form: the S1 anchor's byte-identity rests on it).
    struct EndDim {
        // The End's world (SKYLESS — the nether's D5 twin: the void /
        // world top read sky 0; no day/night, no weather — the render's
        // 6-point gate does not consume them).
        std::optional<World> world;
        // The fill's resolved content (init's EndGen::create — the
        // fail-fast timing ruling, the nether's twin). 10a: the four
        // minimum names (endstone / obsidian / end_portal /
        // end_portal_frame_eyed); 10b widens the vocabulary.
        EndGen gen;
        // The End's streaming family (the nether's member-for-member
        // twin; the shared pool is the overworld's).
        StreamingState streaming;
        std::unordered_map<ChunkPos, ChunkMeshData> chunkMeshData;
        std::unordered_map<ColumnPos, ColumnMesh> columnMeshes;
        std::set<ColumnPos> columnRebuildPending;
        // The cross-domain bookkeeping (the nether's twins).
        std::set<ChunkPos> flammableChunks;
        std::set<ColumnPos> lightPendingColumns;
        std::set<ColumnPos> fluidLightPending;
        // The End's entity list (D3: the player crosses; the drops /
        // arrows it spawns live here — 10a spawns no mobs, the empty
        // pool below; the player-thrown content only).
        std::vector<std::unique_ptr<Entity>> entities;
        // The End's mob system (10a: the EMPTY pool — no mobs data, the
        // structural-immunity root; V3-10c fills it with the
        // mob_spawning_end.json's two-phase slot, the V3-3 precedent).
        MobSystem mobs;
        // The End's EXIT portal tracking set (the worldgen's origin
        // portal — the m_nether.portals twin; the choke point / the
        // rescan / the ambience read it).
        std::map<BlockPos, BlockID> portals;
        // V3-10d: the DRAGON's session state (all never saved — the
        // mob is ephemeral like every other, the egg's world-state is
        // the persistence marker; a no-dragon session keeps every field
        // at its default = the structural-immunity root).
        // `m_dragonSpawnDelay`: the first-visit spawn's frame countdown
        // (-1 = not scheduled; 0 = spawn this frame; the 60-frame
        // MC-1s read). `m_dragon`: the boss singleton (null = none
        // live — the m_ridingMount pointer pattern, the raw pointer is
        // valid until the ascension's removal frame).
        // `m_dragonHealFrame`: the 30-frame heal cadence's counter.
        // `m_dragonXpRemaining` / `m_dragonXpFrame`: the death ritual's
        // 200-XP burst scheduler (the 23-orb's every-5-frames release).
        int m_dragonSpawnDelay = -1;
        MobEntity* m_dragon = nullptr;
        Vec3 m_dragonDeathPos = Vec3::zero();
        int m_dragonHealFrame = 0;
        // V3-10d2: the RE-KILL's death count (session state — the
        // ritual's XP read: death #1 = the 200 burst, #2+ = the single
        // 8-value orb).
        int m_dragonDeaths = 0;
        // V3-10d2: the DEFEATED session flag (set on the first kill's
        // bookkeeping, never cleared — a reload resets it, the D6's
        // "no egg -> dragon back" reload read). The 10d1 spawn gate's
        // "no egg -> spawn" is the FIRST-VISIT read: after a defeat the
        // egg's absence is a BROKEN egg (the summon's precondition),
        // not a spawn signal — the gate stays closed until a summon
        // sequence completes (its explicit 60-frame delay). The gate-217
        // probe's catch: without this flag an egg break re-spawned the
        // dragon 60 frames later, un-summoned.
        bool m_dragonDefeated = false;
        // V3-10d2: the EGG respawn's deadline frame (the player-broken
        // egg's 60-s session timer — -1 = none pending; a reload loses
        // it, D7; the summon's removal clears it).
        int m_eggRespawnFrame = -1;
        // V3-10d2: the SUMMON sequence's countdown (-1 = disarmed;
        // > 0 = counting; the 4-face crystals' presence + no egg + no
        // dragon keep it running, any loss cancels).
        int m_dragonSummonCountdown = -1;
        // V3-10d2: the ritual's LAST orb's value (the bookkeeping's
        // set — the 10d1 first kill's 2, the re-kill's 8; 0 =
        // unreachable, the release only runs after a bookkeeping).
        int m_dragonXpLastValue = 0;
        // The heal beam's SOURCE slot (the dragon's nearest live crystal
        // this cadence window — the render's beam start; -1 = no beam
        // (full health / no crystal / no dragon)).
        int m_dragonHealSlot = -1;
        // The crystal census's previous frame (the crystalDelta feed's
        // change detection — -1 = never censused).
        int m_lastDragonCrystalCount = -1;
        // The dragon's last observed MobState (the transition E2E line's
        // change detection — -1 = never observed).
        int m_dragonLastState = -1;
        int m_dragonXpRemaining = 0;
        int m_dragonXpFrame = 0;
    };
    EndDim m_end;
    // V3-1 Phase B (D6): the post-travel portal cooldown in FRAMES
    // (kPortalCooldownFrames on a completed travel — the destination
    // inside-portal ping-pong guard; session state, never saved, the
    // fly-toggle pattern: a no-travel session keeps it 0).
    int m_portalCooldownFrames = 0;
    // V3-14d (strider riding): the ridden mount — SESSION state (never
    // saved: mobs are not persisted, so the ride cannot outlive the
    // session, the portal-cooldown / flight precedent). Non-null while
    // V3-10d: the ride mount moved into the PlayerSession (V3-12b's
    // per-player family): the rideable mob the player is carried; the
    // ride pass (updateRide) is the single choke point (the dismount
    // conditions + the carry snap + the steer push), the tryRide the
    // mount entry, the dismountRide the exit. Null in every no-ride
    // session (the structural-immunity root: the pass is one null
    // check, the tryRide never fires without an RMB on a rideable mob).
    // V3-10a: the End portal's entry countdown in FRAMES (the
    // m_portalEntryFrames' twin — the two dimensions' portals never
    // co-arm (the player is in one), so the states are separate; the
    // SHARED cooldown m_portalCooldownFrames gates both, the ping-pong
    // guard is dimension-agnostic). 0 = not counting (the structural-
    // immunity root: a no-end-portal session keeps it 0 forever).
    int m_endPortalEntryFrames = 0;
    // The player body family (UX-3): the player's world-space body —
    // the data model (data/player_body.json, parsed through MobRegistry::
    // parseModel's full strict discipline) + the render assets (the static
    // box mesh, the swing geometry, the per-frame dynamic mesh — all
    // built lazily, so a First-person run (the UX-3 constant) never
    // builds them = structural immunity) + the camera mode (First =
    // the body never draws; UX-4 adds the toggle). The phase itself
    // lives on the Player (m_swingPhase — render-only, never saved, the
    // mobs-ephemeral precedent). Consumer sites: game_init (the model
    // load + the atlas cross-check), game_entities (drawPlayerBody + the
    // lazy builders), game_render (the opaque-pass call after drawMobs).
    // All members are default-constructed with no cross-reads (the
    // construction-order red line).
    struct PlayerBody {
        // The parsed model section (boxes + swings; the width/height are
        // the RENDER bounding values — the player's collision AABB stays
        // Player::Params 0.6 x 1.8, untouched).
        MobModel model;
        bool modelLoaded = false;
        // The static box mesh (built lazily — never in a First run).
        Mesh mesh;
        // The swing geometry (the CPU base + parts; built lazily on the
        // first swing frame of a third-person run).
        SwingModel swingModel;
        // The per-frame dynamic mesh (one instance = the player; the
        // VBO is rewritten per swing frame, the IBO/VAO never again).
        Mesh swingMesh;
        // The frame-vertex scratch for buildSwingFrame (drawPlayerBody
        // is single-threaded on the GL thread — the mobs' shared-scratch
        // pattern, this body's own instance).
        std::vector<MeshVertex> swingFrameVerts;
        // V2-6: the first-person arm mesh (the 'arm_r' box re-centered
        // at the origin; built lazily on the first locked first-person
        // frame — a no-input run never builds it).
        Mesh armMesh;
    };
    PlayerBody m_playerBody;
    // UX-3: the camera mode (First = the body is invisible — the UX-3
    // constant; UX-4 adds the in-game toggle).
    CameraMode m_cameraMode = CameraMode::First;
    // V2-19: the third-person orbit distance (the mouse-wheel zoom):
    // SESSION state, never saved (the camera-mode precedent — the V2-1
    // "session state never saved" discipline). One value shared by
    // ThirdBack / ThirdFront (a C mode switch does NOT reset it), the
    // default kThirdPersonDistance keeps a no-input run bit-identical
    // (the structural-immunity root). The Game clamps it to
    // [kZoomMin, kZoomMax] in kZoomStep increments per wheel tick
    // (the Input's per-frame wheelDelta, reset in frameBegin — input
    // state, never a persisted level).
    float m_thirdPersonDistance = kThirdPersonDistance;
    // The player input-channel family (UX-2 B4): everything the pointer-
    // locked interaction maintains — the swing cooldown (Phase 20, the
    // attack input), the bow draw (Phase 24), the eat channel (Phase 28,
    // core/food.hpp) and the hold-to-break state (Phase 11) + the
    // dig-sound dedupe + the footstep timer (Phase 15). Consumer sites:
    // game_interaction (the unlock / escape / panel-mutex resets,
    // fireBow / updateBowDraw / resetBowDraw, updateEatChannel /
    // resetEatChannel, updateBreaking / updateAttacking), game_frame
    // (the update cadence) and game_render / game_ui (the held-hand pose,
    // the crack overlay and the step sounds). Collected so a NEW
    // interaction channel adds one member here and writes
    // m_interaction.xxx explicitly, instead of scattering a member
    // across the input bookkeeping. Member order = the original
    // declaration order of the family (construction order =
    // initialization order = behavior red line) — the break state + the
    // step timer move in from after the item/inventory red line block;
    // all eleven members are default-constructed with no cross-reads.
    // m_arrowShaftMesh / m_arrowTipMesh / m_arrowFletchMesh (the Phase 21
    // render assets) and the red line block (m_items / m_inventory /
    // m_selectedSlot / m_recipes) stay OUT.
    struct InteractionState {
        // Swing cooldown (seconds; Phase 20): one attack hit per
        // kSwingInterval while the left button is held on a mob. Reset to
        // 0 on release / when the aim leaves the mob (the next swing is
        // immediate).
        double swingCooldown = 0.0;
        // Bow draw (Phase 24): true while the right button is held with a
        // launcher selected; bowDrawSeconds accumulates the draw in real
        // dt (capped at kBowFullDrawSeconds). Reset by fireBow/
        // resetBowDraw and by every state change that interrupts a draw
        // (unlock, panel, item switch).
        bool bowDrawing = false;
        double bowDrawSeconds = 0.0;
        // Eat channel (Phase 28, core/food.hpp): the Minecraft-style 1.6 s
        // eat animation — right-clicking with food selected starts the
        // channel (eatSlot / eatItem capture the slot and item; completion
        // consumes one and restores hunger + saturation). Interrupted by
        // damage, death, unlock, panel, selection change, early release,
        // or a left-button swing (none of these consume the food).
        EatChannel eatChannel;
        std::size_t eatSlot = 0;
        ItemID eatItem = ItemID::invalid();
        float eatStartHealth = -1.0f; // damage during the channel cancels it
        // Hold-to-break state (Phase 11): the cell being broken and how
        // long it has been held (seconds). Reset on target change,
        // pointer unlock, selection change, and completion.
        std::optional<BlockPos> breakTarget;
        double breakProgress = 0.0;
        // Last progress value at which a dig sound played (Phase 15).
        double lastDigProgress = 0.0;
        // Footstep timer (seconds since the last step sound; Phase 15).
        double stepTimer = 0.0;
    };
    // The three shared arrow-part meshes (Phase 21; PRE-9 three-part:
    // shaft/tip/fletching; built lazily on first render) + the V3-14b
    // fireball's glow cube (the exploding shooter's projectile — built
    // lazily on first render, the arrow parts' twin).
    Mesh m_arrowShaftMesh;
    Mesh m_arrowTipMesh;
    Mesh m_arrowFletchMesh;
    Mesh m_fireballMesh;

    // Item/inventory system (Phase 11): item registry (implicit block
    // items + explicit tools/materials from assets/data/items), the
    // player's inventory (hotbar = first Inventory::kHotbarSize slots,
    // starting contents from assets/data/hotbar.json) and the selected
    // hotbar slot. Crafting recipes (Phase 12) are loaded and validated at
    // startup; the in-game crafting UI/trigger is Phase 15.
    ItemRegistry m_items;
    RecipeRegistry m_recipes;
    // V2-12 (format 27): the enchantment registry (loaded from assets/data/
    // enchantments/*.json at init — the ItemRegistry / RecipeRegistry
    // pattern). The offer roll, the panel, and the save cross-check read
    // it; the content is static after load.
    EnchantmentRegistry m_enchants;

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
    // V2-6: the blob-shadow unit quad (built lazily on the first frame
    // that has a shadow to draw — a no-input run never builds it).
    Mesh m_mobShadowMesh;
    // V3-10d: the dragon FX's unit box (the crystal->dragon heal beam's
    // column + the breath cloud's flat disc share it — built lazily on
    // the first frame that has a dragon FX to draw, a no-dragon session
    // never builds it = the structural immunity, the m_mobShadowMesh's
    // lazy twin).
    Mesh m_dragonFxMesh;
    // V2-11: the solid orb-cube mesh (built lazily on the first frame
    // that has an orb to draw — a no-input run has zero orbs, so it
    // never builds the mesh = the structural immunity).
    Mesh m_expOrbMesh;
    std::array<Mesh, 5> m_crackMeshes;
    // Phase 15 state (all default-off: the no-input baseline renders no
    // UI at all, keeping the Phase 14 screenshots byte-identical).
    bool m_debugOverlay = false;

    // The panel family (UX-2 B4): the Phase 17 E panel's persistent
    // crafting grid (its 2x2 uses the top-left 4 cells of the 9-cell
    // model; the grid survives a close/reopen — see docs/KNOWN_ISSUES —
    // and is saved, format 3) + the panel UIs (the inventory / table
    // pick-and-place state machines, Phase 15/17, and the furnace UI,
    // Phase 40) + the open-panel state machine + the CRAFT-7 double-click
    // collect memory + the Phase 40/44 furnace panel's bound position.
    // Consumer sites: game_ui (the panel rendering, the click routing and
    // the open / close transitions), game_interaction (the panel-mutex
    // resets) and game_save (the grid collect / apply). Collected so a
    // NEW panel feature adds one member here and writes m_panel.xxx
    // explicitly, instead of scattering a member across the UI
    // bookkeeping. Member order = the original declaration order of the
    // family (construction order = initialization order = behavior red
    // line) — openFurnacePos / furnaceUi move in from after the
    // PerBlockStates family (B1): all three UIs take their references in
    // the PanelState constructor (after m_items / m_inventory /
    // m_recipes, declared above) and the E panel's grid binding stays in
    // the Game constructor body (after every member initialization).
    // CRAFT-1: the crafting TABLE's grid is no longer a single global —
    // it is per-block (one grid per crafting_table block, the
    // m_states.tables map below).
    struct PanelState {
        CraftingGrid inventoryGrid;
        InventoryUI inventoryUi;
        InventoryUI tableUi;
        OpenPanel openPanel = OpenPanel::None;
        // CRAFT-7: the last left click on a PLAYER-INVENTORY slot (for
        // the double-click collect): the slot index (-1 = none) and the
        // real-dt timestamp (seconds, -1 = none). Input-only state (the
        // survival-timer class, hard rule 8) — a no-input run never
        // touches it.
        int lastLeftSlot = -1;
        double lastLeftClickTime = -1.0;
        // Phase 40/44 furnace panel: openFurnacePos is valid while
        // openPanel == OpenPanel::Furnace (the panel's UI is bound to its
        // state).
        BlockPos openFurnacePos {};
        FurnaceUI furnaceUi;
        // V2-12 enchanting panel: openEnchantPos is valid while
        // openPanel == OpenPanel::Enchant. The item / lapis slots are
        // SESSION state (not per-block — the offers are rolled on demand
        // and the slots are returned to the inventory on close), and the
        // offers are re-derived when the item slot changes. The UI is
        // bound to the two slots on open.
        BlockPos openEnchantPos {};
        ItemStack enchantItemSlot;
        ItemStack enchantLapisSlot;
        EnchantOffers enchantOffers;
        EnchantUI enchantUi;
        // V3-11b: the creative's E-panel face (the 物品栏 tab): the
        // search / scroll / grant UI + the active tab (0 = 物品栏,
        // 1 = 背包). SESSION state — reset on every creative open,
        // never saved (the enchant slots' precedent). The survival's
        // E panel ignores both (the creative-only render/click gates).
        CreativeTabUI creativeUi;
        int creativeTab = 1;
        PanelState(Inventory& inventory, const ItemRegistry& items,
                   const RecipeRegistry& recipes)
            : inventoryUi(inventory, items, recipes),
              tableUi(inventory, items, recipes),
              furnaceUi(inventory, items),
              enchantUi(inventory, items),
              creativeUi(inventory, items) {}
    };

    // V3-13: the session stats (the death screen's panel — SESSION
    // ONLY, not serialized, the documented deviation: the counters
    // reset with the process, Minecraft's scoreboard persists).
    // V3-12b: the per-player definition (the PlayerSession's absorbed
    // member's type — the struct moved here for the declaration order).
    struct SessionStats {
        int blocksMined = 0;
        int blocksPlaced = 0;
        int mobsKilled = 0;
    };

    // V3-12b: the PER-PLAYER session state (design §3.1's migration
    // list): the inventory + selected slot, the input-channel family
    // (InteractionState), the local panel (bound to THIS session's
    // inventory — the remote player's panel is disabled in 12b, D11),
    // the survival timers (hurt flash / the prev-health hurt event),
    // the drowning state, the status effects, the pearl-throw cooldown,
    // the ride mount, the session stats, the tutorial state + the
    // double-tap fly window's last-Space timestamp (R2's simTime keeps
    // the real-dt value — the solo path's bit-level), and the R18
    // pointer-lock SIM mirror (solo: synced from the SDL state at every
    // sim read site = the original code's value; net: carried in the
    // FrameState — the local fact enters the simulation through it).
    struct PlayerSession {
        Inventory inventory;
        std::size_t selectedSlot = 0;
        InteractionState interaction;
        PanelState panel;
        StatusEffects effects;
        DrownState drown;
        double hurtFlash = 0.0; // seconds remaining of the red flash
        float prevHealth = -1.0f; // -1 = first frame (no hurt event)
        int pearlThrowCooldownFrames = 0;
        MobEntity* ridingMount = nullptr;
        SessionStats sessionStats;
        // SURV-7: the per-player death-related statistics (format 21):
        // "deathCount" + one "kill.<mob name>" per registered mob. Only
        // NON-ZERO counters are held (the save omits zeros — the
        // omit-when-default discipline). Frame-locked events (the death
        // transition, the mob despawn pass) increment them; deterministic.
        // V3-12b: per-player (the death count is the player's own; the
        // mob-death counters anchor the PRIMARY session — the world
        // event's D10-style primary read, the tutorial anchor's twin).
        std::map<std::string, std::uint64_t> stats;
        TutorialState tutorial;
        bool tutorialEnabled = true;
        int tutorialToastFrames = 0;
        double lastSpacePressTime = -1.0;
        bool inputLocked = false; // the R18 mirror (see above)
        // The member order = the original declaration order of the
        // family it absorbed (the construction-order red line, the
        // InteractionState comment's discipline): inventory first
        // (the panel binds to it), the interaction family next.
        PlayerSession(const ItemRegistry& items, const RecipeRegistry& recipes)
            : panel(inventory, items, recipes) {
            // The Game constructor's original bind call (the crafting
            // grid -> the inventory UI's slot mapping), now per-session.
            panel.inventoryUi.bind(panel.inventoryGrid);
        }
    };

    std::vector<std::unique_ptr<PlayerSession>> m_sessions;
    // V3-12b: the session family's factory (the constructor's init list
    // cannot push_back — the reference aliases below bind into
    // m_sessions[0] at init, before the body runs): one primary session.
    static std::vector<std::unique_ptr<PlayerSession>> makeSessions(
        const ItemRegistry& items, const RecipeRegistry& recipes) {
        std::vector<std::unique_ptr<PlayerSession>> sessions;
        sessions.push_back(std::make_unique<PlayerSession>(items, recipes));
        return sessions;
    }
    // The PRIMARY (index 0) reference aliases: every pre-V3-12b member
    // use keeps compiling and — in solo, where primary == local == the
    // only session — stays bit-identical. The ×N sim passes (design
    // §3.1's looped table) index m_sessions[i] explicitly instead; the
    // local UI (the render / HUD / death screen) reads the local
    // session through localSession() / localPanel().
    Inventory& m_inventory;
    std::size_t& m_selectedSlot;
    InteractionState& m_interaction;
    StatusEffects& m_playerEffects;
    DrownState& m_drown;
    double& m_hurtFlash;
    float& m_prevHealth;
    int& m_pearlThrowCooldownFrames;
    MobEntity*& m_ridingMount;
    SessionStats& m_sessionStats;
    std::map<std::string, std::uint64_t>& m_stats;
    TutorialState& m_tutorial;
    bool& m_tutorialEnabled;
    int& m_tutorialToastFrames;
    double& m_lastSpacePressTime;
    PlayerSession& localSession() const {
        return *m_sessions[static_cast<std::size_t>(m_localSlot)];
    }
    PanelState& localPanel() const {
        return localSession().panel;
    }
    bool panelOpen() const {
        return localPanel().openPanel != OpenPanel::None;
    }
    // V3-12b: the ×N pass's per-player forms (declared after the
    // PlayerSession type — the nested-class parameter-order rule).
    // inputFor = the per-player SIM input source (solo: the live pump,
    // the pre-V3-12b value; net: the N−D reconstructed frame state —
    // wired in the loop step). dismountRideFor / syncAbsorptionFor =
    // the per-player ride-exit + absorption-sync (the alias methods
    // above delegate the primary to them).
    const Input& inputFor(std::size_t i) const;
    void dismountRideFor(Player& p, PlayerSession& s);

    // Phase 83 beacon: the (x, y, z) of a beacon block plus its power
    // level (the beam is taller at higher levels, Minecraft). Filled by
    // updateBeacons() (this frame's active beacons, level >= 1) and read
    // by drawBeaconBeams(). UX-2 B1: a TYPE only (no construction-order
    // effect) — declared before PerBlockStates because its beaconActive
    // member is a std::vector of it, and a nested type is visible to a
    // nested struct only when declared first.
    struct BeaconBeam {
        int x, y, z, level;
    };

    // The per-block world-state family (UX-2 B1): everything maintained
    // at the setBlock choke point (the bookkeeping maps) consumed by the
    // update ticks + the unload protectStates + the collectSave/applySave
    // save passes + the rescan* load rebuilds — the five consumer sites
    // (the A4 assembly point). Collected so a NEW per-block system adds
    // one member here and writes m_states.xxx explicitly, instead of
    // scattering a member across the nine setBlock bookkeeping lines and
    // five call sites. Member order = the original declaration order of
    // the family (construction order = initialization order = behavior
    // red line). The furnace panel's bound position + UI (openFurnacePos
    // / furnaceUi) moved to the panel family (UX-2 B4, m_panel).
    struct PerBlockStates {
        // CRAFT-1: the per-block crafting-table grids — one CraftingGrid
        // per crafting_table block in the world (created on placement,
        // empty; dropped when the cell stops holding a table — the
        // container's items are lost, the furnace-break semantics; the
        // invariant the format-19 save cross-checks against the block
        // edits, exactly like the furnace states). std::map:
        // position-sorted, so the save's write order is deterministic.
        // The table panel's UI is bound to the clicked block's grid on
        // every open (openTablePanel).
        std::map<BlockPos, CraftingGrid> tables;
        // Phase 40/44 furnace: PER-BLOCK smelting states — one FurnaceState
        // per furnace block in the world (created on placement, dropped
        // when the cell stops holding a furnace; the invariant the
        // format-7 save cross-checks). std::map: position-sorted, so the
        // per-frame ticks iterate deterministically.
        std::map<BlockPos, FurnaceState> furnaces;
        // Phase 64 fire: the remaining real-dt life of EVERY burning fire
        // block in the world (created on placement/burn, dropped when the
        // cell stops holding a fire; the invariant the format-10 save
        // cross-checks). std::map: position-sorted, so the per-frame
        // burn-out countdown iterates deterministically. fireId is the
        // registry id of the "fire" block (the burn target; the init
        // cross-check guarantees it whenever a flamable block exists);
        // fireLifetime is the config-driven burn-out duration
        // (fire.lifetime_seconds, default 4).
        std::map<BlockPos, double> fires;
        BlockID fireId = BlockID::invalid();
        double fireLifetime = 4.0;
        // Phase 66 fire spread + embers (Fire v2): the spread roll cadence
        // (fire.spread_interval_frames, default 6 = the 0.1 s Minecraft
        // tick rhythm) and per-neighbor chance (fire.spread_chance,
        // default 0.05 = the Minecraft normal value); fireParticles gates
        // the ember emission (fire.particles, default true — a no-input
        // run without fires is inert either way, and it is the E2E
        // control).
        int fireSpreadInterval = 6;
        double fireSpreadChance = 0.05;
        bool fireParticles = true;
        // V3-15: the portal's ambient steam particles (the fire.
        // particles' twin gate — absent = the code default true).
        bool portalSteam = true;
        // CRAFT-5: the lit-furnace mouth flames (render.furnace_flames,
        // default true — off = zero emissions = bit-identical to v1.21.0;
        // the fire.particles E2E-control pattern).
        bool furnaceFlames = true;
        // PRE-8: the burning-mob flames (render.mob_flames, default true
        // — off = zero emissions = bit-identical to v1.28.0; the
        // render.furnace_flames E2E-control pattern).
        bool mobFlames = true;
        // Phase 71 campfire (the family in Phase 75): PER-BLOCK cooking
        // states — one CampfireCookState per campfire-FAMILY block in the
        // world (lit or extinguished: water puts the lit one out into the
        // extinguished block, which KEEPS its state — the items stay on
        // the unlit logs; created on placement, dropped when the cell
        // stops holding a family block; the invariant the format-13 save
        // cross-checks, exactly like the furnace states). std::map:
        // position-sorted, so the per-frame ticks iterate
        // deterministically.
        std::map<BlockPos, CampfireCookState> campfires;
        // Phase 75: the campfire family ids (init-resolved; invalid = the
        // family-dependent paths are structurally inert, the P70 pattern).
        BlockID campfireLitId = BlockID::invalid();
        BlockID campfireExtId = BlockID::invalid();
        // Phase 48 crop growth: the cells holding a block with a
        // non-empty growsTo property (the growth stage is the block id
        // itself — no per-instance state, no save field). Maintained
        // through the setBlock choke point (like the furnace states) and
        // rebuilt from the world by rescanCrops() after load. std::map:
        // position-sorted, so the per-frame growth ticks iterate
        // deterministically.
        std::map<BlockPos, BlockID> crops;
        // GEN-2 tree growth: the cells holding a SAPLING (a block with a
        // `tree` object — the growth P48 pattern, the crops twin:
        // maintained through the setBlock choke point, rebuilt by
        // rescanSaplings() after load). The GROWTH STAGE is NOT stored
        // here — it is the cell's per-instance state (ENT-2, the
        // format-15 `states` save segment); this set only tracks WHICH
        // cells roll. std::map: position-sorted, so the per-frame
        // sapling ticks iterate deterministically.
        std::map<BlockPos, BlockID> saplings;
        // GEN-9 farmland wetness: the cells holding a block with a
        // model.moisture_top variant (the farmland, the crop-growth crops
        // / saplings tracking pattern). The value is the LAST computed
        // wet state (water within Chebyshev kWetWaterRadius at the
        // cell's y, the soilIsWet predicate) — refreshMoistureStates()
        // re-evaluates on a frame-locked cadence and marks the cell's
        // chunk dirty ONLY when the wetness flipped (a settled farm
        // re-meshes nothing; the fresh world has no farmland and the
        // sweep fast-exits on the empty map). The WETNESS ITSELF is
        // derived at mesh time from the live fluid field (the mesher's
        // moisture hook) — this map is only the change detector + the
        // re-mesh trigger (the sweep fixes the worker-mesh staleness: a
        // farmland chunk meshed before its water neighbor streamed in
        // bakes dry until the next sweep). std::map: position-sorted, so
        // the sweep iterates deterministically.
        std::map<BlockPos, bool> moistureBlocks;
        // GEN-2: the RESOLVED tree specs (the sapling's `tree` names ->
        // BlockIDs, the init-resolved campfireLitId pattern — the
        // registry load validates the names exist, so this map is the
        // placement's fast path). Keyed by the sapling's block id; empty
        // = no sapling content (the structurally-inert pattern).
        std::map<BlockID, TreeSpec> treeSpecs;
        // Phase 83 beacon: the cells holding a block with a non-empty
        // beaconEffect property (a beacon — the growth P48 pattern:
        // maintained through the setBlock choke point, rebuilt by
        // rescanBeacons() after load). The power level is NOT stored
        // here — it is re-derived from the world every frame
        // (world/beacon), so there is no save-format field. std::map:
        // position-sorted, so the per-frame pass iterates
        // deterministically.
        std::map<BlockPos, BlockID> beacons;
        // Phase 83 beacon: this frame's ACTIVE beacons (level >= 1),
        // filled by updateBeacons() and read by drawBeaconBeams().
        std::vector<BeaconBeam> beaconActive;
        // Phase 83 beacon config: the effect range in blocks
        // (beacon.range_blocks, default 50, >0 — a player within
        // Chebyshev distance <= range of the beacon cell receives the
        // effect) and the beam visibility gate (beacon.beam, default true
        // — a no-input run without beacons is inert either way, the E2E
        // control the fire.particles pattern).
        int beaconRange = 50;
        bool beaconBeam = true;
        // Phase 83 beacon: the shared translucent beam column mesh (a thin
        // box from y=0 to y=1, scaled + positioned per beacon by the model
        // matrix) and its resolved effect id (init; -1 = no beacon effect,
        // the path is structurally inert — the m_fireEffectId pattern).
        Mesh beaconBeamMesh;
        int beaconEffectId = -1;
        // V3-1 Phase A nether portal: the frame material's resolved id
        // (init name lookup, the campfireLitId pattern — invalid when the
        // asset set has no obsidian block, then the whole portal system
        // is structurally inert: no frame can ever be complete) and the
        // portal block's own id (invalid when the nether_portal block is
        // absent — the P70 fire/campfire id pattern).
        BlockID obsidianId = BlockID::invalid();
        BlockID portalId = BlockID::invalid();
        // V3-1 Phase B: the nether's generation content ids (the
        // obsidianId pattern — init name lookup; invalid = the nether's
        // fill is unbuildable, ensureNether MC_FAILs at the first need,
        // the P70 family-inert twin): the bedrock (the floor / roof bands
        // AND the generated-portal's platform — the design's bedrock
        // ruling). The netherrack + the V3-2 Part 1 content family are
        // resolved by NetherGen::create at engagement (the nether
        // module's data vocabulary — the WorldGenerator::create twin).
        BlockID bedrockId = BlockID::invalid();
        // V3-10a: the End portal's resolved ids (the portalId's twins —
        // init name lookup; invalid = the End portal system is
        // structurally inert, the nether's absent-asset twin).
        BlockID endPortalId = BlockID::invalid();
        // The eyed frame's resolved id (the embed interaction's write
        // target — the frame cell becomes THIS on an eye placement).
        BlockID endFrameEyedId = BlockID::invalid();
        // V3-10d: the dragon arena's resolved ids (the endPortalId's
        // twins — init name lookup; invalid = the dragon system is
        // structurally inert, the ensureEnd MC_FAILs at the first need):
        // the crystal (the tower/exit worldgen + the break-explosion's
        // data read) + the egg (the death ritual's world-state marker).
        BlockID endCrystalId = BlockID::invalid();
        BlockID dragonEggId = BlockID::invalid();
        // V3-1 Phase A: the portal tracking set — the cells holding the
        // portal block (the beacons' twin: maintained through the setBlock
        // choke point, rebuilt by rescanPortals() after load). Consumed by
        // the ambient pass (the per-frame roll over the visible portals).
        // The power / validity is NOT stored — the frame validity is
        // re-derived by the scan when needed (the beacon's derived-level
        // twin). std::map: position-sorted, deterministic iteration.
        std::map<BlockPos, BlockID> portals;
        // V3-10a: the OVERWORLD's end-portal cell tracking set (the
        // portals' twin — the entry frames live in the overworld; the
        // END's exit set is m_end.portals, the nether's split precedent).
        std::map<BlockPos, BlockID> endPortals;
    };
    PerBlockStates m_states;

    // UX-9a clouds: the ONE-TIME kCloudQuadSize world-space quad (built
    // at init, uploaded once — the PATTERN is computed per pixel by
    // m_cloudShader, an exact GLSL port of the world/cloud field, so
    // nothing is re-uploaded after init: the old per-camera-window mesh
    // rebuild cost the iGPU ~140 ms per MB of buffer update on every
    // 16-block crossing, and its window edge popped in) + its dedicated
    // shader program (independent of the basic/sky/ui programs — they
    // are untouched). render.clouds OFF never touches any of this (the
    // v0.27.0 pipeline bit-identical).
    Mesh m_cloudMesh;
    ShaderProgram m_cloudShader;

    // V2-4 weather (rain/thunder): everything the session-derived weather
    // system maintains (NOT saved — zero format, the cloud-drift
    // precedent class; every run restarts the machine, and the first
    // segment-CLEAR >= kClearMin is the 60f no-input baseline's structural
    // immunity). The pure state machine + presentation constants live in
    // core/weather (the unit tests own them — the campfire pattern).
    // Consumer sites: game_init (the render.weather strict read + the
    // state init from the world seed + the --weather override + the rain
    // program/mesh build), game_frame (updateWeather — the frame-locked
    // step + the verbatim log lines + the thunder sound, and the raining
    // reads in the fire/campfire put-out passes + the crop growth pace),
    // game_render (the weatherDayFactor sky-channel convergence + the
    // drawRain blend pass), game_ui (the player fire-effect rain
    // extinguish + the F3 weather line), game_entities (the mobs'
    // setRaining feed). Member order = append after the cloud family
    // (all default-constructed, no cross-reads — the construction-order
    // red line).
    WeatherState m_weather;
    // render.weather (strict read at init, code default true — the
    // committed config.json carries no such key). OFF = the machine never
    // runs + zero drops + zero interaction = the v2.3.0 pipeline bit-
    // identical (the gate's "new-off" control).
    bool m_weatherEnabled = true;
    // The two world-seed streams (same world, same weather sequence
    // across runs): the main CLEAR/RAIN duration stream (seed = the world
    // seed) and the dedicated-salt THUNDER stream (seed = world seed +
    // kSaltWeatherThunder).
    std::uint64_t m_weatherStream = 0;
    std::uint64_t m_weatherThunderStream = 0;
    // The last thunder event's frame (-1 = none): the sky pulse's
    // presentation state (thunderPulse, the 5-frame linear decay — read
    // by weatherDayFactor; never saved, the render-only class).
    int m_weatherThunderFrame = -1;
    // The rain render assets (the cloud program's fourth precedent — a
    // dedicated shader program + a STATIC kRainDropCount-drop mesh
    // uploaded once at init, zero per-frame upload: the vertex shader
    // re-derives each drop's position from the frame-bounded fall offset
    // + the player-anchored box origin). render.weather OFF never draws
    // any of this (the v2.3.0 pipeline bit-identical).
    Mesh m_rainMesh;
    ShaderProgram m_rainShader;

    // SAVE-1: the auto-save cadence (save.auto_save_seconds — a CODE
    // default; the committed config carries no save section, the
    // fire/mobs/beacon config-key discipline) + the real-dt accumulator
    // (NOT frame state, hard rule 8: never saved, never affects the
    // deterministic frame — the audio-timer class). 0 = off (the
    // pre-SAVE-1 behavior); without a --save slot the timer is
    // structurally inert (updateAutoSave's gate).
    double m_autoSaveInterval = kAutoSaveDefaultSeconds;
    double m_autoSaveAccum = 0.0;

    // V3-7: the save's on-disk encoding (save.compression — strict read
    // at init, absent = Gzip the code default, the user decision 11-04;
    // the committed config carries no save section). Gzip = the
    // deterministic gzip container (the format-29 era's write default);
    // None = the plaintext payload (the pre-V3-7 behavior,
    // bit-identical). Consumed by writeSave (the encode gate) — the
    // read side is encoding-agnostic (the magic probe, zero migration).
    SaveCompression m_saveCompression = SaveCompression::Gzip;

    // UX-1: the configured deep-night sky floor (render.night_sky_level —
    // a CODE default kNightSkyLevel; the fire/mobs/beacon config-key
    // discipline: the committed config.json carries no such key). Strict
    // read at init (absent = 7 = the v1.34.0 pipeline bit-for-bit; 0 =
    // the all-black night, the ambient floor goes with it). Drives the
    // basic shader's u_nightRetreat / u_ambient uniforms (set once at
    // init — per-run constants, no per-draw variation) and the hostile
    // spawn light gate (findMobSpawnCell).
    int m_nightSkyLevel = kNightSkyLevel;

    // V2-18 AO: render.ao (strict read at init, code default true — the
    // weatherEnabled / readNightSkyLevel discipline: the committed
    // config.json carries no such key). The mesher-construction gate:
    // OFF = the AO factor is constant 1.0 + the mask's ao[4] stays zero
    // = the pre-V2-19 mesh bit-identical (the gate's "new-off" control;
    // the shader / vertex format / per-draw light overrides are untouched
    // either way). Consumed at the single worker-mesh construction site
    // (postMeshJobs — the mesh() call census).
    bool m_aoEnabled = true;

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
    // Phase 46 status effects: the data-driven definitions
    // (assets/data/effects) + the player's active instances (core/
    // status_effect.hpp) + the 16 px icon strip (assets/textures/
    // effects.png; the effect's 'icon' field indexes its tile). The game
    // owns the instances and their real-dt tick; the HUD draws one icon
    // per active effect (left-aligned on the heart row).
    EffectRegistry m_effects;
    // UX-7 / V2-3: the tutorial SERIES (the objective-style onboarding)
    // — the step data (assets/data/tutorials/*.json, one file per
    // series, file-name byte order = the series order; strict-parsed at
    // init + the goal cross-checks + the global step-id uniqueness:
    // content lives in the JSON, the core reads it — hard rule 3), the
    // player's frame-locked progress (format 25: the seriesIndex +
    // stepIndex + progress), the enable flag (config tutorial.enabled,
    // code default true — the render.fog getBool precedent; OFF = zero
    // state-machine execution + zero UI + the skip key no-op, the save
    // still writes the segment = the key discipline) and the
    // completion-toast countdown (the frame-locked kTutorialToastFrames
    // window, armed by the done flip ONLY — the series transition and
    // the T series-skip carry no toast; render-only — never saved, the
    // camera-mode session-state class). The event emission is the
    // single choke point emitTutorialEvent (game_frame) over the pure
    // state machine (core/tutorial, the unit tests own it).
    std::vector<std::vector<TutorialStep>> m_tutorialSeries;
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
    // V3-1 Phase A: the portal entry countdown (the frames remaining of
    // kPortalEntryFrames; 0 = unarmed — session state, never saved, the
    // fly-toggle pattern: a no-input run without portals keeps it 0).
    int m_portalEntryFrames = 0;
    double m_fpsSmoothed = 0.0;
    double m_lastTitleUpdate = 0.0;
    bool m_initialized = false;

    // V3-9 (demo replay / record / author): the family's state, APPENDED
    // last (the existing members' declaration order is untouched — the
    // family is new ground). Three CLI flags absent = m_demoMode Off =
    // the bit-identical normal run: m_audienceInput is constructed but
    // NEVER pumped (the SDL queue's sole consumer stays m_input), no
    // injection (m_demoPlayer empty), no tap (m_input's event tap is
    // null), no pacing, no UI (every render gate reads m_demoMode),
    // syncCamera's replay branch never runs (the structural immunity
    // root).
    Input m_audienceInput; // the REPLAY's SDL-queue consumer (the
                           // audience channel — separate from the
                           // player's m_input, the routing boundary)
    enum class DemoMode { Off, Replay, Record };
    DemoMode m_demoMode = DemoMode::Off;
    // Replay (--demo): the injection kernel (the demo's dts + events)
    // + the playback state. The wall-clock fields are PACING ONLY —
    // never the simulation (the demo's dts are the time source; a
    // slow machine drops render frames, the simulation does not chase
    // — the documented deviation).
    std::optional<DemoPlayer> m_demoPlayer;
    std::string m_demoPath; // the .dmo's path (the log + the panel)
    int m_demoFrame = 0;    // the demo frame index (0-based)
    double m_demoSpeed = kDemoSpeedDefault;
    bool m_demoPaused = false; // the ESC pause menu (NOT the exit)
    bool m_demoEnded = false;  // the sequence finished
    bool m_demoEndPanelOpen = true; // the finished panel (ESC = keep
                                    // observing the still last frame)
    double m_demoPacingAnchorWall = 0.0; // the re-anchor's wall time
    std::int64_t m_demoPacingAnchorUs = 0; // the demo-time us at the
                                           // anchor (the int64 drift
                                           // guard)
    std::int64_t m_demoTimeUs = 0; // the completed demo frames' us
                                   // (the llround(dt x 1e6) sum)
    // The audience camera's orbit (render-only — syncCamera re-resolves
    // around the eye; never saved, never touches the simulation; the
    // aim rays read aimForward, not the orbit — the red line).
    float m_demoOrbitYaw = 0.0f;
    float m_demoOrbitPitch = 0.0f;
    // Record (--demo-record): the boundary-tap kernel (the Input's
    // event tap closure; the world spec set in init).
    DemoRecorder m_demoRecorder;

    // -- V3-12b: the NET session (the 2-player lockstep PoC) ------------
    // The transport (one TCP connection; the host owns the session —
    // the client's EOF is the D2's all-exit, the host's EOF is the
    // client's removal + the solo continuation). nullptr = no net
    // session (the structural-immunity root: every net hook gates on
    // m_netActive, the solo loop is the pre-V3-12b sequence).
    // The transport (host: one connection per remote player; client:
    // the single host connection). nullptr/empty = no net session (the
    // structural-immunity root: every net hook gates on m_netActive,
    // the solo loop is the pre-V3-12b sequence).
    std::vector<std::unique_ptr<NetSession>> m_nets;
    bool m_netActive = false;
    bool m_netIsHost = false;
    bool netActive() const { return m_netActive; }
    // The client's WELCOME seed (the world spec's authority — the
    // config's world.seed override before buildScene; -1 = none).
    std::int64_t m_netSeedOverride = -1;
    // The per-player FrameState rings (the depth D+1 — the frame N's
    // read is (N-D) % (D+1); the unified delay rule, design D3's
    // no-prediction: the local player's own frame is relayed like the
    // remote's, no local fast-path). Sized by the player count (the
    // c1 N-generalization — the 12b fixed pair's shape generalizes to
    // the protocol's kNetMaxPlayers).
    std::vector<std::vector<FrameState>> m_frameRings;
    // The per-player SIM Input (the preloadFrame's target — the
    // inputFor's net branch; the solo's live m_input stays the sim
    // source in the no-net run).
    std::vector<Input> m_netInputs;
    // The host's own frame hashes (the client's hash compare — the
    // INPUT's carried hash vs the host's computed-for-that-frame, the
    // ring depth = the hash interval).
    std::array<std::uint64_t, kNetHashInterval> m_hostHashRing {};
    // The host's lockstep barrier's watermarks (the highest frame whose
    // INPUT has arrived, per REMOTE slot — the host's pre(N) blocks
    // until every remote's INPUT[N-D] lands, the mutual barrier's host
    // half: a fast host eating a remote's not-yet-sent input = the
    // silent-desync root the barrier's existence).
    std::vector<std::uint32_t> m_remoteFrameMax;
    // V3-12c1 (the lobby): the per-slot HELLO names (<=12 Latin — the
    // nameplate's + the F3 player line's carrier; the 12b's "client"
    // default for slot 1's shape generalizes).
    std::vector<std::string> m_playerNames;
    // The c1's chat's overlay (the 8-line ring — the render's face;
    // the frame stamp's fade). Presentation only (the hash's
    // exclusion by construction).
    struct ChatLine {
        int frame = 0;
        std::string who;
        std::string text;
    };
    std::vector<ChatLine> m_chatLines;
    // The chat's INPUT state (the T toggle's local face — the live
    // pump, never the sim: the chat is presentation, the hash's
    // exclusion by construction).
    bool m_chatInputActive = false;
    std::string m_chatBuffer;
    // --script: the timeline's injection kernel (the demo player's
    // shape, the dts ignored — the net's dt is the broadcast's).
    std::optional<DemoPlayer> m_scriptPlayer;
    bool m_scriptActive = false;
    // The client's pending host hash (the FRAME's carried value for
    // this frame — the netFramePost's compare after the sim).
    std::uint64_t m_pendingHostHash = 0;
    std::uint32_t m_pendingHostHashFrame = ~0u;

    // The net hooks (init's handshake + the loop's pre/post frame).
    Result<void> netClientHandshake();
    Result<void> netHostHandshake();
    // The frame's net face: the poll / the capture / the preload / the
    // send + the hash exchange (the error string = the fatal's named
    // face, the loop's break signal). dt IN/OUT = the host's wall dt
    // (the broadcast's source) / the client's received dt (the sim's
    // ONLY time source).
    std::string netFramePre(double& dt);
    std::string netFramePost(double dt, std::size_t F);
    // The c1's per-remote consumption (the poll + the INPUT's file +
    // the CHAT's relay — the barrier's select's fan-in face).
    void netConsumeRemote(std::size_t r);
    // The c1's chat's send / display (the presentation face — the
    // hash's exclusion by construction).
    void netSendChat(const std::string& text);
    void displayChat(int slot, const std::string& text);
    // The session-end's send tolerance (the final frame's broken pipe
    // = the peer's clean exit, the gate's --frames face).
    bool finalFrame(std::size_t F) const;
    FrameState captureLocalFrameState() const;
    void preloadNetInputs();
    // The player add / remove (the connect's slot 1 / the disconnect's
    // R19 removal — the 100% drop + the dismount + the effect clear).
    void netAddPlayer(std::size_t slot);
    void netRemovePlayer(std::size_t slot, const std::string& why);
    // The state-hash's exchange (the 30-frame cadence's compute +
    // compare — the desync's named exit).
    std::uint64_t netComputeHash();
};

} // namespace mc
