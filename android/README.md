# Conker Recompiled Android 0.1.8-alpha

Port ARM64 nativo de Conker, RT64/Vulkan. Importación de ROM USA propia, cámara orbital, pantalla completa Expand y objetivo de presentación 60 FPS. **2x fijo y Surface nativa: sin reducción automática de resolución.** No se han comprobado nuevos FPS ni el agua en el A15. Detalles y límites en `reports/0.1.8-blend-surface.md`.

## Reconstruir

Usar la base `b423dbf37e598b31d13fb9ad4330f7835778046a` del fork, con sus submódulos recursivos fijados. Generar privadamente el código con `./build.sh --no-game /ruta/Conker-USA.z64`. Después sustituir la carpeta Android por la de este paquete; no mezclarla con la variante 0.1.5 divergente de la rama remota.

Linux x86-64, CMake, Ninja, C++ del host, JDK17+, Android plataforma35/build-tools35.0.0 y NDK28.0.13004108.

```sh
python3 android/tools/prepare.py --dependencies
python3 android/tools/patch_dependencies.py
python3 android/tools/test.py
python3 android/tools/test_storage.py
python3 android/tools/test_mobile.py
python3 android/tools/test_matcher_math.py
python3 android/tools/test_native_render_017.py --require-camera
python3 android/tools/test_vulkan_surface.py
python3 android/tools/test_queues018.py
```

Conservar la clave de firma privada original. Las contraseñas se leen del entorno `CONKER_KEYSTORE_PASSWORD` y `CONKER_KEY_PASSWORD`; no publicarlas.

```sh
python3 android/tools/build_apk.py --sdk "$ANDROID_HOME" --keystore /privado/conker-android.jks --alias conker-android --jobs 2
```

El script compila el motor y el juego reales, empaqueta Java/DEX y firma. No produce una APK de importador vacío. Verifica la marca nativa nueva antes de empaquetar. No incluye la ROM y conserva las rutas de guardado.

## Prueba GPU fuera de pantalla

Requiere compilador C++ del host, CMake/Ninja, cabeceras X11, loader Vulkan y un ICD instalado. El ICD usado aquí fue SwiftShader 1.3.0 con dualSrcBlend=0. Para seleccionar uno, definir `VK_ICD_FILENAMES` a su JSON real del sistema antes de ejecutar:

```sh
python3 android/tools/test_gpu018.py
```

Esta prueba utiliza la fábrica de pipelines y el empaquetado de salidas de producción con entradas sintéticas, no todas las funciones del shader de materiales o una escena del juego. El registro describe tolerancia y alcance. Los otros tests de swapchain simulan llamadas Vulkan/Android y son distintos de esta prueba real por software.

## Parches y datos privados

El nuevo `mali-blend-surface-018.patch` se aplica sobre las capas anteriores. El parcheador retira/reaplica solamente capas totalmente reconocidas; no usa reset/clean ni descarta cambios ajenos. Los nuevos encabezados de soporte están en `android/native/`.

Las fuentes derivadas de ROM y la firma no van en este paquete. La APK instalable se entrega separadamente. No se modificaron ramas remotas; guardar el ZIP y conservar el respaldo privado anterior. Los informes históricos 0.1.5–0.1.7 se mantienen como historial, no como resultados de pruebas de 0.1.8.
