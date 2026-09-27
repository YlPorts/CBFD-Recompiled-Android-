// Executes the actual DXC-generated RSPModify shader through a Vulkan driver.
// Synthetic vertex input only; no game assets or ROM are required.
#include <vulkan/vulkan.h>
#include <vector>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <cmath>
#include "rt64_screen_modify.h"
static void check(VkResult result,const char* what) { if(result!=VK_SUCCESS)throw std::runtime_error(std::string(what)+": "+std::to_string(result)); }
struct Buffer { VkBuffer handle{};VkDeviceMemory memory{};void* mapped{};VkDeviceSize size{}; };
int main(int argc,char** argv) { try {
    if(argc<2)throw std::runtime_error("Usage: vulkan_modify_probe <RSPModifyCS.spv> [old]");
    bool old=argc>2;
    VkApplicationInfo ai{VK_STRUCTURE_TYPE_APPLICATION_INFO};ai.pApplicationName="Conker real shader probe";ai.apiVersion=VK_API_VERSION_1_0;
    VkInstanceCreateInfo ici{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};ici.pApplicationInfo=&ai;VkInstance instance{};check(vkCreateInstance(&ici,nullptr,&instance),"instance");
    uint32_t n=0;check(vkEnumeratePhysicalDevices(instance,&n,nullptr),"physical count");if(!n)throw std::runtime_error("No Vulkan device");
    std::vector<VkPhysicalDevice> devices(n);check(vkEnumeratePhysicalDevices(instance,&n,devices.data()),"physical devices");auto physical=devices[0];
    VkPhysicalDeviceProperties props{};vkGetPhysicalDeviceProperties(physical,&props);std::printf("Vulkan device: %s, api=%u.%u.%u\n",props.deviceName,VK_VERSION_MAJOR(props.apiVersion),VK_VERSION_MINOR(props.apiVersion),VK_VERSION_PATCH(props.apiVersion));
    VkPhysicalDeviceFeatures features{};vkGetPhysicalDeviceFeatures(physical,&features);std::printf("depthClamp=%u shaderInt16=%u shaderInt64=%u geometry=%u\n",features.depthClamp,features.shaderInt16,features.shaderInt64,features.geometryShader);
    vkGetPhysicalDeviceQueueFamilyProperties(physical,&n,nullptr);std::vector<VkQueueFamilyProperties> families(n);vkGetPhysicalDeviceQueueFamilyProperties(physical,&n,families.data());
    uint32_t family=0;for(;family<n;++family)if(families[family].queueFlags&VK_QUEUE_COMPUTE_BIT)break;if(family==n)throw std::runtime_error("No compute queue");
    float priority=1;VkDeviceQueueCreateInfo qci{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};qci.queueFamilyIndex=family;qci.queueCount=1;qci.pQueuePriorities=&priority;
    VkDeviceCreateInfo dci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};dci.queueCreateInfoCount=1;dci.pQueueCreateInfos=&qci;VkDevice device{};check(vkCreateDevice(physical,&dci,nullptr,&device),"device");VkQueue queue{};vkGetDeviceQueue(device,family,0,&queue);
    VkPhysicalDeviceMemoryProperties memoryProperties{};vkGetPhysicalDeviceMemoryProperties(physical,&memoryProperties);
    auto buffer=[&](VkDeviceSize size,VkBufferUsageFlags usage) {
        Buffer b;b.size=size;VkBufferCreateInfo ci{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};ci.size=size;ci.usage=usage;ci.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
        check(vkCreateBuffer(device,&ci,nullptr,&b.handle),"buffer");VkMemoryRequirements req;vkGetBufferMemoryRequirements(device,b.handle,&req);
        uint32_t type=0;for(;type<memoryProperties.memoryTypeCount;++type)if((req.memoryTypeBits&(1u<<type))&&((memoryProperties.memoryTypes[type].propertyFlags&(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))==(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)))break;
        if(type==memoryProperties.memoryTypeCount)throw std::runtime_error("No host-coherent test memory");
        VkMemoryAllocateInfo ma{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};ma.allocationSize=req.size;ma.memoryTypeIndex=type;check(vkAllocateMemory(device,&ma,nullptr,&b.memory),"memory");check(vkBindBufferMemory(device,b.handle,b.memory,0),"bind");check(vkMapMemory(device,b.memory,0,size,0,&b.mapped),"map");return b;
    };
    constexpr size_t N=256;std::vector<std::array<float,4>> positions(N),expected(N);std::vector<uint32_t> words;RT64::ScreenModifyRecords<N> records;
    for(size_t i=0;i<N;++i)positions[i]=expected[i]={float(i)+.125f,float(i)-.5f,.375f,1.f+float(i)*.01f};
    for(uint32_t i=0;i<N;++i){
        const auto xy=(uint32_t(uint16_t(int16_t(int(i)*7-800)))<<16)|uint16_t(int16_t(900-int(i)*9));
        const uint32_t z=(i*4001u+0x01234567u)%0x03FF0001u;
        if(old)words.insert(words.end(),{i<<1,xy,(i<<1)|1,z});
        else {records.update(i,i,false,0xFFFF0000u,words);records.update(i,i,true,z,words);records.update(i,i,false,xy,words);}
        expected[i][0]=float(int16_t(xy>>16))*.25f;expected[i][1]=float(int16_t(xy))*.25f;expected[i][2]=RT64::normalizedScreenDepth(z);
    }
    Buffer source=buffer(words.size()*4,VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT),out=buffer(positions.size()*16,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    std::memcpy(source.mapped,words.data(),source.size);std::memcpy(out.mapped,positions.data(),out.size);
    VkBufferViewCreateInfo bvi{VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO};bvi.buffer=source.handle;bvi.format=VK_FORMAT_R32_UINT;bvi.range=source.size;VkBufferView view{};check(vkCreateBufferView(device,&bvi,nullptr,&view),"view");
    VkDescriptorSetLayoutBinding bindings[2]={{1,VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr},{2,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr}};
    VkDescriptorSetLayoutCreateInfo sl{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};sl.bindingCount=2;sl.pBindings=bindings;VkDescriptorSetLayout setLayout{};check(vkCreateDescriptorSetLayout(device,&sl,nullptr,&setLayout),"set layout");
    VkPushConstantRange pc{VK_SHADER_STAGE_COMPUTE_BIT,0,4};VkPipelineLayoutCreateInfo pl{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};pl.setLayoutCount=1;pl.pSetLayouts=&setLayout;pl.pushConstantRangeCount=1;pl.pPushConstantRanges=&pc;VkPipelineLayout layout{};check(vkCreatePipelineLayout(device,&pl,nullptr,&layout),"pipeline layout");
    std::ifstream stream(argv[1],std::ios::binary|std::ios::ate);if(!stream)throw std::runtime_error("Missing shader");size_t size=stream.tellg();if(size%4)throw std::runtime_error("Invalid shader size");std::vector<uint32_t> code(size/4);stream.seekg(0);stream.read((char*)code.data(),size);
    VkShaderModuleCreateInfo sm{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};sm.codeSize=size;sm.pCode=code.data();VkShaderModule module{};check(vkCreateShaderModule(device,&sm,nullptr,&module),"shader module");
    VkComputePipelineCreateInfo cp{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};cp.layout=layout;cp.stage={VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};cp.stage.stage=VK_SHADER_STAGE_COMPUTE_BIT;cp.stage.module=module;cp.stage.pName="CSMain";VkPipeline pipeline{};check(vkCreateComputePipelines(device,VK_NULL_HANDLE,1,&cp,nullptr,&pipeline),"compute pipeline");
    VkDescriptorPoolSize sizes[2]={{VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,1},{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1}};VkDescriptorPoolCreateInfo dpi{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};dpi.maxSets=1;dpi.poolSizeCount=2;dpi.pPoolSizes=sizes;VkDescriptorPool pool{};check(vkCreateDescriptorPool(device,&dpi,nullptr,&pool),"descriptor pool");
    VkDescriptorSetAllocateInfo ds{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};ds.descriptorPool=pool;ds.descriptorSetCount=1;ds.pSetLayouts=&setLayout;VkDescriptorSet set{};check(vkAllocateDescriptorSets(device,&ds,&set),"descriptor set");
    VkDescriptorBufferInfo outInfo{out.handle,0,out.size};VkWriteDescriptorSet writes[2]{};for(int i=0;i<2;++i){writes[i].sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;writes[i].dstSet=set;writes[i].dstBinding=i+1;writes[i].descriptorCount=1;}writes[0].descriptorType=VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;writes[0].pTexelBufferView=&view;writes[1].descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;writes[1].pBufferInfo=&outInfo;vkUpdateDescriptorSets(device,2,writes,0,nullptr);
    VkCommandPoolCreateInfo cpi{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};cpi.queueFamilyIndex=family;VkCommandPool commands{};check(vkCreateCommandPool(device,&cpi,nullptr,&commands),"command pool");VkCommandBufferAllocateInfo ca{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};ca.commandPool=commands;ca.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;ca.commandBufferCount=1;VkCommandBuffer command{};check(vkAllocateCommandBuffers(device,&ca,&command),"command buffer");
    VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};check(vkBeginCommandBuffer(command,&begin),"begin");vkCmdBindPipeline(command,VK_PIPELINE_BIND_POINT_COMPUTE,pipeline);vkCmdBindDescriptorSets(command,VK_PIPELINE_BIND_POINT_COMPUTE,layout,0,1,&set,0,nullptr);uint32_t count=old?words.size()/2:words.size()/4;vkCmdPushConstants(command,layout,VK_SHADER_STAGE_COMPUTE_BIT,0,4,&count);vkCmdDispatch(command,(count+63)/64,1,1);
    VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};barrier.srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT;barrier.dstAccessMask=VK_ACCESS_HOST_READ_BIT;vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,1,&barrier,0,nullptr,0,nullptr);check(vkEndCommandBuffer(command),"end");VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};submit.commandBufferCount=1;submit.pCommandBuffers=&command;check(vkQueueSubmit(queue,1,&submit,VK_NULL_HANDLE),"submit");check(vkQueueWaitIdle(queue),"wait");
    auto actual=static_cast<const std::array<float,4>*>(out.mapped);size_t errors=0;float maxDifference=0;
    for(size_t i=0;i<N;++i)for(int c=0;c<4;++c){float difference=std::abs(expected[i][c]-actual[i][c]);maxDifference=std::max(maxDifference,difference);if(!std::isfinite(actual[i][c])||difference>1e-5f)++errors;}
    std::printf("%s: %zu components, %zu mismatches, max_error=%.9g; z_sample expected=%.9g actual=%.9g\n",old?"OLD BASELINE":"NEW SHADER",N*4,errors,maxDifference,expected[0][2],actual[0][2]);
    vkDeviceWaitIdle(device);vkDestroyPipeline(device,pipeline,nullptr);vkDestroyShaderModule(device,module,nullptr);vkDestroyDescriptorPool(device,pool,nullptr);vkDestroyPipelineLayout(device,layout,nullptr);vkDestroyDescriptorSetLayout(device,setLayout,nullptr);vkDestroyBufferView(device,view,nullptr);vkDestroyCommandPool(device,commands,nullptr);for(auto b:{source,out}){vkUnmapMemory(device,b.memory);vkDestroyBuffer(device,b.handle,nullptr);vkFreeMemory(device,b.memory,nullptr);}vkDestroyDevice(device,nullptr);vkDestroyInstance(instance,nullptr);
    return old?(errors?0:2):(errors?1:0);
} catch(const std::exception& e){std::fprintf(stderr,"FAIL: %s\n",e.what());return 1;} }
