# Conker Recompiled Android — 0.1.3-alpha

Port nativo ARM64 con RT64/Vulkan. Esta revisión corrige el cierre por permiso
denegado al crear `/data/.rt64`, observado después de importar la ROM en Android 16.
**Compilación y firma comprobadas; esta APK nueva aún no se ha ejecutado en el A15.
No hay medición de 60 FPS ni validación completa de gameplay.**

## Corrección actual

RT64 ya no detecta una carpeta HOME de escritorio. Recibe `detectDataPath=false`
y una ruta explícita bajo `getFilesDir()/state/rt64`. Se comprueba la carpeta antes
de crear los hilos del motor. No se requieren permisos adicionales y no se cambian
la ROM importada ni los guardados. El diagnóstico distingue construcción de RT64,
setup Vulkan, primer dibujo y primera actualización VI.

Se recompiló y enlazó el motor real ARM64, además de Java/DEX. Pasaron 16 pruebas
de rutas y 42 del importador. Se comprobaron ABI, SDL/JNI, firma y alineación de
16 KiB. El certificado coincide con 0.1.2: actualizar sin desinstalar ni borrar datos.
El informe exacto y sus límites están en [reports/0.1.3-storage.md](reports/0.1.3-storage.md).

La corrección anterior de ventana/pantalla completa de 0.1.2 se conserva. Su prueba
del lanzador Android 16 se documenta en [reports/0.1.2-startup.md](reports/0.1.2-startup.md)
y no constituye una prueba del motor del juego.

## Uso

Importar una ROM USA propia o un ZIP con exactamente una ROM. El importador normaliza
.z64, .v64 y .n64 y valida tamaño e integridad; las aperturas posteriores usan la
copia privada. No hay menú de ajustes. Tras un cierre aparece Copiar diagnóstico /
Volver a entrar. Si ya se importó una ROM, no hay que importarla de nuevo al actualizar.

El perfil conserva objetivo de presentación 60 FPS, Vulkan, horizontal inmersivo,
aspecto Expand, resolución interna 2x y HUD hasta 16:9. No se acelera la lógica del
juego. **Interno 2x no significa 1080p interno, ni configurar 60 garantiza sostenerlos.**
Los controles táctiles usan JNI y se mantiene el soporte de mando SDL.

## Compilar

Linux x86-64, JDK 17+, Python 3, CMake, Ninja, C++ de host, SDK Android plataforma 35,
build-tools 35.0.0 y NDK 28.0.13004108. Para generar el juego también se necesitan
binutils-mips-linux-gnu y las dependencias Python de la base.

```sh
git submodule update --init --recursive
./build.sh --no-game /ruta/a/Conker-USA.z64
python3 android/tools/prepare.py --dependencies
python3 android/tools/test.py
python3 android/tools/test_storage.py
```

Conservar una clave privada permanente y definir CONKER_KEYSTORE_PASSWORD y
CONKER_KEY_PASSWORD en el entorno, sin publicarlas:

```sh
python3 android/tools/build_apk.py \
  --sdk "$ANDROID_HOME" \
  --keystore /ruta/privada/conker-android.jks \
  --alias conker-android \
  --jobs 2
```

Salida: `android/out/Conker-Recompiled-0.1.3-alpha-arm64.apk`. El constructor aplica
las adaptaciones Android sobre las revisiones y parches de Conker fijados; no
sustituye RT64 por una versión antigua. Usa AAPT2/Javac/D8/zipalign/apksigner sin
descargar plugins Gradle. Falla si faltan las fuentes generadas o las bibliotecas
reales; nunca entrega un importador vacío como juego.

`--skip-native-build` solo empaqueta las bibliotecas ya compiladas. `--native-apk`
con SHA-256 verificado está reservado para correcciones exclusivamente Java: **no
sirve para esta corrección C++**, pues reutilizaría el motor defectuoso anterior.

## Privacidad y alcance de las pruebas

La ROM, fuentes generadas desde ella, claves, APK y partidas no se suben al repositorio
ni a Actions. El respaldo privado anterior conserva los insumos de recompilación y
la firma. La APK requiere importar la ROM y no contiene el archivo ROM completo.

Actions comprueba importación, Java y política de rutas en el host; también archiva
únicamente fuentes Android ya seguidas por Git. Un check verde no mide FPS ni prueba
Vulkan/gameplay. `build-report.json` conserva la compilación inicial; los informes
por versión documentan los cambios posteriores. `master` permanece sin modificar.
