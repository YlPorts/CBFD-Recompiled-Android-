#!/usr/bin/env python3
"""Private-log retention, START gesture and actual RT64 draw-state capture; no ROM/GPU."""
from pathlib import Path
import os, subprocess, tempfile
R = Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory(prefix='conker-diagnostics-') as folder:
    out = Path(folder)
    src = R / 'android/app/src/main/java/com/ylports/cbfd'
    subprocess.run(['javac', '--release', '17', '-Xlint:all', '-Werror', '-d', str(out),
        str(src/'DiagnosticFiles.java'), str(src/'StartGesture.java'), str(R/'android/tests/DiagnosticsTest.java')], check=True)
    subprocess.run(['java', '-ea', '-cp', str(out), 'com.ylports.cbfd.DiagnosticsTest'], check=True)
    p = R / 'tools/rt64/src'
    includes = [R/'android/native', R/'android/tests/stubs', p, p/'contrib/hlslpp/include', p/'contrib/plume', p/'contrib']
    # Use RT64's real draw/texture structures without initializing a GPU device.
    subprocess.run([os.environ.get('CXX', 'c++'), '-std=c++17', '-O1', '-g', '-DHLSL_CPU', '-D__ANDROID__', '-fno-strict-aliasing',
        '-ffunction-sections', '-fdata-sections', '-Wl,--gc-sections', '-pthread',
        *[v for i in includes for v in ['-I', str(i)]], str(R/'android/tests/diagnostics_013_test.cpp'),
        str(p/'common/rt64_common.cpp'),
        '-o', str(out/'native')], check=True)
    subprocess.run([str(out/'native')], check=True)
