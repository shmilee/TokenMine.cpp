#include "core/game.hpp"

#include <GL/glew.h>
#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <numbers>
#include <sstream>
#include <unistd.h> // V3-12c2: the host's persistent listener's close

#include "block/block_sound.hpp"
#include "block/chance_drop.hpp"
#include "block/cube_face_table.hpp"
#include "core/game_local.hpp"
#include "core/json.hpp"
#include "core/png.hpp"
#include "entity/end_spawn_pool.hpp"
#include "entity/nether_spawn_pool.hpp"
#include "entity/barter_check.hpp"
#include "entity/wool_drops.hpp"
#include "item/repair.hpp"
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

// V2-2: the box reference for the per-face tile error messages (the part
// name, or the box index when the box is unnamed).
std::string boxRef(const MobBox& box, std::size_t index) {
    return box.name.empty() ? std::to_string(index) : box.name;
}

// V3-9: the record's default file name (data/demos/rec_<ts>.dmo — the
// wall clock names the file, never its CONTENT: the recording is
// byte-deterministic for the same session, the timestamp is the user's
// convenience, the PITS 15 second-run collision is avoided by the
// seconds).
std::string defaultRecordName() {
    std::time_t t = std::time(nullptr);
    std::tm tmv {};
    localtime_r(&t, &tmv);
    char buf[48];
    std::snprintf(buf, sizeof(buf), "rec_%04d%02d%02d_%02d%02d%02d.dmo",
                  tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday,
                  tmv.tm_hour, tmv.tm_min, tmv.tm_sec);
    return std::string(buf);
}

} // namespace

Game::~Game() {
    // V3-12c2: the host's persistent listener's close (the port's
    // release — the early-destruction's belt, the normal path's
    // process exit's twin).
    if (m_listenFd >= 0) {
        ::close(m_listenFd);
        m_listenFd = -1;
    }
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

// V3-12b: the starter inventory's per-session fill (the buildScene's
// hotbar.json block, extracted — the net connect's player-2 gets the
// same fresh-world gift, the assetFNV's parity's twin).
Result<void> Game::fillStarterInventory(PlayerSession& s) {
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
        s.inventory.setSlot(i, ItemStack {iid, m_items.at(iid).maxStack});
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
    for (const StarterSlot& st : *starterSlots) {
        s.inventory.setSlot(st.slot, ItemStack {st.item, st.count});
    }
    return {};
}

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
            capped = m_streaming.saveJournal.recordBlock(mu.pos);
        }
        // The displacement double report: a setBlock into a fluid cell
        // also recorded the drained cell in the fluid table.
        if (mu.fluidCode.has_value()) {
            capped = capped || m_streaming.saveJournal.recordFluid(mu.pos);
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

    // -- UX-9a clouds: dedicated per-pixel shader + ONE-TIME quad -------
    // The pattern (cloudMask ∧ cloudSubMask, the exact GLSL port of
    // world/cloud) is evaluated per fragment in cloud.frag — the quad
    // below is uploaded ONCE here and never again (the old per-16-block
    // window mesh rebuild hit the iGPU's ~140 ms per MB buffer-update
    // wall; this path costs zero uploads after init).
    auto cloudVert = m_assets.loadText("shaders/cloud.vert");
    MC_TRY(cloudVert);
    auto cloudFrag = m_assets.loadText("shaders/cloud.frag");
    MC_TRY(cloudFrag);
    auto cloudShader = ShaderProgram::create(*cloudVert, *cloudFrag);
    MC_TRY(cloudShader);
    m_cloudShader = std::move(*cloudShader);

    // kCloudQuadSize x kCloudQuadSize at y = kCloudHeight, local corners
    // (0,0)-(size,size) (the draw translates it to the camera-anchored
    // 4-block lattice origin). cloud.vert reads the position slot only;
    // the remaining MeshVertex fields stay zero (the unlit pass ignores
    // them — the ui.vert convention). Winding is irrelevant (the draw is
    // culling-off).
    const float cs = static_cast<float>(kCloudQuadSize);
    const float cy = static_cast<float>(kCloudHeight);
    const Vec3 cloudCorners[4] = {{0.0f, cy, 0.0f}, {cs, cy, 0.0f},
                                  {cs, cy, cs}, {0.0f, cy, cs}};
    std::vector<MeshVertex> cloudVerts;
    for (const Vec3& corner : cloudCorners) {
        MeshVertex v;
        v.position = corner;
        cloudVerts.push_back(v);
    }
    auto cloudUpload = m_cloudMesh.upload(cloudVerts, {0, 1, 2, 0, 2, 3});
    MC_TRY(cloudUpload);

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
    // V3-1 Phase A: the portal frame's two ids (the campfireLitId
    // pattern): the frame MATERIAL (obsidian — the named reference the
    // core never switches on by id, hard rule 3: the scan takes the
    // resolved id as a parameter) + the portal block itself (absent =
    // the whole portal system is structurally inert — no frame can ever
    // complete, no portal cell can exist in the tracking set).
    m_states.obsidianId = m_blocks.idForName("obsidian");
    m_states.portalId = m_blocks.idForName("nether_portal");
    // V3-1 Phase B: the nether's generation content (the obsidianId
    // pattern — init name lookup; the shipped asset set carries both, so
    // a stripped set names them invalid and the nether is unbuildable —
    // ensureNether's MC_FAIL at the first travel). The bedrock doubles
    // as the generated-portal's platform (the design's bedrock ruling);
    // the netherrack + the V3-2 Part 1 content family resolve at init
    // (buildScene's NetherGen::create — the nether module's vocabulary).
    m_states.bedrockId = m_blocks.idForName("bedrock");

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
    // V2-12 (format 27): the enchantment registry (the 7 data-driven
    // enchantments — the offer roll, the panel and the save cross-check
    // read it; loaded after the items so the class vocabulary is stable).
    auto enchantsLoad = m_enchants.loadDirectory(m_assets, "data/enchantments");
    MC_TRY(enchantsLoad);
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
        // V2-17: the per-state counts carry the SAME max-stack discipline
        // (one entry per state; the shipped double slab's 2 must fit).
        for (int c : b.properties.dropsCountByState) {
            if (c > m_items.at(dropId).maxStack) {
                return MC_FAIL("block '" + b.name +
                               "' state drops " + std::to_string(c) +
                               " of '" + b.properties.drops +
                               "' but its max stack is " +
                               std::to_string(m_items.at(dropId).maxStack));
            }
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
    MC_TRY(m_mobs.mobTypes.loadDirectory(m_assets, "data/mobs"));
    auto spawnText = m_assets.loadText("data/mob_spawning.json");
    MC_TRY(spawnText);
    auto spawnJson = Json::parse(*spawnText);
    MC_TRY(spawnJson);
    auto spawnCfg = MobSpawningConfig::parse(*spawnJson);
    MC_TRY(spawnCfg);
    m_mobs.spawnConfig = *spawnCfg;
    m_mobs.mobsEnabled = m_config.getBool("mobs.enabled", true);
    // ENT-1: the limb-swing enable flag (config mobs.limb_swing, default
    // true; false = the v0.9.0 static gait — the mobs.enabled pattern,
    // the E2E control group). Pure render layer: it gates nothing else.
    m_mobs.limbSwing = m_config.getBool("mobs.limb_swing", true);
    // Difficulty (Phase 25): strict — an unknown value fails init with a
    // message naming the field (the config default is "normal", which is
    // also the no-behavior-change baseline: scales 1.0 / cadence base).
    auto difficulty = parseDifficulty(
        m_config.getString("mobs.difficulty", "normal"));
    MC_TRY(difficulty);
    m_mobs.difficulty = *difficulty;
    for (const SpawnMobEntry& entry : m_mobs.spawnConfig.mobs) {
        if (m_mobs.mobTypes.findByName(entry.id) == nullptr) {
            return MC_FAIL("mob_spawning.json: unknown mob '" + entry.id +
                           "'");
        }
    }
    for (const MobID mid : m_mobs.mobTypes.allIds()) {
        const Mob& mob = m_mobs.mobTypes.at(mid);
        for (std::size_t bi = 0; bi < mob.model.boxes.size(); ++bi) {
            const MobBox& box = mob.model.boxes[bi];
            if (!m_atlas.contains(box.texture)) {
                return MC_FAIL("mob '" + mob.name + "': model texture '" +
                               box.texture + "' not in the atlas");
            }
            // V2-2: the per-face overrides must exist too (named box +
            // face — the model-texture pattern extended to `faces`).
            for (const auto& [fn, tile] : box.faces) {
                if (!m_atlas.contains(tile)) {
                    return MC_FAIL("mob '" + mob.name +
                                   "': model box '" + boxRef(box, bi) +
                                   "' face '" + fn + "' texture '" + tile +
                                   "' not in the atlas");
                }
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
        // V2-14: the ambient sound name (the bat's squeak — the data-
        // driven general mechanism) must name an EXISTING wav in the
        // sound directory. File-based on purpose (NOT the loaded bank —
        // the checkSoundMaterials discipline: the headless degradation
        // path keeps init strict, the unknown name is rejected by name).
        if (!mob.properties.ambientSoundName.empty()) {
            const std::string rel = "sounds/" +
                                    mob.properties.ambientSoundName +
                                    ".wav";
            if (!m_assets.exists(rel)) {
                return MC_FAIL("mob '" + mob.name + "' ambient_sound '" +
                                mob.properties.ambientSoundName +
                                "' is missing '" + rel + "'");
            }
        }
        // V3-14b: the shot's sound (the shoot cue's data-driven name —
        // the ambient_sound's existence twin, the checkSoundMaterials
        // file-based discipline).
        if (!mob.properties.shootSound.empty()) {
            const std::string rel = "sounds/" + mob.properties.shootSound +
                                    ".wav";
            if (!m_assets.exists(rel)) {
                return MC_FAIL("mob '" + mob.name + "' shoot_sound '" +
                                mob.properties.shootSound +
                                "' is missing '" + rel + "'");
            }
        }
    }

    // -- V2-15 (sheep): the wool_drops cross-check (the checkRepair-
    // Materials pure pattern — a wool-drops mob's data must be complete
    // against the dye vocabulary + the atlas: the "wool" drop entry, the
    // wool_weights' colors, the bare tile + every dye color's wool
    // variant tile. The first failing mob is named.)
    MC_TRY(checkWoolDrops(m_mobs.mobTypes, m_items, m_atlas.layout()));

    // V3-14c (piglin): the barter cross-check (the checkWoolDrops pure
    // pattern — a bartering mob's data must be complete against the item
    // registry: the input exists, every output exists, each output's
    // rolled top fits its stack. The first failing mob is named.)
    MC_TRY(checkBarter(m_mobs.mobTypes, m_items));

    // V3-10b (the ender pearl): the throw channel's cross-check (the
    // mob's shoot_sound's item twin — the throws_teleport item's two
    // sound names must exist in the sound directory; the first failing
    // item names, the S6 gate's subject).
    for (const ItemID iid : m_items.allIds()) {
        const Item& it = m_items.at(iid);
        if (!it.throwsTeleport) {
            continue;
        }
        if (it.throwSound.empty() || it.teleportSound.empty()) {
            return MC_FAIL("item '" + it.name +
                           "' carries 'throws_teleport' — the throw "
                           "channel requires BOTH 'throw_sound' and "
                           "'teleport_sound' (a one-sided cue is a dead "
                           "field)");
        }
        for (const std::string& snd : {it.throwSound, it.teleportSound}) {
            const std::string rel = "sounds/" + snd + ".wav";
            if (!m_assets.exists(rel)) {
                return MC_FAIL("item '" + it.name + "' throw sound '" +
                               snd + "' is missing '" + rel + "'");
            }
        }
    }

    // V3-10c (the enderman): the TELEPORT cue's cross-check (the
    // shoot_sound's mob-twin over the item's throw channel — ANY
    // teleport-capable mob (the data interval pair) requires the
    // landing's wav; the name is the generic teleport action's, not a
    // species switch — the S6 gate's missing-wav face).
    for (const MobID mid : m_mobs.mobTypes.allIds()) {
        const Mob& mob = m_mobs.mobTypes.at(mid);
        if (mob.properties.teleportIntervalMin <= 0.0) {
            continue; // no teleport (the pre-V3-10c bit root)
        }
        const std::string rel = "sounds/enderman_teleport.wav";
        if (!m_assets.exists(rel)) {
            return MC_FAIL("mob '" + mob.name +
                           "' carries a teleport interval — the "
                           "landing cue is missing '" + rel + "'");
        }
    }

    // V3-10d (the ender dragon): the boss's CUE + EFFECT cross-check
    // (the teleport-wav's mob-twin over the dragon family — ANY
    // dragon_flight mob requires the three cue wavs + the dragon_breath
    // effect, the S6's missing-wav / missing-effect faces). The core's
    // names are the generic boss action's (not a species switch).
    for (const MobID mid : m_mobs.mobTypes.allIds()) {
        const Mob& mob = m_mobs.mobTypes.at(mid);
        if (!mob.properties.dragonFlight) {
            continue; // no dragon (the pre-V3-10d bit root)
        }
        for (const char* cue : {"dragon_growl", "dragon_death",
                                "dragon_shoot"}) {
            const std::string rel = std::string("sounds/") + cue + ".wav";
            if (!m_assets.exists(rel)) {
                return MC_FAIL("mob '" + mob.name +
                               "' carries dragon_flight — the cue is "
                               "missing '" + rel + "'");
            }
        }
    }

    // -- player body (UX-3): the world-space body model ----------------------
    // data/player_body.json is a MODEL-ONLY file (exactly one top-level
    // 'model' key — no id / properties / AI fields, the mob file's
    // strictness without the behavior): it is parsed through the mob
    // model's strict parser (MobRegistry::parseModel — the full boxes /
    // swings discipline, labeled "player body model") and cross-checked
    // against the atlas with the same 5-line pattern as the mob textures
    // above. Render-only: the model loads no world/save state (zero
    // format change) and the first-person gate (m_cameraMode) keeps every
    // UX-3 run byte-identical (the structural-immunity root).
    auto playerBodyText = m_assets.loadText("data/player_body.json");
    MC_TRY(playerBodyText);
    auto playerBodyJson = Json::parse(*playerBodyText);
    MC_TRY(playerBodyJson);
    if (!playerBodyJson->isObject() ||
        playerBodyJson->members().size() != 1 ||
        playerBodyJson->find("model") == nullptr) {
        return MC_FAIL("player_body.json must contain exactly the "
                       "'model' section (no id / properties / AI fields)");
    }
    const Json* playerBodyModel = playerBodyJson->find("model");
    if (!playerBodyModel->isObject()) {
        return MC_FAIL("player body model must be a JSON object");
    }
    auto playerBodyParsed =
        MobRegistry::parseModel(*playerBodyModel, m_playerBody.model,
                                "player body model");
    MC_TRY(playerBodyParsed);
    m_playerBody.modelLoaded = true;
    for (std::size_t bi = 0; bi < m_playerBody.model.boxes.size(); ++bi) {
        const MobBox& box = m_playerBody.model.boxes[bi];
        if (!m_atlas.contains(box.texture)) {
            return MC_FAIL("player body: model texture '" + box.texture +
                           "' not in the atlas");
        }
        // V2-2: the per-face overrides must exist too (named box + face —
        // the same pattern as the mob loop above).
        for (const auto& [fn, tile] : box.faces) {
            if (!m_atlas.contains(tile)) {
                return MC_FAIL("player body: model box '" + boxRef(box, bi) +
                               "' face '" + fn + "' texture '" + tile +
                               "' not in the atlas");
            }
        }
    }
    // V2-6: the first-person arm (drawHeldHand's viewmodel) IS the
    // model's 'arm_r' box — a model without it can't render the arm.
    // Reject at init (the named-box discipline: the swings' part
    // references work the same way).
    bool hasArmR = false;
    for (const MobBox& box : m_playerBody.model.boxes) {
        if (box.name == "arm_r") {
            hasArmR = true;
            break;
        }
    }
    if (!hasArmR) {
        return MC_FAIL(
            "player body: the V2-6 first-person arm requires a box named "
            "'arm_r'");
    }

    // -- tutorial (UX-7 / V2-3): the tutorial series data -------------
    // assets/data/tutorials/ = the series collection (one JSON file per
    // series, the FILE-NAME BYTE ORDER is the series order — the
    // registry file-order paradigm). Per file: strict parse
    // (parseTutorial — unchanged) + the goal name halves cross-checked
    // against the registries (validateTutorialGoals — unchanged, the
    // grows_to precedent: the parse owns the grammar, init owns the
    // existence, the registry-order first failure names the step + the
    // missing name), then the step ids checked GLOBALLY unique across
    // the series (checkSeriesGlobalIds — the save's {series, id} pair
    // must name one step). Content lives in the JSON (hard rule 3):
    // the core never switches on a series or step id. Non-.json
    // entries are named and rejected; an empty directory is a named
    // failure (the content is mandatory). The enable flag is the
    // render.fog getBool precedent (code default true; the committed
    // config.json is untouched). The frame-locked state machine
    // (core/tutorial) + the format-25 save segment + the HUD target
    // line / toast / T series-skip are the rest of the feature; a
    // no-input run emits zero events (the base gate's structural
    // immunity).
    auto tutorialFiles = m_assets.listDir("data/tutorials");
    MC_TRY(tutorialFiles);
    if (tutorialFiles->empty()) {
        return MC_FAIL(
            "the tutorials directory 'data/tutorials' is empty (the "
            "series set must hold at least one .json file)");
    }
    std::vector<std::vector<TutorialStep>> tutorialSeries;
    for (const std::string& file : *tutorialFiles) {
        if (file.size() <= 5 || file.compare(file.size() - 5, 5,
                                             ".json") != 0) {
            return MC_FAIL("tutorials directory entry '" + file +
                           "' is not a .json series file");
        }
        auto tutorialText = m_assets.loadText("data/tutorials/" + file);
        MC_TRY(tutorialText);
        auto tutorialJson = Json::parse(*tutorialText);
        MC_TRY(tutorialJson);
        auto tutorialSteps = parseTutorial(*tutorialJson);
        MC_TRY(tutorialSteps);
        tutorialSeries.push_back(std::move(*tutorialSteps));
    }
    for (const std::vector<TutorialStep>& s : tutorialSeries) {
        MC_TRY(validateTutorialGoals(s, m_items, m_blocks, m_mobs.mobTypes));
    }
    MC_TRY(checkSeriesGlobalIds(tutorialSeries));
    m_tutorialSeries = std::move(tutorialSeries);
    // The enable flag (strict read — the SAVE-1 discipline: absent =
    // the code default true, a non-boolean = named init failure; the
    // Config getters' silent fallback is the trap): the committed
    // config.json carries no tutorial section (the render.fog
    // precedent).
    auto tutorialEnabledRead = tutorialEnabled(m_config);
    MC_TRY(tutorialEnabledRead);
    m_tutorialEnabled = *tutorialEnabledRead;

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
    // V3-10d (the ender dragon): the breath EFFECT's existence check (the
    // dragon_flight mob requires dragon_breath — the S6's missing-effect
    // face). Runs AFTER the effect load (the cue-wav check above runs
    // pre-load over m_assets.exists, the file-side).
    for (const MobID mid : m_mobs.mobTypes.allIds()) {
        const Mob& mob = m_mobs.mobTypes.at(mid);
        if (!mob.properties.dragonFlight) {
            continue; // no dragon (the pre-V3-10d bit root)
        }
        if (m_effects.findByName("dragon_breath") == nullptr) {
            return MC_FAIL("mob '" + mob.name +
                           "' carries dragon_flight — the breath "
                           "effect 'dragon_breath' is missing");
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
    for (const MobID mid : m_mobs.mobTypes.allIds()) {
        const Mob& mob = m_mobs.mobTypes.at(mid);
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
    m_states.portalSteam = m_config.getBool("portal.steam", true); // V3-15
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
    // V3-7: the save's on-disk encoding (save.compression — strict
    // read, the autoSaveIntervalSeconds + aoEnabled discipline; absent
    // = Gzip the user decision 11-04; the committed config.json
    // carries no save section = zero config change).
    {
        auto comp = saveCompression(m_config);
        MC_TRY(comp);
        m_saveCompression = *comp;
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
    // V2-18 AO: the mesh-time vertex-shading gate (render.ao — strict
    // read, the weatherEnabled / readNightSkyLevel discipline: absent =
    // true the code default, the committed config.json is untouched; a
    // non-boolean is named and rejected). OFF = the mesher's AO factor
    // is constant 1.0 + the mask's ao[4] stays zero = the pre-V2-19
    // mesh bit-identical (the gate's "new-off" control: the AO path is
    // gated at mesher construction — the shader, the 15-float vertex
    // format and the per-draw light overrides are untouched either way).
    auto aoOn = aoEnabled(m_config);
    MC_TRY(aoOn);
    m_aoEnabled = *aoOn;
    // UX-9c: the fully-occluded ambient floor (render.cave_ambient — a
    // CODE default, the fire/mobs/beacon/night_sky_level config-key
    // discipline: the committed config.json carries no such key).
    // Strict read: absent = kAmbientFloor (at the default night floor
    // both uploaded floors are 0.03f — the pre-UX-9c pipeline
    // bit-for-bit); present = a finite number in [0, 1] (0 = no
    // residual glow in caves / deep water / sealed rooms, 1 = full
    // brightness). No host-side consumer (the shader is the only one),
    // so no new Game member (YAGNI — unlike m_nightSkyLevel, whose
    // host-side pure functions read the floor).
    auto caveAmbient = readCaveAmbient(m_config);
    MC_TRY(caveAmbient);
    // The three uniforms are per-run constants (config-driven, no
    // per-draw variation): set them once on the basic program. At the
    // defaults (L = 7, cave_ambient = 0.03) the uploaded values
    // (8.0f/15.0f, 0.03f, 0.03f) are the pre-UX-9c named constants
    // bit-for-bit (the GLSL 8.0/15.0 and 0.03 float literals round to
    // the same IEEE values).
    m_basicShader.bind();
    m_basicShader.setUniform1f("u_nightRetreat",
                               nightRetreatScale(m_nightSkyLevel));
    m_basicShader.setUniform1f("u_ambient",
                               ambientFloorForLevel(m_nightSkyLevel));
    m_basicShader.setUniform1f("u_caveAmbient",
                               static_cast<float>(*caveAmbient));
    // V3-15: the portal's tile index (the fragment scroll's target —
    // a per-run constant: the atlas layout is fixed at init; the tile
    // is the Phase-A shipped block, its absence is an asset error).
    if (!m_atlas.layout().contains("nether_portal")) {
        return MC_FAIL("the nether_portal tile is missing from the "
                       "block atlas (the portal scroll's target)");
    }
    const auto& portalTile =
        m_atlas.layout().tiles.at("nether_portal");
    m_basicShader.setUniform2f("u_portalTile",
                               static_cast<float>(portalTile.col),
                               static_cast<float>(portalTile.row));
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

    // -- repair materials (UX-5): every item's 'repair_with' names an
    // EXISTING item (the ammo / grows_to discipline — a typo fails init
    // loudly) and only durable items are repairable (the pairing
    // discipline). The pure checkRepairMaterials (the CRAFT-3
    // checkSoundMaterials pattern).
    MC_TRY(checkRepairMaterials(m_items));

    // -- mob arrow ammo (Phase 42): a ranged mob's 'ammo' field names the
    // item a stuck arrow returns to the player's inventory on pickup (the
    // same cross-check pattern as the launcher ammo above).
    for (const MobID id : m_mobs.mobTypes.allIds()) {
        const Mob& def = m_mobs.mobTypes.at(id);
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
    auto r = fillStarterInventory(*m_sessions[0]);
    MC_TRY(r);

    // -- world generation (Phase 4: seeded, data-driven) ---------------------
    auto biomesLoad = m_biomes.loadDirectory(m_assets, "data/biomes");
    MC_TRY(biomesLoad);
    // V3-2 Part 1: the nether's INDEPENDENT biome pool (D1: a second
    // registry instance over data/biomes_nether — the overworld pool
    // stays untouched, the nether's selection walks this pool over the
    // derived seed). A missing / empty directory is a named content
    // regression (the overworld pool's non-empty discipline at create()
    // — the nether's twin, so a stripped asset set fails init loudly
    // instead of degrading the nether to the v3.4.0 slab silently).
    auto netherBiomesLoad =
        m_netherBiomes.loadDirectory(m_assets, "data/biomes_nether");
    MC_TRY(netherBiomesLoad);
    if (m_netherBiomes.count() == 0) {
        return MC_FAIL("nether world generation requires at least one "
                       "biome (assets/data/biomes_nether/*.json)");
    }
    // V3-10b: the End's biome pool (the nether load's twin — the
    // fail-fast: a missing directory / an empty pool names the asset,
    // the S6 gate's subject; 10b ships the single end_plains).
    auto endBiomesLoad =
        m_endBiomes.loadDirectory(m_assets, "data/biomes_end");
    MC_TRY(endBiomesLoad);
    if (m_endBiomes.count() == 0) {
        return MC_FAIL("end world generation requires at least one "
                       "biome (assets/data/biomes_end/*.json)");
    }
    auto worldgen = WorldgenParams::load(m_assets);
    MC_TRY(worldgen);
    const std::int64_t seed = m_config.getInt64("world.seed", 0);
    auto gen = WorldGenerator::create(seed, *worldgen, m_biomes, m_blocks);
    MC_TRY(gen);
    m_generator = std::move(*gen);
    // V3-2 Part 1: the nether's content resolution — FAIL-FAST at init
    // (the WorldGenerator::create's twin: the biome surfaces' block
    // existence + the stateful rejection run HERE, so a bad nether biome
    // file rejects startup with the init's naming, not the first travel
    // — the S6 timing ruling). The result is cached in m_nether.gen; the
    // lazy D2 is the nether World itself (the chunks / meshes / journal /
    // streaming — ensureNether), NOT the content resolution.
    auto netherGen =
        NetherGen::create(netherSeed(seed), m_netherBiomes, m_blocks);
    MC_TRY(netherGen);
    m_nether.gen = std::move(*netherGen);
    // V3-3: the NETHER'S mob spawn pool (data/mob_spawning_nether.json —
    // the Phase B "Phase C fills it" slot). The pure resolution (the
    // file load + the shared strict schema + the vocabulary cross-check
    // against the SHARED type table — the loadNetherSpawnPool, the
    // checkWoolDrops pattern) FAILS FAST at init (the NetherGen::create
    // S6 timing ruling: a missing file / an unknown mob name rejects
    // startup with the naming, not the first travel). The dimension's
    // MobSystem gets the pool + the overworld's session policy (the
    // enable flag / difficulty / limb swing — the player's config is
    // one game-wide: a mobs.enabled off run stays mob-free in BOTH
    // dimensions, the bit-identical no-input baseline's twin). The
    // overworld pool (m_mobs.spawnConfig, the file above) is UNTOUCHED
    // (the zero-touch red line — the structural-immunity root: the
    // nether's pool is consumed only while playerInNether, the
    // attemptMobSpawn's dimension gate).
    auto netherPool = loadNetherSpawnPool(m_assets, m_mobs.mobTypes);
    MC_TRY(netherPool);
    m_nether.mobs.spawnConfig = *netherPool;
    m_nether.mobs.mobsEnabled = m_mobs.mobsEnabled;
    m_nether.mobs.difficulty = m_mobs.difficulty;
    m_nether.mobs.limbSwing = m_mobs.limbSwing;

    // V3-10c: the END's mob spawn pool (data/mob_spawning_end.json —
    // the 10a "two-phase slot"'s consumer, the loadNetherSpawnPool's
    // third consumption: the fail-fast timing ruling — a missing file /
    // unknown mob name rejects STARTUP, not the first travel). 10c
    // ships the single-species pool (enderman — the End's only mob);
    // the overworld / nether pools stay enderman-free (the End-only
    // deviation, the 10d dragon's era re-discusses).
    auto endPool = loadEndSpawnPool(m_assets, m_mobs.mobTypes);
    MC_TRY(endPool);
    m_end.mobs.spawnConfig = *endPool;
    m_end.mobs.mobsEnabled = m_mobs.mobsEnabled;
    m_end.mobs.difficulty = m_mobs.difficulty;
    m_end.mobs.limbSwing = m_mobs.limbSwing;

    // V3-10a: the END's generation content (the NetherGen::create's third
    // consumption — the four minimum names resolved once; the fail-fast
    // timing ruling: a missing / stateful name rejects STARTUP, not the
    // first travel).
    auto endGen = EndGen::create(endSeed(seed), m_endBiomes, m_blocks);
    MC_TRY(endGen);
    m_end.gen = std::move(*endGen);
    // The End portal block's resolved id (the m_states.portalId pattern —
    // absent = the whole End portal system is structurally inert: no
    // ring can ever activate, no portal cell can exist in the tracking
    // sets, the rescan / entry countdown fast-exit).
    m_states.endPortalId = m_blocks.idForName("end_portal");
    m_states.endFrameEyedId = m_blocks.idForName("end_portal_frame_eyed");
    // V3-10d: the dragon arena's resolved ids (the endPortalId's twins —
    // EndGen::create above just fail-fasted their existence, so a
    // failure here is the belt's unreachable branch: the belt names it
    // rather than degrading, the P70 family pattern).
    m_states.endCrystalId = m_blocks.idForName("end_crystal");
    m_states.dragonEggId = m_blocks.idForName("dragon_egg");
    if (!m_states.endCrystalId.valid() || !m_states.dragonEggId.valid()) {
        return MC_FAIL("dragon arena: EndGen resolved but the game's id "
                       "lookup failed (the belt's unreachable branch)");
    }
    // The ender_eye item (the embed interaction's held-item predicate —
    // the resolved id, the m_states.portalId pattern; invalid = the
    // interaction is structurally inert).
    m_enderEyeId = m_items.idForName("ender_eye");

    // -- V2-4 weather (rain/thunder): the session-derived state machine --
    // NOT saved (zero format, the cloud-drift precedent class): every run
    // restarts the machine — the first segment is ALWAYS CLEAR with a
    // duration >= kClearMin (9000 frames = 150 s), so the 60-frame no-
    // input baseline never sees rain (the base gate's structural
    // immunity). render.weather (strict read, the SAVE-1 discipline):
    // absent = true (the code default, the render.clouds precedent — the
    // committed config.json carries no such key); a non-boolean is named
    // and rejected at init. OFF = the machine never runs + zero drops +
    // zero interaction = the v2.3.0 pipeline bit-identical (the gate's
    // "new-off" control). The streams are seeded from the WORLD seed
    // (same world, same weather sequence across runs); the thunder runs
    // on the dedicated kSaltWeatherThunder stream (the independent-salt
    // decision). The --weather override touches the INITIAL STATE only
    // (rain = the fixed-kRainMin opening segment, then the streams take
    // over — the verbatim init logs are the E2E evidence).
    auto weatherEnabledRead = weatherEnabled(m_config);
    MC_TRY(weatherEnabledRead);
    m_weatherEnabled = *weatherEnabledRead;
    if (m_weatherEnabled) {
        m_weatherStream = static_cast<std::uint64_t>(m_generator->seed());
        m_weatherThunderStream = m_weatherStream + kSaltWeatherThunder;
        const bool startRaining =
            m_args.weather.has_value() && *m_args.weather;
        m_weather = weatherInitialState(
            startRaining, m_weatherStream, m_weatherThunderStream);
        if (m_args.weather.has_value()) {
            MC_LOG_INFO(std::string("Weather override: ") +
                        (startRaining ? "rain" : "clear"));
        }
        if (startRaining) {
            // The rain is live from frame 0 (the design's verbatim line —
            // the natural entry's frame-locked event log is the machine's
            // in updateWeather).
            MC_LOG_INFO("Weather: rain started");
        }
    }

    // V2-4: the rain shader program (the cloud program's fourth
    // precedent — a dedicated program; the basic/sky/ui/cloud programs
    // are untouched) + the STATIC kRainDropCount-drop mesh (the per-drop
    // spawn offset + fall phase, uploaded ONCE here, zero per-frame
    // upload: the vertex shader re-derives every drop's position from
    // the frame-bounded fall offset + the player-anchored box origin —
    // the CPU oracle is rainDropPosition in core/weather, the unit
    // tests own it). render.weather OFF / clear weather never draws any
    // of this (the v2.3.0 pipeline bit-identical).
    auto rainVert = m_assets.loadText("shaders/rain.vert");
    MC_TRY(rainVert);
    auto rainFrag = m_assets.loadText("shaders/rain.frag");
    MC_TRY(rainFrag);
    auto rainShader = ShaderProgram::create(*rainVert, *rainFrag);
    MC_TRY(rainShader);
    m_rainShader = std::move(*rainShader);
    {
        // The bar-quad corners (0.02 x 0.5, the X-Y plane — a vertical
        // strip; culling off makes both sides render, the thin edge-on
        // case is sub-pixel). a_position carries the drop's spawn offset
        // (the same for all four corners), a_normal the corner offset,
        // a_uv.x the fall phase [0, 1) (the rain.vert contract).
        const float halfW = kRainDropWidth * 0.5f;
        const float halfL = kRainDropLength * 0.5f;
        const Vec3 corners[4] = {{-halfW, -halfL, 0.0f}, {halfW, -halfL, 0.0f},
                                 {halfW, halfL, 0.0f}, {-halfW, halfL, 0.0f}};
        std::vector<MeshVertex> verts;
        std::vector<std::uint32_t> indices;
        verts.reserve(static_cast<std::size_t>(kRainDropCount) * 4);
        indices.reserve(static_cast<std::size_t>(kRainDropCount) * 6);
        for (const RainDrop& drop : makeRainDrops(m_generator->seed())) {
            const std::uint32_t first =
                static_cast<std::uint32_t>(verts.size());
            for (const Vec3& corner : corners) {
                MeshVertex v;
                v.position = drop.offset;
                v.normal = corner;
                v.uv = {drop.phase, 0.0f};
                verts.push_back(v);
            }
            indices.insert(indices.end(),
                           {first, first + 1, first + 2, first, first + 2,
                            first + 3});
        }
        auto rainUpload = m_rainMesh.upload(verts, indices);
        MC_TRY(rainUpload);
    }

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
    m_streaming.pool = std::make_unique<ThreadPool>(kStreamWorkerCount);
    m_streaming.streamRadius = m_config.getInt("world.view_distance", 8);

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
    // Font-only scale (UX-8e): text-only zoom in renderUi (icons and
    // panel geometry stay put); the same strict-read contract.
    const auto fontScale = uiFontScaleFromConfig(m_config);
    MC_TRY(fontScale);
    m_fontScale = *fontScale;
    const Player::Params& playerDims = *playerParams; // room check
    // V3-12b (R9): the per-INDEX spawn (the i-th valid kSpawnCandidates
    // entry — the two players never share a cell; the world spawn / the
    // spawn-protection anchor is index 0, the pre-V3-12b value). The
    // candidate scan is the original loop, collected (the room-check is
    // the same pure predicate over the same world, so index 0 is the
    // bit-identical first valid). A missing index wraps to 0 (fewer than
    // i valid candidates — the single-player world's shape).
    std::vector<Vec3> spawnCandidates;
    int spawnX = 0;
    int spawnZ = 0;
    {
        constexpr int kSpawnCandidates[][2] = {
            {0, 0}, {4, 0}, {-4, 0}, {8, 0}, {-8, 0},
            {0, 4}, {0, -4}, {4, 4}, {-4, -4}, {8, 8},
        };
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
                spawnCandidates.push_back(cand);
                if (spawnCandidates.size() == 1) {
                    spawnX = c[0];
                    spawnZ = c[1];
                }
            }
        }
    }
    const Vec3 spawnPos =
        spawnCandidates.empty()
            ? Vec3 {0.5f,
                    static_cast<float>(m_generator->surfaceHeight(0, 0)) + 1.0f,
                    0.5f}
            : spawnCandidates[0];
    // V3-12b (R9): keep the valid-candidate list for the net connect (the
    // second player takes index 1 — the spawnPositionForIndex's wrap).
    m_spawnCandidates = spawnCandidates;
    // The WORLD spawn (Phase 25): the center of the hostile spawn-protection
    // circle. Captured BEFORE the save load — a loaded save moves the
    // player, but the protection stays anchored to the world spawn (the
    // Minecraft semantics). V2-13: it is ALSO the fresh-world respawn
    // point (the player is constructed at spawnPos above — format 28's
    // player.respawn writes it for a never-slept player) and the fallback
    // of the respawn-time bed validation (respawnPlayer — a remembered
    // bed that no longer validates reverts the spawn to here).
    m_mobs.spawnPoint = spawnPos;
    // V3-12b: the primary (index 0) player — the m_player alias's single
    // set site (the net session adds the remote player at the connect).
    m_players.push_back(
        std::make_unique<Player>(m_world, m_blocks, spawnPos, *playerParams));
    m_player = m_players[0].get();
    // V2-12 (format 27): the FRESH-world xp seed (worldSeed ^ kXpSeedInit —
    // deterministic). A loaded save REPLACES it in applySave (the saved
    // value, or the same migration for a pre-27 file), so this only
    // affects new worlds. The world seed (static after generation) is also
    // handed to the player for the unbreaking wear-roll state.
    m_player->setXpSeed(xpSeedInit(static_cast<std::uint64_t>(seed)));
    m_player->setWorldSeed(static_cast<std::uint64_t>(seed));
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
    // V3-1 Phase A: the portal load-time frame validity (the rescanBeacons
    // twin + the orphan air-fix): a fresh world scans to nothing (worldgen
    // rejects the stateful portal block — the structural-immunity root),
    // a loaded save finds the player's portals (the block edits + the
    // format-15 states restored them).
    rescanPortals();
    // V3-1 Phase B (D9's load ordering, the two-dimension portal rescan):
    // the nether's frame validity (the rescanPortals twin over the nether
    // world — a no-op while the nether is unengaged: a no-nether load, or
    // a fresh world, never scans; a nether load finds the player's nether
    // portals — the nether's block edits + states restored them).
    rescanNetherPortals();
    // V3-10a (D9's load ordering, the THREE-dimension portal rescan):
    // the End's frame validity over BOTH End-side sets (the exit in the
    // End world — a no-op while the End is unengaged, the lazy D2; the
    // entry in the overworld — the loaded save's block edits restored
    // the frames + the eyes).
    rescanEndPortals();

    // The loaded columns mirror (streaming + unload decide on this set,
    // not per-frame chunk scans). The lighting pipeline has covered every
    // chunk that exists now (the fresh demo area, or the loaded save's
    // world — applySave ends with a full propagation); anything that
    // appears later is NEW (tracked by m_lightPendingColumns).
    for (const ChunkPos& c : m_world.chunkPositions()) {
        m_streaming.loadedColumns.insert({c.x, c.z});
    }
    // V3-1 Phase B: the nether's loaded-columns mirror (the overworld's
    // twin — the nether's chunks the load's replay + catch-up disk
    // generated; the nether's streaming / unload decide on this set). A
    // no-nether load leaves the mirror empty (the lazy D2 — the
    // structural-immunity root).
    if (m_nether.world) {
        for (const ChunkPos& c : m_nether.world->chunkPositions()) {
            m_nether.streaming.loadedColumns.insert({c.x, c.z});
        }
    }
    // V3-10a: the End's loaded-columns mirror (the nether's twin — the
    // End's chunks the load's replay + catch-up disk generated; a
    // no-End load leaves the mirror empty, the lazy D2).
    if (m_end.world) {
        for (const ChunkPos& c : m_end.world->chunkPositions()) {
            m_end.streaming.loadedColumns.insert({c.x, c.z});
        }
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

// ---------------------------------------------------------------------------
// V3-1 Phase B: the nether dimension (the incremental parallel family —
// D1: the overworld members stay untouched; the nether is the m_nether
// aggregate, engaged lazily by ensureNether — D2).
// ---------------------------------------------------------------------------

Game::Dimension Game::dimensionOfWorld(const World& w) const {
    if (m_nether.world && &w == &*m_nether.world) {
        return Dimension::Nether;
    }
    if (m_end.world && &w == &*m_end.world) {
        return Dimension::End;
    }
    return Dimension::Overworld;
}

World& Game::currentWorld() {
    // V3-10a: the three-way (the nether's second consumption — the End
    // leg is the playerInEnd branch; the overworld keeps its exact
    // pre-V3-10a expression).
    if (playerInNether()) {
        return *m_nether.world;
    }
    if (playerInEnd()) {
        return *m_end.world;
    }
    return m_world;
}

const World& Game::currentWorld() const {
    if (playerInNether()) {
        return *m_nether.world;
    }
    if (playerInEnd()) {
        return *m_end.world;
    }
    return m_world;
}

std::vector<std::unique_ptr<Entity>>& Game::currentEntities() {
    if (playerInNether()) {
        return m_nether.entities;
    }
    if (playerInEnd()) {
        return m_end.entities;
    }
    return m_entities;
}

const std::vector<std::unique_ptr<Entity>>& Game::currentEntities() const {
    if (playerInNether()) {
        return m_nether.entities;
    }
    if (playerInEnd()) {
        return m_end.entities;
    }
    return m_entities;
}

const std::map<BlockPos, BlockID>& Game::portalsOf(Dimension dim) const {
    // The dimension's PORTAL tracking set (the overworld's nether
    // portals / the nether's own / the End's exit — the end ENTRY set
    // is m_states.endPortals, the overworld's per-block family).
    if (dim == Dimension::Nether) {
        return m_nether.portals;
    }
    if (dim == Dimension::End) {
        return m_end.portals;
    }
    return m_states.portals;
}

std::map<BlockPos, BlockID>& Game::portalsOf(Dimension dim) {
    if (dim == Dimension::Nether) {
        return m_nether.portals;
    }
    if (dim == Dimension::End) {
        return m_end.portals;
    }
    return m_states.portals;
}

std::int64_t Game::currentSeed() const {
    // The dimension's generation seed (the nether's / the End's are
    // DERIVED — the kSaltNether / kSaltEnd discipline; neither is
    // stored, the parse rebuilds both from the overworld seed, the
    // xp_seed single-source-of-truth twin).
    const std::int64_t seed = m_generator->seed();
    if (playerInNether()) {
        return netherSeed(seed);
    }
    if (playerInEnd()) {
        return endSeed(seed);
    }
    return seed;
}

void Game::ensureNether() {
    // D2 (the laziness): the nether World is constructed on first need —
    // before that there is no nether chunk, no mesh, no journal, no
    // streaming job, no streaming memory (the structural-immunity
    // strongest form: a no-travel session never allocates any of it, and
    // the base gate's byte-identity rests on it). Idempotent.
    if (m_nether.world) {
        return;
    }
    // V3-2 Part 1: the content resolution runs at INIT (buildScene's
    // NetherGen::create — the fail-fast timing ruling); ensureNether
    // only consumes the cached gen. An invalid cache is structurally
    // impossible (init MC_TRYs the create — the game never boots with a
    // bad nether gen): the belt over the belt keeps the travel pipeline's
    // named-abort path for the unreachable case.
    if (!m_nether.gen.valid()) {
        MC_LOG_ERROR("The nether dimension is unbuildable: the generation "
                     "content failed to resolve at init (structurally "
                     "unreachable — buildScene MC_TRYs the resolution)");
        return;
    }
    // The SKYLESS world (D5: the void / world top read sky 0, the rescan
    // top source 0 — the day/night shader term and the night floor are
    // structurally inert in the nether).
    m_nether.world.emplace(/*skyless=*/true);
    m_nether.world->setChunkGenerator(makeNetherChunkGenerator());
    // The nether's cell-mutation observer (the overworld's journal
    // observer's twin — the nether's edits journal into the nether's
    // StreamingState, so the exit save's nether section diffs against the
    // nether's generation reference).
    m_nether.world->setCellMutationObserver([this](const CellMutation& mu) {
        bool capped = false;
        if (mu.block.valid()) {
            capped = m_nether.streaming.saveJournal.recordBlock(mu.pos);
        }
        if (mu.fluidCode.has_value()) {
            capped = capped ||
                     m_nether.streaming.saveJournal.recordFluid(mu.pos);
        }
        if (capped) {
            MC_LOG_INFO("Nether save journal exceeded "
                        "kSaveJournalMaxEntries (" +
                        std::to_string(kSaveJournalMaxEntries) +
                        " entries) — the exit save's nether section falls "
                        "back to the full nether diff");
        }
    });
    // The streaming radius (the shared config value — no new config key;
    // the nether streams the same view_distance disk as the overworld).
    m_nether.streaming.streamRadius = m_streaming.streamRadius;
    MC_LOG_INFO("Nether engaged (derived seed " +
                std::to_string(netherSeed(
                    static_cast<std::int64_t>(m_generator->seed()))) +
                " = world seed ^ kSaltNether)");
}

World& Game::netherWorld() {
    // The belt (every consumer runs only once the nether is engaged —
    // ensureNether at the travel / load sites; an unengaged world here is
    // a logic error, not a runtime state).
    if (!m_nether.world) {
        MC_LOG_ERROR("netherWorld() accessed before ensureNether (logic "
                     "error — the nether is lazy)");
    }
    return *m_nether.world;
}

World::ChunkGenerator Game::makeNetherChunkGenerator() const {
    // The nether's generation reference (the makeChunkGenerator twin):
    // the pure fillNetherChunk (coordinate-pure — zero noise, the
    // constructive determinism). Streaming jobs, first-touch creation,
    // the save-diff reference and the unload retained diffs all run the
    // same fill.
    const NetherGen gen = m_nether.gen; // captured by value (the closure
                                        // outlives nothing — the gen is
                                        // set before any job posts)
    return [gen](Chunk& chunk, const ChunkPos& pos) {
        fillNetherChunk(gen, chunk, pos);
    };
}

// ---------------------------------------------------------------------------
// V3-10a: the End dimension (the nether family's third member — D2's
// laziness, D5's skyless world, the ensureNether / makeNetherChunkGenerator
// twins).
// ---------------------------------------------------------------------------

void Game::ensureEnd() {
    // D2 (the laziness): the End World is constructed on first need —
    // before that there is no End chunk, no mesh, no journal, no
    // streaming job, no streaming memory (the structural-immunity
    // strongest form: a no-End session never allocates any of it, and
    // the base gate's byte-identity rests on it). Idempotent.
    if (m_end.world) {
        return;
    }
    // V3-10a: the content resolution runs at INIT (buildScene's
    // EndGen::create — the fail-fast timing ruling, the nether's twin);
    // ensureEnd only consumes the cached gen (the belt's named-abort
    // keeps the travel pipeline's unreachable-case path).
    if (!m_end.gen.valid()) {
        MC_LOG_ERROR("The End dimension is unbuildable: the generation "
                     "content failed to resolve at init (structurally "
                     "unreachable — buildScene MC_TRYs the resolution)");
        return;
    }
    // The SKYLESS world (the nether's D5 twin: the void / world top read
    // sky 0 — no day/night, no weather, the render's 6-point gate does
    // not consume them).
    m_end.world.emplace(/*skyless=*/true);
    m_end.world->setChunkGenerator(makeEndChunkGenerator());
    // The End's cell-mutation observer (the nether's journal twin — the
    // End's edits journal into the End's StreamingState, so the exit
    // save's end section diffs against the End's generation reference).
    m_end.world->setCellMutationObserver([this](const CellMutation& mu) {
        bool capped = false;
        if (mu.block.valid()) {
            capped = m_end.streaming.saveJournal.recordBlock(mu.pos);
        }
        if (mu.fluidCode.has_value()) {
            capped = capped ||
                     m_end.streaming.saveJournal.recordFluid(mu.pos);
        }
        if (capped) {
            MC_LOG_INFO("End save journal exceeded "
                        "kSaveJournalMaxEntries (" +
                        std::to_string(kSaveJournalMaxEntries) +
                        " entries) — the exit save's end section falls "
                        "back to the full End diff");
        }
    });
    // The streaming radius (the shared config value — no new config key;
    // the End streams the same view_distance disk as the overworld).
    m_end.streaming.streamRadius = m_streaming.streamRadius;
    MC_LOG_INFO("End engaged (derived seed " +
                std::to_string(endSeed(
                    static_cast<std::int64_t>(m_generator->seed()))) +
                " = world seed ^ kSaltEnd)");
}

World& Game::endWorld() {
    // The belt (every consumer runs only once the End is engaged —
    // ensureEnd at the travel / load sites; an unengaged world here is
    // a logic error, not a runtime state).
    if (!m_end.world) {
        MC_LOG_ERROR("endWorld() accessed before ensureEnd (logic error "
                     "— the End is lazy)");
    }
    return *m_end.world;
}

World::ChunkGenerator Game::makeEndChunkGenerator() const {
    // The End's generation reference (the makeNetherChunkGenerator's
    // third member): the pure fillEndChunk (coordinate-pure — zero
    // noise, the constructive determinism). The save-diff reference and
    // the unload retained diffs all run the same fill.
    const EndGen gen = m_end.gen; // captured by value (the closure
                                   // outlives nothing — the gen is set
                                   // before any job posts)
    return [gen](Chunk& chunk, const ChunkPos& pos) {
        fillEndChunk(gen, chunk, pos);
    };
}

void Game::catchUpDisk(Dimension dim, int cx, int cz) {
    // D6 step 3 (the synchronous catch-up): the 3x3 column disk around
    // (cx, cz) — every missing column generated SYNCHRONOUSLY (the
    // first-touch precedent amplified: ensureColumn + the loaded mirror)
    // + ONE synchronous relight over the disk's bounding box (newTerrain
    // — the sky propagation runs even when the rescan writes nothing;
    // the nether's skyless source 0 through the rescan). The ~ms one-time
    // cost (9 columns x the pure fill + one relight box) — the design's
    // "一次性 ~ms 级". Runs in the main-thread-writer window (the travel
    // frame / the load), so no worker is in flight and no window
    // conflict (the risk-surface ② ruling).
    // V3-10a: the three-way (the End disk's synchronous catch-up — the
    // nether's branch untouched, the overworld keeps its exact
    // pre-V3-10a expression).
    World* wPtr = &m_world;
    StreamingState* stPtr = &m_streaming;
    if (dim == Dimension::Nether) {
        wPtr = &*m_nether.world;
        stPtr = &m_nether.streaming;
    } else if (dim == Dimension::End) {
        wPtr = &*m_end.world;
        stPtr = &m_end.streaming;
    }
    World& w = *wPtr;
    StreamingState& st = *stPtr;
    bool any = false;
    for (int dx = -1; dx <= 1; ++dx) {
        for (int dz = -1; dz <= 1; ++dz) {
            const int x = cx + dx;
            const int z = cz + dz;
            if (!w.columnComplete(x, z)) {
                w.ensureColumn(x, z);
                st.loadedColumns.insert({x, z});
                any = true;
            }
        }
    }
    if (any) {
        // The disk's bounding box (the 3x3 columns' world extent — the
        // relight's ±R expansion covers the neighborhood's light flow).
        const BlockPos min {cx * kChunkSize - kChunkSize, 0,
                            cz * kChunkSize - kChunkSize};
        const BlockPos max {(cx + 2) * kChunkSize - 1, kWorldHeight - 1,
                            (cz + 2) * kChunkSize - 1};
        Lighting::updateRegion(w, m_blocks, min, max, true);
    }
}

void Game::rescanNetherPortals() {
    // The rescanPortals twin over the nether world (the load-time frame
    // validity — the PITS 383① two-way cross-check): rebuild the nether's
    // portal tracking set from the world, and every portal cell not
    // strictly inside a complete frame (an orphan — a hand-edited save,
    // or the frame outlived its world) is set to air. A no-op while the
    // nether is unengaged (the lazy D2 — the load engages it first when
    // its section is non-empty or the player sits in it).
    if (!m_nether.world) {
        return;
    }
    m_nether.portals.clear();
    if (!m_states.portalId.valid() || !m_states.obsidianId.valid()) {
        return; // the structural-inert no-op (the rescanPortals twin)
    }
    World& w = *m_nether.world;
    std::vector<BlockPos> orphans;
    for (const ChunkPos& cp : w.chunkPositions()) {
        const Chunk* chunk = w.findChunk(cp);
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
                    if (id != m_states.portalId) {
                        continue;
                    }
                    const BlockPos wp {baseX + lx, baseY + ly, baseZ + lz};
                    if (portalValidFrame(w, wp, m_states.obsidianId,
                                         m_states.fireId, m_states.portalId) ==
                        std::nullopt) {
                        orphans.push_back(wp);
                        continue;
                    }
                    m_nether.portals[wp] = id;
                }
            }
        }
    }
    if (orphans.empty()) {
        return;
    }
    BlockPos min {orphans.front()};
    BlockPos max {orphans.front()};
    for (const BlockPos& p : orphans) {
        w.setBlock(p.x, p.y, p.z, BlockID::air());
        min.x = std::min(min.x, p.x); min.y = std::min(min.y, p.y);
        min.z = std::min(min.z, p.z);
        max.x = std::max(max.x, p.x); max.y = std::max(max.y, p.y);
        max.z = std::max(max.z, p.z);
    }
    // ONE relight over the orphans' bounding box (the rescanPortals
    // twin — the portal is a light-11 source).
    Lighting::updateRegion(w, m_blocks, min, max);
    MC_LOG_INFO("Nether portal rescan: " + std::to_string(orphans.size()) +
                " orphan portal block(s) set to air (no complete frame)");
}

// V3-10a: the End portal's load-time rescan — the rescanNetherPortals'
// twin over BOTH End-side sets (the exit in the End world / the entry
// in the overworld): every end_portal cell not strictly inside a
// complete ALL-EYED ring (the endPortalCellValid pure — the PITS 383①
// two-way cross-check) is an orphan -> air; the tracking sets rebuild.
// One private walk per world (the nether's chunk-scan shape).
namespace {

template <typename SetRef>
void rescanEndPortalWorld(World& w, const BlockRegistry& blocks,
                          SetRef tracking) {
    tracking.clear();
    std::vector<BlockPos> orphans;
    for (const ChunkPos& cp : w.chunkPositions()) {
        const Chunk* chunk = w.findChunk(cp);
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
                    const Block& b = blocks.at(id);
                    if (!b.properties.endPortal) {
                        continue;
                    }
                    const BlockPos wp {baseX + lx, baseY + ly, baseZ + lz};
                    if (!endPortalCellValid(w, blocks, wp)) {
                        orphans.push_back(wp);
                        continue;
                    }
                    tracking[wp] = id;
                }
            }
        }
    }
    if (orphans.empty()) {
        return;
    }
    BlockPos min {orphans.front()};
    BlockPos max {orphans.front()};
    for (const BlockPos& p : orphans) {
        w.setBlock(p.x, p.y, p.z, BlockID::air());
        min.x = std::min(min.x, p.x); min.y = std::min(min.y, p.y);
        min.z = std::min(min.z, p.z); max.x = std::max(max.x, p.x);
        max.y = std::max(max.y, p.y); max.z = std::max(max.z, p.z);
    }
    // ONE relight over the orphans' bounding box (the rescan's twin —
    // the end portal is a light-11 source, the nether_portal's mirror).
    Lighting::updateRegion(w, blocks, min, max);
    MC_LOG_INFO("End portal rescan: " + std::to_string(orphans.size()) +
                " orphan portal block(s) set to air (no complete all-eyed "
                "ring)");
}

} // namespace

void Game::rescanEndPortals() {
    // The two End-side sets: the EXIT (the End world — a no-op while
    // the End is unengaged, the lazy D2) + the ENTRY (the overworld —
    // always present, the structural-inert no-op while no end portal
    // exists: the per-chunk scan only reads the endPortal predicate).
    if (m_end.world) {
        rescanEndPortalWorld(*m_end.world, m_blocks, m_end.portals);
    }
    rescanEndPortalWorld(m_world, m_blocks, m_states.endPortals);
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

void Game::rescanPortals() {
    // V3-1 Phase A: the load-time frame validity (the Minecraft update
    // semantics + the PITS 383① two-way cross-check discipline): rebuild
    // the portal tracking set (the rescanBeacons twin) from the world, and
    // every portal cell that does not sit STRICTLY INSIDE a complete
    // frame — the ring all obsidian (corners optional) + the interior all
    // {air, fire, portal} — is an ORPHAN (a hand-edited save, or the frame
    // outlived its world) and is set to air (the load-time fix; the
    // session's journal records it, so the next save persists the clean
    // world — the two-way check's input side). The raw World API applies
    // the air (the applyWorldEdits precedent — the choke point is the
    // session's writer, the rescan is init's); the tracking set is
    // rebuilt directly, so the air writes need not round-trip it. A
    // fresh world finds nothing (worldgen rejects the stateful portal
    // block — the structural-immunity root); a loaded save finds the
    // player's portals. The per-cell frame scan is bounded (the
    // core/portal discipline); a save holding a 23×23 frame pays one
    // scan per interior cell at LOAD — the one-time init cost, the
    // documented placement-path ~1.6 ms/column class.
    m_states.portals.clear();
    if (!m_states.portalId.valid() || !m_states.obsidianId.valid()) {
        return; // the structural-inert no-op (no portal block or no
                // frame material — the paths never engage)
    }
    std::vector<BlockPos> orphans;
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
                    if (id != m_states.portalId) {
                        continue;
                    }
                    const BlockPos wp {baseX + lx, baseY + ly, baseZ + lz};
                    if (portalValidFrame(m_world, wp, m_states.obsidianId,
                                         m_states.fireId,
                                         m_states.portalId) ==
                        std::nullopt) {
                        orphans.push_back(wp);
                        continue;
                    }
                    m_states.portals[wp] = id;
                }
            }
        }
    }
    if (orphans.empty()) {
        return;
    }
    BlockPos min {orphans.front()};
    BlockPos max {orphans.front()};
    for (const BlockPos& p : orphans) {
        m_world.setBlock(p.x, p.y, p.z, BlockID::air());
        min.x = std::min(min.x, p.x); min.y = std::min(min.y, p.y);
        min.z = std::min(min.z, p.z);
        max.x = std::max(max.x, p.x); max.y = std::max(max.y, p.y);
        max.z = std::max(max.z, p.z);
    }
    // ONE relight over the orphans' bounding box (the portal is a light-
    // 11 source — its removal shifts the neighborhood; applySave's full
    // propagation ran BEFORE this fix, the fluid-tick box pattern).
    Lighting::updateRegion(m_world, m_blocks, min, max);
    MC_LOG_INFO("Portal rescan: " + std::to_string(orphans.size()) +
                " orphan portal block(s) set to air (no complete frame)");
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

    // --- V3-9: the demo mode's state (the three flags absent = Off —
    // the bit-identical normal run below) ---------------------------------
    if (m_args.demoFile.has_value()) {
        m_demoMode = DemoMode::Replay;
        // The emplace (DemoPlayer holds the file's REFERENCE — the
        // m_args member's lifetime covers the session; no move).
        m_demoPlayer.emplace(*m_args.demoFile);
        m_demoPath = m_args.demo.string();
        // The demo's SAVE spec references a slot (deviation 6: not
        // embedded) — a deleted slot is the named reject HERE (the
        // tryLoadSave's "starting fresh" silent fallback is out: a
        // save demo must start from its state).
        const DemoWorldSpec& spec = m_args.demoFile->world;
        if (spec.type == DemoWorldSpec::Type::Save &&
            !std::filesystem::exists(savePath())) {
            return MC_FAIL("demo's save slot '" + spec.slot +
                           "' not found (the demo references the slot, "
                           "it is not embedded — restore it or re-"
                           "record)");
        }
        // V3-16a: the SCENE spec (the committed guide asset — the
        // existence's named reject's twin; the load's path is
        // tryLoadSave's scene face, the never-write-back is
        // structural: the scene is not a slot, the exit's --save
        // composes like the fresh spec).
        if (spec.type == DemoWorldSpec::Type::Scene) {
            if (!std::filesystem::exists(demoScenePath(spec.slot))) {
                return MC_FAIL("demo's scene '" + spec.slot +
                               "' not found (the committed "
                               "assets/demos/scenes directory — the "
                               "guide asset is missing)");
            }
            m_demoSceneName = spec.slot;
        }
    } else if (m_args.demoRecord) {
        m_demoMode = DemoMode::Record;
        DemoWorldSpec spec;
        if (m_args.saveSlot.has_value()) {
            // The record's start point = the slot (the replay loads
            // it — the slot's state at the REPLAY is the start; a
            // modified slot shifts it, the documented boundary).
            spec.type = DemoWorldSpec::Type::Save;
            spec.slot = *m_args.saveSlot;
        } else {
            spec.type = DemoWorldSpec::Type::Fresh;
            spec.seed = seed; // the config's (the --seed-overridden)
        }
        m_demoRecorder.setWorld(spec);
        // The boundary tap (record-first-then-inject — the Input fires
        // it BEFORE the state mutation; the null tap in every other
        // mode = the bit-identical run). The frame attribution = the
        // loop's m_frameCount at the tap moment (the simulated frame
        // the event feeds).
        m_input.setEventTap([this](const InputEvent& ev) {
            m_demoRecorder.tapEvent(ev, m_frameCount);
        });
        MC_LOG_INFO("Demo record mode (world: " +
                    demoWorldDescription(spec) + ")");
    }

    // --- assets -----------------------------------------------------------
    auto assetRoot = m_assets.setRoot("assets");
    MC_TRY(assetRoot);

    // V3-16a: the guide demo's INDEX cross-check (the shipped content's
    // integrity — the fail-fast's named reject, every boot: the
    // nether pool's tradition. The file's full strict load + the SIM's
    // gate + the length's match + the scene's existence — a broken
    // guide asset fails the boot LOUDLY, never a mid-tutorial dead
    // end).
    {
        auto index = loadDemoIndexChecked();
        MC_TRY(index);
    }

    // --- window + GL context ----------------------------------------------
    auto window = m_window.init(m_config);
    MC_TRY(window);

    const char* vendor = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
    const char* glVersion = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    const char* renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    MC_LOG_INFO(std::string("GL ") + glVersion + " | " + vendor + " | " + renderer);

    // --- V3-12b: the net CLIENT's handshake (before the world — the
    // WELCOME's seed is the world spec's authority; the fingerprint's
    // four faces need the config + the assets + the window, all
    // ready here) ---
    if (!m_args.connectHost.empty()) {
        MC_TRY(netClientHandshake());
        if (m_netSeedOverride >= 0) {
            m_config.set("world.seed", static_cast<long long>(m_netSeedOverride));
            MC_LOG_INFO("net: the WELCOME's seed " +
                        std::to_string(m_netSeedOverride) +
                        " is the world spec's authority (the host's "
                        "world, the zero-transfer's shape)");
        }
    }

    // --- renderer + scene ---------------------------------------------------
    auto rendererInit = m_renderer.init();
    MC_TRY(rendererInit);

    auto scene = buildScene();
    MC_TRY(scene);

    // V3-9: the replay's SAVE spec loaded the slot (buildScene's
    // tryLoadSave) — the OBSERVATION-ONLY red line: the referenced
    // slot is NEVER written back (the replay's edits — the mined
    // blocks, the picked-up drops — stay in the session; the fresh
    // spec + --save keeps the slot: that one is the user's EXIT write
    // of the replay's result, not the demo's reference).
    if (m_demoMode == DemoMode::Replay &&
        m_args.demoFile->world.type == DemoWorldSpec::Type::Save) {
        m_args.saveSlot.reset();
    }

    // --- V3-12b: the net HOST's handshake (after the world + the
    // player — the HELLO's comparison needs the world's seed + the
    // creative's gate; the accept's 10-s timeout is the gate's
    // named-error face) + the --script's injection kernel ---
    if (m_args.listenPort != 0) {
        MC_TRY(netHostHandshake());
    }
    if (m_netActive && !m_netIsHost && !m_netSnapshotPayload.empty()) {
        // V3-12c2: the REJOIN's snapshot's apply (the save-form
        // WELCOME's document over the worldgen'd world — the
        // parse/apply path's full reuse, the file-load's twin; the
        // frame's adoption aligns the pacing to the host's).
        MC_TRY(applyNetSnapshot());
    }
    if (m_netActive && !m_netIsHost) {
        // The c1's N-generalization: the client creates EVERY other
        // player's body (slots 1..n-1 — the lockstep simulates every
        // player on every machine; player 0, the host's body, is the
        // init's normal construction). The 12b's single netAddPlayer(1)
        // breaks at N>2 (the local slot 2's body missing = the
        // out-of-range read's segfault, and the client's hash feeding
        // its own 2-player count against the host's 3 = the f0
        // desync's twin — the gate-catch pair). V3-12c2: the
        // rejoin's snapshot already built the EXISTING players'
        // bodies (applySave's ghosts — the document's entries
        // 1..n-2); the loop's skip-face creates only the missing
        // ones (the joining slot's fresh body — the starter gift,
        // the host's netAddPlayer's twin).
        for (std::size_t slot = 1; slot < m_frameRings.size(); ++slot) {
            if (slot < m_players.size() && m_players[slot] != nullptr) {
                continue;
            }
            netAddPlayer(slot);
        }
    }
    if (m_args.scriptFile.has_value()) {
        m_scriptPlayer.emplace(*m_args.scriptFile);
        m_scriptActive = true;
        MC_LOG_INFO("net script: '" + m_args.script.string() + "' (" +
                    std::to_string(m_scriptPlayer->frameCount()) +
                    " frames, " +
                    std::to_string(m_args.scriptFile->events.size()) +
                    " events — the deterministic input's source)");
    }

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

    // V3-9: the loop BRANCHES on the demo mode — Off / Record = the
    // pre-V3-9 loop verbatim (runGameplayLoop; the three flags absent
    // = the bit-identical normal run, the structural immunity root);
    // Replay = the demo-fed loop (runDemoReplay: the audience owns the
    // SDL queue, the demo's dts are the only time source).
    if (m_demoMode == DemoMode::Replay) {
        runDemoReplay();
    } else {
        runGameplayLoop(lastFrame);
    }

    // Chunk streaming (Phase 14): drain the worker pool before the save
    // and shutdown — a job exception would surface in the flush, and the
    // reset joins the workers. In-flight generation columns are not
    // integrated (integration happens in update); that is correct, they
    // are pristine generated terrain that a save never records.
    if (m_streaming.pool) {
        m_streaming.pool->flush();
        m_streaming.pool.reset();
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

    // V3-9: the record's EXIT save (the window close / the ESC exit =
    // the loop's normal break — no special path; the .dmo is written
    // AFTER the save, so a --save run records the slot it left). The
    // quit frame's un-simulated tail events are excluded (see
    // discardPendingFrame).
    if (m_demoMode == DemoMode::Record) {
        m_demoRecorder.discardPendingFrame();
        std::filesystem::path out = m_args.demoRecordOut;
        if (out.empty()) {
            out = "data/demos/" + defaultRecordName();
        }
        auto rec = writeDemoFile(out, m_demoRecorder.file());
        if (!rec.ok()) {
            MC_LOG_ERROR("Demo record failed: " + rec.error().describe());
        } else {
            std::error_code ec;
            const std::uintmax_t bytes = std::filesystem::file_size(out, ec);
            MC_LOG_INFO("Demo recorded: '" + out.string() + "' (" +
                        std::to_string(m_demoRecorder.frameCount()) +
                        " frames, " +
                        std::to_string(m_demoRecorder.file().events.size()) +
                        " events" +
                        (ec ? std::string()
                            : ", " + std::to_string(bytes) + " B") +
                        ")");
        }
    }

    m_window.shutdown();
    m_initialized = false;
    MC_LOG_INFO("Game shut down cleanly");
    return 0;
}

// ---------------------------------------------------------------------------
// The gameplay loop (V3-9: the pre-V3-9 main loop body, verbatim — the
// Off / Record modes. The record's only addition is the flag-gated dt
// append after the frame count (the demo's dts carry the clamped wall
// value the simulation used).)
// ---------------------------------------------------------------------------

void Game::runGameplayLoop(double lastFrame) {
    // V3-14a: the --explode seam's single deterministic blast —
    // applied BEFORE the first frame (frame-0 state, the --weather's
    // initial-state twin): every frame of the run sees the post-blast
    // world, and an identical run blasts identically. The ghast's
    // V3-14b fireball will call the same Game::explodeAt from its
    // impact (the seam has no demo form — the replay's input is the
    // demo's, main.cpp's mutual exclusion).
    if (m_args.explode.has_value()) {
        const ExplosionSpec& spec = *m_args.explode;
        MC_LOG_INFO("Exploding at (" + std::to_string(spec.center.x) +
                    ", " + std::to_string(spec.center.y) + ", " +
                    std::to_string(spec.center.z) + ") radius " +
                    std::to_string(spec.radius));
        explodeAt(spec.center, spec.radius, "");
        m_args.explode.reset();
    }
    while (true) {
        m_input.frameBegin();
        m_input.pump();
        // V3-12b: the --script's deterministic input (the demo
        // injection kernel's fourth consumer — the frame's events into
        // the live pump's same handle* surface; the frame limit's
        // guard: past the timeline's end the input is the no-input
        // zero, the solo's structural-immunity shape).
        if (m_scriptActive &&
            static_cast<int>(m_frameCount) <
                m_scriptPlayer->frameCount()) {
            m_scriptPlayer->injectForFrame(m_frameCount, m_input);
        }
        // ESC exits only while the pointer is free AND the inventory is
        // closed AND the player is alive (while captured it releases the
        // pointer, while the inventory is open it closes it, while dead
        // the death screen owns the input — all handled in update).
        if (m_input.quitRequested() ||
            (!m_window.relativeMouseMode() && !panelOpen() &&
             !localPlayer()->isDead() &&
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

        // V3-12b: the net's frame pre-hook (the poll / the capture / the
        // preload; the client's dt is OVERRIDDEN by the host's broadcast
        // — the raw f64 bits, the sim's ONLY time source. The solo's
        // netActive-false keeps the wall dt verbatim, the bit-level).
        if (netActive()) {
            const std::string err = netFramePre(dt);
            if (!err.empty()) {
                MC_LOG_ERROR("net: " + err);
                if (m_netIsHost && m_players.size() == 1) {
                    // The client's EOF already removed slot 1 (the
                    // session's solo continuation — the loop carries on).
                    m_netActive = false;
                } else {
                    break;
                }
            }
        }

        update(dt);
        render();
        m_window.swapBuffers();
        ++m_frameCount;

        // V3-12b: the net's frame post-hook (the host's FRAME send +
        // the client's INPUT send + the hash's exchange). The frame
        // index = the just-simulated one (the increment already ran).
        if (netActive()) {
            const std::string err = netFramePost(dt, m_frameCount - 1);
            if (!err.empty()) {
                MC_LOG_ERROR("net: " + err);
                break;
            }
        }

        // V3-9: the record's dt append (flag-gated — Off = the pre-V3-9
        // loop verbatim). The dt = the just-simulated frame's clamped
        // wall value (the replay's dts carry it verbatim; the frame's
        // pump already tapped its events at m_frameCount = this index,
        // so the event's frame < dts.size() — the parser's invariant).
        if (m_demoMode == DemoMode::Record) {
            m_demoRecorder.frame(dt);
        }

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
}

// ---------------------------------------------------------------------------
// The demo replay loop (V3-9 — the --demo mode).
//
// The audience's Input owns the SDL queue (the single-consumer
// discipline — the player's Input is demo-fed, NEVER pumped). The
// demo's dts are the ONLY time source (the wall clock paces the
// rendering: target = the anchor + the completed demo time / the
// speed, the int64-microsecond sum guards the float drift; a slow
// machine drops render frames, the simulation does not chase — the
// documented deviation 2). The 0.5x hold renders the demo frame twice
// (the second render-only — no update, no m_frameCount: a frame-
// locked system advances once per demo frame, never twice). The
// audience's WHITELIST (the orbit / the speed / the pause / the view
// mode / the quit-from-panel) never touches the simulation: the orbit
// re-resolves the camera around the eye (render-only; the aim rays
// read aimForward — the player's base view, the red line), the speed
// is pacing, the pause freezes the loop (the re-anchor), the C cycle
// is the session's camera mode. ESC = the pause menu (NOT the exit —
// the pre-V3-9 ESC semantics are the player channel's, the demo's
// events own them).
// ---------------------------------------------------------------------------

void Game::runDemoReplay() {
    const int total = m_demoPlayer->frameCount();
    const std::size_t nEvents = m_args.demoFile->events.size();
    MC_LOG_INFO("Demo replay: '" + m_demoPath + "' (" +
                std::to_string(total) + " frames, " +
                std::to_string(nEvents) + " events, world: " +
                demoWorldDescription(m_args.demoFile->world) +
                ", recorded with " +
                m_args.demoFile->recordedWith + ")");
    // The synthetic gamepad connect (only when the demo carries
    // gamepad events — the player's Input has no real device on the
    // replay machine; the handleGamepad* entry points' not-connected
    // guard needs a tracked device to pass, the source-bit semantics
    // then match the record side exactly).
    if (m_demoPlayer->hasGamepadEvents()) {
        m_input.handleGamepadAdded(kDemoSyntheticGamepadWhich);
    }
    // The pacing's anchor (the wall clock — re-anchored at the speed
    // change and the pause/resume, the demo time is untouched).
    m_demoPacingAnchorWall = nowSeconds();
    m_demoPacingAnchorUs = 0;
    m_demoTimeUs = 0;
    m_demoFrame = 0;

    while (true) {
        const double frameStart = nowSeconds();
        // The audience owns the SDL queue (see the section header).
        m_audienceInput.frameBegin();
        m_audienceInput.pump();
        // The window close = the exit (the audience's quit — the demo
        // vocabulary has no quit event; the player channel is demo-fed).
        if (m_audienceInput.quitRequested()) {
            MC_LOG_INFO("Exit requested, shutting down");
            break;
        }
        if (!m_demoEnded && m_demoFrame >= total) {
            m_demoEnded = true;
            m_demoEndPanelOpen = true;
            MC_LOG_INFO("Demo replay finished (" +
                        std::to_string(total) + " frames)");
        }
        const bool panelUp = m_demoPaused ||
                             (m_demoEnded && m_demoEndPanelOpen);
        // The audience's controls (the whitelist — never the
        // simulation).
        if (m_audienceInput.wasPressed(GameKey::Escape)) {
            if (m_demoPaused) {
                m_demoPaused = false;
            } else if (m_demoEnded && m_demoEndPanelOpen) {
                m_demoEndPanelOpen = false; // keep observing the
                                            // still last frame
            } else {
                m_demoPaused = true;
            }
            m_demoPacingAnchorWall = nowSeconds();
            m_demoPacingAnchorUs = m_demoTimeUs;
        }
        if (panelUp && m_audienceInput.wasPressed(GameKey::Q)) {
            MC_LOG_INFO("Demo replay quit (audience)");
            break;
        }
        if (m_audienceInput.wasPressed(GameKey::C)) {
            m_cameraMode =
                m_cameraMode == CameraMode::First
                    ? CameraMode::ThirdBack
                    : m_cameraMode == CameraMode::ThirdBack
                          ? CameraMode::ThirdFront
                          : CameraMode::First;
        }
        // The speed: the audience's wheel (0.5 -> 1 -> 2) at the frame
        // boundary (the switch lands BETWEEN demo frames — never a
        // half frame, the determinism). The gamepad's LB / RB (the
        // V3-8 mapping's mouse Left / Right — the gamepad-connected
        // guard keeps the audience's REAL mouse clicks out of the
        // cycle; a real click with a connected gamepad still cycles —
        // the documented v1 limitation, the speed is pacing-only).
        const bool audienceSpeed =
            m_audienceInput.wheelDelta() != 0 ||
            (m_audienceInput.gamepadConnected() &&
             (m_audienceInput.wasPressed(MouseButton::Left) ||
              m_audienceInput.wasPressed(MouseButton::Right)));
        if (audienceSpeed) {
            m_demoSpeed = DemoPlayer::nextSpeed(m_demoSpeed);
            m_demoPacingAnchorWall = nowSeconds();
            m_demoPacingAnchorUs = m_demoTimeUs;
        }
        // The audience's orbit (render-only — the mouse's delta + the
        // gamepad's sticks; the player's look convention: right = the
        // yaw decrease, down = the pitch decrease).
        {
            const Vec2 a = m_audienceInput.mouseDelta();
            m_demoOrbitYaw -= a.x * kAudienceOrbitSensitivity;
            m_demoOrbitPitch -= a.y * kAudienceOrbitSensitivity;
            if (m_audienceInput.isDown(GameKey::D)) {
                m_demoOrbitYaw -= kAudienceStickOrbitStep;
            }
            if (m_audienceInput.isDown(GameKey::A)) {
                m_demoOrbitYaw += kAudienceStickOrbitStep;
            }
            if (m_audienceInput.isDown(GameKey::S)) {
                m_demoOrbitPitch -= kAudienceStickOrbitStep;
            }
            if (m_audienceInput.isDown(GameKey::W)) {
                m_demoOrbitPitch += kAudienceStickOrbitStep;
            }
            m_demoOrbitPitch = std::clamp(
                m_demoOrbitPitch, -kAudienceOrbitMaxPitch,
                kAudienceOrbitMaxPitch);
        }
        // The still frame (the pause menu / the finished panel / the
        // finished observation): the simulation freezes, the wall-
        // clock pacing stops, the orbit stays live (syncCamera).
        if (panelUp || m_demoFrame >= total) {
            m_input.frameBegin(); // the stale edges are dropped (the
                                  // demo's events land on the next
                                  // SIMULATED frame, fresh)
            syncCamera();
            render();
            m_window.swapBuffers();
            if (frameStart - m_lastTitleUpdate >= 0.5) {
                m_lastTitleUpdate = frameStart;
                m_window.setTitle("TokenMine.cpp - " +
                                  std::to_string(static_cast<int>(
                                      std::lround(m_fpsSmoothed))) +
                                  " FPS | " +
                                  std::to_string(m_renderer.stats().drawCalls) +
                                  " draw calls");
            }
            // The headless run's exit (--frames): a limit BEYOND the
            // demo's end = the demo's end + the still state. The
            // screenshot (takeScreenshot's glReadPixels) reads the
            // BACK buffer after a swap = the PREVIOUS front (the GLX
            // double-buffer exchange — the project's screenshots are
            // all one rendered frame behind the last swap, the gate's
            // cross-run identity is unaffected because every run lags
            // the same way): so the finished state renders + swaps a
            // SECOND time before the screenshot, making the still
            // content (the panel + the badge) the back buffer's. The
            // interactive run keeps observing here (the panel's ESC /
            // Q owns it — no exit, no double render).
            if (m_args.frames > 0 && m_demoFrame >= total) {
                render();
                m_window.swapBuffers();
            }
            if (m_args.frames > 0 && m_demoFrame >= total) {
                MC_LOG_INFO("Demo replay finished, frame limit " +
                            std::to_string(m_args.frames) + " beyond the "
                            "demo's " + std::to_string(total) + " frames - "
                            "exiting after the finished state");
                if (!m_args.screenshotPath.empty()) {
                    takeScreenshot(m_args.screenshotPath);
                }
                break;
            }
            SDL_Delay(kDemoIdleRenderDelayMs);
            continue;
        }
        // --- The demo frame f (the dts = the time source) ----------
        // The pacing: the wall clock only waits for the frame's start
        // moment (the anchor + the COMPLETED demo time / the speed; a
        // slow machine drops render frames — the simulation never
        // chases, the documented deviation).
        {
            const double target = m_demoPacingAnchorWall +
                                  (m_demoTimeUs - m_demoPacingAnchorUs) /
                                      (kDemoUsPerSecond * m_demoSpeed);
            const double now = nowSeconds();
            if (target - now > kDemoMinPacingWaitSeconds) {
                SDL_Delay(static_cast<Uint32>((target - now) * 1000.0));
            }
        }
        m_input.frameBegin();
        // V3-16a: the panel-click's resolver (the demo's break's loud
        // line — the named problem, the gate's grep face).
        const std::string injErr =
            m_demoPlayer->injectForFrame(
                m_demoFrame, m_input,
                [this](std::uint32_t panel, std::int32_t slot,
                       std::int16_t button) {
                    auto r = demoPanelClick(panel, slot, button);
                    return r.ok() ? std::string("")
                                  : r.error().message;
                });
        if (!injErr.empty()) {
            MC_LOG_ERROR("demo: the panel's click's problem at frame " +
                         std::to_string(m_demoFrame) + " — " + injErr);
        }
        const double dt = m_demoPlayer->dtForFrame(m_demoFrame);
        update(dt);
        if (dt > 0.0) {
            const double fps = 1.0 / dt;
            m_fpsSmoothed =
                (m_fpsSmoothed <= 0.0) ? fps : (m_fpsSmoothed * 0.95 + fps * 0.05);
        }
        render();
        m_window.swapBuffers();
        ++m_frameCount;
        m_demoTimeUs +=
            static_cast<std::int64_t>(std::llround(dt * kDemoUsPerSecond));
        // The 0.5x hold (the render-only second — see the section
        // header; the same frame renders identically, the byte-
        // identity is trivial).
        for (int r = 1; r < DemoPlayer::rendersPerFrame(m_demoSpeed); ++r) {
            render();
            m_window.swapBuffers();
        }
        ++m_demoFrame;
        if (m_args.frames > 0 && m_frameCount >= m_args.frames) {
            MC_LOG_INFO("Frame limit reached (" + std::to_string(m_frameCount) +
                        " frames), exiting");
            if (!m_args.screenshotPath.empty()) {
                takeScreenshot(m_args.screenshotPath);
            }
            break;
        }
        if (frameStart - m_lastTitleUpdate >= 0.5) {
            m_lastTitleUpdate = frameStart;
            m_window.setTitle("TokenMine.cpp - " +
                              std::to_string(static_cast<int>(
                                  std::lround(m_fpsSmoothed))) +
                              " FPS | " + std::to_string(m_renderer.stats().drawCalls) +
                              " draw calls");
        }
    }
}

} // namespace mc
