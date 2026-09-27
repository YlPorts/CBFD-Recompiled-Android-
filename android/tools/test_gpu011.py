#!/usr/bin/env python3
"""Full production RasterPS readback: textures, alpha, coverage and depth.

Needs test_gpu018's offscreen Plume build (built automatically if absent).
The fixture only shrinks texture descriptor arrays from 8192 to 8; shader
arithmetic is unchanged. --sampler permits the old shader regression check.
This is software/hardware Vulkan verification, not Conker gameplay on a phone.
"""
from pathlib import Path
import argparse, os, shutil, subprocess
R = Path(__file__).resolve().parents[2]
P = R / 'tools/rt64/src'
parser = argparse.ArgumentParser()
parser.add_argument('--sampler', type=Path, default=P / 'shaders/TextureSampler.hlsli')
parser.add_argument('--filter-regression', action='store_true', help='Compare spatial RGB/alpha filtering with the pinned PC four-tap sampler')
args = parser.parse_args()
out = R / 'android/build-gpu011'
out.mkdir(exist_ok=True)
backend = R / 'android/build-gpu018'
if not (backend / 'build/libplume.a').exists():
    subprocess.run(['python3', str(R / 'android/tools/test_gpu018.py')], check=True)
shutil.copytree(P / 'shaders', out / 'shaders', dirs_exist_ok=True)
common = out / 'shaders/FbRendererCommon.hlsli'
common.write_text(common.read_text().replace('[8192]', '[8]'))
shutil.copyfile(args.sampler, out / 'shaders/TextureSampler.hlsli')
(out / 'probeVS.hlsl').write_text('''void VSMain(in float4 p:POSITION,in float2 uv:TEXCOORD,in float4 c:COLOR,
out float4 o:SV_POSITION,out float2 u:TEXCOORD,out float4 s:COLOR0,out float4 f:COLOR1){o=p;u=uv;s=c;f=c;}
''')
env = os.environ.copy()
env['LD_LIBRARY_PATH'] = str(P / 'contrib/dxc/lib/x64')
dxc = P / 'contrib/dxc/bin/x64/dxc-linux'
for name, source, entry, target, defines in [
    ('vs', out/'probeVS.hlsl', 'VSMain', 'vs_6_3', []),
    ('color', out/'shaders/RasterPS.hlsl', 'PSMain', 'ps_6_3', ['SINGLE_SOURCE_COLOR']),
    ('coverage', out/'shaders/RasterPS.hlsl', 'PSMain', 'ps_6_3', ['SINGLE_SOURCE_COVERAGE', 'SINGLE_SOURCE_ALPHA_ONLY']),
    ('opaque', out/'shaders/RasterPS.hlsl', 'PSMain', 'ps_6_3', ['SINGLE_SOURCE_COVERAGE']),
    ('dual', out/'shaders/RasterPS.hlsl', 'PSMain', 'ps_6_3', [])]:
    subprocess.run([str(dxc), '-spirv', '-fspv-target-env=vulkan1.0', '-fvk-use-dx-layout',
        '-I', str(P), '-D', 'DYNAMIC_RENDER_PARAMS',
        *[v for define in defines for v in ['-D', define]], '-T', target, '-E', entry,
        '-Fo', str(out/(name+'.spv')), str(source)], env=env, check=True)
pl = P / 'contrib/plume'
if args.filter_regression:
    reference = out/'pc-reference'
    shutil.copytree(out/'shaders', reference, dirs_exist_ok=True)
    sampler = subprocess.check_output(['git', '-C', str(P.parent), 'show', 'HEAD:src/shaders/TextureSampler.hlsli'])
    (reference/'TextureSampler.hlsli').write_bytes(sampler)
    subprocess.run([str(dxc), '-spirv', '-fspv-target-env=vulkan1.0', '-fvk-use-dx-layout',
        '-I', str(P), '-D', 'DYNAMIC_RENDER_PARAMS', '-T', 'ps_6_3', '-E', 'PSMain',
        '-Fo', str(out/'reference.spv'), str(reference/'RasterPS.hlsl')], env=env, check=True)
(out / 'CMakeLists.txt').write_text(f'''cmake_minimum_required(VERSION 3.20)
project(MaterialProbe LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
add_executable(material "{R}/android/tests/gpu011/material_probe.cpp")
target_compile_definitions(material PRIVATE HLSL_CPU)
target_compile_options(material PRIVATE -fno-strict-aliasing)
target_include_directories(material PRIVATE "{backend}" "{P}" "{P}/contrib/hlslpp/include" "{pl}" "{pl}/contrib/volk" "{pl}/contrib/Vulkan-Headers/include" "{pl}/contrib/VulkanMemoryAllocator/include")
target_link_libraries(material "{backend}/build/libplume.a" X11 dl pthread)
''')
subprocess.run(['cmake', '-S', str(out), '-B', str(out/'build'), '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Release'], check=True)
subprocess.run(['cmake', '--build', str(out/'build'), '--parallel', '2'], check=True)
subprocess.run([str(out/'build/material'), str(out), *(['--filter-regression'] if args.filter_regression else [])], check=True)
