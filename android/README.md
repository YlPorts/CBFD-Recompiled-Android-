# Conker Recompiled Android — 0.1.2-alpha

Port nativo ARM64 de Conker con RT64/Vulkan. Esta versión corrige un cierre del lanzador reproducido en Android 16 antes de mostrar el importador. **El juego, el audio, los controles, el guardado y el rendimiento en un teléfono siguen sin comprobarse**. Probar la interfaz no demuestra que Conker se ejecute ni que sostenga 60 FPS.

## Corrección de inicio

0.1.1 solicitaba Window.getInsetsController() antes de crear el decor de la ventana. Android 16 produjo un NullPointerException en PhoneWindow, llamado desde Immersive.apply y LauncherActivity.onCreate. Ahora se crea la vista antes de consultar el controlador y se difiere el modo inmersivo hasta después de setContentView.

La nueva versión añade diagnóstico desde Application.onCreate, registro de carga de bibliotecas, historial de salidas de Android y recuperación tras errores anteriores a SDL. La interfaz normal conserva una sola acción: **Importar ROM**.

La APK conserva paquete y certificado de firma. libmain.so, libSDL2.so y libc++_shared.so son idénticos byte por byte a 0.1.1: esta corrección recompila Java/DEX y recursos, no el motor C++. Actualiza sin desinstalar ni borrar datos.

## Funcionamiento implementado

Importa una ROM USA propia o un ZIP que contenga exactamente una ROM. Se normaliza .z64, .v64 o .n64, se comprueban los 67,108,864 bytes y el SHA-1 4cbadd3c4e0729dec46af64ad018050eada4f47a y se almacena una copia privada. Las aperturas siguientes arrancan directamente, sin menú de ajustes.

El perfil fija objetivo de presentación 60 FPS, Vulkan, modo horizontal inmersivo, aspecto Expand y resolución interna 2x de la original. No cambia el reloj de la lógica del juego. **2x no significa 1080p interno**. El HUD conserva una zona segura hasta 16:9. Se reutilizan los parches de Conker para sprites, transiciones y pantalla ancha; el fondo de pausa conserva las limitaciones del parche PC.

Los controles táctiles usan JNI y también se lee un mando SDL. Tras una interrupción aparece recuperación con **Copiar diagnóstico** y **Volver a entrar**; sin ROM, el segundo botón permite continuar al importador. Una marca de interrupción puede deberse a un cierre forzado, no necesariamente a un error del motor.

## Verificación y límites

La compilación inicial 0.1.1 verificó la ROM y el ZIP proporcionados, generó 127 archivos RecompiledFuncs y comparó .init, .game y .debugger con cero palabras distintas. Juego, runtime, RSP de audio, RT64, Plume y SDL se compilaron y enlazaron para ARM64 con NDK 28.0.13004108 / API mínima 26.

0.1.2 recompila Java contra Android 35 y SDL 2.32.10, genera DEX y comprueba firma v2/v3, alineación ZIP de 16 KiB, el mismo certificado y conservación exacta de las bibliotecas nativas. Las 42 pruebas sintéticas del importador volvieron a pasar.

El lanzador corregido y la apertura del selector de documentos se comprobaron en un emulador Android 16. El detalle y los límites de cada ejecución están en [reports/0.1.2-startup.md](reports/0.1.2-startup.md). **No se han probado gameplay, Vulkan, audio, mandos ni partidas en un teléfono; no hay medición de FPS o temperatura.** build-report.json conserva el informe de compilación nativa inicial 0.1.1, no un benchmark.

## Compilación

Linux x86-64, JDK 17+, Python 3, CMake, Ninja, compilador C++ de host, SDK Android plataforma 35, build-tools 35.0.0 y NDK 28.0.13004108. Para generar el juego también se necesitan binutils-mips-linux-gnu y las dependencias Python de la base.

```sh
git submodule update --init --recursive
./build.sh --no-game /ruta/a/Conker-USA.z64
python3 android/tools/prepare.py --dependencies
```

El proceso original aplica los parches de Conker. Las adaptaciones Android no sustituyen RT64 por un fork antiguo ni alteran las revisiones fijadas. Conserva una clave de firma privada permanente y define CONKER_KEYSTORE_PASSWORD y CONKER_KEY_PASSWORD en el entorno:

```sh
python3 android/tools/build_apk.py \
  --sdk "$ANDROID_HOME" \
  --keystore /ruta/privada/conker-android.jks \
  --alias conker-android \
  --jobs 2
```

Resultado: android/out/Conker-Recompiled-0.1.2-alpha-arm64.apk. Se usan AAPT2/Javac/D8/zipalign/apksigner sin descargar plugins Gradle. El script falla si faltan el juego generado o las bibliotecas reales; no produce una APK de sustitución vacía. --skip-native-build empaqueta bibliotecas reales ya compiladas.

Para correcciones exclusivamente Java/recursos se puede añadir --native-apk /ruta/anterior.apk --native-apk-sha256 <SHA256-verificado>. El script verifica la firma y los ELF AArch64 de origen, conserva las tres bibliotecas y firma el paquete con la clave privada indicada. Ese modo no requiere regenerar el juego ni instalar el NDK; no sirve para cambios C++.

El proyecto Gradle se conserva para desarrollo. Requiere preparar antes los mismos parches y android/.host/file_to_c. El flujo verificado para las APK entregadas es build_apk.py.

## Dependencias, datos y Actions

patch_dependencies.py comprueba anclas y es idempotente: herramientas file_to_c y DXC del ordenador de compilación, ventana SDL/Vulkan en RT64 y Plume, diálogo Android en lugar del de escritorio, enlace sin GTK/X11 y exclusión del constructor de diccionarios zstd no necesario.

ROM, código generado a partir de ella, partidas y firmas privadas **no se suben al repositorio ni a Actions**. La APK no incluye el archivo ROM completo y exige importarlo. Desinstalar borra los datos privados; actualizar con el mismo paquete com.ylports.cbfd y certificado los conserva.

Android source checks prueba importación y compilación Java. Android launcher startup (no game) ejecuta las clases reales del lanzador y el selector en un emulador Android 16; su APK temporal no contiene bibliotecas del juego y **nunca se distribuye como juego**. Solo se publican registros, XML y capturas de esa prueba. La APK ARM64 entregada sí conserva el motor completo.
