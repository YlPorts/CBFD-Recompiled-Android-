#pragma once
#include <algorithm>
#include <cstdint>

namespace conker::mobile {
// Fixed vertical pixel budget, independent of VI mode and display refresh rate.
// With Expand a 2340x1080 Surface keeps its full width; this is not a 2x upscale.
constexpr uint32_t internalHeight = 1080;
inline float resolution_scale(uint32_t viHeight) {
    const uint32_t reference = viHeight ? std::max(viHeight, 60U) : 240U;
    return std::max(float(internalHeight) / float(reference), 1.0f);
}

// RGB and coverage write disjoint channels. A depth write can affect a later
// primitive only when that primitive reads depth (including shader decals).
// Keep per-primitive replay in those cases, including equal-depth overlaps.
inline uint32_t coverage_batch(uint32_t faces, bool writesDepth, bool readsDepth) {
    return writesDepth && readsDepth ? 1U : std::max(faces, 1U);
}
}
