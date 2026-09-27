#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <utility>

namespace conker::mobile {
// Auto matching must not confuse different meshes simply because their RDP state
// and number of triangles coincide. Explicit game-supplied IDs remain authoritative.
inline bool same_mesh(uint64_t currentHash, uint32_t currentCount,
                      uint64_t previousHash, uint32_t previousCount) {
    return currentCount != 0 && currentCount == previousCount && currentHash == previousHash;
}
// Dense, indistinguishable particle/material buckets used to create a Cartesian
// product. Restrict their ordinal search, leaving unmatched draws at the CURRENT
// transform rather than inventing a match. Ordinary buckets remain exhaustive.
inline std::pair<size_t,size_t> candidate_window(size_t ordinal, size_t currentCount,
                                                size_t previousCount) {
    if (previousCount <= 24) return {0,previousCount};
    if (!currentCount || !previousCount) return {0,0};
    const size_t center=std::min(previousCount-1, ordinal*previousCount/currentCount);
    const size_t begin=center>8?center-8:0;
    return {begin,std::min(previousCount,center+9)};
}
struct MatchDescriptor {
    float position[3]{}, predicted[3]{}, screen[3]{}, axes[3][3]{}, determinant=0;
    bool valid=false;
};
// Built ONCE per transform, not for every candidate pair. Matrix layout follows
// hlsl++ rows as used by the upstream matcher (translation in row 3).
inline MatchDescriptor describe_transform(const float m[16], const float vp[16], const float velocity[3]) {
    MatchDescriptor d;
    for (int i=0;i<16;++i) if (!std::isfinite(m[i]) || !std::isfinite(vp[i])) return d;
    d.determinant=m[0]*(m[5]*m[10]-m[6]*m[9])-m[1]*(m[4]*m[10]-m[6]*m[8])+m[2]*(m[4]*m[9]-m[5]*m[8]);
    if (!std::isfinite(d.determinant) || std::abs(d.determinant)<1e-12f) return d;
    for (int r=0;r<3;++r) {
        float len=std::sqrt(m[r*4]*m[r*4]+m[r*4+1]*m[r*4+1]+m[r*4+2]*m[r*4+2]);
        if (!(len>1e-12f) || !std::isfinite(len)) return d;
        for(int c=0;c<3;++c) d.axes[r][c]=m[r*4+c]/len;
        d.position[r]=m[12+r];
        d.predicted[r]=d.position[r]+velocity[r];
        if (!std::isfinite(d.predicted[r])) return d;
    }
    float projected[4]{};
    for(int c=0;c<4;++c) for(int r=0;r<4;++r) projected[c]+=m[12+r]*vp[r*4+c];
    for(int c=0;c<3;++c) {
        d.screen[c]=std::abs(projected[3])<1e-6f?projected[c]:projected[c]/projected[3];
        if (!std::isfinite(d.screen[c])) return d;
    }
    d.valid=true;return d;
}
inline float match_difference(const MatchDescriptor& cur,const MatchDescriptor& prev) {
    if(!cur.valid||!prev.valid || (std::signbit(cur.determinant)!=std::signbit(prev.determinant)))
        return std::numeric_limits<float>::infinity();
    float position=0,screen=0,orientation=0;
    for(int c=0;c<3;++c) { float p=cur.position[c]-prev.predicted[c], s=cur.screen[c]-prev.screen[c]; position+=p*p;screen+=s*s; }
    for(int r=0;r<3;++r) {
        float dot=0; for(int c=0;c<3;++c) dot+=cur.axes[r][c]*prev.axes[r][c];
        orientation+=1.f-std::clamp(dot,-1.f,1.f);
    }
    return std::sqrt(position)+std::sqrt(screen)+orientation;
}
}
