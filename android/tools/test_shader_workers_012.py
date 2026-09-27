#!/usr/bin/env python3
"""Execute the production worker lifecycle with controlled compiler/startup gates."""
from pathlib import Path
import argparse, os, subprocess, tempfile
R=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--source',type=Path,default=R/'tools/rt64/src/render/rt64_raster_shader_cache.cpp')
a=p.parse_args()
s=a.source.read_text()
def method(signature):
    start=s.index(signature);i=s.index('{',start)+1;depth=1
    while depth:
        depth+=(s[i]=='{')-(s[i]=='}');i+=1
    return s[start:i]
with tempfile.TemporaryDirectory(prefix='conker-workers012-') as td:
    t=Path(td)
    # Only the GPU compiler/types are mocks. Thread, mutex, condition variable,
    # constructor, destructor, queue accounting and waitForAll are production.
    fixture=(R/'android/tests/shader_workers_012_test.cpp').read_text()
    signatures=[
        'RasterShaderCache::CompilationThread::CompilationThread(',
        'RasterShaderCache::CompilationThread::~CompilationThread(',
        'void RasterShaderCache::CompilationThread::loop(',
        'void RasterShaderCache::waitForAll(']
    methods=[]
    for signature in signatures:
        code=method(signature)
        if '::~' in signature:
            # Observation only: force the same schedule immediately after the
            # real stop write, including on the old constructor's false flag.
            assert code.count('threadRunning = false;')==1
            code=code.replace('threadRunning = false;', 'threadRunning = false; shutdownPublished.store(true);')
        methods.append(code)
    methods='\n'.join(methods)
    (t/'test.cpp').write_text(fixture.replace('// PRODUCTION_METHODS',methods))
    subprocess.run([os.environ.get('CXX','g++'),'-std=c++20','-O2','-Wall','-Wextra','-Werror','-pthread',
                    '-fsanitize=undefined','-fno-sanitize-recover=all',str(t/'test.cpp'),'-o',str(t/'test')],check=True)
    subprocess.run([str(t/'test')],check=True,timeout=15)
