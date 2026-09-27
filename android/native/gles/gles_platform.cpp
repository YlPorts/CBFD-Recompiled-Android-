// SPDX-License-Identifier: GPL-2.0-only
#include "gles_internal.hpp"
#include <PluginAPI.h>
#include <Config.h>
#include <N64.h>
#include <Log.h>
#include <mupenplus/GLideN64_mupenplus.h>
#include <cstdarg>
#include <cstdio>
#include <cwchar>
#include <stdexcept>

Config config;
int PluginAPI::InitiateGFX(const GFX_INFO& info) {
    _initiateGFX(info);
    return 1;
}
void PluginAPI::GetUserDataPath(wchar_t* path) {
    std::mbstowcs(path, conker::gles::storage.c_str(), PLUGIN_PATH_SIZE - 1);
    path[PLUGIN_PATH_SIZE - 1] = 0;
}
void PluginAPI::GetUserCachePath(wchar_t* path) { GetUserDataPath(path); }
void PluginAPI::FindPluginPath(wchar_t* path) { GetUserDataPath(path); }

void Config_LoadConfig() {
    // RSP_Init already selected Conker's framebuffer fixes before this call.
    const auto hacks = config.generalEmulation.hacks;
    config.resetToDefaults();
    config.generalEmulation.hacks = hacks;
    config.video.threadedVideo = 0; // The runtime owns the graphics thread/context.
    config.video.asyncShaderCompilation = 0;
    config.video.verticalSync = 1;
    config.texture.bilinearMode = BILINEAR_3POINT;
    config.generalEmulation.enableHybridFilter = 0;
    config.frameBufferEmulation.aspect = Config::aAdjust43;
    config.frameBufferEmulation.nativeResFactor = 0; // Native surface, no integer supersampling.
    config.frameBufferEmulation.copyToRDRAM = Config::ctSync;
    config.frameBufferEmulation.copyDepthToRDRAM = Config::cdSoftwareRender;
    config.generalEmulation.enableShadersStorage = 1;
    config.validate();
}

void LogDebug(const char* file, int line, u16 level, const char* format, ...) {
    if (level > LOG_WARNING) return;
    std::fprintf(stderr, "[opengl] %s:%d ", file, line);
    va_list args; va_start(args, format); std::vfprintf(stderr, format, args); va_end(args);
    std::fputc('\n', stderr);
}

// Upstream's optional threaded wrapper declares these video callbacks even when
// disabled. Context management belongs to ConkerDisplay, so fail closed if a
// future change accidentally activates that second context owner.
static m64p_error invalidVideoCall() { return M64ERR_INVALID_STATE; }
ptr_VidExt_Init CoreVideo_Init = invalidVideoCall;
ptr_VidExt_Quit CoreVideo_Quit = invalidVideoCall;
ptr_VidExt_SetVideoMode CoreVideo_SetVideoMode = [](int,int,int,m64p_video_mode,m64p_video_flags) { return M64ERR_INVALID_STATE; };
ptr_VidExt_SetVideoModeWithRate CoreVideo_SetVideoModeWithRate = [](int,int,int,int,m64p_video_mode,m64p_video_flags) { return M64ERR_INVALID_STATE; };
ptr_VidExt_GL_SetAttribute CoreVideo_GL_SetAttribute = [](m64p_GLattr,int) { return M64ERR_INVALID_STATE; };
ptr_VidExt_GL_GetAttribute CoreVideo_GL_GetAttribute = [](m64p_GLattr,int*) { return M64ERR_INVALID_STATE; };
ptr_VidExt_GL_SwapBuffers CoreVideo_GL_SwapBuffers = invalidVideoCall;
