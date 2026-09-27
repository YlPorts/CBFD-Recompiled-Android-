#include <ultra64.h>

#include "functions.h"
#include "variables.h"

#pragma GLOBAL_ASM("asm/nonmatchings/game_138520/func_1510B070.s")

#pragma GLOBAL_ASM("asm/nonmatchings/game_138520/func_1510B128.s")

#pragma GLOBAL_ASM("asm/nonmatchings/game_138520/func_1510B32C.s")

#pragma GLOBAL_ASM("asm/nonmatchings/game_138520/func_1510B3B0.s")

#pragma GLOBAL_ASM("asm/nonmatchings/game_138520/func_1510B458.s")

#pragma GLOBAL_ASM("asm/nonmatchings/game_138520/func_1510B51C.s")

#pragma GLOBAL_ASM("asm/nonmatchings/game_138520/func_1510B5F8.s")

#pragma GLOBAL_ASM("asm/nonmatchings/game_138520/func_1510B690.s")

#pragma GLOBAL_ASM("asm/nonmatchings/game_138520/func_1510B7B4.s")

// Sets the view-space x and y scales that the clip-space culls (func_1510AEE0,
// func_150A6210, func_150A5378, func_150A50C0) multiply a point's x and y by
// before comparing them with its depth: it is inside the view while
// |x| * cullScaleX_800D35E0 <= depth, and likewise for y (cullScaleY_800D35E4).
// camera->baseCullScaleX/Y are the scales at the camera's base fields of view;
// zooming (fovX/fovY differing from baseFovX/baseFovY) shifts them linearly.
// cameraIndex is the camera's index in D_800BE628 (0x180-byte cameras). The PC
// port hooks its return to widen the x scale for widescreen
// (host/src/widescreen.cpp).
void updateCullScales_1510B958(s32 cameraIndex) {
    struct259 *camera = (struct259 *) D_800BE628 + cameraIndex;

    cullScaleX_800D35E0 = ((camera->fovX / camera->baseFovX) - 1.0f) * -1.0f + camera->baseCullScaleX;
    cullScaleY_800D35E4 = ((camera->fovY / camera->baseFovY) - 1.0f) * -1.0f + camera->baseCullScaleY;
}

#pragma GLOBAL_ASM("asm/nonmatchings/game_138520/func_1510B9D0.s")

#pragma GLOBAL_ASM("asm/nonmatchings/game_138520/func_1510BF60.s")

#pragma GLOBAL_ASM("asm/nonmatchings/game_138520/func_1510C4AC.s")

#pragma GLOBAL_ASM("asm/nonmatchings/game_138520/func_1510C8A8.s")
