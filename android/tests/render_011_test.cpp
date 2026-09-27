// The included body and rect/viewport methods are extracted unchanged from RT64.
#include "bounds_types.hpp"
#include "mobile_render_safety.hpp"
#include "mobile_clip_bounds.hpp"
#include "mobile_metrics.hpp"
#include <array>
#include <iostream>
#include <random>
#include <stdexcept>
namespace RT64 {
constexpr uint16_t G_EX_ORIGIN_NONE = 0x800; // rt64_extended_gbi.h
struct Projection { enum class Type { Perspective, Orthographic }; };
struct Result { FixedRect scissorRect, drawColorRect, drawDepthRect; };
Result bounds(const std::array<hlslpp::float3, 3>& screens, const std::array<float, 3>& w,
              bool perspective = true, bool anchored = false, bool writesDepth = true) {
    struct DrawData { std::array<hlslpp::float3, 3> posScreen; std::array<hlslpp::float4, 3> posTransformed; };
    struct Workload { DrawData drawData; } workload;
    workload.drawData.posScreen = screens;
    for (int i = 0; i < 3; ++i) workload.drawData.posTransformed[i] = hlslpp::float4(0, 0, 0, w[i]);
    auto& posScreen = workload.drawData.posScreen;
    const uint32_t globalIndices[3] = {0, 1, 2};
    const uint32_t geometryMode = 0, cullBothMask = 3;
    struct RDP { std::array<FixedRect, 1> scissorRectStack; int scissorStackSize = 1; } rdp{{FixedRect(0, 0, 1280, 960)}};
    struct State { RDP* rdp; } snapshot{&rdp}; auto* state = &snapshot;
    const int viewportStackSize = 1, otherModeStackSize = 1;
    std::array<interop::RSPViewport, 1> viewportStack{{{interop::float3(160, 120, .5f), interop::float3(160, 120, .5f)}}};
    const std::array<int16_t, 4> clipRatios{1, 1, -1, -1};
    struct Mode { bool write; bool zUpd() const { return write; } };
    const std::array<Mode, 1> otherModeStack{{{writesDepth}}};
    [[maybe_unused]] const auto projType = perspective ? Projection::Type::Perspective : Projection::Type::Orthographic;
    struct Extended { std::array<uint16_t, 1> viewportOriginStack; };
    [[maybe_unused]] const Extended extended{{anchored ? uint16_t(0) : G_EX_ORIGIN_NONE}};
    Result fbPair;
#include "bounds_body.inc"
    return fbPair;
}
}
int main() { try {
    unsigned checks = 0;
    auto check = [&](bool condition, const char* message) { ++checks; if (!condition) throw std::runtime_error(message); };
    using RT64::bounds;
    using hlslpp::float3;
    const std::array<float, 3> front{1, 1, 1};
    for (float x : {-35.f, 335.f}) {
        const std::array<float3, 3> triangle{float3(x, 185, 0), float3(x + 10, 190, 0), float3(x, 205, 0)};
        auto wide = bounds(triangle, front);
        check(!wide.drawColorRect.isNull(), "side-only visible triangle lost from framebuffer bounds");
        check(wide.drawColorRect.bottom(true) >= 205, "wide bottom rows missing");
        check(!wide.drawDepthRect.isNull(), "wide depth tracking lost");
        check(bounds(triangle, front, false).drawColorRect.isNull(), "orthographic clipping changed");
        check(bounds(triangle, front, true, true).drawColorRect.isNull(), "anchored viewport expanded");
        check(bounds(triangle, front, true, false, false).drawDepthRect.isNull(), "translucent surface unexpectedly writes depth");
    }
    check(bounds({float3(15, 260, 0), float3(100, 290, 0), float3(50, 310, 0)}, front).drawColorRect.isNull(), "vertical clipping lost");
    check(bounds({float3(10, 30, 0), float3(100, 20, 0), float3(80, 180, 0)}, {-1, -2, -3}).drawColorRect.isNull(), "behind-camera geometry retained");
    check(!bounds({float3(-100, 30, 0), float3(100, 20, 0), float3(80, 180, 0)}, {-1, 1, 2}).drawColorRect.isNull(), "near-plane crossing lost");
    // Sample physically visible points after the SAME horizontal projection
    // expansion used by RT64. Bounds must contain their scanlines, including
    // triangles wholly outside the original 4:3 viewport.
    std::mt19937 rng(110011);
    std::uniform_real_distribution<float> xx(-240, 560), yy(-70, 310);
    for (float aspect : {4.f/3, 16.f/9, 20.f/9, 21.f/9}) {
        for (int i = 0; i < 3000; ++i) {
            std::array<float3, 3> t;
            for (auto& v : t) v = float3(xx(rng), yy(rng), 0);
            auto b = bounds(t, front);
            for (int j = 0; j < 10; ++j) {
                const float u = (j % 4) / 4.f, v = (j / 4) / 4.f;
                const float3 p = t[0] * (1-u-v) + t[1] * u + t[2] * v;
                const float gpuX = 160 + (float(p.x) - 160) * (4.f/3) / aspect;
                if (gpuX < 0 || gpuX > 320 || p.y < 0 || p.y > 240) continue;
                check(!b.drawColorRect.isNull(), "visible projected sample dropped");
                check(b.drawColorRect.uly <= float(p.y)*4 && b.drawColorRect.lry >= float(p.y)*4,
                      "visible projected sample outside tracked framebuffer rows");
            }
        }
    }
    std::cout << "PASS " << checks << " actual RSP bounds assertions (4:3 to 21:9, no phone/GPU claim)\n";
    return 0;
} catch (const std::exception& e) { std::cerr << "FAIL: " << e.what() << '\n'; return 1; } }
