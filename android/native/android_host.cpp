// Android host for the direct PC 0.1.2 port.
// Game/recompiler/widescreen/interpolation behavior comes from upstream PC.
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unistd.h>
#include <jni.h>
#include <SDL.h>

#include "recomp.h"
#include "librecomp/game.hpp"
#include "librecomp/rsp.hpp"
#include "ultramodern/ultramodern.hpp"
#include "conker.hpp"
#include "mobile_profile.hpp"
#include "rt64_storage.hpp"

extern "C" void recomp_entrypoint(uint8_t*, recomp_context*);
RspExitReason conker_audio_ucode(uint8_t*, uint32_t);
std::unique_ptr<ultramodern::renderer::RendererContext> create_android_renderer(
    uint8_t*, ultramodern::renderer::WindowHandle, bool);

// Upstream host/src/widescreen.cpp deliberately references this exact PC global.
SDL_Window* window = nullptr;

namespace {
constexpr auto game_id = u8"conker.n64.us.1.0";
SDL_GameController* controller = nullptr;
uint64_t nextControllerScan = 0;
std::atomic<uint32_t> viCount{0};

struct Input { uint16_t buttons = 0; float x = 0, y = 0; };
Input touch, gamepad;
std::mutex inputMutex;

void set_fr_mode(recomp_context* ctx) {
    constexpr uint32_t STATUS_FR = 0x04000000;
    cop0_status_write(ctx, ctx->status_reg | STATUS_FR);
}
void on_thread_create(uint8_t*, recomp_context* ctx) { set_fr_mode(ctx); }
void on_init(uint8_t* rdram, recomp_context* ctx) {
    set_fr_mode(ctx);
    conker::register_tlb_mapped_code();
    conker::map_tlb_code_pages(rdram);
    MEM_W((int32_t)0x80000310, 0) = 6105;
    ultramodern::set_running_thread_variable((int32_t)0x8002BE00);
}
RspUcodeFunc* get_rsp_microcode(const OSTask* task) {
    if (task->t.type == M_AUDTASK) return conker_audio_ucode;
    std::fprintf(stderr, "[host] no RSP microcode for task type %u\n", unsigned(task->t.type));
    return nullptr;
}
void vi_callback() { ++viCount; }

void error_box(const char* text) {
    std::fprintf(stderr, "[android] %s\n", text ? text : "Unknown error");
    std::fflush(stderr);
    if (window) SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Conker Recompiled", text, window);
}

void* create_gfx() {
    SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
    SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "0");
    SDL_SetHint(SDL_HINT_GAMECONTROLLER_USE_BUTTON_LABELS, "0");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER | SDL_INIT_HAPTIC) != 0)
        throw std::runtime_error(SDL_GetError());
    return nullptr;
}

ultramodern::renderer::WindowHandle create_window(void*) {
    SDL_DisplayMode mode{};
    if (SDL_GetDesktopDisplayMode(0, &mode) != 0) { mode.w = 1280; mode.h = 720; }
    window = SDL_CreateWindow("Conker's Bad Fur Day: Recompiled",
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, mode.w, mode.h,
        SDL_WINDOW_VULKAN | SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!window) throw std::runtime_error(SDL_GetError());
    std::fprintf(stderr, "[startup] PC 0.1.2 SDL Vulkan window %dx%d\n", mode.w, mode.h);
    return window;
}

void scan_controller() {
    if (controller && !SDL_GameControllerGetAttached(controller)) {
        SDL_GameControllerClose(controller);
        controller = nullptr;
    }
    if (controller || SDL_GetTicks64() < nextControllerScan) return;
    nextControllerScan = SDL_GetTicks64() + 2000;
    for (int i = 0; i < SDL_NumJoysticks(); ++i) {
        if (SDL_IsGameController(i)) {
            controller = SDL_GameControllerOpen(i);
            break;
        }
    }
}

void pump(void*) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_CONTROLLERDEVICEADDED || event.type == SDL_CONTROLLERDEVICEREMOVED)
            nextControllerScan = 0;
        if (event.type == SDL_QUIT) ultramodern::quit();
        if (event.type == SDL_APP_WILLENTERBACKGROUND ||
            (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_FOCUS_LOST)) {
            std::lock_guard lock(inputMutex);
            touch = {};
            gamepad = {};
        }
    }

    scan_controller();
    Input next{};
    if (controller) {
        auto key = [&](SDL_GameControllerButton button, uint16_t bit) {
            if (SDL_GameControllerGetButton(controller, button)) next.buttons |= bit;
        };
        key(SDL_CONTROLLER_BUTTON_A, 0x8000);
        key(SDL_CONTROLLER_BUTTON_B, 0x4000);
        key(SDL_CONTROLLER_BUTTON_X, 0x2000);
        key(SDL_CONTROLLER_BUTTON_START, 0x1000);
        key(SDL_CONTROLLER_BUTTON_LEFTSHOULDER, 0x0020);
        key(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, 0x0010);
        key(SDL_CONTROLLER_BUTTON_DPAD_UP, 0x0800);
        key(SDL_CONTROLLER_BUTTON_DPAD_DOWN, 0x0400);
        key(SDL_CONTROLLER_BUTTON_DPAD_LEFT, 0x0200);
        key(SDL_CONTROLLER_BUTTON_DPAD_RIGHT, 0x0100);
        if (SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_TRIGGERLEFT) > 12000) next.buttons |= 0x2000;

        // PC/original behavior: right stick is a convenient mapping to N64 C-buttons.
        const int rx = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_RIGHTX);
        const int ry = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_RIGHTY);
        constexpr int cThreshold = 12000;
        if (rx < -cThreshold) next.buttons |= 0x0002;
        if (rx >  cThreshold) next.buttons |= 0x0001;
        if (ry < -cThreshold) next.buttons |= 0x0008;
        if (ry >  cThreshold) next.buttons |= 0x0004;

        next.x = std::clamp(SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTX) / 32767.0f, -1.f, 1.f);
        next.y = std::clamp(-SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTY) / 32767.0f, -1.f, 1.f);
        if (std::hypot(next.x, next.y) < .12f) next.x = next.y = 0;
    }
    {
        std::lock_guard lock(inputMutex);
        gamepad = next;
    }
    SDL_Delay(3);
}

void poll_input() {}
bool get_input(int port, uint16_t* buttons, float* x, float* y) {
    if (port != 0) return false;
    std::lock_guard lock(inputMutex);
    *buttons = touch.buttons | gamepad.buttons;
    const Input& stick = std::hypot(touch.x, touch.y) > .01f ? touch : gamepad;
    *x = stick.x;
    *y = stick.y;
    return true;
}
void set_rumble(int port, bool enabled) {
    if (port == 0 && controller)
        SDL_GameControllerRumble(controller, enabled ? 0xFFFF : 0, enabled ? 0xFFFF : 0, enabled ? 1000 : 0);
}
}

ultramodern::input::connected_device_info_t conker::get_connected_device_info(int port) {
    using namespace ultramodern::input;
    return port == 0 ? connected_device_info_t{Device::Controller, Pak::RumblePak}
                     : connected_device_info_t{Device::None, Pak::None};
}

extern "C" JNIEXPORT void JNICALL Java_com_ylports_cbfd_GameActivity_nativeInput(
    JNIEnv*, jclass, jint buttons, jfloat x, jfloat y) {
    std::lock_guard lock(inputMutex);
    touch = {static_cast<uint16_t>(buttons), std::clamp(x, -1.f, 1.f), std::clamp(y, -1.f, 1.f)};
}

extern "C" JNIEXPORT void JNICALL Java_com_ylports_cbfd_GameActivity_nativeRequestQuit(JNIEnv*, jclass) {
    ultramodern::quit();
}

extern "C" JNIEXPORT jstring JNICALL Java_com_ylports_cbfd_GameActivity_nativeCaptureDiagnostics(
    JNIEnv* env, jclass) {
    const auto& p = conker::android::pcGraphics;
    std::ostringstream out;
    out << "[direct-pc-port] upstream=0.1.2 commit=152cc380 renderer=RT64/Vulkan"
        << " resolution=" << p.resolution << " downsampling=" << p.downsampling
        << " aspect=" << p.aspect << " msaa=" << p.msaa
        << " refresh=" << p.refresh << '/' << p.refreshManual
        << " hud=" << p.hud << " hpfb=" << p.highPrecision
        << " viCount=" << viCount.load() << "\n";
    return env->NewStringUTF(out.str().c_str());
}

extern "C" __attribute__((visibility("default"))) int SDL_main(int argc, char** argv) {
    try {
        std::filesystem::path rom, state;
        for (int i = 1; i < argc; ++i) {
            std::string_view arg(argv[i]);
            if (arg == "--rom" && i + 1 < argc) rom = argv[++i];
            else if (arg == "--data" && i + 1 < argc) state = argv[++i];
            else if (i + 1 < argc && conker::android::set_pc_graphics_option(arg, argv[i + 1])) ++i;
            else throw std::runtime_error("Invalid Android launch argument.");
        }
        if (rom.empty() || state.empty()) throw std::runtime_error("ROM or private data path missing.");

        std::filesystem::create_directories(state);
        std::filesystem::current_path(state);
        std::freopen((state / "last-run.log").c_str(), "w", stderr);
        setvbuf(stderr, nullptr, _IOLBF, 0);
        dup2(fileno(stderr), STDOUT_FILENO);
        setvbuf(stdout, nullptr, _IOLBF, 0);

        std::ofstream(state / "running.marker") << "Conker PC 0.1.2 direct Android port\n";
        std::fprintf(stderr,
            "[startup] Conker PC 0.1.2 direct Android port ARM64; build=pc-012-direct; upstream=152cc380; renderer=RT64/Vulkan\n");

        recomp::register_config_path(state);
        ultramodern::renderer::set_graphics_config(conker::android::pc_graphics_profile());

        recomp::GameEntry game{};
        game.rom_hash = conker::roms::us_rom_hash;
        game.accept_rom = conker::roms::accept;
        game.internal_name = "CONKER BFD";
        game.display_name = "Conker's Bad Fur Day";
        game.game_id = game_id;
        game.mod_game_id = "conker";
        game.save_type = recomp::SaveType::Eep16k;
        game.is_enabled = true;
        game.has_compressed_code = true;
        game.decompression_routine = conker::decompress_rom;
        game.entrypoint_address = (gpr)(int32_t)0x80001000u;
        game.entrypoint = recomp_entrypoint;
        game.on_init_callback = on_init;
        game.thread_create_callback = on_thread_create;

        recomp::register_game(game);
        conker::register_overlays();
        conker::register_mod_exports();

        if (recomp::select_rom(rom, game_id) != recomp::RomValidationError::Good)
            throw std::runtime_error("Incompatible ROM: PC 0.1.2 requires the US code version.");

        char program[] = "conker-android";
        char flag[] = "--game";
        char name[] = "conker";
        char* args[] = {program, flag, name, nullptr};

        recomp::Configuration cfg{};
        cfg.argc = 3;
        cfg.argv = args;
        cfg.project_version = recomp::Version{0, 1, 2};
        cfg.rsp_callbacks.get_rsp_microcode = get_rsp_microcode;
        cfg.audio_callbacks = {conker::audio::queue_samples, conker::audio::get_frames_remaining, conker::audio::set_frequency};
        cfg.renderer_callbacks.create_render_context = create_android_renderer;
        cfg.gfx_callbacks = {create_gfx, create_window, pump};
        cfg.input_callbacks = {poll_input, get_input, set_rumble, conker::get_connected_device_info};
        cfg.events_callbacks = {vi_callback, nullptr};
        cfg.error_handling_callbacks = {error_box};

        std::fprintf(stderr, "[startup] Starting native runtime with PC project_version=0.1.2\n");
        recomp::start(cfg);
        std::fprintf(stderr, "[shutdown] Runtime finished; save thread joined\n");

        std::error_code ec;
        std::filesystem::remove(state / "running.marker", ec);
        if (controller) { SDL_GameControllerClose(controller); controller = nullptr; }
        if (window) { SDL_DestroyWindow(window); window = nullptr; }
        SDL_Quit();
        return 0;
    }
    catch (const std::exception& error) {
        error_box(error.what());
        return 1;
    }
}
