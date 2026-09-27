// SPDX-License-Identifier: MIT
#include <SDL.h>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <memory>
#include <sstream>
#include "ultramodern/ultramodern.hpp"
#include "librecomp/game.hpp"
#include "gles/gles_bridge.hpp"
#include "mobile_metrics.hpp"
#include "mobile_diagnostics.hpp"

void conker_pump_gles_events();

namespace {
using namespace ultramodern::renderer;
class GLESRenderer final : public RendererContext {
    SDL_Window* window;
    SDL_GLContext context{};
    bool ready{};
    using Clock = std::chrono::steady_clock;
    Clock::time_point last = Clock::now();
    uint64_t lists{}, dlMicros{}, oldLists{};
    uint32_t oldSwaps{};
    static bool start_context(void* opaque, uint32_t& width, uint32_t& height) {
        auto& self = *static_cast<GLESRenderer*>(opaque);
        self.context = SDL_GL_CreateContext(self.window);
        if (!self.context) {
            std::fprintf(stderr, "[opengl] SDL_GL_CreateContext failed: %s\n", SDL_GetError());
            return false;
        }
        int w{}, h{};
        SDL_GL_GetDrawableSize(self.window, &w, &h);
        width = w; height = h;
        SDL_GL_SetSwapInterval(1);
        return width > 0 && height > 0;
    }
    static void stop_context(void* opaque) {
        auto& self = *static_cast<GLESRenderer*>(opaque);
        if (self.context) { SDL_GL_DeleteContext(self.context); self.context = nullptr; }
    }
    static void swap(void* opaque) { SDL_GL_SwapWindow(static_cast<GLESRenderer*>(opaque)->window); }
    void failure(const std::exception& error) {
        std::fprintf(stderr, "[opengl] Renderer failure: %s\n", error.what());
        std::ofstream(recomp::get_config_path() / "startup-error.txt") << "OpenGL: " << error.what() << '\n';
        ready = false;
        ultramodern::quit();
    }
    void publish() {
        const auto s = conker::gles::state();
        conker::mobile::metrics.horizontalAspect.store(s.horizontalScale, std::memory_order_relaxed);
        const auto request = conker::mobile::diagnostics.requested();
        if (request) {
            std::ostringstream out;
            out << "[render-capture] renderer=OpenGL-ES3 GLideN64; draws=" << s.displayLists
                << " trianglesLastDL=" << s.triangles << " extendedRects=" << s.extendedRectangles
                << " cbfdMicrocode=" << s.conkerMicrocode
                << " interpolation=off\n[resolution] surface=" << s.width << 'x' << s.height
                << " vi=" << s.viWidth << 'x' << s.viHeight << " horizontalScale=" << s.horizontalScale
                << " colorViewport=" << s.colorWidth << 'x' << s.colorHeight
                << " colorAllocation=" << s.colorWidth << 'x' << s.colorAllocationHeight
                << "\n[render-capture-end]\n";
            conker::mobile::diagnostics.publish(request, out.str());
        }
        const auto now = Clock::now();
        const double seconds = std::chrono::duration<double>(now - last).count();
        if (seconds < 5.) return;
        std::fprintf(stderr, "[perf] renderer=OpenGL-ES3 window=%.2fs presentSubmitFPS=%.2f gameDLps=%.2f dlWallMs=%.3f gpuMs=unavailable interpolation=off\n",
            seconds, (s.swaps - oldSwaps) / seconds, (s.displayLists - oldLists) / seconds,
            lists ? double(dlMicros) / (1000. * lists) : 0.);
        std::fprintf(stderr, "[resolution] renderer=OpenGL-ES3 vi=%ux%u surface=%ux%u colorViewport=%ux%u colorAllocation=%ux%u scale=%.3f horizontalScale=%.4f\n",
            s.viWidth, s.viHeight, s.width, s.height, s.colorWidth, s.colorHeight,
            s.colorWidth, s.colorAllocationHeight, s.renderScale, s.horizontalScale);
        oldSwaps = s.swaps; oldLists = s.displayLists; lists = dlMicros = 0; last = now;
    }
public:
    GLESRenderer(uint8_t* rdram, WindowHandle w) : window(w) {
        chosen_api = GraphicsApi::OpenGL;
        setup_result = SetupResult::GraphicsDeviceNotFound;
        try {
            static_assert(sizeof(ViRegs) == 14 * sizeof(uint32_t));
            const auto path = (recomp::get_config_path() / "gliden64").string();
            ready = conker::gles::start(rdram, reinterpret_cast<uint32_t*>(get_vi_regs()),
                {this, start_context, stop_context, swap}, path.c_str());
            if (ready) {
                setup_result = SetupResult::Success;
                std::fprintf(stderr, "[renderer] OpenGL ES 3 ready; GLideN64=41c7ba27; native Surface; adaptive resolution OFF; interpolation=off\n");
                publish();
            }
        } catch (const std::exception& error) {
            std::fprintf(stderr, "[opengl] Initialization failed: %s\n", error.what());
            conker::gles::stop(); stop_context(this);
        }
    }
    bool valid() override { return ready; }
    bool update_config(const GraphicsConfig&, const GraphicsConfig&) override { return false; }
    void enable_instant_present() override {}
    void send_dl(const OSTask* task) override {
        if (!ready) return;
        const auto begin = Clock::now();
        try {
            conker_pump_gles_events();
            conker::gles::process(task->t.ucode & 0x00ffffff, task->t.ucode_data & 0x00ffffff,
                task->t.ucode_data_size, task->t.data_ptr & 0x00ffffff, task->t.data_size, task->t.dram_stack_size);
            ++lists;
            dlMicros += std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - begin).count();
            publish();
        } catch (const std::exception& error) { failure(error); }
    }
    void send_dummy_workload(uint32_t address) override { if (ready) conker::gles::dummy(address); }
    void update_screen() override {
        if (!ready) return;
        try { conker_pump_gles_events(); conker::gles::update(); publish(); }
        catch (const std::exception& error) { failure(error); }
    }
    void shutdown() override { conker::gles::stop(); stop_context(this); ready = false; }
    uint32_t get_display_framerate() const override { return 60; }
    float get_resolution_scale() const override {
        const auto s = conker::gles::state();
        return s.renderScale > 0.f ? s.renderScale : 1.f;
    }
};
}
std::unique_ptr<ultramodern::renderer::RendererContext> create_gles_renderer(
    uint8_t* rdram, ultramodern::renderer::WindowHandle window, bool) {
    return std::make_unique<GLESRenderer>(rdram, window);
}
