#pragma once
#include <algorithm>
#include <cstdlib>
#include <string_view>
#include "ultramodern/config.hpp"

namespace conker::android {

// Same visible graphics options/defaults as RecompFrontend PC 0.1.2.
// Android fixes only WindowMode=Fullscreen and GraphicsApi=Vulkan.
struct PcGraphicsPreferences {
    int resolution = 2;      // 0 Original, 1 Original2x, 2 Auto
    int downsampling = 0;    // 0 Off, 2x, 4x
    int hud = 1;             // 0 Original, 1 Clamp16x9, 2 Full
    int aspect = 1;          // 0 Original, 1 Expand
    int msaa = 1;            // 0 None, 1 2X, 2 4X, 3 8X
    int refresh = 1;         // 0 Original, 1 Display, 2 Manual
    int refreshManual = 60;  // PC range 20..240
    int highPrecision = 2;   // 0 Auto, 1 On, 2 Off
};
inline PcGraphicsPreferences pcGraphics{};

inline int parse_int(const char* value, int fallback) {
    if (!value || !*value) return fallback;
    char* end = nullptr;
    long parsed = std::strtol(value, &end, 10);
    return (end && *end == 0) ? int(parsed) : fallback;
}

inline bool set_pc_graphics_option(std::string_view name, const char* value) {
    const int v = parse_int(value, 0);
    if (name == "--resolution") pcGraphics.resolution = std::clamp(v, 0, 2);
    else if (name == "--downsampling") pcGraphics.downsampling = (v == 2 || v == 4) ? v : 0;
    else if (name == "--hud") pcGraphics.hud = std::clamp(v, 0, 2);
    else if (name == "--aspect") pcGraphics.aspect = std::clamp(v, 0, 1);
    else if (name == "--msaa") pcGraphics.msaa = std::clamp(v, 0, 3);
    else if (name == "--refresh") pcGraphics.refresh = std::clamp(v, 0, 2);
    else if (name == "--refresh-value") pcGraphics.refreshManual = std::clamp(v, 20, 240);
    else if (name == "--high-precision") pcGraphics.highPrecision = std::clamp(v, 0, 2);
    else return false;
    return true;
}

inline ultramodern::renderer::GraphicsConfig pc_graphics_profile() {
    using namespace ultramodern::renderer;
    GraphicsConfig config{};
    config.developer_mode = false;
    config.res_option = pcGraphics.resolution == 0 ? Resolution::Original :
        pcGraphics.resolution == 1 ? Resolution::Original2x : Resolution::Auto;
    config.wm_option = WindowMode::Fullscreen;
    config.hr_option = pcGraphics.hud == 0 ? HUDRatioMode::Original :
        pcGraphics.hud == 2 ? HUDRatioMode::Full : HUDRatioMode::Clamp16x9;
    config.api_option = GraphicsApi::Vulkan;
    config.ar_option = pcGraphics.aspect == 0 ? AspectRatio::Original : AspectRatio::Expand;
    config.msaa_option = pcGraphics.msaa == 0 ? Antialiasing::None :
        pcGraphics.msaa == 1 ? Antialiasing::MSAA2X :
        pcGraphics.msaa == 2 ? Antialiasing::MSAA4X : Antialiasing::MSAA8X;
    config.rr_option = pcGraphics.refresh == 0 ? RefreshRate::Original :
        pcGraphics.refresh == 2 ? RefreshRate::Manual : RefreshRate::Display;
    config.hpfb_option = pcGraphics.highPrecision == 0 ? HighPrecisionFramebuffer::Auto :
        pcGraphics.highPrecision == 1 ? HighPrecisionFramebuffer::On : HighPrecisionFramebuffer::Off;
    config.rr_manual_value = pcGraphics.refreshManual;
    config.ds_option = pcGraphics.downsampling;
    return config;
}

} // namespace conker::android
