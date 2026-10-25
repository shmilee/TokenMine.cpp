// CRAFT-3: the per-block sound material — the data-driven mapping from a
// block to its dig / place / break timbre + the init cross-check.
//
// A block's optional `sound_material` property (see BlockProperties)
// names a material; the material's vocabulary is the wav file prefixes on
// disk (assets/sounds/): a material exists iff its three event files
// <material>_dig.wav / <material>_place.wav / <material>_break.wav are
// all present (checked at game init — the grows_to discipline: a typo
// fails loudly, naming the missing piece). A block WITHOUT the field
// keeps the generic event names (dig / place / break) — the event name is
// bit-level invariant (the structural-immunity red line: field-less
// content plays exactly the pre-CRAFT-3 samples).
//
// The event name selection (blockSoundName) and the cross-check are pure
// over the registry + assets (unit-tested without SDL / Game — the
// campfire / durability paradigm).
#pragma once

#include <string>
#include <string_view>

#include "block/block.hpp"
#include "block/block_registry.hpp"
#include "core/asset_manager.hpp"
#include "core/error.hpp"

namespace mc {

// The three world-interaction events a block's material names (the dig
// tick, the placement / state rotation, the break completion). The
// generic (no-material) sound names are these bare strings.
inline constexpr std::string_view kSoundMaterialEvents[3] = {
    "dig", "place", "break"};

// The sound name for a block interaction event ("dig", "place" or
// "break"): "<material>_<event>" (e.g. "stone_break") for a block with a
// sound_material, the bare event (the generic name) otherwise. Pure —
// unit-tested (the bit-level-invariant case is the empty-material one).
inline std::string blockSoundName(const Block& block, std::string_view event) {
    if (block.properties.soundMaterial.empty()) {
        return std::string(event);
    }
    return std::string(block.properties.soundMaterial) + "_" +
           std::string(event);
}

// The init cross-check: every block with a sound_material must name a
// material whose three event wavs exist under `soundDir` (the grows_to
// discipline — a missing piece fails init loudly, naming it). File-based
// on purpose (NOT the loaded SoundBank): the check must hold when the
// audio is disabled (the headless degradation path keeps init strict).
Result<void> checkSoundMaterials(
    const BlockRegistry& blocks, const AssetManager& assets,
    std::string_view soundDir = "sounds");

} // namespace mc
