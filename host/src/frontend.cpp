// The window build's front end: RecompFrontend's launcher, settings and mod menus
// (recompui) and remappable keyboard/controller input (recompinput), on an SDL
// window rendered by RT64 through recompui's renderer.

#include <cstdio>
#include <fstream>
#include <mutex>
#include <string>
#include <vector>

#define SDL_MAIN_HANDLED
#include <SDL.h>
#if defined(_WIN32) || defined(__APPLE__)
// Only Windows and macOS need the native window handle. On Linux this header brings
// in X11's, whose None macro breaks ultramodern's Device::None.
#include <SDL_syswm.h>
#endif

#include "nfd.h"

#include "librecomp/game.hpp"
#include "recompinput/input_events.h"
#include "recompinput/input_state.h"
#include "recompinput/players.h"
#include "recompinput/profiles.h"
#include "recompui/program_config.h"
#include "recompui/recompui.h"
#include "recompui/renderer.h"
#include "base/ui_launcher.h"
#include "util/file.h"

#include "conker.hpp"

// recompui's launcher shows the first entry (ui_launcher.cpp declares it extern).
std::vector<recomp::GameEntry> supported_games;
// The game window, which recompui also uses (ui_state.cpp declares it extern).
SDL_Window* window = nullptr;

namespace {
    std::vector<char> thumbnail;

    // The launcher's Version and Add ROM options, and the window title that names the
    // version in play. The title is set on the main thread (update_gfx), as macOS requires.
    recompui::GameOption* version_option = nullptr;
    recompui::GameOption* add_rom_option = nullptr;
    std::mutex title_mutex;
    std::string pending_title;

    void* create_gfx() {
        SDL_SetHint(SDL_HINT_WINDOWS_DPI_AWARENESS, "permonitorv2");
        SDL_SetHint(SDL_HINT_GAMECONTROLLER_USE_BUTTON_LABELS, "0");
        SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS4_RUMBLE, "1");
        SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS5_RUMBLE, "1");
        SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1");
        SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
        // Debugging aid: CONKER_NO_CONTROLLER=1 ignores game controllers, e.g. for test
        // runs while someone else is playing with the controller on the same machine.
        Uint32 subsystems = SDL_INIT_VIDEO;
        if (SDL_getenv("CONKER_NO_CONTROLLER") == nullptr) {
            subsystems |= SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK | SDL_INIT_HAPTIC;
        }
        if (SDL_Init(subsystems) != 0) {
            std::fprintf(stderr, "[frontend] SDL_Init failed: %s\n", SDL_GetError());
        }
        // The file dialogs (Load ROM, mods). Only after SDL: on macOS, NFD_Init creates the
        // application object if it doesn't exist yet and makes it an accessory app, and SDL
        // then leaves it that way (no Dock icon, and the window opens behind the terminal).
        NFD_Init();
        return nullptr;
    }

    ultramodern::renderer::WindowHandle create_window(void*) {
        uint32_t flags = SDL_WINDOW_RESIZABLE;
#if defined(RT64_SDL_WINDOW_VULKAN)
        flags |= SDL_WINDOW_VULKAN;
#elif defined(__APPLE__)
        flags |= SDL_WINDOW_METAL;
#endif
        window = SDL_CreateWindow(conker::program_name, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            1600, 900, flags);
        if (window == nullptr) {
            std::fprintf(stderr, "[frontend] SDL_CreateWindow failed: %s\n", SDL_GetError());
            return {};
        }
#if defined(_WIN32)
        SDL_SysWMinfo info;
        SDL_VERSION(&info.version);
        SDL_GetWindowWMInfo(window, &info);
        return ultramodern::renderer::WindowHandle{ info.info.win.window, GetCurrentThreadId() };
#elif defined(__APPLE__)
        // RT64 renders with Metal into the CAMetalLayer of a view added to the window.
        SDL_SysWMinfo info;
        SDL_VERSION(&info.version);
        SDL_GetWindowWMInfo(window, &info);
        SDL_MetalView view = SDL_Metal_CreateView(window);
        return ultramodern::renderer::WindowHandle{ info.info.cocoa.window, SDL_Metal_GetLayer(view) };
#else
        return ultramodern::renderer::WindowHandle{ window };
#endif
    }

    void update_gfx(void*) {
        recompinput::handle_events();
        std::string title;
        {
            std::lock_guard lock(title_mutex);
            title.swap(pending_title);
        }
        if (!title.empty() && window != nullptr) {
            SDL_SetWindowTitle(window, title.c_str());
        }
    }

    // Shows which version of the ROM is in play: on the launcher's Version option and in
    // the window title. Version and Add ROM are hidden until there's a ROM (Start Game's
    // Load ROM picks the first).
    void show_version() {
        std::string name = conker::roms::current_name();
        for (recompui::GameOption* option : { version_option, add_rom_option }) {
            if (option != nullptr) {
                if (name.empty()) {
                    option->display_hide();
                }
                else {
                    option->display_show();
                }
            }
        }
        if ((version_option != nullptr) && !name.empty()) {
            version_option->set_title("Version: " + name);
        }
        std::printf("[frontend] ROM version in play: %s (%zu kept)\n", name.empty() ? "none" : name.c_str(),
            conker::roms::version_count());
        std::lock_guard lock(title_mutex);
        pending_title = name.empty() ? std::string(conker::program_name)
                                     : std::string(conker::program_name) + " (" + name + ")";
    }

    // The Version option: switches to the next version kept.
    void on_version_selected() {
        if (conker::roms::version_count() < 2) {
            recompui::message_box("This is the only version of the ROM loaded. Add another with Add ROM "
                "(a ROM hack that only changes the game's assets, such as an uncensored one) to switch between them.");
            return;
        }
        if (conker::roms::switch_to_next()) {
            show_version();
        }
    }

    // The Add ROM option: loads another ROM, which is kept as a version and put in play.
    void on_add_rom_selected() {
        recompui::file::open_file_dialog([](bool success, const std::filesystem::path& path) {
            if (!success) {
                return;
            }
            recomp::RomValidationError result = recomp::select_rom(path, supported_games[0].game_id);
            if (result == recomp::RomValidationError::IncorrectVersion) {
                // Conker's Bad Fur Day, but not a US ROM the game plays: say which region it is.
                std::string region = conker::roms::region_of(path);
                if (!region.empty() && region != "US") {
                    std::string text = "This is the " + region + " version of Conker's Bad Fur Day. Only the US "
                        "version is supported (and ROM hacks of it that only change the game's assets).";
                    recompui::message_box(text.c_str());
                    return;
                }
            }
            if (result != recomp::RomValidationError::Good) {
                recompui::message_box(conker::rom_error_text(result));
            }
            // The launcher's update picks the new version up once it's written.
        });
    }

    // The launcher's options: RecompFrontend's usual ones, with Version and Add ROM after Start Game.
    void init_launcher(recompui::LauncherMenu* menu) {
        const recomp::GameEntry& game = supported_games[0];
        // Here rather than at startup, so a ROM given with --rom is already the one stored.
        conker::roms::init(recomp::get_config_path() / game.stored_filename());
        recompui::GameOptionsMenu* options = menu->init_game_options_menu(
            game.game_id, game.mod_game_id, game.display_name, game.thumbnail_bytes);
        // Lower than recompui's 25% from the bottom: with Version and Add ROM, seven options
        // would reach up into the title.
        options->set_bottom(10.0f, recompui::Unit::Percent);
        recompui::update_game_mod_id(game.mod_game_id);
        options->add_start_game_or_load_rom_option();
        version_option = options->add_option("Version", on_version_selected);
        add_rom_option = options->add_option("Add ROM", on_add_rom_selected);
        options->add_setup_controls_option();
        options->add_settings_option();
        options->add_mods_option();
        options->add_exit_option();
        show_version();
    }

    void update_launcher(recompui::LauncherMenu*) {
        if (conker::roms::update()) {
            show_version();
        }
    }

    std::unique_ptr<ultramodern::renderer::RendererContext> create_render_context(
        uint8_t* rdram, ultramodern::renderer::WindowHandle window_handle, bool developer_mode) {
        return recompui::renderer::create_render_context(rdram, window_handle,
            ultramodern::renderer::PresentationMode::PresentEarly, developer_mode);
    }

}

void conker::frontend::on_vi() {
    recompinput::update_rumble();
}

ultramodern::input::connected_device_info_t conker::frontend::get_connected_device_info(int controller_num) {
    if (recompinput::players::is_single_player_mode() || recompinput::players::get_player_is_assigned(controller_num)) {
        return { ultramodern::input::Device::Controller, ultramodern::input::Pak::RumblePak };
    }
    return { ultramodern::input::Device::None, ultramodern::input::Pak::None };
}

std::u8string conker::program_id() {
    return SDL_getenv("CONKER_TEST_PROFILE") != nullptr ? u8"ConkerRecompiledTest" : u8"ConkerRecompiled";
}

void conker::frontend::init(recomp::GameEntry& game) {
    recompui::programconfig::set_program_name(program_name);
    recompui::programconfig::set_program_id(program_id());

    // The launcher's picture of the game.
    std::ifstream file(recompui::file::get_asset_path("thumbnail.png"), std::ios::binary);
    thumbnail.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    game.thumbnail_bytes = std::span<const char>(thumbnail);
    supported_games.push_back(game);

    // Versions of the ROM (the original, an uncensored ROM hack...) to switch between.
    recompui::register_launcher_init_callback(init_launcher);
    recompui::register_launcher_update_callback(update_launcher);

    recompui::register_primary_font("InterVariable.ttf", "Inter Variable");
    recompui::register_ui_exports();
    recompinput::players::set_single_player_mode(true);
    conker::init_config();
}

void conker::frontend::set_callbacks(recomp::Configuration& cfg) {
    cfg.renderer_callbacks.create_render_context = create_render_context;
    cfg.gfx_callbacks = { create_gfx, create_window, update_gfx };
    cfg.input_callbacks = { recompinput::poll_inputs, recompinput::profiles::get_n64_input, recompinput::set_rumble,
                            conker::get_connected_device_info };
    cfg.error_handling_callbacks.message_box = recompui::message_box;
}
