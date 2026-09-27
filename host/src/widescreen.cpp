// Widescreen fixes, called from hooks in conker.toml.
//
// RT64 widens the 3D view, but 2D texture rectangles are drawn in the N64's
// 320-wide screen space, and the G_TEXRECT command can't hold coordinates left
// of 0. So the game culls and clips its screen-space sprites (bubbles, bees,
// sparkles: func_15130A9C) to the 4:3 screen. These hooks let those sprites
// reach into the widened area: the cull bounds are pushed outwards, and the
// rectangle is emitted as RT64's extended texture rectangle, which takes signed
// coordinates, in the same display list space as the game's own rectangle.

#include <algorithm>
#include <cstdint>
#include <cstring>

#include "recomp.h"

namespace {
    // How far past each 4:3 edge sprites are kept, in N64 screen pixels: enough
    // for a 32:9 window. Sprites outside the actual window cost a draw, nothing more.
    constexpr float cull_margin = 160.0f;

    // RT64's extended GBI (tools/rt64/include/rt64_extended_gbi.h) for F3DEX2,
    // whose no-op (0xE0) carries RT64's hooks.
    constexpr uint32_t rt64_hook_opcode = 0xE0;
    constexpr uint32_t rt64_hook_magic = 0x525464;
    constexpr uint32_t rt64_hook_op_enable = 0x1;
    constexpr uint32_t rt64_extended_opcode = 0x64;
    constexpr uint32_t g_ex_texrect_v1 = 0x000002;
    constexpr uint32_t g_ex_origin_none = 0x800;

    float stack_float(uint8_t* rdram, gpr sp, int32_t offset) {
        uint32_t word = (uint32_t)MEM_W(offset, sp);
        float value;
        std::memcpy(&value, &word, sizeof(value));
        return value;
    }

    void put_command(uint8_t* rdram, gpr& dl, uint32_t w0, uint32_t w1) {
        MEM_W(0, dl) = (int32_t)w0;
        MEM_W(4, dl) = (int32_t)w1;
        dl += 8;
    }
}

// func_15130A9C at 0x15130CEC / 0x15130D08: $f6 and $f10 hold the camera's left
// and right sprite bounds (camera + 0x2C / + 0x30), about to be compared with the
// sprite's right and left edges.
extern "C" void conker_widen_sprite_cull_left(uint8_t* rdram, recomp_context* ctx) {
    ctx->f6.fl -= cull_margin;
}

extern "C" void conker_widen_sprite_cull_right(uint8_t* rdram, recomp_context* ctx) {
    ctx->f10.fl += cull_margin;
}

// func_15130A9C at 0x15130DA0: the game has just written a G_RDPPIPESYNC at $v0,
// the first command of the sprite. RT64 doesn't need syncs; put the enable of its
// extended GBI there instead (RT64 turns it off at the start of every display
// list), so the extended rectangle below costs no extra display list space. The
// sprites are drawn into display lists allocated to fit what the game writes.
extern "C" void conker_enable_extended_gbi(uint8_t* rdram, recomp_context* ctx) {
    gpr dl = ctx->r2;
    put_command(rdram, dl, (rt64_hook_opcode << 24) | rt64_hook_magic, (rt64_hook_op_enable << 28) | rt64_extended_opcode);
}

// func_15130A9C at 0x15131168: $v0 is where the sprite's G_TEXRECT goes (three
// commands: the rectangle and its two G_RDPHALF words). The game clamps its corners
// to 0 and moves the texture start instead; emit the unclamped rectangle as an
// extended one, which is also three commands. RT64 (rt64.patch) clips a rectangle
// whose scissor spans the frame at the edges of the widened frame. The function
// then continues at its end (L_15131360), which returns sp + 0x100.
extern "C" void conker_emit_sprite_texrect(uint8_t* rdram, recomp_context* ctx) {
    gpr sp = ctx->r29;
    // Corners in 10.2 fixed point, already scaled by 4.
    int32_t ulx = (int32_t)stack_float(rdram, sp, 0xBC);
    int32_t uly = (int32_t)stack_float(rdram, sp, 0xB8);
    int32_t lrx = (int32_t)stack_float(rdram, sp, 0xB4);
    int32_t lry = (int32_t)stack_float(rdram, sp, 0xB0);
    // Texture start (s10.5) and steps (s5.10), including the flips' adjustments.
    uint32_t s = (uint32_t)MEM_W(0x98, sp) & 0xFFFF;
    uint32_t t = (uint32_t)MEM_W(0x94, sp) & 0xFFFF;
    uint32_t dsdx = (uint32_t)MEM_W(0x90, sp) & 0xFFFF;
    uint32_t dtdy = (uint32_t)MEM_W(0x8C, sp) & 0xFFFF;
    const uint32_t tile = 0;

    gpr dl = ctx->r2;
    put_command(rdram, dl, (rt64_extended_opcode << 24) | g_ex_texrect_v1,
        tile | (g_ex_origin_none << 3) | (g_ex_origin_none << 15));
    put_command(rdram, dl, ((uint32_t)(ulx & 0xFFFF) << 16) | (uint32_t)(uly & 0xFFFF),
        ((uint32_t)(lrx & 0xFFFF) << 16) | (uint32_t)(lry & 0xFFFF));
    put_command(rdram, dl, (s << 16) | t, (dsdx << 16) | dtdy);
    MEM_W(0x100, sp) = (int32_t)dl;
}

// func_151D5E90 (and func_151D6418) draw a saved copy of the frame, such as the
// pause menu's blurred background, as 42 textured tiles. The copy only holds the
// 4:3 frame, so RT64 draws the tiles in the 4:3 area and the widened sides show
// the game frozen behind them. Scale the tiles up to the whole width instead,
// and as much vertically, so the frame keeps its proportions and loses some of
// its top and bottom, with RT64's rect aspect (zoom, from rt64.patch). The tiles
// are full of load and pipe syncs, which RT64 doesn't need: before the first
// rectangle, the first sync becomes the enable of RT64's extended GBI and the
// second the zoom; the last sync, after the last rectangle, returns to the
// automatic aspect. The display list doesn't grow.
namespace {
    constexpr uint32_t g_ex_setrectaspect_v1 = 0x000033;
    constexpr uint32_t g_ex_aspect_auto = 0x0;
    constexpr uint32_t g_ex_aspect_zoom = 0x3;
    constexpr uint32_t g_texrect = 0xE4;

    gpr frame_copy_dl_start = 0;

    bool is_sync(uint8_t* rdram, gpr cmd) {
        uint32_t w0 = (uint32_t)MEM_W(0, cmd);
        return (w0 == 0xE6000000 || w0 == 0xE7000000 || w0 == 0xE8000000) && MEM_W(4, cmd) == 0;
    }
}

// At the start of the function: $a0 is where it writes its first command.
extern "C" void conker_frame_copy_begin(uint8_t* rdram, recomp_context* ctx) {
    frame_copy_dl_start = ctx->r4;
}

// At its return: $v0 is the end of what it wrote.
extern "C" void conker_frame_copy_end(uint8_t* rdram, recomp_context* ctx) {
    gpr start = frame_copy_dl_start;
    gpr end = ctx->r2;
    frame_copy_dl_start = 0;
    if (start == 0 || end <= start || end - start > 0x10000) {
        return;
    }
    gpr syncs_before[2] = { 0, 0 };
    int before_count = 0;
    gpr last_rect = 0;
    gpr last_sync = 0;
    for (gpr cmd = start; cmd < end; cmd += 8) {
        if (((uint32_t)MEM_W(0, cmd) >> 24) == g_texrect) {
            last_rect = cmd;
            cmd += 16; // its two RDPHALF words
        }
        else if (is_sync(rdram, cmd)) {
            if (last_rect == 0 && before_count < 2) {
                syncs_before[before_count++] = cmd;
            }
            last_sync = cmd;
        }
    }
    if (before_count < 2 || last_rect == 0 || last_sync < last_rect) {
        return;
    }
    gpr dl = syncs_before[0];
    put_command(rdram, dl, (rt64_hook_opcode << 24) | rt64_hook_magic, (rt64_hook_op_enable << 28) | rt64_extended_opcode);
    dl = syncs_before[1];
    put_command(rdram, dl, (rt64_extended_opcode << 24) | g_ex_setrectaspect_v1, g_ex_aspect_zoom);
    dl = last_sync;
    put_command(rdram, dl, (rt64_extended_opcode << 24) | g_ex_setrectaspect_v1, g_ex_aspect_auto);
}

// func_15180580 draws the circle wipe of spawning, dying and changing levels: black
// rectangles around the circle, clipped to the camera, and the circle's four
// quarters as texture rectangles, whose left edges are clamped to 0. RT64 widens
// the rectangles that touch one side of the frame, but when the circle is wider
// than the 4:3 frame, the side rectangles are empty and aren't drawn, and the left
// quarters are cut at 0, so the widened sides show the game through the wipe.
//
// The wipe is rewritten into a display list of our own: the black rectangles
// become RT64's extended ones, with signed coordinates reaching past the frame's
// edges, and the left quarters extended texture rectangles starting where the
// circle does. The display list the game wrote into can't grow, so its first
// command becomes a branch to ours, which branches back to where the game's
// continues. Ours are in RDRAM past the game's 8 MB, where RT64 can still read
// them (it masks addresses to 16 MB) and mods don't load (from 0x81000000).
namespace {
    constexpr uint32_t g_ex_fillrect_v1 = 0x000003;
    constexpr uint32_t g_dl_branch = 0xDE010000;
    constexpr uint32_t wide_dl_start = 0x00F00000;
    // A ring of display lists: RT64 has long finished with one when it comes round again.
    constexpr uint32_t wide_dl_size = 0x10000;
    uint32_t wide_dl_offset = 0;

    // Beyond the frame's sides, in N64 screen pixels. The scissor clips the rest.
    constexpr int32_t wide_reach = 1024;
    // The cameras (D_800BE628, 0x180 bytes each) and their clip bounds.
    constexpr gpr cameras_pointer = (gpr)(int32_t)0x800BE628;
    constexpr uint32_t camera_size = 0x180;
    constexpr int32_t camera_top = 0x24, camera_bottom = 0x28, camera_left = 0x2C, camera_right = 0x30;

    gpr iris_dl_start = 0;
    int32_t iris_camera = 0;

    struct Command {
        uint32_t w0, w1;
        uint32_t op() const { return w0 >> 24; }
    };

    int32_t camera_bound(uint8_t* rdram, int32_t camera, int32_t field) {
        gpr base = (gpr)MEM_W(0, cameras_pointer) + camera * camera_size;
        uint32_t word = (uint32_t)MEM_W(field, base);
        float value;
        std::memcpy(&value, &word, sizeof(value));
        return (int32_t)value;
    }

    void put_fill(uint8_t* rdram, gpr& dl, int32_t ulx, int32_t uly, int32_t lrx, int32_t lry) {
        if (lrx <= ulx || lry <= uly) {
            return;
        }
        put_command(rdram, dl, (rt64_extended_opcode << 24) | g_ex_fillrect_v1, g_ex_origin_none | (g_ex_origin_none << 12));
        put_command(rdram, dl, ((uint32_t)(ulx * 4) << 16) | ((uint32_t)(uly * 4) & 0xFFFF),
            ((uint32_t)(lrx * 4) << 16) | ((uint32_t)(lry * 4) & 0xFFFF));
    }
}

// At the start of the function: $a0 is where it writes its first command, $a1 the camera.
extern "C" void conker_iris_begin(uint8_t* rdram, recomp_context* ctx) {
    iris_dl_start = ctx->r4;
    iris_camera = (int32_t)ctx->r5;
}

// At its return: $v0 is the end of what it wrote.
extern "C" void conker_iris_end(uint8_t* rdram, recomp_context* ctx) {
    gpr start = iris_dl_start;
    gpr end = ctx->r2;
    iris_dl_start = 0;
    constexpr size_t max_commands = 64;
    if (start == 0 || end <= start || end - start > max_commands * 8) {
        return;
    }
    Command cmds[max_commands];
    size_t count = (size_t)(end - start) / 8;
    for (size_t i = 0; i < count; i++) {
        cmds[i] = { (uint32_t)MEM_W(i * 8, start), (uint32_t)MEM_W(i * 8 + 4, start) };
    }

    // The wipe in progress: 8 commands of setup (a display list call first), the
    // black rectangles, the texture's setup, the four quarters (a G_TEXRECT and its
    // two G_RDPHALF words each: below right, above left, above right, below left)
    // and the geometry mode's restore. Anything else is left as the game drew it.
    constexpr size_t setup_count = 8;
    if (count < setup_count || cmds[0].op() != 0xDE) {
        return;
    }
    size_t texture_setup = setup_count;
    while (texture_setup < count && cmds[texture_setup].op() == 0xF6) {
        texture_setup++;
    }
    constexpr size_t quarter_count = 4;
    if (count < texture_setup + quarter_count * 3 + 2) {
        return;
    }
    size_t quarters = count - 1 - quarter_count * 3;
    if (cmds[count - 1].op() != 0xD9) {
        return;
    }
    for (size_t i = texture_setup; i < quarters; i++) {
        if (cmds[i].op() == 0xE4 || cmds[i].op() == 0xF6) {
            return;
        }
    }
    for (size_t q = 0; q < quarter_count; q++) {
        const Command* rect = &cmds[quarters + q * 3];
        if (rect[0].op() != 0xE4 || rect[1].op() != 0xE1 || rect[2].op() != 0xF1) {
            return;
        }
    }
    auto ulx = [&](size_t q) { return (int32_t)((cmds[quarters + q * 3].w1 >> 12) & 0xFFF); };
    auto uly = [&](size_t q) { return (int32_t)(cmds[quarters + q * 3].w1 & 0xFFF); };
    auto lrx = [&](size_t q) { return (int32_t)((cmds[quarters + q * 3].w0 >> 12) & 0xFFF); };
    auto lry = [&](size_t q) { return (int32_t)(cmds[quarters + q * 3].w0 & 0xFFF); };
    // The circle, in 10.2 fixed point, from the unclamped quarter below right.
    int32_t cx4 = ulx(0);
    int32_t cy4 = uly(0);
    int32_t r4 = lrx(0) - cx4;
    if (r4 <= 0 || lry(0) - cy4 != r4 || lrx(1) != cx4 || lrx(3) != cx4) {
        return;
    }

    uint32_t needed = (1 + (uint32_t)count + quarter_count * 2 + 1) * 8;
    if (wide_dl_offset + needed > wide_dl_size) {
        wide_dl_offset = 0;
    }
    uint32_t wide_dl = wide_dl_start + wide_dl_offset;
    wide_dl_offset += (needed + 15) & ~15u;
    gpr dl = (gpr)(int32_t)(0x80000000u | wide_dl);

    put_command(rdram, dl, (rt64_hook_opcode << 24) | rt64_hook_magic, (rt64_hook_op_enable << 28) | rt64_extended_opcode);
    for (size_t i = 0; i < setup_count; i++) {
        put_command(rdram, dl, cmds[i].w0, cmds[i].w1);
    }

    // The black around the circle: bands above and below it, and its sides, which
    // reach past the frame's edges where the camera touches them.
    int32_t top = camera_bound(rdram, iris_camera, camera_top);
    int32_t bottom = camera_bound(rdram, iris_camera, camera_bottom);
    int32_t left = camera_bound(rdram, iris_camera, camera_left);
    int32_t right = camera_bound(rdram, iris_camera, camera_right);
    int32_t cx = cx4 / 4, cy = cy4 / 4, r = r4 / 4;
    // A full-screen camera is clipped to 2..290; split-screen ones stop in the middle.
    int32_t wide_left = left <= 4 ? left - wide_reach : left;
    int32_t wide_right = right >= 286 ? right + wide_reach : right;
    int32_t band_top = std::max(top, cy - r);
    int32_t band_bottom = std::min(bottom, cy + r);
    put_fill(rdram, dl, wide_left, top, wide_right, band_top);
    put_fill(rdram, dl, wide_left, band_bottom, wide_right, bottom);
    put_fill(rdram, dl, wide_left, band_top, std::min(cx - r, wide_right), band_bottom);
    put_fill(rdram, dl, std::max(cx + r, wide_left), band_top, wide_right, band_bottom);

    for (size_t i = texture_setup; i < quarters; i++) {
        put_command(rdram, dl, cmds[i].w0, cmds[i].w1);
    }
    for (size_t q = 0; q < quarter_count; q++) {
        const Command* rect = &cmds[quarters + q * 3];
        bool left_quarter = q == 1 || q == 3;
        if (!left_quarter || cx4 - r4 >= 0) {
            put_command(rdram, dl, rect[0].w0, rect[0].w1);
            put_command(rdram, dl, rect[1].w0, rect[1].w1);
            put_command(rdram, dl, rect[2].w0, rect[2].w1);
            continue;
        }
        // Undo the clamp: move the texture start back to where the circle starts.
        int32_t new_ulx = cx4 - r4;
        int32_t dsdx = (int16_t)(rect[2].w1 >> 16);
        int32_t s = (int16_t)(rect[1].w1 >> 16) + ((new_ulx * dsdx) >> 7);
        uint32_t t = rect[1].w1 & 0xFFFF;
        const uint32_t tile = (rect[0].w1 >> 24) & 0x7;
        put_command(rdram, dl, (rt64_extended_opcode << 24) | g_ex_texrect_v1,
            tile | (g_ex_origin_none << 3) | (g_ex_origin_none << 15));
        put_command(rdram, dl, ((uint32_t)new_ulx << 16) | (uint32_t)uly(q),
            ((uint32_t)lrx(q) << 16) | (uint32_t)lry(q));
        put_command(rdram, dl, ((uint32_t)s << 16) | t, rect[2].w1);
    }
    put_command(rdram, dl, cmds[count - 1].w0, cmds[count - 1].w1);
    put_command(rdram, dl, g_dl_branch, (uint32_t)end & 0x00FFFFFF);

    // The game's first command branches to ours; the rest of its wipe is skipped.
    gpr game_dl = start;
    put_command(rdram, game_dl, g_dl_branch, wide_dl);
}
