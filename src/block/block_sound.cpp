#include "block/block_sound.hpp"

namespace mc {

Result<void> checkSoundMaterials(const BlockRegistry& blocks,
                                 const AssetManager& assets,
                                 std::string_view soundDir) {
    for (const BlockID id : blocks.allIds()) {
        const Block& b = blocks.at(id);
        if (b.properties.soundMaterial.empty()) {
            continue;
        }
        for (const std::string_view ev : kSoundMaterialEvents) {
            const std::string rel = std::string(soundDir) + "/" +
                                    b.properties.soundMaterial + "_" +
                                    std::string(ev) + ".wav";
            if (!assets.exists(rel)) {
                return MC_FAIL("block '" + b.name + "' sound_material '" +
                               b.properties.soundMaterial + "' is missing '" +
                               rel + "'");
            }
        }
    }
    return {};
}

} // namespace mc
