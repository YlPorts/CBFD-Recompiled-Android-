#!/usr/bin/env python3
"""Real production policy and camera-hook checks. Camera test needs private generated USA C.
No ROM data is printed or copied to the report. Vulkan shader test is a separate GPU probe.
"""
from pathlib import Path
import subprocess,tempfile,sys
R=Path(__file__).resolve().parents[2]
def run(*args):subprocess.run([str(x) for x in args],cwd=R,check=True)
with tempfile.TemporaryDirectory(prefix='conker-017-') as td:
    t=Path(td)
    run('clang++','-std=c++20','-O2','-Wall','-Wextra','-Werror','-fsanitize=undefined','-fno-sanitize-recover=all','-pthread',
        '-I',R/'tools/rt64/src/hle','-I',R/'android/native',R/'android/tests/native_render_017_test.cpp','-o',t/'render')
    run(t/'render')
    # Resolution quality is fixed at BOTH levels, not a displayed menu value.
    java=(R/'android/app/src/main/java/com/ylports/cbfd/GameActivity.java').read_text()
    native=(R/'android/native/rt64_renderer.cpp').read_text()
    assert 'setFixedSize' not in java and 'RenderExtent' not in java and 'GameSurface' not in java
    assert 'resolutionMultiplier = 2.0' in native and 'AdaptiveResolution' not in native and '.sample(' not in native
    assert 'mask|=BITS[CU]' not in (R/'android/app/src/main/java/com/ylports/cbfd/TouchLayout.java').read_text()
    print('PASS fixed2x/native-Surface/no-C-remapping source guards')
    s=R/'RecompiledFuncs/funcs_9.c'
    if not s.is_file():
        if '--require-camera' in sys.argv:raise SystemExit('Missing private generated Conker sources')
        print('SKIP real camera-hook fixture: supply generated USA sources; not counted as passed');sys.exit(0)
    text=s.read_text();start=text.index('RECOMP_FUNC void func_15123508(');brace=text.index('{',start);depth=1;i=brace+1
    while depth:
        if text[i]=='{':depth+=1
        if text[i]=='}':depth-=1
        i+=1
    function=text[start:i].replace('func_15123508','conker_original_camera_input',1)
    source=t/'original.c';source.write_text('#include "recomp.h"\n'+function+'\n')
    inc=R/'tools/N64ModernRuntime/N64Recomp/include'
    run('clang','-std=c11','-O2','-Wno-unused-variable','-I',inc,'-c',source,'-o',t/'original.o')
    run('clang++','-std=c++20','-O2','-fsanitize=undefined','-fno-sanitize-recover=all','-I',inc,'-I',R/'android/native',
        R/'android/tests/camera_hook_test.cpp',R/'android/native/mobile_camera.cpp',t/'original.o','-o',t/'camera')
    run(t/'camera')
