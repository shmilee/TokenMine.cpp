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
#include <vector>

#include "block/block_registry.hpp"
#include "core/asset_manager.hpp"
#include "core/camera.hpp"
#include "core/config.hpp"
#include "core/difficulty.hpp"
#include "core/error.hpp"
#include "core/input.hpp"
#include "core/logger.hpp"
#include "core/save.hpp"
#include "core/sound.hpp"
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
#include "item/crafting_grid.hpp"
#include "item/inventory.hpp"
#include "item/item_registry.hpp"
#include "item/recipe.hpp"
#include "player/player.hpp"
#include "renderer/mesh.hpp"
#include "renderer/renderer.hpp"
#include "renderer/shader.hpp"
#include "renderer/texture.hpp"
#include "renderer/texture_atlas.hpp"
#include "world/biome.hpp"
#include "world/chunk.hpp"
#include "world/chunk_mesher.hpp"
#include "world/chunk_streaming.hpp"
#include "world/day_cycle.hpp"
#include "world/fluid.hpp"
#include "world/lighting.hpp"
#include "world/raycast.hpp"
#include "world/world.hpp"
#include "world/world_generator.hpp"

namespace mc {

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
};

class Game {
public:
    // Which crafting panel (Phase 17) is open (None = nothing open).
    enum class OpenPanel {
        None,      // no panel
        Inventory, // E: 2x2 grid + the shapeless recipe list
        Table,     // right-clicked crafting table: the 3x3 grid
    };

    // The crafting UIs (Phase 15/17) hold references to the inventory,
    // the crafting grids and the registries, so they are constructed here
    // (after all of them, in declaration order).
    Game() : m_inventoryUi(m_inventory, m_inventoryGrid, m_items, m_recipes),
             m_tableUi(m_inventory, m_tableGrid, m_items, m_recipes) {}
    ~Game();

    Result<void> init(const GameArgs& args);
    // Runs the main loop. Returns the process exit code (0 = clean exit).
    int run();

    const Config& config() const { return m_config; }
    AssetManager& assets() { return m_assets; }
    const World& world() const { return m_world; }

private:
    // GPU geometry of one chunk: opaque + transparent (blended) sub-meshes.
    struct ChunkMesh {
        std::optional<Mesh> opaque;
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
    // Rebuilds the GPU mesh of every dirty (or never-meshed) chunk.
    Result<void> updateChunkMeshes();

    // Chunk streaming (Phase 14). integrateStreamedColumns runs after the
    // pool flush: worker-generated columns are inserted into the World
    // (insertChunk marks the chunk + face-sharing neighbors dirty, so the
    // next updateChunkMeshes re-meshes them and the boundary). The two
    // post* methods run last in update, after every main-thread World
    // write: from then on until the next flush the workers own the window.
    void integrateStreamedColumns();
    // Decides the columns to stream around the player (trigger + radius,
    // see chunk_streaming) and posts up to kStreamColumnsPerFrame
    // generation jobs to the pool.
    void streamChunks();
    // Finds chunks that appeared this frame (streamed-in or created by the
    // placement path) and posts one worker lighting job per affected
    // column box. The chunks render one frame dark and are re-meshed with
    // the correct light the frame after the job completes (deterministic,
    // frame-locked by the flush).
    void postLightJobs();
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
    // Cycles the hotbar selection; resets break progress (the selected
    // tool changed, Minecraft behavior).
    void cycleHotbar();
    // Drops one from the selected hotbar slot at the player's feet (Q).
    void dropSelectedItem();
    // Break speed of the selected item: tools use their data break_speed,
    // hand (empty slot) and block items break at 1.0.
    float selectedBreakSpeed() const;

    // Entity simulation (Phase 10): per-entity physics, then the item merge
    // pass and despawn (removal is deferred so the merge order — and
    // therefore the merge result — is deterministic).
    void updateEntities(double dt);
    // Dropped-item rendering: the items whose block belongs to the given
    // pass (opaque / transparent), bobbing + spinning around their rest
    // position (frame-counter driven — see kItemBobPeriodFrames).
    void drawItemEntities(const Mat4& view, const Mat4& projection,
                          bool transparentPass);
    // The cached full-cube mesh for a dropped block (built lazily on first
    // render; the geometry is cached, only the draw transform varies).
    const Mesh& itemMesh(BlockID id);

    // Mob rendering + spawning (Phase 20): the mobs are drawn after the
    // item entities with their cached box-model mesh (translate by the
    // mob's bottom-center, rotateY by its facing). attemptMobSpawn runs on
    // the data cadence (see mob_spawning) and pushes one MobEntity at the
    // deterministic scan's cell; it is a no-op unless the spawn config
    // allows it (cap, cadence, first frame, enabled flag).
    void attemptMobSpawn();
    void drawMobs(const Mat4& view, const Mat4& projection);
    const Mesh& mobMesh(MobID id);

    // Arrow rendering (Phase 21): the arrows (the P10 Projectiles the bow
    // launches) draw after the mobs in the opaque pass with the shared
    // box mesh, oriented along their velocity (model = translate *
    // rotateY(yaw) * rotateX(pitch), local -Z = travel direction, see
    // arrowOrientation), in the basic shader's solid-color path
    // (u_useTexture 0).
    void drawArrows(const Mat4& view, const Mat4& projection);
    const Mesh& arrowMesh();

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
    // Footstep sound timer (moving + on ground, pointer captured).
    void updateSoundTimers(double dt);
    // Phase 16 survival polling: a health drop drives the hurt flash +
    // sound, a death releases the pointer (the death screen is rendered
    // from the player's state) and plays the death sound, and R (while
    // dead) respawns the player.
    void updateSurvival(double dt);
    // World-space overlay: the break crack on the target block + the
    // target outline (drawn in the blending pass, depth test on).
    void drawWorldOverlay(const Mat4& view, const Mat4& projection);
    // Particle batch for one pass (dynamic mesh, rebuilt per frame;
    // split by the block's transparency like the item entities).
    void drawParticles(const Mat4& view, const Mat4& projection,
                       bool transparentPass);
    // The whole screen-space pass: hotbar + crosshair + held item (while
    // captured), the inventory/craft panel (while open), the F3 overlay
    // (while toggled). Runs with depth test + culling off; every element
    // is gated on a key/pointer state, so the no-input baseline renders
    // nothing extra.
    void renderUi();
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
    Result<void> writeSave() const;
    // Reads + strictly parses the slot file (cross-checked against the
    // registries and the world seed).
    Result<SaveGame> readSaveFile(const std::filesystem::path& path) const;
    // Applies a parsed save: cell edits + relight, player, inventory
    // (replacing the hotbar.json fill), entities, day-cycle time.
    Result<void> applySave(const SaveGame& save);
    // No-op unless --save is set, the slot is not --new, and the file
    // exists; then reads + applies it.
    Result<void> tryLoadSave();
    // The pure generator as a World chunk callback — used at world
    // creation AND as the generation reference for save diffs.
    World::ChunkGenerator makeChunkGenerator() const;

    GameArgs m_args;
    Config m_config;
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
    std::unordered_map<ChunkPos, ChunkMesh> m_chunkMeshes;

    // Seeded world generator (built in buildScene). Optional: the game only
    // touches it after init succeeded.
    std::optional<WorldGenerator> m_generator;

    // The first-person actor (built in buildScene after the world exists;
    // it holds references to the world and the block registry).
    std::optional<Player> m_player;

    // Chunk streaming (Phase 14): worker pool for out-of-area terrain. The
    // per-frame pipeline (see update) makes this race-free:
    //   flush -> integrate (main writes) -> interaction/fluids (main writes)
    //   -> mesh + render (main reads only) -> post jobs (workers run until
    //   the next flush). A job therefore touches shared state only while the
    //   main thread is in its read-only window.
    //   - generation jobs touch no shared state at all (the generator is a
    //     pure function; results land in the job's own scratch chunks);
    //   - lighting jobs write light arrays + the dirty flag, which the main
    //     thread never writes in its read-only window (meshing only reads
    //     light, baking it into vertices).
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
    // Columns with a generation job in flight (not yet in the World).
    std::set<ColumnPos> m_streamPending;
    // Streaming hysteresis (see chunk_streaming): engaged when the player
    // reaches the edge of the loaded area, disengaged when the player is
    // deep inside it again. While engaged, the full radius ring around
    // the player is filled (view_distance becomes visible terrain).
    bool m_streamingEngaged = false;
    // The streaming radius in chunk columns (config world.view_distance).
    int m_streamRadius = 8;
    // Every chunk the lighting pipeline has covered (all chunks at scene
    // build time, integrated or created since). A chunk appearing outside
    // this set is new this frame (streamed-in or placed into) and gets a
    // worker lighting job for its column box.
    std::set<ChunkPos> m_litChunks;

    // Non-player entities (Phase 10): dropped items + projectiles. The
    // game is the sole owner — it updates them each frame, merges items,
    // and removes the despawned.
    std::vector<std::unique_ptr<Entity>> m_entities;
    // Dropped-item meshes: one full-cube mesh per block id (built lazily).
    std::unordered_map<BlockID, Mesh, BlockIDHash> m_itemMeshes;

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
    Mesh m_outlineMesh;
    std::array<Mesh, 5> m_crackMeshes;
    // Phase 15 state (all default-off: the no-input baseline renders no
    // UI at all, keeping the Phase 14 screenshots byte-identical).
    bool m_debugOverlay = false;

    // Phase 17 crafting: the two persistent crafting grids (the E panel's
    // 2x2 uses the top-left 4 cells of its 9-cell model; the table's grid
    // is separate) and the open-panel state machine. The grids survive a
    // close/reopen (see KNOWN_ISSUES) and are saved (format 3).
    CraftingGrid m_inventoryGrid;
    CraftingGrid m_tableGrid;
    InventoryUI m_inventoryUi;
    InventoryUI m_tableUi;
    OpenPanel m_openPanel = OpenPanel::None;
    bool panelOpen() const { return m_openPanel != OpenPanel::None; }

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
