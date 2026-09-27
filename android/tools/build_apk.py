#!/usr/bin/env python3
"""Build and sign the real ARM64 game, without Gradle downloads or bundling a ROM.
Requires generated Conker sources, pinned dependencies, Android SDK/NDK and JDK17+.
Signing passwords are read by apksigner from CONKER_KEYSTORE_PASSWORD and
CONKER_KEY_PASSWORD; never store signing keys or passwords in the repository.
"""
from __future__ import annotations
import argparse
import hashlib
import struct
import os
from pathlib import Path
import shutil
import subprocess
import xml.etree.ElementTree as ET
import zipfile

ROOT = Path(__file__).resolve().parents[2]
ANDROID = ROOT / 'android'
VERSION = '0.1.2-android-alpha'
VERSION_CODE = '17'

def run(*args: object) -> None:
    subprocess.run([str(a) for a in args], check=True, cwd=ROOT)

def main() -> None:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--sdk', type=Path, default=os.environ.get('ANDROID_HOME'))
    p.add_argument('--ndk', type=Path, default=os.environ.get('ANDROID_NDK_HOME'))
    p.add_argument('--keystore', required=True, type=Path)
    p.add_argument('--alias', default='conker-android')
    p.add_argument('--jobs', type=int, default=2)
    p.add_argument('--skip-native-build', action='store_true', help='Package already compiled real libraries')
    p.add_argument('--native-apk', type=Path, help='Reuse the real engine from a previously signed ARM64 APK for Java-only fixes')
    p.add_argument('--native-apk-sha256', help='Required SHA-256 of --native-apk; prevents accidentally using the wrong engine')
    p.add_argument('--output', type=Path, default=ROOT / 'android/out/Conker-Recompiled-0.1.2-android-alpha-arm64.apk')
    a = p.parse_args()
    if not a.sdk:
        p.error('Set ANDROID_HOME or --sdk.')
    sdk = a.sdk.resolve()
    ndk = (a.ndk or sdk / 'ndk/28.0.13004108').resolve()
    bt = sdk / 'build-tools/35.0.0'
    jar = sdk / 'platforms/android-35/android.jar'
    required = [a.keystore, jar, bt/'aapt2', bt/'d8']
    if not a.native_apk:
        required += [ndk/'build/cmake/android.toolchain.cmake', ROOT/'RecompiledFuncs/inputs.sha256']
    for f in required:
        if not f.is_file():
            p.error(f'Missing required file: {f}')
    for name in ('CONKER_KEYSTORE_PASSWORD', 'CONKER_KEY_PASSWORD'):
        if not os.environ.get(name): p.error(f'Set {name} in the environment.')
    native = ANDROID / 'build-native'
    llvm = ndk / 'toolchains/llvm/prebuilt/linux-x86_64'
    if not a.skip_native_build and not a.native_apk:
        host = ANDROID / '.host/file_to_c'
        host.parent.mkdir(parents=True, exist_ok=True)
        run(os.environ.get('CXX', 'c++'), '-O2', '-std=c++17', ROOT/'tools/rt64/src/tools/file_to_c/file_to_c.cpp', '-o', host)
        run('python3', ANDROID/'tools/patch_dependencies.py')
        run('cmake', '-S', ANDROID, '-B', native, '-G', 'Ninja',
            f'-DCMAKE_TOOLCHAIN_FILE={ndk}/build/cmake/android.toolchain.cmake',
            '-DANDROID_ABI=arm64-v8a', '-DANDROID_PLATFORM=android-26',
            '-DANDROID_STL=c++_shared', '-DCMAKE_BUILD_TYPE=Release',
            '-DCMAKE_POLICY_VERSION_MINIMUM=3.5')
        run('cmake', '--build', native, '--target', 'main', '--parallel', max(1,a.jobs))
    libs = {
        'libmain.so': native/'libmain.so',
        'libSDL2.so': native/'SDL/libSDL2.so',
        'libc++_shared.so': llvm/'sysroot/usr/lib/aarch64-linux-android/libc++_shared.so',
    }
    if a.native_apk:
        if not a.native_apk_sha256 or hashlib.sha256(a.native_apk.read_bytes()).hexdigest() != a.native_apk_sha256.lower():
            p.error('Provide the matching SHA-256 for the original signed engine APK.')
        run(bt/'apksigner', 'verify', '--verbose', a.native_apk.resolve())
        reuse = ANDROID/'build-native-reuse'
        reuse.mkdir(parents=True, exist_ok=True)
        with zipfile.ZipFile(a.native_apk) as archive:
            for name in libs:
                data = archive.read('lib/arm64-v8a/' + name)
                if data[:5] != b'\x7fELF\x02' or struct.unpack_from('<H', data, 18)[0] != 183:
                    p.error(f'Not an AArch64 native library: {name}')
                libs[name] = reuse/name
                libs[name].write_bytes(data)
        print('Reusing unchanged real native engine; recompiling Java and Android resources.')
    for f in libs.values():
        if not f.is_file(): p.error(f'Real native library missing: {f}; no launcher-only APK is produced.')
    main_bytes = libs['libmain.so'].read_bytes()
    for marker in (b'pc-012-direct', b'Java_com_ylports_cbfd_GameActivity_nativeInput',\n                   b'PC 0.1.2 RT64', b'Java_com_ylports_cbfd_GameActivity_nativeRequestQuit'):
        if marker not in main_bytes:
            p.error('This release requires the direct PC 0.1.2 Android engine; an older Android engine cannot be relabeled.')
    stage = ANDROID/'build-package'
    if stage.exists(): shutil.rmtree(stage)
    for d in ['resources','generated','classes','dex','lib/arm64-v8a']: (stage/d).mkdir(parents=True,exist_ok=True)
    ET.register_namespace('android','http://schemas.android.com/apk/res/android')
    manifest = ET.parse(ANDROID/'app/src/main/AndroidManifest.xml')
    manifest.getroot().set('package','com.ylports.cbfd')
    manifest.write(stage/'AndroidManifest.xml',encoding='utf-8',xml_declaration=True)
    run(bt/'aapt2','compile','--dir',ANDROID/'app/src/main/res','-o',stage/'resources.zip')
    run(bt/'aapt2','link','-I',jar,'--manifest',stage/'AndroidManifest.xml',
        '--min-sdk-version','26','--target-sdk-version','35','--version-code',VERSION_CODE,
        '--version-name',VERSION,'--java',stage/'generated','-o',stage/'unsigned.apk',stage/'resources.zip')
    sources = sorted((ANDROID/'app/src/main/java').rglob('*.java'))
    sources += sorted((ANDROID/'.deps/SDL/android-project/app/src/main/java').rglob('*.java'))
    sources += sorted((stage/'generated').rglob('*.java'))
    run('javac','--release','17','-encoding','UTF-8','-classpath',jar,'-d',stage/'classes',*sources)
    run('jar','--create','--file',stage/'classes.jar','-C',stage/'classes','.')
    run(bt/'d8','--release','--min-api','26','--lib',jar,'--output',stage/'dex',stage/'classes.jar')
    with zipfile.ZipFile(stage/'unsigned.apk','a') as z:
        for f in sorted((ANDROID/'app/src/main/assets').rglob('*')):
            if f.is_file():
                z.write(f, 'assets/' + f.relative_to(ANDROID/'app/src/main/assets').as_posix(), compress_type=zipfile.ZIP_DEFLATED)
        for f in sorted((stage/'dex').glob('*.dex')): z.write(f, f.name,compress_type=zipfile.ZIP_DEFLATED)
        for name,f in libs.items():
            copy=stage/'lib/arm64-v8a'/name
            shutil.copyfile(f,copy)
            if not a.native_apk:
                run(llvm/'bin/llvm-strip','--strip-debug',copy)
            z.write(copy,'lib/arm64-v8a/'+name,compress_type=zipfile.ZIP_STORED)
    run(bt/'zipalign','-P','16','-f','4',stage/'unsigned.apk',stage/'aligned.apk')
    a.output.parent.mkdir(parents=True,exist_ok=True)
    run(bt/'apksigner','sign','--ks',a.keystore.resolve(),'--ks-key-alias',a.alias,
        '--ks-pass','env:CONKER_KEYSTORE_PASSWORD','--key-pass','env:CONKER_KEY_PASSWORD',
        '--out',a.output.resolve(),stage/'aligned.apk')
    run(bt/'apksigner','verify','--verbose','--print-certs',a.output.resolve())
    run(bt/'zipalign','-c','-P','16','4',a.output.resolve())
    run(bt/'aapt','dump','badging',a.output.resolve())
    print(f'APK compiled and signed: {a.output.resolve()}')
    print('Compilation is not a device gameplay/FPS test. The APK contains no ROM.')

if __name__ == '__main__': main()
