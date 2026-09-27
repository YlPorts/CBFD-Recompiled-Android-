// Synthetic display lists, no ROM or generated game code. Exercises the same
// shared renderer as the APK and verifies actual ES3 framebuffer pixels.
#include "test_surface.hpp"
#include <array>
#include <cstring>
#include <stdexcept>

namespace {
std::vector<uint8_t> ram(16 * 1024 * 1024);
std::array<uint32_t, 14> vi{};
uint32_t pc;
void word(uint32_t address, uint32_t value) { std::memcpy(ram.data() + address, &value, 4); }
void command(uint32_t a, uint32_t b) { word(pc, a); word(pc + 4, b); pc += 8; }
void begin() { pc = 0x4000; }
void submit() {
    command(0xdf000000, 0);
    conker::gles::process(0x1000, 0x2000, 2048, 0x4000, pc - 0x4000, 2048);
}
void rect(int x0, int y0, int x1, int y1) {
    command(0x64000003, 0);
    command((uint32_t(uint16_t(x0 * 4)) << 16) | uint16_t(y0 * 4),
        (uint32_t(uint16_t(x1 * 4)) << 16) | uint16_t(y1 * 4));
}
void checkPixel(const Surface& s, int x, int y, int r, int g, int b, const char* what) {
    const auto* p = s.pixels.data() + (size_t(s.height - 1 - y) * s.width + x) * 4;
    if (std::abs(int(p[0]) - r) > 5 || std::abs(int(p[1]) - g) > 5 || std::abs(int(p[2]) - b) > 5) {
        std::ofstream image("synthetic-failure.ppm",std::ios::binary);
        image << "P6\n" << s.width << ' ' << s.height << "\n255\n";
        for (int row = int(s.height)-1; row >= 0; --row) for (uint32_t col = 0; col < s.width; ++col)
            image.write(reinterpret_cast<const char*>(s.pixels.data()+(size_t(row)*s.width+col)*4),3);
        image.close();
        std::fprintf(stderr, "%s: (%d,%d) expected %d,%d,%d got %u,%u,%u\n", what,x,y,r,g,b,p[0],p[1],p[2]);
        throw std::runtime_error("Framebuffer pixel mismatch");
    }
}
}
int main() {
    Surface surface; surface.captureFiles = false;
    // Public microcode identifier only; no Nintendo microcode is included.
    const char id[] = "RSP Gfx ucode F3DEX.NoN fifo 2.08 test";
    for (size_t i = 0; i < sizeof(id); ++i) ram[(0x2000 + i) ^ 3] = id[i];
    if (!conker::gles::start(ram.data(),vi.data(),{&surface,Surface::start,Surface::stop,Surface::swap},"synthetic-cache"))
        return 2;
    // Reproduce the first-task-before-VI startup ordering that formerly made
    // a zero-height depth texture. The debug callback + swap check catch it.
    begin(); command(0xfe000000,0x180000); submit(); conker::gles::update();
    vi = {2,0x100000,320,0,0,0,525,0,0,(108u<<16)|748u,(34u<<16)|514u,0,512,1024};
    const int16_t viewport[] = {640,480,511,0,640,480,511,0};
    for (size_t i = 0; i < 8; ++i) std::memcpy(ram.data()+((0x3000+i*2)^2), &viewport[i], 2);
    begin();
    command(0xdc080008,0x3000); // full-width viewport, not the reset sub-viewport
    command(0xff10013f,0x100000); // RGBA16, width 320
    command(0xed000000,(1280u<<12)|960u); // full scissor
    command(0xef300000,0); // fill cycle
    command(0xf7000000,0x07c107c1); // opaque green
    rect(0,0,319,239);
    command(0xe7000000,0);
    command(0xf7000000,0xf801f801); // red, signed rect clipped at left
    rect(-20,60,160,180);
    // Matrix metadata consumes its parameter word, not a fake RDP command.
    command(0x6400000c,0); command(0xffabcdef,0); command(0x6400000d,0);
    submit(); conker::gles::update();
    checkPixel(surface,surface.width*3/4,surface.height/2,0,255,0,"background");
    checkPixel(surface,surface.width/4,surface.height/2,255,0,0,"signed PC rectangle");
    const int contentLeft = std::max(0, (int(surface.width)-int(surface.height)*4/3)/2);
    checkPixel(surface,contentLeft+surface.height/12,surface.height/2,255,0,0,"signed left edge");
    checkPixel(surface,surface.width/16,surface.height/8,0,255,0,"full-width clear");
    auto state = conker::gles::state();
    if (state.extendedRectangles != 2 || state.ignoredMatrixGroups != 2 || state.viWidth != 320 || !state.conkerMicrocode)
        throw std::runtime_error("Extended command stream lost synchronization");

    begin(); command(0xe7000000,0);
    command(0xef000000,0x00404240); // one cycle: src alpha, dst (1-alpha)
    // (zero-zero)*zero + primitive, for both color and alpha cycles.
    command(0xfc000000|(8u<<20)|(31u<<15)|(7u<<12)|(7u<<9)|(8u<<5)|31u,
        (8u<<28)|(8u<<24)|(7u<<21)|(7u<<18)|(3u<<15)|(7u<<12)|(3u<<9)|(3u<<6)|(7u<<3)|3u);
    command(0xfa000000,0xff000080); // half-alpha red on the green right half
    rect(200,60,300,180);
    submit(); conker::gles::update();
    checkPixel(surface,surface.width*3/4,surface.height/2,128,127,0,"translucent blend");
    state = conker::gles::state();
    if (state.colorHeight != surface.height || state.colorAllocationHeight < state.colorHeight)
        throw std::runtime_error("Visible internal height must match the fixed surface target");
    std::printf("PASS real GLES: early task, signed rectangles, clipping, matrix tags, translucent blend; surface=%ux%u colorViewport=%ux%u allocationHeight=%u\n",
        state.width,state.height,state.colorWidth,state.colorHeight,state.colorAllocationHeight);
    conker::gles::stop();
}
