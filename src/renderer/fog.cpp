#include "renderer/fog.hpp"

#include <algorithm>

namespace mc {

FogRange fogDistanceRange(float streamRadiusBlocks, float farPlane) {
    FogRange r;
    r.end = std::min(farPlane, streamRadiusBlocks);
    r.start = kFogStartScale * r.end;
    return r;
}

} // namespace mc
