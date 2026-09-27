#include "recomp.h"
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <vector>

extern "C" void conker_object_matrix_group_begin(uint8_t*, recomp_context*);
extern "C" void conker_object_matrix_group_end(uint8_t*, recomp_context*);
extern "C" void conker_shadow_matrix_group_begin(uint8_t*, recomp_context*);
extern "C" void conker_shadow_matrix_group_end(uint8_t*, recomp_context*);

int main() {
    std::vector<uint8_t> memory(1024 * 1024);
    auto* rdram = memory.data();
    const gpr start = gpr(int32_t(0x80001000));
    recomp_context ctx{};
    ctx.r4 = start;
    ctx.r5 = 7;
    ctx.r29 = gpr(int32_t(0x80002000));
    MEM_W(0x168, ctx.r29) = 3;
    conker_object_matrix_group_begin(rdram, &ctx);
    assert(ctx.r4 == start + 24 && ctx.r5 == 7);
    assert(uint32_t(MEM_W(0, start)) == 0xE0525464);
    assert(uint32_t(MEM_W(4, start)) == 0x10000064);
    assert(uint32_t(MEM_W(8, start)) == 0x6400000C);
    const uint32_t objectId = MEM_W(12, start);
    assert(objectId == 0x43030007);
    const uint32_t objectParams = MEM_W(16, start);
    assert(((objectParams >> 17) & 3) == 0); // linear ordering
    assert(((objectParams >> 13) & 3) == 2); // animated vertices
    assert(((objectParams >> 22) & 3) == 0); // retain object UVs
    ctx.r2 = ctx.r4 + 64;
    conker_object_matrix_group_end(rdram, &ctx);
    assert(ctx.r2 == start + 96);
    assert(uint32_t(MEM_W(88, start)) == 0x6400000D);
    assert(MEM_W(92, start) == 1);

    // Reordering objects never changes their identity; kind and index distinguish them.
    for (unsigned object : {7u, 19u, 7u}) {
        ctx.r4 = start; ctx.r5 = object;
        conker_object_matrix_group_begin(rdram, &ctx);
        assert(uint32_t(MEM_W(12, start)) == (0x43030000u | object));
    }
    MEM_W(0x168, ctx.r29) = 4;
    ctx.r4 = start;
    conker_object_matrix_group_begin(rdram, &ctx);
    assert(uint32_t(MEM_W(12, start)) != objectId);

    ctx.r4 = start; ctx.r5 = 7;
    conker_shadow_matrix_group_begin(rdram, &ctx);
    assert(ctx.r4 == start + 24);
    assert(uint32_t(MEM_W(12, start)) == 0x53480007);
    const uint32_t shadowParams = MEM_W(16, start);
    assert(((shadowParams >> 13) & 3) == 0); // static ground
    assert(((shadowParams >> 22) & 3) == 1); // projected shadow UVs
    ctx.r2 = ctx.r4;
    conker_shadow_matrix_group_end(rdram, &ctx);
    assert(ctx.r2 == start + 32);
    assert(uint32_t(MEM_W(24, start)) == 0x6400000D);
    std::puts("PASS PC interpolation hooks: stable IDs, command layout, pairing policy and balanced groups");
}
