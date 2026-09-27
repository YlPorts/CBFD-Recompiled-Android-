#!/usr/bin/env python3
"""Verify the Android branch is a direct port of PC 0.1.2 shared sources."""
import argparse
import ast
import os
from pathlib import Path
import re
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[2]

UPSTREAM_BLOBS={
    "conker.toml":"0c40138100b9847a6d5855d046499c62deb595d7",
    "host/src/widescreen.cpp":"d48f089f51682e4a2d68ebcbc4c9872265f8a131",
    "recomp/conker.us.syms.toml":"64801e177b6fe5ecde565f273427fb2dae468847",
    "recomp/rt64.patch":"b7f40994a94906304c588260d3ffa01ccf3bd53b",
    "recomp/n64modernruntime.patch":"dd53e7ebd2cc1ccbeb0d14d92603397e1e943d6e",
    "recomp/n64recomp.patch":"e4107a3eaa21ee8332a6da7f1eeb1caef01eac00",
}

def run(*args):
    subprocess.run([str(a) for a in args],cwd=ROOT,check=True)

def blob(path):
    return subprocess.check_output(["git","hash-object",path],cwd=ROOT,text=True).strip()

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rom",type=Path)
    args=parser.parse_args()

    for path,expected in UPSTREAM_BLOBS.items():
        actual=blob(path)
        assert actual==expected,f"{path} diverged from PC 0.1.2: {actual} != {expected}"
    print("PASS exact PC 0.1.2 shared-source blob parity")

    widescreen=(ROOT/"host/src/widescreen.cpp").read_text()
    conker=(ROOT/"conker.toml").read_text()
    for symbol in ("conker_widen_frustum","conker_widen_cull_scale"):
        assert symbol in widescreen and symbol in conker, f"missing PC 0.1.2 widescreen hook {symbol}"
    print("PASS both PC 0.1.2 world-culling fixes present")

    patcher=(ROOT/"android/tools/patch_dependencies.py").read_text()
    forbidden=("mobile-performance","native-render-017","mali-blend","native-1080",
               "visual-bounds","visibility-workers","texture-capture","mali-dual-source","prepare_gles")
    for marker in forbidden:
        assert marker not in patcher,f"old Android render correction still active: {marker}"
    print("PASS Android dependency layer contains platform glue only")

    cmake=(ROOT/"android/CMakeLists.txt").read_text()
    for marker in ("native/gles","gles_renderer","mobile_frustum","mobile_camera"):
        assert marker not in cmake,f"old Android renderer/hook still built: {marker}"

    host=(ROOT/"android/native/android_host.cpp").read_text()
    assert "recomp::Version{0, 1, 2}" in host
    assert "build=pc-012-direct" in host
    print("PASS Android native host identifies PC project_version 0.1.2")

    source=ast.parse((ROOT/"recomp/recompile.py").read_text())
    expected=next(ast.literal_eval(node.value) for node in source.body
                  if isinstance(node,ast.Assign) and any(isinstance(t,ast.Name) and t.id=="RECOMP_INPUTS" for t in node.targets))
    actual_inputs=re.search(r"set\(RECOMP_INPUTS ([^)]+)\)",cmake).group(1).split()
    assert actual_inputs==expected,"Android generated-code input stamp diverged from PC"
    print("PASS Android/PC generated-input compatibility")

    with tempfile.TemporaryDirectory(prefix="conker-pc012-") as directory:
        output=Path(directory)
        run(os.environ.get("CXX","c++"),"-std=c++20","-O2","-fsanitize=undefined",
            "-fno-sanitize-recover=all","-I",ROOT/"tools/N64ModernRuntime/N64Recomp/include",
            ROOT/"android/tests/pc_interpolation_test.cpp",ROOT/"host/src/interpolation.cpp",
            "-o",output/"interpolation")
        run(output/"interpolation")

        if args.rom:
            run("javac","--release","17","-d",output,
                ROOT/"android/app/src/main/java/com/ylports/cbfd/RomImporter.java",
                ROOT/"android/app/src/main/java/com/ylports/cbfd/RomVersions.java",
                ROOT/"android/tests/RomVersionsTest.java")
            run("java","-Xmx512m","-ea","-cp",output,"com.ylports.cbfd.RomVersionsTest",args.rom.resolve())
        else:
            print("SKIP private-ROM integration: pass --rom with your USA ROM")

if __name__=="__main__":
    main()
