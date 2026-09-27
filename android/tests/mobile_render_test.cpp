#include "mobile_triangle_bounds.hpp"
#include "mobile_resolution.hpp"
#include "mobile_metrics.hpp"
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <thread>
#include <vector>
using namespace conker::android;
static int checks=0;
static void check(bool value,const char* label){++checks;if(!value){std::fprintf(stderr,"FAIL: %s\n",label);std::exit(1);}}
int main(){
    auto bounds=[](float a,float b,float c){return conservative_triangle_bounds(a,b,c,0,0,320,0,0,240);};
    check(!bounds(1,1,1),"ordinary forward triangle");
    check(bounds(-1,1,1),"mixed eye-plane triangle");
    check(bounds(1,-1,1),"second vertex behind");check(bounds(1,1,-1),"third vertex behind");
    check(bounds(0,1,1),"zero W");check(bounds(1e-7f,1,1),"near-zero W");
    check(bounds(-1,-1,-1),"behind triangle: GPU still clips");
    check(bounds(std::numeric_limits<float>::quiet_NaN(),1,1),"NaN W");
    check(bounds(1,std::numeric_limits<float>::infinity(),1),"infinite W");
    check(conservative_triangle_bounds(1,1,1,0,0,1e9f,0,0,1),"projected bounds overflow protection");
    check(conservative_triangle_bounds(1,1,1,0,0,0,0,0,std::numeric_limits<float>::quiet_NaN()),"NaN projection protection");
    check(!conservative_triangle_bounds(1,1,1,-160,0,640,0,0,240),"normal widescreen offscreen bounds");
    MobileResolution r;
    check(r.scale==2,"initial scale");check(!r.sample(40,18,100,5),"first slow window");
    check(r.sample(40,18,100,5)&&r.scale==1.75,"second slow window down");
    check(!r.sample(40,18,100,5),"downward hysteresis");check(r.sample(40,18,100,5)&&r.scale==1.5,"lower scale");
    for(int i=0;i<6;i++)check(!r.sample(35,18,100,5)&&r.scale==1.5,"minimum scale maintained");
    for(int i=0;i<3;i++)check(!r.sample(60,6,100,5),"upward hysteresis");
    check(r.sample(60,6,100,5)&&r.scale==1.75,"quality recovery");
    for(int i=0;i<3;i++)check(!r.sample(60,6,100,5),"second recovery hysteresis");
    check(r.sample(60,6,100,5)&&r.scale==2,"full quality recovery");
    for(int i=0;i<8;i++)check(!r.sample(25,4,100,5)&&r.scale==2,"CPU-limited: do not reduce resolution");
    check(!r.sample(40,18,1,5),"too few timestamp samples");
    check(!r.sample(40,18,100,10),"pause window ignored");
    check(!r.sample(40,0,100,5),"unsupported/zero GPU timestamp");
    check(!r.sample(std::numeric_limits<double>::quiet_NaN(),18,100,5),"nonfinite fps");
    check(!r.sample(40,std::numeric_limits<double>::quiet_NaN(),100,5),"nonfinite GPU time");
    check(!r.sample(40,18,100,5),"fresh low interval");
    check(!r.sample(60,10,100,5),"neutral interval resets slow history");
    check(!r.sample(40,18,100,5)&&r.scale==2,"not consecutive: no quality change");
    MobileMetrics m;
    std::vector<std::thread> threads;
    for(int i=0;i<4;i++)threads.emplace_back([&]{for(int j=0;j<1000;j++)m.renderedFrame(5,7);});
    for(auto &t:threads)t.join();
    check(m.rendered==4000&&m.gpuSamples==4000,"atomic frame accounting");
    check(m.gpuUs==20000000&&m.renderUs==28000000,"microsecond accounting");
    m.renderedFrame(std::numeric_limits<double>::quiet_NaN(),-1);
    check(m.rendered==4001&&m.gpuSamples==4000&&m.renderUs==28000000,"invalid samples excluded");
    m.presentedFrame();m.presentedFrame();check(m.presented==2,"actual successful present counter");
    std::printf("PASS %d mobile bounds, adaptive resolution and metrics assertions (host, not a GPU benchmark)\n",checks);
}
