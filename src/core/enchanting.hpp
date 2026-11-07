// V2-12 (v2.13.0): the enchantment system — the pure function family + the
// data model + the enchantment registry.
//
// Everything here is a PURE function of its arguments (the offer roll is a
// deterministic advancing integer stream — the PITS #4 discipline, no
// double, the -O3 fusion red line). The game wires it to the panel, the
// player's XP pool, the item's enchant map and the save; the unit tests
// own every roll (fixed seed + fixed bookshelf count -> pinned offers).
//
// The content is DATA-DRIVEN (assets/data/enchantments/*.json): each
// enchantment names its effect TYPE (a code enum — the grants_effects
// precedent: the core switches on the EFFECT, never on the content id),
// the item classes it applies to (a vocabulary), its weight, its max level,
// the per-level eCost ranges (the Minecraft-verified tables, baked into the
// data) and its incompatible set (empty for the shipped seven — the field
// is kept for extension, the parse discipline unchanged).
//
// The item side (item.hpp): `enchant_class` (vocabulary) + `enchantability`
// (int) + `enchant_material` (bool, the lapis gate) are STRICT-parsed data;
// an ItemStack carries its applied enchantments as a name->level map (the
// save's {id: level} object, omit-when-empty).
#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "core/error.hpp"
#include "core/json.hpp"

namespace mc {

class AssetManager;
class World;
class BlockRegistry;
struct BlockPos;
struct ItemStack;

// The item's ENCHANT CLASS (the data `enchant_class` vocabulary). None =
// the item cannot be enchanted (the default — every pre-V2-12 item). sword
// / hoe are VOCABULARY (the armor_slot precedent: names the data may carry
// even though no shipped item uses them today).
enum class EnchantClass {
    None,
    Sword,
    Axe,
    Pick,
    Shovel,
    Armor,
    Bow,
    Hoe,
};
// The data vocabulary ("sword", "axe", ...) -> EnchantClass; -1 = unknown
// (the parse's named rejection).
int enchantClassFromName(const std::string& name);
// EnchantClass -> the data vocabulary ("", "sword", ...); None -> "".
const char* enchantClassName(EnchantClass c);

// The ENCHANT EFFECT type (a CODE enum — the core switches on this, never
// on the content id, the grants_effects precedent). None = a data entry
// that grants no in-game effect (the field is kept so a future purely-
// cosmetic enchantment can parse; the shipped seven all map to a real
// effect).
enum class EffectType {
    None,
    Sharpness,  // melee + (level+1) damage points
    Protection, // +4*level armor points (shared 20 cap)
    Efficiency, // break speed += (1+level^2) on the right tool
    Unbreaking, // durability-skip roll per wear event
    Power,      // bow shoot damage += level
    Speed,      // bow shoot speed += level (the documented deviation)
    Infinite,   // the last arrow is never spent (holds >= 1)
};
// The data `effect` vocabulary -> EffectType; -1 = unknown (named reject).
int effectFromName(const std::string& name);
// EffectType -> the data vocabulary ("", "sharpness", ...).
const char* effectName(EffectType t);

// One enchantment definition (pure data, loaded from assets/data/
// enchantments/<id>.json — the registry assigns no runtime id, the
// content's `id` string IS the stable key the item's enchant map + the
// save use).
struct Enchantment {
    std::string id; // "sharpness" (the save's enchant key)
    std::string name; // "锋利" (the panel's display name)
    EffectType effect = EffectType::None;
    std::vector<EnchantClass> classes; // the item classes it applies to
    int weight = 0; // the weighted-pick weight (the selection loop)
    int maxLevel = 0; // the highest level (eCost.size())
    // Per-level eCost ranges, INCLUSIVE, index = level-1 (level 1 is
    // eCost[0]). The selection loop admits an enchantment when the slot's
    // eCost falls in one of these bands and takes the HIGHEST such level.
    std::vector<std::pair<int, int>> eCost;
    // The incompatible enchant ids (Minecraft's exclusive pairs). Empty for
    // the shipped seven (no pair among them) — the field is kept, the
    // parse discipline (a named id must exist) is enforced.
    std::vector<std::string> incompatible;
};

// The EnchantmentRegistry: loads assets/data/enchantments/*.json (sorted,
// deterministic) and cross-checks the data (the strict-parse precedent).
// Content is static after load (the ItemRegistry pattern).
class EnchantmentRegistry {
public:
    EnchantmentRegistry() = default;

    // Parse + register one definition (the registerItem twin).
    Result<void> registerEnchantment(const Json& def);
    // Load every *.json in `relativeDir` (sorted).
    Result<void> loadDirectory(AssetManager& assets,
                               const std::string& relativeDir =
                                   "data/enchantments");

    std::size_t count() const { return m_list.size(); }
    const std::vector<Enchantment>& all() const { return m_list; }
    const Enchantment* find(const std::string& id) const;

    // The effect type of `id` (EffectType::None when unknown — the
    // consumer's inert default).
    EffectType effectFor(const std::string& id) const;
    // A content id is a VALID enchantment iff it is registered (the save's
    // cross-check + the item's enchant-map validation).
    bool knows(const std::string& id) const { return find(id) != nullptr; }

    // The INCOMPATIBLE-PAIR check (the save's cross-check): true when
    // `a` and `b` are a registered incompatible pair (symmetric, either
    // names the other).
    bool incompatiblePair(const std::string& a, const std::string& b) const;

private:
    std::vector<Enchantment> m_list; // load (file-name sorted) order
    std::map<std::string, std::size_t> m_byId;
};

// ---------------------------------------------------------------------------
// The pure function family (unit-test-owned — no game, no world mutation).
// ---------------------------------------------------------------------------

// The offer the panel shows per row: the XP cost (whole levels), the lapis
// cost and the applied enchantments (name -> level, 1..3 entries).
struct EnchantOffer {
    int xpCost = 0;
    int lapisCost = 0;
    std::map<std::string, int> enchants; // name -> level
};

// The three offers (top/mid/bottom) — the panel renders them in this order.
struct EnchantOffers {
    EnchantOffer top;
    EnchantOffer mid;
    EnchantOffer bottom;
};

// The bookshelf count (the panel-open-only evaluation — no panel = zero
// cost, the structural-immunity root): the 5x5 ring (Chebyshev distance
// EXACTLY 2 — the 16 outer cells) x two layers (the table's y and y+1),
// counting the blocks whose data flag `enchant_bookshelf` is set, capped at
// kBookshelfCap (Minecraft's 15). A pure function of (world, blocks,
// tablePos): no input, no state, deterministic.
inline constexpr int kBookshelfCap = 15;
int bookshelfCount(const World& world, const BlockRegistry& blocks,
                   const BlockPos& tablePos);

// The deterministic offer roll. A PURE function of its arguments: the seed
// is worldSeed ^ hash3(table, kEnchantTableSalt) ^ enchantability ^ xpSeed
// (the PITS #4 advancing stream — state += golden then splitmix64), and
// EVERY value is integer arithmetic (the -O3 double-fusion red line: the
// 53-bit float equivalents are the (v >> 11) integer forms, never a
// double). `enchantability` is the item's (0 = empty slot / a non-
// enchantable item — the roll still runs, the offers are just the no-
// bookshelf, zero-enchantability stream). B = bookshelfCount.
EnchantOffers generateOffers(std::uint64_t worldSeed, const BlockPos& tablePos,
                             EnchantClass cls, int enchantability,
                             std::uint64_t xpSeed, int B,
                             const EnchantmentRegistry& enchants);

// The XP charge (the integer-exact level deduction, consuming V2-11's
// curve): deduct `xpCost` WHOLE levels from the pool, keeping the within-
// level remainder (the modern Minecraft "deduct displayed levels"
// semantics). L0 = the pool's level; L1 = max(0, L0 - xpCost);
// newPool = T(L1) + (pool - T(L0)). Returns the new pool. A no-op (the
// pool unchanged) when xpCost <= 0 or the pool is below the first level's
// cost... no: it always deducts the whole levels, clamping L1 at 0 (a
// pool that cannot pay keeps its remainder at level 0).
int applyEnchantXpCharge(int pool, int xpCost);

// The xpSeed ADVANCE (the PITS #4 step-then-mix, one golden step): after a
// SUCCESSFUL enchant the seed advances so the next panel-open roll differs
// (Minecraft's XpSeed twin).
std::uint64_t xpSeedStep(std::uint64_t xpSeed);

// The NEW-WORLD xpSeed initializer (a fresh save's player.xp_seed):
// worldSeed ^ kXpSeedInit (deterministic — a new world's seed is fixed by
// the config/CLI).
inline constexpr std::uint64_t kXpSeedInit = 0x5EED123456789ABCULL;
std::uint64_t xpSeedInit(std::uint64_t worldSeed);

// V3-12b (R10): the per-PLAYER xpSeed salts — player i's fresh-world xp
// seed = xpSeedInit(worldSeed ^ kSaltPlayerXp[i]). Index 0 (the primary)
// is 0 = the original worldSeed^kXpSeedInit, the solo bit-level red line
// (a single-player world's seed is unchanged). The remote player's
// non-zero salt de-collides the two players' enchant / unbreaking wear
// streams (the kSalt family's continuation discipline — the value is
// collision-checked against every kSalt* / k*RollSalt in src/: the 0x10D
// 0E11D5E4D73A2..A6 end family is the nearest, A7 is the next free slot).
// V3-12c2: the table's 8-entry width (the kNetMaxPlayers' cap — the
// c1's N-generalization's de-collision for every slot; the A8..AC
// continuation, the same family's spacing).
inline constexpr std::uint64_t kSaltPlayerXp[8] = {
    0x0000000000000000ULL,  // primary (solo bit-level)
    0x10D0E11D5E4D73A7ULL,  // slot 1 (the 12b's second)
    0x10D0E11D5E4D73A8ULL,  // slot 2
    0x10D0E11D5E4D73A9ULL,  // slot 3
    0x10D0E11D5E4D73AAULL,  // slot 4
    0x10D0E11D5E4D73ABULL,  // slot 5
    0x10D0E11D5E4D73ACULL,  // slot 6
    0x10D0E11D5E4D73ADULL,  // slot 7
};
inline std::uint64_t xpSeedInitFor(std::uint64_t worldSeed, std::size_t i) {
    return xpSeedInit(worldSeed ^ kSaltPlayerXp[i % 8]);
}

// The UNBREAKING wear roll (the durability-skip): for a wear event of
// `amount` on an item with unbreaking `level` (isArmor selects the tool vs
// armor formula), return the wear ACTUALLY applied (0..amount — a skip
// drops that point's wear). The roll is a PURE function of (level, isArmor,
// amount, state): the state (see unbreakingState) is NO frame dependence —
// the damage history determines the roll, so the same damageBefore always
// gives the same skip decision (the PITS #4 advancing stream over the
// state). Per integer point: tool p = level/(level+1); armor p =
// 2*level/(5*(level+1)) (the Minecraft 1.21 remove_binomial values).
// Integer dice: (splitmix64(advance(state)) % denom) < numer -> that point
// is SKIPPED; else it wears. `amount` is the event's wear (1.0 / 2.0 for
// the tool events; the float armorWearPerPiece for armor — converted to
// integer points via ceil, the documented armor-point model). level <= 0
// returns `amount` UNCHANGED (the pre-V2-12 bit-identical path for every
// non-enchant item).
inline constexpr std::uint64_t kUnbreakingSalt = 0x91A52B7C6D4E3F01ULL;
float unbreakingWear(int level, bool isArmor, float amount,
                     std::uint64_t state);
// The unbreaking roll's STATE (the design's formula): worldSeed ^
// splitmix64(damageBeforeBits * golden) ^ kUnbreakingSalt ^ itemRegistryId.
// NO frame dependence — the accumulated damage before this event
// (damageBefore, a float whose bit pattern is the stable key) determines
// the roll, so consecutive wear events on the same item (rising damage)
// get different states, and a re-derivation at the same damage is
// identical (headless byte-reproducible). `itemRegistryId` is the item's
// ItemID value (decorrelates items).
std::uint64_t unbreakingState(std::uint64_t worldSeed, float damageBefore,
                              int itemRegistryId);

// The panel's APPLY gate (the four conditions, the design's LMB-offer rule):
// the item has a class AND is not yet enchanted AND the pool's level >=
// xpCost AND the inventory holds >= lapisCost of the material. Returns true
// when the click may apply. Pure over its arguments (the game resolves the
// names to ids).
bool offerApplicable(const ItemStack& itemSlot, int itemEnchantability,
                     int poolLevel, int lapisCount, const EnchantOffer& o);

// The ITEM's applied enchant level sum for an effect (the consumer's pure
// accessor): 0 when the item has no such effect (the no-enchant bit-level
// invariant — every pre-V2-12 item returns 0).
int effectLevel(const ItemStack& stack, const EnchantmentRegistry& reg,
                EffectType t);
// The item's total unbreaking level (the wear-roll input).
int unbreakingLevel(const ItemStack& stack, const EnchantmentRegistry& reg);
// The item's sharpness level (melee).
int sharpnessLevel(const ItemStack& stack, const EnchantmentRegistry& reg);
// The item's protection level (armor).
int protectionLevel(const ItemStack& stack, const EnchantmentRegistry& reg);
// The item's efficiency level (mining).
int efficiencyLevel(const ItemStack& stack, const EnchantmentRegistry& reg);
// The item's power level (bow damage).
int powerLevel(const ItemStack& stack, const EnchantmentRegistry& reg);
// The item's speed level (bow velocity).
int speedLevel(const ItemStack& stack, const EnchantmentRegistry& reg);
// True when the item is INFINITE (the field OR the enchant — the design's
// "并存取或").
bool heldInfinite(const ItemStack& stack, const EnchantmentRegistry& reg);

} // namespace mc
