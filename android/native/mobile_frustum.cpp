// USA func_1501B22C builds the camera planes consumed by func_150A6360.
// Run it first on EVERY update; modifying its output avoids accumulating aspect
// changes or changing the projection/FOV used by the original game.
#include <cstdint>
#include <cstring>
#include "recomp.h"
#include "mobile_frustum.hpp"
#include "mobile_metrics.hpp"
extern "C" void conker_original_frustum(uint8_t*, recomp_context*);
namespace {
float field(uint8_t* rdram, gpr base, int off) {
    uint32_t bits = MEM_W(off, base); float value;
    std::memcpy(&value, &bits, sizeof(value)); return value;
}
void put(uint8_t* rdram, gpr base, int off, float value) {
    uint32_t bits; std::memcpy(&bits, &value, sizeof(bits)); MEM_W(off, base) = bits;
}
}
extern "C" void func_1501B22C(uint8_t* rdram, recomp_context* ctx) {
    const uint32_t cameraIndex = uint32_t(ctx->r4);
    conker_original_frustum(rdram, ctx);
    if (cameraIndex > 3) return;
    const uint32_t cameras = MEM_W(0, gpr(int32_t(0x800BE628)));
    const uint64_t address = uint64_t(cameras) + cameraIndex * 0x180u;
    if ((address & 3) || address < 0x80000000u || address > 0x80800000u - 0x180u) return;
    const gpr base = gpr(int32_t(address));
    const float scale = conker::mobile::metrics.horizontalAspect.load(std::memory_order_relaxed);
    float leftX = field(rdram, base, 0x88), leftZ = field(rdram, base, 0x90);
    float rightX = field(rdram, base, 0x94), rightZ = field(rdram, base, 0x9C);
    if (!conker::mobile::widen_horizontal_plane(leftX, leftZ, scale) ||
        !conker::mobile::widen_horizontal_plane(rightX, rightZ, scale)) return;
    put(rdram, base, 0x88, leftX); put(rdram, base, 0x90, leftZ);
    put(rdram, base, 0x94, rightX); put(rdram, base, 0x9C, rightZ);
    conker::mobile::metrics.frustumUpdates.fetch_add(1, std::memory_order_relaxed);
}
