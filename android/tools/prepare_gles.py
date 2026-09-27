#!/usr/bin/env python3
"""Pin and patch the optional GLES renderer without resetting existing edits."""
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / 'android/.deps/GLideN64'
COMMIT = '41c7ba273a6c9afb43c0574cf3cf5d139182d070'
PATCH = ROOT / 'android/patches/gliden64-014.patch'

def git(*args, **kwargs):
    return subprocess.run(['git', *args], cwd=SOURCE, check=True, **kwargs)

def prepare():
    if not SOURCE.exists():
        SOURCE.mkdir(parents=True)
        git('init', '-q')
        git('remote', 'add', 'origin', 'https://github.com/gonetz/GLideN64.git')
        git('fetch', '--depth=1', 'origin', COMMIT)
        git('checkout', '--detach', 'FETCH_HEAD')
    head = git('rev-parse', 'HEAD', capture_output=True, text=True).stdout.strip()
    if head != COMMIT:
        raise SystemExit('GLideN64 differs from the pinned commit; refusing to reset it.')
    check = subprocess.run(['git','apply','--reverse','--check',str(PATCH)], cwd=SOURCE, capture_output=True)
    if check.returncode:
        git('apply', '--check', str(PATCH))
        git('apply', str(PATCH))
    print('GLideN64 GLES source ready:', head)

if __name__ == '__main__': prepare()
