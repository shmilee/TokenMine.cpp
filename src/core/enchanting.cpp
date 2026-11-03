// V2-12 (v2.13.0): the enchantment system's implementation (see the header
// for the model). Every function is a pure function of its arguments; the
// offer roll is a single deterministic advancing integer stream (the PITS
// #4 discipline: state += golden then splitmix64 — no double anywhere, the
// -O3 fusion red line).
#include "core/enchanting.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "block/block_registry.hpp"
#include "core/json.hpp"
#include "core/logger.hpp"
#include "core/xp_curve.hpp" // levelFromTotalXp / totalXpForLevel (V2-11)
#include "item/item.hpp" // ItemStack (the effect accessors' input)
#include "world/noise.hpp" // splitmix64 + hash3
#include "world/coordinates.hpp"
#include "world/world.hpp"

namespace mc {

namespace {

// PITS #4: splitmix64 is a PURE finalizer — the advancing stream adds the
// golden step BEFORE the mix (a constant input would give a constant
// stream). The weather.cpp constant, reused (the project's one golden).
constexpr std::uint64_t kGolden = 0x9E3779B97F4A7C15ULL;
// 2^53 — the 53-bit integer domain (the double mantissa's exact range).
constexpr std::uint64_t kU53 = 1ULL << 53;
// The offer roll's dedicated salt (the chance-drop / fire-particle salt
// precedent — decorrelates the enchant stream from every other roll).
constexpr std::uint64_t kEnchantTableSalt = 0xC0FFEE1234567890ULL;

// The panel's offer count (Minecraft: three rows).
constexpr int kOfferRows = 3;

// The advancing stream (PITS #4 step-then-mix).
struct Stream {
    std::uint64_t state = 0;
    explicit Stream(std::uint64_t s) : state(s) {}
    std::uint64_t next() {
        state += kGolden;
        return splitmix64(state);
    }
    // One uniform [0, n] integer (n >= 0). ALWAYS advances the stream
    // (randInt(0) = 0 but still consumes a draw — the stream alignment is
    // part of the determinism contract).
    int randInt(int n) {
        return static_cast<int>(next() % static_cast<std::uint64_t>(n + 1));
    }
};

// The three slots' xpCost formulas (Minecraft-exact, the design's B=0 table
// pinned: top 1-2 / mid 1-6 / bottom 1-8). slotIndex 0=top, 1=mid, 2=bottom.
int slotXpCost(int slotIndex, int xpBase, int B) {
    switch (slotIndex) {
        case 0:
            return std::max(1, xpBase / 3);
        case 1:
            return (2 * xpBase) / 3 + 1;
        default: // 2 = bottom
            return std::max(xpBase, 2 * B);
    }
}

// The slot's initial enchantment cost eCost (the design's integer form):
//   base  = xpCost + 1 + randInt(xpCost/4) + randInt(xpCost/4)
//   tri   = (f1 + f2 - 2^53)  in [-2^53, 2^53)  (two 53-bit draws)
//   delta = (base * 3 * tri) / (20 * 2^53)  (C++ truncation toward zero —
//           the round() of the 15% triangle jitter, the integer division
//           form; |delta| <= base*0.15+1)
//   eCost = base + delta  (>= 1: the floor keeps a non-zero cost)
int slotECost(Stream& s, int xpCost) {
    const int base = xpCost + 1 + s.randInt(xpCost / 4) + s.randInt(xpCost / 4);
    const std::uint64_t f1 = s.next() >> 11; // 53 bits
    const std::uint64_t f2 = s.next() >> 11; // 53 bits
    const std::int64_t tri =
        static_cast<std::int64_t>(f1 + f2) - static_cast<std::int64_t>(kU53);
    // numerator = base * 3 * tri fits int64: |base*3*tri| <= (2*xc+1)*3*2^53
    // and xc (xpCost) is bounded by the curve's reachable levels in practice
    // (a few hundred at most) -> well under 2^63.
    const std::int64_t num = static_cast<std::int64_t>(base) * 3 * tri;
    const std::int64_t den = 20 * static_cast<std::int64_t>(kU53);
    const int delta = static_cast<int>(num / den); // trunc toward zero
    return std::max(1, base + delta);
}

// The highest level (1-based) whose eCost band contains `e`; 0 when none
// (the enchantment is not a candidate at this cost). eCost is ascending in
// level (the data is validated to be so), so walk from the top.
int levelForCost(const Enchantment& e, int cost) {
    for (int lvl = static_cast<int>(e.eCost.size()); lvl >= 1; --lvl) {
        const auto& band = e.eCost[static_cast<std::size_t>(lvl - 1)];
        if (cost >= band.first && cost <= band.second) {
            return lvl;
        }
    }
    return 0;
}

// The weighted pick over `cand` (each a (enchant index into `enchants`,
// level) pair): one stream draw reduced into the total-weight domain and
// walked over the cumulative list (the game_entities pick precedent).
// Returns the chosen pair's index into `cand`.
int weightedPick(Stream& s, const std::vector<const Enchantment*>& cand) {
    int total = 0;
    for (const Enchantment* e : cand) {
        total += e->weight;
    }
    if (total <= 0) {
        return 0; // defensive: the registry guarantees weight >= 1
    }
    const int r = s.randInt(total - 1); // 0..total-1
    int acc = 0;
    for (std::size_t i = 0; i < cand.size(); ++i) {
        acc += cand[i]->weight;
        if (r < acc) {
            return static_cast<int>(i);
        }
    }
    return static_cast<int>(cand.size() - 1);
}

// The selection loop (Minecraft-exact, the design's rule): fill `o`'s
// enchant map starting from the slot's `eCost`. Candidates = the registered
// enchantments whose class matches `cls` AND whose eCost band contains the
// current cost (highest level taken). A weighted pick applies one enchant,
// its incompatible set is removed, and the loop CONTINUES when
// v % (eCost + 150) < 150 (i.e. with probability 150/(eCost+150)); on
// continue the cost halves (eCost = eCost/2) so the next enchant is weaker.
// Stops when no candidate remains or the continue roll fails.
void fillEnchants(EnchantOffer& o, Stream& s, int startCost, EnchantClass cls,
                  const EnchantmentRegistry& enchants) {
    if (cls == EnchantClass::None) {
        return; // a classless item is not enchantable (no candidates)
    }
    int cost = startCost;
    // The "already applied" set (for the incompatible removal).
    std::vector<std::string> applied;
    for (;;) {
        // Build the candidate list (registry order — deterministic). An
        // enchantment that is ALREADY applied is excluded (each enchantment
        // appears at most once in an offer — a re-pick at the decayed cost
        // would otherwise OVERWRITE the higher level with a lower one), as
        // is one incompatible with anything already applied.
        std::vector<const Enchantment*> cand;
        for (const Enchantment& e : enchants.all()) {
            bool excluded = false;
            for (const std::string& a : applied) {
                if (a == e.id || enchants.incompatiblePair(e.id, a)) {
                    excluded = true;
                    break;
                }
            }
            if (excluded) {
                continue;
            }
            bool classOk = false;
            for (EnchantClass c : e.classes) {
                if (c == cls) {
                    classOk = true;
                    break;
                }
            }
            if (!classOk) {
                continue;
            }
            if (levelForCost(e, cost) > 0) {
                cand.push_back(&e);
            }
        }
        if (cand.empty()) {
            break; // no candidate at this cost: the slot is done
        }
        const int pick = weightedPick(s, cand);
        const Enchantment& chosen = *cand[static_cast<std::size_t>(pick)];
        const int lvl = levelForCost(chosen, cost);
        o.enchants[chosen.id] = lvl;
        applied.push_back(chosen.id);
        // The continue roll: v % (cost + 150) < 150.
        const int v = static_cast<int>(
            s.next() % static_cast<std::uint64_t>(cost + 150));
        if (v < 150 && cost > 1) {
            cost = cost / 2; // decay: the next enchant is a lower level
        } else {
            break;
        }
    }
}

} // namespace

// -- vocabularies ------------------------------------------------------------

int enchantClassFromName(const std::string& name) {
    if (name == "sword") return static_cast<int>(EnchantClass::Sword);
    if (name == "axe") return static_cast<int>(EnchantClass::Axe);
    if (name == "pick") return static_cast<int>(EnchantClass::Pick);
    if (name == "shovel") return static_cast<int>(EnchantClass::Shovel);
    if (name == "armor") return static_cast<int>(EnchantClass::Armor);
    if (name == "bow") return static_cast<int>(EnchantClass::Bow);
    if (name == "hoe") return static_cast<int>(EnchantClass::Hoe);
    return -1;
}

const char* enchantClassName(EnchantClass c) {
    switch (c) {
        case EnchantClass::Sword: return "sword";
        case EnchantClass::Axe: return "axe";
        case EnchantClass::Pick: return "pick";
        case EnchantClass::Shovel: return "shovel";
        case EnchantClass::Armor: return "armor";
        case EnchantClass::Bow: return "bow";
        case EnchantClass::Hoe: return "hoe";
        default: return "";
    }
}

int effectFromName(const std::string& name) {
    if (name == "sharpness") return static_cast<int>(EffectType::Sharpness);
    if (name == "protection") return static_cast<int>(EffectType::Protection);
    if (name == "efficiency") return static_cast<int>(EffectType::Efficiency);
    if (name == "unbreaking") return static_cast<int>(EffectType::Unbreaking);
    if (name == "power") return static_cast<int>(EffectType::Power);
    if (name == "speed") return static_cast<int>(EffectType::Speed);
    if (name == "infinite") return static_cast<int>(EffectType::Infinite);
    return -1;
}

const char* effectName(EffectType t) {
    switch (t) {
        case EffectType::Sharpness: return "sharpness";
        case EffectType::Protection: return "protection";
        case EffectType::Efficiency: return "efficiency";
        case EffectType::Unbreaking: return "unbreaking";
        case EffectType::Power: return "power";
        case EffectType::Speed: return "speed";
        case EffectType::Infinite: return "infinite";
        default: return "";
    }
}

// -- the registry ------------------------------------------------------------

const Enchantment* EnchantmentRegistry::find(const std::string& id) const {
    const auto it = m_byId.find(id);
    return it == m_byId.end() ? nullptr : &m_list[it->second];
}

EffectType EnchantmentRegistry::effectFor(const std::string& id) const {
    const Enchantment* e = find(id);
    return e ? e->effect : EffectType::None;
}

bool EnchantmentRegistry::incompatiblePair(const std::string& a,
                                           const std::string& b) const {
    const Enchantment* ea = find(a);
    const Enchantment* eb = find(b);
    if (ea && std::find(ea->incompatible.begin(), ea->incompatible.end(), b) !=
                  ea->incompatible.end()) {
        return true;
    }
    if (eb && std::find(eb->incompatible.begin(), eb->incompatible.end(), a) !=
                  eb->incompatible.end()) {
        return true;
    }
    return false;
}

Result<void> EnchantmentRegistry::registerEnchantment(const Json& def) {
    if (!def.isObject()) {
        return MC_FAIL("enchantment definition must be a JSON object");
    }
    Enchantment e;
    const Json* idJson = def.find("id");
    if (idJson == nullptr || !idJson->isString() || idJson->asString().empty()) {
        return MC_FAIL("enchantment definition requires a non-empty 'id'");
    }
    e.id = idJson->asString();
    if (m_byId.count(e.id) != 0) {
        return MC_FAIL("duplicate enchantment id '" + e.id + "'");
    }
    const Json* nameJson = def.find("name");
    if (nameJson == nullptr || !nameJson->isString() ||
        nameJson->asString().empty()) {
        return MC_FAIL("enchantment '" + e.id +
                       "' requires a non-empty display 'name'");
    }
    e.name = nameJson->asString();
    // The effect (a code enum — the grants_effects precedent): the data
    // names it, the core reads it (never the content id).
    const Json* effJson = def.find("effect");
    if (effJson == nullptr || !effJson->isString()) {
        return MC_FAIL("enchantment '" + e.id +
                       "' requires a string 'effect'");
    }
    const int eff = effectFromName(effJson->asString());
    if (eff < 0) {
        return MC_FAIL("enchantment '" + e.id + "' has an unknown effect '" +
                       effJson->asString() +
                       "' (vocabulary: sharpness, protection, efficiency, "
                       "unbreaking, power, speed, infinite)");
    }
    e.effect = static_cast<EffectType>(eff);
    // The classes (a non-empty vocabulary list).
    const Json* clsJson = def.find("classes");
    if (clsJson == nullptr || !clsJson->isArray() || clsJson->size() == 0) {
        return MC_FAIL("enchantment '" + e.id +
                       "' requires a non-empty 'classes' array");
    }
    for (std::size_t i = 0; i < clsJson->size(); ++i) {
        const Json& c = clsJson->at(i);
        if (!c.isString()) {
            return MC_FAIL("enchantment '" + e.id +
                           "' classes[" + std::to_string(i) +
                           "] must be a string");
        }
        const int ci = enchantClassFromName(c.asString());
        if (ci < 0) {
            return MC_FAIL("enchantment '" + e.id + "' classes[" +
                           std::to_string(i) + "] is an unknown class '" +
                           c.asString() +
                           "' (vocabulary: sword, axe, pick, shovel, armor, "
                           "bow, hoe)");
        }
        e.classes.push_back(static_cast<EnchantClass>(ci));
    }
    // The weight (>= 1 — a zero/negative weight would break the pick).
    const Json* wJson = def.find("weight");
    if (wJson == nullptr || !wJson->isNumber() ||
        wJson->asInt() < 1) {
        return MC_FAIL("enchantment '" + e.id +
                       "' requires an integer 'weight' >= 1");
    }
    e.weight = wJson->asInt();
    // The eCost bands: a per-level [min, max] array, non-empty, ascending,
    // each band valid (min <= max), and contiguous-ascending (band i's max <
    // band i+1's min) so levelForCost's highest-band walk is well-defined.
    const Json* ecJson = def.find("ecost");
    if (ecJson == nullptr || !ecJson->isArray() || ecJson->size() == 0) {
        return MC_FAIL("enchantment '" + e.id +
                       "' requires a non-empty 'ecost' array of [min, max] "
                       "bands (one per level)");
    }
    for (std::size_t i = 0; i < ecJson->size(); ++i) {
        const Json& band = ecJson->at(i);
        if (!band.isArray() || band.size() != 2 || !band.at(0).isNumber() ||
            !band.at(1).isNumber()) {
            return MC_FAIL("enchantment '" + e.id + "' ecost[" +
                           std::to_string(i) + "] must be a [min, max] pair");
        }
        const int lo = band.at(0).asInt();
        const int hi = band.at(1).asInt();
        if (lo < 1 || lo > hi) {
            return MC_FAIL("enchantment '" + e.id + "' ecost[" +
                           std::to_string(i) + "] band [" +
                           std::to_string(lo) + ", " + std::to_string(hi) +
                           "] is invalid (min >= 1, min <= max)");
        }
        if (i > 0) {
            const int prevHi = e.eCost.back().second;
            if (lo <= prevHi) {
                return MC_FAIL("enchantment '" + e.id + "' ecost[" +
                               std::to_string(i) + "] band must start above "
                               "the previous band's max (" +
                               std::to_string(prevHi) + ")");
            }
        }
        e.eCost.push_back({lo, hi});
    }
    e.maxLevel = static_cast<int>(e.eCost.size());
    // The incompatible list (may be empty — the shipped seven have no pair).
    // Each named id must exist (a typo is rejected, the drops discipline).
    if (const Json* incJson = def.find("incompatible")) {
        if (!incJson->isArray()) {
            return MC_FAIL("enchantment '" + e.id +
                           "' 'incompatible' must be an array of ids");
        }
        for (std::size_t i = 0; i < incJson->size(); ++i) {
            const Json& c = incJson->at(i);
            if (!c.isString() || c.asString().empty()) {
                return MC_FAIL("enchantment '" + e.id +
                               "' incompatible[" + std::to_string(i) +
                               "] must be a non-empty id string");
            }
            // A named incompatible id must be a REGISTERED enchantment (the
            // drops discipline — a typo is rejected and named). The shipped
            // seven have no pair, so no forward reference occurs; a
            // reference to a not-yet-loaded id is rejected here (the
            // directory is name-sorted, and the shipped data is closed).
            if (m_byId.count(c.asString()) == 0) {
                return MC_FAIL("enchantment '" + e.id +
                               "' names an unknown incompatible "
                               "enchantment '" +
                               c.asString() + "'");
            }
            e.incompatible.push_back(c.asString());
        }
    }
    m_list.push_back(std::move(e));
    m_byId[m_list.back().id] = m_list.size() - 1;
    return {};
}

Result<void> EnchantmentRegistry::loadDirectory(AssetManager& assets,
                                                const std::string& relativeDir) {
    auto dir = assets.resolve(relativeDir);
    MC_TRY(dir);
    if (!std::filesystem::is_directory(*dir)) {
        MC_LOG_WARN("Enchantment directory not found: '" + (*dir).string() +
                    "' (no enchantments loaded)");
        return {};
    }
    std::vector<std::filesystem::path> files;
    for (const auto& entry : std::filesystem::directory_iterator(*dir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".json") {
            files.push_back(entry.path());
        }
    }
    std::sort(files.begin(), files.end());
    for (const auto& file : files) {
        const auto rel =
            std::filesystem::relative(file, assets.root()).generic_string();
        auto text = assets.loadText(rel);
        MC_TRY(text);
        auto parsed = Json::parse(*text);
        if (!parsed.ok()) {
            return MC_FAIL("invalid JSON in enchantment file '" + rel +
                           "': " + parsed.error().message);
        }
        MC_TRY(registerEnchantment(*parsed));
    }
    return {};
}

// -- the pure family ---------------------------------------------------------

int bookshelfCount(const World& world, const BlockRegistry& blocks,
                   const BlockPos& tablePos) {
    // The 5x5 ring: Chebyshev distance EXACTLY 2 (the 16 outer cells) x two
    // layers (the table's y and y+1). Count the `enchant_bookshelf` blocks,
    // capped at kBookshelfCap (15). Deterministic over the fixed offset order.
    int count = 0;
    for (int dy = 0; dy < 2 && count < kBookshelfCap; ++dy) {
        for (int dz = -2; dz <= 2 && count < kBookshelfCap; ++dz) {
            for (int dx = -2; dx <= 2 && count < kBookshelfCap; ++dx) {
                // The ring: max(|dx|, |dz|) == 2 (the outer border of the
                // 5x5, excluding the inner 3x3).
                if (std::max(std::abs(dx), std::abs(dz)) != 2) {
                    continue;
                }
                const BlockID id =
                    world.getBlock(tablePos.x + dx, tablePos.y + dy,
                                   tablePos.z + dz);
                if (id.valid() && id != BlockID::air() &&
                    blocks.at(id).properties.enchantBookshelf) {
                    ++count;
                }
            }
        }
    }
    return std::min(count, kBookshelfCap);
}

EnchantOffers generateOffers(std::uint64_t worldSeed, const BlockPos& tablePos,
                             EnchantClass cls, int enchantability,
                             std::uint64_t xpSeed, int B,
                             const EnchantmentRegistry& enchants) {
    EnchantOffers out;
    // The seed (the design's formula): worldSeed ^ hash3(table, salt) ^
    // enchantability ^ xpSeed, finalized by splitmix64 (PITS #4).
    const std::uint64_t tableHash = hash3(
        static_cast<std::uint64_t>(static_cast<std::int64_t>(tablePos.x)),
        static_cast<std::uint64_t>(static_cast<std::int64_t>(tablePos.y)),
        static_cast<std::uint64_t>(static_cast<std::int64_t>(tablePos.z)),
        kEnchantTableSalt);
    Stream s(splitmix64(worldSeed ^ tableHash ^
                        static_cast<std::uint64_t>(
                            static_cast<std::uint64_t>(enchantability)) ^
                        xpSeed));
    EnchantOffer* rows[kOfferRows] = {&out.top, &out.mid, &out.bottom};
    const int b = std::max(0, B);
    for (int row = 0; row < kOfferRows; ++row) {
        EnchantOffer* o = rows[static_cast<std::size_t>(row)];
        // xpBase = 1 + randInt(7) + floor(B/2) + randInt(B)
        const int xpBase = 1 + s.randInt(7) + b / 2 + s.randInt(b);
        o->xpCost = slotXpCost(row, xpBase, b);
        o->lapisCost = 1 + s.randInt(2); // 1..3 (the uniform deviation)
        const int eCost = slotECost(s, o->xpCost);
        fillEnchants(*o, s, eCost, cls, enchants);
    }
    return out;
}

int applyEnchantXpCharge(int pool, int xpCost) {
    if (xpCost <= 0) {
        return pool;
    }
    // Include the xp_curve via a forward declaration-free path: the curve
    // functions are header-only (core/xp_curve.hpp). We compute the level
    // and totals here to avoid an extra include cycle (xp_curve.hpp is a
    // leaf header — include it directly).
    const int L0 = levelFromTotalXp(pool);
    const int L1 = std::max(0, L0 - xpCost);
    return totalXpForLevel(L1) + (pool - totalXpForLevel(L0));
}

std::uint64_t xpSeedStep(std::uint64_t xpSeed) {
    // PITS #4: one golden step then finalize (the advancing form).
    return splitmix64(xpSeed + kGolden);
}

std::uint64_t xpSeedInit(std::uint64_t worldSeed) {
    return worldSeed ^ kXpSeedInit;
}

float unbreakingWear(int level, bool isArmor, float amount,
                     std::uint64_t state) {
    if (level <= 0 || amount <= 0.0f) {
        return amount; // no unbreaking: the full wear applies (bit-identical
                       // to the pre-V2-12 path for every non-enchant item)
    }
    // The per-point skip probability (Minecraft 1.21 remove_binomial):
    //   tool:  p = level / (level + 1)
    //   armor: p = 2*level / (5*(level+1))
    // Integer dice: skip a point when (splitmix64(advance(state)) % denom)
    // < numer. The event's wear is converted to integer points (ceil — the
    // tool events are already integral, the float armor wear rounds up).
    const int points = static_cast<int>(std::ceil(amount - 1e-6f));
    if (points <= 0) {
        return 0.0f;
    }
    const int numer = isArmor ? 2 * level : level;
    const int denom = isArmor ? 5 * (level + 1) : (level + 1);
    if (numer <= 0 || denom <= numer) {
        // denom <= numer would be p >= 1 (always skip) — unreachable for the
        // shipped formulas (numer < denom always), but guard for clarity.
        return 0.0f;
    }
    int applied = 0;
    std::uint64_t st = state;
    for (int i = 0; i < points; ++i) {
        st += kGolden; // PITS #4 advance
        const std::uint64_t v = splitmix64(st);
        if (static_cast<int>(v % static_cast<std::uint64_t>(denom)) < numer) {
            // skipped: no wear this point
        } else {
            ++applied;
        }
    }
    return static_cast<float>(applied);
}

std::uint64_t unbreakingState(std::uint64_t worldSeed, float damageBefore,
                              int itemRegistryId) {
    // The float's bit pattern is the stable key (a bijection over floats —
    // the same damageBefore always gives the same bits, no rounding).
    std::uint32_t bits = 0;
    std::memcpy(&bits, &damageBefore, sizeof(bits));
    const std::uint64_t dmgKey =
        static_cast<std::uint64_t>(bits) * kGolden;
    return worldSeed ^ splitmix64(dmgKey) ^ kUnbreakingSalt ^
           static_cast<std::uint64_t>(
               static_cast<std::uint32_t>(itemRegistryId));
}

bool offerApplicable(const ItemStack& itemSlot, int itemEnchantability,
                     int poolLevel, int lapisCount, const EnchantOffer& o) {
    // The four gates (the design's LMB-offer rule): the item is non-empty AND
    // has an enchant class (enchantability > 0 is the data proxy for
    // "enchantable" — a classless item has enchantability 0) AND is not yet
    // enchanted AND the pool's level >= xpCost AND the inventory holds >=
    // lapisCost of the material.
    if (itemSlot.empty()) {
        return false;
    }
    if (itemEnchantability <= 0) {
        return false; // no class / not enchantable
    }
    if (!itemSlot.enchants.empty()) {
        return false; // already enchanted (the item is consumed by one apply)
    }
    if (poolLevel < o.xpCost) {
        return false;
    }
    if (lapisCount < o.lapisCost) {
        return false;
    }
    return true;
}

// -- the item-side pure accessors -------------------------------------------

int effectLevel(const ItemStack& stack, const EnchantmentRegistry& reg,
                EffectType t) {
    if (stack.empty()) {
        return 0;
    }
    int sum = 0;
    for (const auto& [id, lvl] : stack.enchants) {
        if (reg.effectFor(id) == t) {
            sum += lvl;
        }
    }
    return sum;
}

int unbreakingLevel(const ItemStack& stack, const EnchantmentRegistry& reg) {
    return effectLevel(stack, reg, EffectType::Unbreaking);
}
int sharpnessLevel(const ItemStack& stack, const EnchantmentRegistry& reg) {
    return effectLevel(stack, reg, EffectType::Sharpness);
}
int protectionLevel(const ItemStack& stack, const EnchantmentRegistry& reg) {
    return effectLevel(stack, reg, EffectType::Protection);
}
int efficiencyLevel(const ItemStack& stack, const EnchantmentRegistry& reg) {
    return effectLevel(stack, reg, EffectType::Efficiency);
}
int powerLevel(const ItemStack& stack, const EnchantmentRegistry& reg) {
    return effectLevel(stack, reg, EffectType::Power);
}
int speedLevel(const ItemStack& stack, const EnchantmentRegistry& reg) {
    return effectLevel(stack, reg, EffectType::Speed);
}
bool heldInfinite(const ItemStack& stack, const EnchantmentRegistry& reg) {
    return effectLevel(stack, reg, EffectType::Infinite) > 0;
}

} // namespace mc
