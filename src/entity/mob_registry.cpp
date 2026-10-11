#include "entity/mob_registry.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <filesystem>

#include "core/logger.hpp"

namespace mc {

namespace {

// Reads a required positive number from `def` (the enclosing object is
// named for the error message).
Result<void> readPositiveNumber(const Json& def, const char* key,
                                float& out, std::string what) {
    const Json* v = def.find(key);
    if (v == nullptr || !v->isNumber()) {
        return MC_FAIL(what + ": '" + key + "' is required and must be a "
                       "number");
    }
    out = static_cast<float>(v->asDouble());
    if (out <= 0.0f) {
        return MC_FAIL(what + ": '" + key + "' must be > 0, got " +
                       std::to_string(out));
    }
    return {};
}

// Reads a required non-negative number from `def`. The AI treats 0 of the
// detect/attack fields as "never" (a passive mob: it wanders and never
// chases or attacks), so 0 is valid data there, not a typo.
Result<void> readNonNegativeNumber(const Json& def, const char* key,
                                   float& out, std::string what) {
    const Json* v = def.find(key);
    if (v == nullptr || !v->isNumber()) {
        return MC_FAIL(what + ": '" + key + "' is required and must be a "
                       "number");
    }
    out = static_cast<float>(v->asDouble());
    if (out < 0.0f) {
        return MC_FAIL(what + ": '" + key + "' must be >= 0, got " +
                       std::to_string(out));
    }
    return {};
}

// Reads a required [x, y, z] array of numbers.
Result<Vec3> readVec3(const Json& v, std::string what) {
    if (!v.isArray() || v.size() != 3 || !v.at(0).isNumber() ||
        !v.at(1).isNumber() || !v.at(2).isNumber()) {
        return MC_FAIL(what + " must be a [x, y, z] array of numbers");
    }
    return Vec3 {static_cast<float>(v.at(0).asDouble()),
                 static_cast<float>(v.at(1).asDouble()),
                 static_cast<float>(v.at(2).asDouble())};
}

} // namespace

Result<void> MobRegistry::parseProperties(const Json& def,
                                          MobProperties& out) {
    if (!def.isObject()) {
        return MC_FAIL("mob 'properties' must be a JSON object");
    }
    auto r = readPositiveNumber(def, "health", out.health, "mob properties");
    MC_TRY(r);
    r = readPositiveNumber(def, "speed", out.speed, "mob properties");
    MC_TRY(r);
    r = readNonNegativeNumber(def, "detect_radius", out.detectRadius,
                              "mob properties");
    MC_TRY(r);
    r = readNonNegativeNumber(def, "attack_range", out.attackRange,
                              "mob properties");
    MC_TRY(r);
    r = readNonNegativeNumber(def, "attack_damage", out.attackDamage,
                              "mob properties");
    MC_TRY(r);
    r = readPositiveNumber(def, "attack_cooldown", out.attackCooldown,
                           "mob properties");
    MC_TRY(r);
    r = readPositiveNumber(def, "wander_interval", out.wanderInterval,
                           "mob properties");
    MC_TRY(r);
    // Optional 'spawn_max_light' (0..15, integer): the spawning light
    // gate (see MobProperties). Defaults to 0 = darkness only.
    if (const Json* sl = def.find("spawn_max_light")) {
        if (!sl->isNumber()) {
            return MC_FAIL("mob properties: 'spawn_max_light' must be a "
                           "number");
        }
        const double d = sl->asDouble();
        if (d != std::floor(d)) {
            return MC_FAIL("mob properties: 'spawn_max_light' must be an "
                           "integer (0..15)");
        }
        out.spawnMaxLight = static_cast<int>(d);
        if (out.spawnMaxLight < 0 || out.spawnMaxLight > 15) {
            return MC_FAIL("mob properties: 'spawn_max_light' must be in "
                           "[0, 15], got " +
                           std::to_string(out.spawnMaxLight));
        }
    }

    // Optional 'hostile' (bool, default false; Phase 25): the difficulty
    // system flag (see MobProperties).
    if (const Json* host = def.find("hostile")) {
        if (!host->isBool()) {
            return MC_FAIL("mob properties: 'hostile' must be a boolean");
        }
        out.hostile = host->asBool();
    }

    // Optional ranged-attack fields (Phase 27, skeleton): all default to
    // 0 (absent = melee mob). shoot_range/retreat_distance accept >= 0;
    // shoot_speed, when PRESENT, must be in (0, 100] (the item field's
    // range) — an explicit 0 is a typo even for melee mobs. A ranged mob
    // (shoot_range > 0) must also define shoot_speed.
    if (const Json* sr = def.find("shoot_range")) {
        if (!sr->isNumber()) {
            return MC_FAIL("mob properties: 'shoot_range' must be a number");
        }
        out.shootRange = static_cast<float>(sr->asDouble());
        if (out.shootRange < 0.0f) {
            return MC_FAIL("mob properties: 'shoot_range' must be >= 0, "
                           "got " + std::to_string(out.shootRange));
        }
    }
    if (const Json* rd = def.find("retreat_distance")) {
        if (!rd->isNumber()) {
            return MC_FAIL("mob properties: 'retreat_distance' must be a "
                           "number");
        }
        out.retreatDistance = static_cast<float>(rd->asDouble());
        if (out.retreatDistance < 0.0f) {
            return MC_FAIL("mob properties: 'retreat_distance' must be >= 0, "
                           "got " + std::to_string(out.retreatDistance));
        }
    }
    if (const Json* ss = def.find("shoot_speed")) {
        if (!ss->isNumber()) {
            return MC_FAIL("mob properties: 'shoot_speed' must be a number");
        }
        out.shootSpeed = static_cast<float>(ss->asDouble());
        if (out.shootSpeed <= 0.0f || out.shootSpeed > 100.0f) {
            return MC_FAIL("mob properties: 'shoot_speed' must be in "
                           "(0, 100], got " + std::to_string(out.shootSpeed));
        }
    }
    if (out.shootRange > 0.0f && out.shootSpeed <= 0.0f) {
        return MC_FAIL("mob properties: 'shoot_range' > 0 requires "
                       "'shoot_speed' in (0, 100]");
    }
    // Optional 'ammo' (string, Phase 42): the item a stuck arrow shot by
    // this mob returns to the player on pickup. Explicit empty is
    // rejected (ammo discipline, like the item field); the game
    // cross-checks the item's existence at init. A ranged mob (shoot
    // range > 0) must define it — its arrows are pickable by design.
    if (const Json* am = def.find("ammo")) {
        if (!am->isString() || am->asString().empty()) {
            return MC_FAIL("mob properties: 'ammo' must be a non-empty "
                           "string (item name)");
        }
        out.ammoItem = am->asString();
    }
    if (out.shootRange > 0.0f && out.ammoItem.empty()) {
        return MC_FAIL("mob properties: 'shoot_range' > 0 requires 'ammo' "
                       "(the item its stuck arrows return on pickup)");
    }
    // Optional milking pair (string, Phase 43, cow): 'milk_with' is the
    // item the player must hold, 'milk_gives' the item a right-click on
    // the mob returns. The two must be present TOGETHER (a lone field is
    // a typo — the dye_base / dye_color pairing discipline); explicit
    // empty is rejected (the ammo discipline). The game cross-checks
    // both item names' existence at init.
    if (const Json* mw = def.find("milk_with")) {
        if (!mw->isString() || mw->asString().empty()) {
            return MC_FAIL("mob properties: 'milk_with' must be a "
                           "non-empty string (item name)");
        }
        out.milkWith = mw->asString();
    }
    if (const Json* mg = def.find("milk_gives")) {
        if (!mg->isString() || mg->asString().empty()) {
            return MC_FAIL("mob properties: 'milk_gives' must be a "
                           "non-empty string (item name)");
        }
        out.milkGives = mg->asString();
    }
    if (out.milkWith.empty() != out.milkGives.empty()) {
        return MC_FAIL("mob properties: 'milk_with' and 'milk_gives' "
                       "must be given together");
    }

    // Migration guard (Phase 28): the single-drop schema
    // ("drops": "item" + "drops_count") is gone — the count now lives
    // inside each list entry. Reject the old field names explicitly
    // instead of silently ignoring them.
    if (def.find("drops_count") != nullptr) {
        return MC_FAIL("mob properties: 'drops_count' is no longer used "
                       "(Phase 28) — put the count inside the matching "
                       "'drops' entry: {\"item\": ..., \"count\": ...}");
    }
    if (const Json* drops = def.find("drops")) {
        // Phase 28: the drop LIST — one {item, count} entry per dropped
        // item, spawned in list order. Absent or [] = no drops.
        if (!drops->isArray()) {
            return MC_FAIL("mob properties: 'drops' must be an array of "
                           "{item, count} entries (the Phase 28 single-"
                           "string schema is no longer accepted)");
        }
        for (std::size_t i = 0; i < drops->size(); ++i) {
            const Json& entry = drops->at(i);
            if (!entry.isObject()) {
                return MC_FAIL("mob properties: 'drops' entry #" +
                               std::to_string(i) + " must be an object "
                               "{\"item\": ..., \"count\": ...}");
            }
            MobDrop drop;
            const Json* item = entry.find("item");
            if (item == nullptr || !item->isString() ||
                item->asString().empty()) {
                return MC_FAIL("mob properties: 'drops' entry #" +
                               std::to_string(i) + " requires a non-empty "
                               "string 'item'");
            }
            drop.item = item->asString();
            // Phase 38: two count forms — the fixed "count" (1..64, the
            // Phase 28 form) or the random range "count_min"/"count_max"
            // (both required together, 0..64, min <= max). Mixing the
            // forms is a data error, as is a range with a missing bound.
            const Json* count = entry.find("count");
            const Json* cmin = entry.find("count_min");
            const Json* cmax = entry.find("count_max");
            if (count && (cmin || cmax)) {
                return MC_FAIL("mob properties: 'drops' entry #" +
                               std::to_string(i) + " gives both 'count' "
                               "and a 'count_min'/'count_max' range — "
                               "use one form, not both");
            }
            if (cmin || cmax) {
                if (!cmin || !cmax) {
                    return MC_FAIL("mob properties: 'drops' entry #" +
                                   std::to_string(i) + " gives only one "
                                   "of 'count_min'/'count_max' — both "
                                   "are required for a random range");
                }
                for (const Json* v : {cmin, cmax}) {
                    if (!v->isNumber() || v->asDouble() !=
                                            std::floor(v->asDouble()) ||
                        v->asDouble() < 0.0 || v->asDouble() > 64.0) {
                        return MC_FAIL("mob properties: 'drops' entry #" +
                                       std::to_string(i) + " range bound "
                                       "must be an integer in [0, 64]");
                    }
                }
                drop.countMin = static_cast<int>(cmin->asDouble());
                drop.countMax = static_cast<int>(cmax->asDouble());
                if (drop.countMin > drop.countMax) {
                    return MC_FAIL("mob properties: 'drops' entry #" +
                                   std::to_string(i) + " has 'count_min' " +
                                   std::to_string(drop.countMin) +
                                   " > 'count_max' " +
                                   std::to_string(drop.countMax));
                }
            } else if (count) {
                if (!count->isNumber() || count->asDouble() !=
                                                std::floor(count->asDouble())) {
                    return MC_FAIL("mob properties: 'drops' entry #" +
                                   std::to_string(i) + " 'count' must be "
                                   "an integer");
                }
                const int n = static_cast<int>(count->asDouble());
                if (n < 1 || n > 64) {
                    return MC_FAIL("mob properties: 'drops' entry #" +
                                   std::to_string(i) + " 'count' must be "
                                   "in [1, 64], got " + std::to_string(n));
                }
                drop.countMin = drop.countMax = n;
            }
            out.drops.push_back(std::move(drop));
        }
    }
    // Optional 'on_hit_effect' (Phase 46, spider): the status effect this
    // mob's landed MELEE hit applies to the player —
    // {"name": "poison", "duration": 15.0}. Both keys are required
    // (pair discipline); the effect name is cross-checked against the
    // EffectRegistry at game init (the ammo/milk pattern). Absent = no
    // effect (every pre-Phase-46 mob).
    if (const Json* oh = def.find("on_hit_effect")) {
        if (!oh->isObject()) {
            return MC_FAIL("mob properties: 'on_hit_effect' must be an "
                           "object {\"name\": ..., \"duration\": ...}");
        }
        for (const auto& [key, value] : oh->members()) {
            if (key != "name" && key != "duration") {
                return MC_FAIL("mob properties: 'on_hit_effect' has an "
                               "unknown field '" + key + "'");
            }
        }
        const Json* nm = oh->find("name");
        if (nm == nullptr || !nm->isString() || nm->asString().empty()) {
            return MC_FAIL("mob properties: 'on_hit_effect' requires a "
                           "non-empty string 'name' (effect id)");
        }
        const Json* du = oh->find("duration");
        if (du == nullptr || !du->isNumber() || du->asDouble() <= 0.0) {
            return MC_FAIL("mob properties: 'on_hit_effect' requires "
                           "'duration' (seconds) > 0");
        }
        out.hitEffectName = nm->asString();
        out.hitEffectDuration = du->asDouble();
    }
    return {};
}

Result<void> MobRegistry::parseModel(const Json& def, MobModel& out) {
    if (!def.isObject()) {
        return MC_FAIL("mob 'model' must be a JSON object");
    }
    auto r = readPositiveNumber(def, "width", out.width, "mob model");
    MC_TRY(r);
    r = readPositiveNumber(def, "height", out.height, "mob model");
    MC_TRY(r);

    const Json* boxesJson = def.find("boxes");
    if (boxesJson == nullptr || !boxesJson->isArray() ||
        boxesJson->size() == 0) {
        return MC_FAIL("mob model: 'boxes' is required and must be a "
                       "non-empty array");
    }
    out.boxes.clear();
    out.boxes.reserve(boxesJson->size());
    for (std::size_t i = 0; i < boxesJson->size(); ++i) {
        const Json& bj = boxesJson->at(i);
        const std::string what = "mob model box " + std::to_string(i);
        if (!bj.isObject()) {
            return MC_FAIL(what + " must be a JSON object");
        }
        MobBox box;
        auto pos = readVec3(*bj.find("pos"), what + ": 'pos'");
        MC_TRY(pos);
        box.center = *pos;
        auto size = readVec3(*bj.find("size"), what + ": 'size'");
        MC_TRY(size);
        box.size = *size;
        if (box.size.x <= 0.0f || box.size.y <= 0.0f || box.size.z <= 0.0f) {
            return MC_FAIL(what + ": 'size' components must all be > 0");
        }
        const Json* tex = bj.find("texture");
        if (tex == nullptr || !tex->isString() || tex->asString().empty()) {
            return MC_FAIL(what + ": 'texture' is required and must be a "
                           "non-empty string (atlas tile name)");
        }
        box.texture = tex->asString();

        // The box must lie inside the model AABB (bottom-center origin):
        // x/z within [-width/2, width/2], y within [0, height].
        const float halfW = out.width / 2.0f;
        if (std::abs(box.center.x) + box.size.x / 2.0f > halfW + 1e-6f ||
            std::abs(box.center.z) + box.size.z / 2.0f > halfW + 1e-6f ||
            box.center.y - box.size.y / 2.0f < -1e-6f ||
            box.center.y + box.size.y / 2.0f > out.height + 1e-6f) {
            return MC_FAIL(what + " lies outside the " +
                           std::to_string(out.width) + "x" +
                           std::to_string(out.height) + " model AABB");
        }
        out.boxes.push_back(box);
    }
    return {};
}

Result<MobID> MobRegistry::registerMob(const Json& def) {
    if (!def.isObject()) {
        return MC_FAIL("mob definition must be a JSON object");
    }

    const Json* idJson = def.find("id");
    if (idJson == nullptr || !idJson->isString() ||
        idJson->asString().empty()) {
        return MC_FAIL("mob definition requires a non-empty string 'id'");
    }
    const std::string name = idJson->asString();

    if (m_names.count(name) != 0) {
        return MC_FAIL("duplicate mob id '" + name + "'");
    }

    // Warn about unknown top-level keys (typos, future fields).
    for (const auto& [key, value] : def.members()) {
        if (key != "id" && key != "properties" && key != "model") {
            MC_LOG_WARN("mob '" + name + "': unknown field '" + key +
                        "' ignored");
        }
    }

    Mob mob;
    mob.name = name;
    const Json* propsJson = def.find("properties");
    if (propsJson == nullptr) {
        return MC_FAIL("mob '" + name +
                       "' requires a 'properties' object");
    }
    auto r = parseProperties(*propsJson, mob.properties);
    MC_TRY(r);
    const Json* modelJson = def.find("model");
    if (modelJson == nullptr) {
        return MC_FAIL("mob '" + name + "' requires a 'model' object");
    }
    r = parseModel(*modelJson, mob.model);
    MC_TRY(r);

    mob.id = MobID{static_cast<int>(m_mobs.size())};
    m_names.emplace(name, mob.id.value);
    m_mobs.push_back(std::move(mob));
    MC_LOG_DEBUG("Registered mob '" + name + "' (id=" +
                 std::to_string(mob.id.value) + ")");
    return mob.id;
}

Result<void> MobRegistry::loadDirectory(AssetManager& assets,
                                        const std::string& relativeDir) {
    auto dir = assets.resolve(relativeDir);
    MC_TRY(dir);
    if (!std::filesystem::is_directory(*dir)) {
        return MC_FAIL("mob directory not found: '" + (*dir).string() + "'");
    }

    std::vector<std::filesystem::path> files;
    for (const auto& entry : std::filesystem::directory_iterator(*dir)) {
        if (entry.is_regular_file() &&
            entry.path().extension() == ".json") {
            files.push_back(entry.path());
        }
    }
    std::sort(files.begin(), files.end());

    for (const auto& file : files) {
        // AssetManager keys are relative to the root.
        const auto rel =
            std::filesystem::relative(file, assets.root()).generic_string();
        auto text = assets.loadText(rel);
        MC_TRY(text);
        auto parsed = Json::parse(*text);
        if (!parsed.ok()) {
            return MC_FAIL("invalid JSON in mob file '" + rel + "': " +
                           parsed.error().message);
        }
        auto id = registerMob(*parsed);
        if (!id.ok()) {
            return MC_FAIL("mob file '" + rel + "': " + id.error().message);
        }
    }
    return {};
}

void MobRegistry::clear() {
    m_mobs.clear();
    m_names.clear();
}

const Mob& MobRegistry::at(MobID id) const {
    // Contract: caller holds a valid id from this registry.
    assert(id.valid() && id.value < static_cast<int>(m_mobs.size()));
    return m_mobs[static_cast<std::size_t>(id.value)];
}

const Mob* MobRegistry::find(MobID id) const {
    if (!id.valid() || id.value >= static_cast<int>(m_mobs.size())) {
        return nullptr;
    }
    return &m_mobs[static_cast<std::size_t>(id.value)];
}

const Mob* MobRegistry::findByName(const std::string& name) const {
    const auto it = m_names.find(name);
    return it == m_names.end()
               ? nullptr
               : &m_mobs[static_cast<std::size_t>(it->second)];
}

MobID MobRegistry::idForName(const std::string& name) const {
    const auto it = m_names.find(name);
    return it == m_names.end() ? MobID::invalid() : MobID{it->second};
}

std::vector<MobID> MobRegistry::allIds() const {
    std::vector<MobID> ids;
    ids.reserve(m_mobs.size());
    for (const Mob& mob : m_mobs) {
        ids.push_back(mob.id);
    }
    return ids;
}

} // namespace mc
