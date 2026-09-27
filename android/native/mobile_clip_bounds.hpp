#pragma once
#include <algorithm>
#include <array>
#include <cmath>

namespace conker::mobile {
struct ClipPoint { double x,y,w; };
struct ClipRect { double left,top,right,bottom; };
enum class ClipResult { Empty, Visible, Invalid };
// Clip in HOMOGENEOUS screen coordinates, BEFORE perspective division. The
// polygon cannot exceed eight vertices (triangle + five clipping planes).
// No heap allocation, no per-triangle atomic counter and no GPU topology change.
inline ClipResult clip_bounds(const std::array<ClipPoint,3>& triangle,ClipRect limit,ClipRect& out,bool expandedHorizontal=false) {
    if (!(limit.left<limit.right && limit.top<limit.bottom)) return ClipResult::Empty;
    std::array<ClipPoint,12> a{},b{};size_t n=3;
    for(size_t i=0;i<3;++i) {
        a[i]=triangle[i];
        if(!std::isfinite(a[i].x)||!std::isfinite(a[i].y)||!std::isfinite(a[i].w)) return ClipResult::Invalid;
    }
    if(a[0].w<=0 && a[1].w<=0 && a[2].w<=0) return ClipResult::Empty;
    for(int plane=0;plane<5;++plane) {
        // CPU vertices retain the original 4:3 projection; RT64 expands X on
        // the GPU. In Expand mode do not cull using that narrower X interval.
        if(expandedHorizontal && (plane==1 || plane==2)) continue;
        auto distance=[&](ClipPoint p) {
            switch(plane) {
                case 0:return p.w-1e-6;
                case 1:return p.x-limit.left*p.w;
                case 2:return limit.right*p.w-p.x;
                case 3:return p.y-limit.top*p.w;
                default:return limit.bottom*p.w-p.y;
            }
        };
        size_t next=0;ClipPoint prev=a[n-1];double pd=distance(prev);
        for(size_t i=0;i<n;++i) {
            ClipPoint cur=a[i];double cd=distance(cur);
            if((pd>=0)!=(cd>=0)) {
                double t=pd/(pd-cd);
                if(next>=b.size()) return ClipResult::Invalid;
                b[next++]={prev.x+(cur.x-prev.x)*t,prev.y+(cur.y-prev.y)*t,prev.w+(cur.w-prev.w)*t};
            }
            if(cd>=0) { if(next>=b.size()) return ClipResult::Invalid;b[next++]=cur; }
            prev=cur;pd=cd;
        }
        n=next;if(n==0) return ClipResult::Empty;a=b;
    }
    out={limit.right,limit.bottom,limit.left,limit.top};
    for(size_t i=0;i<n;++i) {
        double x=a[i].x/a[i].w,y=a[i].y/a[i].w;
        if(!std::isfinite(x)||!std::isfinite(y)) return ClipResult::Invalid;
        out.left=std::min(out.left,x);out.top=std::min(out.top,y);
        out.right=std::max(out.right,x);out.bottom=std::max(out.bottom,y);
    }
    if(expandedHorizontal) { out.left=limit.left;out.right=limit.right; }
    out.left=std::max(out.left,limit.left);out.top=std::max(out.top,limit.top);
    out.right=std::min(out.right,limit.right);out.bottom=std::min(out.bottom,limit.bottom);
    return ClipResult::Visible;
}
}
