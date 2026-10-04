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

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "block/block_registry.hpp"
#include "core/asset_manager.hpp"
#include "core/camera.hpp"
#include "core/config.hpp"
#include "core/error.hpp"
#include "core/input.hpp"
#include "core/logger.hpp"
#include "core/window.hpp"
#include "entity/entity.hpp"
#include "entity/item_entity.hpp"
#include "entity/item_mesh.hpp"
#include "entity/projectile.hpp"
#include "item/inventory.hpp"
#include "item/item_registry.hpp"
#include "player/player.hpp"
#include "renderer/mesh.hpp"
#include "renderer/renderer.hpp"
#include "renderer/shader.hpp"
#include "renderer/texture.hpp"
#include "renderer/texture_atlas.hpp"
#include "world/biome.hpp"
#include "world/chunk.hpp"
#include "world/chunk_mesher.hpp"
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
};

class Game {
public:
    Game() = default;
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

    Result<void> buildScene();
    void update(double dt);
    void render();
    // Rebuilds the GPU mesh of every dirty (or never-meshed) chunk.
    Result<void> updateChunkMeshes();
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
    // Performs the break: air + relight + data-driven drop (ItemEntity).
    // The caller guarantees the hit is destroyable.
    void breakBlock(const BlockHit& hit);
    // Places one from the selected hotbar slot (block items only) and
    // consumes it from the stack.
    void placeBlock();
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

    // Non-player entities (Phase 10): dropped items + projectiles. The
    // game is the sole owner — it updates them each frame, merges items,
    // and removes the despawned.
    std::vector<std::unique_ptr<Entity>> m_entities;
    // Dropped-item meshes: one full-cube mesh per block id (built lazily).
    std::unordered_map<BlockID, Mesh, BlockIDHash> m_itemMeshes;

    // Item/inventory system (Phase 11): item registry (implicit block
    // items + explicit tools from assets/data/items), the player's
    // inventory (hotbar = first Inventory::kHotbarSize slots, starting
    // contents from assets/data/hotbar.json) and the selected hotbar slot.
    ItemRegistry m_items;
    Inventory m_inventory;
    std::size_t m_selectedSlot = 0;

    // Hold-to-break state (Phase 11): the cell being broken and how long
    // it has been held (seconds). Reset on target change, pointer unlock,
    // selection change, and completion.
    std::optional<BlockPos> m_breakTarget;
    double m_breakProgress = 0.0;

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

    // Chunk count the lighting was last fully propagated for; a new chunk
    // (out-of-area placement) starts with no light, so the next frame
    // re-propagates the whole world before meshing.
    std::size_t m_lastLightedChunks = 0;
};

} // namespace mc
