#include "renderer/fog.hpp"

#include <algorithm>

namespace mc {

FogRange fogDistanceRange(float streamRadiusBlocks, float farPlane) {
    FogRange r;
    r.end = std::min(farPlane, streamRadiusBlocks);
    r.start = kFogStartScale * r.end;
    return r;
}

float fillClampedEnd(float staticEnd, float filledRadiusBlocks) {
    // std::min returns the first argument bit-for-bit when it is the
    // smaller (or equal) — a full disk (filledRadiusBlocks >= staticEnd)
    // is the pre-V3-6 end, the steady-state red line.
    return std::min(staticEnd, filledRadiusBlocks);
}

FogRange fogFilledRange(float streamRadiusBlocks, float filledRadiusBlocks,
                        float farPlane) {
    FogRange r = fogDistanceRange(streamRadiusBlocks, farPlane);
    r.end = fillClampedEnd(r.end, filledRadiusBlocks);
    r.start = kFogStartScale * r.end;
    return r;
}

} // namespace mc
