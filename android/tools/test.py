#!/usr/bin/env python3
"""Run the ROM importer tests without Android SDK, an emulator or game content."""
import pathlib
import shutil
import subprocess
import tempfile
import xml.etree.ElementTree as ET

ROOT = pathlib.Path(__file__).resolve().parents[2]

def main():
    for executable in ("javac", "java"):
        if not shutil.which(executable):
            raise SystemExit(f"Required tool missing: {executable} (JDK 17 or later)")
    with tempfile.TemporaryDirectory(prefix="conker-tests-") as output:
        subprocess.run([
            "javac", "--release", "17", "-Xlint:all", "-Werror", "-d", output,
            str(ROOT / "android/app/src/main/java/com/ylports/cbfd/RomImporter.java"),
            str(ROOT / "android/app/src/main/java/com/ylports/cbfd/RomVersions.java"),
            str(ROOT / "android/tests/RomImporterTest.java"),
        ], check=True)
        subprocess.run(["java", "-ea", "-cp", output, "com.ylports.cbfd.RomImporterTest"], check=True)
    ET.parse(ROOT / "android/app/src/main/AndroidManifest.xml")
    ET.parse(ROOT / "android/app/src/main/res/values/styles.xml")
    print("Android XML is well formed. Full Android/native compilation is a separate gate.")

if __name__ == "__main__":
    main()
