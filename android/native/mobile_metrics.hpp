#pragma once
#include <atomic>
#include <cmath>
#include <cstdint>

namespace conker::mobile {
// Each producer only publishes counters; the graphics thread consumes snapshots.
// Submission FPS is NOT display scan-out FPS. No per-frame file I/O or UI polling.
struct Metrics {
    std::atomic<uint32_t> verticalScaleMilli{4500};
    std::atomic<uint64_t> viSize{0}, surfaceSize{0}, presentedSize{0};
    std::atomic<uint64_t> singleSourceDraws{0}, coveragePasses{0}, depthOrderedTriangles{0};
    std::atomic<uint64_t> presents{0}, renders{0}, gpuSamples{0}, gpuUs{0}, edgeClears{0};
    std::atomic<uint64_t> matchPairs{0},matchPairsAvoided{0},meshRejected{0},boundsBehind{0},boundsClipped{0};
    std::atomic<uint64_t> renderUs{0}, matches{0}, matchUs{0}, boundsFallbacks{0};
};
inline Metrics metrics;
inline thread_local uint64_t localBoundsBehind=0,localBoundsClipped=0,localBoundsFallback=0;
inline void publish_bounds() {
    metrics.boundsBehind.fetch_add(localBoundsBehind,std::memory_order_relaxed);
    metrics.boundsClipped.fetch_add(localBoundsClipped,std::memory_order_relaxed);
    metrics.boundsFallbacks.fetch_add(localBoundsFallback,std::memory_order_relaxed);
    localBoundsBehind=localBoundsClipped=localBoundsFallback=0;
}
inline uint64_t microseconds(double milliseconds) {
    return std::isfinite(milliseconds) && milliseconds > 0.0 && milliseconds < 10000.0
        ? static_cast<uint64_t>(milliseconds * 1000.0) : 0;
}
inline void gpu_sample(double milliseconds) {
    const auto us = microseconds(milliseconds);
    if (us) {
        metrics.gpuUs.fetch_add(us, std::memory_order_relaxed);
        metrics.gpuSamples.fetch_add(1, std::memory_order_relaxed);
    }
}
inline void render_sample(double milliseconds) {
    metrics.renderUs.fetch_add(microseconds(milliseconds), std::memory_order_relaxed);
    metrics.renders.fetch_add(1, std::memory_order_relaxed);
}
inline void match_sample(double milliseconds) {
    metrics.matchUs.fetch_add(microseconds(milliseconds), std::memory_order_relaxed);
    metrics.matches.fetch_add(1, std::memory_order_relaxed);
}
struct Snapshot {
    uint64_t presents, renders, gpuSamples, gpuUs, renderUs, matches, matchUs, boundsFallbacks;
};
inline Snapshot snapshot() {
    return {metrics.presents.load(std::memory_order_relaxed), metrics.renders.load(std::memory_order_relaxed),
        metrics.gpuSamples.load(std::memory_order_relaxed), metrics.gpuUs.load(std::memory_order_relaxed),
        metrics.renderUs.load(std::memory_order_relaxed), metrics.matches.load(std::memory_order_relaxed),
        metrics.matchUs.load(std::memory_order_relaxed), metrics.boundsFallbacks.load(std::memory_order_relaxed)};
}
}
