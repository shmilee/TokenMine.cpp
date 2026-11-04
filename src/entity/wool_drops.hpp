// V2-15 (sheep): the wool_drops init cross-check — the campfire /
// checkRepairMaterials pattern: the decision lives here (pure over the
// registries + the atlas layout, unit-tested headless), and the game's
// init calls it once after the other mob cross-checks (a thin adapter).
//
// The wool_drops feature is data-flag gated end to end (the core never
// switches on a species): a mob carrying MobProperties::woolDrops is
// only legal when its DATA and the ASSETS are complete:
//   1. the drop list carries the wool entry (the item named for the
//      wool dye family — kWoolDyeFamily): the death pass recolors it
//      per instance and skips it when bare, and shearing rolls its
//      count range — no entry = a dead flag (the ammo discipline);
//   2. every wool_weights color is a DYE ITEM's color (the data-defined
//      15-color vocabulary — the block dye family's cross-check twin:
//      a color no dye item names would spawn a wool the dyes cannot
//      produce / the family's tiles do not cover);
//   3. the atlas carries the bare tile (kWoolBareTile) AND every dye
//      color's wool variant tile (dyeVariantName(kWoolDyeFamily, c)
//      for EVERY dye item's color c — a mob can be dyed with any dye,
//      so the render's 15-color variant set must exist in full, not
//      just the spawn weights' subset).
// The FIRST failing mob (registry order) is named, like every other
// init cross-check.
#pragma once

#include <optional>
#include <string>

#include "block/block.hpp"
#include "core/error.hpp"
#include "entity/mob_registry.hpp"
#include "item/item_registry.hpp"
#include "renderer/texture_atlas.hpp"

namespace mc {

// The init cross-check (pure over the registries + the atlas layout —
// the campfire / checkRepairMaterials pattern): a wool_drops mob's data
// must be complete (see the file head for the three rules). The FIRST
// failing mob (registry order) is named, like every other init
// cross-check.
Result<void> checkWoolDrops(const MobRegistry& mobs, const ItemRegistry& items,
                            const AtlasLayout& layout);

// The death pass's WOOL entry resolution (pure — the game's despawn
// pass adapter reads this, the armor / bed pattern): a wool_drops mob's
// "wool" drop entry follows the INSTANCE state (the Minecraft rule):
// the BARE death (a sheared mob) drops no wool at all -> nullopt (the
// entry is skipped BEFORE its count roll — no stream draw for a drop
// that cannot happen); the woolly death drops the instance-COLOR
// variant (dyeVariantName over the family base). Every other entry, and
// every non-wool mob's entries, resolve to their data name unchanged
// (the pre-V2-15 bit-identical root).
std::optional<std::string> woolDeathDropName(const MobProperties& props,
                                             const MobDrop& drop, bool bare,
                                             const std::string& woolColor);

} // namespace mc
