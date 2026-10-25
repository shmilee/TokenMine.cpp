// SURV-3: the durability (item wear) system — the pure wear step
// (item/durability, the campfire / drown pattern: the unit tests own the
// logic, the Game's event sites + Player::damage are thin adapters) +
// the item-data 'durability' parse (the allowed-set discipline: tools /
// armor / ammo launchers / igniters only) + the ItemStack::damage
// semantics (operator== includes it; stack moves carry it) + the
// format-18 save discipline (damage key written only when > 0 in the
// three stack-carrying places; version-lie / out-of-bounds / no-
// durability rejections; the 17 -> 18 identity migration) + the armor
// wear formula (Minecraft's float rule: max(1, incoming / 4)) + the
// drowning bypass (a bypassArmor event never wears the armor).
#include <gtest/gtest.h>

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "block/block_registry.hpp"
#include "core/json.hpp"
#include "core/save.hpp"
#include "core/status_effect.hpp"
#include "item/armor.hpp"
#include "item/durability.hpp"
#include "item/inventory.hpp"
#include "item/item_registry.hpp"
#include "player/drown.hpp"
#include "player/player.hpp"
#include "world/world.hpp"

using namespace mc;

namespace {

Json parseJson(const std::string& text) {
    auto r = Json::parse(text);
    if (!r.ok()) {
        ADD_FAILURE() << "parse failed: " << r.error().message;
        return Json::object();
    }
    return *r;
}

// The fixture registry: the durability item set (all with MC-exact
// data durabilities) + the non-durability controls (stick / apple /
// arrow).
struct Fixture {
    ItemRegistry items;
    BlockRegistry blocks;
    EffectRegistry effects;
    World world;
    Inventory inventory;
    std::unique_ptr<Player> player;

    Fixture() {
        const struct {
            const char* def;
        } defs[] = {
            {R"({"id":"iron_pickaxe","type":"tool",)"
             R"("tool":{"break_speed":4.0},"max_stack":1,)"
             R"("durability":250})"},
            {R"({"id":"flint_and_steel","type":"material",)"
             R"("ignites_flamable":true,"durability":64})"},
            {R"({"id":"bow","type":"material","max_stack":1,)"
             R"("shoot_speed":15.0,"shoot_damage":5.0,"ammo":"arrow",)"
             R"("durability":384})"},
            {R"({"id":"iron_helmet","type":"material","max_stack":1,)"
             R"("armor_slot":"head","armor_points":2,"durability":165})"},
            {R"({"id":"iron_chestplate","type":"material","max_stack":1,)"
             R"("armor_slot":"chest","armor_points":6,"durability":240})"},
            // The non-durability controls.
            {R"({"id":"arrow","type":"material"})"},
            {R"({"id":"stick","type":"material"})"},
            {R"({"id":"apple","type":"material","restore_hunger":4})"},
        };
        for (const auto& d : defs) {
            auto r = items.registerItem(parseJson(d.def));
            if (!r.ok()) {
                ADD_FAILURE() << d.def << ": " << r.error().message;
            }
        }
        auto r = blocks.registerBlock(parseJson(
            R"({"id":"stone","model":{"textures":{"top":"t","side":"t","bottom":"t"}}})"));
        if (!r.ok()) {
            ADD_FAILURE() << r.error().message;
        }
        player =
            std::make_unique<Player>(world, blocks, Vec3 {8.5f, 2.0f, 8.5f});
    }

    ItemID id(const char* name) const { return items.idForName(name); }

    // Equips the piece (its data durability rides the setter, the
    // Player::damage wear step's registry-free input).
    void equip(const char* name, int slot) const {
        const Item& item = items.at(id(name));
        player->setArmorSlot(slot, ItemStack {item.id, 1}, item.armorPoints,
                             item.durability);
    }
};

// A complete, minimal save (empty inventory / no world edits): the
// format-18 parse requires every section, serializeSave writes them all.
SaveGame makeSave() {
    SaveGame s;
    s.seed = 42;
    s.time = 0.5;
    s.playerPos = {1.5f, 2.0f, 1.5f};
    s.playerAbsorption = 0.0f; // format 12+ requires the field
    s.playerOxygen = kOxygenMax; // format 17+ requires the field
    s.slots.resize(Inventory::kTotalSize);
    return s;
}

// Reads one real asset file (the content-pin tests run from the repo
// root — the other asset tests' convention).
std::string readAsset(const std::string& rel) {
    std::ifstream in(rel);
    if (!in) {
        ADD_FAILURE() << "cannot open " << rel;
        return "";
    }
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

// Replaces the array element at `idx` (the Json API has no non-const
// array subscript — the test's reconstruction convention).
Json withArrayElement(const Json& arr, std::size_t idx, const Json& v) {
    Json out = Json::array();
    for (std::size_t i = 0; i < arr.size(); ++i) {
        out.push_back(i == idx ? v : arr.at(i));
    }
    return out;
}

} // namespace

// -- applyDurability: the pure wear step --------------------------------------

TEST(ApplyDurabilityTest, WearAccumulates) {
    // A non-breaking wear adds to the accumulated damage and reports
    // false (the stack survives).
    Fixture f;
    ItemStack stack {f.id("iron_pickaxe"), 1};
    EXPECT_FALSE(applyDurability(stack, kToolBlockWear, f.items));
    EXPECT_FLOAT_EQ(stack.damage, 1.0f);
    EXPECT_FALSE(applyDurability(stack, kToolBlockWear, f.items));
    EXPECT_FLOAT_EQ(stack.damage, 2.0f);
    EXPECT_FALSE(stack.empty());
}

TEST(ApplyDurabilityTest, BreaksAtTheExactBoundary) {
    // damage = durability - 1 + wear 1 = the break (the boundary is
    // INCLUSIVE: reaching the max empties the stack, the item is gone).
    Fixture f;
    ItemStack stack {f.id("iron_pickaxe"), 1};
    stack.damage = 249.0f;
    EXPECT_TRUE(applyDurability(stack, kToolBlockWear, f.items));
    EXPECT_TRUE(stack.empty());
}

TEST(ApplyDurabilityTest, NoBreakBelowTheBoundary) {
    // One short of the max: the wear lands, the item survives.
    Fixture f;
    ItemStack stack {f.id("iron_pickaxe"), 1};
    stack.damage = 248.0f;
    EXPECT_FALSE(applyDurability(stack, kToolBlockWear, f.items));
    EXPECT_FLOAT_EQ(stack.damage, 249.0f);
    EXPECT_FALSE(stack.empty());
}

TEST(ApplyDurabilityTest, ZeroWearAndNoDurabilityAreNoOps) {
    // wear 0 never touches the stack (and never breaks); an item
    // WITHOUT durability (the stick) is inert no matter the wear —
    // the parse's allowed-set makes this the "never wears" class.
    Fixture f;
    ItemStack stack {f.id("iron_pickaxe"), 1};
    stack.damage = 100.0f;
    EXPECT_FALSE(applyDurability(stack, 0.0f, f.items));
    EXPECT_FLOAT_EQ(stack.damage, 100.0f);
    ItemStack stick {f.id("stick"), 5};
    EXPECT_FALSE(applyDurability(stick, 5.0f, f.items));
    EXPECT_FLOAT_EQ(stick.damage, 0.0f);
    EXPECT_EQ(stick.count, 5);
    // An empty stack is inert too.
    ItemStack empty {};
    EXPECT_FALSE(applyDurability(empty, 1.0f, f.items));
    EXPECT_TRUE(empty.empty());
}

TEST(ApplyDurabilityTest, ArmorBreakClearsTheSlotAndPointsSync) {
    // The armor-side break (the Player::damage path's epilogue): a
    // helmet at 164.5 + a 10-incoming event's 2.5 wear crosses the 165
    // max -> the slot clears and its points drop out of the total
    // (the single setter resyncs — no piece without points, no points
    // without a piece).
    Fixture f;
    f.equip("iron_helmet", 0);
    ASSERT_EQ(f.player->armorPoints(), 2);
    f.player->armorSlot(0).damage = 164.5f;
    ASSERT_TRUE(f.player->damage(10.0f)); // wear = max(1, 10/4) = 2.5
    EXPECT_TRUE(f.player->armorSlot(0).empty());
    EXPECT_EQ(f.player->armorPoints(), 0);
}

// -- the 'durability' parse (the allowed-set discipline) -----------------------

TEST(DurabilityParseTest, RejectsNonPositiveDurability) {
    Fixture f;
    // 0 / negative / non-number are all rejected and named (the strict
    // number > 0 — the restore_hunger "explicit 0 is a typo" twin).
    for (const char* bad : {R"({"id":"x","type":"tool",)"
                            R"("tool":{"break_speed":2.0},"durability":0})",
                            R"({"id":"y","type":"tool",)"
                            R"("tool":{"break_speed":2.0},"durability":-5})",
                            R"({"id":"z","type":"tool",)"
                            R"("tool":{"break_speed":2.0},"durability":"fast"})"}) {
        auto r = f.items.registerItem(parseJson(bad));
        ASSERT_FALSE(r.ok()) << bad;
        EXPECT_NE(r.error().message.find("durability"), std::string::npos)
            << r.error().message;
        EXPECT_NE(r.error().message.find("number > 0"), std::string::npos)
            << r.error().message;
    }
}

TEST(DurabilityParseTest, RejectsDurabilityOnDisallowedItems) {
    // The allowed set = the "has a use action" items: tool / armor /
    // ammo launcher / igniter. Anything else carrying the field is a
    // no-op typo, rejected and named (the ammo discipline) — a FOOD
    // material (the apple) and a PLAIN material (the stick).
    Fixture f;
    for (const char* bad : {
             R"({"id":"apple2","type":"material","restore_hunger":4,)"
             R"("durability":100})",
             R"({"id":"stick2","type":"material","durability":10})"}) {
        auto r = f.items.registerItem(parseJson(bad));
        ASSERT_FALSE(r.ok()) << bad;
        EXPECT_NE(r.error().message.find("durability"), std::string::npos)
            << r.error().message;
        EXPECT_NE(r.error().message.find("use action"), std::string::npos)
            << r.error().message;
    }
    // The named item is in the message (the apple case).
    auto r = f.items.registerItem(parseJson(
        R"({"id":"apple3","type":"material","restore_hunger":4,"durability":1})"));
    ASSERT_FALSE(r.ok());
    EXPECT_NE(r.error().message.find("apple3"), std::string::npos)
        << r.error().message;
}

TEST(DurabilityParseTest, AllowedSetsParse) {
    // The four allowed sets: the tool (mining / melee), the armor
    // piece (reduction wear), the ammo launcher (shot wear), the
    // igniter (ignite wear).
    Fixture f;
    struct {
        const char* def;
        const char* id;
        float want;
    } cases[] = {
        {R"({"id":"p2","type":"tool","tool":{"break_speed":2.0},)"
         R"("durability":59})",
         "p2", 59.0f},
        {R"({"id":"h2","type":"material","max_stack":1,"armor_slot":"head",)"
         R"("armor_points":1,"durability":55})",
         "h2", 55.0f},
        {R"({"id":"b2","type":"material","max_stack":1,"shoot_speed":15.0,)"
         R"("shoot_damage":5.0,"ammo":"arrow","durability":384})",
         "b2", 384.0f},
        {R"({"id":"f2","type":"material","ignites_flamable":true,)"
         R"("durability":64})",
         "f2", 64.0f},
    };
    for (const auto& c : cases) {
        auto r = f.items.registerItem(parseJson(c.def));
        ASSERT_TRUE(r.ok()) << c.def << ": " << r.error().message;
        EXPECT_FLOAT_EQ(f.items.at(*r).durability, c.want) << c.id;
    }
}

TEST(DurabilityParseTest, ContentPinned27) {
    // The real assets: the 27 MC-exact values (wood 59 / stone 131 /
    // iron 250 x 3 tool kinds; the 5-bow family 384; the flint and
    // steel 64; the 12 armor pieces leather 55/80/75/65, iron
    // 165/240/225/195, diamond 363/528/495/429 — the 11:16:15:13 ratio).
    struct {
        const char* file;
        float want;
    } cases[] = {
        {"assets/data/items/wooden_pickaxe.json", 59.0f},
        {"assets/data/items/wooden_axe.json", 59.0f},
        {"assets/data/items/wooden_shovel.json", 59.0f},
        {"assets/data/items/stone_pickaxe.json", 131.0f},
        {"assets/data/items/stone_axe.json", 131.0f},
        {"assets/data/items/stone_shovel.json", 131.0f},
        {"assets/data/items/iron_pickaxe.json", 250.0f},
        {"assets/data/items/iron_axe.json", 250.0f},
        {"assets/data/items/iron_shovel.json", 250.0f},
        {"assets/data/items/bow.json", 384.0f},
        {"assets/data/items/infinite_bow.json", 384.0f},
        {"assets/data/items/power_bow.json", 384.0f},
        {"assets/data/items/speed_bow.json", 384.0f},
        {"assets/data/items/master_bow.json", 384.0f},
        {"assets/data/items/flint_and_steel.json", 64.0f},
        {"assets/data/items/leather_helmet.json", 55.0f},
        {"assets/data/items/leather_chestplate.json", 80.0f},
        {"assets/data/items/leather_leggings.json", 75.0f},
        {"assets/data/items/leather_boots.json", 65.0f},
        {"assets/data/items/iron_helmet.json", 165.0f},
        {"assets/data/items/iron_chestplate.json", 240.0f},
        {"assets/data/items/iron_leggings.json", 225.0f},
        {"assets/data/items/iron_boots.json", 195.0f},
        {"assets/data/items/diamond_helmet.json", 363.0f},
        {"assets/data/items/diamond_chestplate.json", 528.0f},
        {"assets/data/items/diamond_leggings.json", 495.0f},
        {"assets/data/items/diamond_boots.json", 429.0f},
    };
    ASSERT_EQ(sizeof(cases) / sizeof(cases[0]), 27u); // the 27-item set
    for (const auto& c : cases) {
        ItemRegistry items;
        auto parsed = Json::parse(readAsset(c.file));
        ASSERT_TRUE(parsed.ok()) << c.file << ": " << parsed.error().message;
        auto reg = items.registerItem(*parsed);
        ASSERT_TRUE(reg.ok()) << c.file << ": " << reg.error().message;
        EXPECT_FLOAT_EQ(items.at(*reg).durability, c.want) << c.file;
    }
}

// -- the ItemStack::damage semantics -------------------------------------------

TEST(ItemStackDamageTest, EqualityIncludesDamage) {
    // The damage is part of the stack's identity: same item + count,
    // different damage = different stacks (the merge gates + the save
    // round-trip rely on it).
    Fixture f;
    const ItemStack a {f.id("iron_pickaxe"), 1};
    const ItemStack b {f.id("iron_pickaxe"), 1, 1.0f};
    const ItemStack c {f.id("iron_pickaxe"), 1, 1.0f};
    EXPECT_NE(a, b);
    EXPECT_EQ(b, c);
    EXPECT_EQ(a, (ItemStack {f.id("iron_pickaxe"), 1, 0.0f}));
}

TEST(ItemStackDamageTest, StackMovesCarryTheDamage) {
    // Copy semantics: the damage rides every stack move — the armor
    // equip/swap/unequip (the armorRightClickUse path), the pick-and-
    // place, the Q drop + the ground pickup (the merge gate: same-item
    // AND same-damage only).
    Fixture f;
    const ItemID helmet = f.id("iron_helmet");
    f.inventory.setSlot(0, ItemStack {helmet, 1, 30.0f});
    ASSERT_EQ(armorRightClickUse(*f.player, f.inventory, f.items, 0),
              ArmorUseAction::Equip);
    EXPECT_FLOAT_EQ(f.player->armorSlot(0).damage, 30.0f); // equip carried it
    // Unequip (the empty hand): the damage comes back to the slot.
    ASSERT_EQ(armorRightClickUse(*f.player, f.inventory, f.items, 0),
              ArmorUseAction::Unequip);
    EXPECT_FLOAT_EQ(f.inventory.at(0).damage, 30.0f);
    // The ground round trip: the Q drop carries the damage (the
    // entity), and the pickup's Inventory::add merges only same-
    // damage stacks (a different-damage unit opens a NEW slot).
    f.inventory.setSlot(1, ItemStack {f.id("flint_and_steel"), 1, 5.0f});
    f.inventory.setSlot(2, ItemStack {f.id("flint_and_steel"), 1, 6.0f});
    // The second unit of damage 5 lands in slot 1 (the merge); a unit
    // of damage 7 cannot join either (two separate slots result).
    EXPECT_EQ((f.inventory.add(f.id("flint_and_steel"), 1, f.items, 5.0f)),
              1);
    EXPECT_EQ((f.inventory.add(f.id("flint_and_steel"), 1, f.items, 7.0f)),
              3);
    EXPECT_EQ(f.inventory.at(1).count, 2);
    EXPECT_FLOAT_EQ(f.inventory.at(1).damage, 5.0f);
    EXPECT_EQ(f.inventory.at(2).count, 1);
    EXPECT_FLOAT_EQ(f.inventory.at(3).damage, 7.0f);
}

// -- save format 18 -------------------------------------------------------------

TEST(SaveFormat18Test, MidDamageRoundTripsInAllThreePlaces) {
    // Slot + armor + entity carry mid-range damage values; the "damage"
    // key is written ONLY when > 0 (the full-durability armor stays the
    // pre-18 string shape, the slot / entity omit the key at 0).
    Fixture f;
    SaveGame s = makeSave();
    s.slots[3] = ItemStack {f.id("iron_pickaxe"), 1, 123.5f};
    s.playerArmor[0] = "iron_helmet";
    s.playerArmorDamage[0] = 2.5f; // the S3 E2E value (10/4 exact)
    s.playerArmor[1] = "iron_chestplate"; // damage 0 = full
    SavedItemEntity e;
    e.item = "bow";
    e.count = 1;
    e.pos = {4.5f, 3.0f, 4.5f};
    e.age = 1.0;
    e.pickupDelay = 0.0;
    e.damage = 30.0f;
    s.entities.push_back(e);
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok()) << j.error().message;
    EXPECT_EQ(j->find("format")->asInt(), 18);
    // The shapes: the damaged slot + the entity carry the key; the
    // full-durability armor slot is a STRING; the damaged one an
    // OBJECT {item, damage}.
    const Json& slot = j->find("inventory")->find("slots")->at(3);
    EXPECT_FLOAT_EQ(slot.find("damage")->asDouble(), 123.5f);
    const Json& ent = j->find("entities")->at(0);
    EXPECT_FLOAT_EQ(ent.find("damage")->asDouble(), 30.0f);
    const Json& armor = *j->find("player")->find("armor");
    EXPECT_TRUE(armor.find("chest")->isString());
    const Json& head = *armor.find("head");
    EXPECT_TRUE(head.isObject());
    EXPECT_EQ(head.find("item")->asString(), "iron_helmet");
    EXPECT_FLOAT_EQ(head.find("damage")->asDouble(), 2.5f);
    // The round trip: every value back.
    auto out = parseSave(*j, f.items, f.blocks, f.effects, 42);
    ASSERT_TRUE(out.ok()) << out.error().message;
    EXPECT_EQ(out->slots[3], (ItemStack {f.id("iron_pickaxe"), 1, 123.5f}));
    EXPECT_EQ(out->playerArmor[0], "iron_helmet");
    EXPECT_FLOAT_EQ(out->playerArmorDamage[0], 2.5f);
    EXPECT_EQ(out->playerArmor[1], "iron_chestplate");
    EXPECT_FLOAT_EQ(out->playerArmorDamage[1], 0.0f);
    ASSERT_EQ(out->entities.size(), 1u);
    EXPECT_FLOAT_EQ(out->entities[0].damage, 30.0f);
    // The no-damage slot omits the key (the omit-when-default pin).
    EXPECT_EQ(j->find("inventory")->find("slots")->at(0)
                  .find("damage"),
              nullptr);
}

TEST(SaveFormat18Test, Format17WithDamageKeyIsAVersionLie) {
    // The version-lie discipline: a format-17 file carrying the damage
    // key (slot / entity / armor object shape) is rejected, named —
    // the field whitelist + the armor shape check do the work.
    Fixture f;
    SaveGame s = makeSave();
    s.slots[3] = ItemStack {f.id("iron_pickaxe"), 1, 123.5f};
    s.playerArmor[0] = "iron_helmet";
    s.playerArmorDamage[0] = 2.5f;
    SavedItemEntity e;
    e.item = "bow";
    e.count = 1;
    e.pos = {4.5f, 3.0f, 4.5f};
    e.age = 1.0;
    e.damage = 30.0f;
    s.entities.push_back(e);
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok()) << j.error().message;
    Json root = *j;
    root["format"] = Json(17);
    // The slot's damage key is no longer whitelisted in format 17.
    {
        auto bad = parseSave(root, f.items, f.blocks, f.effects, 42);
        ASSERT_FALSE(bad.ok());
        EXPECT_NE(bad.error().message.find("damage"), std::string::npos)
            << bad.error().message;
    }
}

TEST(SaveFormat18Test, DamageAtOrAboveDurabilityRejected) {
    // The invariant 0 <= damage < durability: a damage AT the max (the
    // broken state — the stack is removed, it has no save form) or
    // ABOVE it is rejected and named.
    Fixture f;
    SaveGame s = makeSave();
    s.slots[3] = ItemStack {f.id("iron_pickaxe"), 1, 250.0f};
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok()) << j.error().message;
    for (double bad : {250.0, 251.0}) {
        Json root = *j;
        Json slots = root["inventory"]["slots"];
        Json slot3 = slots.at(3);
        slot3["damage"] = Json(bad);
        root["inventory"]["slots"] = withArrayElement(slots, 3, slot3);
        auto out = parseSave(root, f.items, f.blocks, f.effects, 42);
        ASSERT_FALSE(out.ok()) << bad;
        EXPECT_NE(out.error().message.find("damage"), std::string::npos)
            << out.error().message;
        EXPECT_NE(out.error().message.find("250"), std::string::npos)
            << out.error().message;
    }
}

TEST(SaveFormat18Test, DamageOnNoDurabilityItemRejected) {
    // An item WITHOUT a data durability cannot carry damage (the
    // no-op-field lie): the stick's damage key is rejected and named.
    Fixture f;
    SaveGame s = makeSave();
    s.slots[5] = ItemStack {f.id("stick"), 3, 5.0f};
    auto j = serializeSave(s, f.items);
    ASSERT_TRUE(j.ok()) << j.error().message;
    Json root = *j;
    Json slots = root["inventory"]["slots"];
    Json slot5 = slots.at(5);
    slot5["damage"] = Json(5.0);
    root["inventory"]["slots"] = withArrayElement(slots, 5, slot5);
    auto out = parseSave(root, f.items, f.blocks, f.effects, 42);
    ASSERT_FALSE(out.ok());
    EXPECT_NE(out.error().message.find("no durability"), std::string::npos)
        << out.error().message;
    EXPECT_NE(out.error().message.find("stick"), std::string::npos)
        << out.error().message;
}

// -- the armor wear formula -------------------------------------------------------

TEST(ArmorWearTest, FormulaIsExactFloat) {
    // Minecraft's float rule: each worn piece takes max(1, incoming /
    // 4) — 10 incoming -> 2.5 EXACT (the float division, not a floor
    // integer), 1 incoming -> 1 (the floor: 0.25 clamps up). The wear
    // applies to EVERY worn breakable piece of one event.
    EXPECT_FLOAT_EQ(armorWearPerPiece(10.0f), 2.5f);
    EXPECT_FLOAT_EQ(armorWearPerPiece(1.0f), 1.0f);
    EXPECT_FLOAT_EQ(armorWearPerPiece(4.0f), 1.0f); // exactly 1 (no clamp)

    Fixture f;
    f.equip("iron_helmet", 0);
    f.equip("iron_chestplate", 1);
    ASSERT_TRUE(f.player->damage(10.0f));
    EXPECT_FLOAT_EQ(f.player->armorSlot(0).damage, 2.5f);
    EXPECT_FLOAT_EQ(f.player->armorSlot(1).damage, 2.5f);
    // A second, smaller event: the 1.0 floor lands (0.25 -> 1).
    ASSERT_TRUE(f.player->damage(1.0f));
    EXPECT_FLOAT_EQ(f.player->armorSlot(0).damage, 3.5f);
    EXPECT_FLOAT_EQ(f.player->armorSlot(1).damage, 3.5f);
}

TEST(ArmorWearTest, DrowningBypassNeverWears) {
    // The SURV-2 exemption is the no-wear rule: a bypassArmor event
    // (drowning) deals its damage but leaves every piece's damage at
    // 0 — the "armor does not block, so it does not wear" alignment
    // (mutually exclusive with the non-bypass wear above).
    Fixture f;
    f.equip("iron_helmet", 0);
    f.equip("iron_chestplate", 1);
    ASSERT_TRUE(f.player->damage(2.0f, /*bypassArmor=*/true));
    EXPECT_FLOAT_EQ(f.player->health(), 18.0f); // the damage landed
    EXPECT_FLOAT_EQ(f.player->armorSlot(0).damage, 0.0f);
    EXPECT_FLOAT_EQ(f.player->armorSlot(1).damage, 0.0f);
    // Repeated drown hits keep it at 0 (the E2E S3b's 4-hit pin).
    for (int i = 0; i < 3; ++i) {
        ASSERT_TRUE(f.player->damage(2.0f, /*bypassArmor=*/true));
    }
    EXPECT_FLOAT_EQ(f.player->armorSlot(0).damage, 0.0f);
    EXPECT_FLOAT_EQ(f.player->armorSlot(1).damage, 0.0f);
}
