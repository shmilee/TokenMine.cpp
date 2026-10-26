// PRE-5 alpha cutout: the pure image-side predicate behind the cutout
// routing.
//
// An RGBA image is a PURE CUTOUT iff every pixel's alpha channel is
// exactly 0 or 255 — its transparent pixels are HOLES (see-through), not
// translucency. Such geometry draws in the cutout render pass (alpha
// discard active + depth write on): the holes write no depth, so the
// geometry behind them (item drops, torches, other cutout geometry) is
// visible through them.
//
// An image with ANY intermediate alpha value (a translucent core: glass
// 220, water 200, the glass pane 90) belongs to the translucent-BLEND
// pass instead (no depth write, blended): its holes still discard (the
// fragment's discard is alpha-based, not pass-based), but its surface
// composites over what is behind it.
//
// The predicate is data, not code policy: the game runs it over each
// item's icon PNG at init (the per-item census) and the block content
// pins it per block in the JSON `cutout` property (the same census on
// the block textures, assigned at content authoring).
#pragma once

#include <cstddef>
#include <cstdint>

namespace mc {

// True iff EVERY pixel of the RGBA buffer (width*height pixels, 4 bytes
// each, the PngImage row order) has alpha 0 or 255. An empty image
// (width or height <= 0) is not a pure cutout (false — there is no
// texture data to route).
bool isPureCutoutImage(const std::uint8_t* rgba, int width, int height);

} // namespace mc
