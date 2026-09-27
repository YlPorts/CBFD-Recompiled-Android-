#!/usr/bin/env python3
"""Verify actual specialized SPIR-V fetch counts against pinned PC, without a GPU."""
from pathlib import Path
import os, re, subprocess, tempfile
R = Path(__file__).resolve().parents[2]
P = R/'tools/rt64/src'
env = os.environ.copy()
env['LD_LIBRARY_PATH'] = str(P/'contrib/dxc/lib/x64')
dxc = P/'contrib/dxc/bin/x64/dxc-linux'
with tempfile.TemporaryDirectory(prefix='conker-filter-') as folder:
    out = Path(folder)
    reference = out/'pc-sampler.hlsli'
    reference.write_bytes(subprocess.check_output(['git', '-C', str(P.parent), 'show', 'HEAD:src/shaders/TextureSampler.hlsli']))
    for native in (False, True):
        for name, filtered, average, linear, expected in [
            ('point', False, False, False, 1), ('three-point', True, False, False, 3),
            ('average', True, True, False, 4), ('linear', True, False, True, 4)]:
            counts = []
            for sampler in (reference, P/'shaders/TextureSampler.hlsli'):
                source = out/'probe.hlsl'
                source.write_text(f'''#include "{sampler}"
float4 PSMain(float2 uv:TEXCOORD):SV_TARGET {{
    RDPTile tile=(RDPTile)0; tile.masks=tile.maskt=4; tile.lrs=tile.lrt=12;
    tile.cms=tile.cmt=2; tile.nativeSampler={9 if native else 0};
    GPUTile gpu=(GPUTile)0; gpu.tcScale=float2(1,1); gpu.textureDimensions=float3(4,4,1);
    gpu.texelMask=uint2(0xffffffff,0xffffffff);
    return sampleTextureLevel(tile,gpu,{str(filtered).lower()},{str(average).lower()},
        {str(linear).lower()},uv,0,false,0,false);
}}
''')
                subprocess.run([str(dxc), '-spirv', '-fspv-target-env=vulkan1.0', '-fvk-use-dx-layout',
                    '-I', str(P), '-I', str(P/'shaders'), '-T', 'ps_6_3', '-E', 'PSMain',
                    '-Fo', str(out/'probe.spv'), '-Fc', str(out/'probe.asm'), str(source)], env=env, check=True)
                counts.append(len(re.findall(r'\bOpImage(?:Fetch|SampleExplicitLod)\b', (out/'probe.asm').read_text())))
            assert counts == [4 if name=='three-point' else expected, expected], (native, name, counts)
            print(f'{"native" if native else "manual"} {name}: PC={counts[0]} Android={counts[1]} SPIR-V texture fetches')
