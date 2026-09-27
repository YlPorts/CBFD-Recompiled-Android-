#pragma once
#include <cstdint>
namespace conker::mobile {
// Conker's near-full-screen clear is clipped to a two-N64-pixel overscan
// margin. In expanded framebuffer space that margin must not retain the last
// scene. This predicate never classifies small object/UI rectangles as clears.
inline bool is_full_width_clear(int32_t left,int32_t top,int32_t right,int32_t bottom,
    int32_t scLeft,int32_t scTop,int32_t scRight,int32_t scBottom,int32_t sourceWidth) {
    if(sourceWidth<=0||sourceWidth>16384||left>=right||top>=bottom||scTop>=scBottom)return false;
    const int32_t far=sourceWidth*4,slack=8; // 10.2 units: exactly 2 source pixels
    return left>=0&&left<=slack&&right>=far-slack&&right<=far+3&&
        scLeft>=0&&scLeft<=slack&&scRight>=far-slack&&scRight<=far+3&&
        top<=scTop&&bottom>=scBottom;
}
}
