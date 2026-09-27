#pragma once
#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <android/native_window.h>

namespace conker::android {
// Owned references, not SDL's mutable/raw window pointer. Only JNI publishes;
// Vulkan retains its own lease until its old swapchain/surface are destroyed.
struct SurfaceLease {
    ANativeWindow* window = nullptr;
    uint64_t generation = 0;
    uint32_t width = 0, height = 0;
};
class SurfaceRegistry {
    mutable std::mutex mutex;
    ANativeWindow* window = nullptr;
    uint64_t generation = 0;
    uint32_t width = 0, height = 0;
    bool foreground = true;
public:
    ~SurfaceRegistry() { if (window) ANativeWindow_release(window); }
    void publish(ANativeWindow* ownedWindow, uint32_t w, uint32_t h) {
        std::lock_guard<std::mutex> lock(mutex);
        if (window != ownedWindow || w != width || h != height) ++generation;
        if (window) ANativeWindow_release(window);
        window = ownedWindow; width = w; height = h;
    }
    void setForeground(bool value) {
        std::lock_guard<std::mutex> lock(mutex); foreground = value;
    }
    SurfaceLease acquire() const {
        std::lock_guard<std::mutex> lock(mutex);
        SurfaceLease lease{nullptr,generation,width,height};
        if (foreground && window && width && height) {
            ANativeWindow_acquire(window); lease.window=window;
        }
        return lease;
    }
    bool usable() const {
        std::lock_guard<std::mutex> lock(mutex);
        return foreground && window && width && height;
    }
    uint64_t epoch() const { std::lock_guard<std::mutex> lock(mutex); return generation; }
};
inline SurfaceRegistry surfaceRegistry;
inline std::atomic<int64_t> surfaceRetryAfter{0};
inline std::atomic<uint64_t> surfaceRecoveries{0}, surfaceLosses{0};
inline int64_t surfaceNowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
inline bool surfaceCanRetry() { return surfaceRegistry.usable() && surfaceNowMs()>=surfaceRetryAfter.load(); }
inline void surfaceBackoff() { surfaceRetryAfter.store(surfaceNowMs()+250); }
}
