#include <cstdlib>
#include <cfloat>
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>
#include <iostream>
#include <random>
#include "common/rt64_math.h"
#include "hle/rt64_rigid_body.h"
#include "../include/rt64_extended_gbi.h"
#include "mobile_match_policy.hpp"
// This file is produced by the test runner from the actual checked-out RT64
// implementation, not a second manually translated reference matcher.
#include "reference_matcher.inc"
static bool identical(const hlslpp::float4x4& a,const hlslpp::float4x4& b) {
    for(int r=0;r<4;r++)for(int c=0;c<4;c++) { float x=a[r][c],y=b[r][c];if(std::memcmp(&x,&y,sizeof(float)))return false; }
    return true;
}
int main() {
    using namespace conker::mobile;
    std::mt19937 rng(42);std::uniform_real_distribution<float> small(-.5f,.5f),move(-100,100);
    unsigned assertions=0;
    for(int t=0;t<2000;t++) {
        float cur[16]{},prev[16]{},proj[16]{},velocity[3]{};
        for(int i=0;i<3;i++){cur[i*4+i]=prev[i*4+i]=1.f+std::abs(small(rng));proj[i*4+i]=1;}
        cur[15]=prev[15]=proj[15]=1;
        // Stable non-singular affine pairs, including shear and scale. This
        // retains the previous score, apart from numeric safety at singularities.
        for(int i:{1,2,4,6,8,9}){cur[i]=small(rng);prev[i]=small(rng);}
        for(int i=0;i<3;i++){cur[12+i]=move(rng);prev[12+i]=move(rng);velocity[i]=small(rng);}
        hlslpp::float4x4 c,p,v;
        for(int row=0;row<4;row++)for(int col=0;col<4;col++){c[row][col]=cur[row*4+col];p[row][col]=prev[row*4+col];v[row][col]=proj[row*4+col];}
        RT64::RigidBody rb;rb.linearVelocity=hlslpp::float3(velocity[0],velocity[1],velocity[2]);
        const auto reference=RT64::computeTransformMatch(c,v,p,v,&rb);
        const auto cached=match_difference(describe_transform(cur,proj,velocity),describe_transform(prev,proj,velocity));
        // Cur predicted velocity is intentionally ignored in match_difference.
        if(reference.valid){ assert(std::abs(reference.computeDifference()-cached)<.001f+std::abs(cached)*1e-5f);++assertions; }
        // Test the production RigidBody path. Arbitrary decompositions must not
        // perturb the exact current frame, even for skinning/sheared matrices.
        rb.updateDecomposition(p,true);rb.updateDecomposition(c,true);
        rb.lerpTranslation=rb.lerpRotation=rb.lerpScale=rb.lerpSkew=true;
        assert(identical(rb.lerp(1.f,p,c,true),c));++assertions;
        const auto middle=rb.lerp(.5f,p,c,true);assert(!RT64::matrixIsNaN(middle));++assertions;
    }
    hlslpp::float4x4 identity=hlslpp::float4x4::identity();RT64::RigidBody rb;
    rb.updateAngular(identity,identity,G_EX_COMPONENT_AUTO,G_EX_COMPONENT_AUTO,G_EX_COMPONENT_AUTO);
    assert(std::isfinite(rb.angularVelocity));++assertions;
    std::cout<<"PASS "<<assertions<<" comparisons using the actual RT64 matcher and RigidBody source (host, not Mali)\n";
}
