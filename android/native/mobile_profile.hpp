#pragma once
#include "ultramodern/config.hpp"

// Internal policy, not user-facing settings. Presentation FPS is not game speed.
inline ultramodern::renderer::GraphicsConfig mobile_profile() {
    using namespace ultramodern::renderer;
    GraphicsConfig config{};
    config.developer_mode = false;
    // The Android RT64 adapter owns the fixed 1080-line policy for every VI mode.
    config.res_option = Resolution::Auto;
    config.wm_option = WindowMode::Fullscreen;
    config.hr_option = HUDRatioMode::Clamp16x9;
    config.api_option = GraphicsApi::Vulkan;
    config.ar_option = AspectRatio::Expand;
    config.msaa_option = Antialiasing::None;
    config.rr_option = RefreshRate::Manual;
    config.hpfb_option = HighPrecisionFramebuffer::Off;
    config.rr_manual_value = 60;
    config.ds_option = 1;
    return config;
}
