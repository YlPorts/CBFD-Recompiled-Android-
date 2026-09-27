#!/usr/bin/env python3
"""Build/run an isolated real-Android UI test. No ROM, SDL/native libraries or release key.
Its APK is test-only and is never uploaded as an artifact or distributed as a game.
"""
from pathlib import Path
import os,subprocess,sys,time,zipfile
ROOT=Path(__file__).resolve().parents[2]
A=ROOT/'android'
D=ROOT/'ui-probe'
def run(*args): subprocess.run(list(map(str,args)),check=True)
if sys.argv[1]=='build':
    for part in ('res','generated','classes','dex','evidence'): (D/part).mkdir(parents=True,exist_ok=True)
    sdk=Path(os.environ['ANDROID_HOME']);bt=sdk/'build-tools/35.0.0';jar=sdk/'platforms/android-35/android.jar'
    manifest=D/'AndroidManifest.xml'
    manifest.write_text('''<manifest xmlns:android="http://schemas.android.com/apk/res/android" package="com.ylports.cbfd.uitest"><application android:label="Conker UI test (no game)" android:theme="@android:style/Theme.Material.NoActionBar"><activity android:name="com.ylports.cbfd.MobileUiProbe" android:exported="true" android:screenOrientation="sensorLandscape" android:configChanges="orientation|screenSize|keyboardHidden"><intent-filter><action android:name="android.intent.action.MAIN"/><category android:name="android.intent.category.LAUNCHER"/></intent-filter></activity></application></manifest>''')
    run(bt/'aapt2','link','-I',jar,'--manifest',manifest,'--min-sdk-version','26','--target-sdk-version','35','-o',D/'unsigned.apk')
    names=('RenderExtent','GameSurface','ControlLayout','TouchState','TouchControls','Immersive')
    sources=[A/'app/src/main/java/com/ylports/cbfd'/f'{s}.java' for s in names]+sorted((A/'tests/ui').glob('*.java'))
    run('javac','--release','17','-encoding','UTF-8','-classpath',jar,'-d',D/'classes',*sources)
    run('jar','cf',D/'classes.jar','-C',D/'classes','.')
    run(bt/'d8','--min-api','26','--lib',jar,'--output',D/'dex',D/'classes.jar')
    with zipfile.ZipFile(D/'unsigned.apk','a') as z:
        for f in (D/'dex').glob('*.dex'): z.write(f,f.name)
    run(bt/'zipalign','-f','4',D/'unsigned.apk',D/'aligned.apk')
    run('keytool','-genkeypair','-keystore',D/'test.jks','-storepass','android','-keypass','android','-alias','test','-dname','CN=Isolated UI Test','-keyalg','RSA','-validity','30')
    run(bt/'apksigner','sign','--ks',D/'test.jks','--ks-pass','pass:android','--out',D/'test.apk',D/'aligned.apk')
else:
    out=D/'evidence';out.mkdir(parents=True,exist_ok=True)
    run('adb','install','--no-incremental','-r',D/'test.apk');run('adb','logcat','-c')
    run('adb','shell','am','start','-W','-n','com.ylports.cbfd.uitest/com.ylports.cbfd.MobileUiProbe')
    logs=''
    try:
        for _ in range(30):
            time.sleep(2)
            logs=subprocess.check_output(['adb','logcat','-d'],text=True)
            if 'PASS Android surface:' in logs or 'FAIL Android UI probe' in logs: break
        assert 'PASS Android surface:' in logs and 'FAIL Android UI probe' not in logs,'UI/surface probe did not pass'
        print('\n'.join(line for line in logs.splitlines() if 'ConkerUiProbe' in line))
    finally:
        (out/'logcat.txt').write_text(logs)
        with (out/'controls.png').open('wb') as f:subprocess.run(['adb','exec-out','screencap','-p'],stdout=f,check=False)
        (out/'scope.txt').write_text('Only real Java UI and Android SurfaceHolder. Test adapters replace SDL callbacks and JNI. No native game, GPU performance or ROM.\n')
