// SPDX-License-Identifier: GPL-2.0-only
#include "gles_internal.hpp"
#include <DisplayWindow.h>
#include <Config.h>
#include <Graphics/OpenGLContext/GLFunctions.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

class ConkerDisplay final : public DisplayWindow {
    bool _start() override {
        auto& h = conker::gles::host;
        if (!h.start(h.window, m_screenWidth, m_screenHeight)) return false;
        m_bFullscreen = true;
        m_screenRefresh = 60;
        _setBufferSize();
        // A graphics task may arrive before the first VI register snapshot.
        // Depth-only startup lists still need a non-zero allocation scale.
        m_scaleX = m_scaleY = static_cast<float>(m_height) / 240.f;
        initGLFunctions();
        if (!ptrGetString || !ptrGetError || !ptrCreateShader || !ptrBindVertexArray) return false;
        const char* version = reinterpret_cast<const char*>(ptrGetString(GL_VERSION));
        std::fprintf(stderr, "[opengl] version=%s vendor=%s renderer=%s surface=%ux%u\n",
            version ? version : "unknown", ptrGetString(GL_VENDOR), ptrGetString(GL_RENDERER), m_width, m_height);
        // GLideN64 also supports ES2; this integration deliberately requires ES3.
        if (!version || std::strncmp(version, "OpenGL ES 3.", 12) != 0) return false;
        return true;
    }
    void _stop() override { auto& h = conker::gles::host; h.stop(h.window); }
    void _restart() override {}
    void _swapBuffers() override { auto& h = conker::gles::host; h.swap(h.window); }
    void _saveScreenshot() override {}
    void _saveBufferContent(graphics::ObjectHandle, CachedTexture*) override {}
    void _changeWindow() override {}
    bool _resizeWindow() override { return false; }
    void _readScreen(void** out, long* width, long* height) override {
        *width = m_screenWidth; *height = m_screenHeight;
        if (!out) return;
        *out = std::malloc(size_t(*width) * *height * 4);
        if (*out) ptrReadPixels(0, 0, *width, *height, GL_RGBA, GL_UNSIGNED_BYTE, *out);
    }
    void _readScreen2(void* out, int* width, int* height, int) override {
        *width = m_screenWidth; *height = m_screenHeight;
        if (out) ptrReadPixels(0, 0, *width, *height, GL_RGBA, GL_UNSIGNED_BYTE, out);
    }
    graphics::ObjectHandle _getDefaultFramebuffer() override { return graphics::ObjectHandle(0); }
};
DisplayWindow& DisplayWindow::get() { static ConkerDisplay display; return display; }
