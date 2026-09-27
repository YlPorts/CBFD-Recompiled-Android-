#pragma once
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cmath>
namespace conker::android {
struct MobileMetrics {
    std::atomic<uint64_t> presented{0},rendered{0},gpuSamples{0},gpuUs{0},renderUs{0},slowPresent{0};
    std::atomic<uint64_t> displayLists{0},displayListUs{0},vi{0},conservativeTriangles{0};
    void renderedFrame(double gpuMs,double cpuMs) noexcept {
        rendered.fetch_add(1,std::memory_order_relaxed);
        if(std::isfinite(gpuMs)&&gpuMs>0&&gpuMs<2000){gpuUs.fetch_add(uint64_t(gpuMs*1000),std::memory_order_relaxed);gpuSamples.fetch_add(1,std::memory_order_relaxed);}
        if(std::isfinite(cpuMs)&&cpuMs>=0&&cpuMs<10000)renderUs.fetch_add(uint64_t(cpuMs*1000),std::memory_order_relaxed);
    }
    // Called only by RT64's presentation thread, after a successful present.
    void presentedFrame() noexcept {
        using Clock=std::chrono::steady_clock;
        static Clock::time_point last{};
        auto now=Clock::now();
        if(last!=Clock::time_point{}&&std::chrono::duration<double,std::milli>(now-last).count()>25)slowPresent.fetch_add(1,std::memory_order_relaxed);
        last=now;presented.fetch_add(1,std::memory_order_relaxed);
    }
};
inline MobileMetrics mobile_metrics;
}
