#pragma once
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <string>
#include "mobile_diagnostics.hpp"
#include "mobile_metrics.hpp"
#include "hle/rt64_workload.h"

namespace conker::mobile {
template <typename Workload>
inline std::string describe_workload(const Workload& workload, uint64_t frame) {
    std::string out;
    out.reserve(44000);
    auto append = [&](const char* format, auto... args) {
        char line[1024];
        const int size = std::snprintf(line, sizeof(line), format, args...);
        if (size > 0) out.append(line, std::min(size, int(sizeof(line) - 1)));
    };
    append("[render-capture] build=texture-capture-013 workload=%llu fbPairs=%u calls=%u CPU-state-before-presentation\n",
        (unsigned long long)frame, workload.fbPairCount, workload.gameCallCount);
    append("horizontalScale=%.4f specialized=%llu uber=%llu; om/cc=H/L; alpha=prim/env/threshold; srcA/clipW are original CPU vertices, not final GPU alpha/depth\n",
        metrics.horizontalAspect.load(), (unsigned long long)metrics.specializedDraws.load(), (unsigned long long)metrics.uberDraws.load());
    const auto& data = workload.drawData;
    uint32_t rows = 0, visited = 0;
    uint64_t verticesScanned = 0;
    // Prioritize alpha/depth-read-only calls (water, foliage, particles) if the budget is exhausted.
    for (int priority = 0; priority < 2; ++priority) {
        for (uint32_t f = 0; f < workload.fbPairCount; ++f) {
            const auto& fb = workload.fbPairs[f];
            for (uint32_t p = 0; p < fb.projectionCount; ++p) {
                const auto& proj = fb.projections[p];
                for (uint32_t d = 0; d < proj.gameCallCount; ++d) {
                    const auto& game = proj.gameCalls[d];
                    const auto& call = game.callDesc;
                    const auto& om = call.otherMode;
                    const bool alpha = !om.zUpd() || om.zMode() == ZMODE_XLU || om.alphaCompare() != G_AC_NONE;
                    if (int(!alpha) != priority) continue;
                    ++visited;
                    if (out.size() > 42000) continue;
                    float minW = std::numeric_limits<float>::infinity(), maxW = -minW;
                    unsigned minAlpha = 255, maxAlpha = 0, behind = 0, invalid = 0, scanned = 0;
                    const bool indexed = proj.type == RT64::Projection::Type::Perspective || proj.type == RT64::Projection::Type::Orthographic;
                    const uint64_t start = indexed ? game.meshDesc.faceIndicesStart : 0;
                    const uint64_t end = start + uint64_t(call.triangleCount) * 3;
                    if (indexed && end <= data.faceIndices.size()) {
                        for (size_t i = start; i < end && verticesScanned < 200000; ++i, ++verticesScanned) {
                            const size_t v = data.faceIndices[i];
                            if (v >= data.posTransformed.size() || v * 4 + 3 >= data.normColBytes.size()) { ++invalid; continue; }
                            const float w = data.posTransformed[v][3];
                            if (std::isfinite(w)) { minW = std::min(minW, w); maxW = std::max(maxW, w); behind += w <= 0; }
                            else ++invalid;
                            minAlpha = std::min(minAlpha, unsigned(data.normColBytes[v * 4 + 3]));
                            maxAlpha = std::max(maxAlpha, unsigned(data.normColBytes[v * 4 + 3]));
                            ++scanned;
                        }
                    }
                    const auto& rp = call.rdpParams;
                    append("call=%u fb=%u p=%u type=%u tris=%u om=%08x/%08x cc=%08x/%08x geom=%08x NoN=%u tiles=%u scissor=%d,%d,%d,%d alpha=%.3f/%.3f/%.3f srcA=%u..%u clipW=%.4g..%.4g behind=%u/%u invalid=%u\n",
                        call.callIndex, f, p, unsigned(proj.type), call.triangleCount, om.H, om.L,
                        call.colorCombiner.H, call.colorCombiner.L, call.geometryMode, unsigned(call.NoN), call.tileCount,
                        call.scissorRect.ulx, call.scissorRect.uly, call.scissorRect.lrx, call.scissorRect.lry,
                        float(rp.primColor.w), float(rp.envColor.w), float(rp.blendColor.w),
                        minAlpha, maxAlpha, scanned ? minW : 0.0f, scanned ? maxW : 0.0f, behind, scanned, invalid);
                    for (uint32_t t = 0; t < std::min(call.tileCount, 2U); ++t) {
                        const size_t index = size_t(call.tileIndex) + t;
                        if (index >= data.callTiles.size() || index >= data.rdpTiles.size()) continue;
                        const auto& tile = data.callTiles[index];
                        const auto& rdp = data.rdpTiles[index];
                        append(" tile%u hash=%016llx size=%ux%u fmt=%u/%u valid=%u raw=%u copy=%u native=%u mode=%u/%u\n",
                            t, (unsigned long long)tile.tmemHashOrID, unsigned(tile.sampleWidth), unsigned(tile.sampleHeight),
                            rdp.fmt, rdp.siz, unsigned(tile.valid), unsigned(tile.rawTMEM), unsigned(tile.tileCopyUsed), rdp.nativeSampler, rdp.cms, rdp.cmt);
                    }
                    ++rows;
                }
            }
        }
    }
    append("[render-capture-end] rows=%u visited=%u omitted=%u verticesScanned=%llu\n", rows, visited, visited - rows, (unsigned long long)verticesScanned);
    return out;
}
inline void capture_workload_if_requested(const RT64::Workload& workload, uint64_t frame) {
    const uint64_t id = diagnostics.requested();
    if (id) diagnostics.publish(id, describe_workload(workload, frame));
}
}
