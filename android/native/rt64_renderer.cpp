// Minimal RT64 adapter without RecompFrontend UI.
// API/method sequencing follows N64Recomp/RecompFrontend's rt64_render_context.cpp.
// Uses the APIs of the pinned RT64 and ultramodern dependencies.
#include <algorithm>
#include <memory>
#include <cstdio>
#include <exception>
#include <chrono>
#include "mobile_metrics.hpp"
#include "mobile_render_policy.hpp"
#include "mobile_camera.hpp"
#include "surface_lifecycle.hpp"
#include "librecomp/game.hpp"
#include "rt64_storage.hpp"
#include "hle/rt64_application.h"
#include "ultramodern/ultramodern.hpp"

namespace {
using namespace ultramodern::renderer;
class AndroidRenderer final : public RendererContext {
    std::unique_ptr<RT64::Application> app;
    uint8_t dmem[0x1000]{}, imem[0x1000]{}, header[0x40]{};
    unsigned int mi = 0, dpc[8]{};
    bool first_display_list = true;
    bool first_screen_update = true;
    using Clock = std::chrono::steady_clock;
    Clock::time_point metrics_start = Clock::now();
    conker::mobile::Snapshot last_metrics{};
    uint64_t dl_count = 0, dl_us = 0;
    uint64_t lastPairs=0,lastAvoided=0,lastRejected=0,lastBehind=0,lastClipped=0;
    uint64_t modifyXY=0,modifyZ=0,modifyClones=0,modifyMerged=0;
    static void interrupt() {}
    void report_metrics() {
        const auto now = Clock::now();
        const double seconds = std::chrono::duration<double>(now - metrics_start).count();
        if (seconds < 5.0) return;
        const auto sample = conker::mobile::snapshot();
        auto average = [](uint64_t sum, uint64_t count) { return count ? double(sum) / (1000.0 * count) : 0.0; };
        std::fprintf(stderr, "[perf] window=%.2fs presentSubmitFPS=%.2f renderedFPS=%.2f gameDLps=%.2f gpuMs=%.3f renderWallMs=%.3f dlWallMs=%.3f matchMs=%.3f conservativeBounds=%llu internal=%.2fx target=60\n",
            seconds, (sample.presents-last_metrics.presents)/seconds, (sample.renders-last_metrics.renders)/seconds, dl_count/seconds,
            average(sample.gpuUs-last_metrics.gpuUs,sample.gpuSamples-last_metrics.gpuSamples),
            average(sample.renderUs-last_metrics.renderUs,sample.renders-last_metrics.renders), average(dl_us,dl_count),
            average(sample.matchUs-last_metrics.matchUs,sample.matches-last_metrics.matches),
            static_cast<unsigned long long>(sample.boundsFallbacks-last_metrics.boundsFallbacks), get_resolution_scale());
        auto& m=conker::mobile::metrics;
        const auto vi = m.viSize.load(std::memory_order_relaxed);
        const auto surface = m.surfaceSize.load(std::memory_order_relaxed);
        const auto texture = m.presentedSize.load(std::memory_order_relaxed);
        std::fprintf(stderr,"[resolution] vi=%ux%u surface=%ux%u colorTexture=%ux%u internalHeightTarget=%u scale=%.3f gpuSamples=%llu\n",
            unsigned(vi >> 32), unsigned(vi), unsigned(surface >> 32), unsigned(surface),
            unsigned(texture >> 32), unsigned(texture),
            conker::mobile::internalHeight, get_resolution_scale(),
            (unsigned long long)(sample.gpuSamples-last_metrics.gpuSamples));
        auto pairs=m.matchPairs.load(),avoided=m.matchPairsAvoided.load(),rejected=m.meshRejected.load();
        auto behind=m.boundsBehind.load(),clipped=m.boundsClipped.load();
        std::fprintf(stderr,"[perf-detail] matchPairs=%llu avoidedPairs=%llu meshRejected=%llu behindBounds=%llu clippedBounds=%llu\n",
            (unsigned long long)(pairs-lastPairs),(unsigned long long)(avoided-lastAvoided),(unsigned long long)(rejected-lastRejected),
            (unsigned long long)(behind-lastBehind),(unsigned long long)(clipped-lastClipped));
        lastPairs=pairs;lastAvoided=avoided;lastRejected=rejected;lastBehind=behind;lastClipped=clipped;
        const auto camera=conker::camera::counters();
        std::fprintf(stderr,"[render-detail] modifyXY=%llu modifyZ=%llu inheritedEdits=%llu mergedEdits=%llu cameraHooks=%llu cameraAllowed=%llu cameraUpdates=%llu cameraBlocked=%llu fullWidthClears=%llu quality=fixed1080\n",
            (unsigned long long)modifyXY,(unsigned long long)modifyZ,(unsigned long long)modifyClones,(unsigned long long)modifyMerged,
            (unsigned long long)camera.hooks,(unsigned long long)camera.allowed,(unsigned long long)camera.updates,(unsigned long long)camera.blocked,
            (unsigned long long)m.edgeClears.load(std::memory_order_relaxed));
        std::fprintf(stderr,"[compat-total] singleSourceDraws=%llu coveragePasses=%llu depthOrderedTriangles=%llu surfaceLosses=%llu surfaceRecoveries=%llu\n",
            (unsigned long long)m.singleSourceDraws.load(),(unsigned long long)m.coveragePasses.load(),
            (unsigned long long)m.depthOrderedTriangles.load(),(unsigned long long)conker::android::surfaceLosses.load(),
            (unsigned long long)conker::android::surfaceRecoveries.load());
        modifyXY=modifyZ=modifyClones=modifyMerged=0;
        last_metrics=sample; dl_us=dl_count=0; metrics_start=now;
    }
public:
    AndroidRenderer(uint8_t* rdram, WindowHandle window) {
        chosen_api = GraphicsApi::Vulkan;
        setup_result = SetupResult::GraphicsDeviceNotFound;
        // This constructor runs on ultramodern's graphics thread, not SDL_main.
        // Keep exceptions inside this thread so its readiness semaphore is signalled.
        try {
            initialize(rdram, window);
        } catch (const std::exception& error) {
            std::fprintf(stderr, "[renderer] RT64 initialization exception: %s\n", error.what());
            if (app) { app->end(); app.reset(); }
            setup_result = SetupResult::GraphicsDeviceNotFound;
        } catch (...) {
            std::fprintf(stderr, "[renderer] Unknown RT64 initialization exception\n");
            if (app) { app->end(); app.reset(); }
            setup_result = SetupResult::GraphicsDeviceNotFound;
        }
    }
private:
    void initialize(uint8_t* rdram, WindowHandle window) {
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
        conker::android::configure_rt64_storage(config, recomp::get_config_path());
        std::fprintf(stderr, "[renderer] RT64 dataPath=%s; detectDataPath=false\n", config.dataPath.c_str());
        app = std::make_unique<RT64::Application>(core, config);
        std::fprintf(stderr, "[renderer] RT64 constructed; starting Vulkan setup\n");
        using U = RT64::UserConfiguration;
        app->userConfig.graphicsAPI = U::GraphicsAPI::Vulkan;
        app->userConfig.developerMode = false;
        // Disable the desktop GPU keep-alive compute loop. It contends for the
        // same worker/queue and spends battery even between useful frames.
        app->userConfig.idleWorkActive = false;
        app->userConfig.resolution = U::Resolution::Manual;
        app->userConfig.resolutionMultiplier = conker::mobile::resolution_scale(240);
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
        const R result = app->setup(0);
        std::fprintf(stderr, "[renderer] RT64 setup result=%d (0=Success)\n", static_cast<int>(result));
        switch (result) {
            case R::Success: setup_result = SetupResult::Success; break;
            case R::DynamicLibrariesNotFound: setup_result = SetupResult::DynamicLibrariesNotFound; break;
            case R::InvalidGraphicsAPI: setup_result = SetupResult::InvalidGraphicsAPI; break;
            case R::GraphicsAPINotFound: setup_result = SetupResult::GraphicsAPINotFound; break;
            default: setup_result = SetupResult::GraphicsDeviceNotFound; break;
        }
        if (setup_result != SetupResult::Success) { app->end(); app.reset(); return; }
        app->setFullScreen(true);
        std::fprintf(stderr, "[mobile] Fixed internal height=1080; VI-aware scaling; native Surface; adaptive resolution OFF; GPU idle work OFF\n");
        metrics_start = Clock::now();
        std::fprintf(stderr, "[renderer] Vulkan ready; fullscreen/Expand; target presentation=60\n");
    }
public:
    bool valid() override { return app != nullptr && setup_result == SetupResult::Success; }
    bool update_config(const GraphicsConfig&, const GraphicsConfig&) override { return false; }
    void enable_instant_present() override {
        app->enhancementConfig.presentation.mode = RT64::EnhancementConfiguration::Presentation::Mode::PresentEarly;
        app->updateEnhancementConfig();
    }
    void send_dl(const OSTask* task) override {
        if (first_display_list) {
            std::fprintf(stderr, "[renderer] First Conker display list received\n");
            first_display_list = false;
        }
        const auto begin = Clock::now();
        app->state->rsp->reset();
        app->interpreter->loadUCodeGBI(task->t.ucode & 0x3FFFFFF, task->t.ucode_data & 0x3FFFFFF, true);
        app->processDisplayLists(app->core.RDRAM, task->t.data_ptr & 0x3FFFFFF, 0, true);
        dl_us += std::chrono::duration_cast<std::chrono::microseconds>(Clock::now()-begin).count();
        ++dl_count;
        const auto& edits=app->state->rsp->screenModifyRecords;
        modifyXY+=edits.xyCommands;modifyZ+=edits.zCommands;
        modifyClones+=edits.clones;modifyMerged+=edits.coalesced;
        conker::mobile::publish_bounds();
    }
    void send_dummy_workload(uint32_t address) override {
        app->state->listProcessBegin();
        app->state->rdp->setColorImage(G_IM_FMT_RGBA, G_IM_SIZ_16b, 320, address);
        app->state->rdp->setOtherMode(0x382C30, 0);
        app->state->rdp->fillRect(0, 0, 320 << 2, 240 << 2);
        app->state->fullSync(); app->state->listProcessEnd();
    }
    void update_screen() override {
        if (first_screen_update) {
            std::fprintf(stderr, "[renderer] First VI update received\n");
            first_screen_update = false;
        }
        app->updateScreen();
        report_metrics();
    }
    void shutdown() override { if (app) { app->end(); app.reset(); } }
    uint32_t get_display_framerate() const override {
        return app ? app->presentQueue->ext.sharedResources->swapChainRate : 0;
    }
    float get_resolution_scale() const override {
        return app ? float(conker::mobile::metrics.verticalScaleMilli.load(std::memory_order_relaxed)) / 1000.0f : 1.f;
    }
};
}
std::unique_ptr<ultramodern::renderer::RendererContext> create_android_renderer(
    uint8_t* rdram, ultramodern::renderer::WindowHandle window, bool) {
    return std::make_unique<AndroidRenderer>(rdram, window);
}
