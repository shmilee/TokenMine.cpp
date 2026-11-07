#include "core/game.hpp"

#include <GL/glew.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <numbers>

#include "block/cube_face_table.hpp"
#include "core/bow.hpp"
#include "core/first_person_arm.hpp"
#include "core/game_local.hpp"
#include "core/math/frustum.hpp"
#include "core/png.hpp"
#include "entity/blob_shadow.hpp"
#include "entity/breath_cloud.hpp"
#include "renderer/alpha_test.hpp"
#include "renderer/fog.hpp"
#include "ui/ui_scale.hpp"
#include "world/beam_light.hpp"

namespace mc {

namespace {

// PRE-3 water ripples: the shimmer's phase period in FRAMES (2.0 s at the
// 60 FPS cap — the frame-locked class, hard rule 8; the phase is a pure
// function of m_frameCount, so the animation is byte-reproducible and the
// period is a named constant with no config key). The u_ripplePhase uniform
// sweeps 0..2*pi over the period; the frame MODULO keeps the sin argument
// bounded (float precision loss over a long session).
constexpr int kRipplePeriodFrames = 120;

// V3-15: the portal's UV scroll period in FRAMES (2.0 s at the 60 FPS
// cap — the frame-locked class, the ripple's twin: one FULL tile
// scroll per period; the texture is 2-periodic horizontally, so the
// wrap is seamless).
constexpr int kPortalAnimPeriodFrames = 120;

// Sprite sizes in pixels (billboarded quads — see sky.vert).
constexpr float kStarSpriteSize = 2.0f;

constexpr float kSunSpriteSize = 40.0f;

constexpr float kMoonSpriteSize = 28.0f;

// Sun/moon fade-out: alpha = clamp(elevation / kHorizonFade, 0, 1), so they
// are fully visible a little above the horizon and invisible on/below it.
constexpr float kHorizonFade = 0.1f;

// V2-6 moon phase: the dark "eclipse" overlay quad — the same 28px disc as
// the moon (zero new texture: the sky shader's solid-color path), colored
// the deep-night sky keyframe (day_cycle's kNightSky): the covered disc
// region reads as sky, the crescent illusion.
constexpr float kMoonPhaseColorR = 0.02f;
constexpr float kMoonPhaseColorG = 0.03f;
constexpr float kMoonPhaseColorB = 0.08f;

constexpr float kBeaconBeamBaseHeight = 64.0f;

// V3-1 Phase B (D7): the nether's render constants (the skyless dimension
// — no sun / moon / stars, no clouds, no rain; the clear color is the
// dark-red nether void, and the fog is the short-range dark red — the
// Minecraft nether's fog start 10 / end 96, the Bedrock JSON per the
// recon §二.3/§十). Named constants, zero config (the kSkyDistance
// discipline). The overworld's render path never reads them (the inNether
// gate below — the default-off bit-identity).
constexpr float kNetherSkyColorR = 0.16f;
constexpr float kNetherSkyColorG = 0.04f;
constexpr float kNetherSkyColorB = 0.04f;
constexpr float kNetherFogStart = 10.0f;
constexpr float kNetherFogEnd = 96.0f;

// V3-10a: the End's sky + fog (the nether's constant family's third
// member — the Minecraft End's sky #000000 with the void-violet read +
// the THICK short fog, the recon's §5 ruling: the island is lit by the
// ambient floor only, the fog swallows the void's edge early). Named
// constants, zero config (the kSkyDistance discipline). The overworld /
// nether render paths never read them (the inEnd gate — the default-off
// bit-identity).
constexpr float kEndSkyColorR = 0.05f;
constexpr float kEndSkyColorG = 0.03f;
constexpr float kEndSkyColorB = 0.06f;
constexpr float kEndFogStart = 8.0f;
constexpr float kEndFogEnd = 48.0f;

constexpr float kBeaconBeamLevelHeight = 32.0f;

// Phase 83 beacon beam color (bright cyan-white, the classic beam) and its
// translucency (the beam is a light column, not a solid pillar).
constexpr float kBeaconBeamR = 0.6f;

constexpr float kBeaconBeamG = 0.95f;

constexpr float kBeaconBeamB = 1.0f;

constexpr float kBeaconBeamAlpha = 0.5f;

// V3-10d dragon FX: the heal beam's pale-violet (the crystal's channeling
// light — the MC end-crystal beam's violet read) + its translucency, and
// the breath cloud's darker violet disc (the dragon_breath effect's
// color, the D11's read). The beam's half-width scale (the unit column's
// x/z, the beacon's kBeaconBeamHalfWidth's thinner boss read).
constexpr float kDragonBeamR = 0.75f;
constexpr float kDragonBeamG = 0.45f;
constexpr float kDragonBeamB = 0.9f;
constexpr float kDragonBeamAlpha = 0.45f;
constexpr float kDragonBeamHalfWidth = 0.18f;
constexpr float kDragonCloudR = 0.45f;
constexpr float kDragonCloudG = 0.2f;
constexpr float kDragonCloudB = 0.6f;
constexpr float kDragonCloudAlpha = 0.35f;

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
    // V2-4: the weather-adjusted dayFactor (the same convergence point as
    // the per-frame set at the top of render() — the beam's sky term
    // dims with the rain, pulses with the thunder, like every other
    // world-lit draw).
    m_basicShader.setUniform1f("u_dayFactor", weatherDayFactor());
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

// V3-10d: the dragon FX's lazy unit column (the heal beam's geometry —
// a vertical box spanning y 0..1, kDragonBeamHalfWidth in x/z, both
// faces (culling off at the draw). Built once on the first beam frame;
// a no-dragon session never builds it (the m_mobShadowMesh's lazy
// twin). The breath cloud reuses it scaled flat (the disc's box read).
const Mesh& Game::dragonFxMesh() {
    if (m_dragonFxMesh.valid()) {
        return m_dragonFxMesh;
    }
    const float h = kDragonBeamHalfWidth;
    const Vec3 kCorners[8] = {
        {-h, 0.0f, -h}, {h, 0.0f, -h}, {h, 0.0f, h}, {-h, 0.0f, h},
        {-h, 1.0f, -h}, {h, 1.0f, -h}, {h, 1.0f, h}, {-h, 1.0f, h},
    };
    const std::uint32_t kIndices[36] = {
        0, 1, 5, 0, 5, 4, // -Z
        2, 3, 7, 2, 7, 6, // +Z
        0, 4, 3, 0, 3, 7, // -X
        1, 2, 6, 1, 6, 5, // +X
        0, 3, 2, 0, 2, 1, // -Y
        4, 5, 6, 4, 6, 7, // +Y
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
    auto r = m_dragonFxMesh.upload(verts, indices, MeshMode::Triangles);
    if (!r.ok()) {
        MC_LOG_ERROR("Dragon FX mesh build failed: " + r.error().describe());
    }
    return m_dragonFxMesh;
}

void Game::drawDragonBeam(const Mat4& view, const Mat4& projection) {
    // V3-10d: the crystal->dragon HEAL beam (the beacon beam's blend
    // twin): a pale-violet column rising from the healing source
    // crystal (the m_end.m_dragonHealSlot's cell, set by the gameplay
    // pass's 30-frame cadence) to the dragon's altitude. A no-dragon /
    // full-health / no-crystal session is one early return (the
    // structural-immunity root — the m_dragonHealSlot is -1).
    if (m_end.m_dragon == nullptr || m_end.m_dragonHealSlot < 0) {
        return;
    }
    const Vec3 slot = kEndCrystalSlots[m_end.m_dragonHealSlot];
    const float cx = slot.x;
    const float cy = slot.y; // the crystal cell's center
    const float cz = slot.z;
    const float height =
        std::max(0.1f, m_end.m_dragon->position().y - (cy - 0.5f));
    const Mat4 model = Mat4::multiply(
        Mat4::translate(cx, cy, cz),
        Mat4::scale(1.0f, height, 1.0f));
    m_basicShader.bind();
    m_basicShader.setUniform1i("u_useTexture", 0);
    m_basicShader.setUniform3f("u_color", kDragonBeamR, kDragonBeamG,
                               kDragonBeamB);
    m_basicShader.setUniform1f("u_alpha", kDragonBeamAlpha);
    m_basicShader.setUniform1f("u_dayFactor", weatherDayFactor());
    m_renderer.setCulling(false);
    m_renderer.draw(dragonFxMesh(), m_basicShader, view, projection, model);
    m_renderer.setCulling(true);
    // (blending state left to the caller — the drawBeaconBeams head's
    // leak lesson)
}

void Game::drawBreathClouds(const Mat4& view, const Mat4& projection) {
    // V3-10d: the dragon's BREATH CLOUDS (the blend-pass translucent
    // discs — the spawnSteam's blend twin): each live cloud's flat box
    // (the unit column scaled to its radius x the kThickness), centered
    // on the cloud, tinted the breath's violet. A no-cloud session is
    // one early return per null (the structural-immunity root).
    const Mesh& mesh = dragonFxMesh();
    if (!mesh.valid()) {
        return;
    }
    m_basicShader.bind();
    m_basicShader.setUniform1i("u_useTexture", 0);
    m_basicShader.setUniform3f("u_color", kDragonCloudR, kDragonCloudG,
                               kDragonCloudB);
    m_basicShader.setUniform1f("u_alpha", kDragonCloudAlpha);
    m_basicShader.setUniform1f("u_dayFactor", weatherDayFactor());
    m_renderer.setCulling(false);
    for (const auto& entity : m_end.entities) {
        const BreathCloudEntity* cloud =
            dynamic_cast<const BreathCloudEntity*>(entity.get());
        if (cloud == nullptr) {
            continue;
        }
        const Vec3 p = cloud->position();
        const float r = cloud->radius();
        const Mat4 model = Mat4::multiply(
            Mat4::translate(p.x, p.y - BreathCloudEntity::kThickness * 0.5f,
                            p.z),
            Mat4::scale(r * 2.0f, BreathCloudEntity::kThickness,
                        r * 2.0f));
        m_renderer.draw(mesh, m_basicShader, view, projection, model);
    }
    m_renderer.setCulling(true);
    // (blending state left to the caller — the drawBeaconBeams head's
    // leak lesson)
}

void Game::drawClouds(const Mat4& view, const Mat4& projection) {
    // UX-9a: the drifting translucent cloud layer (world/cloud) — the
    // MESH is the one-time kCloudQuadSize quad uploaded at init; the
    // PATTERN is computed per pixel by m_cloudShader (cloud.frag — an
    // exact GLSL port of the cloudMask ∧ cloudSubMask field, the CPU
    // functions in world/cloud stay the oracle). World-anchored: the
    // pattern does not move with the camera (no drift, no window-edge
    // pop-in), and nothing is uploaded after init (the old per-16-block
    // window mesh rebuild cost the iGPU ~140 ms per MB of buffer update
    // — the hitch that motivated the shader port).
    // render.clouds (code default true, the render.fog getBool precedent
    // — config.json is untouched) gates the draw; OFF = zero draw (the
    // v0.27.0 pipeline bit-identical — the gate's "new-off" control).
    // The depth test stays on (terrain occludes the layer like any other
    // world geometry); blending is the caller's (the blend section's)
    // state — do NOT enable/disable it here (10-26 user-report fix: the
    // trailing disableBlending leaked into drawWorldOverlay, whose crack
    // cube then drew unblended — its (0,0,0,0) background painted the
    // mined block solid black).
    if (!m_config.getBool("render.clouds", true)) {
        return;
    }
    const Vec3 eye = m_camera.position;
    // The frame-locked drift time step (m_frameCount /
    // kCloudFramesPerCell, hard rule 8) — the shader re-rolls the field
    // on it, so a no-input run is static from its first frame.
    const int t = cloudTimeStep(m_frameCount);
    // The quad's anchor: the camera's 4-block SUB-cell origin minus half
    // the quad (kCloudQuadSize / 2 = 8192, a multiple of 4 — the anchor
    // stays on the world sub lattice for every eye position; floorDiv is
    // negative-safe, hard rule 1). The pattern is world-anchored, so this
    // translation moves the QUAD, not the clouds.
    const float qx = 4.0f *
                         floorDiv(static_cast<int>(std::floor(eye.x)), 4) -
                     static_cast<float>(kCloudQuadSize / 2);
    const float qz = 4.0f *
                         floorDiv(static_cast<int>(std::floor(eye.z)), 4) -
                     static_cast<float>(kCloudQuadSize / 2);
    // The fragment's uniforms (the program must be bound first —
    // Renderer::draw re-binds the same program inside, no side effect).
    m_cloudShader.bind();
    m_cloudShader.setUniform1i("u_t", t);
    m_cloudShader.setUniform1f("u_alpha", kCloudAlpha);
    m_cloudShader.setUniform2f("u_camXZ", eye.x, eye.z);
    // u_model / u_view / u_proj are set by name inside Renderer::draw
    // (the beam's Mat4::translate precedent — the quad is local
    // 0..kCloudQuadSize at y = kCloudHeight, translated into place).
    m_renderer.setCulling(false);
    m_renderer.draw(m_cloudMesh, m_cloudShader, view, projection,
                    Mat4::translate(qx, 0.0f, qz));
    m_renderer.setCulling(true);
}

float Game::weatherDayFactor() const {
    // V2-4: the dayFactor channel's single convergence point (the design
    // decision — every per-frame consumer reads this one value; the
    // shader expression trees are untouched). The base day/night factor,
    // RAIN-dimmed by kRainSkyDim (the named constant), plus the thunder's
    // 5-frame brightness pulse (thunderPulse on the last strike frame, 0
    // with no strike in the window). Weather off / clear returns
    // m_dayCycle.dayFactor() bit-for-bit: the IEEE identities (x * 1.0f
    // == x, x + 0.0f == x) keep the v2.3.0 pipeline exact.
    return m_dayCycle.dayFactor() * (m_weather.raining ? kRainSkyDim : 1.0f) +
           thunderPulse(m_frameCount, m_weatherThunderFrame);
}

void Game::drawRain(const Mat4& view, const Mat4& projection) {
    // V2-4: the rain drops — the static kRainDropCount-drop mesh (built
    // at init, uploaded ONCE — the cloud-quad fourth precedent: zero
    // per-frame upload; the vertex shader re-derives each drop's
    // position from the two per-frame uniforms below, the CPU oracle is
    // rainDropPosition in core/weather). The depth test stays on
    // (terrain occludes the drops like any other world geometry);
    // blending is the caller's (the blend section's) state — do NOT
    // enable/disable it here (the drawBeaconBeams leak lesson); culling
    // is OFF for the double-sided bar quads (the beam pattern).
    // RAIN only: clear weather (the default runs — the first segment is
    // always >= kClearMin frames, so a no-input run never sees rain in
    // the base-gate window) or render.weather OFF (the machine never
    // runs, m_weather.raining stays false) = zero drops = the v2.3.0
    // pipeline bit-identical.
    if (!m_weather.raining) {
        return;
    }
    // The kRainBoxSize^3 box centered on the player's cell (the floor on
    // each axis — the box origin uniform's documented meaning; the hard
    // rule 1 floor on the exact player position).
    const Vec3 p = localPlayer()->position();
    const float ox = static_cast<float>(std::floor(p.x)) - kRainBoxHalf;
    const float oy = static_cast<float>(std::floor(p.y)) - kRainBoxHalf;
    const float oz = static_cast<float>(std::floor(p.z)) - kRainBoxHalf;
    // The program must be BIND first: Renderer::draw re-binds the same
    // program inside (no side effect on the uniforms set here).
    m_rainShader.bind();
    m_rainShader.setUniform3f("u_boxOrigin", ox, oy, oz);
    // The bounded fall offset ((frame % kRainFallPeriodFrames) *
    // kRainDropFallSpeed, in [0, kRainBoxSize) — kRainDropFallSpeed *
    // kRainFallPeriodFrames = kRainBoxSize makes the box wrap seamless;
    // the bound keeps the magnitude float32-exact over a long session,
    // the cloud shader's float premise).
    m_rainShader.setUniform1f(
        "u_fallOffset",
        static_cast<float>(m_frameCount % kRainFallPeriodFrames) *
            kRainDropFallSpeed);
    // The drop's white-blue + alpha (the no-texture decision — a flat
    // unlit color, the beam's solid-color path analog).
    m_rainShader.setUniform4f("u_color", kRainDropColorR, kRainDropColorG,
                              kRainDropColorB, kRainDropAlpha);
    m_renderer.setCulling(false);
    m_renderer.draw(m_rainMesh, m_rainShader, view, projection,
                    Mat4::identity());
    m_renderer.setCulling(true);
}

void Game::drawHeldHand(const Mat4& view, const Mat4& projection) {
    // Phase 42: the 3D held-item hand (the Minecraft hand). Rendered only
    // while the pointer is locked and no panel is open (the same gate as
    // the hotbar/held icon) — a no-input run is never locked, so it renders
    // the pre-Phase-42 image (byte-identical baseline; the arm's lazy mesh
    // below is never even built). UX-4: hidden in third person too (the
    // world-space body is visible instead). V2-1: the gate is "not first
    // person", so the front view hides it as well. V2-6: the FIRST-PERSON
    // ARM (the player's data-model 'arm_r' box — the Steve-skin pa_arm
    // tile, zero new texture; see core/first_person_arm.hpp) draws under
    // the held item for every gated frame: an empty hand shows the bare
    // skin hand (the tile's lower rows); the pose is static (no walking
    // swing — the documented deviation) but follows the item's bow-draw /
    // eat-lift offset, so the hand carries the arm.
    // Pure rendering: it reads the selected slot + the bow/eat channel
    // state and draws the arm + one item model; it mutates nothing.
    if (!firstPersonArmVisible(m_cameraMode, m_window.relativeMouseMode(),
                               panelOpen())) {
        return;
    }
    const ItemStack& stack = localSession().inventory.at(localSession().selectedSlot);
    // Animation offsets (camera frame): the bow draw pulls the hand back
    // and tilts the bow toward the face with the charge; the eat channel
    // raises the hand toward the face with the progress. The arm shares
    // the offset (firstPersonArmLocal), so it follows these channels.
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
    const Mat4 cam = Mat4::multiply(
        Mat4::translate(m_camera.position),
        Mat4::multiply(Mat4::rotateY(m_camera.yaw),
                       Mat4::rotateX(m_camera.pitch)));

    // V2-6: the first-person arm — the data model's 'arm_r' box
    // (firstPersonArmMesh, built lazily) in the camera frame
    // (firstPersonArmLocal: the item's offset + the fixed delta + the
    // static lean). Drawn UNDER the item (the item sits in the hand):
    // opaque atlas texture, blending stays OFF (the item branch below
    // enables it for its transparent pixels).
    m_basicShader.bind();
    m_basicShader.setUniform1i("u_useTexture", 1);
    m_atlas.texture().bind();
    m_renderer.draw(firstPersonArmMesh(), m_basicShader, view, projection,
                    Mat4::multiply(cam, firstPersonArmLocal(offset)));
    if (stack.empty()) {
        return; // empty hand: the arm (skin hand) only
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
        return; // no hand model for this item (v1: no per-item 3D) —
                // the arm (bare hand) stays
    }
    const float scale = isBlock ? kHandCubeScale : kHandIconScale;
    const Mat4 item = Mat4::multiply(
        Mat4::rotateX(spinX),
        Mat4::multiply(Mat4::rotateY(kHandSpinY), Mat4::scale(scale)));
    const Mat4 place = Mat4::multiply(Mat4::translate(offset), item);
    const Mat4 model = Mat4::multiply(cam, place);

    // The basic shader's texture state is set at the top of the render
    // pass (atlas bound, u_useTexture = 1, u_dayFactor = world light). The
    // block-cube hand uses the atlas as-is; the icon hand binds its per-item
    // texture (and the atlas is restored for the passes that follow).
    // The program must be BIND first: Renderer::draw unbinds the program
    // after every draw (the arm's above), and a uniform call on an unbound
    // program is GL_INVALID_OPERATION (the residual error then poisons the
    // lazy mesh-upload's glGetError — same pattern as drawArrows).
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
        // V3-1 Phase B: the crack's block read is the CURRENT world (the
        // break target is a cell in the dimension the player is in).
        const Block& block =
            m_blocks.at(currentWorld().getBlock(b.x, b.y, b.z));
        const double duration = breakDurationSeconds(
            block.properties.hardness, selectedBreakSpeedFor(localSession(), block));
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
                          : cubeFaceTexture(block, face.textureSlot,
                                            face.normal);
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
    // midnight, orange at dawn/dusk, the classic blue by day). V2-4: the
    // weather's sky channel — RAIN multiplies the sky by kRainSkyDim (the
    // named constant) and the thunder adds the 5-frame brightness pulse
    // (thunderPulse) to every channel: the u_dayFactor uniform above,
    // THIS clear and the fog color (u_fogColor = this same sky value) all
    // consume the one converged weather-adjusted value — the shader
    // expression trees are untouched (the fog auto-inherits; the clouds
    // read the darkened sky between their tiles). Weather off / clear =
    // the IEEE identities (x * 1.0f == x, x + 0.0f == x): the v2.3.0
    // pipeline bit-for-bit.
    const float weatherDim = m_weather.raining ? kRainSkyDim : 1.0f;
    const float weatherPulse =
        thunderPulse(m_frameCount, m_weatherThunderFrame);
    // V3-1 Phase B (D7 point 1): the CLEAR color — the nether is the
    // dark red void (no skybox: the day cycle's color + the weather's
    // dim / pulse are the overworld sky's channels, the nether's render
    // does not consume them; the overworld's expression is unchanged —
    // the bit-identical red line).
    // V3-10a: the three-way (the End's constant dark-violet — the day /
    // weather channels are the overworld sky's, both the nether's and
    // the End's renders do not consume them; the overworld's expression
    // is unchanged — the bit-identical red line).
    const Vec3 sky =
        playerInNether()
            ? Vec3 {kNetherSkyColorR, kNetherSkyColorG, kNetherSkyColorB}
            : playerInEnd()
                  ? Vec3 {kEndSkyColorR, kEndSkyColorG, kEndSkyColorB}
                  : m_dayCycle.skyColor() * weatherDim +
                        Vec3 {weatherPulse, weatherPulse, weatherPulse};
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
    // V2-4: the weather-adjusted dayFactor (the per-frame channel's
    // single convergence point — see weatherDayFactor): RAIN dims it by
    // kRainSkyDim, the thunder adds the 5-frame brightness pulse.
    // Weather off / clear = m_dayCycle.dayFactor() bit-for-bit (the
    // IEEE identities — the v2.3.0 pipeline).
    m_basicShader.setUniform1f("u_dayFactor", weatherDayFactor());
    // Solid-color alpha default 1.0 (opaque world + arrows; the beacon beam
    // lowers it for its own draws — Phase 83). The textured path ignores it.
    m_basicShader.setUniform1f("u_alpha", 1.0f);
    // One atlas tile in UV space, for the tiled greedy chunk quads
    // (Phase 18). Unread on the legacy (tiled = 0) path.
    m_basicShader.setUniform2f(
        "u_tileSize", 1.0f / static_cast<float>(m_atlas.layout().columns),
        1.0f / static_cast<float>(m_atlas.layout().rows));
    // V3-6: the ACTIVE dimension's loaded columns — the set→vector
    // snapshot the fog's fill clamp (below) and the three column passes
    // (opaque / cutout / transparent) all consume. The copy the column
    // passes used to make at their own block lives here now: still ONE
    // set walk per frame, and the fog's filled-radius scan runs on the
    // contiguous vector (a linear read) instead of a second pointer-
    // chasing tree walk over the same ~13k columns.
    // V3-1 Phase B (D7): the active dimension (the overworld = the
    // pre-Phase-B m_streaming.loadedColumns — unchanged; the nether's
    // parallel family). The set is stable across render() (the pool
    // flushed at the top of update; no streaming job posts during the
    // render pass), so the snapshot matches what the passes draw.
    // V3-10a: the three-way (the End's parallel loadedColumns family —
    // the fog's fill clamp + the column passes read the ACTIVE
    // dimension's set, the nether's D7 point-2 shape).
    const std::set<ColumnPos>& activeLoadedColumns =
        playerInNether() ? m_nether.streaming.loadedColumns
        : playerInEnd() ? m_end.streaming.loadedColumns
                        : m_streaming.loadedColumns;
    const std::vector<ColumnPos> columns(activeLoadedColumns.begin(),
                                         activeLoadedColumns.end());
    // PRE-2 distance fog: the streaming disc edge (raw chunk
    // cross-sections) dissolves into the horizon — a linear ramp toward
    // the same-frame sky color from kFogStartScale * end to end, where
    // end = min(far plane, effective stream radius in blocks,
    // actually-filled disc rim in blocks — V3-6). The stream radius is
    // m_streaming.streamRadius capped to kStreamCapRadius (STRM-1)
    // times kChunkSize — the same clamp the stream disk itself uses.
    // V3-6: the fill term — filledChebyshevRadius (the loaded
    // columns' max Chebyshev distance from the player's column, the
    // streaming pass's column math) times kChunkSize. The fog end is
    // pinned to the FILLED rim, not the target disk: a startup /
    // filling disk (the no-input run's 4x4 demo area: radius 2 = end
    // 32 / start 24) has its raw cross-sections at or beyond the
    // fully-white end, so they dissolve into the sky instead of
    // showing a hard floating edge; a fully filled disk reads
    // >= its static end / kChunkSize (shipped: min(1000, 1024) / 16
    // = 62.5, so R >= 63 — the capped disk reads 64 or the hysteresis
    // 65), where the clamp is a no-op and the range is the pre-V3-6
    // one bit-for-bit (the steady-state red line — test_fog's V36
    // pins). The fill order
    // (nearest-first posts, FIFO integration) keeps the loaded set
    // downward-closed in column distance, so the filled rim never
    // overstates the terrain (the conservative bias).
    // render.fog (code default true, the fire.particles getBool
    // precedent) gates it; a degenerate range (end <= start — a
    // zero/negative view distance, or an empty fill) falls back to off.
    // Four per-frame uniform sets, zero per-draw cost; the streaming
    // code is untouched (the radius is READ from its loaded-column
    // mirror — no post, no job).
    const bool fogConfigOn = m_config.getBool("render.fog", true);
    const float rEffBlocks = static_cast<float>(
                                 std::min(m_streaming.streamRadius, kStreamCapRadius)) *
                             kChunkSize;
    const Vec3 fogPlayerPos = localPlayer()->position();
    const ColumnPos fogPlayerColumn{
        floorDiv(static_cast<int>(std::floor(fogPlayerPos.x)), kChunkSize),
        floorDiv(static_cast<int>(std::floor(fogPlayerPos.z)), kChunkSize)};
    const float filledBlocks =
        static_cast<float>(filledChebyshevRadius(columns, fogPlayerColumn)) *
        kChunkSize;
    // V3-1 Phase B (D7 point 5): the nether's fog is the SHORT-RANGE
    // dark red (the Minecraft nether's fog 10..96, the named constants
    // above — the fog color is the clear sky, already the dark red).
    // V3-6: its end takes the SAME fill clamp (a first-teleport nether
    // — the 3x3 catch-up, radius 1 = end 16 — hides its rim the same
    // way; a filled nether disk reads >= 96 / 16 = 6 => end 96
    // bit-for-bit); the overworld's streaming-disc-derived range is
    // unchanged.
    // V3-10a: the End's fog is the SHORT thick dark violet (the
    // kEndFogStart..kEndFogEnd named constants — the nether's shape: the
    // fog color is the clear sky, the fill clamp the first-teleport rim
    // guard; a filled End disk reads >= 48 / 16 = 3 => end 48
    // bit-for-bit).
    const FogRange fog =
        playerInNether()
            ? FogRange {kNetherFogStart,
                        fillClampedEnd(kNetherFogEnd, filledBlocks)}
            : playerInEnd()
                  ? FogRange {kEndFogStart,
                              fillClampedEnd(kEndFogEnd, filledBlocks)}
                  : fogFilledRange(rEffBlocks, filledBlocks, kFar);
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
    // V3-15: the portal's UV scroll (the frame-locked phase — a pure
    // function of m_frameCount, the ripple's twin discipline; the
    // config gate portal.anim defaults true, OFF = the fragment's
    // phase branch never executes = the pre-V3-15 pipeline
    // bit-identical). Two per-frame uniform sets, zero per-draw cost
    // (the ripple precedent).
    const bool portalAnimOn = m_config.getBool("portal.anim", true);
    const float portalPhase =
        static_cast<float>(m_frameCount % kPortalAnimPeriodFrames) /
        static_cast<float>(kPortalAnimPeriodFrames);
    m_basicShader.setUniform1i("u_portalAnim", portalAnimOn ? 1 : 0);
    m_basicShader.setUniform1f("u_portalPhase", portalPhase);
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
    // V3-1 Phase B (D7): the column passes draw the ACTIVE dimension's
    // merged column meshes over the `columns` snapshot taken at the fog
    // block (V3-6 — the shared set→vector copy). The frustum test is
    // dimension-agnostic (world coordinates — the active dimension's
    // space).
    // V3-10a: the three-way (the End's parallel columnMeshes family).
    std::unordered_map<ColumnPos, ColumnMesh>& columnMeshes =
        playerInNether() ? m_nether.columnMeshes
        : playerInEnd() ? m_end.columnMeshes : m_columnMeshes;
    for (const ColumnPos& col : columns) {
        if (!columnVisible(col)) {
            continue;
        }
        const auto it = columnMeshes.find(col);
        if (it == columnMeshes.end() || !it->second.opaque.has_value()) {
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
    // V3-10c: the carried block's OPAQUE cubes (the endstone — the
    // carriable set's opaque member; the cutout member's twin call sits
    // in the cutout section below). A no-carry session draws nothing
    // (the structural-immunity root: the bit root's zero-cube frame).
    drawCarriedBlocks(view, projection, RenderPass::Opaque);
    // UX-3: the player's world-space body — third person ONLY
    // (m_cameraMode != First); UX-3 ships First = a no-op on every run
    // (baseline byte-identical, the structural-immunity root).
    drawPlayerBody(view, projection);
    // Arrows (Phase 21): the bow's in-flight projectiles, solid color.
    drawArrows(view, projection);
    // V2-11 (XP): the experience orbs — the small solid-green cubes
    // (the opaque pass, after the arrows: the solid-color precedent).
    // Zero orbs = zero draws = the no-input run's structural immunity
    // (the lazy mesh is never built; the HUD bar is the documented
    // pixel exception, the gate-182 re-anchor).
    drawExpOrbs(view, projection);
    // 3D held hand (Phase 42): the selected item's view model, in the
    // opaque pass (the depth test occludes it behind terrain like
    // everything else). No-op unless the pointer is locked and no panel is
    // open (a no-input run never renders it — baseline byte-identical).
    drawHeldHand(view, projection);
    // Campfire cooking items (Phase 71): the item icon floating above each
    // campfire with a non-empty slot — opaque pass, depth-occluded. No-op
    // without a cooking campfire (baseline byte-identical).
    // V3-10a: the inOverworld gate (the End holds no cooking campfire —
    // the D9 closed-set triple, the nether's shape).
    if (inOverworld()) {
        drawCampfireCookItems(view, projection);
    }
    // Opaque break particles (Phase 15) belong to the opaque pass so the
    // transparent geometry blends over them.
    drawParticles(view, projection, RenderPass::Opaque);

    m_renderer.enableBlending();
    // Sky objects (unlit billboarded quads at kSkyDistance): drawn after
    // the opaque pass so the depth test occludes them behind terrain, and
    // before the transparent pass (they never overlap chunk geometry in
    // depth — they sit behind everything in the demo area).
    if (inOverworld()) { // V3-10a: the End is skyless (the nether's
                        // shape — the sun / moon / stars never draw).
        drawSkyObjects(view, projection, h);
    }

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
        const auto it = columnMeshes.find(col);
        if (it == columnMeshes.end() || !it->second.cutout.has_value()) {
            continue;
        }
        m_renderer.draw(*it->second.cutout, m_basicShader, view, projection,
                        columnModel(col));
    }
    drawItemEntities(view, projection, RenderPass::Cutout);
    // V3-10c: the carried block's CUTOUT cubes (the chorus_plant — the
    // carriable set's cutout member; the discard pipeline is the
    // cutout section's, the item drop's routing twin).
    drawCarriedBlocks(view, projection, RenderPass::Cutout);
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
        const auto it = columnMeshes.find(col);
        if (it == columnMeshes.end() ||
            !it->second.transparent.has_value()) {
            continue;
        }
        m_renderer.draw(*it->second.transparent, m_basicShader, view,
                        projection, columnModel(col));
    }
    drawItemEntities(view, projection, RenderPass::Blend);
    drawParticles(view, projection, RenderPass::Blend);
    // V2-6 blob shadows: the flat translucent-black quads under the mobs
    // (and the player in third person), blended + depth-occluded like the
    // other world geometry. Zero draws without a shadow to draw (a
    // no-input run: no mobs + first person — baseline byte-identical).
    drawMobShadows(view, projection);
    // Beacon beams (Phase 83): the translucent light columns from active
    // beacons, blended + depth-occluded like the other world geometry.
    // No-op without an active beacon (baseline byte-identical).
    if (inOverworld()) { // V3-10a: the D9 closed-set triple.
        drawBeaconBeams(view, projection);
    }
    // V3-10d: the dragon arena's blend FX (the crystal->dragon heal
    // beam + the breath clouds — the drawBeaconBeams' blend-pass twins,
    // gated on playerInEnd: a no-End session is one bool check, the
    // structural-immunity root; the D9 closed-set triple's End member).
    if (playerInEnd()) {
        drawDragonBeam(view, projection);
        drawBreathClouds(view, projection);
    }
    // UX-9a clouds: the drifting translucent layer at y = kCloudHeight,
    // blended + depth-occluded like the other world geometry. Gated on
    // render.clouds (code default true); OFF = zero draw (the v0.27.0
    // pipeline bit-identical). The pattern is per-pixel (the one-time
    // quad, zero upload after init — see drawClouds).
    if (inOverworld()) { // V3-10a: the D9 closed-set triple.
        drawClouds(view, projection);
    }
    // V2-4 rain: the 300-drop field in the 16^3 box around the player —
    // the blend pass after the clouds, before the HUD (the depth test
    // occludes the drops behind terrain; the bar quads are double-sided,
    // culling off). RAIN only: clear weather or render.weather OFF = zero
    // drops (the v2.3.0 pipeline bit-identical — the first segment is
    // always >= kClearMin frames, so a no-input run never sees rain in
    // the base-gate window).
    if (inOverworld()) { // V3-10a: the D9 closed-set triple.
        drawRain(view, projection);
    }
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
        const Vec3 moonPos = moon * static_cast<float>(kSkyDistance);
        // V2-6: the moon PHASE — the dark "eclipse" quad over the moon
        // disc. 8 phases from the day cycle's cumulative day index
        // (dayIndex() is a pure function of the frame count — the cycle
        // advances on the fixed 1/60 step); the offset is the pure
        // DayCycle::moonPhaseOffset(phase): the same 28px disc (zero new
        // texture — the sky shader's solid-color path) slides sideways,
        // 0 = concentric (new, the disc fully covered) to 1 = tangent
        // (full, zero overlap — the quad is skipped). The slide is along
        // the view-plane X axis (the billboard's own axis — the phase
        // reads correctly from any camera yaw), sized in disc radii at
        // the moon's view depth.
        //
        // DRAW ORDER (the depth-quantization root): the phase quad draws
        // FIRST, the moon SECOND, both on the same depth plane (the X
        // offset is perpendicular to the view axis — the float32
        // residual is far below the 24-bit depth resolution at
        // kSkyDistance, so both quantize to the SAME depth value). The
        // LESS test is strict: the moon then fails where the phase
        // already wrote = the dark phase shows through the overlap, and
        // the moon's non-overlap side (the crescent) finds no phase
        // depth = it draws bright. (Drawing the phase LATER nudge-
        // closer never works: at the ~740-800 depth of the kSkyDistance
        // plane with near 0.1 / far 1000 the depth buffer resolves
        // ~160 BLOCKS per step, so any sub-block nudge is quantized
        // away — verified in the V2-6 gate.) The phase's halo (its disc
        // beyond the moon's) is sky-colored — invisible against the
        // night sky — and its sky-plane depth write is harmless:
        // nothing drawn later in the blend section sits at or beyond
        // the kSkyDistance plane (clouds y = 108, the rain box, the
        // beacons). Terrain still occludes the phase correctly (depth
        // test on: a ridge closer than the sky plane rejects both the
        // moon's and the phase's fragments there). Baseline immunity:
        // the moon draws only when moonAlpha > 0 (above the horizon =
        // night); the 60-frame no-input run starts at noon (start_time
        // 0.25) where the moon is at the nadir (moon.y < 0) = this
        // whole branch, phase included, never executes.
        const int phase = m_dayCycle.moonPhase();
        const Vec2 po = DayCycle::moonPhaseOffset(phase);
        if (po.y < 1.0f) { // full moon: tangent, zero overlap — no quad
            const Vec3 viewRight {view.at(0, 0), view.at(0, 1),
                                  view.at(0, 2)};
            // The moon's depth in view space (the sky shader's billboard
            // scale uses exactly this: -viewPos.z).
            const float depth =
                -view.transformVec4(Vec4::fromVec3(moonPos, 1.0f)).z;
            // The disc radius in world units at that depth (the quad
            // SIDE is sizePixels * pxToWorld * depth; the disc is half).
            const float discRadius =
                0.5f * kMoonSpriteSize * pxToWorld * depth;
            const Vec3 phasePos =
                moonPos + viewRight * (po.x * po.y * discRadius);
            m_skyShader.bind();
            m_skyShader.setUniformMat4("u_view", view);
            m_skyShader.setUniformMat4("u_proj", projection);
            m_skyShader.setUniform3f("u_offset", phasePos.x, phasePos.y,
                                     phasePos.z);
            m_skyShader.setUniform2f("u_sizePixels", kMoonSpriteSize,
                                     kMoonSpriteSize);
            m_skyShader.setUniform1f("u_pxToWorld", pxToWorld);
            m_skyShader.setUniform4f("u_color", kMoonPhaseColorR,
                                     kMoonPhaseColorG, kMoonPhaseColorB,
                                     moonAlpha);
            m_renderer.drawSky(m_skyPointMesh, m_skyShader, view, projection);
        }
        // The moon quad, drawn AFTER the phase (see the draw-order note
        // above): the bright disc — the crescent where the phase left
        // room, fully dark on the new-moon phase.
        m_skyShader.bind();
        m_skyShader.setUniformMat4("u_view", view);
        m_skyShader.setUniformMat4("u_proj", projection);
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

void Game::applyDimensionAmbient() {
    // V3-10a: the per-dimension ambient floor (the init's one-time set's
    // flip-frame generalization): the End reads kAmbientEnd on BOTH
    // floors (the Minecraft ambient_light 0.25 — the sourceless island
    // is LIGHT-0 everywhere, and the shader's light<=0 branch reads
    // u_caveAmbient, not u_ambient: setting one alone left the island
    // at the config's 0.03 cave floor — the gate's black-screenshot
    // catch). Every other dimension keeps the init's exact values
    // bit-for-bit (the OW / nether red line — the re-read config is
    // the init's same source).
    m_basicShader.bind();
    if (playerInEnd()) {
        m_basicShader.setUniform1f("u_ambient", kAmbientEnd);
        m_basicShader.setUniform1f("u_caveAmbient", kAmbientEnd);
        return;
    }
    m_basicShader.setUniform1f("u_ambient",
                               ambientFloorForLevel(m_nightSkyLevel));
    if (auto caveAmbient = readCaveAmbient(m_config); caveAmbient.ok()) {
        m_basicShader.setUniform1f(
            "u_caveAmbient", static_cast<float>(*caveAmbient));
    }
}

} // namespace mc
