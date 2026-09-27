#!/usr/bin/env python3
"""Execute the actual USA camera-plane builder and object culler. ROM stays local."""
from pathlib import Path
import argparse, os, subprocess, tempfile
from game_sources import function_source
R = Path(__file__).resolve().parents[2]

def extract(text, name):
    start=text.index('RECOMP_FUNC void '+name+'(')
    i=text.index('{',start)+1; depth=1
    while depth:
        depth+=(text[i]=='{')-(text[i]=='}'); i+=1
    return text[start:i]

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--require-game',action='store_true')
p.add_argument('--baseline',action='store_true',help='Negative control: original 4:3 culling')
a=p.parse_args()
try:
    sources=[extract(function_source(R/'RecompiledFuncs',n).read_text(),n) for n in ['func_1501B22C','func_150A6360']]
except ValueError:
    if a.require_game: raise
    print('SKIP original game visibility execution: private regenerated USA sources required'); raise SystemExit(0)
with tempfile.TemporaryDirectory(prefix='conker-visibility012-') as td:
    t=Path(td); inc=R/'tools/N64ModernRuntime/N64Recomp/include'
    (t/'original.c').write_text('#include "recomp.h"\nvoid sinf_recomp(uint8_t*,recomp_context*);\nvoid cosf_recomp(uint8_t*,recomp_context*);\n'+sources[0].replace('func_1501B22C','conker_original_frustum',1)+'\n'+sources[1])
    subprocess.run([os.environ.get('CC','gcc'),'-std=c11','-O2','-fno-strict-aliasing','-I',str(inc),'-c',str(t/'original.c'),'-o',str(t/'original.o')],check=True)
    hook=R/'android/native/mobile_frustum.cpp'
    if a.baseline:
        hook=t/'baseline.cpp';hook.write_text('#include "recomp.h"\nextern "C" void conker_original_frustum(uint8_t*,recomp_context*);\nextern "C" void func_1501B22C(uint8_t*r,recomp_context*c){conker_original_frustum(r,c);}\n')
    subprocess.run([os.environ.get('CXX','g++'),'-std=c++20','-O2','-fno-strict-aliasing','-fsanitize=undefined','-fno-sanitize-recover=all','-I',str(inc),'-I',str(R/'android/native'),str(R/'android/tests/visibility_012_test.cpp'),str(hook),str(t/'original.o'),'-o',str(t/'test')],check=True)
    subprocess.run([str(t/'test')],check=True,timeout=20)
