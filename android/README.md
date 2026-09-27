# Conker Recompiled Android 0.1.13-alpha

Integra PC **V0.1.1**, commit `48e10dce99b9de6146b1d449d8fab538bbc7c55b`, sobre el código completo de Android 0.1.8 recuperado y versionado en esta rama.

- Interpolación por objeto para Conker y sus partes; interpolación de la proyección de sus sombras.
- Corrección del parpadeo entre 4:3/panorámico en el hub y fondo de pausa ampliado proporcionalmente.
- Validación de ROMs USA con modificaciones de recursos: textos, audio y gráficos. Se conserva la comprobación del código ejecutable original.
- Añadir, conservar y seleccionar versiones de ROM desde **Atrás → Cambiar ROM**. La partida termina y guarda antes de abrir el selector. El arranque normal sigue entrando directamente al juego.
- Actualización del runtime, incluido el arreglo de arranque con mods de PC. Esto no añade un gestor Android de archivos `.nrm`.
- Recompilación desde los símbolos publicados por PC; cámara orbital localizada automáticamente aunque cambie el archivo generado que la contiene.

La versión 0.1.13 reduce de cuatro a tres las lecturas de textura del filtro normal de tres puntos de N64, conservando su fórmula, alfa, LOD y modos de repetición. Mantiene cuatro lecturas para el filtro promedio y el bilineal real. El SPIR-V compilado confirma la reducción; no equivale a una mejora del 25 % en FPS. También corrige la dependencia de archivos incluidos en la compilación incremental de shaders.

**El usuario confirmó que el agua sigue fallando en 0.1.12. No se considera reparada en esta entrega.** La nueva captura guarda los estados de dibujo de RT64 (modos de mezcla/profundidad, alfa, geometría y referencias de textura) cuando mantienes START. Conserva dos capturas y el último registro con mediciones aunque después haya reinicios breves. Una pulsación corta de START pausa al soltar; una pulsación larga captura sin pausar la escena.

La versión 0.1.12 sincroniza los planos horizontales de visibilidad del juego con el mismo factor panorámico que usa RT64. La versión anterior ampliaba la imagen pero seguía descartando objetos con el campo horizontal original. Conserva el descarte vertical, la profundidad y las distancias originales. La prueba ejecuta el constructor de planos y el descarte originales de Conker: reproduce el fallo lateral de 0.1.11 y verifica 45.800 casos/aserciones con la corrección. Esto no sustituye una prueba del agua y la vegetación en el dispositivo.

También corrige una carrera de arranque/cierre de los hilos de shaders y sustituye la espera activa de recompilación por una condición. Los nuevos diagnósticos muestran el factor de visibilidad, actualizaciones de planos, shaders compilados y usos de shaders especializados/generales. Estos cambios no demuestran por sí solos una mejora de FPS.

La versión 0.1.11 corrige el cálculo de límites del framebuffer para geometría visible en los laterales panorámicos. También impide niveles de mipmap negativos y calcula su distancia con los mismos desplazamientos UV que el muestreo. Conserva el alfa de las texturas; la corrección de mipmaps afecta a reemplazos que tengan una cadena de mipmaps. El LOD original de N64 queda definido para coordenadas constantes. Las pruebas usan el shader completo para comparar materiales transparentes y opacos con la mezcla de PC.

Se mantienen **1080 píxeles internos de alto**, calculando la escala según la VI del juego (4.5x para 240 líneas), Surface nativa y aspecto Expand: en una pantalla 2340x1080, el objetivo panorámico es 2340x1080. No usa resolución dinámica. Conserva Vulkan/RT64, ARM64, cámara táctil, objetivo de presentación 60 FPS, audio, rutas privadas y guardados. La APK usa `com.ylports.cbfd`, versionCode 14 y el certificado original.

La pasada de cobertura elimina cálculos de color que no escribe, y evita dividir por triángulo cuando no hay dependencia de profundidad. Conserva el orden para superficies superpuestas que comparan/escriben Z. También evita compilar shaders que no usará. La prueba Vulkan por software compara esta ruta con la mezcla dual de PC. **El registro de 0.1.11 en el SM-A155M confirma 2340x1080 internos, unos 17–26 FPS renderizados y 28–47 ms de GPU. No mantiene 60 FPS.** La GPU anuncia mezcla dual y no usa las pasadas alternativas de cobertura. Esta entrega mantiene 1080 líneas internas; 60 FPS necesitaría unos 16,7 ms por imagen. El informe de 0.1.12 confirma el fallo visual, pero sus reinicios sustituyeron las mediciones. Todavía no hay mediciones de 0.1.13 en el A15.

Para comparar el agua: mantén START 1,5 segundos cuando el río se vea; después gira hasta que desaparezca y repite. Pega el segundo diagnóstico: conserva ambas capturas. Para FPS, juega al menos 30 segundos en la zona problemática antes de capturar. El informe incluye resolución, tiempos CPU/GPU, FPS de render/envío y estado térmico. Los FPS de envío no miden el escaneo real de pantalla. Los estados de dibujo permiten investigar el descarte; no son una captura de los píxeles finales de la GPU.

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
python3 android/tools/test_render_011.py
CXX=g++ python3 android/tools/test_diagnostics_013.py
CXX=g++ python3 android/tools/test_shader_workers_012.py
CXX=g++ CC=gcc python3 android/tools/test_visibility_012.py --require-game
# Requiere compilador/CMake/Ninja, DXC, cabeceras X11 y un ICD Vulkan:
python3 android/tools/test_gpu018.py
python3 android/tools/test_filter_013.py
python3 android/tools/test_gpu011.py --filter-regression
```

La prueba de ROM modifica únicamente copias temporales locales; no exporta datos del juego. Las pruebas de cámara usan la rutina original regenerada. CI omite las pruebas que necesitan ROM y lo informa; no declara gameplay validado. Evidencia: [0.1.13 filtrado y diagnóstico](reports/0.1.13-filter-capture.md), [0.1.12 visibilidad y shaders](reports/0.1.12-visibility.md), [0.1.11 revisión visual](reports/0.1.11-visual.md), [0.1.10 render](reports/0.1.10-render.md), [sincronización PC](reports/0.1.9-pc-sync.md).
