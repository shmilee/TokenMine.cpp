#pragma once

#include "core/config.hpp"
#include "core/error.hpp"

namespace mc {

// Screen-space UI zoom for high-resolution displays (Phase 34). The UI is
// laid out in a "UI pixel" space = logical window size / ui.scale, which
// the ortho projection stretches onto the drawable (the GPU upsamples;
// NEAREST textures stay crisp). 1.0 is the pixel-exact baseline; larger
// values make the font, hotbar, panels and item icons bigger. Below 1.0
// the UI would shrink below its designed size; above 4.0 the UI space
// drops under the smallest sensible window size (320 px) and the panels
// clip (the existing narrow-window behavior).
constexpr float kUiScaleMin = 1.0f;
constexpr float kUiScaleMax = 4.0f;

// Reads "ui.scale": a missing key = 1.0 (the baseline); a present key
// must be a JSON number in [kUiScaleMin, kUiScaleMax]. The Config getters
// silently fall back to the default on a type mismatch, so this reads the
// raw value (the Player::Params::fromConfig pattern).
Result<float> uiScaleFromConfig(const Config& config);

} // namespace mc
