#pragma once
#include <string_view>
namespace conker::mobile {
enum class Renderer { Vulkan, OpenGL };
// Written once before the graphics/game threads start; never changed live.
inline Renderer renderer = Renderer::Vulkan;
inline bool select_renderer(std::string_view value) {
    if (value == "vulkan") renderer = Renderer::Vulkan;
    else if (value == "opengl") renderer = Renderer::OpenGL;
    else return false;
    return true;
}
inline bool use_opengl() { return renderer == Renderer::OpenGL; }
inline const char* renderer_name() { return use_opengl() ? "OpenGL ES 3 / GLideN64" : "Vulkan / RT64"; }
}
