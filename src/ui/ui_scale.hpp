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

// UX-8e (ui.font_scale): the TEXT-ONLY zoom. Unlike ui.scale (a GPU
// drawable stretch that upsamples every UI element — font, hotbar,
// panels, icons together, with the panel fit scale shrinking the row
// text back), font_scale multiplies ONLY the text scale parameters in
// Game::renderUi; the icons, slot rects, panel geometry and the HUD row
// pitch stay put. 1.0 (kFontScaleDefault) is the bit-exact baseline —
// s * 1.0 is IEEE-exact, so the default run is structurally immune.
// A present key must be a JSON number in (0, kFontScaleMax]: zero or
// negative would invert/clamp the glyphs, and beyond 4.0 the text stops
// fitting the panel geometry it decorates (the fit-scale range ceiling).
constexpr double kFontScaleDefault = 1.0;
constexpr double kFontScaleMax = 4.0;

Result<double> uiFontScaleFromConfig(const Config& config);

} // namespace mc
