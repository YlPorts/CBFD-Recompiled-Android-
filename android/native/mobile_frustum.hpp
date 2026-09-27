#pragma once
#include <cmath>

namespace conker::mobile {
// A camera-space plane nx*x + nz*z = 0 widens with x' = x / scale.
// Normalize again: the original culler compares signed distance to object radii.
inline bool widen_horizontal_plane(float& nx, float& nz, float scale) {
    if (!std::isfinite(scale) || scale <= 1.0f || !std::isfinite(nx) || !std::isfinite(nz)) return false;
    const float x = nx / scale;
    const float length = std::hypot(x, nz);
    if (!std::isfinite(length) || length < 1e-6f) return false;
    nx = x / length;
    nz /= length;
    return true;
}
}
