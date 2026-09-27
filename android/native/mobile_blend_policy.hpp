#pragma once
#include <cstdint>
#include <cstring>

namespace conker::mobile {

// Samsung's Mali-G57 driver advertises dual-source blending on the test device,
// but the river captures keep reaching valid translucent draws while the final
// water intermittently disappears. Keep the general RT64 capability path and
// only route this GPU family through the already-tested single-source RGB /
// coverage implementation. This is a rendering compatibility policy, not a
// resolution or game-speed change.
inline bool mali_g57_dual_source_workaround(uint32_t vendorId, const char* deviceName) {
    constexpr uint32_t ArmVendorId = 0x13B5u;
    return vendorId == ArmVendorId && deviceName != nullptr &&
        std::strstr(deviceName, "Mali-G57") != nullptr;
}

} // namespace conker::mobile
