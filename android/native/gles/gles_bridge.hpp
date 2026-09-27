// SPDX-License-Identifier: MIT
#pragma once
#include <cstdint>

// Renderer-only bridge. CPU, audio, saves and input remain in N64ModernRuntime.
// All calls, including the platform callbacks, run on its graphics thread.
namespace conker::gles {
struct Host {
    void* window{};
    bool (*start)(void*, uint32_t&, uint32_t&){};
    void (*stop)(void*){};
    void (*swap)(void*){};
};
struct State {
    uint32_t width{}, height{}, viWidth{}, viHeight{}, swaps{}, triangles{};
    uint32_t colorWidth{}, colorHeight{}, colorAllocationHeight{};
    uint64_t displayLists{}, extendedRectangles{}, ignoredMatrixGroups{};
    float horizontalScale{1.f}, renderScale{1.f};
    bool conkerMicrocode{};
};
#define CONKER_GLES_API __attribute__((visibility("default")))
CONKER_GLES_API bool start(uint8_t* rdram, uint32_t* viRegisters, const Host&, const char* dataPath);
CONKER_GLES_API void process(uint32_t ucode, uint32_t ucodeData, uint32_t ucodeSize,
             uint32_t data, uint32_t dataSize, uint32_t stackSize);
CONKER_GLES_API void update();
CONKER_GLES_API void dummy(uint32_t address);
CONKER_GLES_API void stop();
CONKER_GLES_API State state();
#undef CONKER_GLES_API
}
