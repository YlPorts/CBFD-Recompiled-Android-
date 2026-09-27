#!/usr/bin/env python3
"""Exercise the actual Java launcher on an Android emulator; never run a fake game.
The test APK has the real launcher/SDL Java but intentionally no native libraries.
It must NOT be distributed as a playable APK. No ROM or private key is used here.
"""
from __future__ import annotations
import re
import subprocess
import sys
import time
from pathlib import Path
import xml.etree.ElementTree as ET


def adb(*args: str) -> str:
    return subprocess.check_output(['adb', *args], text=True, stderr=subprocess.STDOUT)


def main() -> None:
    apk, out = Path(sys.argv[1]), Path(sys.argv[2])
    out.mkdir(parents=True, exist_ok=True)
    results: list[str] = []

    def capture(name: str) -> ET.Element:
        adb('shell', 'uiautomator', 'dump', '/sdcard/conker-window.xml')
        adb('pull', '/sdcard/conker-window.xml', str(out / f'{name}.xml'))
        return ET.parse(out / f'{name}.xml').getroot()

    def importer(name: str) -> ET.Element:
        root = capture(name)
        matches = [n for n in root.iter('node') if n.get('text', '').lower() == 'importar rom']
        assert matches, f'{name}: import button was not displayed'
        assert matches[0].get('enabled') == 'true', 'ROM import is disabled'
        results.append('PASS ' + name)
        return matches[0]

    try:
        print(adb('install', '-r', str(apk)))
        adb('logcat', '-c')
        print(adb('shell', 'am', 'start', '-W', '-n', 'com.ylports.cbfd/.LauncherActivity'))
        time.sleep(4)
        button = importer('cold-start')
        bounds = list(map(int, re.findall(r'\d+', button.get('bounds', ''))))
        assert len(bounds) == 4, 'Missing import button bounds'
        adb('shell', 'input', 'tap', str((bounds[0]+bounds[2])//2), str((bounds[1]+bounds[3])//2))
        time.sleep(3)
        picker = capture('document-picker')
        assert any('.documentsui' in n.get('package', '') for n in picker.iter('node')), 'Document picker did not open'
        results.append('PASS Android document picker')
        adb('shell', 'input', 'keyevent', 'BACK')
        time.sleep(2)
        importer('cancel-picker')
        adb('shell', 'input', 'keyevent', 'HOME')
        print(adb('shell', 'am', 'start', '-W', '-n', 'com.ylports.cbfd/.LauncherActivity'))
        time.sleep(2)
        importer('resume-launcher')
        adb('shell', 'am', 'force-stop', 'com.ylports.cbfd')
        print(adb('shell', 'am', 'start', '-W', '-n', 'com.ylports.cbfd/.LauncherActivity'))
        time.sleep(2)
        importer('relaunch-after-force-stop')
        crash = adb('logcat', '-b', 'crash', '-d')
        assert 'Process: com.ylports.cbfd' not in crash, 'App crashed during launcher test'
        results.append('PASS no Conker Java crash')
    finally:
        (out/'crash.txt').write_text(adb('logcat', '-b', 'crash', '-d'))
        (out/'logcat.txt').write_text(adb('logcat', '-d'))
        (out/'activity.txt').write_text(adb('shell', 'dumpsys', 'activity', 'activities'))
        with (out/'screen.png').open('wb') as f:
            subprocess.run(['adb','exec-out','screencap','-p'],stdout=f,check=False)
        (out/'checks.txt').write_text('\n'.join(results) + '\nJava launcher only; native game/FPS not tested.\n')
        print('\n'.join(results))

if __name__ == '__main__': main()
