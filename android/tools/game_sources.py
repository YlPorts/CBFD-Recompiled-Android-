"""Locate the camera routine after N64Recomp repartitions generated functions."""
from pathlib import Path
import re
import sys


def function_source(directory: Path, symbol: str) -> Path:
    pattern = re.compile(r'RECOMP_FUNC\s+void\s+' + re.escape(symbol) + r'\s*\(')
    matches = [p for p in sorted(directory.glob('funcs_*.c')) if pattern.search(p.read_text())]
    if len(matches) != 1:
        raise ValueError(f'Expected one USA {symbol} definition; found {len(matches)}')
    return matches[0]


def camera_source(directory: Path) -> Path:
    return function_source(directory, 'func_15123508')


if __name__ == '__main__':
    print(function_source(Path(sys.argv[1]), sys.argv[2] if len(sys.argv) > 2 else 'func_15123508'))
