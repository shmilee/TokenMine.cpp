#include "entity/wool_drops.hpp"

#include <vector>

namespace mc {

namespace {

// The dye items' colors (the data-defined 15-color vocabulary — the
// block dye family's cross-check builds the same list): empty when the
// registry holds no dye item.
std::vector<std::string> dyeItemColors(const ItemRegistry& items) {
    std::vector<std::string> colors;
    for (const ItemID id : items.allIds()) {
        if (!items.at(id).dyeColor.empty()) {
            colors.push_back(items.at(id).dyeColor);
        }
    }
    return colors;
}

} // namespace

Result<void> checkWoolDrops(const MobRegistry& mobs, const ItemRegistry& items,
                            const AtlasLayout& layout) {
    const std::vector<std::string> colors = dyeItemColors(items);
    for (const MobID id : mobs.allIds()) {
        const Mob& mob = mobs.at(id);
        const MobProperties& p = mob.properties;
        if (!p.woolDrops) {
            continue; // every pre-V2-15 mob: structurally inert
        }
        // 1. The wool drop entry (the death pass's recolor / skip target
        // and shearing's count range) must exist — a wool_drops flag
        // without a "wool" entry is dead data (the ammo discipline).
        if (woolDropEntry(&p) == nullptr) {
            return MC_FAIL("mob '" + mob.name + "' has wool_drops but no "
                           "'wool' drop entry (the colored wool's "
                           "death / shear drop needs the family's base "
                           "entry)");
        }
        // 2. The spawn roll's vocabulary: every wool_weights color is a
        // dye item's color (data-defined — a color no dye names would
        // spawn wool the dye family cannot cover).
        for (std::size_t i = 0; i < p.woolWeights.size(); ++i) {
            const std::string& c = p.woolWeights[i].color;
            bool known = false;
            for (const std::string& dyeColor : colors) {
                if (dyeColor == c) {
                    known = true;
                    break;
                }
            }
            if (!known) {
                return MC_FAIL("mob '" + mob.name + "' wool_weights[" +
                               std::to_string(i) + "] color '" + c +
                               "' is not a dye item's color (the "
                               "vocabulary is the dye data)");
            }
        }
        // 3. The atlas: the bare tile (the sheared body) + EVERY dye
        // color's wool variant (the mob can be dyed with any dye, so
        // the render's 15-color set must exist in full — not just the
        // spawn weights' subset).
        if (!layout.contains(std::string(kWoolBareTile))) {
            return MC_FAIL("mob '" + mob.name + "' has wool_drops but the "
                           "atlas is missing the bare tile '" +
                           std::string(kWoolBareTile) + "'");
        }
        for (const std::string& c : colors) {
            const std::string variant =
                dyeVariantName(kWoolDyeFamily, c);
            if (!layout.contains(variant)) {
                return MC_FAIL("mob '" + mob.name +
                               "' has wool_drops but the atlas is missing "
                               "the wool variant tile '" + variant +
                               "' (dye color '" + c + "')");
            }
        }
    }
    return {};
}

std::optional<std::string> woolDeathDropName(const MobProperties& props,
                                             const MobDrop& drop, bool bare,
                                             const std::string& woolColor) {
    // Only the wool_drops mob's family entry is instance-stateful;
    // everything else is the data name (the pre-V2-15 bit).
    if (drop.item != std::string(kWoolDyeFamily) || !props.woolDrops) {
        return drop.item;
    }
    if (bare) {
        return std::nullopt;  // the sheared death: no wool (Minecraft)
    }
    return dyeVariantName(kWoolDyeFamily, woolColor);
}

} // namespace mc
