#include "rt64_screen_modify.h"
#include "mobile_frame_coverage.hpp"
#include "mobile_camera.hpp"
#include <iostream>
#include <random>
#include <cmath>
#include <cstring>
#include <map>
#include <vector>
#include <array>
#include <thread>
#include <cstdlib>
int main(){
    uint64_t checks=0;auto check=[&](bool ok,const char* what){++checks;if(!ok){std::cerr<<"FAIL "<<checks<<": "<<what<<'\n';std::abort();}};
    RT64::ScreenModifyRecords<64> records;std::vector<uint32_t> words;
    records.update(0,17,false,0x12345678,words);records.update(0,17,true,0x01234567,words);
    records.update(0,17,false,0x89abcdef,words);
    check(words==std::vector<uint32_t>({17,3,0x89abcdef,0x01234567}),"last XY command wins, one writer, Z kept");
    records.copySlot(0,18,words);
    check(words.size()==8&&words[4]==18&&words[5]==3&&words[6]==0x89abcdef&&words[7]==0x01234567,"ST/RGBA clone inherits pending XY and Z");
    records.update(0,18,true,0x03000000,words);
    check(words[3]==0x01234567&&words[7]==0x03000000,"clone edits leave earlier draw untouched");
    records.resetSlot(0);records.update(0,19,true,0,words);
    check(words.size()==12&&words[9]==2&&words[10]==0,"vertex cache load invalidates old edits");
    check(records.xyCommands==2&&records.zCommands==3&&records.clones==1&&records.coalesced==3,"command counts");
    records.reset();words.clear();check(records.offsets[0]==UINT32_MAX&&records.clones==0,"task reset");
    check(RT64::normalizedScreenDepth(0)==0,"depth near zero");
    check(RT64::normalizedScreenDepth(0x03ff0000)==1023.f/1024.f,"screen-Z and viewport use same depth units");
    check(RT64::normalizedScreenDepth(0x02000000)==.5f,"depth midpoint");
    struct Value{uint32_t mask=0,xy=0,z=0;};std::map<uint32_t,Value> expected;
    std::array<uint32_t,64> active{};uint32_t next=0;std::mt19937 rng(0x170017);
    for(uint32_t& v:active)v=next++;
    for(int i=0;i<20000;++i){const auto slot=rng()%64;auto cmd=rng()%5;auto old=active[slot];
        if(cmd==0){records.resetSlot(slot);active[slot]=next++;}
        else if(cmd==1){active[slot]=next++;records.copySlot(slot,active[slot],words);
            auto e=expected.find(old);if(e!=expected.end())expected[active[slot]]=e->second;}
        else{bool z=cmd==2;uint32_t data=rng();records.update(slot,active[slot],z,data,words);auto& e=expected[active[slot]];e.mask|=z?2:1;(z?e.z:e.xy)=data;}
    }
    check(words.size()==expected.size()*4,"one record per modified global vertex");
    std::map<uint32_t,bool> unique;
    for(size_t i=0;i<words.size();i+=4){auto e=expected.at(words[i]);check(!unique[words[i]],"no two GPU writers for the same vertex");unique[words[i]]=true;
        check(words[i+1]==e.mask&&words[i+2]==e.xy&&words[i+3]==e.z,"random command stream matches immutable clone reference");}
    using conker::mobile::is_full_width_clear;
    check(is_full_width_clear(0,0,1280,960,8,0,1272,960,320),"full Conker clear and overscan scissor");
    check(is_full_width_clear(8,0,1272,960,8,0,1272,960,320),"clear within two source pixels");
    check(!is_full_width_clear(40,0,1280,960,8,0,1272,960,320),"not a full clear");
    check(!is_full_width_clear(0,0,1280,959,8,0,1272,960,320),"partial vertical clip not expanded");
    check(!is_full_width_clear(0,0,1280,960,80,0,1272,960,320),"split viewport excluded");
    check(!is_full_width_clear(0,0,1280,960,8,0,1272,960,0),"invalid extent excluded");
    check(!is_full_width_clear(0,0,1280,960,8,0,1272,960,16385),"overflow extent excluded");
    // Synthetic persistent border: test the actual expansion predicate, not an Android screenshot.
    std::vector<unsigned> pixels(1040,0xffff00ffu);
    if(is_full_width_clear(0,0,1280,960,8,0,1272,960,320))std::fill(pixels.begin(),pixels.end(),0x00332211);
    for(auto x:pixels)check(x==0x00332211,"full-width clear reaches every last-column pixel in the fixture");
    // Numeric comparison of old ((V*W)*p) and new (V*(W*p)) shader expressions.
    std::uniform_real_distribution<float> v(-4,4);double maxError=0;
    for(int n=0;n<4000;++n){float a[4][4],b[4][4],p[4],c[4][4]{},bp[4]{},old[4]{},now[4]{};
        for(auto& row:a)for(float& x:row)x=v(rng);for(auto& row:b)for(float& x:row)x=v(rng);for(float& x:p)x=v(rng);
        for(int i=0;i<4;++i)for(int j=0;j<4;++j)for(int k=0;k<4;++k)c[i][j]+=a[i][k]*b[k][j];
        for(int i=0;i<4;++i)for(int j=0;j<4;++j){old[i]+=c[i][j]*p[j];bp[i]+=b[i][j]*p[j];}
        for(int i=0;i<4;++i)for(int j=0;j<4;++j)now[i]+=a[i][j]*bp[j];
        for(int i=0;i<4;++i){maxError=std::max(maxError,double(std::abs(old[i]-now[i])));check(std::abs(old[i]-now[i])<.00015f,"matrix-vector reassociation FP32 tolerance (not bit-identical)");}
    }
    using namespace conker::camera;Vec3 eye{0,20,100},pivot{0,20,0};
    for(int i=0;i<360;++i)check(orbit(pivot,eye,{1,0},1.f/110.f),"continuous yaw orbit");
    check(std::abs(eye.x)<.001&&std::abs(eye.z-100)<.001&&eye.y==20,"yaw completes 360 without C step limits");
    for(int i=0;i<150;++i)check(orbit(pivot,eye,{0,1},1.f/30.f),"pitch movement");
    check(std::abs(std::asin((eye.y-20)/100.0)*180/3.141592653589793-80)<.001,"pitch upper safety limit");
    auto old=eye;check(!orbit(pivot,eye,{0,0},1.f/30.f)&&std::memcmp(&old,&eye,sizeof eye)==0,"no axes no camera edits");
    check(!orbit(pivot,eye,{1,0},NAN),"invalid elapsed time blocked");
    auto axes=unpack(pack(INFINITY,-3));check(axes.x==0&&axes.y==-1,"finite axis snapshots");
    release();padInput=pack(.4f,.2f);check(input().x==.4f,"physical analog stick");touchInput=pack(-.3f,.1f);check(input().x==-.3f,"touch owns orbit when active");release();check(input().x==0,"release camera");
    std::cout<<"PASS "<<checks<<" screen-edit/clear/matrix/orbit assertions; max position reassociation error="<<maxError<<" (synthetic CPU tests, no phone FPS)\n";
}
