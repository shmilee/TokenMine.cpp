#include "player/drown.hpp"

#include <cmath>

namespace mc {

float drownStep(DrownState& state, bool headInFluid, unsigned frame) {
    if (!headInFluid) {
        // Breathe (Minecraft: the surface refills the air instantly).
        state.oxygen = kOxygenMax;
        state.drownTicks = 0;
        return 0.0f;
    }
    if (state.oxygen > 0) {
        // The air decays one unit per kOxygenDecayFrames frames on the
        // GLOBAL phase (frame % 6 — the pass hands the absolute frame,
        // so the rhythm is frame-locked, not per-player).
        if (frame % static_cast<unsigned>(kOxygenDecayFrames) == 0u) {
            --state.oxygen;
        }
        return 0.0f;
    }
    // Exhausted: the drown accumulator runs, and at a full
    // kDrownDamageFrames it resets and deals the damage (the adapter
    // feeds it into Player::damage with the armor bypass).
    ++state.drownTicks;
    if (state.drownTicks == kDrownDamageFrames) {
        state.drownTicks = 0;
        return kDrownDamage;
    }
    return 0.0f;
}

bool headInFluid(const Player& player, const World& world) {
    // The single eye cell (Minecraft's rule — the eyes, not the feet):
    // the position's eye height is the crouch-reduced value
    // (Player::eyePosition uses the same formula as the camera).
    const Vec3 eye = player.eyePosition();
    const int x = static_cast<int>(std::floorf(eye.x));
    const int y = static_cast<int>(std::floorf(eye.y));
    const int z = static_cast<int>(std::floorf(eye.z));
    // Any fluid kind drowns (water OR lava); an out-of-range / uncreated
    // cell reads as air (the World convention).
    return world.getFluidKind(x, y, z).has_value();
}

} // namespace mc
