// Real Vulkan raster readback. Pipeline-creation method and output packing are
// extracted from the production sources by test_gpu018.py, not rewritten here.
#include "production_pipeline.hpp"
#include "mobile_render_policy.hpp"
#include "plume_vulkan.h"
#include <fstream>
#include <execinfo.h>
#include <csignal>
#include <unistd.h>
#include <iostream>
#include <cmath>
#include <cstring>
#include <array>
#include <stdexcept>
#include <random>
using namespace plume;
namespace plume { std::unique_ptr<RenderInterface> CreateVulkanInterface(); }
static std::vector<char> read(const std::string& p){std::ifstream f(p,std::ios::binary);if(!f)throw std::runtime_error(p);return {std::istreambuf_iterator<char>(f),{}};}
struct Tri {float z, coverage;std::array<float,4> color;bool discard;};
int main(int argc,char**argv){std::signal(SIGSEGV,[](int){void* f[32];int n=backtrace(f,32);backtrace_symbols_fd(f,n,2);_exit(139);});try{
 if(argc!=2) throw std::runtime_error("Expected shader directory");
 auto iface=CreateVulkanInterface(); if(!iface)throw std::runtime_error("No Vulkan interface");
 auto device=iface->createDevice();if(!device)throw std::runtime_error("No Vulkan device");
 std::cout<<"actual Vulkan dualSourceBlend="<<device->getCapabilities().dualSourceBlend<<" depthClamp="<<device->getCapabilities().depthClamp<<"\n";
 auto q=device->createCommandQueue(RenderCommandListType::DIRECT);
 auto cmd=q->createCommandList();auto fence=device->createCommandFence();
 auto vb=read(std::string(argv[1])+"/probeVS.spv"),cb=read(std::string(argv[1])+"/probeColor.spv"),ab=read(std::string(argv[1])+"/probeCoverage.spv");
 auto vs=device->createShader(vb.data(),vb.size(),"VSMain",RenderShaderFormat::SPIRV);
 auto rgb=device->createShader(cb.data(),cb.size(),"PSMain",RenderShaderFormat::SPIRV);
 auto coverage=device->createShader(ab.data(),ab.size(),"PSMain",RenderShaderFormat::SPIRV);
 auto alphaBytes=read(std::string(argv[1])+"/probeAlpha.spv");
 auto alphaCoverage=device->createShader(alphaBytes.data(),alphaBytes.size(),"PSMain",RenderShaderFormat::SPIRV);
 auto dualBytes=read(std::string(argv[1])+"/probeDual.spv");
 auto dual=device->getCapabilities().dualSourceBlend ? device->createShader(dualBytes.data(),dualBytes.size(),"PSMain",RenderShaderFormat::SPIRV) : nullptr;
 RenderPipelineLayoutDesc ld;ld.allowInputLayout=true;auto layout=device->createPipelineLayout(ld);
 constexpr unsigned W=32,H=32;auto color=device->createTexture(RenderTextureDesc::ColorTarget(W,H,RenderFormat::R8G8B8A8_UNORM));
 auto depth=device->createTexture(RenderTextureDesc::DepthTarget(W,H,RenderFormat::D32_FLOAT));
 const RenderTexture* colors[]={color.get()};auto fb=device->createFramebuffer(RenderFramebufferDesc(colors,1,depth.get()));
 auto readback=device->createBuffer(RenderBufferDesc::ReadbackBuffer(W*H*4));
 auto depthReadback=device->createBuffer(RenderBufferDesc::ReadbackBuffer(W*H*4));
 auto quant=[](float x){return int(std::round(std::clamp(x,0.f,1.f)*255));};
 unsigned checks=0,cases=0,errors=0,pcChecks=0;
 std::mt19937 rng(181818);
 for(int mode=0;mode<5;++mode)for(int zcmp=0;zcmp<2;++zcmp)for(int zupd=0;zupd<2;++zupd){
  // 0=coverage clamp,1=add/wrap,2=save,3=opaque,4=zero-alpha translucent
  const bool alpha=mode!=3,add=mode==1,save=mode==2;
  std::vector<Tri> triangles;
  for(unsigned j=0;j<37;++j){Tri t{};t.z=float(rng()%9+1)*.1f; // includes equal-depth overlaps
   t.coverage=save?0.f:float(rng()%8+1)/255.f;
   for(int k=0;k<4;++k)t.color[k]=float(rng()%256)/255.f;
   if(mode==4)t.color[3]=0;
   t.discard=j%7==2;triangles.push_back(t);
  }
  std::vector<float> pos,uv,col;
  for(auto&t:triangles)for(int v=0;v<3;++v){
   pos.insert(pos.end(),{v==1?3.f:-1.f,v==2?3.f:-1.f,t.z,1.f});
   uv.insert(uv.end(),{t.coverage,t.discard?-1.f:1.f});col.insert(col.end(),t.color.begin(),t.color.end());
  }
  auto makeBuf=[&](std::vector<float>&v){auto b=device->createBuffer(RenderBufferDesc::VertexBuffer(v.size()*4,RenderHeapType::UPLOAD));memcpy(b->map(),v.data(),v.size()*4);b->unmap();return b;};
  auto positions=makeBuf(pos),texcoords=makeBuf(uv),vertexColors=makeBuf(col);
  const RenderVertexBufferView views[]={{positions.get(),unsigned(pos.size()*4)},{texcoords.get(),unsigned(uv.size()*4)},{vertexColors.get(),unsigned(col.size()*4)}};
  RT64::PipelineCreation c{};c.device=device.get();c.pipelineLayout=layout.get();c.vertexShader=vs.get();c.pixelShader=alpha?rgb.get():coverage.get();c.alphaBlend=alpha;c.culling=false;c.NoN=false;c.zCmp=zcmp;c.zUpd=zupd;c.cvgAdd=add;c.usesHDR=false;c.singleSourcePass=alpha?(save?3:1):0;
  auto colorPipeline=RT64::RasterShader::createPipeline(c);
  c.pixelShader=alphaCoverage.get();c.alphaBlend=false;c.singleSourcePass=2;c.cvgAdd=add;auto coveragePipeline=RT64::RasterShader::createPipeline(c);
  cmd->begin();cmd->barriers(RenderBarrierStage::GRAPHICS,RenderTextureBarrier(color.get(),RenderTextureLayout::COLOR_WRITE));cmd->barriers(RenderBarrierStage::GRAPHICS,RenderTextureBarrier(depth.get(),RenderTextureLayout::DEPTH_WRITE));
  cmd->setFramebuffer(fb.get());cmd->clearColor(0,RenderColor(.2f,.4f,.6f,3.f/255));cmd->clearDepth(true,1.f);
  cmd->setGraphicsPipelineLayout(layout.get());cmd->setViewports(RenderViewport(0,0,W,H));cmd->setScissors(RenderRect(0,0,W,H));cmd->setVertexBuffers(0,views,3,RT64::RasterInputSlots);
  if(!alpha||save){cmd->setPipeline(colorPipeline.get());cmd->drawInstanced(triangles.size()*3,1,0,0);}
  else{
   unsigned batch=conker::mobile::coverage_batch(triangles.size(),zupd,zcmp);
   for(unsigned first=0;first<triangles.size();first+=batch){cmd->setPipeline(colorPipeline.get());cmd->drawInstanced(batch*3,1,first*3,0);cmd->setPipeline(coveragePipeline.get());cmd->drawInstanced(batch*3,1,first*3,0);}
  }
  cmd->barriers(RenderBarrierStage::COPY,RenderTextureBarrier(color.get(),RenderTextureLayout::COPY_SOURCE));cmd->barriers(RenderBarrierStage::COPY,RenderTextureBarrier(depth.get(),RenderTextureLayout::COPY_SOURCE));
  // Plume's public copy helper only implements uploads in this pinned revision.
  // Use Vulkan readback solely in this test, after its production barriers.
  VkBufferImageCopy region{};region.bufferRowLength=W;region.bufferImageHeight=H;region.imageSubresource.layerCount=1;region.imageExtent={W,H,1};
  region.imageSubresource.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;
  vkCmdCopyImageToBuffer(static_cast<VulkanCommandList*>(cmd.get())->vk,static_cast<VulkanTexture*>(color.get())->vk,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,static_cast<VulkanBuffer*>(readback.get())->vk,1,&region);
  region.imageSubresource.aspectMask=VK_IMAGE_ASPECT_DEPTH_BIT;
  vkCmdCopyImageToBuffer(static_cast<VulkanCommandList*>(cmd.get())->vk,static_cast<VulkanTexture*>(depth.get())->vk,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,static_cast<VulkanBuffer*>(depthReadback.get())->vk,1,&region);
  cmd->end();
  const RenderCommandList* commands[]={cmd.get()};q->executeCommandLists(commands,1,nullptr,0,nullptr,0,fence.get());q->waitForCommandFence(fence.get());
  std::array<int,4> expected{51,102,153,3};float expectedZ=1.f;
  for(auto&t:triangles){if(t.discard || (zcmp && t.z>=expectedZ))continue;const float a=alpha?t.color[3]:1.f;
   for(int k=0;k<3;++k)expected[k]=quant(t.color[k]*a+float(expected[k])/255.f*(1-a));
   if(!save)expected[3]=add?std::min(255,expected[3]+quant(t.coverage)):quant(t.coverage);
   if(zupd)expectedZ=t.z;
  }
  auto got=(const unsigned char*)readback->map();auto zd=(const float*)depthReadback->map();
  for(unsigned i=0;i<W*H;++i){for(int k=0;k<4;++k){++checks;if(std::abs(int(got[i*4+k])-expected[k])>2){if(errors++<8)std::cerr<<"case "<<mode<<zcmp<<zupd<<" channel "<<k<<" got="<<int(got[i*4+k])<<" expected="<<expected[k]<<"\n";}}++checks;if(std::abs(zd[i]-expectedZ)>1e-5f){if(errors++<8)std::cerr<<"depth mismatch "<<zd[i]<<" expected="<<expectedZ<<"\n";}}
  std::vector<unsigned char> androidPixels(got,got+W*H*4);
  readback->unmap();depthReadback->unmap();++cases;
  // Same production pipeline factory, now the desktop dual-source path.
  if(dual) {
   c.singleSourcePass=0;c.alphaBlend=alpha;c.cvgAdd=add||save;c.pixelShader=dual.get();
   auto pcPipeline=RT64::RasterShader::createPipeline(c);
   cmd->begin();cmd->barriers(RenderBarrierStage::GRAPHICS,RenderTextureBarrier(color.get(),RenderTextureLayout::COLOR_WRITE));cmd->barriers(RenderBarrierStage::GRAPHICS,RenderTextureBarrier(depth.get(),RenderTextureLayout::DEPTH_WRITE));
   cmd->setFramebuffer(fb.get());cmd->clearColor(0,RenderColor(.2f,.4f,.6f,3.f/255));cmd->clearDepth(true,1.f);
   cmd->setGraphicsPipelineLayout(layout.get());cmd->setViewports(RenderViewport(0,0,W,H));cmd->setScissors(RenderRect(0,0,W,H));cmd->setVertexBuffers(0,views,3,RT64::RasterInputSlots);
   cmd->setPipeline(pcPipeline.get());cmd->drawInstanced(triangles.size()*3,1,0,0);
   cmd->barriers(RenderBarrierStage::COPY,RenderTextureBarrier(color.get(),RenderTextureLayout::COPY_SOURCE));
   region.imageSubresource.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;
   vkCmdCopyImageToBuffer(static_cast<VulkanCommandList*>(cmd.get())->vk,static_cast<VulkanTexture*>(color.get())->vk,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,static_cast<VulkanBuffer*>(readback.get())->vk,1,&region);
   cmd->end();q->executeCommandLists(commands,1,nullptr,0,nullptr,0,fence.get());q->waitForCommandFence(fence.get());
   auto pc=(const unsigned char*)readback->map();
   for(unsigned i=0;i<W*H*4;++i){++pcChecks;if(std::abs(int(pc[i])-androidPixels[i])>2){if(errors++<8)std::cerr<<"PC/Android blend mismatch\n";}}
   readback->unmap();
  }
 }
 std::cout<<"GPU probe: "<<cases<<" overlap/depth/coverage cases, "<<checks<<" components, "<<errors<<" discrepancies (RGB UNORM tolerance 2/255)\n";
 std::cout<<"Desktop dual-source / Android split-pass GPU comparison: "<<pcChecks<<" components"<<(dual?"\n":" (SKIP: device lacks dual source)\n");
 return errors?1:0;
}catch(std::exception&e){std::cerr<<e.what()<<"\n";return 2;}}
