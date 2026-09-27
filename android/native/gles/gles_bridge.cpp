// SPDX-License-Identifier: GPL-2.0-only
#include "gles_internal.hpp"
#include <PluginAPI.h>
#include <N64.h>
#include <RSP.h>
#include <GBI.h>
#include <gSP.h>
#include <gDP.h>
#include <VI.h>
#include <DisplayWindow.h>
#include <FrameBuffer.h>
#include <Graphics/OpenGLContext/GLFunctions.h>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <stdexcept>

namespace conker::gles {
Host host;
std::string storage;
State stats;
static uint8_t dmem[4096]{}, imem[4096]{}, header[64]{};
static uint32_t mi{}, dpc[8]{};
static bool opened{};
static void interrupt() {} // The runtime sends DP completion after process() returns.

// The PC port emits a small, known subset of RT64's extended GBI. Decode its
// signed rectangles here; matrix tags carry interpolation metadata only.
static void extended(uint32_t w0, uint32_t w1) {
    const uint32_t op = w0 & 0x00ffffff;
    uint32_t extra = op == 2 ? 16 : (op == 3 || op == 12 ? 8 : 0);
    const uint32_t pc = RSP.PC[RSP.PCi];
    if (!isRDRAMRangeValid(pc, extra)) throw std::runtime_error("Extended GBI outside RDRAM");
    auto word = [&](uint32_t offset) { uint32_t value; std::memcpy(&value, RDRAM + pc + offset, 4); return value; };
    if (op == 2 || op == 3) {
        const uint32_t a = word(0), b = word(4);
        const float x0 = int16_t(a >> 16) / 4.f, y0 = int16_t(a) / 4.f;
        const float x1 = int16_t(b >> 16) / 4.f, y1 = int16_t(b) / 4.f;
        if (op == 2) {
            const uint32_t uv = word(8), delta = word(12);
            gDPTextureRectangle(x0, y0, x1, y1, w1 & 7, int16_t(uv >> 16), int16_t(uv),
                int16_t(delta >> 16) / 1024.f, int16_t(delta) / 1024.f, false);
        } else gDPFillRectangle(int(x0), int(y0), int(x1), int(y1));
        ++stats.extendedRectangles;
    } else if (op == 12 || op == 13) {
        ++stats.ignoredMatrixGroups;
    } else if (op != 0x33) {
        // Never interpret a parameter word as a triangle or silently lose sync.
        throw std::runtime_error("Unsupported extended GBI command in OpenGL renderer");
    }
    // 0x33 is the PC pause-background zoom hint; GLideN64 handles that copy itself.
    RSP.PC[RSP.PCi] += extra;
}

bool start(uint8_t* rdram, uint32_t* vi, const Host& platform, const char* path) {
    host = platform; storage = path; stats = {};
    std::filesystem::create_directories(storage);
    // Header is word-swapped, like game memory. This selects GLideN64's CBFD fixes.
    std::memset(header, ' ', sizeof(header));
    const char name[] = "CONKER BFD          ";
    for (uint32_t i = 0; i < 20; ++i) header[(32 + i) ^ 3] = name[i];
    header[0x3e ^ 3] = 'E';
    GFX_INFO info{};
    info.HEADER = header; info.RDRAM = rdram; info.DMEM = dmem; info.IMEM = imem;
    info.MI_INTR_REG = &mi; info.CheckInterrupts = interrupt;
    info.DPC_START_REG = &dpc[0]; info.DPC_END_REG = &dpc[1]; info.DPC_CURRENT_REG = &dpc[2];
    info.DPC_STATUS_REG = &dpc[3]; info.DPC_CLOCK_REG = &dpc[4]; info.DPC_BUFBUSY_REG = &dpc[5];
    info.DPC_PIPEBUSY_REG = &dpc[6]; info.DPC_TMEM_REG = &dpc[7];
    info.VI_STATUS_REG = vi; info.VI_ORIGIN_REG = vi + 1; info.VI_WIDTH_REG = vi + 2;
    info.VI_INTR_REG = vi + 3; info.VI_V_CURRENT_LINE_REG = vi + 4; info.VI_TIMING_REG = vi + 5;
    info.VI_V_SYNC_REG = vi + 6; info.VI_H_SYNC_REG = vi + 7; info.VI_LEAP_REG = vi + 8;
    info.VI_H_START_REG = vi + 9; info.VI_V_START_REG = vi + 10; info.VI_V_BURST_REG = vi + 11;
    info.VI_X_SCALE_REG = vi + 12; info.VI_Y_SCALE_REG = vi + 13;
    // Vanilla Conker occupies 8 MiB; the PC iris hook uses 0x00f00000..0x00f10000.
    RDRAMSize = 0x00ffffff;
    api().InitiateGFX(info);
    opened = api().RomOpen() != 0;
    if (!opened) host.stop(host.window);
    return opened;
}

void process(uint32_t ucode, uint32_t ucodeData, uint32_t ucodeSize,
             uint32_t data, uint32_t dataSize, uint32_t stackSize) {
    auto put = [](uint32_t off, uint32_t value) { std::memcpy(dmem + off, &value, 4); };
    put(0xfd0, ucode); put(0xfd8, ucodeData); put(0xfdc, ucodeSize);
    put(0xfe4, stackSize); put(0xff0, data); put(0xff4, dataSize);
    // Load first, then install extensions. RSP_ProcessDList sees the same ucode
    // addresses and does not reset the table while processing this task.
    if (ucode != RSP.uc_start || ucodeData != RSP.uc_dstart)
        gSPLoadUcodeEx(ucode, ucodeData, ucodeSize);
    GBI.cmd[0x64] = extended;
    // Match the PC port's forceBranch policy for CBFD's distance LOD lists.
    // Geometry clipping and the game's camera-plane tests still run normally.
    if (GBI.getMicrocodeType() == F3DEX2CBFD)
        GBI.cmd[G_BRANCH_Z] = [](uint32_t, uint32_t) { gSPBranchList(gDP.half_1); };
    api().ProcessDList();
    ++stats.displayLists;
}
void update() { api().UpdateScreen(); }
void dummy(uint32_t address) { gDPSetColorImage(G_IM_FMT_RGBA, G_IM_SIZ_16b, 320, address); }
void stop() { if (opened) { api().RomClosed(); opened = false; } }
State state() {
    stats.width = dwnd().getWidth(); stats.height = dwnd().getHeight();
    stats.viWidth = VI.width; stats.viHeight = VI.height;
    stats.swaps = dwnd().getBuffersSwapCount(); stats.triangles = gSP.tri_num;
    stats.horizontalScale = 1.f / dwnd().getAdjustScale();
    stats.renderScale = dwnd().getScaleY();
    stats.conkerMicrocode = GBI.getMicrocodeType() == F3DEX2CBFD;
    if (const auto buffer = frameBufferList().getCurrent(); buffer && buffer->m_pTexture) {
        stats.colorWidth = buffer->m_pTexture->width;
        stats.colorHeight = std::min<uint32_t>(buffer->m_pTexture->height, static_cast<uint32_t>(VI.real_height * buffer->m_scale));
        stats.colorAllocationHeight = buffer->m_pTexture->height;
    }
    return stats;
}
}
