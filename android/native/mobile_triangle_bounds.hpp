#pragma once
#include <cmath>
namespace conker::android {
// Bounds metadata only: actual depth tests and triangle clipping remain on the GPU.
inline bool conservative_triangle_bounds(float wa,float wb,float wc,float ax,float ay,float bx,float by,float cx,float cy) noexcept {
    if (!std::isfinite(wa)||!std::isfinite(wb)||!std::isfinite(wc)||wa<=1e-5f||wb<=1e-5f||wc<=1e-5f) return true;
    return !std::isfinite(ax)||!std::isfinite(ay)||!std::isfinite(bx)||!std::isfinite(by)||!std::isfinite(cx)||!std::isfinite(cy)||
        std::fabs(ax)>=1048576.f||std::fabs(ay)>=1048576.f||std::fabs(bx)>=1048576.f||std::fabs(by)>=1048576.f||std::fabs(cx)>=1048576.f||std::fabs(cy)>=1048576.f;
}
}
