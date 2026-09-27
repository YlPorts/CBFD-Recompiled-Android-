#include "mobile_render_policy.hpp"
#include <array>
#include <cassert>
#include <cmath>
#include <iostream>
#include <random>
#include <vector>

struct Pixel { double color=.2, coverage=.01, depth=1.; };
struct Fragment { double color, alpha, coverage, depth; unsigned mask; bool discard; };
using Image = std::array<Pixel,16>;

int main() {
    unsigned checks=0;
    auto check=[&](bool ok) { assert(ok); ++checks; };
    using namespace conker::mobile;
    for (unsigned h : {120U, 224U, 240U, 288U, 480U, 576U, 1080U}) {
        check(std::abs(resolution_scale(h)*h-1080.f) < .001f);
    }
    check(resolution_scale(0)==4.5f);
    check(resolution_scale(1)==18.f);
    check(resolution_scale(2160)==1.f);
    // Expand preserves display proportions at 1080 lines, including the A15.
    for (unsigned w : {1920U,2340U,2400U}) {
        float ratio=(float(w)/1080.f)/(320.f/240.f);
        check(std::abs(320.f*resolution_scale(240)*ratio-w)<.001f);
    }
    check(coverage_batch(0,false,false)==1);
    check(coverage_batch(72,true,false)==72);
    check(coverage_batch(72,true,true)==1);

    std::mt19937 random(10108060);
    // Independent per-fragment reference versus the production batch policy.
    // Vary overlap, discard, equal depth, coverage wrap, depth reads and writes.
    for (int wrap=0;wrap<2;++wrap) for(int read=0;read<2;++read)
    for (int write=0;write<2;++write) for(int scene=0;scene<250;++scene) {
        std::vector<Fragment> input;
        for(int t=0;t<73;++t) input.push_back({double(random()%256)/255,
            double(random()%256)/255,double(random()%8+1)/255,
            double(random()%9+1)/10,unsigned(random()),random()%7==0});
        Image reference{}, batched{};
        auto apply=[&](Image& image,const Fragment& f,bool rgb,bool alpha,bool depth) {
            for(unsigned p=0;p<image.size();++p) {
                auto& out=image[p];
                if(f.discard || !(f.mask&(1U<<p)) || (read && f.depth>=out.depth)) continue;
                if(rgb) out.color=f.color*f.alpha+out.color*(1-f.alpha);
                if(alpha) out.coverage=wrap?std::min(1.,out.coverage+f.coverage):f.coverage;
                if(depth) out.depth=f.depth;
            }
        };
        for (const auto& f : input) apply(reference,f,true,true,write);
        auto batch=coverage_batch(input.size(),write,read);
        for (unsigned first=0;first<input.size();first+=batch) {
            auto last=std::min(unsigned(input.size()),first+batch);
            for (unsigned t=first;t<last;++t) apply(batched,input[t],true,false,false);
            for (unsigned t=first;t<last;++t) apply(batched,input[t],false,true,write);
        }
        for(unsigned p=0;p<reference.size();++p) {
            check(std::abs(reference[p].color-batched[p].color)<1e-12);
            check(reference[p].coverage==batched[p].coverage);
            check(reference[p].depth==batched[p].depth);
        }
    }
    std::cout<<"PASS "<<checks<<" fixed-1080 and ordered blending checks (2000 overlapping scenes)\n";
}
