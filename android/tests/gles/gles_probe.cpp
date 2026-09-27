// Real OpenGL ES test surface, with the same renderer core and game as Android.
// The ROM and framebuffer captures stay in the local, ignored build directory.
#include "conker.hpp"
#include "test_surface.hpp"
#include "mobile_metrics.hpp"
#include "gles/gles_bridge.hpp"
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <vector>
#include <execinfo.h>

namespace {

class Probe final : public ultramodern::renderer::RendererContext {
    Surface surface;
    bool ready;
public:
    Probe(uint8_t* ram) {
        ready = conker::gles::start(ram,reinterpret_cast<uint32_t*>(ultramodern::renderer::get_vi_regs()),
            {&surface,Surface::start,Surface::stop,Surface::swap},"gles-cache");
        setup_result = ready ? ultramodern::renderer::SetupResult::Success : ultramodern::renderer::SetupResult::GraphicsDeviceNotFound;
        chosen_api = ultramodern::renderer::GraphicsApi::OpenGL;
        conker::mobile::metrics.horizontalAspect.store(conker::gles::state().horizontalScale);
    }
    bool valid() override { return ready; }
    bool update_config(const ultramodern::renderer::GraphicsConfig&,const ultramodern::renderer::GraphicsConfig&) override { return false; }
    void enable_instant_present() override {}
    void send_dl(const OSTask* t) override { conker::gles::process(t->t.ucode&0xffffff,t->t.ucode_data&0xffffff,t->t.ucode_data_size,t->t.data_ptr&0xffffff,t->t.data_size,t->t.dram_stack_size); }
    void send_dummy_workload(uint32_t a) override { conker::gles::dummy(a); }
    void update_screen() override { conker::gles::update(); }
    void shutdown() override { std::fprintf(stderr,"[gles-probe] shutdown frames=%u\n",surface.frames); conker::gles::stop(); }
    uint32_t get_display_framerate() const override { return 60; }
    float get_resolution_scale() const override { return 1.f; }
};
}
std::unique_ptr<ultramodern::renderer::RendererContext> conker::create_null_renderer(uint8_t* ram,ultramodern::renderer::WindowHandle,bool) {
    return std::make_unique<Probe>(ram);
}
