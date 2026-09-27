"""Locate the camera routine after N64Recomp repartitions generated functions."""
from pathlib import Path
import re
import sys


def camera_source(directory: Path) -> Path:
    pattern = re.compile(r'RECOMP_FUNC\s+void\s+func_15123508\s*\(')
    matches = [p for p in sorted(directory.glob('funcs_*.c')) if pattern.search(p.read_text())]
    if len(matches) != 1:
        raise ValueError(f'Expected one USA camera definition; found {len(matches)}')
    return matches[0]


if __name__ == '__main__':
    print(camera_source(Path(sys.argv[1])))
