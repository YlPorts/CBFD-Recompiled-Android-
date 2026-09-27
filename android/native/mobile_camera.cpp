// USA camera input hook at 15123508, called from the normal camera call site at
// 15122F8C BEFORE angle recalculation, obstruction/collision and view-matrix setup.
// Original digital controls are preserved; analog axes do not generate C bits.
#include <cstdio>
#include <cstring>
#include "recomp.h"
#include "mobile_camera.hpp"
extern "C" void conker_original_camera_input(uint8_t*,recomp_context*);
namespace {
float field(uint8_t* rdram,gpr camera,int off) {
    uint32_t bits=uint32_t(MEM_W(off,camera));float value;std::memcpy(&value,&bits,4);return value;
}
void store(uint8_t* rdram,gpr camera,int off,float value) {
    uint32_t bits;std::memcpy(&bits,&value,4);MEM_W(off,camera)=bits;
}
bool main_memory(uint32_t p,uint32_t size) { return (p&3)==0 && p>=0x80000000u && p<=0x80800000u-size; }
}
extern "C" void func_15123508(uint8_t* rdram,recomp_context* ctx) {
    using namespace conker::camera;
    const gpr camera=ctx->r4;
    conker_original_camera_input(rdram,ctx);
    const auto call=hookCalls.fetch_add(1,std::memory_order_relaxed);
    if(!main_memory(uint32_t(camera),0x868)) { ++blockedCalls;return; }
    const uint32_t flags=uint32_t(MEM_W(0x84,camera));
    const uint32_t actor=uint32_t(MEM_W(0x3D4,camera));
    const gpr actorAddress=static_cast<gpr>(static_cast<int32_t>(actor));
    // Same player, mode and interaction guards as the original yaw caller/body.
    // Do not require unknown field 7F4==0 (that wrongly excluded normal camera states).
    const bool allowed=MEM_BU(0x23D,camera)==0 && (flags&2)!=0 && (flags&0x00200000u)==0 &&
        MEM_W(0x698,camera)==0 && main_memory(actor,0x124) && MEM_BU(0x120,actorAddress)==0;
    if(call==0) std::fprintf(stderr,"[camera] Normal-camera hook reached: addr=%08X flags=%08X actor=%08X allowed=%d\n",
        uint32_t(camera),flags,actor,int(allowed));
    if(!allowed) { ++blockedCalls;return; }
    ++allowedCalls;
    // 2A4 is the orbit pivot used by the actual digital-yaw routine. 2BC is a
    // separate smoothed look target and is not an interchangeable pivot.
    Vec3 center{field(rdram,camera,0x2A4),field(rdram,camera,0x2A8),field(rdram,camera,0x2AC)};
    Vec3 eye{field(rdram,camera,0x2F8),field(rdram,camera,0x2FC),field(rdram,camera,0x300)};
    if(!orbit(center,eye,input(),field(rdram,camera,0x7B4))) return;
    store(rdram,camera,0x2F8,eye.x);store(rdram,camera,0x2FC,eye.y);store(rdram,camera,0x300,eye.z);
    store(rdram,camera,0x6C4,0.f); // no leftover digital angular velocity while analog owns the orbit
    if(updatedCalls.fetch_add(1,std::memory_order_relaxed)==0)
        std::fprintf(stderr,"[camera] Native orbital yaw/pitch applied to game camera; original collision/view update follows\n");
}
