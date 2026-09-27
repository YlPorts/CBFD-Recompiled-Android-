#!/usr/bin/env python3
"""Test Android touch ownership and the PC-style N64 input mapping. No ROM/GPU."""
from pathlib import Path
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[2]

def main():
    with tempfile.TemporaryDirectory(prefix="conker-touch-pc012-") as folder:
        out=Path(folder)
        subprocess.run(["javac","--release","17","-Xlint:all","-Werror","-d",str(out),
            str(ROOT/"android/app/src/main/java/com/ylports/cbfd/TouchLayout.java"),
            str(ROOT/"android/tests/TouchLayoutTest.java")],cwd=ROOT,check=True)
        subprocess.run(["java","-ea","-cp",str(out),"com.ylports.cbfd.TouchLayoutTest"],cwd=ROOT,check=True)
    print("PASS Android touch layer maps to original PC/N64 inputs; no custom camera/render policy tested.")

if __name__=="__main__":
    main()
