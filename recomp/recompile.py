#!/usr/bin/env python3
"""
Recompiles the game from your ROM into RecompiledFuncs/, on Windows, Linux or
macOS, with only Python's standard library, N64Recomp and RSPRecomp:

  python recomp/recompile.py [--bin DIR]

The ROM must be at conker/baserom.us.z64 (build.cmd / build.sh put it there).

1. recomp/unpack_rom.py unpacks the ROM's code (decompressing .game) into
   recomp/build/, with and without the code rewrites.
2. N64Recomp recompiles it with conker.toml, whose functions come from the
   committed recomp/conker.us.syms.toml.
3. recomp/emit_tlb_pages.py writes RecompiledFuncs/tlb_pages.c.
4. RSPRecomp recompiles the audio microcode (recomp/audio_ucode.toml).
5. RecompiledFuncs/inputs.sha256 records the inputs, which the game's build checks.

--bin is the folder with N64Recomp and RSPRecomp; by default the usual CMake build
folders of tools/N64Recomp are searched. Developers regenerate the committed
symbols from the decompilation with recomp/run.sh instead.
"""
import argparse
import hashlib
import os
import shutil
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "recomp"))
import emit_tlb_pages  # noqa: E402
import unpack_rom  # noqa: E402

# Files that shape RecompiledFuncs/; keep in sync with RECOMP_INPUTS in host/CMakeLists.txt.
RECOMP_INPUTS = ["conker.toml", "recomp/conker.us.syms.toml", "recomp/code_rewrites.txt",
                 "recomp/n64recomp.patch", "recomp/unpack_rom.py", "recomp/emit_tlb_pages.py",
                 "recomp/audio_ucode.toml"]
BIN_DIRS = ["tools/N64Recomp/build", "tools/N64Recomp/build-win", "tools/N64Recomp/build/Release",
            "tools/N64Recomp/build-win/Release"]


def find_tool(name, bin_dir):
    exe = name + (".exe" if os.name == "nt" else "")
    for d in [bin_dir] if bin_dir else BIN_DIRS:
        path = os.path.join(ROOT, d, exe)
        if os.path.isfile(path):
            return path
    sys.exit(f"recompile: {exe} not found (looked in {', '.join([bin_dir] if bin_dir else BIN_DIRS)}). "
             "Build N64Recomp first (build.cmd / build.sh do).")


def run(cmd, log=None):
    with open(log, "w") if log else open(os.devnull, "w") as out:
        result = subprocess.run(cmd, cwd=ROOT, stdout=out, stderr=subprocess.PIPE, text=True)
    if result.returncode != 0:
        sys.exit(f"recompile: {os.path.basename(cmd[0])} failed:\n" + "\n".join(result.stderr.splitlines()[-5:]))
    return result.stderr


def main():
    parser = argparse.ArgumentParser(description="Recompiles the game from your ROM into RecompiledFuncs/.")
    parser.add_argument("--bin", help="folder with N64Recomp and RSPRecomp")
    args = parser.parse_args()
    n64recomp = find_tool("N64Recomp", args.bin)
    rsprecomp = find_tool("RSPRecomp", args.bin)

    build = os.path.join(ROOT, "recomp", "build")
    os.makedirs(build, exist_ok=True)
    rom = open(os.path.join(ROOT, "conker", "baserom.us.z64"), "rb").read()
    original = unpack_rom.unpack(rom)
    open(os.path.join(build, "conker.us.original.bin"), "wb").write(original)
    code = bytearray(original)
    unpack_rom.apply_rewrites(code, os.path.join(ROOT, "recomp", "code_rewrites.txt"))
    open(os.path.join(build, "conker.us.code.bin"), "wb").write(code)

    out = os.path.join(ROOT, "RecompiledFuncs")
    shutil.rmtree(out, ignore_errors=True)
    os.makedirs(out)
    run([n64recomp, "conker.toml"], os.path.join(build, "n64recomp.out"))
    emit_tlb_pages.main(os.path.join(ROOT, "recomp", "conker.us.syms.toml"),
                        os.path.join(build, "conker.us.original.bin"),
                        os.path.join(ROOT, "recomp", "code_rewrites.txt"), os.path.join(out, "tlb_pages.c"))
    os.makedirs(os.path.join(out, "rsp"), exist_ok=True)
    run([rsprecomp, "recomp/audio_ucode.toml"])

    # Hashes of the inputs, which host/CMakeLists.txt checks so a build can't use stale
    # output (line endings stripped, so a CRLF checkout on Windows matches).
    with open(os.path.join(out, "inputs.sha256"), "w", newline="\n") as f:
        for name in RECOMP_INPUTS:
            data = open(os.path.join(ROOT, name), "rb").read().replace(b"\r", b"")
            f.write(f"{hashlib.sha256(data).hexdigest()} {name}\n")
    print(f"Recompiled: {len(os.listdir(out))} files in RecompiledFuncs/")


if __name__ == "__main__":
    main()
