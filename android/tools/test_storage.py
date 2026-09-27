#!/usr/bin/env python3
"""Run the exact native storage policy on the host, without ROM/SDL/GPU inputs."""
from pathlib import Path
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory(prefix="conker-storage-build-") as tmp:
    binary = Path(tmp) / "storage-test"
    subprocess.run([os.environ.get("CXX", "c++"), "-std=c++20", "-Wall", "-Wextra",
                    "-Werror", str(ROOT / "android/tests/rt64_storage_test.cpp"),
                    "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
