#include "core/game.hpp"

#include <GL/glew.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <numbers>

#include "block/cube_face_table.hpp"
#include "core/bow.hpp"
#include "core/game_local.hpp"
#include "core/math/frustum.hpp"
#include "core/png.hpp"
#include "renderer/alpha_test.hpp"
#include "renderer/fog.hpp"
#include "ui/ui_scale.hpp"
#include "world/beam_light.hpp"

namespace mc {

namespace {

constexpr float kFovDegrees = 70.0f;

constexpr float kNear = 0.1f;

constexpr float kFar = 1000.0f;

// PRE-3 water ripples: the shimmer's phase period in FRAMES (2.0 s at the
// 60 FPS cap — the frame-locked class, hard rule 8; the phase is a pure
// function of m_frameCount, so the animation is byte-reproducible and the
// period is a named constant with no config key). The u_ripplePhase uniform
// sweeps 0..2*pi over the period; the frame MODULO keeps the sin argument
// bounded (float precision loss over a long session).
constexpr int kRipplePeriodFrames = 120;

// Sprite sizes in pixels (billboarded quads — see sky.vert).
constexpr float kStarSpriteSize = 2.0f;

constexpr float kSunSpriteSize = 40.0f;

constexpr float kMoonSpriteSize = 28.0f;

// Sun/moon fade-out: alpha = clamp(elevation / kHorizonFade, 0, 1), so they
// are fully visible a little above the horizon and invisible on/below it.
constexpr float kHorizonFade = 0.1f;

constexpr float kBeaconBeamBaseHeight = 64.0f;

constexpr float kBeaconBeamLevelHeight = 32.0f;

// Phase 83 beacon beam color (bright cyan-white, the classic beam) and its
// translucency (the beam is a light column, not a solid pillar).
constexpr float kBeaconBeamR = 0.6f;

constexpr float kBeaconBeamG = 0.95f;

constexpr float kBeaconBeamB = 1.0f;

constexpr float kBeaconBeamAlpha = 0.5f;

// 3D held hand (Phase 42): the view-anchored hand model constants (see
// drawHeldHand). The offset is in the CAMERA frame (x = right, y = up,
// z = behind the eye), so the hand sits low-right in front of the eye.
constexpr Vec3 kHandOffset {0.42f, -0.38f, -0.85f};

constexpr float kHandCubeScale = 0.28f; // block-cube hand size

constexpr float kHandIconScale = 0.5f;  // icon-quad hand size

constexpr float kHandSpinX = -0.35f; // tilt the top toward the camera

constexpr float kHandSpinY = 0.6f;   // turn a corner toward the camera

// Bow draw (Phase 42, replaces the Phase 24 2D icon pull): the hand pulls
// back toward the face and the bow tilts back with the charge power.
constexpr float kHandDrawPullZ = 0.3f;

constexpr float kHandDrawSpinX = 0.9f;

// Eat channel (Phase 42): the hand rises toward the face with the eat
// progress (replaces the Phase 28 2D icon pull).
constexpr float kHandEatLiftY = 0.35f;

constexpr float kHandEatPullZ = 0.35f;

// Phase 78: the four-slot icon row (Minecraft shows every cooking item
// on the campfire). One 0.35 icon per occupied slot, 0.3 apart in a 1x4
// row across the cell's upper half (total span 1.25 — a 0.5 icon leaves
// no room for four, so the icons shrink to fit; the small neighbour
// overlap is intentional adjacency).
constexpr float kCampfireIconScale = 0.35f;

constexpr float kCampfireIconSpacing = 0.3f;

} // namespace

void Game::drawCampfireCookItems(const Mat4& view, const Mat4& projection) {
    // Phase 71 (all four slots render in Phase 78): the items cooking on
    // each campfire. No occupied slot: nothing draws (a no-input run has
    // no campfire at all — worldgen never emits one — and a placed-but-
    // empty campfire draws nothing either: baseline byte-identical).
    for (const auto& [pos, st] : m_states.campfires) {
        bool any = false;
        for (const ItemStack& slot : st.slots) {
            if (!slot.empty()) {
                any = true;
                break;
            }
        }
        if (!any) {
            continue;
        }
        // The icons float in the upper half of the cell (the campfire is
        // a cross block — the food sits above the flame). Crossed quads
        // (the cross-block convention: view-independent; culling stays
        // ON, so from any horizontal angle at least one quad's front
        // face shows). The quad's +Y normal keeps it in the top-face
        // brightness bucket through the 90° cross rotation (the
        // handIconMesh convention). Slot i is centred at the cell centre
        // + (i - 1.5) * the spacing.
        m_basicShader.bind();
        m_basicShader.setUniform1i("u_useTexture", 1);
        m_renderer.enableBlending();
        for (std::size_t i = 0; i < kCampfireSlotCount; ++i) {
            const ItemStack& slot = st.slots[i];
            if (slot.empty()) {
                continue;
            }
            const std::size_t iconIdx =
                static_cast<std::size_t>(slot.item.value);
            if (iconIdx >= m_itemIcons.size() ||
                !m_itemIcons[iconIdx].valid()) {
                continue; // cannot happen (item data is validated at init)
            }
            const float dx =
                (static_cast<float>(i) - 1.5f) * kCampfireIconSpacing;
            const Vec3 center {static_cast<float>(pos.x) + 0.5f + dx,
                               static_cast<float>(pos.y) + 0.75f,
                               static_cast<float>(pos.z) + 0.5f};
            const Mat4 base =
                Mat4::multiply(Mat4::translate(center),
                               Mat4::scale(kCampfireIconScale));
            // The icon texture carries transparent pixels (the
            // drawHeldHand pattern): blending on for the two draws, the
            // atlas restored after the row.
            m_itemIcons[iconIdx].bind();
            m_renderer.draw(handIconMesh(), m_basicShader, view, projection,
                            base);
            m_renderer.draw(handIconMesh(), m_basicShader, view, projection,
                            Mat4::multiply(
                                base, Mat4::rotateY(0.5f * std::numbers::pi)));
        }
        m_atlas.texture().bind(); // restore for the passes that follow
        m_renderer.disableBlending();
    }
}

void Game::drawBeaconBeams(const Mat4& view, const Mat4& projection) {
    // Phase 83: the translucent light column rising from every ACTIVE
    // (level>=1) beacon to the sky. Drawn in the blending pass (the depth
    // test occludes it behind terrain like the other world geometry). A
    // no-input run has no beacon -> m_states.beaconActive empty -> this is a no-op
    // (baseline byte-identical, structural immunity); the `beacon.beam`
    // config gate is the E2E control (the fire.particles pattern). Culling
    // is off for the beam (both faces of each quad render, alpha-blended).
    //
    // LGT-4: the beam RECEIVES the world light. Each active beacon's draw
    // feeds the vertex per-draw override (u_useLightOverride /
    // u_lightOverrideSky / u_lightOverrideBlock — the PRE-6 itemDropLight
    // pattern, renamed neutral) with beamLightSample: the light of the
    // AIR CELL directly above the beacon block (the beam's lowest cell —
    // the block's own cell is opaque and reads (0, 0)): the beam is
    // moonlit at deep night (the LGT-3 affine retreat with the
    // kNightSkyLevel 7 floor), bright next to glowstone / torches, and
    // full white at open-sky noon = the legacy constant look there. The
    // override is uniform-gated per draw, so the baked vertex light
    // (sky 0 / block 1.0) is the fallback a non-override draw sees — no
    // config gate (the beam draw itself is structurally inert without
    // beacons), and the sample is deterministic frame state (the light
    // field + dayFactor, hard rule 8: no real-dt).
    if (!m_states.beaconBeam || m_states.beaconActive.empty()) {
        return;
    }
    m_basicShader.bind();
    m_basicShader.setUniform1i("u_useTexture", 0);
    m_basicShader.setUniform3f("u_color", kBeaconBeamR, kBeaconBeamG,
                               kBeaconBeamB);
    m_basicShader.setUniform1f("u_alpha", kBeaconBeamAlpha);
    m_basicShader.setUniform1f("u_dayFactor", m_dayCycle.dayFactor());
    // Blending is the caller's (the blend section's) state — do NOT
    // enable/disable it here (10-26 user-report fix: the trailing
    // disableBlending leaked past this draw and killed the crack/outline
    // overlay's blending — the mined block rendered through the crack
    // cube's (0,0,0,0) background as a solid black box).
    m_renderer.setCulling(false);
    for (const BeaconBeam& b : m_states.beaconActive) {
        // The unit-height beam box, scaled to the level's height and
        // positioned on the beacon cell's top face (y = cell y + 1).
        const float height =
            kBeaconBeamBaseHeight + b.level * kBeaconBeamLevelHeight;
        const Mat4 model = Mat4::multiply(
            Mat4::translate(static_cast<float>(b.x) + 0.5f,
                            static_cast<float>(b.y) + 1.0f,
                            static_cast<float>(b.z) + 0.5f),
            Mat4::scale(1.0f, height, 1.0f));
        // LGT-4: this beacon's light (the vertex override: the air cell
        // directly above the beacon block — the beam's lowest cell; the
        // block's own cell is opaque and always reads (0, 0), the gate
        // 119 E2E defect). The reset below restores the 0 every other
        // draw expects — a leak would light the draws that follow with
        // this beacon's cell light, the PRE-6 / UXFIX-1 discipline. The
        // program must be BIND first: Renderer::draw unbinds it after
        // every draw (the top-of-function bind was already consumed by
        // the previous iteration).
        m_basicShader.bind();
        const std::pair<float, float> light =
            beamLightSample(m_world, b.x, b.y, b.z);
        m_basicShader.setUniform1i("u_useLightOverride", 1);
        m_basicShader.setUniform1f("u_lightOverrideSky", light.first);
        m_basicShader.setUniform1f("u_lightOverrideBlock", light.second);
        m_renderer.draw(m_states.beaconBeamMesh, m_basicShader, view, projection,
                        model);
        // LGT-4: the reset (draw unbound the program — re-bind): the
        // override must not leak into the draws that follow.
        m_basicShader.bind();
        m_basicShader.setUniform1i("u_useLightOverride", 0);
    }
    m_renderer.setCulling(true);
    // (blending state left to the caller — see drawBeaconBeams head)
}

void Game::drawClouds(const Mat4& view, const Mat4& projection) {
    // PRE-4: the drifting translucent cloud layer (world/cloud).
    // render.clouds (code default true, the render.fog getBool precedent —
    // config.json is untouched) gates the draw; OFF = zero mesh, zero
    // draw (the v0.27.0 pipeline bit-identical — the gate's "new-off"
    // control). The clouds are UNLIT WHITE quads through the basic
    // program's solid-color path (the beam pattern: u_useTexture = 0 +
    // u_alpha, culling off so both faces of each quad render
    // alpha-blended; the depth test stays on, so terrain occludes the
    // layer like any other world geometry).
    if (!m_config.getBool("render.clouds", true)) {
        return;
    }
    // The rebuild key: the camera's cloud cell (the floor of the eye's
    // x/z on the kCloudCellBlocks grid — floorDiv, hard rule 1) + the
    // frame-locked drift time step (m_frameCount / kCloudFramesPerCell,
    // hard rule 8). A no-input run is static from the first draw;
    // walking rebuilds once per 16 blocks (the 99.x discipline — no
    // per-frame VBO upload).
    const Vec3 eye = m_camera.position;
    const int cellX =
        floorDiv(static_cast<int>(std::floor(eye.x)), kCloudCellBlocks);
    const int cellZ =
        floorDiv(static_cast<int>(std::floor(eye.z)), kCloudCellBlocks);
    const int t = cloudTimeStep(m_frameCount);
    if (cloudMeshNeedsRebuild(m_cloudMeshValid, m_cloudMeshCellX,
                              m_cloudMeshCellZ, m_cloudMeshT, cellX, cellZ,
                              t)) {
        std::vector<MeshVertex> verts;
        std::vector<std::uint32_t> indices;
        buildCloudMesh(cellX, cellZ, t, verts, indices);
        if (verts.empty()) {
            // Unreachable at 50% density over 576 cells — defensive: no
            // geometry, no draw, and the cache stays invalid so the next
            // frame retries (the key is untouched).
            m_cloudMeshValid = false;
            return;
        }
        const auto up = m_cloudMesh.upload(verts, indices,
                                           MeshMode::Triangles);
        if (!up.ok()) {
            MC_LOG_ERROR("Cloud mesh upload failed: " + up.error().describe());
            m_cloudMeshValid = false;
            return;
        }
        m_cloudMeshValid = true;
        m_cloudMeshCellX = cellX;
        m_cloudMeshCellZ = cellZ;
        m_cloudMeshT = t;
    }
    if (!m_cloudMeshValid) {
        return;
    }
    // The unlit-white draw: the beam's uniform recipe (u_dayFactor is
    // read by the vertex shader — with skyLight = 0 the clouds render
    // full-brightness white at every time of day, the documented
    // deviation "the clouds do not darken at night").
    m_basicShader.bind();
    m_basicShader.setUniform1i("u_useTexture", 0);
    m_basicShader.setUniform3f("u_color", 1.0f, 1.0f, 1.0f);
    m_basicShader.setUniform1f("u_alpha", kCloudAlpha);
    m_basicShader.setUniform1f("u_dayFactor", m_dayCycle.dayFactor());
    // Blending is the caller's (the blend section's) state — do NOT
    // enable/disable it here (10-26 user-report fix: the trailing
    // disableBlending leaked into drawWorldOverlay, whose crack cube
    // then drew unblended — its (0,0,0,0) background painted the mined
    // block solid black).
    m_renderer.setCulling(false);
    // The mesh is WORLD-SPACE (absolute quad coordinates) — the identity
    // model matrix (the beam scales a unit box per beacon; the clouds
    // carry their own positions).
    m_renderer.draw(m_cloudMesh, m_basicShader, view, projection,
                    Mat4::identity());
    m_renderer.setCulling(true);
}

void Game::drawHeldHand(const Mat4& view, const Mat4& projection) {
    // Phase 42: the 3D held-item hand (the Minecraft hand). Rendered only
    // while the pointer is locked and no panel is open (the same gate as
    // the hotbar/held icon) — a no-input run is never locked, so it renders
    // the pre-Phase-42 image (byte-identical baseline). Pure rendering: it
    // reads the selected slot + the bow/eat channel state and draws one
    // model; it mutates nothing.
    if (!m_window.relativeMouseMode() || panelOpen()) {
        return;
    }
    const ItemStack& stack = m_inventory.at(m_selectedSlot);
    if (stack.empty()) {
        return; // empty hand: nothing to hold
    }
    const Item& it = m_items.at(stack.item);
    bool isBlock = it.type == ItemType::Block;
    std::size_t iconIdx = static_cast<std::size_t>(stack.item.value);
    const bool hasIcon =
        !isBlock && iconIdx < m_itemIcons.size() &&
        m_itemIcons[iconIdx].valid();
    // CRAFT-2: a model item renders its box mesh INSTEAD of the icon
    // quad (the block-cube hand is untouched). The bow-draw and eat-lift
    // camera transforms below apply to the model branch exactly as to
    // the icon branch (the eat lift is inherited — zero extra code).
    const bool hasModel =
        !isBlock && it.model.has_value() &&
        iconIdx < m_itemModelTextures.size() &&
        m_itemModelTextures[iconIdx].valid();
    if (!isBlock && !hasIcon && !hasModel) {
        return; // no hand model for this item (v1: no per-item 3D)
    }
    const float scale = isBlock ? kHandCubeScale : kHandIconScale;
    // Animation offsets (camera frame): the bow draw pulls the hand back
    // and tilts the bow toward the face with the charge; the eat channel
    // raises the hand toward the face with the progress.
    Vec3 offset = kHandOffset;
    float spinX = kHandSpinX;
    if (m_interaction.bowDrawing) {
        const float p = bowPower(m_interaction.bowDrawSeconds);
        offset.z += kHandDrawPullZ * p;
        spinX += kHandDrawSpinX * p;
    } else if (m_interaction.eatChannel.active) {
        const float p = m_interaction.eatChannel.progress();
        offset.y += kHandEatLiftY * p;
        offset.z += kHandEatPullZ * p;
    }
    // Camera -> world (yaw-then-pitch FPS convention, see Camera::forward):
    // C = T(pos) * Ry(yaw) * Rx(pitch). The hand sits in the camera frame
    // (x = right, y = up, z = behind the eye), spun + tilted, then scaled.
    const Mat4 item = Mat4::multiply(
        Mat4::rotateX(spinX),
        Mat4::multiply(Mat4::rotateY(kHandSpinY), Mat4::scale(scale)));
    const Mat4 place = Mat4::multiply(Mat4::translate(offset), item);
    const Mat4 cam = Mat4::multiply(
        Mat4::translate(m_camera.position),
        Mat4::multiply(Mat4::rotateY(m_camera.yaw),
                       Mat4::rotateX(m_camera.pitch)));
    const Mat4 model = Mat4::multiply(cam, place);

    // The basic shader's texture state is set at the top of the render
    // pass (atlas bound, u_useTexture = 1, u_dayFactor = world light). The
    // block-cube hand uses the atlas as-is; the icon hand binds its per-item
    // texture (and the atlas is restored for the passes that follow).
    // The program must be BIND first: Renderer::draw unbinds the program
    // after every draw, and a uniform call on an unbound program is
    // GL_INVALID_OPERATION (the residual error then poisons the lazy
    // mesh-upload's glGetError — same pattern as drawArrows).
    m_basicShader.bind();
    m_basicShader.setUniform1i("u_useTexture", 1);
    // Blending is enabled for the hand: the opaque pass runs with blending
    // off, but the icon texture carries transparent (alpha 0) pixels —
    // unblended they would draw as an opaque black square behind the icon
    // (translucent tiles such as glass cubes need it too).
    m_renderer.enableBlending();
    if (isBlock) {
        m_atlas.texture().bind();
        m_renderer.draw(itemMesh(it.block), m_basicShader, view, projection,
                        model);
    } else if (hasModel) {
        // CRAFT-2: the box-model hand — the per-item model texture bound
        // at draw time (the icon-hand's binding path), culling OFF for
        // the double-sided box (every face renders from any spin state),
        // the canonical unit geometry scaled by kHandIconScale in the
        // model matrix (the icon-quad footprint).
        m_itemModelTextures[iconIdx].bind();
        m_renderer.setCulling(false);
        m_renderer.draw(itemModelMesh(it.id), m_basicShader, view,
                        projection, model);
        m_renderer.setCulling(true);
        m_atlas.texture().bind(); // restore for the passes that follow
    } else {
        m_itemIcons[iconIdx].bind();
        m_renderer.draw(handIconMesh(), m_basicShader, view, projection,
                        model);
        m_atlas.texture().bind(); // restore for the passes that follow
    }
    m_renderer.disableBlending();
}

// ---------------------------------------------------------------------------
// Phase 15: UI keys, inventory, sounds, overlay and particles
// ---------------------------------------------------------------------------

void Game::drawWorldOverlay(const Mat4& view, const Mat4& projection) {
    // Only while playing (pointer captured, no inventory): the crosshair
    // aim is what the overlay describes. The crack is drawn over the
    // block being broken; the outline over whatever block the ray hits.
    // Both are world-space draws through the UI shader (unlit).
    const bool playing =
        m_window.relativeMouseMode() && !panelOpen();
    if (!playing) {
        return;
    }
    // The overlay is self-contained blending: the crack/outline are
    // alpha-composited over the block faces. The blend section's state
    // used to be assumed, but per-draw state leaks (10-26 user report:
    // a disableBlending from an earlier draw reached here and the crack
    // cube's (0,0,0,0) background painted the mined block solid black).
    m_renderer.enableBlending();
    if (m_interaction.breakTarget.has_value() && m_interaction.breakProgress > 0.0) {
        const BlockPos& b = *m_interaction.breakTarget;
        const Block& block =
            m_blocks.at(m_world.getBlock(b.x, b.y, b.z));
        const double duration = breakDurationSeconds(
            block.properties.hardness, selectedBreakSpeed(block));
        const int stage =
            duration > 0.0
                ? std::clamp(
                      static_cast<int>(std::floor(
                          (m_interaction.breakProgress / duration) * kCrackStages)),
                      0, kCrackStages - 1)
                : kCrackStages - 1;
        const Vec3 center {static_cast<float>(b.x) + 0.5f,
                           static_cast<float>(b.y) + 0.5f,
                           static_cast<float>(b.z) + 0.5f};
        m_uiShader.bind();
        m_uiShader.setUniform1i("u_texture", 0);
        m_uiShader.setUniform1i("u_useTexture", 1);
        m_uiShader.setUniform4f("u_color", 1.0f, 1.0f, 1.0f, 1.0f);
        m_crackTexture.bind();
        m_renderer.draw(m_crackMeshes[static_cast<std::size_t>(stage)],
                        m_uiShader, view, projection,
                        Mat4::translate(center));
    }
    if (const std::optional<BlockHit> hit = targetBlock()) {
        const Vec3 center {static_cast<float>(hit->block.x) + 0.5f,
                           static_cast<float>(hit->block.y) + 0.5f,
                           static_cast<float>(hit->block.z) + 0.5f};
        m_uiShader.bind();
        m_uiShader.setUniform1i("u_texture", 0);
        m_uiShader.setUniform1i("u_useTexture", 0);
        m_uiShader.setUniform4f("u_color", 0.0f, 0.0f, 0.0f, 0.85f);
        m_renderer.draw(m_outlineMesh, m_uiShader, view, projection,
                        Mat4::translate(center));
    }
    m_renderer.disableBlending();
}

void Game::drawParticles(const Mat4& view, const Mat4& projection,
                         RenderPass pass) {
    if (m_particles.empty()) {
        return;
    }
    // One dynamic batch per pass: all particles of matching blocks as a
    // single indexed mesh (a 0.12-block cube per particle, the broken
    // block's own faces). Light values were baked at spawn time.
    std::vector<MeshVertex> verts;
    std::vector<std::uint32_t> indices;
    const float half = 0.06f;
    for (const Particle& p : m_particles.particles()) {
        const Block& block = m_blocks.at(p.block);
        // PRE-5: the particle routes like its block — opaque blocks'
        // break particles stay in the opaque pass, `cutout` blocks'
        // (leaves, cross plants: the fire embers) join the cutout pass,
        // the remaining transparent blocks' the blend pass.
        const RenderPass want =
            !block.properties.transparent
                ? RenderPass::Opaque
                : (block.properties.cutout ? RenderPass::Cutout
                                           : RenderPass::Blend);
        if (pass != want) {
            continue;
        }
        for (const CubeFace& face : kCubeFaces) {
            // Cross blocks (Phase 45) carry no cube faces: the particle
            // cube uses the cross texture on every face (the same
            // single-lookup the item-mesh and chunk-mesher cross branches
            // use — cubeFaceTexture would return the empty top/side/bottom
            // name and the atlas lookup would throw). Fence / glass pane
            // (Phase 98/99) are the same rule for the thin side-only
            // shapes: their top/bottom names are empty by design.
            const std::string& tex =
                block.model.shape == BlockModel::Shape::Cross
                    ? block.model.textures.cross
                    : (block.model.shape == BlockModel::Shape::Fence ||
                       block.model.shape == BlockModel::Shape::GlassPane)
                          ? block.model.textures.side
                          : cubeFaceTexture(block, face.textureSlot);
            const Vec2 u0 = AtlasLayout::uvMin(m_atlas.layout(), tex);
            const Vec2 u1 = AtlasLayout::uvMax(m_atlas.layout(), tex);
            const Vec2 faceUv[4] = {
                {u0.x, u1.y}, {u1.x, u1.y}, {u1.x, u0.y}, {u0.x, u0.y},
            };
            const std::uint32_t first =
                static_cast<std::uint32_t>(verts.size());
            for (int i = 0; i < 4; ++i) {
                MeshVertex v;
                v.position = {
                    p.position.x +
                        (static_cast<float>(face.corner[i][0]) - 0.5f) * 2.0f * half,
                    p.position.y +
                        (static_cast<float>(face.corner[i][1]) - 0.5f) * 2.0f * half,
                    p.position.z +
                        (static_cast<float>(face.corner[i][2]) - 0.5f) * 2.0f * half};
                // Cross particles (the fire embers, Phase 66) take the
                // full-brightness face shade, like every other cross
                // rendering (the chunk-mesher and item-mesh cross
                // branches): the cube face normals would darken the
                // side faces to 0.6x and the embers would read as dim
                // specks against the bright flame body.
                v.normal =
                    block.model.shape == BlockModel::Shape::Cross
                        ? Vec3 {0.0f, 1.0f, 0.0f}
                        : face.normal;
                v.uv = faceUv[i];
                v.skyLight = p.skyLight;
                v.blockLight = p.blockLight;
                verts.push_back(v);
            }
            indices.insert(indices.end(),
                           {first, first + 1, first + 2, first, first + 2,
                            first + 3});
        }
    }
    if (verts.empty()) {
        return;
    }
    Mesh mesh;
    auto up = mesh.upload(verts, indices);
    if (!up.ok()) {
        MC_LOG_ERROR("Particle mesh upload failed: " + up.error().describe());
        return;
    }
    m_renderer.draw(mesh, m_basicShader, view, projection, Mat4::identity());
}

// ---------------------------------------------------------------------------
// Phase 15: the screen-space UI pass
// ---------------------------------------------------------------------------

void Game::render() {
    const int w = m_window.width();
    const int h = m_window.height();

    m_renderer.beginFrame(w, h);
    // The sky background is the day-cycle color (deep night blue at
    // midnight, orange at dawn/dusk, the classic blue by day).
    const Vec3 sky = m_dayCycle.skyColor();
    m_renderer.clear({sky.x, sky.y, sky.z, 1.0f});

    const Mat4 projection = Mat4::perspective(
        kFovDegrees * static_cast<float>(M_PI) / 180.0f,
        static_cast<float>(w) / static_cast<float>(h), kNear, kFar);
    const Mat4 view = m_camera.viewMatrix();

    // Shared uniforms (program state persists across draws). Lighting is
    // baked into the vertices by the mesher (Phase 7) — no light uniforms
    // except the day/night scale for sky light.
    m_basicShader.bind();
    m_basicShader.setUniform1i("u_texture", 0);
    m_basicShader.setUniform1i("u_useTexture", 1);
    m_basicShader.setUniform1f("u_dayFactor", m_dayCycle.dayFactor());
    // Solid-color alpha default 1.0 (opaque world + arrows; the beacon beam
    // lowers it for its own draws — Phase 83). The textured path ignores it.
    m_basicShader.setUniform1f("u_alpha", 1.0f);
    // One atlas tile in UV space, for the tiled greedy chunk quads
    // (Phase 18). Unread on the legacy (tiled = 0) path.
    m_basicShader.setUniform2f(
        "u_tileSize", 1.0f / static_cast<float>(m_atlas.layout().columns),
        1.0f / static_cast<float>(m_atlas.layout().rows));
    // PRE-2 distance fog: the streaming disc edge (raw chunk
    // cross-sections) dissolves into the horizon — a linear ramp toward
    // the same-frame sky color from kFogStartScale * end to end, where
    // end = min(far plane, effective stream radius in blocks). The
    // radius is m_streaming.streamRadius capped to kStreamCapRadius (STRM-1)
    // times kChunkSize — the same clamp the stream disk itself uses.
    // render.fog (code default true, the fire.particles getBool
    // precedent) gates it; a degenerate range (end <= start — a
    // zero/negative view distance) falls back to off. Four per-frame
    // uniform sets, zero per-draw cost; the streaming code is untouched.
    const bool fogConfigOn = m_config.getBool("render.fog", true);
    const float rEffBlocks = static_cast<float>(
                                 std::min(m_streaming.streamRadius, kStreamCapRadius)) *
                             kChunkSize;
    const FogRange fog = fogDistanceRange(rEffBlocks, kFar);
    const bool fogOn = fogConfigOn && fog.end > fog.start;
    m_basicShader.setUniform1i("u_fogEnabled", fogOn ? 1 : 0);
    m_basicShader.setUniform1f("u_fogStart", fog.start);
    m_basicShader.setUniform1f("u_fogEnd", fog.end);
    m_basicShader.setUniform3f("u_fogColor", sky.x, sky.y, sky.z);
    // PRE-3 water ripples: the frame-locked phase of the water-top UV
    // wander (the white-noise tile shimmers as its sample window moves —
    // the vertex computes the offset, the fragment's tiled branch wraps
    // it inside the water tile). render.water_ripples (code default true,
    // the render.fog getBool precedent) gates it; OFF leaves the v0.26.0
    // pipeline bit-identical — the vertex branch never executes
    // (u_ripplesEnabled = 0 short-circuits before the a_water read, so
    // v_rippleOff = (0,0) for every vertex and fract(x + (0,0)) = x
    // bit-for-bit) and every non-water-top vertex carries water = 0.
    // u_tileSize (the third ripple-related uniform) is the same program
    // uniform already set above for the tiled greedy quads — no second
    // set needed. Two new per-frame uniform sets, zero per-draw cost.
    const bool rippleOn = m_config.getBool("render.water_ripples", true);
    const float ripplePhase =
        2.0f * static_cast<float>(m_frameCount % kRipplePeriodFrames) /
        static_cast<float>(kRipplePeriodFrames) *
        (static_cast<float>(M_PI) * 2.0f);
    m_basicShader.setUniform1i("u_ripplesEnabled", rippleOn ? 1 : 0);
    m_basicShader.setUniform1f("u_ripplePhase", ripplePhase);
    // PRE-5 alpha cutout: the render.alpha_test gate (code default true,
    // the render.fog / render.clouds getBool precedent — config.json is
    // untouched). ON = the fragment's `alpha < 0.5 -> discard` branch
    // runs on every basic-program draw (bucket-independent: the cutout
    // holes AND the glass frame AND the item-icon backgrounds), and the
    // blend section below drops its depth write (setDepthMask(false));
    // OFF = the branch is never taken and the depth mask is never
    // touched, so the pipeline is the v0.28.0 one bit-for-bit (the S1
    // three-way gate). One per-frame uniform set, zero per-draw cost.
    const bool alphaTestOn = m_config.getBool("render.alpha_test", true);
    m_basicShader.setUniform1i("u_discardEnabled", alphaTestOn ? 1 : 0);

    m_atlas.texture().bind();

    // Frustum culling (Phase 99.9): the chunk passes must not submit draw
    // calls for chunks the camera cannot see. At view distance 80 the
    // loaded ring holds ~200k chunks while the frustum covers a few
    // hundred; drawing them all is what collapsed the vd80 frame to
    // seconds. The test is conservative (a box is culled only when fully
    // outside one plane), so a culled chunk had zero screen coverage —
    // the rendered pixels are unchanged for any camera (byte-identical
    // no-input baseline). Entities/particles are player-scale and stay
    // unculled here (their own draw calls are few).
    const Mat4 mvp = Mat4::multiply(projection, view);
    const Frustum frustum = Frustum::fromMVP(mvp);
    const auto columnVisible = [&frustum](const ColumnPos& col) {
        const float x0 = static_cast<float>(col.x * kChunkSize);
        const float z0 = static_cast<float>(col.z * kChunkSize);
        return frustum.intersectsAabb(
            Vec3{x0, 0.0f, z0},
            Vec3{x0 + kChunkSize, static_cast<float>(kWorldHeight),
                 z0 + kChunkSize});
    };

    // Column passes: all opaque geometry first, then the PRE-5 cutout
    // geometry (discard + depth write), then the translucent (blended)
    // geometry (no depth write with alpha_test on), so blending composites
    // over the opaque + cutout scene. Phase 99.9: one merged mesh per
    // COLUMN (its eight chunks) instead of
    // one per chunk — the per-chunk submission was ~10-30 us/draw on the
    // Intel iGPU and grew with the view-distance ring (the vd80 render
    // collapse). The column list is m_streaming.loadedColumns (the incremental
    // mirror): no per-frame chunkPositions() snapshot + sort of the ~200k
    // loaded chunks.
    const std::vector<ColumnPos> columns(m_streaming.loadedColumns.begin(),
                                         m_streaming.loadedColumns.end());
    for (const ColumnPos& col : columns) {
        if (!columnVisible(col)) {
            continue;
        }
        const auto it = m_columnMeshes.find(col);
        if (it == m_columnMeshes.end() || !it->second.opaque.has_value()) {
            continue;
        }
        m_renderer.draw(*it->second.opaque, m_basicShader, view, projection,
                        columnModel(col));
    }

    // Dropped items (Phase 10): the opaque ones with the chunk pass; the
    // cutout ones (leaves, the pure-cutout model items, PRE-5) join the
    // cutout pass below; the remaining transparent ones (glass) the
    // blending pass.
    drawItemEntities(view, projection, RenderPass::Opaque);
    // Mobs (Phase 20): opaque box models, with the opaque pass (the depth
    // test then occludes them behind terrain exactly like the chunks).
    drawMobs(view, projection);
    // UX-3: the player's world-space body — third person ONLY
    // (m_cameraMode != First); UX-3 ships First = a no-op on every run
    // (baseline byte-identical, the structural-immunity root).
    drawPlayerBody(view, projection);
    // Arrows (Phase 21): the bow's in-flight projectiles, solid color.
    drawArrows(view, projection);
    // 3D held hand (Phase 42): the selected item's view model, in the
    // opaque pass (the depth test occludes it behind terrain like
    // everything else). No-op unless the pointer is locked and no panel is
    // open (a no-input run never renders it — baseline byte-identical).
    drawHeldHand(view, projection);
    // Campfire cooking items (Phase 71): the item icon floating above each
    // campfire with a non-empty slot — opaque pass, depth-occluded. No-op
    // without a cooking campfire (baseline byte-identical).
    drawCampfireCookItems(view, projection);
    // Opaque break particles (Phase 15) belong to the opaque pass so the
    // transparent geometry blends over them.
    drawParticles(view, projection, RenderPass::Opaque);

    m_renderer.enableBlending();
    // Sky objects (unlit billboarded quads at kSkyDistance): drawn after
    // the opaque pass so the depth test occludes them behind terrain, and
    // before the transparent pass (they never overlap chunk geometry in
    // depth — they sit behind everything in the demo area).
    drawSkyObjects(view, projection, h);

    // PRE-5 CUTOUT section (always after the sky, before the blend
    // section — the v0.28.0 relative order for a water/glass-free scene,
    // which is what the OFF immunity rests on): the leaves + cross-plant
    // column geometry, the cutout item drops and the cutout particles.
    // With render.alpha_test ON the discard is active (u_discardEnabled,
    // set above) AND the depth write is on (the GL default — the cutout
    // surface occludes what is behind its SOLID pixels, the holes write
    // no depth = the visibility fix). With it OFF the section is the old
    // transparent section bit-for-bit (no discard, depth write on).
    for (const ColumnPos& col : columns) {
        if (!columnVisible(col)) {
            continue;
        }
        const auto it = m_columnMeshes.find(col);
        if (it == m_columnMeshes.end() || !it->second.cutout.has_value()) {
            continue;
        }
        m_renderer.draw(*it->second.cutout, m_basicShader, view, projection,
                        columnModel(col));
    }
    drawItemEntities(view, projection, RenderPass::Cutout);
    drawParticles(view, projection, RenderPass::Cutout);

    // PRE-5 BLEND section: the translucent geometry (glass, the pane, the
    // fluids, the translucent items/particles, the beams, the clouds, the
    // world overlay) with the depth WRITE off — a translucent surface
    // composites over what is behind it and must not occlude it. With
    // render.alpha_test OFF the mask is never touched (the GL default
    // stays on) and the pipeline is the v0.28.0 one bit-for-bit (for a
    // water/glass-free scene; the mixed-scene order deviation is the
    // documented PRE-5 one).
    if (alphaTestOn) {
        m_renderer.setDepthMask(false);
    }
    for (const ColumnPos& col : columns) {
        if (!columnVisible(col)) {
            continue;
        }
        const auto it = m_columnMeshes.find(col);
        if (it == m_columnMeshes.end() ||
            !it->second.transparent.has_value()) {
            continue;
        }
        m_renderer.draw(*it->second.transparent, m_basicShader, view,
                        projection, columnModel(col));
    }
    drawItemEntities(view, projection, RenderPass::Blend);
    drawParticles(view, projection, RenderPass::Blend);
    // Beacon beams (Phase 83): the translucent light columns from active
    // beacons, blended + depth-occluded like the other world geometry.
    // No-op without an active beacon (baseline byte-identical).
    drawBeaconBeams(view, projection);
    // PRE-4 clouds: the drifting translucent layer at y = kCloudHeight,
    // blended + depth-occluded like the other world geometry. Gated on
    // render.clouds (code default true); OFF = zero draw (the v0.27.0
    // pipeline bit-identical). The cached mesh rebuilds only when the
    // camera's cloud cell or the drift step changes.
    drawClouds(view, projection);
    // The break crack + target outline (Phase 15): world-space, blended,
    // depth test still on (the overlay cubes sit just outside the block).
    drawWorldOverlay(view, projection);
    m_renderer.disableBlending();
    if (alphaTestOn) {
        m_renderer.setDepthMask(true); // restore the GL default
    }

    // Phase 15 UI pass: screen space (y-down ortho), depth test + culling
    // off. Every element is gated on a pointer/key state, so a no-input
    // run renders exactly the Phase 14 image (byte-identical baseline).
    m_renderer.setDepthTest(false);
    m_renderer.setCulling(false);
    m_renderer.enableBlending();
    renderUi();
    m_renderer.disableBlending();
    m_renderer.setDepthTest(true);
    m_renderer.setCulling(true);

    m_renderer.endFrame();
}

void Game::drawSkyObjects(const Mat4& view, const Mat4& projection,
                          int viewportHeight) {
    // View-space length of one pixel at the sprite's depth (see sky.vert).
    const float pxToWorld = 2.0f *
                             std::tan(kFovDegrees * static_cast<float>(M_PI) /
                                      360.0f) /
                             static_cast<float>(viewportHeight);

    const float starAlpha = m_dayCycle.starAlpha();
    if (starAlpha > 0.0f) {
        m_skyShader.bind();
        m_skyShader.setUniformMat4("u_view", view);
        m_skyShader.setUniformMat4("u_proj", projection);
        m_skyShader.setUniform3f("u_offset", 0.0f, 0.0f, 0.0f);
        m_skyShader.setUniform2f("u_sizePixels", kStarSpriteSize,
                                 kStarSpriteSize);
        m_skyShader.setUniform1f("u_pxToWorld", pxToWorld);
        m_skyShader.setUniform4f("u_color", 1.0f, 1.0f, 1.0f, starAlpha);
        m_renderer.drawSky(m_starMesh, m_skyShader, view, projection);
    }

    // Sun and moon share the single quad, offset along the celestial
    // direction. Both fade out as they cross the horizon.
    const Vec3 sun = m_dayCycle.sunDirection();
    const float sunAlpha = std::clamp(sun.y / kHorizonFade, 0.0f, 1.0f);
    if (sunAlpha > 0.0f) {
        m_skyShader.bind();
        m_skyShader.setUniformMat4("u_view", view);
        m_skyShader.setUniformMat4("u_proj", projection);
        const Vec3 sunPos = sun * static_cast<float>(kSkyDistance);
        m_skyShader.setUniform3f("u_offset", sunPos.x, sunPos.y, sunPos.z);
        m_skyShader.setUniform2f("u_sizePixels", kSunSpriteSize,
                                 kSunSpriteSize);
        m_skyShader.setUniform1f("u_pxToWorld", pxToWorld);
        m_skyShader.setUniform4f("u_color", 1.0f, 0.93f, 0.78f, sunAlpha);
        m_renderer.drawSky(m_skyPointMesh, m_skyShader, view, projection);
    }

    const Vec3 moon = m_dayCycle.moonDirection();
    const float moonAlpha = std::clamp(moon.y / kHorizonFade, 0.0f, 1.0f);
    if (moonAlpha > 0.0f) {
        m_skyShader.bind();
        m_skyShader.setUniformMat4("u_view", view);
        m_skyShader.setUniformMat4("u_proj", projection);
        const Vec3 moonPos = moon * static_cast<float>(kSkyDistance);
        m_skyShader.setUniform3f("u_offset", moonPos.x, moonPos.y, moonPos.z);
        m_skyShader.setUniform2f("u_sizePixels", kMoonSpriteSize,
                                 kMoonSpriteSize);
        m_skyShader.setUniform1f("u_pxToWorld", pxToWorld);
        m_skyShader.setUniform4f("u_color", 0.85f, 0.88f, 1.0f, moonAlpha);
        m_renderer.drawSky(m_skyPointMesh, m_skyShader, view, projection);
    }
}

// Capture the front buffer (valid right after swapBuffers) into a PNG.
// Called on the game thread with the GL context current.
Result<void> Game::takeScreenshot(const std::filesystem::path& path) {
    const int w = m_window.width();
    const int h = m_window.height();
    std::vector<std::uint8_t> pixels(static_cast<std::size_t>(w) * h * 4);
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    const GLenum glErr = glGetError();
    if (glErr != GL_NO_ERROR) {
        return MC_FAIL("glReadPixels failed: " +
                       std::to_string(static_cast<unsigned>(glErr)));
    }

    // glReadPixels delivers rows bottom-to-top; PNG needs top-to-bottom.
    PngImage image;
    image.width = w;
    image.height = h;
    image.rgba.resize(pixels.size());
    for (int y = 0; y < h; ++y) {
        std::memcpy(image.rgba.data() + static_cast<std::size_t>(y) * w * 4,
                    pixels.data() + static_cast<std::size_t>(h - 1 - y) * w * 4,
                    static_cast<std::size_t>(w) * 4);
    }

    // The framebuffer alpha is a blend-state byproduct (e.g. water's texture
    // alpha), not part of the displayed image: screenshots are opaque.
    for (std::size_t i = 3; i < image.rgba.size(); i += 4) {
        image.rgba[i] = 255;
    }

    auto png = Png::encode(image);
    MC_TRY(png);
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        return MC_FAIL("cannot write screenshot file: '" + path.string() + "'");
    }
    out.write(png->data(), static_cast<std::streamsize>(png->size()));
    MC_LOG_INFO("Screenshot saved: " + path.string() + " (" +
                std::to_string(w) + "x" + std::to_string(h) + ")");
    return {};
}

} // namespace mc
