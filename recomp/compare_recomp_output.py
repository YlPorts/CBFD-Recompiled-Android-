#!/usr/bin/env python3
"""
Compares two N64Recomp outputs function by function, ignoring the order of the
functions and the files they're in: each function is keyed by the address of its
first instruction, and its name is left out of the comparison (only the body).

Usage: compare_recomp_output.py <dir_a> <dir_b>
Exits with 1 if any function differs or exists in only one of them.
"""
import glob
import os
import re
import sys

FUNC_START = re.compile(r'^RECOMP_FUNC \w+ (\w+)\(uint8_t\* rdram, recomp_context\* ctx\) \{$', re.M)
ADDR = re.compile(r'// 0x([0-9A-F]{8}):')


def load(directory):
    funcs = {}
    for path in glob.glob(os.path.join(directory, 'funcs_*.c')):
        text = open(path, encoding='utf-8').read()
        starts = [m for m in FUNC_START.finditer(text)]
        for i, m in enumerate(starts):
            end = starts[i + 1].start() if i + 1 < len(starts) else len(text)
            body = text[m.end():end]
            first = ADDR.search(body)
            key = first.group(1) if first else m.group(1)
            # Calls and references to renamed functions differ only in the name.
            funcs[key] = (m.group(1), body)
    return funcs


def main(a, b):
    fa, fb = load(a), load(b)
    only_a = sorted(set(fa) - set(fb))
    only_b = sorted(set(fb) - set(fa))
    differ = [k for k in sorted(set(fa) & set(fb)) if fa[k][1] != fb[k][1]]
    renamed = [k for k in sorted(set(fa) & set(fb)) if fa[k][0] != fb[k][0]]
    print(f'{len(fa)} / {len(fb)} functions; {len(differ)} bodies differ, {len(renamed)} renamed, '
          f'{len(only_a)} only in {a}, {len(only_b)} only in {b}')
    for k in differ[:10]:
        print(f'  differs: {fa[k][0]} / {fb[k][0]} at 0x{k}')
    for k in renamed[:10]:
        print(f'  renamed: {fa[k][0]} -> {fb[k][0]}')
    for k in only_a[:10]:
        print(f'  only in {a}: {fa[k][0]}')
    for k in only_b[:10]:
        print(f'  only in {b}: {fb[k][0]}')
    return 1 if differ or only_a or only_b else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1], sys.argv[2]))
