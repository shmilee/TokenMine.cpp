#include "core/game.hpp"

#include <cmath>
#include <numbers>

#include "core/bow.hpp"
#include "core/game_local.hpp"
#include "entity/arrow_mesh.hpp"
#include "entity/item_light.hpp"
#include "entity/item_model_mesh.hpp"
#include "entity/mob_mesh.hpp"
#include "entity/mob_swing.hpp"
#include "world/noise.hpp"

namespace mc {

namespace {

// Mobs (Phase 20) and arrows (Phase 21) — every MOVING entity — simulate
// on the SAME fixed per-frame step: a moving body's position at frame N
// must be a pure function of N (the day cycle and the item animation set
// the precedent — real dt varies with load, and a moving body's final
// position would drift by sub-pixels between runs). At the 60 FPS cap the
// real-time speed is exact; at lower caps the body slows down with the
// frame clock (the day cycle's behavior).
constexpr double kMobStepPerFrame = 1.0 / 60.0;

// Arrows share the moving-entity convention (Phase 21).
constexpr double kProjectileStepPerFrame = 1.0 / 60.0;

// Arrow pickup (Phase 42): a STUCK arrow within this radius of the
// player's bottom-center returns to the inventory as the launcher's ammo
// item (Minecraft: walk up to a stuck arrow to pick it up; flying arrows
// are not pickable).
constexpr float kArrowPickupRadius = 1.0f;

// A dropped item is picked up when its bottom-center is within this
// distance of the player's bottom-center (and past the item's pickup
// delay). Minecraft pulls items in gradually (magnetism); the fixed
// radius is the v1 — see docs/KNOWN_ISSUES.md.
constexpr float kPickupDistance = 1.0f;

// Dropped-item animation (Phase 10): derived from the frame counter, not
// the wall clock, so headless runs stay byte-reproducible. At 60 FPS the
// pickup bobs with a 1.5 s period (amplitude 0.05 blocks) and spins once
// every 4 s, like Minecraft's item pickup.
constexpr int kItemBobPeriodFrames = 90;

constexpr int kItemSpinPeriodFrames = 240;

constexpr float kItemBobAmplitude = 0.05f;

} // namespace

void Game::updateEntities(double dt) {
    // Mob spawning (Phase 20): a deterministic frame cadence from the data
    // config (first_spawn_frame keeps the 60-frame no-input baseline
    // mob-free). The scan is a pure function of world + player, so the
    // same runs spawn the same mobs.
    // The cadence is the data base scaled by the difficulty (Phase 25:
    // check_every_frames / hostileSpawnRateScale — 30 -> 60/30/20 for
    // easy/normal/hard). Normal = the base itself (baseline unchanged).
    if (m_mobs.mobsEnabled &&
        m_frameCount >= m_mobs.spawnConfig.firstSpawnFrame &&
        m_frameCount %
                checkEveryFramesFor(m_mobs.spawnConfig.checkEveryFrames,
                                    m_mobs.difficulty) ==
            0) {
        attemptMobSpawn();
    }
    // The mobs need the player's state for their AI (the player updates
    // before the entities in the frame; a dead player is not chased).
    // Mobs step on the fixed kMobStepPerFrame (see its comment); the other
    // entities keep the real dt (they rest — a dropped item's position is
    // an exact boundary regardless of the dt history).
    const Vec3 playerPos = m_player->position();
    const bool playerAlive = !m_player->isDead();
    // Arrow pre-step positions (Phase 21): captured before the update so
    // the swept arrow-vs-mob hit test below can run over each arrow's
    // per-step displacement.
    std::vector<std::pair<Projectile*, Vec3>> arrowSteps;
    for (auto& entity : m_entities) {
        if (auto* arrow = dynamic_cast<Projectile*>(entity.get())) {
            arrowSteps.push_back({arrow, arrow->position()});
        }
    }
    // Phase 73: INDEX loop (not range-for): the skeleton's shoot
    // callback (invoked inside mob->update) pushes a new arrow into
    // m_entities — a vector reallocation would DANGLE the range-for's
    // reference (latent P27 UB, exposed the first time a skeleton
    // finally got an unobstructed line of sight). The index re-fetches
    // m_entities[i] from the current storage every iteration, so the
    // deferred arrow is reached and takes its spawn-frame step — the
    // original intent. (No pass ERASES from m_entities mid-loop, so the
    // index is stable enough; removals run in the post-loop despawn
    // pass.)
    for (std::size_t i = 0; i < m_entities.size(); ++i) {
        auto& entity = m_entities[i];
        if (auto* mob = dynamic_cast<MobEntity*>(entity.get())) {
            mob->setPlayer(playerPos, playerAlive);
            mob->update(kMobStepPerFrame);
            // SURV-5: the mob's active status effects tick on the SAME
            // fixed mob step (frame-locked, hard rule 8) — the damage
            // floors use the effect's min_health_mob (poison: 0, the mob
            // can die; a killing tick flows through the existing
            // despawn/drop path below). count == 0 fast-exits (every
            // effect-free mob's frame is bit-identical to before).
            mob->updateStatusEffects(kMobStepPerFrame, m_effects);
        } else if (dynamic_cast<Projectile*>(entity.get()) != nullptr) {
            // Moving entity: the fixed step (kProjectileStepPerFrame).
            entity->update(kProjectileStepPerFrame);
        } else {
            entity->update(dt);
        }
    }
    // Arrow hits (Phase 21): a swept test from each arrow's pre-step
    // position to its new one against every live mob's AABB — raycastAabb
    // with the per-step displacement as the direction, where t in [0, 1]
    // is a hit inside this step (t = 0: the arrow starts inside the mob).
    // The first mob hit (entity index order) takes the launcher's data
    // damage + the charge-scaled knockback along the arrow's horizontal
    // velocity (Phase 42, core/bow.hpp arrowKnockback), and the arrow is
    // consumed (the despawn pass removes it this frame). Arrows fly
    // through the player and every other entity
    // (collidesWithEntities = false) and never damage the player
    // (single-player v1, docs/KNOWN_ISSUES.md).
    for (auto& [arrow, prev] : arrowSteps) {
        if (arrow->dead()) {
            continue;  // a mob hit this step already consumed it
        }
        if (arrow->stuck()) {
            continue;  // stuck in a block (Phase 24): no motion, no hits
        }
        const Vec3 step = arrow->position() - prev;
        // Mob-shot arrows (Phase 27) can hit the player: the same swept
        // test as the mobs, run FIRST (the arrow's target). The player's
        // own bow arrows are unflagged and keep flying through the player
        // (no self-damage, single-player v1).
        if (arrow->hurtPlayer()) {
            const float tp = raycastAabb(prev, step, m_player->aabbMin(),
                                         m_player->aabbMax());
            if (tp >= 0.0f && tp <= 1.0f && !m_player->isDead()) {
                m_player->damage(arrow->damage());
                if (arrow->effectId() >= 0) {
                    // SURV-5: the arrow's effect lands on the player
                    // (the skeleton's wither) through the SAME single
                    // choke point as the spider's on_hit_effect (the
                    // name is init-cross-checked; the instant settlement
                    // is a no-op for a lingering effect like the wither).
                    applyPlayerEffect(m_effects.at(arrow->effectId()).name,
                                      arrow->effectDuration(),
                                      arrow->effectTier());
                    syncAbsorption(); // the pool takes the new total
                }
                arrow->kill();
                MC_LOG_DEBUG("mob arrow hit player for " +
                             std::to_string(arrow->damage()));
                continue;  // consumed: no mob test this step
            }
        }
        for (auto& entity : m_entities) {
            auto* mob = dynamic_cast<MobEntity*>(entity.get());
            if (mob == nullptr || mob->dead() || mob == arrow->source()) {
                continue;  // the shooter's own arrow never self-hits
            }
            const float t =
                raycastAabb(prev, step, mob->aabbMin(), mob->aabbMax());
            if (t < 0.0f || t > 1.0f) {
                continue;
            }
            mob->damage(arrow->damage());
            // Phase 42: the knockback scales with the shot's charge
            // (arrowKnockback in core/bow.hpp — a full-power shot
            // reproduces the legacy melee-style constants bit-exactly).
            mob->addKnockback(arrowKnockback(arrow->velocity(),
                                             arrow->power()));
            if (arrow->effectId() >= 0 && !mob->dead()) {
                // SURV-5: the arrow's effect lands on the mob (the poison
                // bow's arrow). applyEffect runs the gates (affects_mobs —
                // init-cross-checked for a bow's arrow — and the undead
                // exemption: a poisoned zombie / skeleton is ignored).
                // A mob KILLED by this arrow's damage takes no effect
                // (it despawns with its drops this frame).
                mob->applyEffect(arrow->effectId(), arrow->effectDuration(),
                                 m_effects, arrow->effectTier());
            }
            m_sound.play("hurt");
            arrow->kill();
            break;
        }
    }
    // Entity-vs-entity separation (Phase 19; the player-vs-item part is
    // the Phase 16 pass, now generalized): the pass runs over a fixed-order
    // body list — the player first (immovable: Minecraft pushes items out
    // of the player), then the entity list by index — so the pair order
    // and the exact-center tie are deterministic (an item dropped exactly
    // at the player's feet goes to the player's -x side, the Phase 16
    // direction). Overlapping pairs are pushed apart along the
    // minimum-penetration axis (50/50 between two pushable bodies, full
    // span against an immovable one); projectiles never participate
    // (collidesWithEntities). Repeated until stable, capped (see
    // separateEntityList).
    std::vector<Entity*> bodies;
    bodies.reserve(m_entities.size() + 1);
    bodies.push_back(m_player.operator->());
    for (auto& entity : m_entities) {
        bodies.push_back(entity.get());
    }
    separateEntityList(bodies);
    // Item merge: index order, `j` absorbed into `i` (j > i). A removed
    // entity is skipped as a future merge target, so each item is absorbed
    // at most once (deterministic result for a given spawn order).
    const std::size_t n = m_entities.size();
    std::vector<bool> gone(n, false);
    for (std::size_t i = 0; i < n; ++i) {
        if (gone[i]) {
            continue;
        }
        auto* a = dynamic_cast<ItemEntity*>(m_entities[i].get());
        if (a == nullptr) {
            continue;
        }
        for (std::size_t j = i + 1; j < n; ++j) {
            if (gone[j]) {
                continue;
            }
            auto* b = dynamic_cast<ItemEntity*>(m_entities[j].get());
            if (b != nullptr && b->canMergeInto(*a)) {
                a->absorb(*b);
                gone[j] = true;
            }
        }
    }
    // Pickup (Phase 11): items within kPickupDistance of the player's
    // bottom-center that are past their pickup delay go into the inventory
    // (deterministic index order; if the inventory has no room the item
    // stays where it is and is retried next frame).
    const float pickupSq = kPickupDistance * kPickupDistance;
    for (std::size_t i = 0; i < n; ++i) {
        if (gone[i]) {
            continue;
        }
        auto* item = dynamic_cast<ItemEntity*>(m_entities[i].get());
        if (item == nullptr || !item->pickupReady()) {
            continue;
        }
        const Vec3 d = item->position() - playerPos;
        if (lengthSq(d) > pickupSq) {
            continue;
        }
        // Validity is guaranteed: drops are validated at init and only
        // spawn from that validated data.
        const ItemID iid = m_items.idForName(item->itemName());
        // SURV-3: the picked-up stack's damage returns with it (the
        // Inventory::add merge rule: same-item AND same-damage only).
        if (m_inventory.add(iid, item->count(), m_items, item->damage()) < 0) {
            continue; // inventory full: the item stays on the ground
        }
        gone[i] = true;
        // CRAFT-3: the generic "pickup" timbre (items have no
        // sound_material — YAGNI, documented), spatialized at the picked
        // item's position.
        m_sound.play("pickup", item->position());
    }
    // Arrow pickup (Phase 42): a STUCK arrow within kArrowPickupRadius of
    // the player's bottom-center returns to the inventory as the
    // launcher's ammo item (Minecraft: walk up to a stuck arrow to pick
    // it up). Flying arrows are not pickable (they are in the air). Same
    // deterministic index order as the item pickup above; if the
    // inventory has no room the arrow stays stuck and is retried next
    // frame.
    {
        const float arrowPickupSq =
            kArrowPickupRadius * kArrowPickupRadius;
        for (std::size_t i = 0; i < n; ++i) {
            if (gone[i]) {
                continue;
            }
            auto* arrow = dynamic_cast<Projectile*>(m_entities[i].get());
            if (arrow == nullptr || !arrow->stuck() ||
                arrow->ammoItem().empty()) {
                continue;
            }
            const Vec3 d = arrow->position() - playerPos;
            if (lengthSq(d) > arrowPickupSq) {
                continue;
            }
            // Validity is guaranteed: the ammo item exists (init
            // cross-check, the launcher's 'ammo' field / the mob's 'ammo').
            const ItemID aid = m_items.idForName(arrow->ammoItem());
            if (m_inventory.add(aid, 1, m_items) < 0) {
                continue; // inventory full: the arrow stays stuck
            }
            gone[i] = true;
            m_sound.play("pickup");
            const Vec3 p = arrow->position();
            MC_LOG_DEBUG("picked up stuck arrow (" +
                         arrow->ammoItem() + ") at (" +
                         std::to_string(p.x) + ", " + std::to_string(p.y) +
                         ", " + std::to_string(p.z) + ")");
        }
    }
    // Despawn: merged-away / picked-up + aged-out / impact / below the
    // kill plane + dead mobs (Phase 20: their data-defined drops spawn
    // here, AFTER the list rebuild — pushing during the scan would lose
    // them) + mobs beyond the despawn distance from the player.
    const float despawnSq =
        m_mobs.spawnConfig.despawnDistance * m_mobs.spawnConfig.despawnDistance;
    struct PendingDrop {
        std::string item;
        int count;
        Vec3 pos;
    };
    std::vector<PendingDrop> mobDrops;
    int mobDeaths = 0;
    for (std::size_t i = 0; i < n; ++i) {
        if (gone[i]) {
            continue;
        }
        MobEntity* mob = dynamic_cast<MobEntity*>(m_entities[i].get());
        if (mob != nullptr && !mob->dead()) {
            const Vec3 d = mob->position() - playerPos;
            if (lengthSq(d) > despawnSq) {
                gone[i] = true;
                continue;
            }
        }
        if (!m_entities[i]->shouldDespawn()) {
            continue;
        }
        gone[i] = true;
        if (mob != nullptr && mob->dead()) {
            // SURV-7: the per-mob kill counter — every DEAD mob, drop-
            // less included (the death is the event, not the drop; a
            // distance despawn does NOT count — the mob was never dead).
            ++m_stats["kill." + mob->def().name];
            // UX-7: the tutorial kill event (the death-REMOVAL point,
            // any cause of death — the same place the drops spawn; the
            // data-driven mob name).
            emitTutorialEvent("kill." + mob->def().name, 1);
        }
        if (mob != nullptr && mob->dead() &&
            !mob->def().properties.drops.empty()) {
            // Every data entry becomes its own item entity (Phase 28: a
            // mob can drop several items, e.g. leather + beef), spawned
            // in list order at the same rest position. Phase 38: a range
            // entry is rolled once with the mob's seeded stream (a
            // deterministic death frame → a reproducible roll); a roll of
            // 0 drops nothing for that entry (the skeleton's 0-2 bones).
            for (const MobDrop& drop : mob->def().properties.drops) {
                const int n = mob->rollDropCount(drop.countMin,
                                                 drop.countMax);
                if (n > 0) {
                    mobDrops.push_back({drop.item, n, mob->position()});
                }
            }
            ++mobDeaths;
        }
    }
    if (std::none_of(gone.begin(), gone.end(), [](bool g) { return g; })) {
        return;
    }
    std::vector<std::unique_ptr<Entity>> kept;
    kept.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        if (!gone[i]) {
            kept.push_back(std::move(m_entities[i]));
        } else if (auto* mob = dynamic_cast<MobEntity*>(m_entities[i].get())) {
            // ENT-1: free the mob's dynamic swing mesh (if it ever swung
            // — the key is absent otherwise) so its GL buffers die with
            // the entity. The pointer key is valid here: the pointee is
            // destroyed only AFTER this rebuild (unique_ptr move).
            m_mobs.swingMeshes.erase(mob);
        }
    }
    m_entities = std::move(kept);
    for (const PendingDrop& drop : mobDrops) {
        // Validity is guaranteed: drops are validated at init. The drop
        // spawns at the mob's bottom-center with zero velocity (the same
        // deterministic rest-position convention as broken blocks).
        const Item& it = m_items.at(m_items.idForName(drop.item));
        m_entities.push_back(std::make_unique<ItemEntity>(
            m_world, m_blocks, it.name, it.maxStack, drop.count, drop.pos));
    }
    if (mobDeaths > 0) {
        m_sound.play("death");
    }
}

void Game::drawItemEntities(const Mat4& view, const Mat4& projection,
                            RenderPass pass) {
    // PRE-6 / LGT-4: dropped items take the light of the cell they sit in
    // (the AABB-center cell, itemDropLight) via the vertex shader's
    // per-draw override (u_useLightOverride / u_lightOverrideSky /
    // u_lightOverrideBlock — the PRE-6 drop names, renamed neutral when
    // the beacon beams joined the pattern) — a stone cube dropped in an
    // unlit cave no longer glows like daylight.
    // render.drop_light (code default true, the render.fog /
    // render.water_ripples getBool precedent — config.json is untouched)
    // gates it: OFF = the three uniforms are NEVER set,
    // u_useLightOverride stays its link-time 0 and the vertex branch never
    // runs, so the pipeline is the v0.29.0 one bit-for-bit (the gate 113
    // S1 off control). The program's uniform state persists per program
    // object across draws (Renderer::draw unbinds after every draw), so
    // each drop draw MUST reset u_useLightOverride = 0 afterwards — a
    // leak would light the next draw (mobs, chunks, arrows) with this
    // drop's cell light.
    const bool dropLightOn = m_config.getBool("render.drop_light", true);
    for (const auto& entity : m_entities) {
        const ItemEntity* item =
            dynamic_cast<const ItemEntity*>(entity.get());
        if (item == nullptr) {
            continue;
        }
        const ItemID iid = m_items.idForName(item->itemName());
        if (!iid.valid()) {
            continue; // cannot happen (item data is validated at init)
        }
        const Item& it = m_items.at(iid);
        // CRAFT-2: a model item (a tool / material with a 'model'
        // section) renders its box mesh — replacing the legacy INVISIBLE
        // non-block drop. The box samples the per-item model texture
        // (the icon PNG: it carries transparent pixels, so the model
        // joins the BLENDING pass only — unblended they would draw as an
        // opaque black square, the icon-hand pattern), double-sided
        // (culling OFF for the batch). No model = the legacy paths
        // bit-level unchanged (the block cube below / the invisible
        // non-block drop).
        if (it.model.has_value()) {
            const std::size_t idx = static_cast<std::size_t>(iid.value);
            // PRE-5: the model item routes by its init-time icon census
            // (m_itemCutout) — a PURE cutout icon (alpha set ⊆ {0, 255})
            // draws in the CUTOUT pass (discard + depth write, visible
            // through the leaf holes); an icon with a translucent core
            // stays in the BLEND pass. Never the opaque pass (the icon
            // carries transparent pixels — unblended it would draw as an
            // opaque black square, the CRAFT-2 pattern).
            const RenderPass want =
                (idx < m_itemCutout.size() && m_itemCutout[idx])
                    ? RenderPass::Cutout
                    : RenderPass::Blend;
            if (pass != want) {
                continue;
            }
            if (idx >= m_itemModelTextures.size() ||
                !m_itemModelTextures[idx].valid()) {
                continue; // cannot happen (the texture loads at init)
            }
            const Mesh& mesh = itemModelMesh(iid);
            if (!mesh.valid()) {
                continue;
            }
            // Bob + spin from the frame counter (see kItemBobPeriodFrames):
            // deterministic in headless runs; at 60 FPS the pickup bobs
            // with a 1.5 s period and spins around once every 4 s (the
            // block-cube-drop convention).
            const Vec3 p = item->position();
            const float t = static_cast<float>(m_frameCount);
            const float bob =
                kItemBobAmplitude *
                std::sin(2.0f * std::numbers::pi * t /
                         static_cast<float>(kItemBobPeriodFrames));
            const float spin = 2.0f * std::numbers::pi * t /
                               static_cast<float>(kItemSpinPeriodFrames);
            const Vec3 center {p.x, p.y + item->height() * 0.5f + bob, p.z};
            // The model geometry is canonical unit space (-0.5..0.5); the
            // block cube bakes kSize into its geometry, the model applies
            // it in the matrix — the same world footprint.
            const Mat4 model = Mat4::multiply(
                Mat4::translate(center),
                Mat4::multiply(Mat4::rotateY(spin),
                               Mat4::scale(ItemEntity::kSize)));
            // The program must be BIND first: Renderer::draw unbinds the
            // program after every draw, and a uniform call on an unbound
            // program is GL_INVALID_OPERATION (the residual error then
            // poisons the lazy mesh-upload's glGetError — same pattern as
            // drawArrows).
            m_basicShader.bind();
            m_basicShader.setUniform1i("u_useTexture", 1);
            // PRE-6: this drop's cell light (the vertex override; the
            // reset below restores the 0 every other draw expects).
            if (dropLightOn) {
                const std::pair<float, float> light =
                    itemDropLight(m_world, *item);
                m_basicShader.setUniform1i("u_useLightOverride", 1);
                m_basicShader.setUniform1f("u_lightOverrideSky", light.first);
                m_basicShader.setUniform1f("u_lightOverrideBlock",
                                           light.second);
            }
            m_renderer.setCulling(false); // double-sided box
            m_itemModelTextures[idx].bind();
            m_renderer.draw(mesh, m_basicShader, view, projection, model);
            // PRE-6: the reset (draw unbound the program — re-bind): the
            // override must not leak into the draws that follow.
            if (dropLightOn) {
                m_basicShader.bind();
                m_basicShader.setUniform1i("u_useLightOverride", 0);
            }
            m_atlas.texture().bind(); // restore for the passes that follow
            m_renderer.setCulling(true);
            continue;
        }
        if (it.type != ItemType::Block) {
            // Tools have no in-world cube (the held-item view arrives with
            // the UI in Phase 15).
            continue;
        }
        const Block& block = m_blocks.at(it.block);
        // PRE-5: the block-cube drop routes by the block's properties —
        // opaque blocks stay in the opaque pass, `cutout` blocks (leaves)
        // join the cutout pass, the remaining transparent blocks (glass)
        // the blend pass.
        const RenderPass want =
            !block.properties.transparent
                ? RenderPass::Opaque
                : (block.properties.cutout ? RenderPass::Cutout
                                           : RenderPass::Blend);
        if (pass != want) {
            continue;
        }
        const Mesh& mesh = itemMesh(it.block);
        if (!mesh.valid()) {
            continue;
        }
        // Bob + spin from the frame counter (see kItemBobPeriodFrames):
        // deterministic in headless runs; at 60 FPS the pickup bobs with a
        // 1.5 s period and spins around once every 4 s.
        const Vec3 p = item->position();
        const float t = static_cast<float>(m_frameCount);
        const float bob =
            kItemBobAmplitude *
            std::sin(2.0f * std::numbers::pi * t /
                     static_cast<float>(kItemBobPeriodFrames));
        const float spin = 2.0f * std::numbers::pi * t /
                           static_cast<float>(kItemSpinPeriodFrames);
        const Vec3 center {p.x, p.y + item->height() * 0.5f + bob, p.z};
        const Mat4 model =
            Mat4::multiply(Mat4::translate(center), Mat4::rotateY(spin));
        // PRE-6: the block-cube path needs the program bound before the
        // per-draw uniform (Renderer::draw binds on its own, but the
        // override must be set first — the model path's pattern).
        if (dropLightOn) {
            const std::pair<float, float> light =
                itemDropLight(m_world, *item);
            m_basicShader.bind();
            m_basicShader.setUniform1i("u_useLightOverride", 1);
            m_basicShader.setUniform1f("u_lightOverrideSky", light.first);
            m_basicShader.setUniform1f("u_lightOverrideBlock", light.second);
        }
        m_renderer.draw(mesh, m_basicShader, view, projection, model);
        // PRE-6: the reset (draw unbound the program — re-bind).
        if (dropLightOn) {
            m_basicShader.bind();
            m_basicShader.setUniform1i("u_useLightOverride", 0);
        }
    }
}

const Mesh& Game::itemMesh(BlockID id) {
    auto it = m_itemMeshes.find(id);
    if (it != m_itemMeshes.end()) {
        return it->second;
    }
    Mesh& mesh = m_itemMeshes[id];
    const ItemMeshGeometry geom =
        makeItemCubeGeometry(m_blocks.at(id), m_atlas.layout());
    auto r = mesh.upload(geom.vertices, geom.indices);
    if (!r.ok()) {
        MC_LOG_ERROR("Item mesh build failed for block '" +
                     m_blocks.at(id).name + "': " + r.error().describe());
    }
    return mesh;
}

const Mesh& Game::itemModelMesh(ItemID id) {
    // CRAFT-2: the cached box-model mesh for a model item. Only model
    // items are looked up (the callers gate on item.model / the texture);
    // a non-model id yields the invalid cached mesh (the callers skip
    // invalid). The geometry is static per item (makeItemModelGeometry
    // is a pure function of the parsed data), so build-once + cache is
    // byte-reproducible across runs. The cache is sized at init, before
    // any frame renders.
    const std::size_t idx = static_cast<std::size_t>(id.value);
    assert(idx < m_itemModelMeshes.size());
    Mesh& mesh = m_itemModelMeshes[idx];
    if (!mesh.valid()) {
        const Item& item = m_items.at(id);
        if (item.model.has_value()) {
            // PRE-5: the geometry carries the cutout identity mark (the
            // init-time icon census) on its vertices (location 8).
            const ItemMeshGeometry geom = makeItemModelGeometry(
                item.model.value(),
                idx < m_itemCutout.size() && m_itemCutout[idx]);
            auto r = mesh.upload(geom.vertices, geom.indices);
            if (!r.ok()) {
                MC_LOG_ERROR("Item model mesh build failed for item '" +
                             item.name + "': " + r.error().describe());
            }
        }
    }
    return mesh;
}

void Game::attemptMobSpawn() {
    // The live-mob census (count + bottom-centers for the clearance rule).
    int mobCount = 0;
    std::vector<Vec3> mobPositions;
    for (const auto& entity : m_entities) {
        if (dynamic_cast<const MobEntity*>(entity.get()) != nullptr) {
            ++mobCount;
            mobPositions.push_back(entity->position());
        }
    }
    if (mobCount >= m_mobs.spawnConfig.maxMobs) {
        MC_LOG_DEBUG("mob spawn f" + std::to_string(m_frameCount) +
                     ": cap reached (" + std::to_string(mobCount) +
                     " live), attempt skipped");
        return;
    }
    // Deterministic weighted type pick among the data-listed mobs (the
    // seed mixes the world seed with the frame, so the choice is a pure
    // function of the run). ONE stream value per attempt, reduced into
    // the total-weight domain and walked over the cumulative list (the
    // Phase 39 weights; all weights 1 — the shipped data — is exactly
    // the legacy pick % list.size()). Note: the pick happens BEFORE the
    // light gate, so an attempt whose type cannot spawn at the current
    // light is simply wasted (e.g. a zombie pick by day) — the cadence,
    // not the type, decides when attempts happen.
    const std::uint64_t pick =
        splitmix64(static_cast<std::uint64_t>(m_generator->seed()) ^
                   (std::uint64_t{0x9E3779B97F4A7C15} *
                    static_cast<std::uint64_t>(m_frameCount)));
    const Mob& def = m_mobs.mobTypes.at(m_mobs.mobTypes.idForName(
        m_mobs.spawnConfig
            .mobs[m_mobs.spawnConfig.pickIndex(pick %
                                          static_cast<std::uint64_t>(
                                              m_mobs.spawnConfig.totalWeight()))]
            .id));

    // Player anchor: the player's column + the floor block under its feet
    // (the highest solid cell at or below the feet in that column).
    const Vec3 p = m_player->position();
    const int px = static_cast<int>(std::floor(p.x));
    const int pz = static_cast<int>(std::floor(p.z));
    int floorY = -1;
    for (int y = static_cast<int>(std::floor(p.y)); y >= 0; --y) {
        if (m_blocks.at(m_world.getBlock(px, y, pz)).properties.solid) {
            floorY = y;
            break;
        }
    }
    if (floorY < 0) {
        return;  // the player is over the void: no spawning
    }
    // Spawn protection (Phase 25): hostiles are gated by the difficulty's
    // radius around the world spawn point; passive mobs pass 0 (exempt).
    const float protection = def.properties.hostile
        ? spawnProtectionRadius(m_mobs.difficulty)
        : 0.0f;
    auto cell = findMobSpawnCell(m_world, m_blocks, m_mobs.spawnConfig.minRadius,
                                 m_mobs.spawnConfig.maxRadius, px, pz, floorY,
                                 def.model.width, def.model.height,
                                 m_dayCycle.dayFactor(),
                                 def.properties.spawnMaxLight,
                                 m_player->aabbMin(), m_player->aabbMax(),
                                 mobPositions, protection, m_mobs.spawnPoint.x,
                                 m_mobs.spawnPoint.z, m_nightSkyLevel);
    if (!cell) {
        MC_LOG_DEBUG("mob spawn f" + std::to_string(m_frameCount) +
                     ": pick '" + def.name + "' found no valid cell "
                     "(light gate / clearance / protection)");
        return;
    }
    // Spawn at the cell center, feet on the floor top; the per-mob seed is
    // the world seed mixed with the (increasing) spawn counter, so the same
    // runs produce the same mobs with the same AI streams.
    const Vec3 spawn {static_cast<float>(cell->x) + 0.5f,
                      static_cast<float>(cell->y) + 1.0f,
                      static_cast<float>(cell->z) + 0.5f};
    MC_LOG_DEBUG("mob spawn f" + std::to_string(m_frameCount) +
                 ": '" + def.name + "' at (" + std::to_string(spawn.x) +
                 ", " + std::to_string(spawn.y) + ", " +
                 std::to_string(spawn.z) + ")");
    ++m_mobs.spawnCounter;
    const std::uint64_t seed =
        static_cast<std::uint64_t>(m_generator->seed()) ^
        (std::uint64_t{0x9E3779B97F4A7C15} * m_mobs.spawnCounter);
    auto mob = std::make_unique<MobEntity>(m_world, m_blocks, def, spawn,
                                           seed);
    // Phase 58: the fire system reaches the mobs (Minecraft's lava ignites
    // every entity). Arm from the SAME fire definition + config duration
    // as the player's ignition; an asset set without the fire effect
    // leaves every mob inert (structural, the P57 pattern).
    if (m_fireEffectId != -1) {
        const Effect& fire = m_effects.at(m_fireEffectId);
        mob->armFire(fire.tickInterval, fire.tickAmount,
                     m_player->params().fireDurationSeconds);
    }
    // The AI's landed attacks damage the player (Phase 16's damage path
    // owns the flash/sound/death — updateSurvival polls the health).
    // Hostile damage is scaled by the difficulty (Phase 25); passive mobs
    // (attack_damage 0 in the data) pass the scale untouched.
    const float damageScale = def.properties.hostile
        ? hostileDamageScale(m_mobs.difficulty)
        : 1.0f;
    // Phase 46: a landed melee hit also applies the mob's on_hit_effect
    // (the spider's poison) — the effect name is init-cross-checked, so
    // idForName cannot fail here (the dead check guards both).
    const std::string hitEffectName = def.properties.hitEffectName;
    const double hitEffectDuration = def.properties.hitEffectDuration;
    const int hitEffectTier = def.properties.hitEffectTier;
    mob->setHitCallback(
        [this, damageScale, hitEffectName, hitEffectDuration,
         hitEffectTier](float amount) {
            if (m_player->isDead()) {
                return;
            }
            m_player->damage(amount * damageScale);
            if (!hitEffectName.empty()) {
                // SURV-4: through the single choke point (the name is
                // init-cross-checked; the instant settlement is a no-op for
                // the pre-SURV-4 hit effects — the spider's poison has
                // none). SURV-6: the data's tier rides along (0 = the
                // pre-SURV-6 apply, bit-identical).
                applyPlayerEffect(hitEffectName, hitEffectDuration,
                                  hitEffectTier);
                syncAbsorption(); // Phase 72 (the pool takes the new total)
            }
        });
    // Ranged mobs (Phase 27, the skeleton): the AI's shot spawns an arrow
    // from the mob's head at the player's eye — the same Projectile the
    // bow fires (it sticks in blocks, ages out, and renders), flagged
    // hurtPlayer so the update pass sweeps it against the player too.
    // Damage = the data attack_damage × the difficulty scale (the melee
    // callback above uses the same scale). The raw pointer stays valid:
    // the mob lives in m_entities until the despawn pass removes it, and
    // the callback only runs while the mob is alive (runAi).
    if (def.properties.shootRange > 0.0f) {
        const float arrowDamage = def.properties.attackDamage * damageScale;
        const float arrowSpeed = def.properties.shootSpeed;
        // Phase 42: the mob's arrows are full-power shots (power 1.0 =
        // the legacy knockback constants) and return the mob's data 'ammo'
        // item when a stuck one is picked up (registry: a ranged mob must
        // define it; init: the item exists).
        const std::string arrowAmmo = def.properties.ammoItem;
        // SURV-5: the mob's arrow effect (the skeleton's wither) — the
        // data pair captured like the hit effect above; the id resolves
        // at SHOT time (init cross-checked the name's existence, so
        // idForName cannot fail; empty = a pre-SURV-5 mob, the arrow
        // carries no effect, bit-identical).
        const std::string arrowEffectName = def.properties.arrowEffectName;
        const double arrowEffectDuration = def.properties.arrowEffectDuration;
        const int arrowEffectTier = def.properties.arrowEffectTier;
        MobEntity* raw = mob.get();
        mob->setShootCallback(
            [this, raw, arrowSpeed, arrowDamage, arrowAmmo, arrowEffectName,
             arrowEffectDuration, arrowEffectTier]() {
            if (m_player->isDead() || raw->dead()) {
                return;
            }
            const Vec3 eye = raw->eyePosition();
            const Vec3 target = m_player->eyePosition();
            Vec3 toPlayer = target - eye;
            const float dist = std::sqrt(dot(toPlayer, toPlayer));
            if (dist < 1e-4f) {
                return;
            }
            // Phase 38 LOS gate: a wall between the two makes the arrow
            // stick and vanish on impact, so don't fire at all. The AI
            // already consumed the cooldown, so the skeleton retries after
            // the full interval (Minecraft's behavior: it keeps nocking
            // but never releases).
            if (!hasLineOfSight(m_world, eye, target)) {
                MC_LOG_DEBUG("skeleton shot blocked (no line of sight) from ("
                             + std::to_string(eye.x) + ", " +
                             std::to_string(eye.y) + ", " +
                             std::to_string(eye.z) + ") dist=" +
                             std::to_string(dist));
                return;
            }
            // Phase 38 leading: aim where the player WILL be when the
            // arrow arrives (computeAimDirection iterates the prediction
            // twice; for a stationary target it reproduces the Phase 27
            // arc-only aim bit-exactly, so standing players are hit exactly
            // as before).
            const Vec3 dir = computeAimDirection(eye, target,
                                                 m_player->velocity(),
                                                 arrowSpeed);
            // SURV-5: the arrow's effect (the wither on the skeleton) —
            // resolved to the registry id at shot time (init
            // cross-checked the name's existence; empty = -1 = no
            // effect, the pre-SURV-5 arrow).
            int effectId = -1;
            double effectDuration = 0.0;
            int effectTier = 0;
            if (!arrowEffectName.empty()) {
                effectId = m_effects.idForName(arrowEffectName);
                effectDuration = arrowEffectDuration;
                effectTier = arrowEffectTier;
            }
            auto arrow = std::make_unique<Projectile>(
                m_world, m_blocks, eye + dir * kArrowSpawnOffset, dir,
                arrowSpeed, arrowDamage, /*hurtPlayer=*/true,
                /*power=*/1.0f, arrowAmmo, effectId, effectDuration,
                effectTier);
            arrow->setSource(raw);  // the hit test skips its shooter
            m_entities.push_back(std::move(arrow));
            m_sound.play("shoot");
            MC_LOG_DEBUG("skeleton shot: from (" + std::to_string(eye.x) +
                         ", " + std::to_string(eye.y) + ", " +
                         std::to_string(eye.z) + ") dist=" +
                         std::to_string(dist) + " dmg=" +
                         std::to_string(arrowDamage));
        });
    }
    m_entities.push_back(std::move(mob));
}

void Game::drawMobs(const Mat4& view, const Mat4& projection) {
    // Mobs are always opaque (the model boxes carry no transparency), so
    // they draw with the opaque item pass. Dead mobs are removed in the
    // same frame's update, so a dead one is never rendered.
    //
    // ENT-1: an actively swinging mob (config on + the type has swing
    // data + the phase != 0) draws its per-entity DYNAMIC mesh (the
    // frame's vertices — the swing parts rotated, the rest verbatim);
    // every other mob takes the STATIC per-type cache (the fast path =
    // structural immunity: phase 0 is the identity pose, so the static
    // mesh is byte-identical to a phase-0 frame).
    for (const auto& entity : m_entities) {
        const MobEntity* mob =
            dynamic_cast<const MobEntity*>(entity.get());
        if (mob == nullptr) {
            continue;
        }
        const Mob& def = m_mobs.mobTypes.at(mob->typeId());
        const Mesh& mesh =
            limbSwingActive(m_mobs.limbSwing, !def.model.swings.empty(),
                            mob->swingPhase())
                ? swingMobMesh(*mob, def)
                : mobMesh(mob->typeId());
        if (!mesh.valid()) {
            continue;
        }
        // Bottom-center origin: translate to the mob, rotate by its facing
        // (camera yaw convention — the -Z face of a box is the front).
        const Mat4 model = Mat4::multiply(
            Mat4::translate(mob->position()), Mat4::rotateY(mob->facingYaw()));
        m_renderer.draw(mesh, m_basicShader, view, projection, model);
    }
}

const Mesh& Game::mobMesh(MobID id) {
    auto it = m_mobs.mobMeshes.find(id);
    if (it != m_mobs.mobMeshes.end()) {
        return it->second;
    }
    Mesh& mesh = m_mobs.mobMeshes[id];
    const ItemMeshGeometry geom =
        makeMobModelGeometry(m_mobs.mobTypes.at(id).model, m_atlas.layout());
    auto r = mesh.upload(geom.vertices, geom.indices);
    if (!r.ok()) {
        MC_LOG_ERROR("Mob mesh build failed for mob '" +
                     m_mobs.mobTypes.at(id).name + "': " + r.error().describe());
    }
    return mesh;
}

const Mesh& Game::swingMobMesh(const MobEntity& mob, const Mob& def) {
    // The per-type swing geometry (built once, lazily) + this frame's
    // vertices (the pure buildSwingFrame into the shared scratch) + the
    // per-entity dynamic mesh (allocated on the first swing frame; its
    // VBO is rewritten every frame afterwards — the topology is static,
    // so the IBO/VAO are never touched again).
    const SwingModel& sm = swingModel(def.id);
    buildSwingFrame(sm, mob.swingPhase(), m_mobs.swingFrameVerts);
    Mesh& mesh = m_mobs.swingMeshes[&mob];
    if (!mesh.valid()) {
        auto r = mesh.uploadDynamic(sm.base, sm.indices);
        if (!r.ok()) {
            MC_LOG_ERROR("Mob swing mesh build failed for mob '" + def.name +
                         "': " + r.error().describe());
        }
    }
    mesh.updateVertices(m_mobs.swingFrameVerts);
    return mesh;
}

const SwingModel& Game::swingModel(MobID id) {
    auto it = m_mobs.swingModels.find(id);
    if (it != m_mobs.swingModels.end()) {
        return it->second;
    }
    SwingModel& sm = m_mobs.swingModels[id];
    sm = buildSwingModel(m_mobs.mobTypes.at(id).model, m_atlas.layout());
    return sm;
}

void Game::drawPlayerBody(const Mat4& view, const Mat4& projection) {
    // UX-3: the player's world-space body draws ONLY in third person
    // (m_cameraMode != First). UX-3 ships the mode constant at First, so
    // this returns immediately on EVERY run — the no-input baseline is
    // byte-identical (the structural-immunity root). The held hand (the
    // first-person view model, drawHeldHand) is untouched by this
    // function.
    if (m_cameraMode == CameraMode::First || !m_playerBody.modelLoaded ||
        !m_player) {
        return;
    }
    // The ENT-1 pattern: an actively swinging player (the model has swing
    // data + the phase != 0) draws the per-frame DYNAMIC mesh (the swing
    // parts rotated about their pivots, the rest verbatim); the rest pose
    // (phase 0 = the identity pose) draws the STATIC cache — byte-
    // identical. mobs.limb_swing does NOT gate the player body: its
    // visibility belongs to the camera mode alone (the task's rule — the
    // config is the mobs' render switch).
    const Mesh& mesh =
        limbSwingActive(true, !m_playerBody.model.swings.empty(),
                        m_player->swingPhase())
            ? playerBodySwingMesh()
            : playerBodyMesh();
    if (!mesh.valid()) {
        return;
    }
    // Bottom-center origin (the player's AABB position — the Entity
    // convention), rotated by the camera yaw (the mob's facingYaw
    // convention: yaw 0 = facing -Z, the box's -Z face is the front).
    const Mat4 model = Mat4::multiply(
        Mat4::translate(m_player->position()), Mat4::rotateY(m_player->yaw));
    // Zero per-draw uniforms (the drawMobs pattern, the function right
    // above in the same pass): the render pass set u_useTexture = 1 and
    // bound the atlas at the top, and the bake is skyLight = 1.0 /
    // blockLight = 0 (makeMobModelGeometry — the mobs' always-bright
    // convention; the PRE-6 / LGT-4 world-light override is NOT applied,
    // the documented deviation shared with the mobs).
    m_renderer.draw(mesh, m_basicShader, view, projection, model);
}

const Mesh& Game::playerBodyMesh() {
    if (m_playerBody.mesh.valid()) {
        return m_playerBody.mesh;
    }
    const ItemMeshGeometry geom =
        makeMobModelGeometry(m_playerBody.model, m_atlas.layout());
    auto r = m_playerBody.mesh.upload(geom.vertices, geom.indices);
    if (!r.ok()) {
        MC_LOG_ERROR("Player body mesh build failed: " +
                     r.error().describe());
    }
    return m_playerBody.mesh;
}

const Mesh& Game::playerBodySwingMesh() {
    // swingMobMesh mirrored for the single instance: the per-frame
    // vertices (buildSwingFrame into this body's scratch) rewrite the
    // dynamic VBO (the topology is static — the IBO/VAO are never
    // touched again).
    const SwingModel& sm = playerBodySwingModel();
    buildSwingFrame(sm, m_player->swingPhase(), m_playerBody.swingFrameVerts);
    if (!m_playerBody.swingMesh.valid()) {
        auto r = m_playerBody.swingMesh.uploadDynamic(sm.base, sm.indices);
        if (!r.ok()) {
            MC_LOG_ERROR("Player body swing mesh build failed: " +
                         r.error().describe());
        }
    }
    m_playerBody.swingMesh.updateVertices(m_playerBody.swingFrameVerts);
    return m_playerBody.swingMesh;
}

const SwingModel& Game::playerBodySwingModel() {
    // Built lazily on the first swing frame (only reachable when the
    // model has swing data — the draw gate above), so parts.empty() is
    // the not-built marker (a swing-less model never comes here).
    if (m_playerBody.swingModel.parts.empty()) {
        m_playerBody.swingModel =
            buildSwingModel(m_playerBody.model, m_atlas.layout());
    }
    return m_playerBody.swingModel;
}

void Game::drawArrows(const Mat4& view, const Mat4& projection) {
    // The arrows render in the basic shader's solid-color path (v1 has no
    // arrow texture): u_useTexture is per-program state shared with every
    // textured draw of the frame, so it is switched here and restored
    // immediately (the next pass, the particles, samples the atlas).
    // PRE-9: each arrow is THREE parts (the brown shaft, the gray tip,
    // the white fletching) — three single-color draws per arrow (the
    // classic MC arrow colors).
    buildArrowMeshes();
    if (!m_arrowShaftMesh.valid() || !m_arrowTipMesh.valid() ||
        !m_arrowFletchMesh.valid()) {
        return;
    }
    // The program is UNBOUND here: Renderer::draw binds/unbinds around
    // every draw, and the last textured draw of the opaque pass left it
    // off. Setting a uniform on an unbound program is
    // GL_INVALID_OPERATION, so bind it for the switch (uniforms persist
    // on the program object; the next pass re-binds per draw).
    m_basicShader.bind();
    m_basicShader.setUniform1i("u_useTexture", 0);
    m_basicShader.setUniform1f("u_alpha", 1.0f); // arrows are opaque
    for (const auto& entity : m_entities) {
        const Projectile* arrow =
            dynamic_cast<const Projectile*>(entity.get());
        if (arrow == nullptr) {
            continue;
        }
        // Collision-box center (the box is kSize tall; the part meshes
        // are centered on the origin) oriented along the travel direction
        // (local -Z). A stuck arrow (Phase 24) keeps the pre-impact
        // direction for the orientation — the collision resolver zeroed
        // the blocked axis of the live velocity. Dead arrows are removed
        // in this frame's update, so a dead one is never rendered.
        const Vec3 c = arrow->position() +
                       Vec3 {0.0f, Projectile::kSize * 0.5f, 0.0f};
        const Vec3 dir =
            arrow->stuck() ? arrow->stuckDir() : arrow->velocity();
        const auto [yaw, pitch] = arrowOrientation(dir);
        const Mat4 model = Mat4::multiply(
            Mat4::multiply(Mat4::translate(c), Mat4::rotateY(yaw)),
            Mat4::rotateX(pitch));
        // Three parts, one color each (re-bind before each: the draw
        // unbinds, and u_color is per-part).
        m_basicShader.bind();
        m_basicShader.setUniform3f("u_color", kArrowShaftColorR,
                                   kArrowShaftColorG, kArrowShaftColorB);
        m_renderer.draw(m_arrowShaftMesh, m_basicShader, view, projection,
                        model);
        m_basicShader.bind();
        m_basicShader.setUniform3f("u_color", kArrowTipColorR,
                                   kArrowTipColorG, kArrowTipColorB);
        m_renderer.draw(m_arrowTipMesh, m_basicShader, view, projection, model);
        m_basicShader.bind();
        m_basicShader.setUniform3f("u_color", kArrowFletchColorR,
                                   kArrowFletchColorG, kArrowFletchColorB);
        m_renderer.draw(m_arrowFletchMesh, m_basicShader, view, projection,
                        model);
    }
    // Re-bind for the restore: the last arrow draw (if any) unbound the
    // program.
    m_basicShader.bind();
    m_basicShader.setUniform1i("u_useTexture", 1);
    m_basicShader.unbind();
}

void Game::buildArrowMeshes() {
    // Idempotent: build the three arrow-part meshes once (the pure
    // makeArrowGeometry — the brown shaft box, the gray tip pyramid, the
    // white fletching box).
    if (m_arrowShaftMesh.valid() && m_arrowTipMesh.valid() &&
        m_arrowFletchMesh.valid()) {
        return;
    }
    const ArrowGeometry geom = makeArrowGeometry();
    const auto rShaft =
        m_arrowShaftMesh.upload(geom.shaft.vertices, geom.shaft.indices);
    const auto rTip =
        m_arrowTipMesh.upload(geom.tip.vertices, geom.tip.indices);
    const auto rFletch = m_arrowFletchMesh.upload(geom.fletching.vertices,
                                                  geom.fletching.indices);
    if (!rShaft.ok()) {
        MC_LOG_ERROR("Arrow shaft mesh build failed: " +
                     rShaft.error().describe());
    }
    if (!rTip.ok()) {
        MC_LOG_ERROR("Arrow tip mesh build failed: " + rTip.error().describe());
    }
    if (!rFletch.ok()) {
        MC_LOG_ERROR("Arrow fletching mesh build failed: " +
                     rFletch.error().describe());
    }
}

const Mesh& Game::handIconMesh() {
    if (m_handIconMesh.valid()) {
        return m_handIconMesh;
    }
    // A unit quad in the local XY plane (z = 0), +Z-facing, CCW from the
    // front. Full-texture UVs (the per-item icon is bound at draw time);
    // the top edge (y = +0.5) is v = 0 so the icon is NOT flipped relative
    // to the 2D UI icon (the UI quad maps its top screen row to v = 0).
    // skyLight = blockLight = 1: the shader's LGT-3 light is
    // max(sky - (8.0/15.0) * (1 - dayFactor), block) = max(< 1, 1.0) =
    // 1.0 — the block-light side dominates at EVERY time of day (the
    // moonlit sky term is never >= 1.0), so the icon stays fully bright
    // at night like the 2D icon. The normal is +Y (NOT the geometric
    // +Z): the vertex shader derives faceShade from the model-rotated
    // normal, and +Y keeps it in the top-face bucket (1.0) through every
    // hand spin state — a geometric +Z normal would land in the 0.6 bucket
    // and dim the icon to 61 %.
    MeshVertex tl;
    tl.position = {-0.5f, 0.5f, 0.0f};
    tl.normal = {0.0f, 1.0f, 0.0f};
    tl.uv = {0.0f, 0.0f};
    tl.skyLight = 1.0f;
    tl.blockLight = 1.0f;
    MeshVertex tr;
    tr.position = {0.5f, 0.5f, 0.0f};
    tr.normal = {0.0f, 1.0f, 0.0f};
    tr.uv = {1.0f, 0.0f};
    tr.skyLight = 1.0f;
    tr.blockLight = 1.0f;
    MeshVertex br;
    br.position = {0.5f, -0.5f, 0.0f};
    br.normal = {0.0f, 1.0f, 0.0f};
    br.uv = {1.0f, 1.0f};
    br.skyLight = 1.0f;
    br.blockLight = 1.0f;
    MeshVertex bl;
    bl.position = {-0.5f, -0.5f, 0.0f};
    bl.normal = {0.0f, 1.0f, 0.0f};
    bl.uv = {0.0f, 1.0f};
    bl.skyLight = 1.0f;
    bl.blockLight = 1.0f;
    std::vector<MeshVertex> verts = {tl, tr, br, bl};
    std::vector<std::uint32_t> idx = {0, 3, 2, 0, 2, 1}; // CCW from +Z
    auto r = m_handIconMesh.upload(verts, idx);
    if (!r.ok()) {
        MC_LOG_ERROR("Hand icon mesh build failed: " + r.error().describe());
    }
    return m_handIconMesh;
}

// Phase 78: the four-slot icon row (Minecraft shows every cooking item
// on the campfire). One 0.35 icon per occupied slot, 0.3 apart in a 1x4
// row across the cell's upper half (total span 1.25 — a 0.5 icon leaves
// no room for four, so the icons shrink to fit; the small neighbour
// overlap is intentional adjacency).
inline constexpr float kCampfireIconScale = 0.35f;
inline constexpr float kCampfireIconSpacing = 0.3f;

} // namespace mc
