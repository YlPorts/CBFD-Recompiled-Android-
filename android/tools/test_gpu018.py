#!/usr/bin/env python3
"""Real software/hardware Vulkan readback, NOT phone/gameplay testing.
Needs system C++/CMake/Ninja/X11, an installed Vulkan ICD and the pinned source tree.
Extracts the pipeline factory/output packing from production sources for the test.
"""
from pathlib import Path
import subprocess,os,shutil
R=Path(__file__).resolve().parents[2];out=R/'android/build-gpu018';out.mkdir(exist_ok=True)
p=R/'tools/rt64/src';pl=p/'contrib/plume'
def block(s, start):
 a=s.index(start);i=s.index('{',a);n=1;j=i+1
 while n:
  n+=(s[j]=='{')-(s[j]=='}');j+=1
 return s[a:j]
s=(p/'render/rt64_raster_shader.cpp').read_text();h=(p/'render/rt64_raster_shader.h').read_text()
factory=block(s,'    std::unique_ptr<RenderPipeline> RasterShader::createPipeline')
tables=s[s.index('    static const RenderFormat RasterPositionFormat'):s.index('    // OptimizerCacheSPIRV')]
fmt=block((p/'render/rt64_render_target.cpp').read_text(),'    RenderFormat RenderTarget::colorBufferFormat')
(out/'production_pipeline.hpp').write_text('#include "plume_render_interface.h"\n#include <algorithm>\n#include <iterator>\nnamespace RT64 { using namespace plume;\n'+tables+'\n'+block(h,'    struct PipelineCreation')+';\nstruct RenderTarget { static RenderFormat colorBufferFormat(bool); };\n'+fmt+'\nstruct RasterShader { static std::unique_ptr<RenderPipeline> createPipeline(const PipelineCreation&); };\n'+factory+'\n}\n')
ps=(p/'shaders/RasterPS.hlsl').read_text();a=ps.rindex('#if defined(SINGLE_SOURCE_COLOR)');b=ps.index('\n}',a)
packing=ps[a:b]
(out/'probe.hlsl').write_text('''void VSMain(in float4 p:POSITION,in float2 uv:TEXCOORD,in float4 c:COLOR,out float4 o:SV_POSITION,out float2 u:TEXCOORD,out float4 k:COLOR){o=p;u=uv;k=c;}
void PSMain(in float4 p:SV_POSITION,in float2 uv:TEXCOORD,in float4 color:COLOR,
 [[vk::location(0)]] [[vk::index(0)]] out float4 pixelColor:SV_TARGET0) {
 if(uv.y<0)discard;
 float4 resultColor=float4(color.rgb,uv.x);
 float4 resultAlpha=float4(1,1,1,color.a);
'''+packing+'\n}\n')
dxc=p/'contrib/dxc/bin/x64/dxc-linux';env=os.environ.copy();env['LD_LIBRARY_PATH']=str(p/'contrib/dxc/lib/x64')
for filename,entry,target,define in [('probeVS','VSMain','vs_6_0','SINGLE_SOURCE_COLOR'),('probeColor','PSMain','ps_6_0','SINGLE_SOURCE_COLOR'),('probeCoverage','PSMain','ps_6_0','SINGLE_SOURCE_COVERAGE')]:
 subprocess.run([str(dxc),'-spirv','-T',target,'-E',entry,'-D',define,'-Fo',str(out/(filename+'.spv')),str(out/'probe.hlsl')],env=env,check=True)
shutil.copy(R/'android/tests/gpu018/blend_probe.cpp',out/'probe.cpp')
# Test-only offscreen adaptation: SwiftShader has no Xlib WSI. No swapchain is tested here.
backend=(pl/'plume_vulkan.cpp').read_text().replace('        VK_KHR_XLIB_SURFACE_EXTENSION_NAME,','        // Offscreen test does not create an Xlib surface.')
(out/'plume_offscreen.cpp').write_text(backend)
(out/'CMakeLists.txt').write_text(f'''cmake_minimum_required(VERSION 3.20)
project(GPUProbe LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
add_library(plume STATIC plume_offscreen.cpp)
target_include_directories(plume PUBLIC "{pl}" "{pl}/contrib/volk" "{pl}/contrib/Vulkan-Headers/include" "{pl}/contrib/VulkanMemoryAllocator/include")
add_executable(probe probe.cpp)
target_include_directories(probe PRIVATE .)
target_compile_options(plume PRIVATE -g)
target_compile_options(probe PRIVATE -g)
target_link_options(probe PRIVATE -rdynamic -no-pie)
target_link_libraries(probe plume X11 dl pthread)
''')
subprocess.run(['cmake','-S',str(out),'-B',str(out/'build'),'-G','Ninja','-DCMAKE_BUILD_TYPE=Release'],check=True)
subprocess.run(['cmake','--build',str(out/'build'),'--parallel','2'],check=True)
subprocess.run([str(out/'build/probe'),str(out)],check=True)
