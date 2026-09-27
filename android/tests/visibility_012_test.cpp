#include "recomp.h"
#include "mobile_metrics.hpp"
#include <vector>
#include <cstring>
#include <iostream>
#include <cstdlib>
extern "C" void conker_original_frustum(uint8_t*,recomp_context*);
extern "C" void func_1501B22C(uint8_t*,recomp_context*);
extern "C" void func_150A6360(uint8_t*,recomp_context*);
extern "C" void cosf_recomp(uint8_t*,recomp_context* c){c->f0.fl=std::cos(c->f12.fl);}
extern "C" void sinf_recomp(uint8_t*,recomp_context* c){c->f0.fl=std::sin(c->f12.fl);}
int main(){
    unsigned checks=0;
    auto require=[&](bool ok,const char* label){++checks;if(!ok){std::cerr<<"FAIL "<<label<<'\n';std::exit(1);}};
    std::vector<uint8_t> mem(8*1024*1024);uint8_t* rdram=mem.data();
    const gpr cam=gpr(int32_t(0x80100000)),matrix=gpr(int32_t(0x80200000)),sp=gpr(int32_t(0x80700000));
    auto bits=[](float value){uint32_t b;std::memcpy(&b,&value,4);return b;};
    auto put=[&](gpr addr,int off,float value){MEM_W(off,addr)=bits(value);};
    MEM_W(0,gpr(int32_t(0x800BE628)))=uint32_t(cam);
    put(gpr(int32_t(0x80096900)),0,float(std::acos(-1.0)/180));
    put(gpr(int32_t(0x80096904)),0,float(std::acos(-1.0)/180));
    put(gpr(int32_t(0x800D9B20)),0,1);
    for(int i=0;i<4;++i)put(matrix,i*20,1);
    for(int i=0;i<4;++i){put(cam+i*0x180,0x74,60);put(cam+i*0x180,0x78,50);}
    auto build=[&](bool widened,int index=0){recomp_context c{};c.r4=index;c.r29=sp;
        if(widened)func_1501B22C(rdram,&c);else conker_original_frustum(rdram,&c);return c;};
    auto cull=[&](float x,float y,float z,float rh=0,float rv=0){recomp_context c{};c.mips3_float_mode=true;c.f_odd=&c.f1.u32l;c.r4=cam;c.r5=matrix;c.r6=bits(x);c.r7=bits(y);c.r29=sp;
        put(sp,0x10,z);put(sp,0x14,rh);put(sp,0x18,rv);put(sp,0x1c,1000);func_150A6360(rdram,&c);return c.r2!=0;};
    conker::mobile::metrics.horizontalAspect=1.0f;
    auto originalCtx=build(false);auto original=mem;auto wrappedCtx=build(true);
    require(mem==original,"original aspect preserves all memory");
    require(std::memcmp(&originalCtx,&wrappedCtx,sizeof originalCtx)==0,"hook preserves register results");
    require(!cull(75,0,-100)&&!cull(-75,0,-100),"reproduce visible-side object rejected by original game");
    conker::mobile::metrics.horizontalAspect=(2340.f/1080)/(292.f/216);
    build(true);
    require(cull(75,0,-100)&&cull(-75,0,-100),"panoramic CPU culler retains both visible sides");
    for(size_t i=0;i<mem.size();++i)if(mem[i]!=original[i]){
        // cull() changes its own stack argument block; the hook changes exactly four plane fields.
        const auto off=i-0x100000u;
        require((off>=0x88&&off<0x8c)||(off>=0x90&&off<0x98)||(off>=0x9c&&off<0xa0)||
            (i>=0x700010&&i<0x700020),"no projection, vertical, near/far or object-data edits");
    }
    for(float scale:{1.f,1.2f,4.f/3.f,1.6027397f,1.75f,2.4f}){
        conker::mobile::metrics.horizontalAspect=scale;build(true);
        const double h=std::atan(std::tan(std::acos(-1.0)/6)*scale),v=25*std::acos(-1.0)/180;
        for(float radius:{0.f,5.f,35.f})for(int x=-180;x<=180;x+=3)for(int y=-90;y<=90;y+=9){
            bool expected=100*std::sin(h)-std::abs(x)*std::cos(h)>=-radius &&
                100*std::sin(v)-std::abs(y)*std::cos(v)>=-radius;
            require(cull(x,y,-100,radius,radius)==expected,"actual game culler matches normalized wide-plane geometry");
        }
        require(!cull(0,0,100),"behind camera rejected");
        require(!cull(0,0,-2000),"far distance retained");
        require(!cull(0,200,-100),"vertical limit retained");
        auto once=mem;build(true);
        require(std::memcmp(mem.data()+0x100000,once.data()+0x100000,0x180)==0,"no repeated widening drift");
        for(int i=1;i<4;++i){build(true,i);require(std::memcmp(mem.data()+0x100088,mem.data()+0x100088+i*0x180,0x30)==0,"all camera indices rebuilt consistently");}
    }
    std::cout<<"PASS "<<checks<<" original USA frustum/culler + native hook assertions; host only, no gameplay/FPS claim\n";
}
