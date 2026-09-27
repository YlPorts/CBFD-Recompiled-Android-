#!/usr/bin/env python3
"""Exercise actual GLES3 pixels with synthetic CBFD display lists; no ROM needed.

Linux dependencies: CMake, Ninja, C++17, EGL development headers and Mesa ES3.
This software-renderer regression does not measure phone FPS or test river scenes.
"""
from pathlib import Path
import argparse
import os
import subprocess
from prepare_gles import prepare

ROOT = Path(__file__).resolve().parents[2]

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--build', type=Path, default=ROOT/'android/.host/gles-tests')
    p.add_argument('--jobs', type=int, default=2)
    args = p.parse_args()
    prepare()
    build = args.build.resolve()
    subprocess.run(['cmake','-S',str(ROOT/'android/tests/gles'),'-B',str(build),'-G','Ninja',
        '-DCMAKE_BUILD_TYPE=Release','-DCONKER_GLES_ROM_PROBE=OFF'],check=True)
    subprocess.run(['cmake','--build',str(build),'--target','gles_regression','--parallel',str(max(1,args.jobs))],check=True)
    for w,h in [(640,480),(2340,1080)]:
        env = dict(os.environ, EGL_PLATFORM='surfaceless', LIBGL_ALWAYS_SOFTWARE='1',
            CONKER_PROBE_WIDTH=str(w),CONKER_PROBE_HEIGHT=str(h))
        subprocess.run([str(build/'gles_regression')],cwd=build,env=env,check=True,timeout=60)

if __name__ == '__main__': main()
