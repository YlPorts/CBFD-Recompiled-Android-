// Android host for the existing Conker recompilation. No desktop launcher or menus.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <unistd.h>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>
#include <jni.h>
#include <SDL.h>
#include "recomp.h"
#include "librecomp/game.hpp"
#include "librecomp/rsp.hpp"
#include "ultramodern/ultramodern.hpp"
#include "conker.hpp"
#include "mobile_profile.hpp"
#include "mobile_camera.hpp"
#include "surface_lifecycle.hpp"
#include <android/native_window_jni.h>
#include "rt64_storage.hpp"

extern "C" void recomp_entrypoint(uint8_t*, recomp_context*);
RspExitReason conker_audio_ucode(uint8_t*, uint32_t);
std::unique_ptr<ultramodern::renderer::RendererContext> create_android_renderer(
    uint8_t*, ultramodern::renderer::WindowHandle, bool);

namespace {
constexpr auto game_id = u8"conker.n64.us.1.0";
SDL_Window* game_window = nullptr;
SDL_GameController* controller = nullptr;
uint64_t next_controller_scan = 0;
struct Input { uint16_t buttons = 0; float x = 0, y = 0; } touch, gamepad;
std::mutex input_mutex;

void fr_mode(recomp_context* ctx) { cop0_status_write(ctx, ctx->status_reg | 0x04000000); }
void on_thread(uint8_t*, recomp_context* ctx) { fr_mode(ctx); }
void on_init(uint8_t* rdram, recomp_context* ctx) {
    fr_mode(ctx);
    conker::register_tlb_mapped_code();
    conker::map_tlb_code_pages(rdram);
    MEM_W((int32_t)0x80000310, 0) = 6105;
    ultramodern::set_running_thread_variable((int32_t)0x8002BE00);
}
RspUcodeFunc* rsp(const OSTask* task) { return task->t.type == M_AUDTASK ? conker_audio_ucode : nullptr; }
void error_box(const char* text) {
    std::fprintf(stderr, "[android] %s\n", text);
    std::fflush(stderr);
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Conker Recompiled", text, game_window);
}
void* create_gfx() {
    SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
    SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "0");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) != 0) {
        throw std::runtime_error(SDL_GetError());
    }
    return nullptr;
}
ultramodern::renderer::WindowHandle create_window(void*) {
    SDL_DisplayMode mode{};
    if (SDL_GetDesktopDisplayMode(0, &mode) != 0) { mode.w = 1280; mode.h = 720; }
    game_window = SDL_CreateWindow("Conker Recompiled", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        mode.w, mode.h, SDL_WINDOW_VULKAN | SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!game_window) throw std::runtime_error(SDL_GetError());
    std::fprintf(stderr, "[startup] SDL Vulkan window %dx%d\n", mode.w, mode.h);
    return game_window;
}
void pump(void*) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_CONTROLLERDEVICEADDED || event.type == SDL_CONTROLLERDEVICEREMOVED) next_controller_scan = 0;
        if (event.type == SDL_QUIT || (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_AC_BACK)) {
            ultramodern::quit();
        }
        if (event.type == SDL_APP_WILLENTERBACKGROUND || (event.type == SDL_WINDOWEVENT
            && event.window.event == SDL_WINDOWEVENT_FOCUS_LOST)) {
            std::lock_guard lock(input_mutex); touch = {}; gamepad = {}; conker::camera::release();
        }
    }
    if (controller && !SDL_GameControllerGetAttached(controller)) {
        SDL_GameControllerClose(controller); controller = nullptr;
    }
    if (!controller && SDL_GetTicks64() >= next_controller_scan) {
        next_controller_scan = SDL_GetTicks64() + 2000;
        for (int i = 0; i < SDL_NumJoysticks(); ++i) {
            if (SDL_IsGameController(i)) { controller = SDL_GameControllerOpen(i); break; }
        }
    }
    Input next{};
    conker::camera::padInput.store(0,std::memory_order_relaxed);
    if (controller) {
        auto key = [&](SDL_GameControllerButton button, uint16_t bit) {
            if (SDL_GameControllerGetButton(controller, button)) next.buttons |= bit;
        };
        key(SDL_CONTROLLER_BUTTON_A, 0x8000); key(SDL_CONTROLLER_BUTTON_B, 0x4000);
        key(SDL_CONTROLLER_BUTTON_X, 0x2000); key(SDL_CONTROLLER_BUTTON_START, 0x1000);
        key(SDL_CONTROLLER_BUTTON_LEFTSHOULDER, 0x20); key(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, 0x10);
        key(SDL_CONTROLLER_BUTTON_DPAD_UP, 0x800); key(SDL_CONTROLLER_BUTTON_DPAD_DOWN, 0x400);
        key(SDL_CONTROLLER_BUTTON_DPAD_LEFT, 0x200); key(SDL_CONTROLLER_BUTTON_DPAD_RIGHT, 0x100);
        if (SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_TRIGGERLEFT) > 12000) next.buttons |= 0x2000;
        const int cx = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_RIGHTX);
        const int cy = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_RIGHTY);
        float ax=cx/32767.f,ay=-cy/32767.f;
        const float radius=std::hypot(ax,ay);
        if(radius>.18f) {
            const float factor=std::min(1.f,(radius-.18f)/.82f)/radius;
            conker::camera::padInput.store(conker::camera::pack(ax*factor,ay*factor),std::memory_order_relaxed);
        }
        next.x = std::clamp(SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTX) / 32767.0f, -1.f, 1.f);
        next.y = std::clamp(-SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTY) / 32767.0f, -1.f, 1.f);
        if (std::hypot(next.x, next.y) < .12f) next.x = next.y = 0;
    }
    { std::lock_guard lock(input_mutex); gamepad = next; }
    // librecomp already sleeps 1 ms. Avoid polling Java/SDL/gamepad devices at
    // ~1000 Hz. Touch snapshots go straight through JNI, not through this wait.
    SDL_Delay(3);
}
void poll_input() {}
bool get_input(int port, uint16_t* buttons, float* x, float* y) {
    if (port != 0) return false;
    std::lock_guard lock(input_mutex);
    *buttons = touch.buttons | gamepad.buttons;
    const Input& stick = std::hypot(touch.x, touch.y) > .01f ? touch : gamepad;
    *x = stick.x; *y = stick.y;
    return true;
}
void rumble(int, bool) {}
}

ultramodern::input::connected_device_info_t conker::get_connected_device_info(int port) {
    using namespace ultramodern::input;
    return port == 0 ? connected_device_info_t{Device::Controller, Pak::None}
                     : connected_device_info_t{Device::None, Pak::None};
}
extern "C" JNIEXPORT void JNICALL Java_com_ylports_cbfd_GameActivity_nativeSurface(
    JNIEnv* env, jclass, jobject surface, jint width, jint height) {
    ANativeWindow* window = surface ? ANativeWindow_fromSurface(env, surface) : nullptr;
    conker::android::surfaceRegistry.publish(window, uint32_t(std::max(0,int(width))), uint32_t(std::max(0,int(height))));
    conker::android::surfaceRetryAfter.store(0);
}
extern "C" JNIEXPORT void JNICALL Java_com_ylports_cbfd_GameActivity_nativeForeground(
    JNIEnv*, jclass, jboolean active) {
    conker::android::surfaceRegistry.setForeground(active);
    if (active) conker::android::surfaceRetryAfter.store(0);
}
extern "C" JNIEXPORT void JNICALL Java_com_ylports_cbfd_GameActivity_nativeInput(
    JNIEnv*, jclass, jint buttons, jfloat x, jfloat y) {
    std::lock_guard lock(input_mutex);
    touch = {static_cast<uint16_t>(buttons), std::clamp(x, -1.f, 1.f), std::clamp(y, -1.f, 1.f)};
}
extern "C" JNIEXPORT void JNICALL Java_com_ylports_cbfd_GameActivity_nativeCamera(JNIEnv*, jclass, jfloat x, jfloat y) {
    conker::camera::touchInput.store(conker::camera::pack(x,y),std::memory_order_relaxed);
}
extern "C" JNIEXPORT void JNICALL Java_com_ylports_cbfd_GameActivity_nativeRequestQuit(JNIEnv*, jclass) {
    ultramodern::quit();
}

extern "C" __attribute__((visibility("default"))) int SDL_main(int argc, char** argv) {
    try {
        std::filesystem::path rom, state;
        for (int i = 1; i < argc; ++i) {
            const std::string arg(argv[i]);
            if (arg == "--rom" && i + 1 < argc) rom = argv[++i];
            else if (arg == "--data" && i + 1 < argc) state = argv[++i];
            else throw std::runtime_error("Argumento de arranque no válido.");
        }
        if (rom.empty() || state.empty()) throw std::runtime_error("Falta la ROM o la carpeta privada de guardado.");
        std::filesystem::create_directories(state);
        std::filesystem::current_path(state);
        std::freopen((state / "last-run.log").c_str(), "w", stderr);
        setvbuf(stderr, nullptr, _IOLBF, 0);
        dup2(fileno(stderr), STDOUT_FILENO);
        setvbuf(stdout, nullptr, _IOLBF, 0);
        std::ofstream(state / "running.marker") << "Conker Android 0.1.10-alpha\n";
        std::fprintf(stderr, "[startup] Conker Android 0.1.10-alpha ARM64; build=native1080-010; target=60; aspect=Expand; internal=fixed1080; noDRS\n");
        // Validate storage on SDL_main before spawning RT64's graphics thread.
        // This uses --data from Android getFilesDir(), never HOME or /data.
        const auto renderer_path = conker::android::rt64_data_path(state);
        std::fprintf(stderr, "[startup] RT64 private storage ready: %s\n", renderer_path.c_str());
        recomp::register_config_path(state);
        ultramodern::renderer::set_graphics_config(mobile_profile());
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
        game.thread_create_callback = on_thread;
        std::fprintf(stderr, "[startup] Registering Conker and overlays\n");
        recomp::register_game(game);
        conker::register_overlays();
        conker::register_mod_exports();
        std::fprintf(stderr, "[startup] Validating imported ROM\n");
        if (recomp::select_rom(rom, game_id) != recomp::RomValidationError::Good) {
            throw std::runtime_error("ROM incompatible: necesita el código original USA; los cambios de recursos son compatibles.");
        }
        char program[] = "conker-android", flag[] = "--game", name[] = "conker";
        char* args[] = {program, flag, name, nullptr};
        recomp::Configuration cfg{};
        cfg.argc = 3; cfg.argv = args;
        cfg.project_version = recomp::Version{0, 1, 10};
        cfg.rsp_callbacks.get_rsp_microcode = rsp;
        cfg.audio_callbacks = {conker::audio::queue_samples, conker::audio::get_frames_remaining, conker::audio::set_frequency};
        cfg.renderer_callbacks.create_render_context = create_android_renderer;
        cfg.gfx_callbacks = {create_gfx, create_window, pump};
        cfg.input_callbacks = {poll_input, get_input, rumble, conker::get_connected_device_info};
        cfg.error_handling_callbacks = {error_box};
        std::fprintf(stderr, "[startup] Starting native runtime\n");
        recomp::start(cfg);
        std::fprintf(stderr, "[shutdown] Runtime finished; save thread joined\n");
        std::error_code remove_error;
        std::filesystem::remove(state / "running.marker", remove_error);
        if (controller) { SDL_GameControllerClose(controller); controller = nullptr; }
        if (game_window) { SDL_DestroyWindow(game_window); game_window = nullptr; }
        SDL_Quit();
        return 0;
    } catch (const std::exception& error) {
        error_box(error.what());
        return 1;
    }
}
