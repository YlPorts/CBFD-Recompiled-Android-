#pragma once
#include <atomic>
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
namespace conker::camera {
// UI and SDL only publish float axes. Only the N64 game thread edits its camera.
inline std::atomic<uint64_t> touchInput{0}, padInput{0};
inline std::atomic<uint64_t> hookCalls{0},allowedCalls{0},updatedCalls{0},blockedCalls{0};
struct Input { float x,y; };
inline uint64_t pack(float x,float y) {
    x=std::isfinite(x)?std::clamp(x,-1.f,1.f):0.f;
    y=std::isfinite(y)?std::clamp(y,-1.f,1.f):0.f;
    return uint64_t(std::bit_cast<uint32_t>(x)) | (uint64_t(std::bit_cast<uint32_t>(y))<<32);
}
inline Input unpack(uint64_t bits) {
    return {std::bit_cast<float>(uint32_t(bits)),std::bit_cast<float>(uint32_t(bits>>32))};
}
inline Input input() {
    auto t=unpack(touchInput.load(std::memory_order_relaxed));
    return std::abs(t.x)+std::abs(t.y)>.001f?t:unpack(padInput.load(std::memory_order_relaxed));
}
inline void release() { touchInput.store(0);padInput.store(0); }
struct Counters { uint64_t hooks,allowed,updates,blocked; };
inline Counters counters() { return {hookCalls.load(),allowedCalls.load(),updatedCalls.load(),blockedCalls.load()}; }
struct Vec3 { float x,y,z; };
inline bool orbit(Vec3 center,Vec3& eye,Input axes,float dt) {
    if(!std::isfinite(dt)||dt<=0||dt>.25f||!std::isfinite(axes.x)||!std::isfinite(axes.y)) return false;
    if(std::abs(axes.x)+std::abs(axes.y)<.001f) return false;
    double x=double(eye.x)-center.x,y=double(eye.y)-center.y,z=double(eye.z)-center.z;
    const double distance=std::sqrt(x*x+y*y+z*z);
    if(!std::isfinite(distance)||distance<1||distance>10000) return false;
    constexpr double pi=3.14159265358979323846;
    double yaw=std::atan2(x,z)-std::clamp(axes.x,-1.f,1.f)*(110*pi/180)*dt;
    double pitch=std::clamp(std::asin(std::clamp(y/distance,-1.,1.))+
        std::clamp(axes.y,-1.f,1.f)*(75*pi/180)*dt,-65*pi/180,80*pi/180);
    const double horizontal=std::cos(pitch)*distance;
    const Vec3 result{float(center.x+std::sin(yaw)*horizontal),float(center.y+std::sin(pitch)*distance),
        float(center.z+std::cos(yaw)*horizontal)};
    if(!std::isfinite(result.x)||!std::isfinite(result.y)||!std::isfinite(result.z)) return false;
    eye=result;return true;
}
}
