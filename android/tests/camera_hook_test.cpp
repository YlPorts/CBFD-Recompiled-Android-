#include "recomp.h"
#include "mobile_camera.hpp"
#include <vector>
#include <cstring>
#include <iostream>
#include <cstdlib>
extern "C" void conker_original_camera_input(uint8_t*,recomp_context*);
extern "C" void func_15123508(uint8_t*,recomp_context*);
int main(){
    unsigned checks=0;auto check=[&](bool p,const char* w){++checks;if(!p){std::cerr<<"FAIL "<<w<<'\n';std::abort();}};
    std::vector<uint8_t> mem(8*1024*1024);uint8_t* rdram=mem.data();
    const gpr camera=gpr(int32_t(0x8014fa30)),actor=gpr(int32_t(0x80144bec));
    auto fp=[&](int offset,float x){uint32_t bits;std::memcpy(&bits,&x,4);MEM_W(offset,camera)=bits;};
    auto get=[&](int offset){uint32_t bits=MEM_W(offset,camera);float x;std::memcpy(&x,&bits,4);return x;};
    auto initialize=[&](){std::fill(mem.begin(),mem.end(),0);MEM_W(0x84,camera)=0xE;MEM_W(0x3d4,camera)=uint32_t(actor);
        fp(0x2A4,10);fp(0x2A8,20);fp(0x2AC,30);fp(0x2F8,10);fp(0x2FC,20);fp(0x300,130);fp(0x7B4,1.f/30.f);
        conker::camera::release();};
    // Compare wrapper against the REAL ROM-generated original routine, with no input.
    for(int state=0;state<16;++state){initialize();MEM_H(0x36a,camera)=state;MEM_W(0x7f4,camera)=state%2;
        std::vector<uint8_t> expected=mem;recomp_context a{},b{};a.r4=b.r4=camera;
        conker_original_camera_input(expected.data(),&a);func_15123508(rdram,&b);
        check(mem==expected,"neutral camera preserves entire RDRAM exactly");check(std::memcmp(&a,&b,sizeof a)==0,"neutral camera preserves original registers");}
    initialize();conker::camera::touchInput=conker::camera::pack(.5f,.3f);recomp_context ctx{};ctx.r4=camera;
    auto snapshot=mem;func_15123508(rdram,&ctx);
    check(get(0x2F8)!=10&&get(0x2FC)>20,"real hook writes continuous eye coordinates");
    check(MEM_HU(0x36a,camera)==0,"no C-button mask injection");
    check(std::abs(std::hypot(std::hypot(get(0x2F8)-10,get(0x2FC)-20),get(0x300)-30)-100)<.001,"orbital distance preserved");
    for(size_t i=0;i<mem.size();++i)if(mem[i]!=snapshot[i]){auto p=i-0x14fa30;check((p>=0x2f8&&p<0x304)||(p>=0x6c4&&p<0x6c8),"hook edits only eye/velocity fields");}
    for(int state=0;state<5;++state){initialize();if(state==0)MEM_W(0x84,camera)=0xC;if(state==1)MEM_W(0x84,camera)=0x20000E;
        if(state==2)MEM_W(0x698,camera)=1;if(state==3)MEM_B(0x120,actor)=1;if(state==4)MEM_B(0x23D,camera)=1;
        auto prev=mem;conker::camera::touchInput=conker::camera::pack(.9f,.2f);ctx={};ctx.r4=camera;func_15123508(rdram,&ctx);check(mem==prev,"script/interaction/second-player guard preserves camera");}
    initialize();MEM_W(0x7F4,camera)=1;conker::camera::padInput=conker::camera::pack(.4f,-.2f);ctx={};ctx.r4=camera;func_15123508(rdram,&ctx);
    check(get(0x2F8)!=10,"ordinary nonzero7F4 state does not incorrectly disable orbit");
    auto counts=conker::camera::counters();check(counts.updates==2&&counts.blocked==5,"real hook counters");
    std::cout<<"PASS "<<checks<<" actual camera-hook + original recompiled routine comparisons (no graphics)\n";
}
