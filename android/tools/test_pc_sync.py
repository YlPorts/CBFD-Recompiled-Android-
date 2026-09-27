#!/usr/bin/env python3
"""Test the PC V0.1.1 integration. Optional ROM stays local and is never copied to reports."""
import argparse
import ast
import os
from pathlib import Path
import re
import subprocess
import tempfile

from game_sources import camera_source

ROOT = Path(__file__).resolve().parents[2]


def run(*args):
    subprocess.run([str(a) for a in args], cwd=ROOT, check=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--rom', type=Path)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='conker-pc-sync-') as directory:
        output = Path(directory)
        run(os.environ.get('CXX', 'c++'), '-std=c++20', '-O2', '-fsanitize=undefined',
            '-fno-sanitize-recover=all', '-I', ROOT/'tools/N64ModernRuntime/N64Recomp/include',
            ROOT/'android/tests/pc_interpolation_test.cpp', ROOT/'host/src/interpolation.cpp',
            '-o', output/'interpolation')
        run(output/'interpolation')
        # Different recompilers partition the same camera routine into different files.
        (output/'funcs_9.c').write_text('void caller() { func_15123508(rdram, ctx); }')
        actual = output/'funcs_75.c'
        actual.write_text('RECOMP_FUNC void func_15123508(uint8_t* rdram, recomp_context* ctx) {}')
        assert camera_source(output) == actual
        (output/'funcs_76.c').write_text(actual.read_text())
        try:
            camera_source(output)
        except ValueError:
            pass
        else:
            raise AssertionError('Ambiguous camera definition accepted')
        print('PASS camera source discovery across regenerated translation units')
        source = ast.parse((ROOT/'recomp/recompile.py').read_text())
        expected = next(ast.literal_eval(node.value) for node in source.body
                        if isinstance(node, ast.Assign) and any(isinstance(t, ast.Name) and t.id == 'RECOMP_INPUTS' for t in node.targets))
        cmake = (ROOT/'android/CMakeLists.txt').read_text()
        actual_inputs = re.search(r'set\(RECOMP_INPUTS ([^)]+)\)', cmake).group(1).split()
        assert actual_inputs == expected, 'Android generated-code input stamp diverged from PC'
        print('PASS Android/PC generated-input compatibility')
        if args.rom:
            run('javac', '--release', '17', '-d', output,
                ROOT/'android/app/src/main/java/com/ylports/cbfd/RomImporter.java',
                ROOT/'android/app/src/main/java/com/ylports/cbfd/RomVersions.java',
                ROOT/'android/tests/RomVersionsTest.java')
            run('java', '-Xmx512m', '-ea', '-cp', output, 'com.ylports.cbfd.RomVersionsTest', args.rom.resolve())
        else:
            print('SKIP private-ROM integration: pass --rom with your USA ROM')


if __name__ == '__main__':
    main()
