// Android platform adapter for the unmodified PC 0.1.2 RT64 path.
// The option mapping mirrors RecompFrontend's rt64_render_context.cpp.
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <memory>

#include "librecomp/game.hpp"
#include "rt64_storage.hpp"
#include "hle/rt64_application.h"
#include "ultramodern/ultramodern.hpp"
#include "ultramodern/config.hpp"

namespace {
using namespace ultramodern::renderer;

uint8_t dmem[0x1000]{};
uint8_t imem[0x1000]{};
uint8_t header[0x40]{};
unsigned int mi = 0;
unsigned int dpc[8]{};

void interrupt() {}

RT64::UserConfiguration::AspectRatio to_rt64(AspectRatio option) {
    switch (option) {
        case AspectRatio::Original: return RT64::UserConfiguration::AspectRatio::Original;
        case AspectRatio::Expand: return RT64::UserConfiguration::AspectRatio::Expand;
        case AspectRatio::Manual: return RT64::UserConfiguration::AspectRatio::Manual;
        default: return RT64::UserConfiguration::AspectRatio::Original;
    }
}
RT64::UserConfiguration::Antialiasing to_rt64(Antialiasing option) {
    switch (option) {
        case Antialiasing::MSAA2X: return RT64::UserConfiguration::Antialiasing::MSAA2X;
        case Antialiasing::MSAA4X: return RT64::UserConfiguration::Antialiasing::MSAA4X;
        case Antialiasing::MSAA8X: return RT64::UserConfiguration::Antialiasing::MSAA8X;
        default: return RT64::UserConfiguration::Antialiasing::None;
    }
}
RT64::UserConfiguration::RefreshRate to_rt64(RefreshRate option) {
    switch (option) {
        case RefreshRate::Display: return RT64::UserConfiguration::RefreshRate::Display;
        case RefreshRate::Manual: return RT64::UserConfiguration::RefreshRate::Manual;
        default: return RT64::UserConfiguration::RefreshRate::Original;
    }
}
RT64::UserConfiguration::InternalColorFormat to_rt64(HighPrecisionFramebuffer option) {
    switch (option) {
        case HighPrecisionFramebuffer::On: return RT64::UserConfiguration::InternalColorFormat::High;
        case HighPrecisionFramebuffer::Off: return RT64::UserConfiguration::InternalColorFormat::Standard;
        default: return RT64::UserConfiguration::InternalColorFormat::Automatic;
    }
}

void set_application_user_config(RT64::Application* app, const GraphicsConfig& config) {
    switch (config.res_option) {
        default:
        case Resolution::Auto:
            app->userConfig.resolution = RT64::UserConfiguration::Resolution::WindowIntegerScale;
            app->userConfig.downsampleMultiplier = 1;
            break;
        case Resolution::Original:
            app->userConfig.resolution = RT64::UserConfiguration::Resolution::Manual;
            app->userConfig.resolutionMultiplier = std::max(config.ds_option, 1);
            app->userConfig.downsampleMultiplier = std::max(config.ds_option, 1);
            break;
        case Resolution::Original2x:
            app->userConfig.resolution = RT64::UserConfiguration::Resolution::Manual;
            app->userConfig.resolutionMultiplier = 2.0 * std::max(config.ds_option, 1);
            app->userConfig.downsampleMultiplier = std::max(config.ds_option, 1);
            break;
    }

    switch (config.hr_option) {
        default:
        case HUDRatioMode::Original:
            app->userConfig.extAspectRatio = RT64::UserConfiguration::AspectRatio::Original;
            break;
        case HUDRatioMode::Clamp16x9:
            app->userConfig.extAspectRatio = RT64::UserConfiguration::AspectRatio::Manual;
            app->userConfig.extAspectTarget = 16.0 / 9.0;
            break;
        case HUDRatioMode::Full:
            app->userConfig.extAspectRatio = RT64::UserConfiguration::AspectRatio::Expand;
            break;
    }

    app->userConfig.aspectRatio = to_rt64(config.ar_option);
    app->userConfig.antialiasing = to_rt64(config.msaa_option);
    app->userConfig.refreshRate = to_rt64(config.rr_option);
    app->userConfig.refreshRateTarget = config.rr_manual_value;
    app->userConfig.internalColorFormat = to_rt64(config.hpfb_option);
    app->userConfig.displayBuffering = RT64::UserConfiguration::DisplayBuffering::Triple;
}

SetupResult map_setup(RT64::Application::SetupResult result) {
    using R = RT64::Application::SetupResult;
    switch (result) {
        case R::Success: return SetupResult::Success;
        case R::DynamicLibrariesNotFound: return SetupResult::DynamicLibrariesNotFound;
        case R::InvalidGraphicsAPI: return SetupResult::InvalidGraphicsAPI;
        case R::GraphicsAPINotFound: return SetupResult::GraphicsAPINotFound;
        default: return SetupResult::GraphicsDeviceNotFound;
    }
}

class AndroidRT64Context final : public RendererContext {
    std::unique_ptr<RT64::Application> app;
public:
    AndroidRT64Context(uint8_t* rdram, WindowHandle window) {
        chosen_api = GraphicsApi::Vulkan;
        setup_result = SetupResult::GraphicsDeviceNotFound;

        RT64::Application::Core core{};
        core.window = window;
        core.checkInterrupts = interrupt;
        core.HEADER = header; core.RDRAM = rdram; core.DMEM = dmem; core.IMEM = imem;
        core.MI_INTR_REG = &mi;
        core.DPC_START_REG = &dpc[0]; core.DPC_END_REG = &dpc[1];
        core.DPC_CURRENT_REG = &dpc[2]; core.DPC_STATUS_REG = &dpc[3];
        core.DPC_CLOCK_REG = &dpc[4]; core.DPC_BUFBUSY_REG = &dpc[5];
        core.DPC_PIPEBUSY_REG = &dpc[6]; core.DPC_TMEM_REG = &dpc[7];

        auto* vi = get_vi_regs();
        core.VI_STATUS_REG = &vi->VI_STATUS_REG;
        core.VI_ORIGIN_REG = &vi->VI_ORIGIN_REG;
        core.VI_WIDTH_REG = &vi->VI_WIDTH_REG;
        core.VI_INTR_REG = &vi->VI_INTR_REG;
        core.VI_V_CURRENT_LINE_REG = &vi->VI_V_CURRENT_LINE_REG;
        core.VI_TIMING_REG = &vi->VI_TIMING_REG;
        core.VI_V_SYNC_REG = &vi->VI_V_SYNC_REG;
        core.VI_H_SYNC_REG = &vi->VI_H_SYNC_REG;
        core.VI_LEAP_REG = &vi->VI_LEAP_REG;
        core.VI_H_START_REG = &vi->VI_H_START_REG;
        core.VI_V_START_REG = &vi->VI_V_START_REG;
        core.VI_V_BURST_REG = &vi->VI_V_BURST_REG;
        core.VI_X_SCALE_REG = &vi->VI_X_SCALE_REG;
        core.VI_Y_SCALE_REG = &vi->VI_Y_SCALE_REG;

        RT64::ApplicationConfiguration appConfig{};
        conker::android::configure_rt64_storage(appConfig, recomp::get_config_path());
        app = std::make_unique<RT64::Application>(core, appConfig);

        const auto& config = get_graphics_config();
        set_application_user_config(app.get(), config);
        app->userConfig.developerMode = config.developer_mode;
        app->userConfig.graphicsAPI = RT64::UserConfiguration::GraphicsAPI::Vulkan;

        // Same Conker enhancements and presentation mode as PC 0.1.2.
        app->enhancementConfig.f3dex.forceBranch = true;
        app->enhancementConfig.textureLOD.scale = true;
        app->enhancementConfig.presentation.mode =
            RT64::EnhancementConfiguration::Presentation::Mode::PresentEarly;

        setup_result = map_setup(app->setup(0));
        chosen_api = GraphicsApi::Vulkan;
        if (setup_result != SetupResult::Success) {
            app.reset();
            return;
        }
        app->setFullScreen(true);
        std::fprintf(stderr,
            "[renderer] PC 0.1.2 RT64; Vulkan; resolution=%d ds=%d aspect=%d msaa=%d refresh=%d/%d hud=%d hpfb=%d\n",
            int(config.res_option), config.ds_option, int(config.ar_option), int(config.msaa_option),
            int(config.rr_option), config.rr_manual_value, int(config.hr_option), int(config.hpfb_option));
    }

    bool valid() override { return app != nullptr && setup_result == SetupResult::Success; }

    bool update_config(const GraphicsConfig& oldConfig, const GraphicsConfig& newConfig) override {
        if (!app || oldConfig == newConfig) return false;
        set_application_user_config(app.get(), newConfig);
        const bool resolutionChanged = oldConfig.res_option != newConfig.res_option;
        const bool aspectChanged = oldConfig.ar_option != newConfig.ar_option;
        const bool downsamplingChanged = oldConfig.ds_option != newConfig.ds_option;
        const bool msaaChanged = oldConfig.msaa_option != newConfig.msaa_option;
        app->updateUserConfig(resolutionChanged || aspectChanged || downsamplingChanged || msaaChanged);
        if (msaaChanged) app->updateMultisampling();
        return true;
    }

    void enable_instant_present() override {
        if (!app) return;
        app->enhancementConfig.presentation.mode =
            RT64::EnhancementConfiguration::Presentation::Mode::PresentEarly;
        app->updateEnhancementConfig();
    }

    void send_dl(const OSTask* task) override {
        if (!app) return;
        app->state->rsp->reset();
        app->interpreter->loadUCodeGBI(task->t.ucode & 0x3FFFFFF,
            task->t.ucode_data & 0x3FFFFFF, true);
        app->processDisplayLists(app->core.RDRAM, task->t.data_ptr & 0x3FFFFFF, 0, true);
    }

    void send_dummy_workload(uint32_t address) override {
        if (!app) return;
        app->state->listProcessBegin();
        app->state->rdp->setColorImage(G_IM_FMT_RGBA, G_IM_SIZ_16b, 320, address);
        app->state->rdp->setOtherMode(0x382C30, 0);
        app->state->rdp->fillRect(0, 0, 320 << 2, 240 << 2);
        app->state->fullSync();
        app->state->listProcessEnd();
    }

    void update_screen() override { if (app) app->updateScreen(); }

    void shutdown() override {
        if (app) {
            app->end();
            app.reset();
        }
    }

    uint32_t get_display_framerate() const override {
        return app ? app->presentQueue->ext.sharedResources->swapChainRate : 0;
    }

    float get_resolution_scale() const override {
        if (!app) return 1.0f;
        constexpr int ReferenceHeight = 240;
        switch (app->userConfig.resolution) {
            case RT64::UserConfiguration::Resolution::WindowIntegerScale:
                if (app->sharedQueueResources->swapChainHeight > 0)
                    return std::max(float((app->sharedQueueResources->swapChainHeight + ReferenceHeight - 1) / ReferenceHeight), 1.0f);
                return 1.0f;
            case RT64::UserConfiguration::Resolution::Manual:
                return float(app->userConfig.resolutionMultiplier);
            default:
                return 1.0f;
        }
    }
};
} // namespace

std::unique_ptr<ultramodern::renderer::RendererContext> create_android_renderer(
    uint8_t* rdram, ultramodern::renderer::WindowHandle window, bool) {
    return std::make_unique<AndroidRT64Context>(rdram, window);
}
