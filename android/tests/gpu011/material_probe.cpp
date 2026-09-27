// Full RasterPS/TextureSampler/ColorCombiner/Blender plus the production pipeline
// factory. Synthetic materials with known mip colors and texture alpha.
#include "production_pipeline.hpp"
#include "plume_vulkan.h"
#include "shared/rt64_frame_params.h"
#include "shared/rt64_f3d_defines.h"
#include "shared/rt64_framebuffer_params.h"
#include "shared/rt64_gpu_tile.h"
#include "shared/rt64_rdp_tile.h"
#include "shared/rt64_rdp_params.h"
#include "shared/rt64_render_indices.h"
#include "shared/rt64_render_params.h"
#include "shared/rt64_raster_params.h"
#include <fstream>
#include <iostream>
#include <cstring>
#include <cmath>
#include <array>
#include <stdexcept>
using namespace plume;
namespace plume { std::unique_ptr<RenderInterface> CreateVulkanInterface(); }
static std::vector<char> read(const std::string& p) {
    std::ifstream f(p, std::ios::binary); if (!f) throw std::runtime_error(p);
    return {std::istreambuf_iterator<char>(f), {}};
}
int main(int argc, char** argv) { try {
    if (argc != 2 && argc != 3) throw std::runtime_error("Expected shader directory and optional filter regression");
    auto iface = CreateVulkanInterface(); auto device = iface->createDevice();
    if (!device || !device->getCapabilities().dualSourceBlend) throw std::runtime_error("PC comparison needs dual-source Vulkan support");
    auto queue = device->createCommandQueue(RenderCommandListType::DIRECT);
    auto cmd = queue->createCommandList(); auto fence = device->createCommandFence();
    auto shader = [&](const char* name, const char* entry) {
        auto bytes = read(std::string(argv[1]) + "/" + name + ".spv");
        return device->createShader(bytes.data(), bytes.size(), entry, RenderShaderFormat::SPIRV);
    };
    auto vs = shader("vs", "VSMain"), colorPS = shader("color", "PSMain"), alphaPS = shader("coverage", "PSMain"),
         opaquePS = shader("opaque", "PSMain"), dualPS = shader("dual", "PSMain");
    std::array<RenderDescriptorSetBuilder, 4> sets;
    for (auto& b : sets) b.begin();
    sets[0].addConstantBuffer(1);
    for (unsigned i = 2; i <= 6; ++i) sets[0].addStructuredBuffer(i);
    for (unsigned i = 7; i <= 24; ++i) sets[0].addSampler(i);
    sets[1].addTexture(0, 8); sets[2].addTexture(0, 8);
    sets[3].addConstantBuffer(0); sets[3].addTexture(2);
    for (auto& b : sets) b.end();
    RenderPipelineLayoutBuilder lb; lb.begin(false, true);
    lb.addPushConstant(0, 0, sizeof(interop::RasterParams), RenderShaderStageFlag::PIXEL);
    for (auto& b : sets) lb.addDescriptorSet(b.descriptorSetDesc);
    lb.end(); auto layout = lb.create(device.get());
    std::array<std::unique_ptr<RenderDescriptorSet>, 4> descriptors;
    for (unsigned i = 0; i < 4; ++i) descriptors[i] = sets[i].create(device.get());
    std::vector<std::unique_ptr<RenderBuffer>> buffers;
    auto upload = [&](const void* data, size_t size, RenderBufferFlags flags = RenderBufferFlag::NONE) {
        auto b = device->createBuffer(RenderBufferDesc::UploadBuffer(std::max(size, size_t(256)), flags));
        std::memset(b->map(), 0, std::max(size, size_t(256))); b->unmap();
        std::memcpy(b->map(), data, size); b->unmap();
        auto ptr = b.get(); buffers.push_back(std::move(b)); return ptr;
    };
    auto update = [](RenderBuffer* b, const auto& data) { std::memcpy(b->map(), &data, sizeof(data)); b->unmap(); };
    auto bind = [&](unsigned set, unsigned index, const auto& data, bool constant = false) {
        auto b = upload(&data, sizeof(data), constant ? RenderBufferFlag::CONSTANT : RenderBufferFlag::STORAGE);
        RenderBufferStructuredView view(sizeof(data));
        descriptors[set]->setBuffer(index, b, constant ? 256 : sizeof(data), constant ? nullptr : &view);
        return b;
    };
    constexpr unsigned W = 16, H = 16;
    interop::FrameParams frame{}; bind(0, 0, frame, true);
    interop::RDPParams rdp{}; rdp.primColor = interop::float4(1,1,1,1); auto rdpBuffer = bind(0, 1, rdp);
    interop::RDPTile tile{}; tile.shifts = tile.shiftt = 1; tile.masks = tile.maskt = 4;
    tile.lrs = tile.lrt = 12; tile.cms = tile.cmt = G_TX_CLAMP;
    auto tileBuffer = bind(0, 2, tile);
    interop::GPUTile gpu{}; gpu.ulScale = gpu.tcScale = interop::float2(1,1);
    gpu.texelMask = interop::uint2(~0U,~0U); gpu.textureDimensions = interop::float3(4,4,3);
    gpu.flags.highRes = true; auto gpuBuffer = bind(0, 3, gpu);
    interop::RenderIndices indices{}; indices.rdpTileCount = 1; bind(0, 4, indices);
    interop::RenderParams rp{}; rp.omH = G_TP_PERSP | G_CYC_1CYCLE;
    // (0-0)*0 + TEXEL0, for RGB and alpha, in both combine cycles.
    rp.ccL = (15U<<20)|(31U<<15)|(7U<<12)|(7U<<9)|(15U<<5)|31U;
    rp.ccH = (15U<<28)|(15U<<24)|(7U<<21)|(7U<<18)|(1U<<15)|(7U<<12)|(1U<<9)|(1U<<6)|(7U<<3)|1U;
    rp.flags.smoothShade = true; rp.flags.usesTexture0 = true; rp.flags.dynamicTiles = true;
    rp.flags.upscale2D = true; rp.flags.upscaleLOD = true;
    auto rpBuffer = bind(0, 5, rp);
    interop::FramebufferParams fp{}; fp.resolution = interop::float2(W,H); fp.resolutionScale = interop::float2(4.5f,4.5f);
    bind(3, 0, fp, true);
    std::vector<std::unique_ptr<RenderSampler>> samplers;
    for (unsigned i = 0; i < 18; ++i) {
        const RenderTextureAddressMode modes[] = {RenderTextureAddressMode::WRAP, RenderTextureAddressMode::MIRROR, RenderTextureAddressMode::CLAMP};
        RenderSamplerDesc d; d.addressU = modes[(i%9)/3]; d.addressV = modes[i%3];
        d.anisotropyEnabled = false; if (i >= 9) d.minFilter = d.magFilter = RenderFilter::NEAREST;
        auto s = device->createSampler(d); descriptors[0]->setSampler(i+6, s.get()); samplers.push_back(std::move(s));
    }
    auto texture = device->createTexture(RenderTextureDesc::Texture2D(4,4,3,RenderFormat::R8G8B8A8_UNORM));
    // TMEM is unused by these decoded texture cases, but bind a valid 1D uint image.
    auto td = RenderTextureDesc::Texture2D(1,1,1,RenderFormat::R32_UINT); td.dimension = RenderTextureDimension::TEXTURE_1D;
    auto tmem = device->createTexture(td);
    auto backgroundDepth = device->createTexture(RenderTextureDesc::DepthTarget(W,H,RenderFormat::D32_FLOAT));
    descriptors[3]->setTexture(1, backgroundDepth.get(), RenderTextureLayout::DEPTH_READ);
    for (unsigned i = 0; i < 8; ++i) {
        descriptors[1]->setTexture(i, texture.get(), RenderTextureLayout::SHADER_READ);
        descriptors[2]->setTexture(i, tmem.get(), RenderTextureLayout::SHADER_READ);
    }
    const std::array<std::array<unsigned char,4>,3> mipColors{{{{51,153,230,128}},{{204,51,26,192}},{{13,230,77,64}}}};
    std::vector<unsigned char> pixels; std::array<VkBufferImageCopy,3> copies{};
    for (unsigned m = 0; m < 3; ++m) {
        unsigned side = 4U >> m; auto& c = copies[m]; c.bufferOffset = pixels.size();
        c.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT,m,0,1}; c.imageExtent = {side,side,1};
        for (unsigned i = 0; i < side*side; ++i) pixels.insert(pixels.end(), mipColors[m].begin(), mipColors[m].end());
    }
    auto textureUpload = upload(pixels.data(), pixels.size());
    auto submit = [&]() { cmd->end(); const RenderCommandList* list[] = {cmd.get()}; queue->executeCommandLists(list,1,nullptr,0,nullptr,0,fence.get()); queue->waitForCommandFence(fence.get()); };
    cmd->begin(); cmd->barriers(RenderBarrierStage::COPY, RenderTextureBarrier(texture.get(),RenderTextureLayout::COPY_DEST));
    vkCmdCopyBufferToImage(static_cast<VulkanCommandList*>(cmd.get())->vk, static_cast<VulkanBuffer*>(textureUpload)->vk,
        static_cast<VulkanTexture*>(texture.get())->vk,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,3,copies.data());
    cmd->barriers(RenderBarrierStage::GRAPHICS, RenderTextureBarrier(texture.get(),RenderTextureLayout::SHADER_READ));
    cmd->barriers(RenderBarrierStage::GRAPHICS, RenderTextureBarrier(tmem.get(),RenderTextureLayout::SHADER_READ));
    cmd->barriers(RenderBarrierStage::GRAPHICS, RenderTextureBarrier(backgroundDepth.get(),RenderTextureLayout::DEPTH_READ)); submit();
    auto color = device->createTexture(RenderTextureDesc::ColorTarget(W,H,RenderFormat::R8G8B8A8_UNORM));
    auto depth = device->createTexture(RenderTextureDesc::DepthTarget(W,H,RenderFormat::D32_FLOAT));
    const RenderTexture* colors[] = {color.get()}; auto fb = device->createFramebuffer(RenderFramebufferDesc(colors,1,depth.get()));
    auto readback = device->createBuffer(RenderBufferDesc::ReadbackBuffer(W*H*4));
    const std::array<float,12> positions{-1,-1,.4f,1, 3,-1,.4f,1, -1,3,.4f,1};
    const std::array<float,12> vertexColors{1,1,1,1, 1,1,1,1, 1,1,1,1};
    auto pos = upload(positions.data(),sizeof(positions),RenderBufferFlag::VERTEX);
    auto col = upload(vertexColors.data(),sizeof(vertexColors),RenderBufferFlag::VERTEX);
    std::array<float,6> uvs{}; auto uv = upload(uvs.data(),sizeof(uvs),RenderBufferFlag::VERTEX);
    const RenderVertexBufferView views[] = {{pos,sizeof(positions)},{uv,sizeof(uvs)},{col,sizeof(vertexColors)}};
    unsigned cases = 0, checks = 0, errors = 0;
    auto check = [&](bool ok, const char* why) { ++checks; if (!ok && errors++ < 8) std::cerr << "case " << cases << ": " << why << '\n'; };
    std::array<std::array<std::unique_ptr<RenderPipeline>,3>,2> pipelines;
    for (bool translucent : {false,true}) {
        RT64::PipelineCreation c{}; c.device=device.get(); c.pipelineLayout=layout.get(); c.vertexShader=vs.get();
        c.alphaBlend=translucent; c.zCmp=true; c.zUpd=!translucent; c.NoN=false; c.usesHDR=false;
        c.singleSourcePass=translucent?1:0; c.pixelShader=translucent?colorPS.get():opaquePS.get();
        pipelines[translucent][0]=RT64::RasterShader::createPipeline(c);
        c.singleSourcePass=2; c.alphaBlend=false; c.pixelShader=alphaPS.get();
        pipelines[translucent][1]=RT64::RasterShader::createPipeline(c);
        c.singleSourcePass=0; c.alphaBlend=translucent; c.pixelShader=dualPS.get();
        pipelines[translucent][2]=RT64::RasterShader::createPipeline(c);
        std::cerr << "Compiled full material pipelines: translucent=" << translucent << '\n';
    }
    for (bool nativeSampler : {false,true}) for (bool perspective : {false,true})
    for (bool mipmapped : {false,true}) for (float shift : {.5f,1.f,2.f})
    for (float derivative : {0.f,.25f,.5f,1.f,2.f,4.f,8.f}) for (bool translucent : {false,true}) {
        ++cases; tile.shifts = tile.shiftt = shift;
        tile.nativeSampler = nativeSampler ? NATIVE_SAMPLER_CLAMP_CLAMP : NATIVE_SAMPLER_NONE;
        update(tileBuffer,tile);
        gpu.flags.hasMipmaps = mipmapped; update(gpuBuffer,gpu);
        rp.omL = Z_CMP | (translucent ? FORCE_BL | (0x40U<<16) : Z_UPD);
        // Include N64's own LOD calculation at zero and non-zero derivatives.
        rp.omH = (perspective ? G_TP_PERSP : 0) | G_CYC_1CYCLE | G_TL_LOD; update(rpBuffer,rp);
        uvs = {0,0, derivative*W*2,0, 0,derivative*H*2}; update(uv,uvs);
        const auto& androidPipeline=pipelines[translucent][0];
        const auto& coveragePipeline=pipelines[translucent][1];
        const auto& pcPipeline=pipelines[translucent][2];
        auto draw = [&](bool pc, float initialDepth) {
            cmd->begin(); cmd->barriers(RenderBarrierStage::GRAPHICS,RenderTextureBarrier(color.get(),RenderTextureLayout::COLOR_WRITE));
            cmd->barriers(RenderBarrierStage::GRAPHICS,RenderTextureBarrier(depth.get(),RenderTextureLayout::DEPTH_WRITE));
            cmd->setFramebuffer(fb.get()); cmd->clearColor(0,RenderColor(.2f,.4f,.6f,3.f/255)); cmd->clearDepth(true,initialDepth);
            cmd->setGraphicsPipelineLayout(layout.get());
            for (unsigned i=0;i<4;++i) cmd->setGraphicsDescriptorSet(descriptors[i].get(),i);
            interop::RasterParams constants{}; cmd->setGraphicsPushConstants(0,&constants);
            cmd->setViewports(RenderViewport(0,0,W,H)); cmd->setScissors(RenderRect(0,0,W,H));
            cmd->setVertexBuffers(0,views,3,RT64::RasterInputSlots);
            cmd->setPipeline(pc?pcPipeline.get():androidPipeline.get()); cmd->drawInstanced(3,1,0,0);
            if (!pc && translucent) { cmd->setPipeline(coveragePipeline.get()); cmd->drawInstanced(3,1,0,0); }
            cmd->barriers(RenderBarrierStage::COPY,RenderTextureBarrier(color.get(),RenderTextureLayout::COPY_SOURCE));
            VkBufferImageCopy r{}; r.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1}; r.imageExtent={W,H,1};
            vkCmdCopyImageToBuffer(static_cast<VulkanCommandList*>(cmd.get())->vk, static_cast<VulkanTexture*>(color.get())->vk,
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,static_cast<VulkanBuffer*>(readback.get())->vk,1,&r); submit();
            auto data=static_cast<const unsigned char*>(readback->map()); std::vector<unsigned char> result(data,data+W*H*4); readback->unmap(); return result;
        };
        auto androidPixels=draw(false,1), pcPixels=draw(true,1);
        float mip=mipmapped ? std::clamp(std::log2(std::max(derivative*shift*(perspective?1.f:.5f),1e-8f))-(nativeSampler?0.f:.25f),0.f,2.f) : 0.f;
        unsigned hi=unsigned(std::floor(mip)),lo=std::min(hi+1,2U); float frac=mip-hi;
        std::array<float,4> sample{};
        for (unsigned k=0;k<4;++k) sample[k]=(mipColors[hi][k]*(1-frac)+mipColors[lo][k]*frac)/255.f;
        const float a=translucent?sample[3]:1.f; const float bg[]={.2f,.4f,.6f};
        for (unsigned i=0;i<W*H;++i) {
            for (unsigned k=0;k<4;++k) check(std::abs(int(androidPixels[i*4+k])-int(pcPixels[i*4+k]))<=2,"PC/Android material mismatch");
            for (unsigned k=0;k<3;++k) check(std::abs(int(androidPixels[i*4+k])-int(std::round((sample[k]*a+bg[k]*(1-a))*255)))<=2,"sampled texture RGB/alpha differs from analytic mip reference");
            check(androidPixels[i*4+3]==7,"coverage differs from RDP full-pixel result");
        }
        auto occluded=draw(false,.2f);
        for (unsigned i=0;i<W*H;++i) check(occluded[i*4]==51 && occluded[i*4+1]==102 && occluded[i*4+2]==153 && occluded[i*4+3]==3,"material leaks through foreground depth");
    }
    if (argc == 3) {
        auto referencePS = shader("reference", "PSMain");
        std::array<std::unique_ptr<RenderPipeline>,2> referencePipelines;
        for (bool blend : {false,true}) {
            RT64::PipelineCreation c{}; c.device=device.get(); c.pipelineLayout=layout.get(); c.vertexShader=vs.get();
            c.pixelShader=referencePS.get(); c.alphaBlend=blend; c.zCmp=true; c.zUpd=!blend; c.NoN=false;
            referencePipelines[blend]=RT64::RasterShader::createPipeline(c);
        }
        // Spatially varying color AND alpha, including transparent texels. Constant mip
        // colors alone cannot detect selecting the wrong triangle/corner at a seam.
        std::array<unsigned char,64> pattern{};
        for (unsigned i=0;i<16;++i) {
            pattern[i*4]=(i*71)%256; pattern[i*4+1]=(i*109+31)%256;
            pattern[i*4+2]=(i*47+19)%256; pattern[i*4+3]=(i*85)%256;
        }
        auto patternUpload=upload(pattern.data(),pattern.size());
        cmd->begin(); cmd->barriers(RenderBarrierStage::COPY,RenderTextureBarrier(texture.get(),RenderTextureLayout::COPY_DEST));
        vkCmdCopyBufferToImage(static_cast<VulkanCommandList*>(cmd.get())->vk,static_cast<VulkanBuffer*>(patternUpload)->vk,
            static_cast<VulkanTexture*>(texture.get())->vk,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,copies.data());
        cmd->barriers(RenderBarrierStage::GRAPHICS,RenderTextureBarrier(texture.get(),RenderTextureLayout::SHADER_READ)); submit();
        gpu.flags.hasMipmaps=false; update(gpuBuffer,gpu);
        tile.shifts=tile.shiftt=1; rdp.blendColor.w=.5f; update(rdpBuffer,rdp);
        unsigned filterCases=0, filterErrors=0;
        const unsigned modes[] = {G_TX_WRAP,G_TX_MIRROR,G_TX_CLAMP};
        for (unsigned u=0;u<3;++u) for (unsigned v=0;v<3;++v) for (bool native : {false,true})
        for (unsigned filter : {G_TF_POINT,G_TF_BILERP,G_TF_AVERAGE}) for (bool linear : {false,true})
        for (unsigned alphaMode=0;alphaMode<3;++alphaMode) for (float offset : {-.5f,-.25f,.125f,.5f}) {
            ++cases; ++filterCases;
            tile.cms=modes[u]; tile.cmt=modes[v]; tile.nativeSampler=native?NATIVE_SAMPLER_WRAP_WRAP+u*3+v:NATIVE_SAMPLER_NONE;
            update(tileBuffer,tile);
            bool blend=alphaMode!=0;
            rp.omH=G_TP_PERSP|G_CYC_1CYCLE|filter;
            rp.omL=Z_CMP|(blend?FORCE_BL|(0x40U<<16):Z_UPD|G_AC_THRESHOLD)|(alphaMode==2?CVG_X_ALPHA:0);
            rp.flags.linearFiltering=linear; update(rpBuffer,rp);
            uvs={-4+offset,-4+offset, 12+offset,-4+offset, -4+offset,12+offset}; update(uv,uvs);
            auto drawFilter=[&](RenderPipeline* pipeline) {
                cmd->begin(); cmd->barriers(RenderBarrierStage::GRAPHICS,RenderTextureBarrier(color.get(),RenderTextureLayout::COLOR_WRITE));
                cmd->barriers(RenderBarrierStage::GRAPHICS,RenderTextureBarrier(depth.get(),RenderTextureLayout::DEPTH_WRITE));
                cmd->setFramebuffer(fb.get()); cmd->clearColor(0,RenderColor(.2f,.4f,.6f,3.f/255)); cmd->clearDepth(true,1);
                cmd->setGraphicsPipelineLayout(layout.get());
                for(unsigned i=0;i<4;++i) cmd->setGraphicsDescriptorSet(descriptors[i].get(),i);
                interop::RasterParams constants{}; cmd->setGraphicsPushConstants(0,&constants);
                cmd->setViewports(RenderViewport(0,0,W,H)); cmd->setScissors(RenderRect(0,0,W,H));
                cmd->setVertexBuffers(0,views,3,RT64::RasterInputSlots);
                cmd->setPipeline(pipeline); cmd->drawInstanced(3,1,0,0);
                cmd->barriers(RenderBarrierStage::COPY,RenderTextureBarrier(color.get(),RenderTextureLayout::COPY_SOURCE));
                VkBufferImageCopy r{};r.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};r.imageExtent={W,H,1};
                vkCmdCopyImageToBuffer(static_cast<VulkanCommandList*>(cmd.get())->vk,static_cast<VulkanTexture*>(color.get())->vk,
                    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,static_cast<VulkanBuffer*>(readback.get())->vk,1,&r);submit();
                auto data=static_cast<const unsigned char*>(readback->map());
                std::vector<unsigned char> result(data,data+W*H*4);readback->unmap();return result;
            };
            auto actual=drawFilter(pipelines[blend][2].get()), expected=drawFilter(referencePipelines[blend].get());
            for(size_t i=0;i<actual.size();++i) {
                bool same=std::abs(int(actual[i])-int(expected[i]))<=1;
                filterErrors+=!same; check(same,"three-tap filtering differs from the pinned PC four-tap RGB/alpha reference");
            }
        }
        std::cout<<"Filter regression: "<<filterCases<<" spatial/alpha/seam cases, "<<filterErrors<<" discrepancies (1/255 tolerance)\n";
    }
    std::cout << "Full RasterPS Vulkan: " << cases << " texture/alpha/LOD/depth cases, " << checks << " checks, " << errors << " discrepancies (2/255 tolerance; filter 1/255)\n";
    return errors?1:0;
} catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 2; } }
