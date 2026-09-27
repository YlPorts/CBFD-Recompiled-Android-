#include "mobile_draw_capture.hpp"
#include <cassert>
#include <future>
#include <iostream>
#include <thread>
using namespace std::chrono_literals;
static uint64_t await_request(conker::mobile::DiagnosticMailbox& mailbox) {
    const auto deadline = std::chrono::steady_clock::now() + 2s;
    while (!mailbox.requested() && std::chrono::steady_clock::now() < deadline) std::this_thread::yield();
    const auto id = mailbox.requested(); assert(id); return id;
}
int main() {
    conker::mobile::DiagnosticMailbox mailbox;
    auto a = std::async(std::launch::async, [&] { return mailbox.request(); });
    const auto id = await_request(mailbox);
    assert(mailbox.request(1ms).find("busy") != std::string::npos);
    mailbox.publish(id + 1, "wrong frame");
    mailbox.publish(id, "visible-water");
    assert(a.get() == "visible-water" && !mailbox.requested());
    assert(mailbox.request(1ms).find("timeout") != std::string::npos);
    mailbox.publish(id + 1, "expired");
    auto b = std::async(std::launch::async, [&] { return mailbox.request(); });
    const auto next = await_request(mailbox);
    mailbox.publish(id + 1, "stale completion");
    mailbox.publish(next, "missing-water");
    assert(b.get() == "missing-water");

    // Actual RT64 structures, including behind-camera geometry and transparent alpha.
    struct Fixture {
        RT64::DrawData drawData;
        std::vector<RT64::FramebufferPair> fbPairs;
        uint32_t fbPairCount, gameCallCount;
    } w{};
    w.fbPairs.resize(1); w.fbPairCount = 1; w.gameCallCount = 1;
    auto& fb = w.fbPairs[0]; fb.projections.resize(1); fb.projectionCount = 1;
    auto& p = fb.projections[0]; p.type = RT64::Projection::Type::Perspective;
    p.gameCalls.resize(1); p.gameCallCount = 1;
    auto& g = p.gameCalls[0]; g.meshDesc.faceIndicesStart = 0;
    g.callDesc.callIndex = 42; g.callDesc.triangleCount = 1;
    g.callDesc.otherMode.L = Z_CMP | ZMODE_XLU; g.callDesc.tileCount = 1;
    w.drawData.faceIndices = {0,1,2};
    w.drawData.posTransformed = {hlslpp::float4(0,0,0,-1), hlslpp::float4(0,0,0,2), hlslpp::float4(0,0,0,3)};
    w.drawData.normColBytes = {255,255,255,0, 255,255,255,128, 255,255,255,255};
    w.drawData.rdpTiles.resize(1); w.drawData.callTiles.resize(1);
    w.drawData.callTiles[0].tmemHashOrID = 0x1234; w.drawData.callTiles[0].valid = true;
    const auto text = conker::mobile::describe_workload(w, 99);
    assert(text.find("call=42") != std::string::npos && text.find("srcA=0..255") != std::string::npos);
    assert(text.find("behind=1/3") != std::string::npos && text.find("0000000000001234") != std::string::npos);
    // A large scene must remain bounded and explicitly report missing rows.
    const auto copy = g;
    p.gameCalls.resize(1000, copy); p.gameCallCount = 1000; w.gameCallCount = 1000;
    const auto large = conker::mobile::describe_workload(w, 100);
    assert(large.size() < 48000 && large.find("visited=1000") != std::string::npos);
    assert(large.find("omitted=0 ") == std::string::npos);
    std::cout << "Capture mailbox, timeout/stale completion, RT64 draw-state and bounded output passed.\n";
}
