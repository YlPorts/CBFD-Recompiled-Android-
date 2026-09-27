#!/usr/bin/env python3
"""Exercise production touch geometry, homogeneous-bounds safety and metrics. No ROM/GPU."""
from pathlib import Path
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parents[2]

def run(*args):
    subprocess.run([str(a) for a in args],cwd=ROOT,check=True)

def main():
    with tempfile.TemporaryDirectory(prefix='conker-mobile-') as folder:
        out=Path(folder)
        run('javac','--release','17','-Xlint:all','-Werror','-d',out,
            ROOT/'android/app/src/main/java/com/ylports/cbfd/TouchLayout.java',ROOT/'android/tests/TouchLayoutTest.java')
        run('java','-ea','-cp',out,'com.ylports.cbfd.TouchLayoutTest')
        run('c++','-std=c++17','-O2','-Wall','-Wextra','-Werror','-fsanitize=undefined','-fno-sanitize-recover=all','-pthread',
            '-I',ROOT/'android/native',ROOT/'android/tests/mobile_render_test.cpp','-o',out/'render-tests')
        run(out/'render-tests')
        run('c++','-std=c++17','-O2','-Wall','-Wextra','-Werror','-fsanitize=undefined','-fno-sanitize-recover=all','-pthread',
            '-I',ROOT/'android/native',ROOT/'android/tests/mobile_frame_test.cpp','-o',out/'frame-tests')
        run(out/'frame-tests')
    print('Touch/bounds/metrics tests passed. These do not measure FPS or prove the water is fixed in game.')

if __name__=='__main__':main()
