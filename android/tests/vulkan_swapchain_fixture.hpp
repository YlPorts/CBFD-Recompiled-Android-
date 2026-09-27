// Mock platform resources for unit-testing the actual patched Plume swapchain methods.
// NOT a GPU test, Android runtime, game renderer or distributable library.
#pragma once
#include <vulkan/vulkan.h>
#include "../native/vulkan_surface_policy.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <memory>
#include <mutex>
#include <set>
#include <stdexcept>
#include <vector>
#define __ANDROID__ 1
#define PLUME_SDL_VULKAN_ENABLED 1
struct SDL_Window { int width=2340,height=1080; };
using SDL_bool=int;
constexpr SDL_bool SDL_FALSE=0;
namespace fixture {
template<class T> T handle(uintptr_t n) { return reinterpret_cast<T>(n); }
struct Driver {
 VkSurfaceCapabilitiesKHR caps{};
 std::vector<VkSurfaceFormatKHR> formats{{VK_FORMAT_R8G8B8A8_UNORM,VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}};
 VkResult queryResult=VK_SUCCESS,createResult=VK_SUCCESS;
 bool failView=false;
 unsigned creates=0,destroys=0,presents=0,acquires=0,badHandles=0;
 VkSwapchainCreateInfoKHR last{};
 std::set<VkSwapchainKHR> live;
 Driver() {
  caps.minImageCount=2;caps.maxImageCount=0;caps.maxImageArrayLayers=1;
  caps.minImageExtent={1,1};caps.maxImageExtent={4096,4096};caps.currentExtent={UINT32_MAX,UINT32_MAX};
  caps.supportedCompositeAlpha=VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
  caps.supportedTransforms=VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
  caps.currentTransform=VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
  caps.supportedUsageFlags=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
 }
} inline driver;
inline unsigned checks=0;
inline void check(bool condition,const char* label) { if(!condition)throw std::runtime_error(label);++checks; }
}
inline SDL_bool SDL_Vulkan_CreateSurface(SDL_Window*,VkInstance,VkSurfaceKHR* out) {*out=fixture::handle<VkSurfaceKHR>(2);return 1;}
inline const char* SDL_GetError() {return "mock SDL";}
inline void SDL_GetWindowSizeInPixels(SDL_Window* w,int* x,int* y) {*x=w->width;*y=w->height;}
VKAPI_ATTR VkResult VKAPI_CALL vkGetPhysicalDeviceSurfaceSupportKHR(VkPhysicalDevice,uint32_t,VkSurfaceKHR,VkBool32* out) {*out=VK_TRUE;return VK_SUCCESS;}
VKAPI_ATTR VkResult VKAPI_CALL vkGetPhysicalDeviceSurfaceCapabilitiesKHR(VkPhysicalDevice,VkSurfaceKHR,VkSurfaceCapabilitiesKHR* out) {*out=fixture::driver.caps;return fixture::driver.queryResult;}
VKAPI_ATTR VkResult VKAPI_CALL vkGetPhysicalDeviceSurfaceFormatsKHR(VkPhysicalDevice,VkSurfaceKHR,uint32_t* count,VkSurfaceFormatKHR* data) {
 if(fixture::driver.queryResult!=VK_SUCCESS)return fixture::driver.queryResult;
 if(data)std::copy(fixture::driver.formats.begin(),fixture::driver.formats.end(),data);
 *count=uint32_t(fixture::driver.formats.size());return VK_SUCCESS;
}
VKAPI_ATTR VkResult VKAPI_CALL vkGetPhysicalDeviceSurfacePresentModesKHR(VkPhysicalDevice,VkSurfaceKHR,uint32_t* count,VkPresentModeKHR* data) {*count=1;if(data)*data=VK_PRESENT_MODE_FIFO_KHR;return VK_SUCCESS;}
VKAPI_ATTR VkResult VKAPI_CALL vkCreateSwapchainKHR(VkDevice,const VkSwapchainCreateInfoKHR* info,const VkAllocationCallbacks*,VkSwapchainKHR* out) {
 auto& d=fixture::driver;++d.creates;d.last=*info;
 if(d.createResult!=VK_SUCCESS)return d.createResult;
 *out=fixture::handle<VkSwapchainKHR>(100+d.creates);d.live.insert(*out);return VK_SUCCESS;
}
VKAPI_ATTR void VKAPI_CALL vkDestroySwapchainKHR(VkDevice,VkSwapchainKHR chain,const VkAllocationCallbacks*) {auto& d=fixture::driver;++d.destroys;if(!d.live.erase(chain))++d.badHandles;}
VKAPI_ATTR void VKAPI_CALL vkDestroySurfaceKHR(VkInstance,VkSurfaceKHR,const VkAllocationCallbacks*) {}
VKAPI_ATTR VkResult VKAPI_CALL vkGetSwapchainImagesKHR(VkDevice,VkSwapchainKHR chain,uint32_t* count,VkImage* data) {
 auto& d=fixture::driver;if(!d.live.count(chain)){++d.badHandles;return VK_ERROR_SURFACE_LOST_KHR;}
 *count=d.last.minImageCount;
 if(data)for(uint32_t i=0;i<*count;++i)data[i]=fixture::handle<VkImage>(300+i);
 return VK_SUCCESS;
}
VKAPI_ATTR void VKAPI_CALL vkDestroyImageView(VkDevice,VkImageView,const VkAllocationCallbacks*) {}
VKAPI_ATTR VkResult VKAPI_CALL vkQueuePresentKHR(VkQueue,const VkPresentInfoKHR* info) {auto& d=fixture::driver;++d.presents;if(!d.live.count(*info->pSwapchains))++d.badHandles;return VK_SUCCESS;}
VKAPI_ATTR VkResult VKAPI_CALL vkAcquireNextImageKHR(VkDevice,VkSwapchainKHR chain,uint64_t,VkSemaphore,VkFence,uint32_t* out) {auto& d=fixture::driver;++d.acquires;if(!d.live.count(chain))++d.badHandles;*out=0;return VK_SUCCESS;}
VKAPI_ATTR VkResult VKAPI_CALL vkWaitForPresentKHR(VkDevice,VkSwapchainKHR,uint64_t,uint64_t) {return VK_SUCCESS;}
VKAPI_ATTR VkResult VKAPI_CALL vkGetRefreshCycleDurationGOOGLE(VkDevice,VkSwapchainKHR,VkRefreshCycleDurationGOOGLE* out) {out->refreshDuration=16666667;return VK_SUCCESS;}
namespace plume {
enum class RenderFormat {UNKNOWN,R8G8B8A8_UNORM,B8G8R8A8_UNORM};
inline VkFormat toVk(RenderFormat format) {return format==RenderFormat::R8G8B8A8_UNORM?VK_FORMAT_R8G8B8A8_UNORM:VK_FORMAT_B8G8R8A8_UNORM;}
using RenderWindow=SDL_Window*;
struct RenderSwapChainDesc {RenderWindow renderWindow=nullptr;RenderFormat format=RenderFormat::B8G8R8A8_UNORM;uint32_t textureCount=3;bool enablePresentWait=false;uint32_t maxFrameLatency=1;};
enum class RenderTextureDimension {TEXTURE_2D};
enum class RenderTextureFlag {RENDER_TARGET};
struct RenderTexture {virtual ~RenderTexture()=default;};
struct RenderCommandSemaphore {virtual ~RenderCommandSemaphore()=default;};
struct VulkanCommandSemaphore : RenderCommandSemaphore {VkSemaphore vk=fixture::handle<VkSemaphore>(5);};
struct VulkanInterface {VkInstance instance=fixture::handle<VkInstance>(1);};
struct VulkanDevice {VkDevice vk=fixture::handle<VkDevice>(3);VkPhysicalDevice physicalDevice=fixture::handle<VkPhysicalDevice>(4);VulkanInterface* renderInterface;struct {bool presentWait=false;} capabilities;};
struct VulkanSwapChain;
struct VulkanQueue {std::mutex* mutex;VkQueue vk=fixture::handle<VkQueue>(6);};
struct VulkanCommandQueue {VulkanDevice* device;VulkanQueue* queue;uint32_t familyIndex=0;std::set<VulkanSwapChain*> swapChains;};
struct VulkanTexture : RenderTexture {
 struct {RenderTextureDimension dimension{};RenderFormat format{};uint32_t width=0,height=0,depth=0,mipLevels=0,arraySize=0;RenderTextureFlag flags{};} desc;
 VkImageView imageView=VK_NULL_HANDLE;
 VulkanTexture()=default;VulkanTexture(VulkanDevice*,VkImage){}
 void fillSubresourceRange(){}
 void createImageView(VkFormat f){fixture::check(f==toVk(desc.format),"image view/descriptor formats disagree");if(!fixture::driver.failView)imageView=fixture::handle<VkImageView>(7);}
};
}
