#pragma once
#include <cmath>
namespace conker::android {
struct MobileResolution {
    double scale=2.0;
    unsigned heavyWindows=0,lightWindows=0;
    // Five-second windows. Never reduce image quality for a CPU-only bottleneck.
    bool sample(double fps,double gpuMs,unsigned samples,double seconds) noexcept {
        if(samples<30||seconds<4||seconds>8||!std::isfinite(fps)||!std::isfinite(gpuMs)||gpuMs<=0){heavyWindows=lightWindows=0;return false;}
        if(fps<57&&gpuMs>13.5){lightWindows=0;if(++heavyWindows>=2&&scale>1.5){scale-=.25;heavyWindows=0;return true;}}
        else if(fps>=58.5&&gpuMs<8){heavyWindows=0;if(++lightWindows>=4&&scale<2){scale+=.25;lightWindows=0;return true;}}
        else heavyWindows=lightWindows=0;
        return false;
    }
};
}
