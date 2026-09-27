#!/usr/bin/env python3
"""Fetch pinned public render/window sources. Never downloads or replaces a ROM."""
import argparse
import pathlib
import subprocess
from prepare_gles import prepare as prepare_gles

ROOT = pathlib.Path(__file__).resolve().parents[2]
SDL = ROOT / "android/.deps/SDL"
SDL_TAG = "release-2.32.10"
SDL_URL = "https://github.com/libsdl-org/SDL.git"

def git(*args, cwd=None):
    return subprocess.check_output(["git", *args], cwd=cwd, text=True).strip()

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dependencies", action="store_true", help="Download the pinned SDL release source")
    args = parser.parse_args()
    if args.dependencies:
        SDL.parent.mkdir(parents=True, exist_ok=True)
        if not SDL.exists():
            git("clone", "--depth", "1", "--branch", SDL_TAG, SDL_URL, str(SDL))
        elif not (SDL / ".git").is_dir():
            raise SystemExit(f"Existing {SDL} is not a Git checkout; not overwriting it.")
        if git("describe", "--tags", "--exact-match", "HEAD", cwd=SDL) != SDL_TAG:
            raise SystemExit(f"SDL is not at {SDL_TAG}; refusing to reset an existing checkout.")
        if git("status", "--porcelain", cwd=SDL):
            raise SystemExit("SDL has local changes; refusing to treat it as a verified dependency.")
        print(f"SDL source ready: {git('rev-parse', 'HEAD', cwd=SDL)}")
        print("SDL C and Java sources must always come from this same checkout.")
        prepare_gles()
    else:
        parser.print_help()
    print("SDL preparation does not generate or run the game. Use android/tools/build_apk.py after recompilation; see android/README.md.")

if __name__ == "__main__":
    main()
