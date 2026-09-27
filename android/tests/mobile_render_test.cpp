#include "mobile_render_safety.hpp"
#include "mobile_metrics.hpp"
#include <limits>
#include <cassert>
#include <iostream>
#include <thread>
#include <vector>

int main() {
    unsigned checks=0;
    auto check=[&](bool v){assert(v);++checks;};
    using conker::mobile::unsafe_screen_bounds;
    check(!unsafe_screen_bounds(160,120,10));
    check(!unsafe_screen_bounds(0,0,1));
    check(!unsafe_screen_bounds(320,240,1));
    check(!unsafe_screen_bounds(-640,900,2));
    check(unsafe_screen_bounds(160,120,-1));
    check(unsafe_screen_bounds(160,120,0));
    check(unsafe_screen_bounds(160,120,1e-8f));
    check(unsafe_screen_bounds(std::numeric_limits<float>::infinity(),120,1));
    check(unsafe_screen_bounds(160,std::numeric_limits<float>::quiet_NaN(),1));
    check(unsafe_screen_bounds(160,120,std::numeric_limits<float>::infinity()));
    check(unsafe_screen_bounds(3e8f,120,1));
    check(unsafe_screen_bounds(-3e8f,120,1));
    check(unsafe_screen_bounds(160,3e8f,1));
    check(unsafe_screen_bounds(160,-3e8f,1));
    // Same homogeneous triangle, with one vertex behind the eye: old screen
    // winding can reverse. The actual production helper must select conservative
    // bookkeeping, without changing the GPU triangle indices or cull/depth modes.
    struct V {float x,y,w;};
    V tri[]={{0,0,1},{320,0,1},{160,-240,-1}};
    bool full=false;for(auto v:tri)full|=unsafe_screen_bounds(v.x,v.y,v.w);
    check(full);
    tri[2]={160,240,1};full=false;for(auto v:tri)full|=unsafe_screen_bounds(v.x,v.y,v.w);
    check(!full);
    using namespace conker::mobile;
    check(bulk_sleep_ns(-10)==0);check(bulk_sleep_ns(1000000)==0);
    check(bulk_sleep_ns(2000000)==0);check(bulk_sleep_ns(3000000)==2000000);
    check(bulk_sleep_ns(16666667)==15666667);
    check(microseconds(16.5)==16500);
    check(microseconds(-1)==0);
    check(microseconds(std::numeric_limits<double>::quiet_NaN())==0);
    check(microseconds(std::numeric_limits<double>::infinity())==0);
    check(microseconds(20000)==0);
    gpu_sample(0);check(snapshot().gpuSamples==0);
    std::vector<std::thread> threads;
    for(int t=0;t<4;++t)threads.emplace_back([]{
        for(int i=0;i<10000;++i) {
            gpu_sample(2.5);render_sample(4.0);match_sample(.25);
            metrics.presents.fetch_add(1,std::memory_order_relaxed);
        }
    });
    for(auto& t:threads)t.join();
    auto s=snapshot();
    check(s.presents==40000);check(s.renders==40000);check(s.gpuSamples==40000);
    check(s.gpuUs==100000000);check(s.renderUs==160000000);check(s.matchUs==10000000);check(s.matches==40000);
    std::cout<<"PASS "<<checks<<" render safety/metrics checks, including 40000 concurrent samples\n";
}
