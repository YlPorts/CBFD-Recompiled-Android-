// Android presentation policy shared by Plume and ROM-free host regression tests.
// Keep UNORM output: Conker's VI shader already applies the game's gamma curve.
#pragma once
#include <vulkan/vulkan.h>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace conker::android::wsi {
inline bool choose_format(const VkSurfaceFormatKHR* formats, size_t count,
                          VkFormat requested, VkSurfaceFormatKHR& picked) {
    picked = {};
    const VkFormat candidates[] = { requested, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_B8G8R8A8_UNORM };
    for (const auto candidate : candidates) {
        if (candidate != VK_FORMAT_R8G8B8A8_UNORM && candidate != VK_FORMAT_B8G8R8A8_UNORM) continue;
        for (size_t i = 0; formats && i < count; ++i) {
            if (formats[i].format == candidate && formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                picked = formats[i];
                return true;
            }
        }
    }
    return false; // Never send an undefined or invented format/color-space pair to the driver.
}

struct SurfaceSettings {
    uint32_t imageCount = 0;
    VkExtent2D extent{};
    VkImageUsageFlags usage = 0;
    VkSurfaceTransformFlagBitsKHR transform{};
    VkCompositeAlphaFlagBitsKHR alpha{};
};
inline bool choose_settings(const VkSurfaceCapabilitiesKHR& caps, uint32_t requestedCount,
                            uint32_t width, uint32_t height, SurfaceSettings& picked) {
    picked = {};
    if (!width || !height || !caps.minImageCount || !caps.maxImageArrayLayers ||
        (caps.maxImageCount && caps.maxImageCount < caps.minImageCount) ||
        caps.minImageExtent.width > caps.maxImageExtent.width ||
        caps.minImageExtent.height > caps.maxImageExtent.height ||
        !(caps.supportedUsageFlags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)) return false;
    SurfaceSettings next{};
    next.imageCount = std::max(requestedCount, caps.minImageCount);
    if (caps.maxImageCount) next.imageCount = std::min(next.imageCount, caps.maxImageCount);
    if (caps.currentExtent.width != UINT32_MAX && caps.currentExtent.height != UINT32_MAX) {
        next.extent = caps.currentExtent;
    } else {
        next.extent = {std::clamp(width, caps.minImageExtent.width, caps.maxImageExtent.width),
                       std::clamp(height, caps.minImageExtent.height, caps.maxImageExtent.height)};
    }
    if (!next.extent.width || !next.extent.height ||
        next.extent.width < caps.minImageExtent.width || next.extent.width > caps.maxImageExtent.width ||
        next.extent.height < caps.minImageExtent.height || next.extent.height > caps.maxImageExtent.height) return false;
    // PresentQueue writes these images as render targets; it does not sample from them.
    next.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    next.usage |= caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    if (caps.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) {
        next.transform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    } else if (caps.currentTransform && (caps.supportedTransforms & caps.currentTransform)) {
        next.transform = caps.currentTransform;
    } else return false;
    const VkCompositeAlphaFlagBitsKHR choices[] = {
        VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR, VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
        VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR, VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR
    };
    for (const auto alpha : choices) {
        if (caps.supportedCompositeAlpha & alpha) {
            next.alpha = alpha;
            picked = next;
            return true;
        }
    }
    return false;
}

// The count may change between queries. Never consume zero-filled/unreturned entries.
template<class T, class Query>
VkResult enumerate(Query query, std::vector<T>& values) {
    values.clear();
    for (unsigned attempt = 0; attempt < 4; ++attempt) {
        uint32_t count = 0;
        VkResult result = query(&count, nullptr);
        if (result != VK_SUCCESS) return result;
        if (!count) return VK_ERROR_INITIALIZATION_FAILED;
        values.resize(count);
        result = query(&count, values.data());
        if (result == VK_SUCCESS && count && count <= values.size()) {
            values.resize(count);
            return VK_SUCCESS;
        }
        values.clear();
        if (result != VK_INCOMPLETE) return result == VK_SUCCESS ? VK_ERROR_INITIALIZATION_FAILED : result;
    }
    return VK_INCOMPLETE;
}

inline const char* result_name(VkResult result) {
    switch (result) {
    case VK_SUCCESS: return "VK_SUCCESS";
    case VK_SUBOPTIMAL_KHR: return "VK_SUBOPTIMAL_KHR";
    case VK_ERROR_SURFACE_LOST_KHR: return "VK_ERROR_SURFACE_LOST_KHR";
    case VK_ERROR_OUT_OF_DATE_KHR: return "VK_ERROR_OUT_OF_DATE_KHR";
    case VK_ERROR_FORMAT_NOT_SUPPORTED: return "VK_ERROR_FORMAT_NOT_SUPPORTED";
    case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
    case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST";
    case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
    case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
    default: return "VkResult";
    }
}
}
