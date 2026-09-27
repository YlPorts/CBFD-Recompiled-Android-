// Pure policy tests: no Android device, Vulkan driver or gameplay is simulated as real.
#include "../native/vulkan_surface_policy.hpp"
#include <iostream>
#include <stdexcept>
#include <string>
using namespace conker::android::wsi;
static unsigned checks = 0;
static void check(bool condition, const char* label) {
    if (!condition) throw std::runtime_error(label);
    ++checks;
}
static VkSurfaceCapabilitiesKHR capabilities() {
    VkSurfaceCapabilitiesKHR c{};
    c.minImageCount = 2; c.maxImageCount = 0;
    c.currentExtent = {UINT32_MAX, UINT32_MAX};
    c.minImageExtent = {1,1}; c.maxImageExtent = {4096,4096};
    c.maxImageArrayLayers = 1;
    c.supportedTransforms = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    c.currentTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    c.supportedCompositeAlpha = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
    c.supportedUsageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    return c;
}
int main() {
 try {
    const VkSurfaceFormatKHR rgba{VK_FORMAT_R8G8B8A8_UNORM,VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
    const VkSurfaceFormatKHR bgra{VK_FORMAT_B8G8R8A8_UNORM,VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
    VkSurfaceFormatKHR selected{};
    check(choose_format(&rgba,1,bgra.format,selected) && selected.format==rgba.format,"BGRA request must fall back to advertised RGBA");
    check(choose_format(&bgra,1,rgba.format,selected) && selected.format==bgra.format,"RGBA request must fall back to advertised BGRA");
    VkSurfaceFormatKHR both[]{bgra,rgba};
    check(choose_format(both,2,rgba.format,selected) && selected.format==rgba.format,"Keep requested RGBA when available");
    check(choose_format(both,2,bgra.format,selected) && selected.format==bgra.format,"Keep requested BGRA when available");
    check(!choose_format(nullptr,0,rgba.format,selected) && selected.format==VK_FORMAT_UNDEFINED,"Reject empty query");
    auto unsupported=rgba;unsupported.format=VK_FORMAT_R16G16B16A16_SFLOAT;
    check(!choose_format(&unsupported,1,rgba.format,selected),"Do not assume HDR format is compatible");
    unsupported=rgba;unsupported.colorSpace=VK_COLOR_SPACE_HDR10_ST2084_EXT;
    check(!choose_format(&unsupported,1,rgba.format,selected),"Do not invent an sRGB color-space pair");
    unsupported=rgba;unsupported.format=VK_FORMAT_R8G8B8A8_SRGB;
    check(!choose_format(&unsupported,1,rgba.format,selected),"Reject extra hardware gamma conversion");
    unsupported={VK_FORMAT_UNDEFINED,VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
    check(!choose_format(&unsupported,1,rgba.format,selected),"Do not send undefined format");
    VkSurfaceFormatKHR spaces[]{{rgba.format,VK_COLOR_SPACE_HDR10_ST2084_EXT},rgba};
    check(choose_format(spaces,2,rgba.format,selected) && selected.colorSpace==rgba.colorSpace,"Search all format/color-space pairs");
    SurfaceSettings s{};auto c=capabilities();
    check(choose_settings(c,3,2340,1080,s),"Android color-only surface accepted");
    check(s.imageCount==3,"maxImageCount zero means unbounded, not minImageCount");
    check(s.usage==VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,"Do not require SAMPLED for swapchain");
    check(s.alpha==VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,"Use supported inherit alpha");
    check(s.extent.width==2340 && s.extent.height==1080,"Use actual horizontal size");
    c.maxImageCount=2;check(choose_settings(c,3,2340,1080,s) && s.imageCount==2,"Clamp maximum image count");
    c=capabilities();check(choose_settings(c,1,2340,1080,s) && s.imageCount==2,"Clamp minimum image count");
    c.minImageCount=4;check(choose_settings(c,3,2340,1080,s) && s.imageCount==4,"Honor larger surface minimum");
    c=capabilities();c.currentExtent={1920,1080};check(choose_settings(c,3,2340,1080,s) && s.extent.width==1920,"Honor fixed current extent");
    c=capabilities();c.maxImageExtent={1920,720};check(choose_settings(c,3,2340,1080,s) && s.extent.height==720,"Clamp undefined extent");
    c=capabilities();c.currentExtent={0,0};check(!choose_settings(c,3,2340,1080,s),"Suspend zero-size surface");
    c=capabilities();check(!choose_settings(c,3,0,1080,s),"Suspend zero-size SDL window");
    c=capabilities();c.supportedUsageFlags=VK_IMAGE_USAGE_SAMPLED_BIT;check(!choose_settings(c,3,2340,1080,s),"Require render-target usage");
    c=capabilities();c.supportedUsageFlags|=VK_IMAGE_USAGE_TRANSFER_DST_BIT;check(choose_settings(c,3,2340,1080,s) && s.usage==(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT),"Only supported optional usage");
    c=capabilities();c.supportedCompositeAlpha|=VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;check(choose_settings(c,3,2340,1080,s) && s.alpha==VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,"Prefer opaque");
    c=capabilities();c.supportedCompositeAlpha=VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR;check(choose_settings(c,3,2340,1080,s) && s.alpha==VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,"Handle pre-multiplied-only surface");
    c.supportedCompositeAlpha=VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR;check(choose_settings(c,3,2340,1080,s),"Handle post-multiplied-only surface");
    c.supportedCompositeAlpha=0;check(!choose_settings(c,3,2340,1080,s),"No unsupported alpha flags");
    c=capabilities();c.supportedTransforms=VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR;c.currentTransform=VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR;
    check(choose_settings(c,3,2340,1080,s) && s.transform==VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR,"Fallback to supported transform");
    c.currentTransform=VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;check(!choose_settings(c,3,2340,1080,s),"Reject unavailable transform");
    c=capabilities();c.maxImageCount=1;check(!choose_settings(c,3,2340,1080,s),"Reject invalid image count range");
    c=capabilities();c.minImageExtent.width=5000;check(!choose_settings(c,3,2340,1080,s),"Reject inverted extent bounds");
    c=capabilities();c.maxImageArrayLayers=0;check(!choose_settings(c,3,2340,1080,s),"Require one image layer");
    std::vector<int> values;
    auto query=[](uint32_t* count,int* data){if (!data) *count=3;else {*count=1;data[0]=42;}return VK_SUCCESS;};
    check(enumerate<int>(query,values)==VK_SUCCESS && values.size()==1 && values[0]==42,"Use only returned query values");
    int calls=0;
    auto growing=[&](uint32_t* count,int* data){++calls;if (!data) {*count=2;return VK_SUCCESS;}if(calls==2) return VK_INCOMPLETE;*count=2;data[0]=1;data[1]=2;return VK_SUCCESS;};
    check(enumerate<int>(growing,values)==VK_SUCCESS && calls==4 && values.size()==2,"Retry VK_INCOMPLETE");
    auto zero=[](uint32_t* count,int*){*count=0;return VK_SUCCESS;};
    check(enumerate<int>(zero,values)==VK_ERROR_INITIALIZATION_FAILED && values.empty(),"Reject empty query results");
    auto fail=[](uint32_t*,int*){return VK_ERROR_SURFACE_LOST_KHR;};
    check(enumerate<int>(fail,values)==VK_ERROR_SURFACE_LOST_KHR && values.empty(),"Propagate query failure");
    auto fail_data=[](uint32_t* count,int* data){if(!data){*count=2;return VK_SUCCESS;}return VK_ERROR_DEVICE_LOST;};
    check(enumerate<int>(fail_data,values)==VK_ERROR_DEVICE_LOST && values.empty(),"Drop incomplete values on error");
    auto looping=[](uint32_t* count,int* data){if(!data){*count=2;return VK_SUCCESS;}return VK_INCOMPLETE;};
    check(enumerate<int>(looping,values)==VK_INCOMPLETE && values.empty(),"Bound query retry count");
    check(std::string(result_name(VK_ERROR_SURFACE_LOST_KHR))=="VK_ERROR_SURFACE_LOST_KHR","Decode actual device error");
    std::cout<<"PASS: "<<checks<<" Vulkan surface policy checks (no driver/gameplay test)\n";
 }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}
