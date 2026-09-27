#include "mobile_match_policy.hpp"
#include "mobile_clip_bounds.hpp"
#include "mobile_metrics.hpp"
#include <array>
#include <cassert>
#include <cstring>
#include <iostream>
#include <random>
#include <chrono>
using namespace conker::mobile;
int main() {
    unsigned tests=0;
    auto check=[&](bool ok){ if(!ok){std::cerr<<"Failed check "<<tests+1<<"\n";std::abort();}++tests; };
    check(!same_mesh(1,0,1,0));check(!same_mesh(1,10,2,10));check(!same_mesh(1,9,1,10));check(same_mesh(23,30,23,30));
    for(size_t n:{0,1,8,24,25,50,1000,2000})for(size_t m:{0,1,20,25,1000}) {
        uint64_t candidates=0;
        for(size_t i=0;i<n;++i) {
            auto [b,e]=candidate_window(i,n,m);check(b<=e&&e<=m);
            check(e-b<=(m<=24?m:17));candidates+=e-b;
            if(n==m&&m>24)check(b<=i&&i<e);
        }
        check(candidates<=n*std::min(size_t(24),m));
    }
    std::array<float,16> identity{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};float velocity[3]{};
    auto a=describe_transform(identity.data(),identity.data(),velocity);check(a.valid);check(match_difference(a,a)==0);
    auto flipped=identity;flipped[0]=-1;auto b=describe_transform(flipped.data(),identity.data(),velocity);
    check(!std::isfinite(match_difference(a,b)));
    auto singular=identity;singular[0]=0;check(!describe_transform(singular.data(),identity.data(),velocity).valid);
    auto nan=identity;nan[12]=std::numeric_limits<float>::quiet_NaN();check(!describe_transform(nan.data(),identity.data(),velocity).valid);
    velocity[0]=1;auto moving=describe_transform(identity.data(),identity.data(),velocity);velocity[0]=0;
    auto translated=identity;translated[12]=1;auto now=describe_transform(translated.data(),identity.data(),velocity);
    check(std::abs(match_difference(now,moving)-1.0f)<1e-6f); // screen motion remains, predicted position is exact.
    ClipRect limit{0,0,320,240},out{};
    check(clip_bounds({ClipPoint{1,1,1},{100,1,1},{1,100,1}},limit,out)==ClipResult::Visible);
    check(out.left==1&&out.right==100&&out.top==1&&out.bottom==100);
    check(clip_bounds({ClipPoint{1,1,-1},{100,1,-1},{1,100,-1}},limit,out)==ClipResult::Empty);
    check(clip_bounds({ClipPoint{1,1,1},{100,1,1},{1,100,-1}},limit,out)==ClipResult::Visible);
    check(out.left>=0&&out.top>=0&&out.right<=320&&out.bottom<=240);
    check(clip_bounds({ClipPoint{-100,-100,1},{-200,-100,1},{-200,-200,1}},limit,out)==ClipResult::Empty);
    check(clip_bounds({ClipPoint{500,10,1},{600,100,1},{600,100,1}},limit,out)==ClipResult::Empty);
    check(clip_bounds({ClipPoint{500,10,1},{600,100,1},{600,100,1}},limit,out,true)==ClipResult::Visible);
    check(out.left==0&&out.right==320&&out.top==10&&out.bottom==100); // Wide-only geometry retains coverage.

    check(clip_bounds({ClipPoint{INFINITY,0,1},{0,0,1},{0,1,1}},limit,out)==ClipResult::Invalid);
    check(clip_bounds({ClipPoint{0,0,1},{0,0,1},{0,0,1}},limit,out)==ClipResult::Visible);
    // For positive w, any visible point of the triangle must be inside the
    // returned bound. Random points are tested in homogeneous space, not after
    // interpolating divided coordinates.
    std::mt19937 rng(0xCBFD);std::uniform_real_distribution<double> xy(-1000,1000),w(-2,3),unit(0,1);
    for(int t=0;t<2000;++t) {
        std::array<ClipPoint,3> tri;for(auto& p:tri)p={xy(rng),xy(rng),w(rng)};
        auto result=clip_bounds(tri,limit,out);check(result!=ClipResult::Invalid);
        for(int j=0;j<20;++j) {
            double u=unit(rng),v=unit(rng);if(u+v>1){u=1-u;v=1-v;}
            ClipPoint p{u*tri[0].x+v*tri[1].x+(1-u-v)*tri[2].x,u*tri[0].y+v*tri[1].y+(1-u-v)*tri[2].y,u*tri[0].w+v*tri[1].w+(1-u-v)*tri[2].w};
            if(p.w>1e-6&&p.x/p.w>=0&&p.x/p.w<=320&&p.y/p.w>=0&&p.y/p.w<=240)
                check(result==ClipResult::Visible&&p.x/p.w>=out.left-1e-6&&p.x/p.w<=out.right+1e-6&&p.y/p.w>=out.top-1e-6&&p.y/p.w<=out.bottom+1e-6);
        }
    }
    localBoundsBehind=40;localBoundsClipped=3;localBoundsFallback=2;publish_bounds();
    check(metrics.boundsBehind.load()==40&&metrics.boundsClipped.load()==3&&metrics.boundsFallbacks.load()==2);
    check(localBoundsBehind==0&&localBoundsClipped==0&&localBoundsFallback==0);
    std::cout<<"PASS "<<tests<<" native matching/clipping assertions (synthetic, no GPU/game)\n";
}
