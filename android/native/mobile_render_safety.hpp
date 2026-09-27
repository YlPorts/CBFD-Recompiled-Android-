#pragma once
#include <cmath>
#include <cstdint>

namespace conker::mobile {
// CPU framebuffer bookkeeping is not homogeneous clipping. If a vertex crosses
// w=0, perspective division can reverse screen winding and produce unbounded
// coordinates. Keep conservative scissor bounds; let the GPU clip/cull the tri.
inline bool unsafe_screen_bounds(float x, float y, float w) {
    constexpr float limit = 268435456.0f; // headroom for ceil(), 10.2 coordinates
    return !std::isfinite(x) || !std::isfinite(y) || !std::isfinite(w)
        || w <= 0.0001f || std::abs(x) >= limit || std::abs(y) >= limit;
}
inline int64_t bulk_sleep_ns(int64_t remaining) {
    return remaining > 2000000 ? remaining - 1000000 : 0;
}
}
