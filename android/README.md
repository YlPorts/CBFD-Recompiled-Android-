# Conker Android — inicio del port, no versión jugable

**Estado: código de integración en desarrollo. No hay una APK jugable validada.**
Esta carpeta empieza el port nativo del repositorio PC en el commit
`96d2ae4b18dd241131f233465475b23d7171af68`. No contiene un emulador alternativo,
un juego de demostración, una ROM ni código de juego generado a partir de una ROM.

## Experiencia objetivo

Abrir la aplicación, importar una ROM propia de Conker USA y entrar al juego.
Las siguientes aperturas usan la copia privada. Sin menú de gráficos, sin selector
FPS y sin instalación manual de drivers. El código fija el objetivo de presentación
RT64 en 60 FPS y la relación de aspecto en Expand, con pantalla horizontal inmersiva.
Esto es **un objetivo configurado, no una medición de rendimiento**. No modifica
la velocidad de la lógica del juego ni garantiza 60 FPS en ningún dispositivo.

La resolución interna inicial es 2x la original para reducir coste GPU; se presenta
sobre toda la pantalla, sin imponer una resolución interna de 1080p. El escenario
3D usa el ancho disponible y el HUD conserva una zona legible de hasta 16:9.

## Lo que está escrito

- Importación mediante el selector de documentos Android: `.z64`, `.v64` y `.n64`.
  Convierte el orden de bytes, comprueba tamaño y SHA-1 completos, y reemplaza la
  copia privada solo después de validarla. Una importación fallida no borra la anterior.
- Lanzador de una sola acción y arranque directo; guardados en almacenamiento privado.
- Actividad SDL en proceso `:game`, controles táctiles multitáctiles, mando, pantalla
  inmersiva y puente JNI. El cierre espera al hilo SDL antes de terminar ese proceso.
- Host que registra el juego, TLB, FR=1, CIC, EEPROM y RSP de Conker; adaptador RT64
  sin RecompFrontend; salida de audio sin depender del menú de volumen de PC.
- Proyecto Gradle/CMake arm64, validación del sello de generación y pruebas del
  importador. El proyecto no ofrece un target de APK con un motor simulado.

## Pruebas y límites actuales

`python3 android/tools/test.py` compila el importador con JDK 17+ y ejecuta **33
pruebas sintéticas**, sin una ROM. Comprueba tres órdenes de bytes, lecturas fragmentadas
y vacías, límites, interrupciones, integridad y reemplazo seguro. También comprueba
que los XML estén bien formados. Estas pruebas no ejecutan Android, SDL, Vulkan o Conker.

El workflow `Android source checks` repite esas pruebas y comprueba por separado
la compilación Java contra el SDK Android 35 y SDL 2.32.10. No publica ninguna APK.
Un resultado verde de ese workflow **no valida la compilación C++ ni el juego**.

## Pendiente antes de ofrecer una APK

1. Completar las adaptaciones Android de las dependencias nativas: RT64/Plume,
   compilador DXC y `file_to_c` del equipo de compilación, carga Vulkan y diálogos.
   `android/CMakeLists.txt` conecta esas dependencias pero aún usa sus scripts PC;
   todavía no constituye una compilación Android completa.
2. Generar `RecompiledFuncs/` usando una ROM propia USA y el proceso `recomp/run.sh`
   de la base. Aplicar los parches específicos de Conker del proceso original.
   La ROM se necesita para compilar el port, además de importarse por el jugador.
3. Compilar y enlazar todo para arm64; comprobar JNI, shaders y bibliotecas empaquetadas.
4. Probar arranque, gráficos, audio, mandos, guardado, salir y volver a entrar,
   pausa/reanudación y pérdida de superficie en un teléfono real. La liberación de
   botones al perder foco no es una implementación completa de pausa del runtime.
5. Medir frame pacing, FPS reales, memoria y temperatura en el dispositivo; fijar
   una firma de publicación privada y persistente antes de distribuir actualizaciones.

No se ha medido rendimiento ni ejecutado Conker en un teléfono en esta fase.
No reemplazar RT64 por una rama Android más antigua sin conservar los parches
microcode/widescreen propios de Conker. Esa sustitución puede perder funciones
necesarias del proyecto base.

## Preparación para desarrollo

```sh
python3 android/tools/test.py
python3 android/tools/prepare.py --dependencies
```

El segundo comando obtiene únicamente SDL de su repositorio oficial. No termina la
integración pendiente de RT64 ni obtiene una ROM. Las dependencias originales siguen
fijadas por los submódulos del repositorio base; no se modifican aquí.

El proyecto declara JDK 17, Gradle 8.11.1, Android Gradle Plugin 8.9.2, SDK 35,
NDK 28.0.13004108 y CMake 3.22.1. No hay wrapper Gradle incluido en esta fase.
Una vez resueltos los bloqueos anteriores, el punto de entrada será:

```sh
gradle -p android :app:assembleRelease
```

Ese comando **no está validado todavía**, y no se configura una firma pública o
una clave de desarrollo como firma de distribución. No publicar una APK sin firmar
como instalable. No subir ROMs, código derivado de ROM, partidas o claves al repositorio.

ROM de referencia admitida por el host original: USA sin modificar, 67 108 864 bytes,
SHA-1 `4cbadd3c4e0729dec46af64ad018050eada4f47a` después de normalizar a big-endian.
La validación XXH3 propia de librecomp se conserva en el arranque nativo.

## Referencias técnicas

- Host original: `host/src/main.cpp`, `audio_output.cpp` y `widescreen.cpp`.
- APIs del runtime fijado: `tools/N64ModernRuntime/ultramodern/include/ultramodern/`.
- Secuencia de integración RT64: `tools/RecompFrontend/recompui/src/renderer/rt64_render_context.cpp`.
- Adaptaciones Android estudiadas: `linkzenic/Zelda64Recomp-Android`, cuyo RT64
  `8df288260ad893e33e9ca500691482136b0ba8e7` no sustituye automáticamente al de Conker.

Las licencias de la base y de cada dependencia conservan su ámbito; no se incluyen
activos comerciales del juego en este cambio.
