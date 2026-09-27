#!/usr/bin/env python3
"""Compile the actual RSP bounds block with state fixtures and UBSan (no ROM).

--rsp can select the previous source for a regression/mutation check.
"""
from pathlib import Path
import argparse, os, subprocess, tempfile

R = Path(__file__).resolve().parents[2]
P = R / 'tools/rt64/src'
parser = argparse.ArgumentParser()
parser.add_argument('--rsp', type=Path, default=P / 'hle/rt64_rsp.cpp')
args = parser.parse_args()

def block(text, start):
    a = text.index(start)
    i = text.index('{', a)
    n, j = 1, i + 1
    while n:
        n += (text[j] == '{') - (text[j] == '}')
        j += 1
    return text[a:j]

with tempfile.TemporaryDirectory(prefix='conker-011-') as folder:
    out = Path(folder)
    common = (P / 'common/rt64_common.cpp').read_text()
    rect = block((P / 'common/rt64_common.h').read_text(), '    struct FixedRect')
    viewport = block((P / 'shared/rt64_rsp_viewport.h').read_text(), '    struct RSPViewport')
    (out / 'bounds_types.hpp').write_text(
        '#include <cassert>\n#include <cmath>\n#include "shared/rt64_hlsl.h"\n'
        'namespace RT64 {\n' + rect + ';\n' +
        common[common.index('    FixedRect::FixedRect()'):common.index('    // FixedMatrix')] +
        '}\nnamespace interop {\n' + viewport + ';\n}\n')
    rsp = args.rsp.read_text()
    a = rsp.index('        bool conservativeBounds = false;')
    b = rsp.index('        drawCall.triangleCount++;', a)
    (out / 'bounds_body.inc').write_text(rsp[a:b])
    subprocess.run([os.environ.get('CXX', 'g++'), '-std=c++20', '-O2', '-g',
        '-Wall', '-Wextra', '-Werror', '-fno-strict-aliasing', '-fsanitize=undefined', '-fno-sanitize-recover=all',
        '-DHLSL_CPU', '-D__ANDROID__', '-I', str(out), '-I', str(P),
        '-I', str(P / 'contrib/hlslpp/include'), '-I', str(R / 'android/native'),
        str(R / 'android/tests/render_011_test.cpp'), '-o', str(out / 'bounds')], check=True)
    subprocess.run([str(out / 'bounds')], check=True)
