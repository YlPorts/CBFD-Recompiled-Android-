#!/usr/bin/env python3
"""Run the production RT64 matrix score/lerp with cached descriptors. No game/ROM/GPU."""
from pathlib import Path
import subprocess,tempfile
ROOT=Path(__file__).resolve().parents[2]
rt=ROOT/'tools/rt64/src'
source=(rt/'hle/rt64_game_frame.cpp').read_text()
start=source.index('    struct TransformMatchResult {')
end=source.index('    void GameFrame::match(',start)
with tempfile.TemporaryDirectory(prefix='cbfd-math-') as tmp:
    tmp=Path(tmp)
    (tmp/'reference_matcher.inc').write_text('namespace RT64 {\n'+source[start:end]+'\n}')
    subprocess.run(['c++','-std=c++17','-O2','-D__ANDROID__','-include','cstdlib','-fsanitize=undefined','-fno-sanitize-recover=all',
        '-I',str(rt),'-I',str(rt/'contrib/hlslpp/include'),'-I',str(ROOT/'android/native'),'-I',str(tmp),
        str(ROOT/'android/tests/matcher_math_test.cpp'),str(rt/'hle/rt64_rigid_body.cpp'),str(rt/'common/rt64_math.cpp'),
        '-o',str(tmp/'math-test')],check=True,cwd=ROOT)
    subprocess.run([str(tmp/'math-test')],check=True)
