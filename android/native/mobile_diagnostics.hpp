#pragma once
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>

namespace conker::mobile {
// JNI waits on its diagnostic thread, never on the UI/game/render threads.
// The graphics thread formats one bounded CPU snapshot only after an explicit request.
class DiagnosticMailbox {
    std::atomic<uint64_t> pending{0};
    std::mutex mutex;
    std::condition_variable ready;
    uint64_t serial = 0, completed = 0;
    bool active = false;
    std::string result;
public:
    uint64_t requested() const { return pending.load(std::memory_order_acquire); }
    std::string request(std::chrono::milliseconds timeout = std::chrono::seconds(3)) {
        std::unique_lock lock(mutex);
        if (active) return "[render-capture] busy; an earlier capture is pending\n";
        active = true;
        const uint64_t id = ++serial;
        pending.store(id, std::memory_order_release);
        if (ready.wait_for(lock, timeout, [&] { return completed == id; })) { active = false; return std::move(result); }
        uint64_t expected = id;
        pending.compare_exchange_strong(expected, 0);
        active = false;
        return "[render-capture] timeout; no completed display list captured (paused, background or busy renderer)\n";
    }
    void publish(uint64_t id, std::string text) {
        std::lock_guard lock(mutex);
        if (id == 0 || pending.load() != id) return; // Discard expired requests.
        result = std::move(text);
        completed = id;
        pending.store(0, std::memory_order_release);
        ready.notify_all();
    }
};
inline DiagnosticMailbox diagnostics;
}
