#include "block/block_registry.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <filesystem>

#include "core/logger.hpp"
#include "world/chunk.hpp" // fluidKindForName (the fluid vocabulary)
#include "world/tree_shape.hpp" // treeShapeValid (the JSON shape vocabulary)

namespace mc {

namespace {

// LGT-2: the light opacity of a block WITHOUT an explicit `light_opacity`
// key: an opaque block fully blocks light (15), a non-opaque block lets it
// pass (0) — a bit-for-bit reproduction of the pre-LGT-2 lighting (which
// keyed off `opaque` alone).
int derivedLightOpacity(bool opaque) {
    return opaque ? 15 : 0;
}

// The reserved air sentinel: id 0, "no block". Not data-driven (no JSON,
// no textures) — chunk storage defaults to it and the mesher never emits
// its faces. Registered in every BlockRegistry so at(BlockID::air()) and
// findByName("air") always work.
Block makeAirBlock() {
    Block air;
    air.id = BlockID::air();
    air.name = "air";
    air.properties.solid = false;
    air.properties.opaque = false;
    air.properties.transparent = false;
    air.properties.destroyable = false;
    air.properties.hardness = 0.0f;
    return air;
}

} // namespace

BlockRegistry::BlockRegistry() {
    m_blocks.push_back(makeAirBlock());
    m_names.emplace("air", BlockID::air().value);
}

void BlockRegistry::clear() {
    // Reset to the pristine state (built-in air sentinel only).
    *this = BlockRegistry{};
}

namespace {

// Typed field reader: returns an error naming the field when the JSON value
// has the wrong type (no silent fallbacks for malformed data files).
template <typename F>
Result<void> checkField(const Json& obj, const std::string& key,
                        const char* type, F&& apply) {
    const Json* v = obj.find(key);
    if (v == nullptr) {
        return {}; // absent = keep default
    }
    const bool typeOk = (std::string(type) == "string") ? v->isString()
                   : (std::string(type) == "number") ? v->isNumber()
                                                     : v->isBool();
    if (!typeOk) {
        return MC_FAIL("block property '" + key + "' must be a " + type);
    }
    apply(*v);
    return {};
}

} // namespace

Result<void> BlockRegistry::parseProperties(const Json& def,
                                            BlockProperties& out) {
    const Json* p = def.find("properties");
    if (p == nullptr) {
        // All defaults — the light opacity still derives from the
        // (default) opaque flag (LGT-2).
        out.lightOpacity = derivedLightOpacity(out.opaque);
        return {};
    }
    if (!p->isObject()) {
        return MC_FAIL("'properties' must be an object");
    }

    auto r = checkField(*p, "solid", "boolean",
                        [&](const Json& v) { out.solid = v.asBool(); });
    MC_TRY(r);
    r = checkField(*p, "opaque", "boolean",
                   [&](const Json& v) { out.opaque = v.asBool(); });
    MC_TRY(r);
    r = checkField(*p, "transparent", "boolean",
                   [&](const Json& v) { out.transparent = v.asBool(); });
    MC_TRY(r);
    // PRE-5 alpha cutout: routes the block's geometry to the cutout
    // render pass (depth write + alpha discard, the leaves / cross-plant
    // case). A strict boolean (the transparent precedent); the
    // cutout-implies-transparent pairing is validated below (the
    // opaque/transparent conflict pattern).
    r = checkField(*p, "cutout", "boolean",
                   [&](const Json& v) { out.cutout = v.asBool(); });
    MC_TRY(r);
    r = checkField(*p, "destroyable", "boolean",
                   [&](const Json& v) { out.destroyable = v.asBool(); });
    MC_TRY(r);
    r = checkField(*p, "flamable", "boolean",
                   [&](const Json& v) { out.flamable = v.asBool(); });
    MC_TRY(r);
    // Beacon pyramid base flag (Phase 83): a pure data flag (the core reads
    // the property, never a block id — hard rule 3).
    r = checkField(*p, "beacon_base", "boolean",
                   [&](const Json& v) { out.beaconBase = v.asBool(); });
    MC_TRY(r);
    r = checkField(*p, "fluid", "string",
                   [&](const Json& v) { out.fluidName = v.asString(); });
    MC_TRY(r);
    r = checkField(*p, "hardness", "number",
                   [&](const Json& v) { out.hardness = v.asDouble(); });
    MC_TRY(r);
    r = checkField(*p, "light_emission", "number",
                   [&](const Json& v) { out.lightEmission = v.asInt(); });
    MC_TRY(r);
    // Light opacity (LGT-2, the leaf light attenuation): the light lost
    // when ENTERING this block's cell (0 = passes like air, 15 = fully
    // blocks). ABSENT = derived from the final opaque flag below (the
    // bit-for-bit backward-compat contract); an explicit value overrides
    // the derivation. A FLUID cell never reads the field (the fixed
    // kFluidLightAttenuation — see world/lighting).
    bool lightOpacitySet = false;
    r = checkField(*p, "light_opacity", "number", [&](const Json& v) {
        out.lightOpacity = v.asInt();
        lightOpacitySet = true;
    });
    MC_TRY(r);
    r = checkField(*p, "drops", "string",
                   [&](const Json& v) { out.drops = v.asString(); });
    MC_TRY(r);
    r = checkField(*p, "drops_count", "number",
                   [&](const Json& v) { out.dropsCount = v.asInt(); });
    MC_TRY(r);
    r = checkField(*p, "interact", "string",
                   [&](const Json& v) { out.interact = v.asString(); });
    MC_TRY(r);
    r = checkField(*p, "dye_base", "string",
                   [&](const Json& v) { out.dyeBase = v.asString(); });
    MC_TRY(r);
    r = checkField(*p, "dye_color", "string",
                   [&](const Json& v) { out.dyeColor = v.asString(); });
    MC_TRY(r);
    r = checkField(*p, "grows_to", "string",
                   [&](const Json& v) { out.growsTo = v.asString(); });
    MC_TRY(r);
    r = checkField(*p, "growth_ticks", "number",
                   [&](const Json& v) { out.growthTicks = v.asInt(); });
    MC_TRY(r);
    r = checkField(*p, "grows_on", "string",
                   [&](const Json& v) { out.growsOn = v.asString(); });
    MC_TRY(r);
    r = checkField(*p, "ignites_to", "string",
                   [&](const Json& v) { out.ignitesTo = v.asString(); });
    MC_TRY(r);
    // Igniter target (Phase 76): an EXPLICIT empty ignites_to is a no-op
    // typo (ABSENT is the default — the ammo discipline, the grows_to
    // pattern).
    if (const Json* it = p->find("ignites_to")) {
        if (it->asString().empty()) {
            return MC_FAIL("block property 'ignites_to' must be a non-empty "
                           "block name (omit it for a plain block)");
        }
    }
    // Stair rotation target (Phase 92): an EXPLICIT empty stair_rotates_to
    // is a no-op typo (ABSENT is the default — the ammo discipline, the
    // grows_to / ignites_to pattern). The target block's existence is
    // cross-checked at game init.
    r = checkField(*p, "stair_rotates_to", "string",
                   [&](const Json& v) { out.stairRotatesTo = v.asString(); });
    MC_TRY(r);
    if (const Json* sr = p->find("stair_rotates_to")) {
        if (sr->asString().empty()) {
            return MC_FAIL("block property 'stair_rotates_to' must be a "
                           "non-empty block name (omit it for a plain block)");
        }
    }
    r = checkField(*p, "sound_material", "string",
                   [&](const Json& v) { out.soundMaterial = v.asString(); });
    MC_TRY(r);
    // Sound material (CRAFT-3): an EXPLICIT empty sound_material is a
    // no-op typo (ABSENT is the generic-sound default — the ammo
    // discipline, the ignites_to / stair_rotates_to pattern). The
    // material's three wavs are cross-checked at game init (the grows_to
    // discipline, naming the missing piece).
    if (const Json* sm = p->find("sound_material")) {
        if (sm->asString().empty()) {
            return MC_FAIL("block property 'sound_material' must be a "
                           "non-empty material name (omit it for the "
                           "generic dig/place/break sounds)");
        }
    }
    r = checkField(*p, "beacon_effect", "string",
                   [&](const Json& v) { out.beaconEffect = v.asString(); });
    MC_TRY(r);
    // Beacon effect (Phase 83): an EXPLICIT empty beacon_effect is a no-op
    // typo (ABSENT is the "not a beacon" default — the ammo discipline, the
    // ignites_to pattern). The effect-name existence cross-check happens at
    // game init (the grows_to discipline).
    if (const Json* be = p->find("beacon_effect")) {
        if (be->asString().empty()) {
            return MC_FAIL("block property 'beacon_effect' must be a "
                           "non-empty effect name (omit it for a plain "
                           "block)");
        }
    }
    // Phase 91: the beacon's EFFECT LIST (the "enchanting table" — multiple
    // effects on the same beacon). An optional non-empty array of effect
    // names; an empty array is a no-op and rejected (the ammo discipline).
    // Mutually exclusive with the single `beacon_effect` (both present =
    // rejected, the grants_effect / grants_effects discipline). Each name
    // must be non-empty (the ammo discipline); existence is cross-checked
    // at game init (the grows_to discipline).
    if (const Json* bes = p->find("beacon_effects")) {
        if (!bes->isArray()) {
            return MC_FAIL("block property 'beacon_effects' must be an "
                           "array of effect names");
        }
        if (bes->size() == 0) {
            return MC_FAIL("block property 'beacon_effects' must be a "
                           "non-empty array of effect names (omit it for a "
                           "plain block)");
        }
        for (std::size_t i = 0; i < bes->size(); ++i) {
            const Json& e = bes->at(i);
            if (!e.isString() || e.asString().empty()) {
                return MC_FAIL("beacon_effects[" + std::to_string(i) +
                               "] must be a non-empty effect name");
            }
            out.beaconEffects.push_back(e.asString());
        }
    }
    if (!out.beaconEffect.empty() && !out.beaconEffects.empty()) {
        return MC_FAIL("a block cannot have both 'beacon_effect' (single) "
                       "and 'beacon_effects' (list)");
    }
    // Merge the single `beacon_effect` into the authoritative list (the
    // backward-compat 1-element beacon). The core reads `beaconEffects`.
    if (out.beaconEffects.empty() && !out.beaconEffect.empty()) {
        out.beaconEffects = {out.beaconEffect};
    }
    // Planting placement restriction (Phase 53): an OPTIONAL array of block
    // names. When present, a placement of this block is allowed only when
    // the cell DIRECTLY BELOW the target holds one of the listed blocks (the
    // soil) — a placement-time gate, independent of the growth-time grows_on
    // gate (Phase 50). Absent = unrestricted (the pre-Phase-53 behavior); an
    // empty array is a no-op and rejected (the ammo discipline). Each name
    // must be a non-empty string (the block-existence cross-check happens at
    // game init, the grows_on discipline).
    if (const Json* po = p->find("plant_on")) {
        if (!po->isArray()) {
            return MC_FAIL("block property 'plant_on' must be an array of "
                           "block names");
        }
        if (po->size() == 0) {
            return MC_FAIL("block property 'plant_on' must not be an empty "
                           "array (omit it when the block can be placed "
                           "anywhere)");
        }
        for (std::size_t i = 0; i < po->size(); ++i) {
            const Json& v = po->at(i);
            if (!v.isString() || v.asString().empty()) {
                return MC_FAIL("block property 'plant_on[" +
                               std::to_string(i) + "]' must be a non-empty "
                               "block name string");
            }
            out.plantOn.push_back(v.asString());
        }
    }
    // Crop environment (Phase 50): integer fields with explicit
    // integer-ness + range checks (the restore_hunger discipline).
    if (const Json* gl = p->find("growth_light")) {
        if (!gl->isNumber()) {
            return MC_FAIL("block property 'growth_light' must be a number");
        }
        const double d = gl->asDouble();
        if (d != std::floor(d)) {
            return MC_FAIL("block property 'growth_light' must be an "
                           "integer (0..15)");
        }
        out.growthLight = static_cast<int>(d);
        if (out.growthLight < 0 || out.growthLight > 15) {
            return MC_FAIL("block property 'growth_light' must be in "
                           "[0, 15], got " + std::to_string(out.growthLight));
        }
    }
    if (const Json* ds = p->find("dry_slowdown")) {
        if (!ds->isNumber()) {
            return MC_FAIL("block property 'dry_slowdown' must be a number");
        }
        const double d = ds->asDouble();
        if (d != std::floor(d)) {
            return MC_FAIL("block property 'dry_slowdown' must be an "
                           "integer (1..64)");
        }
        out.drySlowdown = static_cast<int>(d);
        if (out.drySlowdown < 1 || out.drySlowdown > 64) {
            return MC_FAIL("block property 'dry_slowdown' must be in "
                           "[1, 64], got " + std::to_string(out.drySlowdown));
        }
    }
    // Chance secondary drop (Phase 49): an OPTIONAL object
    // {"item": <name>, "count": N, "chance": (0,1]} — an extra drop rolled
    // per break, independent of the primary `drops`. Parsed field-by-field
    // below (the three keys are a strict triple, validated after the loop).
    if (const Json* cd = p->find("chance_drop")) {
        if (!cd->isObject()) {
            return MC_FAIL("block property 'chance_drop' must be an object "
                           "{\"item\": ..., \"count\": ..., \"chance\": ...}");
        }
        for (const auto& [key, value] : cd->members()) {
            if (key != "item" && key != "count" && key != "chance") {
                return MC_FAIL("block property 'chance_drop' has an unknown "
                               "field '" + key + "'");
            }
        }
        r = checkField(*cd, "item", "string",
                       [&](const Json& v) { out.chanceDropItem = v.asString(); });
        MC_TRY(r);
        r = checkField(*cd, "count", "number",
                       [&](const Json& v) { out.chanceDropCount = v.asInt(); });
        MC_TRY(r);
        r = checkField(*cd, "chance", "number",
                       [&](const Json& v) { out.chanceDropChance = v.asDouble(); });
        MC_TRY(r);
    }
    // Chance secondary drop LIST (Phase 52): an OPTIONAL array of the same
    // strict triples as the single `chance_drop` above. Each entry rolls
    // independently (the entry-indexed salt — block/chance_drop). The array
    // is mutually exclusive with the single `chance_drop` (checked in the
    // validation pass, where both are known); an empty array is a no-op and
    // rejected (the ammo discipline).
    if (const Json* cdl = p->find("chance_drops")) {
        if (!cdl->isArray()) {
            return MC_FAIL("block property 'chance_drops' must be an array "
                           "of {\"item\": ..., \"count\": ..., \"chance\": ...} "
                           "objects");
        }
        if (cdl->size() == 0) {
            return MC_FAIL("block property 'chance_drops' must not be an "
                           "empty array (omit it when the block has no "
                           "chance drops)");
        }
        for (std::size_t i = 0; i < cdl->size(); ++i) {
            const Json& entry = cdl->at(i);
            const std::string at =
                "block property 'chance_drops[" + std::to_string(i) + "]'";
            if (!entry.isObject()) {
                return MC_FAIL(at + " must be an object {\"item\": ..., "
                                   "\"count\": ..., \"chance\": ...}");
            }
            for (const auto& [key, value] : entry.members()) {
                if (key != "item" && key != "count" && key != "chance") {
                    return MC_FAIL(at + " has an unknown field '" + key + "'");
                }
            }
            ChanceDrop cdEntry;
            r = checkField(entry, "item", "string",
                           [&](const Json& v) { cdEntry.item = v.asString(); });
            MC_TRY(r);
            r = checkField(entry, "count", "number",
                           [&](const Json& v) { cdEntry.count = v.asInt(); });
            MC_TRY(r);
            r = checkField(entry, "chance", "number",
                           [&](const Json& v) { cdEntry.chance = v.asDouble(); });
            MC_TRY(r);
            out.chanceDrops.push_back(std::move(cdEntry));
        }
    }
    // Tree growth (GEN-2): an OPTIONAL strict SIX-KEY object
    // {"trunk", "leaves", "min_height", "height_range", "leaf_radius",
    //  "shape"} — the tree a sapling grows into (the chance_drops-entry
    // precedent: every key present and typed, unknown keys rejected by
    // name, the `tree.<key>` addressing). The trunk / leaves NAMES are
    // cross-checked at directory load (they may reference a block in a
    // later file); the shape vocabulary + the integer ranges are checked
    // here; `tree` requires state_count >= 2 (checked at registration —
    // the model is parsed after the properties).
    if (const Json* tr = p->find("tree")) {
        if (!tr->isObject()) {
            return MC_FAIL("block property 'tree' must be an object "
                           "{\"trunk\": ..., \"leaves\": ..., "
                           "\"min_height\": ..., \"height_range\": ..., "
                           "\"leaf_radius\": ..., \"shape\": ...}");
        }
        for (const auto& [key, value] : tr->members()) {
            if (key != "trunk" && key != "leaves" &&
                key != "min_height" && key != "height_range" &&
                key != "leaf_radius" && key != "shape") {
                return MC_FAIL("block property 'tree' has an unknown field "
                               "'" + key + "'");
            }
        }
        // Strict six keys: every key present (a missing one is a no-op
        // typo — the chance_drop-triple discipline, named).
        for (const char* key : {"trunk", "leaves", "min_height",
                                "height_range", "leaf_radius", "shape"}) {
            if (tr->find(key) == nullptr) {
                return MC_FAIL(std::string("block property 'tree.") + key +
                               "' is required (a strict six-key object)");
            }
        }
        TreeGrowData tree;
        r = checkField(*tr, "trunk", "string",
                       [&](const Json& v) { tree.trunk = v.asString(); });
        MC_TRY(r);
        r = checkField(*tr, "leaves", "string",
                       [&](const Json& v) { tree.leaves = v.asString(); });
        MC_TRY(r);
        r = checkField(*tr, "shape", "string",
                       [&](const Json& v) { tree.shape = v.asString(); });
        MC_TRY(r);
        // The three integer fields: explicit integer-ness + range (the
        // growth_light discipline), addressed tree.<key>.
        const struct { const char* key; int* out; int lo; int hi; } ints[] = {
            {"min_height", &tree.minHeight, 1, 16},
            {"height_range", &tree.heightRange, 0, 8},
            {"leaf_radius", &tree.leafRadius, 1, 2},
        };
        for (const auto& f : ints) {
            const Json* v = tr->find(f.key);
            if (v == nullptr) {
                return MC_FAIL(std::string("block property 'tree.'") + f.key +
                               "' is required (a strict six-key object)");
            }
            if (!v->isNumber() || v->asDouble() != std::floor(v->asDouble())) {
                return MC_FAIL(std::string("block property 'tree.'") + f.key +
                               "' must be an integer in [" +
                               std::to_string(f.lo) + ", " +
                               std::to_string(f.hi) + "]");
            }
            *f.out = static_cast<int>(v->asDouble());
            if (*f.out < f.lo || *f.out > f.hi) {
                return MC_FAIL(std::string("block property 'tree.'") + f.key +
                               "' must be in [" + std::to_string(f.lo) +
                               ", " + std::to_string(f.hi) + "], got " +
                               std::to_string(*f.out));
            }
        }
        if (tree.trunk.empty()) {
            return MC_FAIL("block property 'tree.trunk' must be a non-empty "
                           "block name");
        }
        if (tree.leaves.empty()) {
            return MC_FAIL("block property 'tree.leaves' must be a non-empty "
                           "block name");
        }
        if (!treeShapeValid(tree.shape)) {
            return MC_FAIL("block property 'tree.shape' is unsupported: '" +
                           tree.shape +
                           "' (only 'oak', 'spruce' and 'birch' exist)");
        }
        out.tree = std::move(tree);
    }
    // Water reaction (Phase 51): an OPTIONAL object
    // {"source": <block name>, "flowing": <block name>} — the blocks a cell
    // of this fluid converts into when 6-adjacent to a water cell (see
    // world/fluid). A strict pair of non-empty names on a fluid block; the
    // names are cross-checked at game init (the ammo pattern).
    if (const Json* wr = p->find("water_reaction")) {
        if (!wr->isObject()) {
            return MC_FAIL("block property 'water_reaction' must be an "
                           "object {\"source\": ..., \"flowing\": ...}");
        }
        for (const auto& [key, value] : wr->members()) {
            if (key != "source" && key != "flowing") {
                return MC_FAIL("block property 'water_reaction' has an "
                               "unknown field '" + key + "'");
            }
        }
        r = checkField(*wr, "source", "string",
                       [&](const Json& v) {
                           out.waterReactionSource = v.asString();
                       });
        MC_TRY(r);
        r = checkField(*wr, "flowing", "string",
                       [&](const Json& v) {
                           out.waterReactionFlowing = v.asString();
                       });
        MC_TRY(r);
        if (out.waterReactionSource.empty() ||
            out.waterReactionFlowing.empty()) {
            return MC_FAIL("block property 'water_reaction' requires both a "
                           "non-empty 'source' and 'flowing' block name "
                           "(a strict pair)");
        }
        if (out.fluidName.empty()) {
            return MC_FAIL("block property 'water_reaction' requires the "
                           "block to be a fluid ('fluid' property) — only a "
                           "fluid cell reacts to water");
        }
    }

    if (out.dropsCount < 1 || out.dropsCount > 64) {
        return MC_FAIL("'drops_count' must be in [1, 64], got " +
                       std::to_string(out.dropsCount));
    }
    if (out.drops.empty() && out.dropsCount != 1) {
        return MC_FAIL("'drops_count' without 'drops' is meaningless "
                       "(got " +
                       std::to_string(out.dropsCount) + ")");
    }
    if (!out.fluidName.empty() && !fluidKindForName(out.fluidName)) {
        return MC_FAIL("block property 'fluid' has an unknown fluid name '" +
                       out.fluidName + "' (vocabulary: \"water\", \"lava\")");
    }
    if (out.hardness < 0.0) {
        return MC_FAIL("'hardness' must be >= 0, got " +
                       std::to_string(out.hardness));
    }
    if (out.lightEmission < 0 || out.lightEmission > 15) {
        return MC_FAIL("'light_emission' must be in [0, 15], got " +
                       std::to_string(out.lightEmission));
    }
    if (out.lightOpacity < 0 || out.lightOpacity > 15) {
        return MC_FAIL("'light_opacity' must be in [0, 15], got " +
                       std::to_string(out.lightOpacity));
    }
    if (!out.interact.empty() && out.interact != kInteractCraftingTable &&
        out.interact != kInteractFurnace &&
        out.interact != kInteractCampfire) {
        return MC_FAIL("block property 'interact' is unsupported: '" +
                       out.interact + "' (only '" +
                       std::string(kInteractCraftingTable) + "', '" +
                       std::string(kInteractFurnace) + "' and '" +
                       std::string(kInteractCampfire) + "' exist)");
    }
    // Dye family (Phase 41): both fields are optional; an EXPLICIT empty
    // string is a typo (ABSENT is the "not dyeable" default — the ammo
    // discipline). dye_color requires dye_base; a dyeable block cannot
    // also open a GUI.
    if (const Json* db = p->find("dye_base")) {
        if (db->asString().empty()) {
            return MC_FAIL("block property 'dye_base' must be a non-empty "
                           "family name (omit it when the block is not "
                           "dyeable)");
        }
    }
    if (const Json* dc = p->find("dye_color")) {
        if (dc->asString().empty()) {
            return MC_FAIL("block property 'dye_color' must be a non-empty "
                           "color name (omit it when the block has no dye "
                           "color)");
        }
    }
    if (!out.dyeColor.empty() && out.dyeBase.empty()) {
        return MC_FAIL("block property 'dye_color' requires 'dye_base' "
                       "(got '" + out.dyeColor + "')");
    }
    if (!out.dyeBase.empty() && !out.interact.empty()) {
        return MC_FAIL("a block cannot be both dyeable ('dye_base: '" +
                       out.dyeBase + "') and interactive ('interact: '" +
                       out.interact + "')");
    }
    if (out.opaque && out.transparent) {
        return MC_FAIL("block cannot be both 'opaque' and 'transparent'");
    }
    // PRE-5: the cutout pass is a SUB-pass of the transparent routing — a
    // cutout block that is not transparent would route its geometry
    // nowhere (a dead field). The same contradiction as opaque+transparent,
    // named and rejected (the cross/glass-pane forced-combination
    // precedent).
    if (out.cutout && !out.transparent) {
        return MC_FAIL("block property 'cutout' requires 'transparent' (the "
                       "cutout pass is a transparent sub-pass)");
    }
    // Crop growth (Phase 48): both fields are optional; an EXPLICIT empty
    // growsTo is a typo (ABSENT is the "not a crop" default — the ammo
    // discipline). The two fields are a strict pair: each one requires
    // the other (the game init cross-checks that growsTo names an existing
    // block; here the pairing + range + no self-reference are enforced).
    if (const Json* gt = p->find("grows_to")) {
        if (gt->asString().empty()) {
            return MC_FAIL("block property 'grows_to' must be a non-empty "
                           "block name (omit it when the block is mature "
                           "or not a crop)");
        }
        const Json* id = def.find("id");
        if (id != nullptr && id->isString() &&
            id->asString() == out.growsTo) {
            return MC_FAIL("block property 'grows_to' must not reference "
                           "the block itself ('" + out.growsTo + "')");
        }
    }
    // GEN-2: growth_ticks is ALSO the sapling's per-stage roll pace (a
    // `tree` block has no grows_to — the stage is its per-instance state):
    // it must accompany a growsTo OR a tree, and stay in range when it does.
    if (!out.growsTo.empty() &&
        (out.growthTicks < 1 || out.growthTicks > 1000000)) {
        return MC_FAIL("'grows_to' requires 'growth_ticks' in [1, 1000000], "
                       "got " +
                       std::to_string(out.growthTicks));
    }
    if (out.tree.has_value() &&
        (out.growthTicks < 1 || out.growthTicks > 1000000)) {
        return MC_FAIL("'tree' requires 'growth_ticks' in [1, 1000000] "
                       "(the per-stage roll pace), got " +
                       std::to_string(out.growthTicks));
    }
    if (out.growsTo.empty() && !out.tree.has_value() &&
        out.growthTicks != 0) {
        return MC_FAIL("'growth_ticks' without 'grows_to' (or 'tree') is "
                       "meaningless (got " +
                       std::to_string(out.growthTicks) + ")");
    }
    // Chance secondary drop (Phase 49): a present 'chance_drop' object is a
    // strict TRIPLE — item (non-empty), count ([1,64]), chance ((0,1]).
    // (The item's existence + max-stack fit are cross-checked at game init,
    // the drops discipline.)
    if (p->find("chance_drop") != nullptr) {
        if (out.chanceDropItem.empty()) {
            return MC_FAIL("block property 'chance_drop.item' must be a "
                           "non-empty item name");
        }
        if (out.chanceDropCount < 1 || out.chanceDropCount > 64) {
            return MC_FAIL("block property 'chance_drop.count' must be in "
                           "[1, 64], got " +
                           std::to_string(out.chanceDropCount));
        }
        if (out.chanceDropChance <= 0.0 || out.chanceDropChance > 1.0) {
            return MC_FAIL("block property 'chance_drop.chance' must be in "
                           "(0, 1], got " +
                           std::to_string(out.chanceDropChance));
        }
    }
    // Chance drop list (Phase 52): MUTUALLY EXCLUSIVE with the single
    // 'chance_drop' (a block has at most one chance-drop system), and each
    // entry is the same strict triple — item (non-empty), count ([1,64]),
    // chance ((0,1]). (The items' existence + max-stack fit are
    // cross-checked at game init, the drops discipline.)
    if (p->find("chance_drops") != nullptr) {
        if (p->find("chance_drop") != nullptr) {
            return MC_FAIL("block 'chance_drops' (the list) is mutually "
                           "exclusive with 'chance_drop' (the single entry): "
                           "write exactly one of the two");
        }
        for (std::size_t i = 0; i < out.chanceDrops.size(); ++i) {
            const ChanceDrop& e = out.chanceDrops[i];
            const std::string at = "block property 'chance_drops[" +
                                   std::to_string(i) + "].";
            if (e.item.empty()) {
                return MC_FAIL(at + "item' must be a non-empty item name");
            }
            if (e.count < 1 || e.count > 64) {
                return MC_FAIL(at + "count' must be in [1, 64], got " +
                               std::to_string(e.count));
            }
            if (e.chance <= 0.0 || e.chance > 1.0) {
                return MC_FAIL(at + "chance' must be in (0, 1], got " +
                               std::to_string(e.chance));
            }
        }
    }
    // Soil requirement (Phase 50): an EXPLICIT empty grows_on is a typo
    // (ABSENT is the "no soil requirement" default — the ammo discipline);
    // the non-empty name is cross-checked at game init (it must reference
    // an existing block, the grows_to discipline).
    if (p->find("grows_on") != nullptr && out.growsOn.empty()) {
        return MC_FAIL("block property 'grows_on' must be a non-empty block "
                       "name (omit it when the crop needs no specific soil)");
    }
    // LGT-2: an ABSENT light_opacity derives from the FINAL opaque flag
    // (opaque -> 15, non-opaque -> 0 — the pre-LGT-2 lighting keyed off
    // `opaque` alone, so field-less content is bit-for-bit unchanged).
    // An explicit value overrides the derivation (checked above for
    // range).
    if (!lightOpacitySet) {
        out.lightOpacity = derivedLightOpacity(out.opaque);
    }
    return {};
}

Result<void> BlockRegistry::parseModel(const Json& def, BlockModel& out) {
    const Json* m = def.find("model");
    if (m == nullptr) {
        return MC_FAIL("block requires a 'model' object");
    }
    if (!m->isObject()) {
        return MC_FAIL("'model' must be an object");
    }

    // Shape is optional: full_cube is the default. Phase 45: "cross" =
    // two crossing vertical quads (cross plants, e.g. flowers).
    if (const Json* s = m->find("shape")) {
        if (!s->isString() || (s->asString() != "full_cube" &&
                               s->asString() != "slab" &&
                               s->asString() != "stairs" &&
                               s->asString() != "cross" &&
                               s->asString() != "wall" &&
                               s->asString() != "fence" &&
                               s->asString() != "glass_pane")) {
            return MC_FAIL(std::string("'model.shape' is unsupported: '") +
                           s->asString() +
                           "' (only 'full_cube', 'slab', 'stairs', 'cross', "
                           "'wall', 'fence' and 'glass_pane' exist)");
        }
        out.shape = (s->asString() == "cross")
                        ? BlockModel::Shape::Cross
                        : (s->asString() == "slab"
                               ? BlockModel::Shape::Slab
                               : (s->asString() == "stairs"
                                      ? BlockModel::Shape::Stairs
                                      : (s->asString() == "wall"
                                             ? BlockModel::Shape::Wall
                                             : (s->asString() == "fence"
                                                    ? BlockModel::Shape::Fence
                                                    : (s->asString() ==
                                                         "glass_pane"
                                                          ? BlockModel::
                                                                Shape::
                                                                    GlassPane
                                                          : BlockModel::
                                                                Shape::
                                                                    FullCube)))));
    }
    // Phase 89: the stair facing (0..3; only meaningful for shape "stairs").
    if (const Json* sf = m->find("stair_facing")) {
        if (!sf->isNumber() || sf->asDouble() != std::floor(sf->asDouble()) ||
            sf->asDouble() < 0.0 || sf->asDouble() > 3.0) {
            return MC_FAIL("model.stair_facing must be an integer in [0, 3], "
                           "got " + sf->asString());
        }
        out.stairFacing = static_cast<int>(sf->asDouble());
    }
    // ENT-2: the explicit per-instance state pair (model.state_count /
    // model.state_default): a STRICT pair (one without the other is a
    // no-op typo — the ammo discipline, the fast_against pattern). The
    // count is [1, 255] (the chunk's state byte holds 0..count-1; 255 is
    // the unset sentinel the count must never reach); the default is
    // [0, count). A STAIR shape never carries these fields: the Stairs
    // shape already has its implicit state (count 4, default =
    // stair_facing), so an explicit state_count on a stair is a
    // contradiction (named and rejected — the shipped stair content stays
    // field-free).
    const bool hasStateCount = m->find("state_count") != nullptr;
    const bool hasStateDefault = m->find("state_default") != nullptr;
    if (hasStateCount != hasStateDefault) {
        return MC_FAIL("model.state_count and model.state_default are a "
                       "strict pair (both present or both absent)");
    }
    if (hasStateCount) {
        const Json* sc = m->find("state_count");
        const Json* sd = m->find("state_default");
        if (!sc->isNumber() || sc->asDouble() != std::floor(sc->asDouble()) ||
            sc->asDouble() < 1.0 || sc->asDouble() > 255.0) {
            return MC_FAIL("model.state_count must be an integer in [1, "
                           "255], got " + sc->asString());
        }
        const int count = static_cast<int>(sc->asDouble());
        if (!sd->isNumber() || sd->asDouble() != std::floor(sd->asDouble()) ||
            sd->asDouble() < 0.0 || sd->asDouble() >= count) {
            return MC_FAIL("model.state_default must be an integer in [0, " +
                           std::to_string(count) +
                           ") (the state_count), got " + sd->asString());
        }
        if (out.shape == BlockModel::Shape::Stairs) {
            return MC_FAIL("a 'stairs' model cannot set state_count: the "
                           "Stairs shape carries its own state (count 4, "
                           "default = stair_facing)");
        }
        out.stateCount = count;
        out.stateDefault = static_cast<int>(sd->asDouble());
    }
    // GEN-9: the moisture top-face variant (model.moisture_top). FULL-CUBE
    // ONLY — the other shapes own their geometry (slab / stairs / wall /
    // fence / glass pane / cross) and a moisture field on them would be
    // dead data (named and rejected — the cutout shape-gate discipline).
    // Strict non-empty string; an explicit empty string is a no-op typo
    // (the ammo discipline). The tile's existence is cross-checked at
    // init (requiredTextures — the sound_material pattern).
    if (const Json* mt = m->find("moisture_top")) {
        if (!mt->isString() || mt->asString().empty()) {
            return MC_FAIL("model.moisture_top must be a non-empty string");
        }
        if (out.shape != BlockModel::Shape::FullCube) {
            const char* sn =
                out.shape == BlockModel::Shape::Slab    ? "slab"
                : out.shape == BlockModel::Shape::Stairs ? "stairs"
                : out.shape == BlockModel::Shape::Wall   ? "wall"
                : out.shape == BlockModel::Shape::Fence  ? "fence"
                : out.shape == BlockModel::Shape::GlassPane
                    ? "glass_pane"
                    : "cross";
            return MC_FAIL(std::string("model.moisture_top is only valid "
                                       "for shape 'full_cube' (the ") +
                           sn + " shape owns its own geometry)");
        }
        out.moistureTop = mt->asString();
    }

    const Json* t = m->find("textures");
    if (t == nullptr || !t->isObject()) {
        return MC_FAIL("block requires a 'model.textures' object");
    }
    if (out.shape == BlockModel::Shape::Cross) {
        // Cross shapes: exactly one texture key ("cross"). The cube face
        // keys would be dead data (no cube faces are emitted), so they are
        // rejected (the ammo discipline).
        const Json* c = t->find("cross");
        if (c == nullptr || !c->isString() || c->asString().empty()) {
            return MC_FAIL("model.textures.cross is required for shape "
                           "'cross' and must be a non-empty string");
        }
        for (const char* face : {"top", "side", "bottom"}) {
            if (t->find(face) != nullptr) {
                return MC_FAIL(std::string("model.textures.") + face +
                               " is not valid for shape 'cross' (use a "
                               "single 'cross' texture)");
            }
        }
        out.textures.cross = c->asString();
        return {};
    }
    if (out.shape == BlockModel::Shape::Fence) {
        // Phase 98: fence shapes use exactly one texture key ("side") — the
        // posts + the connection bars all draw it. The other keys would be
        // dead data (no cube faces are emitted), so they are rejected (the
        // cross discipline).
        const Json* sd = t->find("side");
        if (sd == nullptr || !sd->isString() || sd->asString().empty()) {
            return MC_FAIL("model.textures.side is required for shape "
                           "'fence' and must be a non-empty string");
        }
        for (const char* face : {"top", "bottom", "cross"}) {
            if (t->find(face) != nullptr) {
                return MC_FAIL(std::string("model.textures.") + face +
                               " is not valid for shape 'fence' (use a "
                               "single 'side' texture)");
            }
        }
        out.textures.side = sd->asString();
        return {};
    }
    if (out.shape == BlockModel::Shape::GlassPane) {
        // Phase 99: glass pane shapes use exactly one texture key ("side")
        // — the pane sheet + the rim slivers all draw it. The other keys
        // would be dead data (no cube faces are emitted), so they are
        // rejected (the cross/fence discipline).
        const Json* sd = t->find("side");
        if (sd == nullptr || !sd->isString() || sd->asString().empty()) {
            return MC_FAIL("model.textures.side is required for shape "
                           "'glass_pane' and must be a non-empty string");
        }
        for (const char* face : {"top", "bottom", "cross"}) {
            if (t->find(face) != nullptr) {
                return MC_FAIL(std::string("model.textures.") + face +
                               " is not valid for shape 'glass_pane' (use "
                               "a single 'side' texture)");
            }
        }
        out.textures.side = sd->asString();
        return {};
    }
    // Full cubes: exactly the three face-group keys (an explicit "cross"
    // key is a typo for a cross shape — rejected, not ignored).
    if (t->find("cross") != nullptr) {
        return MC_FAIL("model.textures.cross is only valid for shape "
                       "'cross' (full cubes use top/side/bottom)");
    }
    for (const char* face : {"top", "side", "bottom"}) {
        const Json* v = t->find(face);
        if (v == nullptr || !v->isString() || v->asString().empty()) {
            return MC_FAIL(std::string("model.textures.") + face +
                           " is required and must be a non-empty string");
        }
    }
    out.textures.top = t->find("top")->asString();
    out.textures.side = t->find("side")->asString();
    out.textures.bottom = t->find("bottom")->asString();
    return {};
}

Result<BlockID> BlockRegistry::registerBlock(const Json& def) {
    if (!def.isObject()) {
        return MC_FAIL("block definition must be a JSON object");
    }

    const Json* idJson = def.find("id");
    if (idJson == nullptr || !idJson->isString() || idJson->asString().empty()) {
        return MC_FAIL("block definition requires a non-empty string 'id'");
    }
    const std::string name = idJson->asString();

    if (m_names.count(name) != 0) {
        return MC_FAIL("duplicate block id '" + name + "'");
    }

    // Warn about unknown top-level keys (typos, future fields).
    for (const auto& [key, value] : def.members()) {
        if (key != "id" && key != "properties" && key != "model") {
            MC_LOG_WARN("Block '" + name + "': unknown field '" + key +
                        "' ignored");
        }
    }

    Block block;
    block.name = name;

    auto r = parseProperties(def, block.properties);
    MC_TRY(r);
    r = parseModel(def, block.model);
    MC_TRY(r);

    // Cross plants (Phase 45): they render as two blended quads in the
    // transparent pass and are walk-through, so the property combination is
    // forced (an opaque/solid cross would occlude faces/light it does not
    // visually cover).
    if (block.model.shape == BlockModel::Shape::Cross) {
        if (block.properties.opaque) {
            return MC_FAIL("cross-plant block '" + name +
                           "' must not be 'opaque' (it renders as two "
                           "blended quads)");
        }
        // LGT-2: the two thin quads cannot fully block light either — the
        // same contradiction as opacity (a full-opacity cross would seal
        // light the mesh does not visually cover).
        if (block.properties.lightOpacity == 15) {
            return MC_FAIL("cross-plant block '" + name +
                           "' must not have 'light_opacity' 15 (light "
                           "passes through the two quads)");
        }
        if (block.properties.solid) {
            return MC_FAIL("cross-plant block '" + name +
                           "' must not be 'solid' (cross plants are "
                           "walk-through)");
        }
        if (!block.properties.transparent) {
            return MC_FAIL("cross-plant block '" + name +
                           "' must be 'transparent' (it renders in the "
                           "blending pass)");
        }
    }

    // Glass panes (Phase 99): a blended 1/16-thick sheet — light must pass
    // through (non-opaque) and it renders in the transparent pass. Unlike
    // the cross, a pane IS solid (it blocks movement; jumpable at 1.0).
    if (block.model.shape == BlockModel::Shape::GlassPane) {
        if (block.properties.opaque) {
            return MC_FAIL("glass-pane block '" + name +
                           "' must not be 'opaque' (light passes through "
                           "the pane)");
        }
        // LGT-2: the 1/16-thick sheet cannot fully block light either —
        // the same contradiction as opacity (the pane renders one thin
        // blended sheet).
        if (block.properties.lightOpacity == 15) {
            return MC_FAIL("glass-pane block '" + name +
                           "' must not have 'light_opacity' 15 (light "
                           "passes through the pane)");
        }
        if (!block.properties.transparent) {
            return MC_FAIL("glass-pane block '" + name +
                           "' must be 'transparent' (it renders in the "
                           "blending pass)");
        }
    }

    // PRE-5: the cutout routing is honored by exactly two mesh emitters —
    // the greedy CUBE-FACE pass (full-cube blocks) and the CROSS pass.
    // Every other emitter keeps its fixed bucket regardless of the field
    // (slab / stairs / wall / fence are opaque-bucket, the glass pane and
    // the fluids are blend-bucket by construction), so a `cutout` flag on
    // them would be a dead field — rejected by name (the ammo discipline,
    // the cross/glass-pane forced-combination precedent). Only a
    // full-cube or cross block may carry it (and it still requires
    // `transparent`, checked in parseProperties).
    if (block.properties.cutout) {
        const bool cubeOrCross =
            block.model.shape == BlockModel::Shape::FullCube ||
            block.model.shape == BlockModel::Shape::Cross;
        if (!cubeOrCross || !block.properties.fluidName.empty()) {
            return MC_FAIL("block '" + name + "' cannot be 'cutout' (only "
                           "full-cube and cross shapes route to the cutout "
                           "pass; fluids, slabs, stairs, walls, fences and "
                           "glass panes keep their fixed bucket)");
        }
    }

    // GEN-2: a `tree` block is a SAPLING — its growth stage IS its
    // per-instance state (ENT-2: state_count stages, the growth engine
    // advances the state byte; completion rolls the tree out). Fewer than
    // two states would leave no stage to advance to (a stage 0 sapling
    // that is already "mature" — a contradiction, named).
    if (block.properties.tree.has_value() && block.stateCount() < 2) {
        return MC_FAIL("block '" + name +
                       "' has a 'tree' object and requires "
                       "'model.state_count' >= 2 (the growth stage is the "
                       "per-instance state)");
    }

    block.id = BlockID{static_cast<int>(m_blocks.size())};
    m_names.emplace(name, block.id.value);
    m_blocks.push_back(std::move(block));
    MC_LOG_DEBUG("Registered block '" + name + "' (id=" +
                 std::to_string(block.id.value) + ")");
    return block.id;
}

Result<void> BlockRegistry::loadDirectory(AssetManager& assets,
                                          const std::string& relativeDir) {
    auto dir = assets.resolve(relativeDir);
    MC_TRY(dir);
    if (!std::filesystem::is_directory(*dir)) {
        MC_LOG_WARN("Block directory not found: '" + (*dir).string() +
                    "' (no blocks loaded)");
        return {};
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
            return MC_FAIL("invalid JSON in block file '" + rel + "': " +
                           parsed.error().message);
        }
        auto id = registerBlock(*parsed);
        if (!id.ok()) {
            return MC_FAIL("block file '" + rel + "': " + id.error().message);
        }
    }
    // GEN-2: the tree-reference cross-check (block-to-block, so it can run
    // only once EVERY file of the directory is registered — a sapling may
    // reference a trunk defined in a later file alphabetically). A missing
    // name fails the load loudly (the grows_to discipline).
    for (const Block& b : m_blocks) {
        if (!b.properties.tree.has_value()) {
            continue;
        }
        if (m_names.count(b.properties.tree->trunk) == 0) {
            return MC_FAIL("block '" + b.name + "' tree trunk names unknown "
                           "block '" + b.properties.tree->trunk + "'");
        }
        if (m_names.count(b.properties.tree->leaves) == 0) {
            return MC_FAIL("block '" + b.name + "' tree leaves names unknown "
                           "block '" + b.properties.tree->leaves + "'");
        }
    }
    return {};
}

const Block& BlockRegistry::at(BlockID id) const {
    // Contract: caller holds a valid id from this registry.
    assert(id.valid() && id.value < static_cast<int>(m_blocks.size()));
    return m_blocks[static_cast<std::size_t>(id.value)];
}

const Block* BlockRegistry::find(BlockID id) const {
    if (!id.valid() || id.value >= static_cast<int>(m_blocks.size())) {
        return nullptr;
    }
    return &m_blocks[static_cast<std::size_t>(id.value)];
}

const Block* BlockRegistry::findByName(const std::string& name) const {
    const auto it = m_names.find(name);
    return it == m_names.end() ? nullptr : &m_blocks[static_cast<std::size_t>(it->second)];
}

BlockID BlockRegistry::idForName(const std::string& name) const {
    const auto it = m_names.find(name);
    return it == m_names.end() ? BlockID::invalid() : BlockID{it->second};
}

std::optional<BlockID> BlockRegistry::fluidBlock(
    const std::string& fluidName) const {
    // The reference block of a fluid KIND (properties.fluid names the
    // kind): its textures light the mesher's fluid faces and its
    // light_emission makes cells of that kind glow (see world/lighting).
    // nullopt = the kind has no reference block.
    for (const Block& b : m_blocks) {
        if (b.properties.fluidName == fluidName) {
            return b.id;
        }
    }
    return std::nullopt;
}

std::vector<BlockID> BlockRegistry::allIds() const {
    std::vector<BlockID> ids;
    ids.reserve(m_blocks.size());
    for (const auto& b : m_blocks) {
        ids.push_back(b.id);
    }
    return ids;
}

std::set<std::string> BlockRegistry::requiredTextures() const {
    std::set<std::string> names;
    for (const auto& b : m_blocks) {
        // Empty names mean "no texture" (the built-in air sentinel) and
        // never appear in a valid data block (parseModel rejects them).
        if (!b.model.textures.top.empty()) {
            names.insert(b.model.textures.top);
        }
        if (!b.model.textures.side.empty()) {
            names.insert(b.model.textures.side);
        }
        if (!b.model.textures.bottom.empty()) {
            names.insert(b.model.textures.bottom);
        }
        if (!b.model.textures.cross.empty()) {
            names.insert(b.model.textures.cross);
        }
        // GEN-9: the moisture top variant is a full-cube reference too
        // (the init cross-check must see it — the sound_material pattern).
        if (!b.model.moistureTop.empty()) {
            names.insert(b.model.moistureTop);
        }
    }
    return names;
}

} // namespace mc
