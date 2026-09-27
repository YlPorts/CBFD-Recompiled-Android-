# Conker Recompiled Android 0.1.10-alpha

Integra PC **V0.1.1**, commit `48e10dce99b9de6146b1d449d8fab538bbc7c55b`, sobre el código completo de Android 0.1.8 recuperado y versionado en esta rama.

- Interpolación por objeto para Conker y sus partes; interpolación de la proyección de sus sombras.
- Corrección del parpadeo entre 4:3/panorámico en el hub y fondo de pausa ampliado proporcionalmente.
- Validación de ROMs USA con modificaciones de recursos: textos, audio y gráficos. Se conserva la comprobación del código ejecutable original.
- Añadir, conservar y seleccionar versiones de ROM desde **Atrás → Cambiar ROM**. La partida termina y guarda antes de abrir el selector. El arranque normal sigue entrando directamente al juego.
- Actualización del runtime, incluido el arreglo de arranque con mods de PC. Esto no añade un gestor Android de archivos `.nrm`.
- Recompilación desde los símbolos publicados por PC; cámara orbital localizada automáticamente aunque cambie el archivo generado que la contiene.

La versión 0.1.10 configura **1080 píxeles internos de alto**, calculando la escala según la VI del juego (4.5x para 240 líneas). Conserva Surface nativa y aspecto Expand: en una pantalla 2340x1080, el objetivo panorámico es 2340x1080. No usa resolución dinámica. Se mantienen Vulkan/RT64, ARM64, cámara táctil, objetivo de presentación 60 FPS, audio, rutas privadas y guardados. La APK usa `com.ylports.cbfd`, versionCode 11 y el certificado original.

La pasada de cobertura elimina cálculos de color que no escribe, y evita dividir por triángulo cuando no hay dependencia de profundidad. Conserva el orden para superficies superpuestas que comparan/escriben Z. También evita compilar shaders que no usará. La prueba Vulkan por software compara esta ruta con la mezcla dual de PC. **1080p/60 FPS sostenidos y los errores visuales del juego todavía requieren una prueba en el A15.** Subir desde 2x a 4.5x aumenta aproximadamente 5.06 veces el número de píxeles; las optimizaciones no demuestran que la GPU pueda mantener 60 FPS.

Para medir: juega diez minutos en la zona problemática, mantén START pulsado 1.5 segundos y pega el diagnóstico. Incluye una captura del defecto visual. El informe registra VI, Surface, tamaño de la textura presentada, tiempos CPU/GPU, FPS de render/envío y estado térmico de Android. Los FPS de envío no miden directamente lo que escanea la pantalla.

El importador mantiene el tamaño de 64 MiB y admite `.z64`, `.v64`, `.n64` y ZIP con una sola ROM. La región de código `0x40..0x1A37E0` debe coincidir con USA, como en PC. Las variantes comparten las partidas existentes. Cada variante se guarda en archivos privados y ocupa 64 MiB adicionales.

## Compilar desde un clon limpio

Linux x86-64, JDK 17+, Python 3, CMake, Ninja y C++ del host; Android SDK plataforma 35, build-tools 35.0.0, NDK 28.0.13004108. No se distribuyen ROMs ni claves de firma.

```sh
git submodule update --init --recursive
git -C tools/N64Recomp apply ../../recomp/n64recomp.patch
git -C tools/N64ModernRuntime apply ../../recomp/n64modernruntime.patch
git -C tools/rt64 apply ../../recomp/rt64.patch
cmake -S tools/N64Recomp -B tools/N64Recomp/build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_POLICY_VERSION_MINIMUM=3.5
cmake --build tools/N64Recomp/build --target N64RecompCLI RSPRecomp --parallel 2
cp /ruta/Conker-USA.z64 conker/baserom.us.z64
python3 recomp/recompile.py
python3 android/tools/prepare.py --dependencies
python3 android/tools/patch_dependencies.py
python3 android/tools/build_apk.py --sdk "$ANDROID_HOME" --keystore /privado/conker-android.jks --alias conker-android --jobs 2
```

Esas tres órdenes `git apply` son para submódulos limpios. No las repitas sobre dependencias ya parcheadas. Para regenerar el juego tras cambiar `conker.toml`, usa `python3 recomp/recompile.py`; el script Android verifica su sello de entradas. `build.sh` es el flujo de escritorio de PC, no el preparador Android.

Las contraseñas se leen de `CONKER_KEYSTORE_PASSWORD` y `CONKER_KEY_PASSWORD`. Conserva la clave original para instalar actualizaciones sin desinstalar.

## Verificación

```sh
python3 android/tools/test.py
python3 android/tools/test_storage.py
python3 android/tools/test_mobile.py
python3 android/tools/test_matcher_math.py
CXX=g++ CC=gcc python3 android/tools/test_native_render_017.py --require-camera
python3 android/tools/test_vulkan_surface.py
CXX=g++ python3 android/tools/test_queues018.py
python3 android/tools/test_pc_sync.py --rom conker/baserom.us.z64
python3 android/tools/test_render_010.py
# Requiere compilador/CMake/Ninja, DXC, cabeceras X11 y un ICD Vulkan:
python3 android/tools/test_gpu018.py
```

La prueba de ROM modifica únicamente copias temporales locales; no exporta datos del juego. Las pruebas de cámara usan la rutina original regenerada. CI omite las pruebas que necesitan ROM y lo informa; no declara gameplay validado. Evidencia: [0.1.10 render](reports/0.1.10-render.md), [sincronización PC](reports/0.1.9-pc-sync.md).
