#!/usr/bin/env python3
"""Apply narrow Android build fixes on top of the pinned, Conker-patched runtime.
No checkout reset, network access, ROM processing or desktop-source deletion.
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
        raise SystemExit(f"Dependency changed; refusing an ambiguous patch: {path} / {before[:80]!r}")
    file.write_text(text.replace(before, after, count))


def apply_base():
    rt64 = 'tools/rt64/CMakeLists.txt'
    replace(rt64, 'add_subdirectory(src/tools/file_to_c)', '''if (ANDROID)
    if (NOT CONKER_HOST_FILE_TO_C OR NOT EXISTS "${CONKER_HOST_FILE_TO_C}")
        message(FATAL_ERROR "Build the host file_to_c tool and set CONKER_HOST_FILE_TO_C.")
    endif()
    add_executable(file_to_c IMPORTED GLOBAL)
    set_target_properties(file_to_c PROPERTIES IMPORTED_LOCATION "${CONKER_HOST_FILE_TO_C}")
else()
    add_subdirectory(src/tools/file_to_c)
endif()''')
    replace(rt64, 'add_subdirectory(src/contrib/nativefiledialog-extended)', '''if (NOT TARGET nfd)
    add_subdirectory(src/contrib/nativefiledialog-extended)
endif()''')
    replace(rt64, 'elseif (APPLE)\n    if (CMAKE_SYSTEM_PROCESSOR', 'elseif (CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")\n    if (CMAKE_SYSTEM_PROCESSOR')
    replace(rt64, '''else()
    if (CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
        set (DXC "LD_LIBRARY_PATH=${PROJECT_SOURCE_DIR}/src/contrib/dxc/lib/x64" "${PROJECT_SOURCE_DIR}/src/contrib/dxc/bin/x64/dxc-linux")
    else()
        set (DXC "LD_LIBRARY_PATH=${PROJECT_SOURCE_DIR}/src/contrib/dxc/lib/arm64" "${PROJECT_SOURCE_DIR}/src/contrib/dxc/bin/arm64/dxc-linux")
    endif()
endif()''', '''else()
    if (CMAKE_HOST_SYSTEM_PROCESSOR STREQUAL "x86_64")
        set (DXC ${CMAKE_COMMAND} -E env "LD_LIBRARY_PATH=${PROJECT_SOURCE_DIR}/src/contrib/dxc/lib/x64" "${PROJECT_SOURCE_DIR}/src/contrib/dxc/bin/x64/dxc-linux")
    else()
        set (DXC ${CMAKE_COMMAND} -E env "LD_LIBRARY_PATH=${PROJECT_SOURCE_DIR}/src/contrib/dxc/lib/arm64" "${PROJECT_SOURCE_DIR}/src/contrib/dxc/bin/arm64/dxc-linux")
    endif()
endif()''')
    replace(rt64, 'if (CMAKE_SYSTEM_NAME MATCHES "Linux" AND RT64_SDL_WINDOW_VULKAN)', 'if ((CMAKE_SYSTEM_NAME MATCHES "Linux" OR ANDROID) AND RT64_SDL_WINDOW_VULKAN)', count=2)
    replace(rt64, '''if (NOT ANDROID)
    target_link_libraries(rt64 ${SDL2_LIBRARIES})
endif()''', '''if (ANDROID)
    target_link_libraries(rt64 SDL2::SDL2 android log)
else()
    target_link_libraries(rt64 ${SDL2_LIBRARIES})
endif()''')
    replace('tools/rt64/src/contrib/plume/CMakeLists.txt',
        'cmake_dependent_option(PLUME_SDL_VULKAN_ENABLED "Enable SDL Vulkan integration" OFF IS_LINUX OFF)',
        'cmake_dependent_option(PLUME_SDL_VULKAN_ENABLED "Enable SDL Vulkan integration" OFF "IS_LINUX OR ANDROID" OFF)')
    replace(rt64, '\nelse()\n    find_package(SDL2 REQUIRED)\nendif()', '\nelseif (TARGET SDL2::SDL2)\n    set(SDL2_INCLUDE_DIRS "$<TARGET_PROPERTY:SDL2::SDL2,INTERFACE_INCLUDE_DIRECTORIES>")\n    set(SDL2_LIBRARIES SDL2::SDL2)\nelse()\n    find_package(SDL2 REQUIRED)\nendif()')
    replace('tools/rt64/src/hle/rt64_application_window.cpp',
        '#   elif defined(__ANDROID__)\n        static_assert(false && "Android unimplemented");\n#   elif defined(__linux__) || defined(__APPLE__)',
        '#   elif defined(__ANDROID__) || defined(__linux__) || defined(__APPLE__)')
    replace('tools/rt64/src/contrib/plume/plume_render_interface_types.h',
        '#elif defined(__ANDROID__)\n    typedef ANativeWindow* RenderWindow;\n#elif defined(PLUME_SDL_VULKAN_ENABLED)\n    typedef SDL_Window *RenderWindow;',
        '#elif defined(PLUME_SDL_VULKAN_ENABLED)\n    typedef SDL_Window *RenderWindow;\n#elif defined(__ANDROID__)\n    typedef ANativeWindow* RenderWindow;')
    patch = ROOT / 'android/patches/vulkan-surface.patch'
    reverse = subprocess.run(['git', 'apply', '--reverse', '--check', str(patch)],
                             cwd=ROOT, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    if reverse.returncode != 0:
        subprocess.run(['git', 'apply', '--check', str(patch)], cwd=ROOT, check=True)
        subprocess.run(['git', 'apply', str(patch)], cwd=ROOT, check=True)
    mobile_patch = ROOT / 'android/patches/mobile-performance-v2.patch'
    def check(patch, reverse=False):
        return subprocess.run(['git','apply',*(['--reverse'] if reverse else []),'--check',str(patch)],
            cwd=ROOT,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL).returncode==0
    if not check(mobile_patch, True):
        if not check(mobile_patch):
            old = ROOT / 'android/patches/mobile-performance.patch'
            if not check(old, True):
                raise SystemExit('Mobile dependency files differ; refusing to overwrite local changes.')
            subprocess.run(['git','apply','--reverse',str(old)],cwd=ROOT,check=True)
        subprocess.run(['git','apply','--check',str(mobile_patch)],cwd=ROOT,check=True)
        subprocess.run(['git','apply',str(mobile_patch)],cwd=ROOT,check=True)
    print('Android surface, mobile matching and clipped bounds patches applied.')

def apply_017():
    # Temporarily remove only this known, fully matched layer. Older patch layers
    # may otherwise fail reverse-check because 0.1.7 touches the same lines.
    # git apply is atomic for each layer; there is no checkout/reset/clean.
    patch = ROOT / 'android/patches/native-render-017.patch'
    def check(reverse=False):
        return subprocess.run(['git','apply',*(['--reverse'] if reverse else []),'--check',str(patch)],
            cwd=ROOT,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL).returncode == 0
    removed = check(True)
    if removed:
        subprocess.run(['git','apply','--reverse',str(patch)],cwd=ROOT,check=True)
    try:
        apply_base()
        if not check():
            raise SystemExit('0.1.7 native dependencies differ; refusing to overwrite local changes.')
        subprocess.run(['git','apply',str(patch)],cwd=ROOT,check=True)
    except BaseException:
        # Restore a removed layer when it still applies after the failed step.
        if removed and check():
            subprocess.run(['git','apply',str(patch)],cwd=ROOT,check=True)
        raise
    print('0.1.7 screen-Z, ordered vertex edits, matrix-vector path and edge-clear layer applied.')



def apply_018():
    # Layer 0.1.8 depends on 0.1.7. Remove only a fully matched known patch;
    # never reset a dependency or silently discard unrelated local edits.
    patch = ROOT / 'android/patches/mali-blend-surface-018.patch'
    def check(reverse=False):
        return subprocess.run(['git','apply',*(['--reverse'] if reverse else []),'--check',str(patch)],
            cwd=ROOT,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL).returncode == 0
    removed = check(True)
    if removed:
        subprocess.run(['git','apply','--reverse',str(patch)],cwd=ROOT,check=True)
    try:
        apply_017()
        if not check():
            raise SystemExit('0.1.8 dependencies differ; refusing to overwrite local changes.')
        subprocess.run(['git','apply',str(patch)],cwd=ROOT,check=True)
    except BaseException:
        if removed and check():
            subprocess.run(['git','apply',str(patch)],cwd=ROOT,check=True)
        raise
    print('0.1.8 capability-based blend, retained Android surface and queue-wakeup layer applied.')

def apply_010():
    patch = ROOT / 'android/patches/native-1080-010.patch'
    def check(reverse=False):
        return subprocess.run(['git','apply',*(['--reverse'] if reverse else []),'--check',str(patch)],
            cwd=ROOT,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL).returncode == 0
    removed = check(True)
    if removed:
        subprocess.run(['git','apply','--reverse',str(patch)],cwd=ROOT,check=True)
    try:
        apply_018()
        if not check():
            raise SystemExit('0.1.10 dependencies differ; refusing to overwrite local changes.')
        subprocess.run(['git','apply',str(patch)],cwd=ROOT,check=True)
    except BaseException:
        if removed and check():
            subprocess.run(['git','apply',str(patch)],cwd=ROOT,check=True)
        raise
    print('0.1.10 VI-aware 1080-line render and lean coverage shaders applied.')

def apply_011():
    patch = ROOT / 'android/patches/visual-bounds-textures-011.patch'
    def check(reverse=False):
        return subprocess.run(['git','apply',*(['--reverse'] if reverse else []),'--check',str(patch)],
            cwd=ROOT,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL).returncode == 0
    removed = check(True)
    if removed:
        subprocess.run(['git','apply','--reverse',str(patch)],cwd=ROOT,check=True)
    try:
        apply_010()
        if not check():
            raise SystemExit('0.1.11 dependencies differ; refusing to overwrite local changes.')
        subprocess.run(['git','apply',str(patch)],cwd=ROOT,check=True)
    except BaseException:
        if removed and check():
            subprocess.run(['git','apply',str(patch)],cwd=ROOT,check=True)
        raise
    print('0.1.11 wide framebuffer bounds and finite texture LOD layer applied.')

def main():
    patch = ROOT / 'android/patches/visibility-workers-012.patch'
    def check(reverse=False):
        return subprocess.run(['git','apply',*(['--reverse'] if reverse else []),'--check',str(patch)],
            cwd=ROOT,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL).returncode == 0
    removed = check(True)
    if removed:
        subprocess.run(['git','apply','--reverse',str(patch)],cwd=ROOT,check=True)
    try:
        apply_011()
        if not check():
            raise SystemExit('0.1.12 dependencies differ; refusing to overwrite local changes.')
        subprocess.run(['git','apply',str(patch)],cwd=ROOT,check=True)
    except BaseException:
        if removed and check():
            subprocess.run(['git','apply',str(patch)],cwd=ROOT,check=True)
        raise
    print('0.1.12 CPU-frustum aspect synchronization and shader-worker lifecycle layer applied.')

if __name__ == '__main__':
    main()
