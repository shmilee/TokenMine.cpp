// V2-12 (v2.13.0): the enchantment system's pure function family
// (core/enchanting). The offer roll is pinned for fixed inputs (a
// determinism red line — a formula edit turns the suite red), and every
// structural invariant (class match, level-in-band, xpCost range, the
// B=0 table) is checked. bookshelfCount / applyEnchantXpCharge /
// unbreakingWear are pinned at their boundaries.
#include <gtest/gtest.h>

#include <cstdint>
#include <map>
#include <string>

#include "block/block_registry.hpp"
#include "core/enchanting.hpp"
#include "core/json.hpp"
#include "item/item.hpp"
#include "world/world.hpp"

using namespace mc;

namespace {

Json parseDef(const std::string& text) {
    auto r = Json::parse(text);
    if (!r.ok()) {
        ADD_FAILURE() << "parse failed: " << r.error().message;
        return Json::object();
    }
    return *r;
}

// The shipped seven enchantments, registered in file-name-sorted order
// (efficiency, infinite, power, protection, sharpness, speed, unbreaking —
// the loadDirectory's sort). The bands mirror assets/data/enchantments/.
EnchantmentRegistry sevenEnchants() {
    EnchantmentRegistry reg;
    const std::string defs[] = {
        R"({"id":"efficiency","name":"效率","effect":"efficiency","classes":["pick","shovel","axe"],"weight":10,"ecost":[[1,10],[11,20],[21,30],[31,40],[41,91]]})",
        R"({"id":"infinite","name":"无限","effect":"infinite","classes":["bow"],"weight":1,"ecost":[[20,50]]})",
        R"({"id":"power","name":"力量","effect":"power","classes":["bow"],"weight":10,"ecost":[[1,10],[11,20],[21,30],[31,40],[41,56]]})",
        R"({"id":"protection","name":"保护","effect":"protection","classes":["armor"],"weight":10,"ecost":[[5,12],[13,20],[21,28],[29,37]]})",
        R"({"id":"sharpness","name":"锋利","effect":"sharpness","classes":["axe"],"weight":10,"ecost":[[1,11],[12,22],[23,33],[34,44],[45,65]]})",
        R"({"id":"speed","name":"速度","effect":"speed","classes":["bow"],"weight":10,"ecost":[[12,31],[32,50],[51,91]]})",
        R"({"id":"unbreaking","name":"耐久","effect":"unbreaking","classes":["axe","pick","shovel","armor","bow"],"weight":5,"ecost":[[5,12],[13,20],[21,71]]})",
    };
    for (const std::string& d : defs) {
        auto r = reg.registerEnchantment(parseDef(d));
        if (!r.ok()) {
            throw std::runtime_error(r.error().message);
        }
    }
    return reg;
}

// A registry with a bookshelf (enchant_bookshelf) + an inert stone.
BlockRegistry bookshelfRegistry(BlockID& bookshelf, BlockID& stone) {
    BlockRegistry reg;
    const std::string bdef =
        "{\"id\":\"bookshelf\",\"properties\":{\"hardness\":1.5,"
        "\"drops\":\"bookshelf\",\"enchant_bookshelf\":true},"
        "\"model\":{\"textures\":{\"top\":\"bookshelf\",\"side\":\"bookshelf\","
        "\"bottom\":\"bookshelf\"}}}";
    const std::string sdef =
        "{\"id\":\"stone\",\"properties\":{\"hardness\":1.5,\"drops\":\"stone\"},"
        "\"model\":{\"textures\":{\"top\":\"stone\",\"side\":\"stone\","
        "\"bottom\":\"stone\"}}}";
    auto rb = reg.registerBlock(parseDef(bdef));
    if (!rb.ok()) throw std::runtime_error(rb.error().message);
    bookshelf = *rb;
    auto rs = reg.registerBlock(parseDef(sdef));
    if (!rs.ok()) throw std::runtime_error(rs.error().message);
    stone = *rs;
    return reg;
}

} // namespace

// ===========================================================================
// bookshelfCount
// ===========================================================================

TEST(EnchantBookshelfCount, EmptyWorldIsZero) {
    BlockRegistry reg;
    BlockID bs, st;
    reg = bookshelfRegistry(bs, st);
    World world;
    world.getOrCreateChunk({0, 0, 0});
    EXPECT_EQ(bookshelfCount(world, reg, {8, 4, 8}), 0);
}

TEST(EnchantBookshelfCount, CountsTheOuterRingTwoLayers) {
    BlockRegistry reg;
    BlockID bs, st;
    reg = bookshelfRegistry(bs, st);
    World world;
    world.getOrCreateChunk({0, 0, 0});
    const BlockPos table {8, 4, 8};
    // Place bookshelves at a KNOWN ring cell (max(|dx|,|dz|)==2): (10,4,8)
    // = +2 X. Both layers (y=4 and y=5) count.
    world.setBlock(10, 4, 8, bs); // ring, layer y
    EXPECT_EQ(bookshelfCount(world, reg, table), 1);
    world.setBlock(10, 5, 8, bs); // ring, layer y+1
    EXPECT_EQ(bookshelfCount(world, reg, table), 2);
}

TEST(EnchantBookshelfCount, InnerCellsDoNotCount) {
    BlockRegistry reg;
    BlockID bs, st;
    reg = bookshelfRegistry(bs, st);
    World world;
    world.getOrCreateChunk({0, 0, 0});
    const BlockPos table {8, 4, 8};
    // A bookshelf at max(|dx|,|dz|)<2 (the inner 3x3) is NOT counted.
    world.setBlock(9, 4, 8, bs); // dx=1 -> inner
    world.setBlock(8, 4, 9, bs); // dz=1 -> inner
    world.setBlock(8, 4, 8, bs); // the table cell itself (dx=dz=0) -> inner
    EXPECT_EQ(bookshelfCount(world, reg, table), 0);
}

TEST(EnchantBookshelfCount, InertBlocksDoNotCount) {
    BlockRegistry reg;
    BlockID bs, st;
    reg = bookshelfRegistry(bs, st);
    World world;
    world.getOrCreateChunk({0, 0, 0});
    const BlockPos table {8, 4, 8};
    // Stone (no enchant_bookshelf) at a ring cell does not count.
    world.setBlock(10, 4, 8, st);
    world.setBlock(6, 5, 8, st);
    EXPECT_EQ(bookshelfCount(world, reg, table), 0);
}

TEST(EnchantBookshelfCount, CapsAtFifteen) {
    BlockRegistry reg;
    BlockID bs, st;
    reg = bookshelfRegistry(bs, st);
    World world;
    world.getOrCreateChunk({0, 0, 0});
    const BlockPos table {8, 4, 8};
    // Fill EVERY ring cell at BOTH layers (16 cells x 2 = 32 potential) ->
    // the count caps at 15.
    for (int dz = -2; dz <= 2; ++dz) {
        for (int dx = -2; dx <= 2; ++dx) {
            if (std::max(std::abs(dx), std::abs(dz)) == 2) {
                world.setBlock(table.x + dx, table.y, table.z + dz, bs);
                world.setBlock(table.x + dx, table.y + 1, table.z + dz, bs);
            }
        }
    }
    EXPECT_EQ(bookshelfCount(world, reg, table), 15);
}

// ===========================================================================
// generateOffers (determinism + structure + pins)
// ===========================================================================

TEST(EnchantOffers, IsDeterministicForFixedInputs) {
    const EnchantmentRegistry reg = sevenEnchants();
    const std::uint64_t seed = 123456789;
    const std::uint64_t xpSeed = xpSeedInit(seed);
    EnchantOffers a = generateOffers(seed, {8, 4, 8}, EnchantClass::Axe, 14,
                                     xpSeed, 0, reg);
    EnchantOffers b = generateOffers(seed, {8, 4, 8}, EnchantClass::Axe, 14,
                                     xpSeed, 0, reg);
    // A full re-derivation (different code path, same math) matches too.
    EnchantOffers c = generateOffers(seed, {8, 4, 8}, EnchantClass::Axe, 14,
                                     xpSeed, 0, reg);
    EXPECT_EQ(a.top.xpCost, b.top.xpCost);
    EXPECT_EQ(a.mid.xpCost, b.mid.xpCost);
    EXPECT_EQ(a.bottom.xpCost, b.bottom.xpCost);
    EXPECT_EQ(a.top.lapisCost, b.top.lapisCost);
    EXPECT_EQ(a.top.enchants, b.top.enchants);
    EXPECT_EQ(a.mid.enchants, b.mid.enchants);
    EXPECT_EQ(a.bottom.enchants, b.bottom.enchants);
    EXPECT_EQ(a.top.enchants, c.top.enchants);
}

TEST(EnchantOffers, ChangesWithTheSeedInputs) {
    // The offer is a pure function of the seed inputs: changing ANY input
    // (world seed / table position / xpSeed) changes the stream and thus the
    // full 3-row offer. Compare the ENTIRE structure (a full-structure
    // collision would mean the seed is not actually consumed — the red line).
    const EnchantmentRegistry reg = sevenEnchants();
    const std::uint64_t seed = 123456789;
    const std::uint64_t xpSeed = xpSeedInit(seed);
    auto fullEq = [](const EnchantOffers& a, const EnchantOffers& b) {
        return a.top.xpCost == b.top.xpCost &&
               a.top.lapisCost == b.top.lapisCost &&
               a.top.enchants == b.top.enchants &&
               a.mid.xpCost == b.mid.xpCost &&
               a.mid.lapisCost == b.mid.lapisCost &&
               a.mid.enchants == b.mid.enchants &&
               a.bottom.xpCost == b.bottom.xpCost &&
               a.bottom.lapisCost == b.bottom.lapisCost &&
               a.bottom.enchants == b.bottom.enchants;
    };
    const EnchantOffers base =
        generateOffers(seed, {8, 4, 8}, EnchantClass::Axe, 14, xpSeed, 0, reg);
    // A different world seed -> a different stream.
    const EnchantOffers otherSeed =
        generateOffers(seed + 1, {8, 4, 8}, EnchantClass::Axe, 14, xpSeed, 0,
                       reg);
    EXPECT_FALSE(fullEq(base, otherSeed));
    // A different table position -> a different hash3 -> a different stream.
    const EnchantOffers otherPos =
        generateOffers(seed, {9, 4, 8}, EnchantClass::Axe, 14, xpSeed, 0, reg);
    EXPECT_FALSE(fullEq(base, otherPos));
    // A different xpSeed -> a different stream (the advance's decorrelation).
    const EnchantOffers otherXp =
        generateOffers(seed, {8, 4, 8}, EnchantClass::Axe, 14, xpSeedStep(xpSeed),
                       0, reg);
    EXPECT_FALSE(fullEq(base, otherXp));
}

TEST(EnchantOffers, TheBZeroXpCostTable) {
    // The design's B=0 pinned table: top 1-2 / mid 1-6 / bottom 1-8.
    // Over many seeds the xpCost must stay in those ranges.
    const EnchantmentRegistry reg = sevenEnchants();
    for (std::uint64_t s = 1; s <= 200; ++s) {
        const std::uint64_t xpSeed = xpSeedInit(s);
        const EnchantOffers o =
            generateOffers(s, {8, 4, 8}, EnchantClass::Axe, 14, xpSeed, 0, reg);
        EXPECT_GE(o.top.xpCost, 1);
        EXPECT_LE(o.top.xpCost, 2);
        EXPECT_GE(o.mid.xpCost, 1);
        EXPECT_LE(o.mid.xpCost, 6);
        EXPECT_GE(o.bottom.xpCost, 1);
        EXPECT_LE(o.bottom.xpCost, 8);
        // lapisCost is always 1..3 (the uniform deviation).
        EXPECT_GE(o.top.lapisCost, 1);
        EXPECT_LE(o.top.lapisCost, 3);
        EXPECT_GE(o.mid.lapisCost, 1);
        EXPECT_LE(o.mid.lapisCost, 3);
        EXPECT_GE(o.bottom.lapisCost, 1);
        EXPECT_LE(o.bottom.lapisCost, 3);
    }
}

TEST(EnchantOffers, EnchantsMatchTheClassAndBands) {
    // An axe (class Axe) can only get sharpness / efficiency / unbreaking
    // (the classes that include Axe). Each applied level must fall in the
    // enchantment's band for the slot's eCost (structural: the level is
    // derived from the cost, so it is in-band by construction — check the
    // class match + level bounds).
    const EnchantmentRegistry reg = sevenEnchants();
    for (std::uint64_t s = 1; s <= 100; ++s) {
        const std::uint64_t xpSeed = xpSeedInit(s);
        const EnchantOffers o =
            generateOffers(s, {8, 4, 8}, EnchantClass::Axe, 14, xpSeed, 15, reg);
        for (const EnchantOffer& row : {o.top, o.mid, o.bottom}) {
            for (const auto& [id, lvl] : row.enchants) {
                const Enchantment* e = reg.find(id);
                ASSERT_NE(e, nullptr) << id;
                // Class must include Axe.
                bool ok = false;
                for (EnchantClass c : e->classes) {
                    if (c == EnchantClass::Axe) {
                        ok = true;
                        break;
                    }
                }
                EXPECT_TRUE(ok) << "enchant " << id << " does not apply to Axe";
                // Level within [1, maxLevel].
                EXPECT_GE(lvl, 1);
                EXPECT_LE(lvl, e->maxLevel);
            }
        }
    }
}

TEST(EnchantOffers, AClasslessItemGetsNoEnchants) {
    const EnchantmentRegistry reg = sevenEnchants();
    const std::uint64_t xpSeed = xpSeedInit(1);
    const EnchantOffers o = generateOffers(1, {8, 4, 8}, EnchantClass::None, 0,
                                           xpSeed, 15, reg);
    EXPECT_TRUE(o.top.enchants.empty());
    EXPECT_TRUE(o.mid.enchants.empty());
    EXPECT_TRUE(o.bottom.enchants.empty());
}

TEST(EnchantOffers, ABowGetsOnlyBowEnchants) {
    const EnchantmentRegistry reg = sevenEnchants();
    const std::uint64_t xpSeed = xpSeedInit(7);
    const EnchantOffers o =
        generateOffers(7, {8, 4, 8}, EnchantClass::Bow, 1, xpSeed, 15, reg);
    for (const EnchantOffer& row : {o.top, o.mid, o.bottom}) {
        for (const auto& [id, lvl] : row.enchants) {
            const Enchantment* e = reg.find(id);
            ASSERT_NE(e, nullptr);
            bool bow = false;
            for (EnchantClass c : e->classes) {
                if (c == EnchantClass::Bow) {
                    bow = true;
                    break;
                }
            }
            EXPECT_TRUE(bow) << "enchant " << id << " is not a bow enchant";
        }
    }
}

// The pinned offers for the canonical fixture (a determinism red line — a
// formula / stream-order edit turns the suite red). Values captured from the
// implementation for (seed=123456789, table=(8,4,8), class=Axe, ability=14,
// xpSeed=xpSeedInit(seed)). B=0 (no bookshelves) and B=15 (full ring).
TEST(EnchantOffers, PinnedOffersForTheCanonicalFixture) {
    const EnchantmentRegistry reg = sevenEnchants();
    const std::uint64_t seed = 123456789;
    const std::uint64_t xpSeed = xpSeedInit(seed);

    // B=0: the no-bookshelf stream (the top/mid/bottom xpCost 2/5/4, the
    // B=0 table's mid range 1-6, bottom 1-8).
    const EnchantOffers b0 =
        generateOffers(seed, {8, 4, 8}, EnchantClass::Axe, 14, xpSeed, 0, reg);
    EXPECT_EQ(b0.top.xpCost, 2);
    EXPECT_EQ(b0.top.lapisCost, 1);
    EXPECT_EQ(b0.top.enchants,
              (std::map<std::string, int> {{"efficiency", 1},
                                           {"sharpness", 1}}));
    EXPECT_EQ(b0.mid.xpCost, 5);
    EXPECT_EQ(b0.mid.lapisCost, 2);
    EXPECT_EQ(b0.mid.enchants,
              (std::map<std::string, int> {{"efficiency", 1},
                                           {"sharpness", 1}}));
    EXPECT_EQ(b0.bottom.xpCost, 4);
    EXPECT_EQ(b0.bottom.lapisCost, 1);
    EXPECT_EQ(b0.bottom.enchants,
              (std::map<std::string, int> {{"efficiency", 1},
                                           {"sharpness", 1}}));

    // B=15: the full-ring stream. The bottom slot's xpCost = max(xpBase,
    // 2*B) = 30 (pinned — 2*15 dominates xpBase <= 30), and the high eCost
    // reaches the top bands (unbreaking III, the [21,71] band at cost 47).
    const EnchantOffers b15 =
        generateOffers(seed, {8, 4, 8}, EnchantClass::Axe, 14, xpSeed, 15, reg);
    EXPECT_EQ(b15.top.xpCost, 5);
    EXPECT_EQ(b15.top.lapisCost, 1);
    EXPECT_EQ(b15.top.enchants,
              (std::map<std::string, int> {{"efficiency", 1},
                                           {"sharpness", 1}}));
    EXPECT_EQ(b15.mid.xpCost, 16);
    EXPECT_EQ(b15.mid.lapisCost, 2);
    EXPECT_EQ(b15.mid.enchants,
              (std::map<std::string, int> {{"efficiency", 3},
                                           {"sharpness", 1},
                                           {"unbreaking", 1}}));
    EXPECT_EQ(b15.bottom.xpCost, 30);
    EXPECT_EQ(b15.bottom.lapisCost, 1);
    EXPECT_EQ(b15.bottom.enchants,
              (std::map<std::string, int> {{"unbreaking", 3}}));
}

// ===========================================================================
// applyEnchantXpCharge
// ===========================================================================

TEST(EnchantXpCharge, DeductsWholeLevelsWithRemainder) {
    // pool = T(2) + 3 = 40 + 3 = 43 (level 2, 3 into the level; T(2)=4*2+
    // 6*2=... let me use the curve: T(2)=2^2+6*2=4+12=16? No: T(2)=L^2+6L for
    // L<=16 = 4+12=16. So pool=16+3=19 is level 2 with 3 into. Deduct 1
    // level -> L1=1, newPool=T(1)+(19-T(2))=7+(19-16)=7+3=10 (level 1, 3
    // into; T(1)=1+6=7, into=10-7=3). The REMAINDER (3) is preserved.
    EXPECT_EQ(applyEnchantXpCharge(19, 1), 10);
    // Deduct 2 levels from 19 -> L1=0, newPool=T(0)+(19-16)=0+3=3.
    EXPECT_EQ(applyEnchantXpCharge(19, 2), 3);
}

TEST(EnchantXpCharge, ClampsAtZeroWhenUnderpaying) {
    // pool=19 (level 2). Deducting 5 levels -> L1=max(0,2-5)=0, newPool=
    // T(0)+(19-16)=3 (the remainder is kept at level 0). It never goes
    // negative.
    EXPECT_EQ(applyEnchantXpCharge(19, 5), 3);
    // pool at exactly a level boundary (16 = level 2, 0 into). Deduct 1 ->
    // L1=1, newPool=T(1)+(16-16)=7.
    EXPECT_EQ(applyEnchantXpCharge(16, 1), 7);
}

TEST(EnchantXpCharge, NoOpOnZeroCost) {
    EXPECT_EQ(applyEnchantXpCharge(100, 0), 100);
    EXPECT_EQ(applyEnchantXpCharge(100, -3), 100);
}

// ===========================================================================
// unbreakingWear
// ===========================================================================

TEST(EnchantUnbreakingWear, NoUnbreakingIsTheFullWear) {
    // level 0 -> the wear is applied in full (the pre-V2-12 bit-identical
    // path for every non-enchant item).
    EXPECT_EQ(unbreakingWear(0, false, 2, 0xABC), 2);
    EXPECT_EQ(unbreakingWear(0, true, 5, 0xABC), 5);
    EXPECT_EQ(unbreakingWear(0, false, 0, 0xABC), 0);
}

TEST(EnchantUnbreakingWear, IsDeterministicPerState) {
    // The same (level, isArmor, amount, state) always gives the same wear.
    const int a = unbreakingWear(3, false, 2, 0x123456789);
    const int b = unbreakingWear(3, false, 2, 0x123456789);
    EXPECT_EQ(a, b);
    // The wear is in [0, amount].
    EXPECT_GE(a, 0);
    EXPECT_LE(a, 2);
}

TEST(EnchantUnbreakingWear, HigherLevelSkipsMore) {
    // Over many states, a higher unbreaking level applies STRICTLY LESS
    // wear on average (the skip probability rises with level). This is a
    // statistical property pinned over a fixed state sweep.
    int wear1 = 0, wear3 = 0;
    for (std::uint64_t st = 0; st < 2000; ++st) {
        wear1 += unbreakingWear(1, false, 1, st);
        wear3 += unbreakingWear(3, false, 1, st);
    }
    // level 1: p=1/2 skip -> ~1000 applied of 2000. level 3: p=3/4 skip ->
    // ~500 applied. The level-3 total must be well below the level-1 total.
    EXPECT_LT(wear3, wear1);
    EXPECT_GT(wear1, 0);
}

TEST(EnchantUnbreakingWear, ArmorFormulaSkipsLessThanTool) {
    // For the same level, the ARMOR skip probability (2L/(5(L+1))) is LOWER
    // than the tool (L/(L+1)), so armor wears MORE (applies more). level 1:
    // tool p=1/2, armor p=2/10=1/5. Over a state sweep, armor applies more.
    int toolWear = 0, armorWear = 0;
    for (std::uint64_t st = 0; st < 2000; ++st) {
        toolWear += unbreakingWear(1, false, 1, st);
        armorWear += unbreakingWear(1, true, 1, st);
    }
    EXPECT_GT(armorWear, toolWear);
}

// ===========================================================================
// the item-side effect accessors
// ===========================================================================

TEST(EnchantEffectAccessors, EmptyItemIsZero) {
    const EnchantmentRegistry reg = sevenEnchants();
    ItemStack empty;
    EXPECT_EQ(sharpnessLevel(empty, reg), 0);
    EXPECT_EQ(protectionLevel(empty, reg), 0);
    EXPECT_EQ(efficiencyLevel(empty, reg), 0);
    EXPECT_EQ(unbreakingLevel(empty, reg), 0);
    EXPECT_EQ(powerLevel(empty, reg), 0);
    EXPECT_EQ(speedLevel(empty, reg), 0);
    EXPECT_FALSE(heldInfinite(empty, reg));
}

TEST(EnchantEffectAccessors, ReadsTheAppliedLevels) {
    const EnchantmentRegistry reg = sevenEnchants();
    ItemStack stack;
    stack.item = ItemID{0};
    stack.count = 1;
    stack.enchants["sharpness"] = 5;
    stack.enchants["unbreaking"] = 3;
    EXPECT_EQ(sharpnessLevel(stack, reg), 5);
    EXPECT_EQ(unbreakingLevel(stack, reg), 3);
    EXPECT_EQ(protectionLevel(stack, reg), 0);
    EXPECT_FALSE(heldInfinite(stack, reg));
    stack.enchants["infinite"] = 1;
    EXPECT_TRUE(heldInfinite(stack, reg));
    // sharpness 5 -> melee bonus (L+1) = 6 points.
    EXPECT_EQ(sharpnessLevel(stack, reg) + 1, 6);
}

// ===========================================================================
// the registry's strict parse
// ===========================================================================

TEST(EnchantRegistry, RejectsUnknownEffect) {
    EnchantmentRegistry reg;
    auto r = reg.registerEnchantment(parseDef(
        R"({"id":"x","name":"x","effect":"bogus","classes":["axe"],"weight":1,"ecost":[[1,5]]})"));
    EXPECT_FALSE(r.ok());
}

TEST(EnchantRegistry, RejectsUnknownClass) {
    EnchantmentRegistry reg;
    auto r = reg.registerEnchantment(parseDef(
        R"({"id":"x","name":"x","effect":"sharpness","classes":["wrench"],"weight":1,"ecost":[[1,5]]})"));
    EXPECT_FALSE(r.ok());
}

TEST(EnchantRegistry, RejectsInvertedBand) {
    EnchantmentRegistry reg;
    auto r = reg.registerEnchantment(parseDef(
        R"({"id":"x","name":"x","effect":"sharpness","classes":["axe"],"weight":1,"ecost":[[5,1]]})"));
    EXPECT_FALSE(r.ok());
}

TEST(EnchantRegistry, RejectsOverlappingBands) {
    EnchantmentRegistry reg;
    // band 2 must start above band 1's max (10); [8,20] overlaps.
    auto r = reg.registerEnchantment(parseDef(
        R"({"id":"x","name":"x","effect":"sharpness","classes":["axe"],"weight":1,"ecost":[[1,10],[8,20]]})"));
    EXPECT_FALSE(r.ok());
}

TEST(EnchantRegistry, RejectsZeroWeight) {
    EnchantmentRegistry reg;
    auto r = reg.registerEnchantment(parseDef(
        R"({"id":"x","name":"x","effect":"sharpness","classes":["axe"],"weight":0,"ecost":[[1,5]]})"));
    EXPECT_FALSE(r.ok());
}

TEST(EnchantRegistry, RejectsUnknownIncompatible) {
    EnchantmentRegistry reg;
    auto r = reg.registerEnchantment(parseDef(
        R"({"id":"x","name":"x","effect":"sharpness","classes":["axe"],"weight":1,"ecost":[[1,5]],"incompatible":["nope"]})"));
    EXPECT_FALSE(r.ok());
}

TEST(EnchantRegistry, RejectsDuplicateId) {
    EnchantmentRegistry reg;
    const std::string d =
        R"({"id":"x","name":"x","effect":"sharpness","classes":["axe"],"weight":1,"ecost":[[1,5]]})";
    EXPECT_TRUE(reg.registerEnchantment(parseDef(d)).ok());
    EXPECT_FALSE(reg.registerEnchantment(parseDef(d)).ok());
}
