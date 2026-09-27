#!/usr/bin/env python3
"""Exercise real mobile geometry/input/timing policies without a ROM or Android device."""
from pathlib import Path
import os
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parents[2]
A=ROOT/'android'
with tempfile.TemporaryDirectory(prefix='cbfd-mobile-') as tmp:
    d=Path(tmp)
    subprocess.run(['javac','--release','17','-Xlint:all','-Werror','-d',str(d),
        *map(str,[(A/'app/src/main/java/com/ylports/cbfd'/f'{n}.java') for n in ('ControlLayout','TouchState','RenderExtent')]),
        str(A/'tests/MobileControlsTest.java')],check=True)
    subprocess.run(['java','-ea','-cp',str(d),'com.ylports.cbfd.MobileControlsTest'],check=True)
    subprocess.run([os.environ.get('CXX','c++'),'-std=c++17','-O1','-Wall','-Wextra','-Werror',
        '-fsanitize=undefined','-fno-sanitize-recover=all','-pthread',f'-I{A}/native',
        str(A/'tests/mobile_render_test.cpp'),'-o',str(d/'mobile-render')],check=True)
    subprocess.run([str(d/'mobile-render')],check=True)
