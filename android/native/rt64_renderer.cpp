// Minimal RT64 adapter without RecompFrontend UI.
// API/method sequencing follows N64Recomp/RecompFrontend's rt64_render_context.cpp.
// Uses the APIs of the pinned RT64 and ultramodern dependencies.
#include <algorithm>
#include <memory>
#include "hle/rt64_application.h"
#include "ultramodern/ultramodern.hpp"

namespace {
using namespace ultramodern::renderer;
class AndroidRenderer final : public RendererContext {
    std::unique_ptr<RT64::Application> app;
    uint8_t dmem[0x1000]{}, imem[0x1000]{}, header[0x40]{};
    unsigned int mi = 0, dpc[8]{};
    static void interrupt() {}
public:
    AndroidRenderer(uint8_t* rdram, WindowHandle window) {
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
        RT64::ApplicationConfiguration config{};
        config.useConfigurationFile = false;
        app = std::make_unique<RT64::Application>(core, config);
        using U = RT64::UserConfiguration;
        app->userConfig.graphicsAPI = U::GraphicsAPI::Vulkan;
        app->userConfig.developerMode = false;
        app->userConfig.resolution = U::Resolution::Manual;
        app->userConfig.resolutionMultiplier = 2.0;
        app->userConfig.downsampleMultiplier = 1;
        app->userConfig.aspectRatio = U::AspectRatio::Expand;
        app->userConfig.extAspectRatio = U::AspectRatio::Manual;
        app->userConfig.extAspectTarget = 16.0 / 9.0;
        app->userConfig.antialiasing = U::Antialiasing::None;
        app->userConfig.refreshRate = U::RefreshRate::Manual;
        app->userConfig.refreshRateTarget = 60;
        app->userConfig.internalColorFormat = U::InternalColorFormat::Standard;
        app->userConfig.displayBuffering = U::DisplayBuffering::Triple;
        app->enhancementConfig.f3dex.forceBranch = true;
        app->enhancementConfig.textureLOD.scale = true;
        app->enhancementConfig.presentation.mode = RT64::EnhancementConfiguration::Presentation::Mode::PresentEarly;
        chosen_api = GraphicsApi::Vulkan;
        using R = RT64::Application::SetupResult;
        switch (app->setup(0)) {
            case R::Success: setup_result = SetupResult::Success; break;
            case R::DynamicLibrariesNotFound: setup_result = SetupResult::DynamicLibrariesNotFound; break;
            case R::InvalidGraphicsAPI: setup_result = SetupResult::InvalidGraphicsAPI; break;
            case R::GraphicsAPINotFound: setup_result = SetupResult::GraphicsAPINotFound; break;
            default: setup_result = SetupResult::GraphicsDeviceNotFound; break;
        }
        if (setup_result != SetupResult::Success) { app.reset(); return; }
        app->setFullScreen(true);
    }
    bool valid() override { return app != nullptr && setup_result == SetupResult::Success; }
    bool update_config(const GraphicsConfig&, const GraphicsConfig&) override { return false; }
    void enable_instant_present() override {
        app->enhancementConfig.presentation.mode = RT64::EnhancementConfiguration::Presentation::Mode::PresentEarly;
        app->updateEnhancementConfig();
    }
    void send_dl(const OSTask* task) override {
        app->state->rsp->reset();
        app->interpreter->loadUCodeGBI(task->t.ucode & 0x3FFFFFF, task->t.ucode_data & 0x3FFFFFF, true);
        app->processDisplayLists(app->core.RDRAM, task->t.data_ptr & 0x3FFFFFF, 0, true);
    }
    void send_dummy_workload(uint32_t address) override {
        app->state->listProcessBegin();
        app->state->rdp->setColorImage(G_IM_FMT_RGBA, G_IM_SIZ_16b, 320, address);
        app->state->rdp->setOtherMode(0x382C30, 0);
        app->state->rdp->fillRect(0, 0, 320 << 2, 240 << 2);
        app->state->fullSync(); app->state->listProcessEnd();
    }
    void update_screen() override { app->updateScreen(); }
    void shutdown() override { if (app) { app->end(); app.reset(); } }
    uint32_t get_display_framerate() const override {
        return app ? app->presentQueue->ext.sharedResources->swapChainRate : 0;
    }
    float get_resolution_scale() const override {
        return app ? static_cast<float>(app->userConfig.resolutionMultiplier) : 1.f;
    }
};
}
std::unique_ptr<ultramodern::renderer::RendererContext> create_android_renderer(
    uint8_t* rdram, ultramodern::renderer::WindowHandle window, bool) {
    return std::make_unique<AndroidRenderer>(rdram, window);
}
