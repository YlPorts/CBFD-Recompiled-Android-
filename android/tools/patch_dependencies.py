#!/usr/bin/env python3
"""Apply only the platform glue required to build the PC 0.1.2 RT64 path on Android.

The game, N64ModernRuntime and RT64 behavior come from the PC 0.1.2 sources and
their upstream patches. This file deliberately does not apply Android-specific
filtering, culling, blend, interpolation, quality or performance patches.
"""
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[2]

def replace(path, before, after, count=1):
    file = ROOT / path
    text = file.read_text()
    if after in text:
        return
    if text.count(before) != count:
        raise SystemExit(f"PC 0.1.2 dependency changed; refusing ambiguous Android glue: {path}")
    file.write_text(text.replace(before, after, count))

def apply_patch(path):
    patch = ROOT / path
    reverse = subprocess.run(
        ["git", "apply", "--reverse", "--check", str(patch)],
        cwd=ROOT, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    if reverse.returncode == 0:
        return
    subprocess.run(["git", "apply", "--check", str(patch)], cwd=ROOT, check=True)
    subprocess.run(["git", "apply", str(patch)], cwd=ROOT, check=True)

def main():
    rt64 = "tools/rt64/CMakeLists.txt"

    # file_to_c must execute on the Linux build host, not on the Android target.
    replace(rt64, "add_subdirectory(src/tools/file_to_c)", """if (ANDROID)
    if (NOT CONKER_HOST_FILE_TO_C OR NOT EXISTS "${CONKER_HOST_FILE_TO_C}")
        message(FATAL_ERROR "Build the host file_to_c tool and set CONKER_HOST_FILE_TO_C.")
    endif()
    add_executable(file_to_c IMPORTED GLOBAL)
    set_target_properties(file_to_c PROPERTIES IMPORTED_LOCATION "${CONKER_HOST_FILE_TO_C}")
else()
    add_subdirectory(src/tools/file_to_c)
endif()""")

    # Android supplies a tiny NFD target because the launcher uses Android's
    # Storage Access Framework instead of desktop native-file-dialog.
    replace(rt64, "add_subdirectory(src/contrib/nativefiledialog-extended)", """if (NOT TARGET nfd)
    add_subdirectory(src/contrib/nativefiledialog-extended)
endif()""")

    # DXC is a host tool during cross compilation.
    replace(rt64, "elseif (APPLE)\n    if (CMAKE_SYSTEM_PROCESSOR",
        "elseif (CMAKE_HOST_SYSTEM_NAME STREQUAL \"Darwin\")\n    if (CMAKE_SYSTEM_PROCESSOR")
    replace(rt64, """else()
    if (CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
        set (DXC "LD_LIBRARY_PATH=${PROJECT_SOURCE_DIR}/src/contrib/dxc/lib/x64" "${PROJECT_SOURCE_DIR}/src/contrib/dxc/bin/x64/dxc-linux")
    else()
        set (DXC "LD_LIBRARY_PATH=${PROJECT_SOURCE_DIR}/src/contrib/dxc/lib/arm64" "${PROJECT_SOURCE_DIR}/src/contrib/dxc/bin/arm64/dxc-linux")
    endif()
endif()""", """else()
    if (CMAKE_HOST_SYSTEM_PROCESSOR STREQUAL "x86_64")
        set (DXC ${CMAKE_COMMAND} -E env "LD_LIBRARY_PATH=${PROJECT_SOURCE_DIR}/src/contrib/dxc/lib/x64" "${PROJECT_SOURCE_DIR}/src/contrib/dxc/bin/x64/dxc-linux")
    else()
        set (DXC ${CMAKE_COMMAND} -E env "LD_LIBRARY_PATH=${PROJECT_SOURCE_DIR}/src/contrib/dxc/lib/arm64" "${PROJECT_SOURCE_DIR}/src/contrib/dxc/bin/arm64/dxc-linux")
    endif()
endif()""")

    # Reuse the PC SDL/Vulkan window path on Android.
    replace(rt64, 'if (CMAKE_SYSTEM_NAME MATCHES "Linux" AND RT64_SDL_WINDOW_VULKAN)',
        'if ((CMAKE_SYSTEM_NAME MATCHES "Linux" OR ANDROID) AND RT64_SDL_WINDOW_VULKAN)', count=2)
    replace(rt64, """if (NOT ANDROID)
    target_link_libraries(rt64 ${SDL2_LIBRARIES})
endif()""", """if (ANDROID)
    target_link_libraries(rt64 SDL2::SDL2 android log)
else()
    target_link_libraries(rt64 ${SDL2_LIBRARIES})
endif()""")
    replace("tools/rt64/src/contrib/plume/CMakeLists.txt",
        'cmake_dependent_option(PLUME_SDL_VULKAN_ENABLED "Enable SDL Vulkan integration" OFF IS_LINUX OFF)',
        'cmake_dependent_option(PLUME_SDL_VULKAN_ENABLED "Enable SDL Vulkan integration" OFF "IS_LINUX OR ANDROID" OFF)')
    replace(rt64, "\nelse()\n    find_package(SDL2 REQUIRED)\nendif()",
        "\nelif (TARGET SDL2::SDL2)\n    set(SDL2_INCLUDE_DIRS \"$<TARGET_PROPERTY:SDL2::SDL2,INTERFACE_INCLUDE_DIRECTORIES>\")\n    set(SDL2_LIBRARIES SDL2::SDL2)\nelse()\n    find_package(SDL2 REQUIRED)\nendif()")
    replace("tools/rt64/src/hle/rt64_application_window.cpp",
        '#   elif defined(__ANDROID__)\n        static_assert(false && "Android unimplemented");\n#   elif defined(__linux__) || defined(__APPLE__)',
        '#   elif defined(__ANDROID__) || defined(__linux__) || defined(__APPLE__)')
    replace("tools/rt64/src/contrib/plume/plume_render_interface_types.h",
        '#elif defined(__ANDROID__)\n    typedef ANativeWindow* RenderWindow;\n#elif defined(PLUME_SDL_VULKAN_ENABLED)\n    typedef SDL_Window *RenderWindow;',
        '#elif defined(PLUME_SDL_VULKAN_ENABLED)\n    typedef SDL_Window *RenderWindow;\n#elif defined(__ANDROID__)\n    typedef ANativeWindow* RenderWindow;')

    # Android WSI negotiation only. No shader/material/culling/performance changes.
    apply_patch("android/patches/vulkan-surface.patch")
    print("PC 0.1.2 RT64 preserved; Android cross-compile + Vulkan WSI glue applied.")

if __name__ == "__main__":
    main()
