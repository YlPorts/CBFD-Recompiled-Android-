# Conker Recompiled Android — 0.1.4-alpha

Port nativo ARM64 con RT64/Vulkan. Esta revisión corrige la selección de formato y
la continuación insegura después de un fallo al crear la superficie de presentación.
**Compilación y firma comprobadas; la nueva APK aún no se ha ejecutado en el A15.
No hay medición de 60 FPS ni validación completa de gameplay.**

## Corrección actual

RT64/Plume seleccionan RGBA8 o BGRA8 UNORM según los pares de formato/espacio de
color anunciados por la superficie. El formato elegido llega también a los
pipelines finales de VI. Las dimensiones, usos y demás parámetros respetan las
capacidades consultadas. Se comprueba el swapchain antes de anunciar éxito y se
impide usar imágenes inválidas después de un fallo de creación.

Se recompiló y enlazó el motor real ARM64, además de Java/DEX. Pasaron las pruebas
locales del importador (42), rutas (16), política Vulkan (40), métodos de Plume con
SDL/Vulkan simulados (30) y tres comprobaciones de integración de fuentes.
**Estas pruebas no ejecutan Vulkan en un teléfono ni el juego.**

El certificado coincide con 0.1.3: actualizar sin desinstalar ni borrar datos.
Informe y límites exactos: [reports/0.1.4-vulkan.md](reports/0.1.4-vulkan.md).
Se conservan los arreglos de [rutas privadas de 0.1.3](reports/0.1.3-storage.md)
y de [inicio de ventana de 0.1.2](reports/0.1.2-startup.md).

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
python3 android/tools/build_apk.py --sdk "$ANDROID_HOME" \
  --keystore /ruta/privada/conker-android.jks --alias conker-android --jobs 2
```

Define CONKER_KEYSTORE_PASSWORD y CONKER_KEY_PASSWORD en el entorno. Conserva la
misma clave privada para las actualizaciones. El script aplica los parches Android
sobre las dependencias ya parcheadas por build.sh, compila el juego real y firma
con AAPT2/Javac/D8/zipalign/apksigner. Falla si faltan las fuentes generadas o las
bibliotecas reales; no produce una APK vacía de sustitución. El proyecto Gradle
se conserva para desarrollo, pero el script anterior es el flujo verificado.

Para repetir las pruebas locales, después de preparar los parches de dependencias:

```sh
python3 android/tools/test.py
python3 android/tools/test_storage.py
python3 android/tools/test_vulkan_surface.py
```

Las pruebas Vulkan extraen los métodos de Plume realmente parcheados y simulan
únicamente las llamadas de plataforma; no contienen un motor de juego alternativo.

## Privacidad y estado

ROM, fuentes generadas desde ROM, partidas y claves de firma no se publican ni se
envían a GitHub Actions. La APK exige importar la ROM y no incluye el archivo ROM
completo. Actions comprueba fuentes/Java o prepara herramientas públicas: un check
verde no demuestra gameplay. Informes históricos conservados en reports/.

Las partidas están en los archivos privados de la app. Desinstalar borra esos datos;
actualizar con la misma firma e identificador com.ylports.cbfd los conserva.
