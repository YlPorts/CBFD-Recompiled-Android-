# Conker Recompiled Android — 0.1.1-alpha

Port nativo ARM64 de la recompilación de Conker, con RT64/Vulkan. Primera APK completa compilada y firmada; **el arranque, el audio, los controles, el guardado y el rendimiento en un teléfono siguen sin comprobarse**. Una compilación correcta no demuestra 60 FPS ni compatibilidad con todos los drivers.

## Funcionamiento implementado

Al abrir, selecciona una ROM USA propia o un ZIP que contenga exactamente una ROM. Se normaliza `.z64`, `.v64` o `.n64`, se comprueban los 67,108,864 bytes y el SHA-1 `4cbadd3c4e0729dec46af64ad018050eada4f47a`, y se almacena una copia privada. Las aperturas siguientes arrancan directamente, sin menú de ajustes.

El perfil interno fija presentación objetivo de 60 FPS, Vulkan, modo horizontal inmersivo, relación de aspecto `Expand` y resolución interna 2x de la original. No se cambia el reloj de la lógica del juego. La resolución interna 2x **no significa renderizado interno a 1080p**. El HUD conserva una zona segura 16:9. Se reutilizan los parches de Conker para sprites, transiciones y pantalla ancha. El fondo de pausa sigue teniendo las limitaciones del parche de PC.

Los controles táctiles se conectan al host mediante JNI; también se lee un mando SDL. Tras una interrupción no limpia aparece una pantalla de recuperación con **Copiar diagnóstico** y **Volver a entrar**, no opciones gráficas. Un cierre forzado por Android también puede dejar esa marca; no prueba por sí solo un fallo del motor.

## Verificación realizada

- ROM USA proporcionada: tamaño y SHA-1 correctos; importación del ZIP real comprobada localmente.
- 42 pruebas sintéticas del importador: aprobadas. Cubren los tres órdenes de bytes, lecturas fragmentadas, cancelación, CRC ZIP incorrecto, entradas múltiples y conservación de la copia anterior ante errores.
- Código intermedio: `RecompiledFuncs/` generado correctamente (127 archivos). La comparación de `.init`, `.game` y `.debugger` con la ROM arrojó cero palabras distintas.
- Compilación nativa Release con Android NDK 28.0.13004108, `arm64-v8a`, API mínima 26: completada, incluido RT64, Plume, SDL, runtime, juego y microcódigo de audio.
- Java compilado contra Android API 35 y las clases SDL 2.32.10 del mismo checkout nativo; conversión DEX completada.
- APK con `libmain.so`, `libSDL2.so` y `libc++_shared.so`; entradas SDL/JNI presentes. Firma v2/v3 y alineación ZIP/segmentos ELF de 16 KiB verificadas.
- **No ejecutado en dispositivo Android. No medidos FPS ni temperatura.** Compatibilidad gráfica Mali/Adreno y comportamiento al pausar/reanudar pendientes de una prueba real.

El archivo `build-report.json` registra la huella de la APK y el alcance de las pruebas, no un benchmark.

## Compilar con una ROM propia

Linux x86-64, JDK 17 o posterior, Python 3, CMake, Ninja, compilador C++ de host, Android SDK con plataforma 35, build-tools 35.0.0 y NDK 28.0.13004108.

Primero genera el código del juego con el proceso original. Sus requisitos adicionales incluyen `binutils-mips-linux-gnu` y las dependencias Python del repositorio:

```sh
git submodule update --init --recursive
./build.sh --no-game /ruta/a/Conker-USA.z64
python3 android/tools/prepare.py --dependencies
```

`build.sh` aplica los parches originales de Conker. Después, el constructor Android aplica adaptaciones acotadas sobre esas dependencias. No sustituye RT64 por un fork antiguo ni cambia sus revisiones fijadas.

Crea una clave de firma **privada** una sola vez. Conserva esa misma clave para que las futuras APK puedan actualizar la primera sin desinstalarla. Define `CONKER_KEYSTORE_PASSWORD` y `CONKER_KEY_PASSWORD` en el entorno y usa:

```sh
python3 android/tools/build_apk.py \
  --sdk "$ANDROID_HOME" \
  --keystore /ruta/privada/conker-android.jks \
  --alias conker-android \
  --jobs 2
```

El script compila y enlaza el juego real, usa AAPT2/Javac/D8 para empaquetarlo y firma con `apksigner`. No depende de descargar plugins Gradle. El resultado predeterminado es `android/out/Conker-Recompiled-0.1.1-alpha-arm64.apk`. Falla si faltan el código recompilado o las bibliotecas reales; no produce una APK vacía de sustitución. `--skip-native-build` solo sirve para empaquetar bibliotecas reales ya compiladas a partir del mismo código.

El proyecto Gradle se conserva para desarrollo; requiere preparar antes los mismos parches Android y `android/.host/file_to_c`. El script anterior es el flujo que se verificó para esta APK.

## Adaptaciones de dependencias

`android/tools/patch_dependencies.py` es idempotente y comprueba anclas antes de modificar:

- Herramienta `file_to_c` y DXC ejecutados para la arquitectura del ordenador de compilación, no para la del teléfono.
- Ventana SDL/Vulkan en RT64 y Plume; selección coherente de `SDL_Window*`.
- Diálogos de escritorio desactivados; el importador usa el selector de documentos Android.
- Enlace con SDL, `android` y `log`, sin GTK/X11.
- Constructor de diccionarios zstd excluido de este target: no lo necesita el juego y su `qsort_r` no está disponible en la API mínima.

## Datos privados y GitHub Actions

La ROM, código generado a partir de ella, partidas y claves de firma **no se incluyen en el repositorio ni se envían a Actions**. La APK no incluye el archivo ROM completo y exige importarlo en el teléfono.

`Android source checks` comprueba el importador y Java sin ejecutar el juego. Los workflows manuales de herramientas solo preparan dependencias públicas/recompiladores o el SDK; sus artefactos **no son APKs**. No confundir un check verde de esos workflows con una prueba jugable.

Las partidas están en los archivos privados de la app. Desinstalar borra esos datos; para conservarlos hay que actualizar con la misma firma y el mismo identificador `com.ylports.cbfd`.
