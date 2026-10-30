#include "core/game.hpp"

#include <GL/glew.h>
#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <numbers>
#include <sstream>

#include "block/block_sound.hpp"
#include "block/chance_drop.hpp"
#include "block/cube_face_table.hpp"
#include "core/game_local.hpp"
#include "core/json.hpp"
#include "core/png.hpp"
#include "renderer/alpha_test.hpp"
#include "ui/ui_scale.hpp"
#include "world/beacon.hpp"
#include "world/cloud.hpp"
#include "world/fire_spread.hpp"
#include "world/growth.hpp"
#include "world/noise.hpp"

namespace mc {

namespace {

constexpr int kStarCount = 300;

// Fixed starfield (deterministic hash; any value works).
constexpr std::uint64_t kStarSeed = 0x5EEDULL;

// Phase 83 beacon beam: the translucent column geometry — a thin box
// (kBeaconBeamHalfWidth on x/z) from y=0 to y=1, scaled up to
// kBeaconBeamBaseHeight + level * kBeaconBeamLevelHeight per active beacon
// (the beam is taller at higher pyramid levels, Minecraft).
constexpr float kBeaconBeamHalfWidth = 0.15f;

// Chunk streaming (Phase 14) + worker meshing (PIPE-1). The demo area is
// generated synchronously at startup; its 16 columns' initial meshing
// goes through the worker path like everything else (postMeshJobs 2
// columns/frame -> meshed within the first ~9 frames). Beyond the demo
// area, columns are generated on worker threads as the player approaches
// the edge of the loaded area.
// Worker count of the streaming pool (generation + mesh + lighting jobs).
constexpr int kStreamWorkerCount = 4;

// The overlay cubes sit just outside the target block's faces so they
// never z-fight with the block itself (the crack) and stay visible from
// outside (the outline).
constexpr float kCrackCubeScale = 1.006f;

constexpr float kOutlineScale = 1.004f;

} // namespace

Game::~Game() {
    // Close audio before the window's SDL_Quit (idempotent: run() already
    // did this on the normal path; this covers an early destruction).
    m_sound.shutdown();
    if (m_initialized) {
        m_window.shutdown();
    }
}

// ---------------------------------------------------------------------------
// Scene construction
// ---------------------------------------------------------------------------

Result<void> Game::buildScene() {
    // PIPE-2: the save journal's observer, installed at the TOP of
    // buildScene — before ANY cell-level write: the demo-area generation
    // (the generator callback) writes fresh content (never a journal
    // entry), and the save load's replay (tryLoadSave -> applySave ->
    // applyWorldEdits -> World::setBlock/setFluid, below) seeds the
    // journal automatically with zero extra code.
    m_world.setCellMutationObserver([this](const CellMutation& mu) {
        bool capped = false;
        if (mu.block.valid()) {
            capped = m_saveJournal.recordBlock(mu.pos);
        }
        // The displacement double report: a setBlock into a fluid cell
        // also recorded the drained cell in the fluid table.
        if (mu.fluidCode.has_value()) {
            capped = capped || m_saveJournal.recordFluid(mu.pos);
        }
        if (capped) {
            MC_LOG_INFO("Save journal exceeded kSaveJournalMaxEntries (" +
                        std::to_string(kSaveJournalMaxEntries) +
                        " entries) — the exit save falls back to the "
                        "full world diff");
        }
    });
    // -- shaders -------------------------------------------------------------
    auto vertSrc = m_assets.loadText("shaders/basic.vert");
    MC_TRY(vertSrc);
    auto fragSrc = m_assets.loadText("shaders/basic.frag");
    MC_TRY(fragSrc);

    auto shader = ShaderProgram::create(*vertSrc, *fragSrc);
    MC_TRY(shader);
    m_basicShader = std::move(*shader);

    // -- sky shader (unlit point sprites: sun, moon, stars) -------------------
    auto skyVert = m_assets.loadText("shaders/sky.vert");
    MC_TRY(skyVert);
    auto skyFrag = m_assets.loadText("shaders/sky.frag");
    MC_TRY(skyFrag);
    auto skyShader = ShaderProgram::create(*skyVert, *skyFrag);
    MC_TRY(skyShader);
    m_skyShader = std::move(*skyShader);

    // -- Phase 15 UI: unlit screen-space shader + generated textures --------
    auto uiVert = m_assets.loadText("shaders/ui.vert");
    MC_TRY(uiVert);
    auto uiFrag = m_assets.loadText("shaders/ui.frag");
    MC_TRY(uiFrag);
    auto uiShader = ShaderProgram::create(*uiVert, *uiFrag);
    MC_TRY(uiShader);
    m_uiShader = std::move(*uiShader);

    auto fontPath = m_assets.resolve("textures/font.png");
    MC_TRY(fontPath);
    auto fontTex = Texture::loadPng(*fontPath);
    MC_TRY(fontTex);
    m_fontTexture = std::move(*fontTex);

    auto crackPath = m_assets.resolve("textures/cracks.png");
    MC_TRY(crackPath);
    auto crackTex = Texture::loadPng(*crackPath);
    MC_TRY(crackTex);
    m_crackTexture = std::move(*crackTex);

    // Health heart strip (Phase 16): 48x16, three 16x16 tiles
    // (full/half/empty; tile t occupies u in [t/3, (t+1)/3]).
    auto healthPath = m_assets.resolve("textures/health.png");
    MC_TRY(healthPath);
    auto healthTex = Texture::loadPng(*healthPath);
    MC_TRY(healthTex);
    m_healthTexture = std::move(*healthTex);

    // Hunger drumstick strip (Phase 23): same 48x16 three-tile layout.
    auto hungerPath = m_assets.resolve("textures/hunger.png");
    MC_TRY(hungerPath);
    auto hungerTex = Texture::loadPng(*hungerPath);
    MC_TRY(hungerTex);
    m_hungerTexture = std::move(*hungerTex);

    // SURV-2: the oxygen bubble strip — a SINGLE 16x16 tile (no
    // half-state: the fill count IS the number of drawn icons). It
    // replaces the hunger row on screen while the air is below full.
    auto oxygenPath = m_assets.resolve("textures/oxygen.png");
    MC_TRY(oxygenPath);
    auto oxygenTex = Texture::loadPng(*oxygenPath);
    MC_TRY(oxygenTex);
    m_oxygenTexture = std::move(*oxygenTex);

    // Status effect icon strip (Phase 46): 16 px tiles (tile t occupies u
    // in [t/N, (t+1)/N]); the effect data's 'icon' field indexes a tile
    // (cross-checked against this texture's width at init, with the
    // registry loaded).
    auto effectPath = m_assets.resolve("textures/effects.png");
    MC_TRY(effectPath);
    auto effectTex = Texture::loadPng(*effectPath);
    MC_TRY(effectTex);
    m_effectTexture = std::move(*effectTex);

    // Target-block outline: a slightly enlarged unit cube as 24 line
    // vertices (12 edges x 2 endpoints), centered on the origin.
    {
        const float s = kOutlineScale * 0.5f;
        const float x0 = -s, x1 = s, y0 = -s, y1 = s, z0 = -s, z1 = s;
        const Vec3 kEdges[24] = {
            {x0, y0, z0}, {x1, y0, z0}, {x1, y0, z0}, {x1, y0, z1},
            {x1, y0, z1}, {x0, y0, z1}, {x0, y0, z1}, {x0, y0, z0},
            {x0, y1, z0}, {x1, y1, z0}, {x1, y1, z0}, {x1, y1, z1},
            {x1, y1, z1}, {x0, y1, z1}, {x0, y1, z1}, {x0, y1, z0},
            {x0, y0, z0}, {x0, y1, z0}, {x1, y0, z0}, {x1, y1, z0},
            {x1, y0, z1}, {x1, y1, z1}, {x0, y0, z1}, {x0, y1, z1},
        };
        std::vector<MeshVertex> verts;
        verts.reserve(24);
        for (const Vec3& p : kEdges) {
            MeshVertex v;
            v.position = p;
            verts.push_back(v);
        }
        auto up = m_outlineMesh.upload(verts, {}, MeshMode::Lines);
        MC_TRY(up);
    }

    // Phase 83 beacon beam: a thin translucent column (half-width on x/z,
    // unit height on y) — scaled + positioned per active beacon by the model
    // matrix in drawBeaconBeams. All 8 corners share normal +Y (so every face
    // gets the top-face shade = a uniform bright column, independent of
    // orientation). The baked light (skyLight 0.0 / blockLight 1.0 = the
    // legacy constant-white fallback) is swapped PER DRAW for the beacon
    // block's cell light by the LGT-4 vertex override (beamLightSample,
    // the PRE-6 itemDropLight pattern) — the geometry is never re-baked.
    // Culling is disabled for the beam draw, so the winding only needs to
    // trace each quad (both faces render, alpha-blended).
    {
        const float h = kBeaconBeamHalfWidth;
        const Vec3 kCorners[8] = {
            {-h, 0.0f, -h}, {h, 0.0f, -h}, {h, 0.0f, h}, {-h, 0.0f, h},
            {-h, 1.0f, -h}, {h, 1.0f, -h}, {h, 1.0f, h}, {-h, 1.0f, h},
        };
        const std::uint32_t kIndices[36] = {
            0, 1, 5, 0, 5, 4,  // -Z
            2, 3, 7, 2, 7, 6,  // +Z
            3, 0, 4, 3, 4, 7,  // -X
            1, 2, 6, 1, 6, 5,  // +X
            0, 3, 2, 0, 2, 1,  // -Y
            4, 5, 6, 4, 6, 7,  // +Y
        };
        std::vector<MeshVertex> verts;
        verts.reserve(8);
        for (const Vec3& p : kCorners) {
            MeshVertex v;
            v.position = p;
            v.normal = {0.0f, 1.0f, 0.0f};
            v.uv = {0.0f, 0.0f};
            v.skyLight = 0.0f;
            v.blockLight = 1.0f;
            v.uvLocal = {0.0f, 0.0f};
            v.tiled = 0.0f;
            verts.push_back(v);
        }
        std::vector<std::uint32_t> indices(kIndices, kIndices + 36);
        auto up =
            m_states.beaconBeamMesh.upload(verts, indices, MeshMode::Triangles);
        MC_TRY(up);
    }

    // Crack overlays: one full cube per stage (kCrackStages tiles of the
    // crack strip), slightly enlarged, UVs = the stage's tile.
    for (int stage = 0; stage < kCrackStages; ++stage) {
        std::vector<MeshVertex> verts;
        std::vector<std::uint32_t> indices;
        verts.reserve(24);
        indices.reserve(36);
        const float u0 = static_cast<float>(stage) / kCrackStages;
        const float u1 = static_cast<float>(stage + 1) / kCrackStages;
        for (const CubeFace& face : kCubeFaces) {
            // Full tile height (the crack art is directionless); the corner
            // UV order matches the verified cube-face mapping.
            const Vec2 faceUv[4] = {
                {u0, 1.0f}, {u1, 1.0f}, {u1, 0.0f}, {u0, 0.0f},
            };
            const std::uint32_t first =
                static_cast<std::uint32_t>(verts.size());
            for (int i = 0; i < 4; ++i) {
                MeshVertex v;
                v.position = {
                    (static_cast<float>(face.corner[i][0]) - 0.5f) *
                        kCrackCubeScale,
                    (static_cast<float>(face.corner[i][1]) - 0.5f) *
                        kCrackCubeScale,
                    (static_cast<float>(face.corner[i][2]) - 0.5f) *
                        kCrackCubeScale};
                v.normal = face.normal;
                v.uv = faceUv[i];
                verts.push_back(v);
            }
            indices.insert(indices.end(),
                           {first, first + 1, first + 2, first, first + 2,
                            first + 3});
        }
        auto up = m_crackMeshes[stage].upload(verts, indices);
        MC_TRY(up);
    }

    // -- day/night cycle (config-driven; --time overrides the start time) -----
    DayCycle::Params dayParams;
    dayParams.dayLengthSeconds =
        m_config.getDouble("world.day_length_seconds", 240.0);
    dayParams.startTimeOfDay =
        m_config.getDouble("world.start_time_of_day", 0.25);
    m_dayCycle = DayCycle(dayParams);

    // -- sky sprites: the starfield (deterministic quads) + the single quad
    //    the sun and moon share (positioned per draw by u_offset) ------------
    // Shared quad corners: view-space [-0.5, 0.5]^2, carried in the vertex
    // "normal" slot (the sky shader reads it as a_corner).
    const Vec3 kQuadCorners[4] = {{-0.5f, -0.5f, 0.0f},
                                  {0.5f, -0.5f, 0.0f},
                                  {0.5f, 0.5f, 0.0f},
                                  {-0.5f, 0.5f, 0.0f}};
    const std::uint32_t kQuadIndices[6] = {0, 1, 2, 0, 2, 3};

    std::vector<MeshVertex> starVerts;
    starVerts.reserve(static_cast<std::size_t>(kStarCount) * 4);
    std::vector<std::uint32_t> starIndices;
    starIndices.reserve(static_cast<std::size_t>(kStarCount) * 6);
    int star = 0;
    for (const Vec3& p :
         DayCycle::makeStarPositions(kStarSeed, kStarCount, kSkyDistance)) {
        for (const Vec3& corner : kQuadCorners) {
            MeshVertex v;
            v.position = p;
            v.normal = corner;
            starVerts.push_back(v);
        }
        const std::uint32_t base = static_cast<std::uint32_t>(star * 4);
        for (const std::uint32_t idx : kQuadIndices) {
            starIndices.push_back(base + idx);
        }
        ++star;
    }
    auto starUpload = m_starMesh.upload(starVerts, starIndices);
    MC_TRY(starUpload);

    std::vector<MeshVertex> pointVerts;
    for (const Vec3& corner : kQuadCorners) {
        MeshVertex v;
        v.normal = corner;
        pointVerts.push_back(v);
    }
    auto pointUpload =
        m_skyPointMesh.upload(pointVerts, {0, 1, 2, 0, 2, 3});
    MC_TRY(pointUpload);

    // -- blocks (data-driven from assets/data/blocks/*.json) ------------------
    auto blocksLoad = m_blocks.loadDirectory(m_assets, "data/blocks");
    MC_TRY(blocksLoad);

    // Phase 75: the campfire FAMILY ids (lit + extinguished), resolved
    // once (the registry is static after init; an absent name leaves the
    // id invalid = the family-dependent paths are structurally inert —
    // the P70 fire/campfire id pattern).
    m_states.campfireLitId = m_blocks.idForName("campfire");
    m_states.campfireExtId = m_blocks.idForName("campfire_extinguished");

    // -- texture atlas (16x16 tiles from assets/textures/blocks/*.png) --------
    // The layout is data (data/textures/block_atlas.json): explicit cells
    // per tile on a power-of-two grid, so adding a texture never moves an
    // existing tile's UVs (byte-stable baseline screenshots).
    auto atlasLoad = m_atlas.loadDirectory(m_assets, "textures/blocks");
    MC_TRY(atlasLoad);
    // Mob texture tiles (Phase 20) join the same atlas (separate directory,
    // same data-driven layout file).
    auto mobAtlasLoad = m_atlas.loadDirectory(m_assets, "textures/mobs");
    MC_TRY(mobAtlasLoad);
    auto atlasLayoutText = m_assets.loadText("data/textures/block_atlas.json");
    MC_TRY(atlasLayoutText);
    auto atlasLayout = Json::parse(*atlasLayoutText);
    MC_TRY(atlasLayout);
    auto atlasUpload = m_atlas.upload(*atlasLayout);
    MC_TRY(atlasUpload);

    // Cross-check: every texture a block references must exist in the atlas.
    for (const std::string& name : m_blocks.requiredTextures()) {
        if (!m_atlas.contains(name)) {
            return MC_FAIL("block references missing texture '" + name +
                           "' (no such tile in the atlas)");
        }
    }

    // -- items (Phase 11): implicit block items + explicit data items -------
    // Block items are defined by their blocks (same name); explicit items
    // (tools and materials) come from data files. Build the block items
    // first so an explicit item colliding with a block name is rejected.
    MC_TRY(m_items.buildBlockItems(m_blocks));
    auto itemsLoad = m_items.loadDirectory(m_assets, "data/items");
    MC_TRY(itemsLoad);
    // Strict data check: a block's drops must name a registered item whose
    // max stack fits dropsCount (block items cover the usual case, e.g.
    // grass -> wheat_1 seed; ores drop material items, e.g.
    // iron_ore -> iron_ingot; mature crops drop a harvest material, e.g.
    // wheat_7 -> wheat x2).
    for (const BlockID id : m_blocks.allIds()) {
        const Block& b = m_blocks.at(id);
        if (b.properties.drops.empty()) {
            continue;
        }
        const ItemID dropId = m_items.idForName(b.properties.drops);
        if (!dropId.valid()) {
            return MC_FAIL("block '" + b.name + "' drops unknown item '" +
                           b.properties.drops + "'");
        }
        if (b.properties.dropsCount > m_items.at(dropId).maxStack) {
            return MC_FAIL("block '" + b.name + "' drops " +
                           std::to_string(b.properties.dropsCount) +
                           " of '" + b.properties.drops +
                           "' but its max stack is " +
                           std::to_string(m_items.at(dropId).maxStack));
        }
    }

    // Strict data check (Phase 48): a block's grows_to must name an
    // EXISTING block (the next growth stage). The pairing + range are
    // enforced at data load; this is the existence cross-check (a typo in
    // the target name fails init loudly — the ammo discipline).
    for (const BlockID id : m_blocks.allIds()) {
        const Block& b = m_blocks.at(id);
        if (b.properties.growsTo.empty()) {
            continue;
        }
        const BlockID toId = m_blocks.idForName(b.properties.growsTo);
        if (!toId.valid()) {
            return MC_FAIL("block '" + b.name + "' grows to unknown block '" +
                           b.properties.growsTo + "'");
        }
    }

    // Strict data check (Phase 50): a block's grows_on must name an
    // EXISTING block (the soil the crop needs below it) — the same
    // existence cross-check as grows_to (a typo fails init loudly).
    for (const BlockID id : m_blocks.allIds()) {
        const Block& b = m_blocks.at(id);
        if (b.properties.growsOn.empty()) {
            continue;
        }
        const BlockID onId = m_blocks.idForName(b.properties.growsOn);
        if (!onId.valid()) {
            return MC_FAIL("block '" + b.name + "' grows on unknown block '" +
                           b.properties.growsOn + "'");
        }
    }

    // Strict data check (Phase 53): each plant_on name must reference an
    // EXISTING block (the soil the block can be placed on) — the same
    // existence cross-check as grows_on (a typo fails init loudly).
    for (const BlockID id : m_blocks.allIds()) {
        const Block& b = m_blocks.at(id);
        for (const std::string& soil : b.properties.plantOn) {
            const BlockID onId = m_blocks.idForName(soil);
            if (!onId.valid()) {
                return MC_FAIL("block '" + b.name + "' plant_on names "
                               "unknown block '" + soil + "'");
            }
        }
    }

    // GEN-2: resolve the tree specs (the sapling `tree` names -> BlockIDs,
    // the m_states.campfireLitId init-resolved pattern). The registry load
    // already validated the trunk / leaves names EXIST and the parse
    // validated the geometry, so the ids resolve (invalid would be a
    // logic error — the defensive guard keeps the spec out of the cache).
    m_states.treeSpecs.clear();
    for (const BlockID id : m_blocks.allIds()) {
        const Block& b = m_blocks.at(id);
        if (!b.hasTree()) {
            continue;
        }
        TreeSpec spec = treeSpecFromBlock(b);
        spec.trunk = m_blocks.idForName(b.properties.tree->trunk);
        spec.leaves = m_blocks.idForName(b.properties.tree->leaves);
        if (!spec.trunk.valid() || !spec.leaves.valid()) {
            return MC_FAIL("block '" + b.name + "' tree names unknown "
                           "block(s) '" + b.properties.tree->trunk +
                           "' / '" + b.properties.tree->leaves + "'");
        }
        m_states.treeSpecs[b.id] = std::move(spec);
    }

    // Strict data check (Phase 76): a block's ignites_to must name an
    // EXISTING block (the relight target) — the same existence
    // cross-check as grows_on (a typo fails init loudly).
    for (const BlockID id : m_blocks.allIds()) {
        const Block& b = m_blocks.at(id);
        if (b.properties.ignitesTo.empty()) {
            continue;
        }
        const BlockID toId = m_blocks.idForName(b.properties.ignitesTo);
        if (!toId.valid()) {
            return MC_FAIL("block '" + b.name + "' ignites_to names unknown "
                           "block '" + b.properties.ignitesTo + "'");
        }
    }

    // Strict data check (Phase 92): a block's stair_rotates_to must name an
    // EXISTING block (the next stair facing variant) — the same existence
    // cross-check as grows_on / ignites_to (a typo fails init loudly).
    for (const BlockID id : m_blocks.allIds()) {
        const Block& b = m_blocks.at(id);
        if (b.properties.stairRotatesTo.empty()) {
            continue;
        }
        const BlockID toId = m_blocks.idForName(b.properties.stairRotatesTo);
        if (!toId.valid()) {
            return MC_FAIL("block '" + b.name + "' stair_rotates_to names "
                           "unknown block '" + b.properties.stairRotatesTo +
                           "'");
        }
    }

    // CRAFT-3: a block's sound_material must name a material whose three
    // event wavs exist in the sound directory (the grows_to discipline —
    // a typo fails init loudly, naming the missing piece). File-based,
    // not the loaded bank: the check holds when the audio is disabled
    // (the headless degradation path keeps init strict).
    MC_TRY(checkSoundMaterials(m_blocks, m_assets));

    // Strict data check (Phase 51): a fluid block's water_reaction names
    // two EXISTING blocks (the source-water and flowing-water products of
    // the reaction — the ammo discipline; a typo fails init loudly). The
    // pairing is enforced at data load; this is the existence check.
    for (const BlockID id : m_blocks.allIds()) {
        const Block& b = m_blocks.at(id);
        if (b.properties.waterReactionSource.empty()) {
            continue;
        }
        const BlockID sourceId =
            m_blocks.idForName(b.properties.waterReactionSource);
        const BlockID flowingId =
            m_blocks.idForName(b.properties.waterReactionFlowing);
        if (!sourceId.valid() || !flowingId.valid()) {
            return MC_FAIL("block '" + b.name + "' water_reaction names "
                           "unknown block(s) '" +
                           b.properties.waterReactionSource + "' / '" +
                           b.properties.waterReactionFlowing + "'");
        }
    }

    // Strict data check (Phase 49): a block's chance_drop item must name a
    // registered item whose max stack fits chanceDropCount (the drops
    // discipline — a typo in the extra-drop name fails init loudly).
    for (const BlockID id : m_blocks.allIds()) {
        const Block& b = m_blocks.at(id);
        if (b.properties.chanceDropItem.empty() &&
            b.properties.chanceDrops.empty()) {
            continue;
        }
        if (!b.properties.chanceDropItem.empty()) {
            const ItemID cdId = m_items.idForName(b.properties.chanceDropItem);
            if (!cdId.valid()) {
                return MC_FAIL("block '" + b.name + "' chance_drop names unknown "
                               "item '" + b.properties.chanceDropItem + "'");
            }
            if (b.properties.chanceDropCount > m_items.at(cdId).maxStack) {
                return MC_FAIL("block '" + b.name + "' chance_drop yields " +
                               std::to_string(b.properties.chanceDropCount) +
                               " of '" + b.properties.chanceDropItem +
                               "' but its max stack is " +
                               std::to_string(m_items.at(cdId).maxStack));
            }
        }
        // Phase 52: the chance_drops LIST — the same discipline per entry
        // (the item must exist and its max stack fit the entry's count).
        for (std::size_t i = 0; i < b.properties.chanceDrops.size(); ++i) {
            const ChanceDrop& e = b.properties.chanceDrops[i];
            const ItemID id2 = m_items.idForName(e.item);
            if (!id2.valid()) {
                return MC_FAIL("block '" + b.name + "' chance_drops[" +
                               std::to_string(i) + "] names unknown item '" +
                               e.item + "'");
            }
            if (e.count > m_items.at(id2).maxStack) {
                return MC_FAIL("block '" + b.name + "' chance_drops[" +
                               std::to_string(i) + "] yields " +
                               std::to_string(e.count) + " of '" + e.item +
                               "' but its max stack is " +
                               std::to_string(m_items.at(id2).maxStack));
            }
        }
        continue;
    }

    // -- mobs (Phase 20): data-driven types + spawn config ------------------
    // Types come from data/mobs/*.json (one file per mob); the spawn
    // cadence/ring/cap from data/mob_spawning.json; the enable flag from
    // the config (mobs.enabled, default true). Cross-checks (same
    // discipline as block drops): the spawning list, every model texture
    // and every drop must resolve.
    MC_TRY(m_mobTypes.loadDirectory(m_assets, "data/mobs"));
    auto spawnText = m_assets.loadText("data/mob_spawning.json");
    MC_TRY(spawnText);
    auto spawnJson = Json::parse(*spawnText);
    MC_TRY(spawnJson);
    auto spawnCfg = MobSpawningConfig::parse(*spawnJson);
    MC_TRY(spawnCfg);
    m_spawnConfig = *spawnCfg;
    m_mobsEnabled = m_config.getBool("mobs.enabled", true);
    // ENT-1: the limb-swing enable flag (config mobs.limb_swing, default
    // true; false = the v0.9.0 static gait — the mobs.enabled pattern,
    // the E2E control group). Pure render layer: it gates nothing else.
    m_limbSwing = m_config.getBool("mobs.limb_swing", true);
    // Difficulty (Phase 25): strict — an unknown value fails init with a
    // message naming the field (the config default is "normal", which is
    // also the no-behavior-change baseline: scales 1.0 / cadence base).
    auto difficulty = parseDifficulty(
        m_config.getString("mobs.difficulty", "normal"));
    MC_TRY(difficulty);
    m_difficulty = *difficulty;
    for (const SpawnMobEntry& entry : m_spawnConfig.mobs) {
        if (m_mobTypes.findByName(entry.id) == nullptr) {
            return MC_FAIL("mob_spawning.json: unknown mob '" + entry.id +
                           "'");
        }
    }
    for (const MobID mid : m_mobTypes.allIds()) {
        const Mob& mob = m_mobTypes.at(mid);
        for (const MobBox& box : mob.model.boxes) {
            if (!m_atlas.contains(box.texture)) {
                return MC_FAIL("mob '" + mob.name + "': model texture '" +
                               box.texture + "' not in the atlas");
            }
        }
        for (const MobDrop& drop : mob.properties.drops) {
            const ItemID dropId = m_items.idForName(drop.item);
            if (!dropId.valid()) {
                return MC_FAIL("mob '" + mob.name + "' drops unknown item '" +
                               drop.item + "'");
            }
            // The ROLLED count can never exceed countMax (Phase 38), so
            // the max stack is checked against the range's top.
            if (drop.countMax > m_items.at(dropId).maxStack) {
                return MC_FAIL("mob '" + mob.name + "' drops up to " +
                               std::to_string(drop.countMax) +
                               " of '" + drop.item +
                               "' but its max stack is " +
                               std::to_string(m_items.at(dropId).maxStack));
            }
        }
    }

    // -- status effects (Phase 46) ------------------------------------------
    // The data-driven effect definitions (data/effects/*.json) + the
    // cross-checks: a mob's on_hit_effect must name a registered effect
    // (the ammo/milk pattern) and every effect's icon must be a valid
    // tile of the effects.png strip (the block-texture pattern).
    MC_TRY(m_effects.loadDirectory(m_assets, "data/effects"));
    const int effectTiles =
        static_cast<int>(m_effectTexture.width() / TextureAtlas::kTileSize);
    for (const int effId : m_effects.allIds()) {
        const Effect& eff = m_effects.at(effId);
        if (eff.icon < 0 || eff.icon >= effectTiles) {
            return MC_FAIL("effect '" + eff.name + "' icon " +
                           std::to_string(eff.icon) + " is outside the "
                           "effects.png tile range [0, " +
                           std::to_string(effectTiles - 1) + "]");
        }
    }
    // Phase 57: at most ONE effect may ignite the player from the lava
    // (the fire). Zero is legal (no fire system in a modded asset set).
    // Phase 58: the MOB burn ticks on the fixed 1/60 frame grid (hard rule
    // 8), so the ignites effect's tick interval must be a whole number of
    // frames (the shipped fire: 0.2 s = 12 frames) — a non-grid interval
    // would silently quantize the mob's burn rate.
    for (const int effId : m_effects.allIds()) {
        const Effect& eff = m_effects.at(effId);
        if (!eff.ignitesOnLava) {
            continue;
        }
        if (m_fireEffectId != -1) {
            return MC_FAIL("only one effect may have 'ignites_on_lava' "
                           "(found '" + m_effects.at(m_fireEffectId).name +
                           "' and '" + eff.name + "')");
        }
        // The tolerance is in FRAME units (1e-4 frames = 1.7 us per tick):
        // the float interval 0.2f * 60 lands at 12.00000018, while any
        // genuinely non-grid interval (0.33 s = 19.8 frames) misses by a
        // large fraction of a frame.
        const double frames = eff.tickInterval * 60.0;
        if (frames < 1.0 ||
            std::abs(frames - std::round(frames)) > 1e-4) {
            return MC_FAIL("effect '" + eff.name +
                           "' has 'ignites_on_lava' but its tick_interval "
                           "is not a whole number of 1/60 s frames (the "
                           "mob burn grid)");
        }
        m_fireEffectId = effId;
    }
    // Strict data check (Phase 83, a list in Phase 91): every effect a
    // block's beacon applies must name an EXISTING effect (the beacon
    // applies them to in-range players). The empty-string rejection is at
    // data load; this is the existence cross-check (a typo in an effect
    // name fails init loudly — the grows_to / on_hit_effect discipline).
    // Any registered effect is a valid beacon target (the shipped data uses
    // ["absorption", "speed"] — Phase 91's multi-effect "enchanting
    // table").
    for (const BlockID id : m_blocks.allIds()) {
        const Block& b = m_blocks.at(id);
        for (const std::string& effName : b.properties.beaconEffects) {
            if (m_effects.findByName(effName) == nullptr) {
                return MC_FAIL("block '" + b.name + "' beacon_effects names "
                               "unknown effect '" + effName + "'");
            }
        }
    }
    for (const MobID mid : m_mobTypes.allIds()) {
        const Mob& mob = m_mobTypes.at(mid);
        if (!mob.properties.hitEffectName.empty()) {
            const Effect* eff =
                m_effects.findByName(mob.properties.hitEffectName);
            if (eff == nullptr) {
                return MC_FAIL("mob '" + mob.name + "' on_hit_effect "
                               "references the unregistered effect '" +
                               mob.properties.hitEffectName + "'");
            }
            // SURV-6: the grant's tier range (0-based < tier_max).
            if (mob.properties.hitEffectTier >= eff->tierMax) {
                return MC_FAIL("mob '" + mob.name + "' on_hit_effect grants "
                               "'" + eff->name + "' at tier " +
                               std::to_string(mob.properties.hitEffectTier) +
                               ", but the effect has tier_max " +
                               std::to_string(eff->tierMax) +
                               " (tiers are 0-based, in [0, " +
                               std::to_string(eff->tierMax - 1) + "])");
            }
        }
        // SURV-5: the arrow effect must name an EXISTING effect (the
        // on_hit_effect pattern). Unlike the BOW's arrow_effect it does
        // NOT require `affects_mobs`: the skeleton's arrow hits the
        // PLAYER (its wither is player-facing), and a wither arrow that
        // happens to strike another mob passes the mob-side gate on its
        // own (the wither carries affects_mobs too).
        if (!mob.properties.arrowEffectName.empty()) {
            const Effect* eff =
                m_effects.findByName(mob.properties.arrowEffectName);
            if (eff == nullptr) {
                return MC_FAIL("mob '" + mob.name + "' arrow_effect "
                               "references the unregistered effect '" +
                               mob.properties.arrowEffectName + "'");
            }
            // SURV-6: the grant's tier range (0-based < tier_max).
            if (mob.properties.arrowEffectTier >= eff->tierMax) {
                return MC_FAIL("mob '" + mob.name + "' arrow_effect grants "
                               "'" + eff->name + "' at tier " +
                               std::to_string(mob.properties.arrowEffectTier) +
                               ", but the effect has tier_max " +
                               std::to_string(eff->tierMax) +
                               " (tiers are 0-based, in [0, " +
                               std::to_string(eff->tierMax - 1) + "])");
            }
        }
    }
    for (const ItemID iid : m_items.allIds()) {
        const Item& it = m_items.at(iid);
        // A grant must name an EXISTING effect (the ammo pattern) AND obey
        // the SURV-4 duration/instant equivalence: a grant of duration 0 is
        // an INSTANT effect (instant_hp != 0 — settled on apply, no
        // lingering); a grant of positive duration is a LINGERING effect
        // (instant_hp == 0). Both directions are named: a 0 on a lingering
        // effect is a dead grant; a positive duration on an instant effect
        // would leave it lingering (an instant should not).
        auto checkGrant = [&](const std::string& field,
                              const std::string& effName,
                              double duration,
                              int tier) -> Result<void> {
            const Effect* eff = m_effects.findByName(effName);
            if (eff == nullptr) {
                return MC_FAIL("item '" + it.name + "' " + field +
                               " references the unregistered effect '" +
                               effName + "'");
            }
            // SURV-6: the grant's tier is 0-based and must be < the
            // effect's tier_max (a single-tier effect rejects any
            // non-zero tier — named).
            if (tier >= eff->tierMax) {
                return MC_FAIL("item '" + it.name + "' " + field +
                               " grants '" + effName + "' at tier " +
                               std::to_string(tier) + ", but the effect "
                               "has tier_max " +
                               std::to_string(eff->tierMax) + " (tiers "
                               "are 0-based, in [0, " +
                               std::to_string(eff->tierMax - 1) + "])");
            }
            const bool instant = eff->instantHp != 0.0f;
            if (duration == 0.0 && !instant) {
                return MC_FAIL("item '" + it.name + "' " + field +
                               " grants '" + effName +
                               "' with duration 0, but the effect is not "
                               "instant (instant_hp 0) — a 0-duration grant "
                               "must name an instant effect");
            }
            if (duration > 0.0 && instant) {
                return MC_FAIL("item '" + it.name + "' " + field +
                               " grants the instant effect '" + effName +
                               "' with a positive duration — an instant "
                               "effect settles on apply and must be granted "
                               "duration 0");
            }
            return {};
        };
        if (!it.grantsEffect.empty()) {
            MC_TRY(checkGrant("grants_effect", it.grantsEffect,
                              it.grantsEffectDuration, it.grantsEffectTier));
        }
        // Phase 56: the list form, entry by entry (indexed name, the
        // chance_drops[i] pattern).
        for (std::size_t i = 0; i < it.grantsEffects.size(); ++i) {
            const Item::GrantEffect& g = it.grantsEffects[i];
            MC_TRY(checkGrant("grants_effects[" + std::to_string(i) + "]",
                              g.name, g.duration, g.tier));
        }
        // Phase 67: the fast_against list, entry by entry (the grows_on
        // discipline: a typo must fail init loudly, not silently break
        // at the normal speed).
        for (std::size_t i = 0; i < it.fastAgainst.size(); ++i) {
            const std::string& bn = it.fastAgainst[i];
            if (!m_blocks.idForName(bn).valid()) {
                return MC_FAIL("item '" + it.name + "' fast_against[" +
                               std::to_string(i) +
                               "] references the unknown block '" + bn +
                               "'");
            }
        }
        // SURV-5: the launcher's arrow effect must name an EXISTING
        // effect (the ammo pattern) AND carry `affects_mobs` — a bow's
        // arrow only ever hits MOBS (a player's bow arrow is unflagged
        // hurtPlayer, no self-damage), so a player-only effect on a bow
        // would be a dead grant (the named rejection).
        if (!it.arrowEffectName.empty()) {
            const Effect* eff = m_effects.findByName(it.arrowEffectName);
            if (eff == nullptr) {
                return MC_FAIL("item '" + it.name + "' arrow_effect "
                               "references the unregistered effect '" +
                               it.arrowEffectName + "'");
            }
            if (!eff->affectsMobs) {
                return MC_FAIL("item '" + it.name + "' arrow_effect names "
                               "effect '" + it.arrowEffectName +
                               "' which does not accept mobs "
                               "('affects_mobs' false) — a bow's arrow "
                               "only hits mobs");
            }
            // SURV-6: the grant's tier range (0-based < tier_max).
            if (it.arrowEffectTier >= eff->tierMax) {
                return MC_FAIL("item '" + it.name + "' arrow_effect grants "
                               "'" + it.arrowEffectName + "' at tier " +
                               std::to_string(it.arrowEffectTier) +
                               ", but the effect has tier_max " +
                               std::to_string(eff->tierMax) +
                               " (tiers are 0-based, in [0, " +
                               std::to_string(eff->tierMax - 1) + "])");
            }
        }
    }

    // -- fire (Phase 64, the fire visuals): the burn target --------------
    // The ignition (Phase 60) and the lava burn pass (Phase 58) burn a
    // `flamable` block into the FIRE block (a light source that burns out
    // after fire.lifetime_seconds and is extinguished by water) — not air.
    // A registry with ANY flamable block must therefore provide the
    // "fire" block (the single named reference — without it the burn
    // consumers would have no target). Zero flamable blocks (a stripped
    // asset set) is legal: the burn paths stay inert and the fire block
    // is optional.
    bool anyFlamable = false;
    for (const BlockID bid : m_blocks.allIds()) {
        if (m_blocks.at(bid).properties.flamable) {
            anyFlamable = true;
            break;
        }
    }
    m_states.fireId = m_blocks.idForName("fire");
    if (anyFlamable && !m_states.fireId.valid()) {
        return MC_FAIL("the flamable blocks require the 'fire' block (the "
                       "burn target of the ignition and the lava burn "
                       "pass) — register it in data/blocks");
    }
    m_states.fireLifetime = m_config.getDouble("fire.lifetime_seconds", 4.0);
    if (m_states.fireLifetime <= 0.0) {
        return MC_FAIL("config fire.lifetime_seconds must be > 0 (got " +
                       std::to_string(m_states.fireLifetime) + ")");
    }
    // Fire v2 (Phase 66): the spread roll cadence + per-neighbor chance
    // (Minecraft values: the 0.1 s tick rhythm, the normal-difficulty
    // per-neighbor 5%) and the ember-emission gate.
    m_states.fireSpreadInterval =
        m_config.getInt("fire.spread_interval_frames", 6);
    if (m_states.fireSpreadInterval < 1) {
        return MC_FAIL("config fire.spread_interval_frames must be >= 1 "
                       "(got " +
                       std::to_string(m_states.fireSpreadInterval) + ")");
    }
    m_states.fireSpreadChance = m_config.getDouble("fire.spread_chance", 0.05);
    if (m_states.fireSpreadChance < 0.0 || m_states.fireSpreadChance > 1.0) {
        return MC_FAIL("config fire.spread_chance must be in [0, 1] (got " +
                       std::to_string(m_states.fireSpreadChance) + ")");
    }
    m_states.fireParticles = m_config.getBool("fire.particles", true);
    // CRAFT-5: the lit-furnace flame gate (the fire.particles pattern;
    // off = zero emissions = bit-identical to v1.21.0).
    m_states.furnaceFlames = m_config.getBool("render.furnace_flames", true);
    // PRE-8: the burning-mob flame gate (the render.furnace_flames
    // pattern; off = zero emissions = bit-identical to v1.28.0).
    m_states.mobFlames = m_config.getBool("render.mob_flames", true);

    // Phase 83 beacon: the effect range (Chebyshev blocks from the beacon
    // cell; the Minecraft beacon reaches the whole build area, 50 is a
    // generous default) and the beam visibility gate (the E2E control, the
    // fire.particles pattern — a no-input run without beacons is inert
    // either way).
    m_states.beaconRange = m_config.getInt("beacon.range_blocks", 50);
    if (m_states.beaconRange <= 0) {
        return MC_FAIL("config beacon.range_blocks must be > 0 (got " +
                       std::to_string(m_states.beaconRange) + ")");
    }
    m_states.beaconBeam = m_config.getBool("beacon.beam", true);

    // SAVE-1: the auto-save interval (save.auto_save_seconds — a CODE
    // default, the fire/mobs/beacon config-key discipline: the committed
    // config.json carries no save section). Strict read: absent = the
    // default, present = a finite number >= 0 (0 = off = the pre-SAVE-1
    // behavior; a negative / non-finite / non-number value is named and
    // rejected).
    {
        auto interval = autoSaveIntervalSeconds(m_config);
        MC_TRY(interval);
        m_autoSaveInterval = *interval;
    }

    // UX-1: the deep-night sky floor (render.night_sky_level — a CODE
    // default kNightSkyLevel, the fire/mobs/beacon config-key discipline:
    // the committed config.json carries no such key). Strict read: absent
    // = 7 (bit-identical to v1.34.0); present = an integer in [0,15]
    // (0 = the all-black night, the ambient floor goes with it; 15 = no
    // retreat). Non-integer / out-of-range / non-number is named and
    // rejected (the SAVE-1 discipline — the Config getters would silently
    // fall back on a type mismatch).
    auto nightLevel = readNightSkyLevel(m_config);
    MC_TRY(nightLevel);
    m_nightSkyLevel = *nightLevel;
    // The two uniforms are per-run constants (config-driven, no per-draw
    // variation): set them once on the basic program. At the default L =
    // 7 the uploaded values (8.0f/15.0f, 0.03f) are the pre-UX-1 named
    // constants bit-for-bit (the GLSL 8.0/15.0 and 0.03 float literals
    // round to the same IEEE values).
    m_basicShader.bind();
    m_basicShader.setUniform1f("u_nightRetreat",
                               nightRetreatScale(m_nightSkyLevel));
    m_basicShader.setUniform1f("u_ambient",
                               ambientFloorForLevel(m_nightSkyLevel));
    m_basicShader.unbind();

    // -- item icons (Phase 15 UI) ------------------------------------------
    // Each explicit item may name a 2D icon under assets/textures/items/
    // (strict: the file must exist — same cross-check pattern as the block
    // textures). Block items have no icon (the UI renders a 3D block cube).
    m_itemIcons.resize(m_items.count());
    for (const ItemID id : m_items.allIds()) {
        const Item& item = m_items.at(id);
        if (item.icon.empty()) {
            continue;
        }
        const std::string rel = "textures/items/" + item.icon + ".png";
        if (!m_assets.exists(rel)) {
            return MC_FAIL("item '" + item.name + "' references missing "
                           "icon texture '" + rel + "'");
        }
        auto iconPath = m_assets.resolve(rel);
        MC_TRY(iconPath);
        auto iconTex = Texture::loadPng(*iconPath);
        MC_TRY(iconTex);
        m_itemIcons[static_cast<std::size_t>(id.value)] = std::move(*iconTex);
    }

    // -- item model textures (CRAFT-2) ------------------------------------
    // Each model item names one texture file under
    // assets/textures/items/ (its icon or a dedicated one) for the box-
    // model render path. The file-existence cross-check is the icon-file
    // discipline (the grows_to pattern: it could not be validated per
    // parse because it is a filesystem check). The model geometry itself
    // is pure data — the mesh is built lazily on first render
    // (itemModelMesh).
    m_itemModelTextures.resize(m_items.count());
    m_itemModelMeshes.resize(m_items.count());
    // PRE-5: the per-item cutout census (parallel to the model textures):
    // decode the model's PNG ONCE and run isPureCutoutImage over its alpha
    // channel — a pure cutout (alpha set ⊆ {0, 255}) routes the item's
    // world drop to the CUTOUT pass (visible through the leaf holes); an
    // icon with a translucent core stays in the BLEND pass. (The shipped
    // icons are all pure cutout — the census is content data, and a
    // future translucent icon re-routes itself with zero code changes.)
    m_itemCutout.assign(m_items.count(), false);
    for (const ItemID id : m_items.allIds()) {
        const Item& item = m_items.at(id);
        if (!item.model.has_value()) {
            continue;
        }
        const std::string rel =
            "textures/items/" + item.model->texture + ".png";
        if (!m_assets.exists(rel)) {
            return MC_FAIL("item '" + item.name +
                           "' model references missing texture '" + rel +
                           "'");
        }
        auto modelPath = m_assets.resolve(rel);
        MC_TRY(modelPath);
        std::ifstream in(*modelPath, std::ios::binary);
        if (!in) {
            return MC_FAIL("cannot open item model texture file: '" +
                           modelPath->string() + "'");
        }
        const std::string bytes((std::istreambuf_iterator<char>(in)),
                                std::istreambuf_iterator<char>());
        auto image = Png::decode(bytes);
        MC_TRY(image);
        m_itemCutout[static_cast<std::size_t>(id.value)] =
            isPureCutoutImage(image->rgba.data(), image->width,
                              image->height);
        auto modelTex = Texture::createFromPixels(
            image->width, image->height, image->rgba.data());
        MC_TRY(modelTex);
        m_itemModelTextures[static_cast<std::size_t>(id.value)] =
            std::move(*modelTex);
    }

    // -- launcher ammo (Phase 24): the 'ammo' field names an item that
    // must exist (same cross-check pattern as the icon file check; it
    // could not be validated per-file, the ammo item may be defined in a
    // later file).
    for (const ItemID id : m_items.allIds()) {
        const Item& item = m_items.at(id);
        if (!item.ammo.empty() && m_items.findByName(item.ammo) == nullptr) {
            return MC_FAIL("item '" + item.name + "' references unknown "
                           "ammo item '" + item.ammo + "'");
        }
        // Phase 40: the furnace product (same cross-check pattern as the
        // ammo — the product may be defined in a later file).
        if (!item.smeltableTo.empty() &&
            m_items.findByName(item.smeltableTo) == nullptr) {
            return MC_FAIL("item '" + item.name + "' references unknown "
                           "smelting product '" + item.smeltableTo + "'");
        }
        // Phase 43: the drinkable's returned container (same cross-check
        // pattern — the container may be defined in a later file).
        if (!item.returnsItem.empty() &&
            m_items.findByName(item.returnsItem) == nullptr) {
            return MC_FAIL("item '" + item.name + "' references unknown "
                           "container item '" + item.returnsItem + "'");
        }
        // Phase 47 (kinds in Phase 51): the fluid-bucket PAIR (the ammo
        // pattern extended to pairs). The EMPTY side's companion is
        // DERIVED per named fluid: there must be exactly ONE full bucket
        // that pours that fluid back into this item (the parse keeps each
        // side locally consistent; only the round trip needs both files).
        // The FULL side's companion must exist and be an empty bucket
        // that collects this fluid.
        for (const std::string& fluid : item.collectsFluids) {
            // Exactly ONE full bucket must pour this fluid back into this
            // item (zero = a dead field, two = an ambiguous scoop target).
            int matches = 0;
            for (const ItemID otherId : m_items.allIds()) {
                const Item& other = m_items.at(otherId);
                if (other.poursFluid == fluid &&
                    other.poursBecomes == item.name) {
                    ++matches;
                }
            }
            if (matches != 1) {
                return MC_FAIL("item '" + item.name + "' collects fluid '" +
                               fluid + "' with " + std::to_string(matches) +
                               " full bucket(s) pouring it back (exactly "
                               "one is required — the scoop target must "
                               "be unique)");
            }
        }
        if (!item.poursBecomes.empty()) {
            const Item* other = m_items.findByName(item.poursBecomes);
            const bool collectsThisFluid =
                other != nullptr &&
                std::find(other->collectsFluids.begin(),
                          other->collectsFluids.end(),
                          item.poursFluid) != other->collectsFluids.end();
            if (other == nullptr || !collectsThisFluid) {
                return MC_FAIL("item '" + item.name + "' pours back to '" +
                               item.poursBecomes +
                               "', but that item does not collect fluid '" +
                               item.poursFluid +
                               "' (a bucket pair must be symmetric: the "
                               "empty bucket names this fluid)");
            }
        }
    }

    // -- mob arrow ammo (Phase 42): a ranged mob's 'ammo' field names the
    // item a stuck arrow returns to the player's inventory on pickup (the
    // same cross-check pattern as the launcher ammo above).
    for (const MobID id : m_mobTypes.allIds()) {
        const Mob& def = m_mobTypes.at(id);
        if (!def.properties.ammoItem.empty() &&
            m_items.findByName(def.properties.ammoItem) == nullptr) {
            return MC_FAIL("mob '" + def.name + "' references unknown "
                           "arrow ammo item '" + def.properties.ammoItem +
                           "'");
        }
        // Phase 43: the milking pair (both names must exist — the parsing
        // guarantees the pair is complete, only the names are checked
        // here, the ammo pattern).
        if (!def.properties.milkWith.empty()) {
            if (m_items.findByName(def.properties.milkWith) == nullptr) {
                return MC_FAIL("mob '" + def.name + "' milking pair "
                               "references unknown item '" +
                               def.properties.milkWith + "'");
            }
            if (m_items.findByName(def.properties.milkGives) == nullptr) {
                return MC_FAIL("mob '" + def.name + "' milking pair "
                               "references unknown item '" +
                               def.properties.milkGives + "'");
            }
        }
    }

    // -- dye families (Phase 41): the recolor interaction resolves its
    // target block purely by NAME (dyeVariantName), so the data must be
    // COMPLETE: every dyeable block names an existing dye item color,
    // and for every dye family (distinct 'dye_base' values) every dye
    // item color has its variant block (a missing variant would be a
    // dead dye — same cross-check pattern as the ammo/smelt product).
    {
        std::vector<std::string> dyeItemColors;
        for (const ItemID id : m_items.allIds()) {
            if (!m_items.at(id).dyeColor.empty()) {
                dyeItemColors.push_back(m_items.at(id).dyeColor);
            }
        }
        std::vector<std::string> dyeBases;
        for (const BlockID id : m_blocks.allIds()) {
            const BlockProperties& p = m_blocks.at(id).properties;
            if (p.dyeBase.empty()) {
                continue;
            }
            dyeBases.push_back(p.dyeBase);
            if (p.dyeColor.empty()) {
                return MC_FAIL("dyeable block '" +
                               m_blocks.at(id).name + "' (dye_base '" +
                               p.dyeBase + "') requires a 'dye_color' "
                               "property");
            }
            bool colorKnown = false;
            for (const std::string& c : dyeItemColors) {
                if (c == p.dyeColor) {
                    colorKnown = true;
                    break;
                }
            }
            if (!colorKnown) {
                return MC_FAIL("dyeable block '" + m_blocks.at(id).name +
                               "' has dye color '" + p.dyeColor +
                               "' but no dye item of that color exists");
            }
        }
        for (const std::string& base : dyeBases) {
            for (const std::string& color : dyeItemColors) {
                const std::string variant = dyeVariantName(base, color);
                if (m_blocks.findByName(variant) == nullptr) {
                    return MC_FAIL("dye family '" + base + "' is missing "
                                   "the variant block '" + variant +
                                   "' (dye color '" + color + "')");
                }
            }
        }
    }

    // -- recipes (Phase 12): data-driven crafting (UI/trigger = Phase 15) --
    auto recipesLoad = m_recipes.loadDirectory(m_assets, m_items,
                                               "data/recipes");
    MC_TRY(recipesLoad);

    // -- hotbar (starting inventory contents, data-driven) -------------------
    // assets/data/hotbar.json: { "items": [name, ...] } — at most
    // Inventory::kHotbarSize entries; slot i starts with a full stack of
    // the item (placing consumes the stacks; blocks are no longer infinite).
    auto hotbarText = m_assets.loadText("data/hotbar.json");
    MC_TRY(hotbarText);
    auto hotbarJson = Json::parse(*hotbarText);
    MC_TRY(hotbarJson);
    const Json* list = hotbarJson->find("items");
    if (list == nullptr || !list->isArray() || list->size() == 0) {
        return MC_FAIL("hotbar.json: 'items' array is required and must "
                       "not be empty");
    }
    if (list->size() > static_cast<std::size_t>(Inventory::kHotbarSize)) {
        return MC_FAIL("hotbar.json: at most " +
                       std::to_string(Inventory::kHotbarSize) +
                       " items (hotbar size)");
    }
    for (std::size_t i = 0; i < list->size(); ++i) {
        const Json& entry = list->at(i);
        if (!entry.isString()) {
            return MC_FAIL("hotbar.json: entry #" + std::to_string(i) +
                           " must be an item name string");
        }
        const ItemID iid = m_items.idForName(entry.asString());
        if (!iid.valid()) {
            return MC_FAIL("hotbar.json: unknown item '" + entry.asString() +
                           "'");
        }
        m_inventory.setSlot(i, ItemStack {iid, m_items.at(iid).maxStack});
    }

    // GEN-2: the starter STORAGE fill (hotbar.json's `inventory` section,
    // the strict parse in item/inventory — the hidden-starter pattern:
    // the saplings sit in the storage, NOT the hotbar, so a no-input run
    // renders nothing new; the slot ranges are disjoint, so no overwrite
    // of the `items` fill is possible). A SAVE LOAD still replaces the
    // whole inventory (the starter is a fresh-world gift — the applySave
    // contract).
    auto starterSlots = parseStarterSlots(*hotbarJson, m_items);
    MC_TRY(starterSlots);
    for (const StarterSlot& s : *starterSlots) {
        m_inventory.setSlot(s.slot, ItemStack {s.item, s.count});
    }

    // -- world generation (Phase 4: seeded, data-driven) ---------------------
    auto biomesLoad = m_biomes.loadDirectory(m_assets, "data/biomes");
    MC_TRY(biomesLoad);
    auto worldgen = WorldgenParams::load(m_assets);
    MC_TRY(worldgen);
    const std::int64_t seed = m_config.getInt64("world.seed", 0);
    auto gen = WorldGenerator::create(seed, *worldgen, m_biomes, m_blocks);
    MC_TRY(gen);
    m_generator = std::move(*gen);

    m_world.setChunkGenerator(makeChunkGenerator());
    for (int cx = kDemoChunkMin; cx <= kDemoChunkMax; ++cx) {
        for (int cz = kDemoChunkMin; cz <= kDemoChunkMax; ++cz) {
            for (int cy = 0; cy * kChunkSize < kWorldHeight; ++cy) {
                m_world.getOrCreateChunk({cx, cy, cz});
            }
        }
    }

    // Lighting: full propagation over the generated area. Runs once at
    // startup (a few tens of ms); break/place then use the cheap local
    // update, and chunks that appear later (streamed-in or placed into)
    // get a worker lighting job for their column box (postLightJobs).
    Lighting::propagateAll(m_world, m_blocks);

    // Chunk streaming (Phase 14, STRM-1): the worker pool and the radius
    // (config world.view_distance, in chunk columns around the player).
    // The raw config value is kept here for reporting; the geometric
    // consumers clamp it to kStreamCapRadius (far-plane cap).
    m_pool = std::make_unique<ThreadPool>(kStreamWorkerCount);
    m_streamRadius = m_config.getInt("world.view_distance", 8);

    // Spawn the player on dry land: the seed may put the origin underwater
    // or a tree may stand on the column, so check nearby candidates before
    // falling back to (0, 0).
    // Player tuning (Phase 32): the config's player.* keys override the
    // Player::Params defaults (a missing key = the struct default; present
    // keys are validated, so a mistyped value fails startup instead of
    // silently falling back).
    const auto playerParams = Player::Params::fromConfig(m_config);
    MC_TRY(playerParams);
    // UI scale (Phase 34): same missing-key / validated-value contract.
    const auto uiScale = uiScaleFromConfig(m_config);
    MC_TRY(uiScale);
    m_uiScale = *uiScale;
    const Player::Params& playerDims = *playerParams; // room check
    Vec3 spawnPos {0.5f,
                   static_cast<float>(m_generator->surfaceHeight(0, 0)) + 1.0f,
                   0.5f};
    constexpr int kSpawnCandidates[][2] = {
        {0, 0}, {4, 0}, {-4, 0}, {8, 0}, {-8, 0},
        {0, 4}, {0, -4}, {4, 4}, {-4, -4}, {8, 8},
    };
    int spawnX = 0;
    int spawnZ = 0;
    for (const auto& c : kSpawnCandidates) {
        const int h = m_generator->surfaceHeight(c[0], c[1]);
        if (h <= m_generator->params().seaLevel) {
            continue;
        }
        const Vec3 cand {static_cast<float>(c[0]) + 0.5f,
                         static_cast<float>(h) + 1.0f,
                         static_cast<float>(c[1]) + 0.5f};
        if (Player::hasRoomAt(m_world, m_blocks, cand, playerDims.width,
                              playerDims.height)) {
            spawnPos = cand;
            spawnX = c[0];
            spawnZ = c[1];
            break;
        }
    }
    // The WORLD spawn (Phase 25): the center of the hostile spawn-protection
    // circle. Captured BEFORE the save load — a loaded save moves the
    // player, but the protection stays anchored to the world spawn (the
    // Minecraft semantics; there are no beds in v1).
    m_spawnPoint = spawnPos;
    m_player.emplace(m_world, m_blocks, spawnPos, *playerParams);
    m_player->pitch = -0.25f; // look slightly down at the landscape
    m_camera.position = m_player->eyePosition();
    m_camera.yaw = m_player->yaw;
    m_camera.pitch = m_player->pitch;

    // -- save load (Phase 13): a saved state REPLACES the fresh world ----
    // Runs after generation + spawn: the cell edits apply on top of the
    // generated terrain, and the saved player/inventory/entities/time win
    // over the fresh ones (--time only affects fresh worlds).
    MC_TRY(tryLoadSave());

    // Crop-growth tracking set (Phase 48): rebuilt from the world after
    // the save load (block edits restored the planted crops). A fresh
    // world scans to nothing — worldgen places no crops (v1).
    rescanCrops();
    // GEN-2: the sapling tracking set (the rescanCrops twin): a fresh
    // world scans to nothing — worldgen places no saplings (stateful
    // blocks are rejected at generator init, the ENT-2 invariant).
    rescanSaplings();
    rescanMoistureBlocks();
    rescanBeacons();
    rescanFlammableChunks();

    // The loaded columns mirror (streaming + unload decide on this set,
    // not per-frame chunk scans). The lighting pipeline has covered every
    // chunk that exists now (the fresh demo area, or the loaded save's
    // world — applySave ends with a full propagation); anything that
    // appears later is NEW (tracked by m_lightPendingColumns).
    for (const ChunkPos& c : m_world.chunkPositions()) {
        m_loadedColumns.insert({c.x, c.z});
    }

    // -- audio (Phase 15) ----------------------------------------------------
    // Graceful: with no audio device the bank stays disabled and every
    // play() is a no-op (the game is otherwise unaffected).
    m_sound.init(m_assets, "sounds",
                 m_config.getDouble("audio.volume", 0.7));

    MC_LOG_INFO("Scene built: " + std::to_string(m_blocks.count()) +
                " blocks, " + std::to_string(m_atlas.tileCount()) +
                " atlas tiles, " + std::to_string(m_world.chunkCount()) +
                " chunks (seed=" + std::to_string(m_generator->seed()) +
                ", spawn x=" + std::to_string(spawnX) + " z=" +
                std::to_string(spawnZ) + ", surface y=" +
                std::to_string(m_generator->surfaceHeight(spawnX, spawnZ)) +
                ", biome=" + m_generator->biomeNameAt(spawnX, spawnZ) + ")");
    return {};
}

// ---------------------------------------------------------------------------
// Save / load (Phase 13): seed + modifications + full game state
// ---------------------------------------------------------------------------

World::ChunkGenerator Game::makeChunkGenerator() const {
    // The generator is stateless and only writes into the given chunk
    // (never back into the World), which the World contract requires.
    // The SAME callback serves world creation and the save diff reference.
    return [this](Chunk& chunk, const ChunkPos& pos) {
        m_generator->generateChunk(chunk, pos);
    };
}

std::filesystem::path Game::savePath() const {
    return "data/saves/" + *m_args.saveSlot + ".json";
}

void Game::rescanCrops() {
    // Rebuild the growth tracking set from the world: every cell whose
    // block carries a non-empty growsTo. The block id IS the stage, so a
    // full rescan is exact (no per-instance state to restore). Runs at the
    // end of buildScene — a fresh world finds nothing (worldgen places no
    // crops), a loaded save finds the player's planted ones (restored by
    // the block edits).
    m_states.crops.clear();
    for (const ChunkPos& cp : m_world.chunkPositions()) {
        const Chunk* chunk = m_world.findChunk(cp);
        if (chunk == nullptr) {
            continue;
        }
        // A chunk is 16x16x16 (kChunkSize on every axis); the world's
        // height is stacked across ChunkPos.y (see FluidSim's origin math).
        // Local coordinates are [0, kChunkSize) on all three axes.
        const int baseX = cp.x * kChunkSize;
        const int baseY = cp.y * kChunkSize;
        const int baseZ = cp.z * kChunkSize;
        for (int ly = 0; ly < kChunkSize; ++ly) {
            for (int lz = 0; lz < kChunkSize; ++lz) {
                for (int lx = 0; lx < kChunkSize; ++lx) {
                    const BlockID id = chunk->get(lx, ly, lz);
                    if (m_blocks.at(id).properties.growsTo.empty()) {
                        continue;
                    }
                    const BlockPos wp {baseX + lx, baseY + ly, baseZ + lz};
                    m_states.crops[wp] = id;
                }
            }
        }
    }
}

void Game::rescanSaplings() {
    // GEN-2: the rescanCrops twin over the `tree` predicate. The STAGE is
    // not rescanned — it is the world's per-instance state byte (the
    // save's format-15 segment restored it with the block edits); this
    // set only decides which cells the growth engine rolls.
    m_states.saplings.clear();
    for (const ChunkPos& cp : m_world.chunkPositions()) {
        const Chunk* chunk = m_world.findChunk(cp);
        if (chunk == nullptr) {
            continue;
        }
        const int baseX = cp.x * kChunkSize;
        const int baseY = cp.y * kChunkSize;
        const int baseZ = cp.z * kChunkSize;
        for (int ly = 0; ly < kChunkSize; ++ly) {
            for (int lz = 0; lz < kChunkSize; ++lz) {
                for (int lx = 0; lx < kChunkSize; ++lx) {
                    const BlockID id = chunk->get(lx, ly, lz);
                    if (!m_blocks.at(id).hasTree()) {
                        continue;
                    }
                    const BlockPos wp {baseX + lx, baseY + ly, baseZ + lz};
                    m_states.saplings[wp] = id;
                }
            }
        }
    }
}

void Game::rescanMoistureBlocks() {
    // GEN-9: the rescanSaplings twin over the model.moisture_top
    // predicate. The WETNESS is not stored — it is derived at mesh time
    // from the live fluid field; the map value is SEEDED with the first
    // evaluation so a just-loaded farm only re-meshes on a real flip
    // (not on the load itself). Runs at the end of buildScene — a fresh
    // world scans to nothing (worldgen places no farmland), a loaded save
    // finds the player's placed ones (restored by the block edits).
    m_states.moistureBlocks.clear();
    for (const ChunkPos& cp : m_world.chunkPositions()) {
        const Chunk* chunk = m_world.findChunk(cp);
        if (chunk == nullptr) {
            continue;
        }
        const int baseX = cp.x * kChunkSize;
        const int baseY = cp.y * kChunkSize;
        const int baseZ = cp.z * kChunkSize;
        for (int ly = 0; ly < kChunkSize; ++ly) {
            for (int lz = 0; lz < kChunkSize; ++lz) {
                for (int lx = 0; lx < kChunkSize; ++lx) {
                    const BlockID id = chunk->get(lx, ly, lz);
                    if (m_blocks.at(id).model.moistureTop.empty()) {
                        continue;
                    }
                    const BlockPos wp {baseX + lx, baseY + ly, baseZ + lz};
                    m_states.moistureBlocks[wp] = soilIsWet(m_world, wp);
                }
            }
        }
    }
}

void Game::refreshMoistureStates() {
    // GEN-9: re-evaluate the wetness of every tracked moisture cell after
    // a fluid tick that changed cells, and mark the cell's chunk dirty
    // ONLY when the wetness flipped. Position-sorted iteration + the pure
    // soilIsWet predicate = deterministic (hard rule 8). The re-mesh
    // re-derives the top face from the now-updated fluid field (the
    // mesher's moisture hook), so the dirty mark is the whole effect.
    for (auto& [pos, wet] : m_states.moistureBlocks) {
        const bool nowWet = soilIsWet(m_world, pos);
        if (nowWet == wet) {
            continue;
        }
        wet = nowWet;
        m_world.markChunkDirty(worldToChunk(pos));
    }
}

void Game::placeGrownTree(const BlockPos& saplingPos, int height) {
    // GEN-2: one completed sapling -> a grown tree. The pure geometry
    // (planTree) was already FITS-probed by the growth step, so only
    // verified cells are written — trunk column first (the root cell
    // becomes the trunk base), canopy after. Every write goes through
    // the setBlock choke point (journal + tracking sets + edited-column
    // protection + the ENT-2 state seeding). The rolled height re-uses
    // the step's value (frame-independent — the probe and the placement
    // plan the SAME tree).
    const BlockID saplingId = m_world.getBlock(saplingPos.x, saplingPos.y,
                                               saplingPos.z);
    const auto it = m_states.treeSpecs.find(saplingId);
    if (it == m_states.treeSpecs.end()) {
        return; // defensive: an untracked sapling id (a stale step)
    }
    const TreeSpec& spec = it->second;
    int minX = saplingPos.x, minY = saplingPos.y, minZ = saplingPos.z;
    int maxX = saplingPos.x, maxY = saplingPos.y, maxZ = saplingPos.z;
    for (const TreeCell& c : planTree(height, spec)) {
        const int wx = saplingPos.x + c.offset.x;
        const int wy = saplingPos.y + c.offset.y;
        const int wz = saplingPos.z + c.offset.z;
        setBlock(wx, wy, wz, c.trunkCell ? spec.trunk : spec.leaves);
        minX = std::min(minX, wx); maxX = std::max(maxX, wx);
        minY = std::min(minY, wy); maxY = std::max(maxY, wy);
        minZ = std::min(minZ, wz); maxZ = std::max(maxZ, wz);
    }
    // The choke point already dropped the tracking entry when the root
    // cell became trunk (a non-`tree` block); the belt is deterministic.
    m_states.saplings.erase(saplingPos);
    // ONE relight over the tree's bounding box (the fluid-tick pattern,
    // not per-cell updateLocal): the trunk column + the canopy shadow
    // (LGT-2: the leaves' light_opacity 2 darkens what is under the
    // crown). The player triggered this — a one-time main-thread cost,
    // not a per-frame one (documented).
    Lighting::updateRegion(m_world, m_blocks,
                           BlockPos {minX, minY, minZ},
                           BlockPos {maxX, maxY, maxZ});
}

void Game::rescanFlammableChunks() {
    // The burn pass's gate: every chunk that holds a flamable block.
    m_flammableChunks.clear();
    for (const ChunkPos& cp : m_world.chunkPositions()) {
        const Chunk* c = m_world.findChunk(cp);
        if (c == nullptr) {
            continue;
        }
        const BlockID* blocks = c->data();
        for (int i = 0; i < Chunk::kVolume; ++i) {
            if (m_blocks.at(blocks[i]).properties.flamable) {
                m_flammableChunks.insert(cp);
                break;
            }
        }
    }
}

void Game::rescanBeacons() {
    // Phase 83: rebuild the beacon tracking set from the world — every cell
    // whose block carries a non-empty beaconEffect. A fresh world finds
    // nothing (worldgen places no beacons); a loaded save finds the
    // player's placed ones (restored by the block edits). No per-instance
    // state: the power level is re-derived from the world each frame.
    m_states.beacons.clear();
    for (const ChunkPos& cp : m_world.chunkPositions()) {
        const Chunk* chunk = m_world.findChunk(cp);
        if (chunk == nullptr) {
            continue;
        }
        const int baseX = cp.x * kChunkSize;
        const int baseY = cp.y * kChunkSize;
        const int baseZ = cp.z * kChunkSize;
        for (int ly = 0; ly < kChunkSize; ++ly) {
            for (int lz = 0; lz < kChunkSize; ++lz) {
                for (int lx = 0; lx < kChunkSize; ++lx) {
                    const BlockID id = chunk->get(lx, ly, lz);
                    if (m_blocks.at(id).properties.beaconEffects.empty()) {
                        continue;
                    }
                    const BlockPos wp {baseX + lx, baseY + ly, baseZ + lz};
                    m_states.beacons[wp] = id;
                }
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Init / main loop
// ---------------------------------------------------------------------------

Result<void> Game::init(const GameArgs& args) {
    m_args = args;

    // --- configuration ----------------------------------------------------
    if (!std::filesystem::exists(m_args.configPath)) {
        MC_LOG_INFO("Config not found at '" + m_args.configPath.string() +
                    "', creating with defaults");
        std::error_code ec;
        std::filesystem::create_directories(m_args.configPath.parent_path(), ec);
        std::ofstream out(m_args.configPath);
        if (!out) {
            return MC_FAIL("cannot create config file: '" +
                           m_args.configPath.string() + "'");
        }
        out << Config::defaultConfigJson() << '\n';
        out.close();
    }

    auto loaded = Config::load(m_args.configPath);
    MC_TRY(loaded);
    m_config = std::move(*loaded);
    if (m_args.seed >= 0) {
        m_config.set("world.seed", static_cast<long long>(m_args.seed));
    }
    if (m_args.time >= 0.0) {
        m_config.set("world.start_time_of_day", m_args.time);
    }

    const auto seed = m_config.getInt64("world.seed", 0);
    const int viewDistance = m_config.getInt("world.view_distance", 8);
    MC_LOG_INFO("Config loaded from '" + m_config.path().string() + "' (seed=" +
                std::to_string(seed) + ", view_distance=" +
                std::to_string(viewDistance) + ")");

    // --- assets -----------------------------------------------------------
    auto assetRoot = m_assets.setRoot("assets");
    MC_TRY(assetRoot);

    // --- window + GL context ----------------------------------------------
    auto window = m_window.init(m_config);
    MC_TRY(window);

    const char* vendor = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
    const char* glVersion = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    const char* renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    MC_LOG_INFO(std::string("GL ") + glVersion + " | " + vendor + " | " + renderer);

    // --- renderer + scene ---------------------------------------------------
    auto rendererInit = m_renderer.init();
    MC_TRY(rendererInit);

    auto scene = buildScene();
    MC_TRY(scene);

    m_initialized = true;
    return {};
}

int Game::run() {
    double lastFrame = nowSeconds();
    m_lastTitleUpdate = lastFrame;

    MC_LOG_INFO("Entering main loop (frames=" +
                (m_args.frames > 0 ? std::to_string(m_args.frames)
                                   : std::string("unlimited")) +
                ")");

    while (true) {
        m_input.frameBegin();
        m_input.pump();
        // ESC exits only while the pointer is free AND the inventory is
        // closed AND the player is alive (while captured it releases the
        // pointer, while the inventory is open it closes it, while dead
        // the death screen owns the input — all handled in update).
        if (m_input.quitRequested() ||
            (!m_window.relativeMouseMode() && !panelOpen() &&
             !m_player->isDead() &&
             m_input.wasPressed(GameKey::Escape))) {
            MC_LOG_INFO("Exit requested, shutting down");
            break;
        }

        const double frameStart = nowSeconds();
        double dt = frameStart - lastFrame;
        lastFrame = frameStart;
        if (dt < 0.0) {
            dt = 0.0;
        }
        // Clamp huge deltas (debugger pause, window drag) so one hiccup
        // cannot explode the simulation.
        constexpr double kMaxDt = 0.25;
        if (dt > kMaxDt) {
            dt = kMaxDt;
        }
        if (dt > 0.0) {
            const double fps = 1.0 / dt;
            m_fpsSmoothed =
                (m_fpsSmoothed <= 0.0) ? fps : (m_fpsSmoothed * 0.95 + fps * 0.05);
        }

        update(dt);
        render();
        m_window.swapBuffers();
        ++m_frameCount;

        if (m_args.frames > 0 && m_frameCount >= m_args.frames) {
            MC_LOG_INFO("Frame limit reached (" + std::to_string(m_frameCount) +
                        " frames), exiting");
            if (!m_args.screenshotPath.empty()) {
                takeScreenshot(m_args.screenshotPath);
            }
            break;
        }

        // Optional software FPS cap (in addition to vsync).
        const int fpsCap = m_config.getInt("render.fps_cap", 0);
        if (fpsCap > 0) {
            const double limit = 1.0 / static_cast<double>(fpsCap);
            const double elapsed = nowSeconds() - frameStart;
            if (elapsed < limit) {
                SDL_Delay(static_cast<Uint32>((limit - elapsed) * 1000.0));
            }
        }

        // Refresh the title (shows live FPS + draw calls) at most twice
        // per second. ASCII only: SDL's X11 backend silently REFUSES
        // non-ASCII titles ("Failed to convert title to UTF8") and the
        // meter would never appear (the em dash used to be here — the
        // FPS display was dead for every release).
        if (frameStart - m_lastTitleUpdate >= 0.5) {
            m_lastTitleUpdate = frameStart;
            m_window.setTitle("TokenMine.cpp - " +
                              std::to_string(static_cast<int>(
                                  std::lround(m_fpsSmoothed))) +
                              " FPS | " + std::to_string(m_renderer.stats().drawCalls) +
                              " draw calls");
        }
    }

    // Chunk streaming (Phase 14): drain the worker pool before the save
    // and shutdown — a job exception would surface in the flush, and the
    // reset joins the workers. In-flight generation columns are not
    // integrated (integration happens in update); that is correct, they
    // are pristine generated terrain that a save never records.
    if (m_pool) {
        m_pool->flush();
        m_pool.reset();
    }

    // Audio (Phase 15): close the stream/device before the window's
    // SDL_Quit (idempotent — the destructor does the same).
    m_sound.shutdown();

    // Save (Phase 13): persist the world state for the slot (opt-in via
    // --save, after the final frame — and after any screenshot, which is
    // taken inside the loop). A failed save is logged, not fatal: the
    // session itself ran cleanly.
    if (m_args.saveSlot.has_value()) {
        auto saved = writeSave();
        if (!saved.ok()) {
            MC_LOG_ERROR("Save failed: " + saved.error().describe());
        }
    }

    m_window.shutdown();
    m_initialized = false;
    MC_LOG_INFO("Game shut down cleanly");
    return 0;
}

} // namespace mc
