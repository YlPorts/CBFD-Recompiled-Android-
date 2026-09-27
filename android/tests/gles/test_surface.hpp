#pragma once
#include "gles/gles_bridge.hpp"
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <vector>
#include <execinfo.h>
struct Surface {
    bool captureFiles{true};
    std::vector<unsigned char> pixels;
    EGLDisplay display = EGL_NO_DISPLAY;
    EGLSurface surface = EGL_NO_SURFACE;
    EGLContext context = EGL_NO_CONTEXT;
    uint32_t frames{}, width{}, height{};
    static bool start(void* data, uint32_t& w, uint32_t& h) {
        auto& s = *static_cast<Surface*>(data);
        auto getDisplay = reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(eglGetProcAddress("eglGetPlatformDisplayEXT"));
        s.display = getDisplay(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, nullptr);
        if (!eglInitialize(s.display, nullptr, nullptr) || !eglBindAPI(EGL_OPENGL_ES_API)) return false;
        const EGLint attrs[] = {EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
            EGL_RED_SIZE,8,EGL_GREEN_SIZE,8,EGL_BLUE_SIZE,8,EGL_ALPHA_SIZE,8,EGL_DEPTH_SIZE,24,EGL_NONE};
        EGLConfig cfg{}; EGLint count{};
        if (!eglChooseConfig(s.display, attrs, &cfg, 1, &count) || count < 1) return false;
        w = s.width = std::getenv("CONKER_PROBE_WIDTH") ? std::atoi(std::getenv("CONKER_PROBE_WIDTH")) : 640;
        h = s.height = std::getenv("CONKER_PROBE_HEIGHT") ? std::atoi(std::getenv("CONKER_PROBE_HEIGHT")) : 480;
        const EGLint pb[] = {EGL_WIDTH,int(w),EGL_HEIGHT,int(h),EGL_NONE};
        const EGLint ctx[] = {EGL_CONTEXT_CLIENT_VERSION,3,EGL_NONE};
        s.surface = eglCreatePbufferSurface(s.display,cfg,pb);
        s.context = eglCreateContext(s.display,cfg,EGL_NO_CONTEXT,ctx);
        if (!eglMakeCurrent(s.display,s.surface,s.surface,s.context)) return false;
        using Debug = void(*)(unsigned,unsigned,unsigned,unsigned,int,const char*,const void*);
        const auto callback = reinterpret_cast<void(*)(Debug,const void*)>(eglGetProcAddress("glDebugMessageCallback"));
        const auto enable = reinterpret_cast<void(*)(unsigned)>(eglGetProcAddress("glEnable"));
        if (callback) {
            enable(0x92E0); enable(0x8242);
            callback([](unsigned,unsigned type,unsigned,unsigned,int,const char* text,const void*) {
                if (type == 0x824c) {
                    std::fprintf(stderr,"[gles-debug] %s\n",text);
                    void* frames[20]; backtrace_symbols_fd(frames,backtrace(frames,20),2);
                }
            },nullptr);
        }
        return true;
    }
    static void stop(void* data) {
        auto& s = *static_cast<Surface*>(data);
        eglMakeCurrent(s.display,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);
        eglDestroyContext(s.display,s.context); eglDestroySurface(s.display,s.surface); eglTerminate(s.display);
    }
    static void swap(void* data) {
        auto& s = *static_cast<Surface*>(data);
        ++s.frames;
        auto getError = reinterpret_cast<unsigned(*)()>(eglGetProcAddress("glGetError"));
        if (const auto error = getError()) { std::fprintf(stderr,"[gles-probe] GL error=%x\n",error); std::abort(); }
        if (!s.captureFiles || s.frames == 1 || s.frames % 120 == 0) {
            const auto read = reinterpret_cast<void(*)(int,int,int,int,unsigned,unsigned,void*)>(eglGetProcAddress("glReadPixels"));
            s.pixels.resize(size_t(s.width)*s.height*4);
            auto& pixels = s.pixels;
            read(0,0,s.width,s.height,0x1908,0x1401,pixels.data());
            if (s.captureFiles) {
                std::ofstream file("gles-frame-" + std::to_string(s.frames) + ".ppm",std::ios::binary);
                file << "P6\n" << s.width << ' ' << s.height << "\n255\n";
                for (int y=int(s.height)-1;y>=0;--y) for(uint32_t x=0;x<s.width;++x)
                    file.write(reinterpret_cast<char*>(pixels.data()+(size_t(y)*s.width+x)*4),3);
                const auto state = conker::gles::state();
                std::fprintf(stderr,"[gles-probe] frames=%u dls=%llu vi=%ux%u extended=%llu\n",s.frames,
                    (unsigned long long)state.displayLists,state.viWidth,state.viHeight,(unsigned long long)state.extendedRectangles);
            }
        }
        eglSwapBuffers(s.display,s.surface);
    }
};
