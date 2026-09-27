cmake_minimum_required(VERSION 3.22)
project(ConkerAndroid LANGUAGES C CXX)
if(NOT ANDROID OR NOT ANDROID_ABI STREQUAL "arm64-v8a")
    message(FATAL_ERROR "This target requires Android NDK arm64-v8a.")
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

get_filename_component(ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
set(GENERATED "${ROOT}/RecompiledFuncs")
if(NOT EXISTS "${GENERATED}/inputs.sha256")
    message(FATAL_ERROR "Missing recompiled PC 0.1.2 game sources. Regenerate RecompiledFuncs from your US ROM.")
endif()

set(RECOMP_INPUTS conker.toml recomp/conker.us.syms.toml recomp/code_rewrites.txt recomp/n64recomp.patch recomp/unpack_rom.py recomp/emit_tlb_pages.py recomp/audio_ucode.toml)
file(READ "${GENERATED}/inputs.sha256" ACTUAL)
set(EXPECTED "")
foreach(input IN LISTS RECOMP_INPUTS)
    file(READ "${ROOT}/${input}" TEXT)
    string(REPLACE "\r" "" TEXT "${TEXT}")
    string(SHA256 HASH "${TEXT}")
    string(APPEND EXPECTED "${HASH} ${input}\n")
endforeach()
if(NOT ACTUAL STREQUAL EXPECTED)
    message(FATAL_ERROR "RecompiledFuncs does not match PC 0.1.2 inputs; run recomp/recompile.py again.")
endif()

set(SDL_SHARED ON CACHE BOOL "" FORCE)
set(SDL_STATIC OFF CACHE BOOL "" FORCE)
set(SDL_TEST OFF CACHE BOOL "" FORCE)
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/.deps/SDL" "${CMAKE_CURRENT_BINARY_DIR}/SDL")

add_subdirectory("${ROOT}/tools/N64ModernRuntime" "${CMAKE_CURRENT_BINARY_DIR}/runtime")

set(RT64_STATIC ON CACHE BOOL "" FORCE)
set(RT64_SDL_WINDOW_VULKAN ON CACHE BOOL "" FORCE)
set(ZSTD_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_DICTBUILDER OFF CACHE BOOL "" FORCE)
set(PLUME_SDL_VULKAN_ENABLED ON CACHE BOOL "" FORCE)
add_compile_definitions(HLSL_CPU RT64_SDL_WINDOW_VULKAN PLUME_SDL_VULKAN_ENABLED)

set(RT64_DIR "${ROOT}/tools/rt64")
add_library(nfd STATIC native/nfd_android.cpp)
target_include_directories(nfd PUBLIC "${RT64_DIR}/src/contrib/nativefiledialog-extended/src/include")
set(CONKER_HOST_FILE_TO_C "${CMAKE_CURRENT_SOURCE_DIR}/.host/file_to_c" CACHE FILEPATH "Host shader conversion tool")
add_subdirectory("${RT64_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/rt64")
target_include_directories(rt64 PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/native")
target_include_directories(plume PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/native")

file(GLOB FUNCS CONFIGURE_DEPENDS "${GENERATED}/funcs_*.c")
if(NOT FUNCS)
    message(FATAL_ERROR "No PC 0.1.2 recompiled Conker functions found.")
endif()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${GENERATED}/inputs.sha256")

# Compile the PC-generated functions unchanged. PC 0.1.2 hooks in conker.toml
# call the shared host implementations directly.
add_library(ConkerGame STATIC ${FUNCS} "${GENERATED}/tlb_pages.c")
target_include_directories(ConkerGame PRIVATE "${GENERATED}" "${ROOT}/tools/N64ModernRuntime/N64Recomp/include")
target_compile_options(ConkerGame PRIVATE -include setjmp.h -fno-strict-aliasing
    -Wno-implicit-function-declaration -Wno-unused-label -Wno-unused-variable -Wno-unused-but-set-variable)

add_library(main SHARED
    native/android_host.cpp
    native/rt64_renderer.cpp
    native/audio_output.cpp
    "${ROOT}/host/src/overlays.cpp"
    "${ROOT}/host/src/ultra_extras.cpp"
    "${ROOT}/host/src/mod_api.cpp"
    "${ROOT}/host/src/widescreen.cpp"
    "${ROOT}/host/src/interpolation.cpp"
    "${ROOT}/host/src/rom_versions.cpp"
    "${GENERATED}/rsp/audio_ucode.cpp")

target_compile_definitions(main PRIVATE CONKER_RT64)
target_compile_options(main PRIVATE -fno-strict-aliasing -fms-extensions)
target_include_directories(main PRIVATE
    "${ROOT}/host/src" "${GENERATED}"
    "${ROOT}/tools/N64ModernRuntime/N64Recomp/include"
    "${RT64_DIR}/src" "${RT64_DIR}/src/contrib" "${RT64_DIR}/src/contrib/plume"
    "${RT64_DIR}/src/contrib/imgui" "${RT64_DIR}/src/contrib/hlslpp/include"
    "${RT64_DIR}/src/contrib/mupen64plus-core/src/api")

target_link_libraries(main PRIVATE ConkerGame librecomp ultramodern rt64 SDL2::SDL2 android log)
target_link_options(main PRIVATE "-Wl,-z,max-page-size=16384" "-Wl,--threads=1")
