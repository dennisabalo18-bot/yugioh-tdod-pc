# Yu-Gi-Oh! The Dawn of Destiny — port a PC (estado)

Método: el de `..\GUIA_PORT_XBOX.md` (recompilación estática con el fork `..\xboxrecomp-apex`,
que ya tiene aplicados los 72 parches del port de Red Dead Revolver).

## Resumen al 2026-11-10 (tarde)

**Funciona** (confirmado por el usuario salvo donde dice lo contrario; detalle en "✅ FUNCIONA" y en las
entradas por fecha):
- **Arranque:** `Yu-Gi-Oh.exe` en la carpeta del port arranca solo; lee `ygo-apex\env.bat`, sin programa
  intermedio.
- **Configuración:** `Configuración.exe` (launcher) sincronizado con F2, ajustes en `Ajustes\` (sobreviven a
  una actualización). Pantalla completa guardada.
- **Mouse en todo el juego:**
  - título y nombre;
  - menú principal (rueda);
  - Biblioteca;
  - armado del mazo;
  - duelo: campo, mano, botones, cuadros, ataque, cementerio;
  - piedra-papel-tijera y selección de oponente;
  - Opciones y Guardar;
  - Target List y botones lupa/engranaje de la lista del cementerio.
- **Puntero:** el guante del juego en alta resolución, onda al hacer clic, clic correcto con cualquier
  resolución, aspecto u opción de imagen (sin probar con todas).
- **Modelos de IA:** el principal es `digital-art-4x` (todo el pack). **Segunda opción si deforma algo**
  (caras chicas, detalles): `ultramix-balanced-4x` o `remacri-4x`, solo en esa zona, con
  `tools/zonas_otro_modelo.py` (tabla `ZONAS`: textura, modelo, zona, fundido). Probado en la cara del Mago
  Oscuro; ultrasharp, high-fidelity y upscayl-standard quedaron peor o parecido.
- **Texturas mejoradas:** pack BC3 de 4,85 GB, 7560 texturas, F6 para comparar; chispas del título y del menú
  recuperadas; papel debajo del ATK/DEF igual al de cada carta (confirmado: "quedó genial"); cara del Mago
  Oscuro y costuras de ojos/boca de los 81 retratos arregladas.
- **Voces latinas** (opcionales):
  - 509 voces que siguen los FPS (x1, x1.5 o x2);
  - un toque llena el globo y el doble toque desvanece la voz;
  - con el texto completo, el clic que cierra el globo también desvanece la voz (visto en el registro);
  - F2 / Configuración "Voces": Sigue los FPS / Normal / No;
  - la boca de los retratos se mueve con el volumen de la voz (N.env; confirmado).
- **Idiomas:** español, inglés, francés, alemán e italiano en F2 y en el launcher (portugués quitado).

**Pendiente:**
- mouse propio en Resultados y Créditos (hoy clic = A);
- `.igb`: ya están sacadas y escaladas (`texturas_igb.py`, 2026-11-09). Quedan afuera 27 fondos de 16 bits (formatos 3/8/9/10: ninguna conversión reproduce la huella y nunca aparecieron en el volcado) y 3 cadenas de mipmaps. Del volcado (7977) quedan 164 sin IA a propósito: todas de 32 px o menos (regla: la IA las arruina);
- firmar los .exe o pasarlos por VirusTotal (Malwarebytes);
- botón "Buscar actualizaciones" ya está en el launcher: falta poner el repo de GitHub (`Actualizar.cs`, `Repo`);
- paquete para Release: zips en `dist\` (falta publicarlos y subir el pack de texturas con su `.usadas`);
- cierre visto una vez al empezar un duelo (motor 3D, `sub_001ABAD0` / `sub_00186F70`, 2026-11-10): no se
  repitió todavía.

## El juego

| Dato | Valor |
|---|---|
| Carpeta del juego | `..\Yu-Gi-Oh TDOD` (ya extraído, ~2 GB, 7267 archivos) |
| Title ID | 0x4B4E001E (Konami) |
| Región / versión | "Rest of World" (PAL), disco 0, versión 2 |
| Compilado | 2004-07-30, XDK 1.0.5849 (la misma que RDR) |
| `default.xbe` | 7.254.016 bytes, FNV-1a 64 = `0x700522D705460839` |
| Punto de entrada | `0x0012405D` |
| Ruta de depuración | `c:\DEV\XBOX\YUGIXBOX\yugixbox\XboxRelease\yugixbox.exe` |
| Motor | Alchemy (Intrinsic/Vicarious Visions): archivos `.igb` en `bg\`, `monster\`, etc. |
| Secciones de código | `.text`, D3D, XNET, XMV, DSOUND, D3DX, XGRPH, XPP |
| Videos | sección XMV enlazada: probablemente videos `.xmv` (no Bink) — por confirmar |
| Kernel | 172 imports |

## Carpetas

- `xbe\` — copia del `default.xbe` traducido y `default_analysis.json`.
- `analysis\` — salida del pipeline: `disasm\` (asm, functions, xrefs, strings), `func_id\`, `abi\`,
  `recomp\`, y los registros `2_disasm.log` … `5_recomp.log`.
- `ygo-apex\` — el proyecto del juego (lo que se vuelve `ygo_recomp.exe`).
  - `src\main.c` — tomado del de RDR y adaptado (abajo).
  - `src\recomp\gen\` — el C generado (no se edita a mano; se regenera).
  - `env.bat` / `run.bat` — variables y arranque (registro en `runs\`).
- `third_party\SDL3-3.4.18` — copia de SDL3 (mandos no XInput).

## Pipeline (hecho el 2026-11-08)

1. `tools.xbe_parser` → `xbe\default_analysis.json`.
2. `tools.disasm` con todas las secciones de código (sin `--text-only`): 264 s.
3. `tools.func_id`: 8,5 s.
4. `tools.abi_analysis`: 35.925 funciones, 4.227 thiscall.
5. `tools.recomp --all --split 250 --gen-dir ygo-apex\src\recomp\gen`.

## Cambios en `main.c` respecto del de RDR

- Punto de entrada `0x0012405D`; XBE en `..\xbe\default.xbe`; juego en `..\..\Yu-Gi-Oh TDOD`.
- Chequeo de versión: tamaño y FNV del XBE de arriba (`RDR_XBE_ANY=1` lo saltea).
- **Valla de D3D** (`xbox_Nv2aMirrorFence`): `0x0038ECA8`. Hallada como en RDR: es la global más leída de
  la sección D3D (199 lecturas); la segunda está 0x4A0 más abajo, igual que en RDR (0x3497F8 / 0x349358).
  Sin probar.
- Quitado lo propio de RDR: `mouselook.c`, `widescreen.c`, `movies.c`, `fatal.c`, `mando_directo.c`
  (envolvían funciones de RDR por dirección). Hay que buscar sus equivalentes en este juego.
- Ajustes del menú F2 en `ygo_menu.ini`. Panorámico apagado por defecto.

## Compilación

- Hay que usar el toolset **ClangCL** (`-T ClangCL`), como en RDR: con MSVC puro falla en
  `__ATOMIC_*` del runtime. Comando:
  `cmake -S ygo-apex -B ygo-apex\build -G "Visual Studio 18 2026" -A x64 -T ClangCL -DXBOXRECOMP_SDL3_DIR=...\third_party\SDL3-3.4.18`
  y `cmake --build ygo-apex\build --config Release --parallel 8`.
- **Después de cada regeneración del C:** `py -3 tools\rename_pads.py`.

## Funciones encontradas

| Dirección | Qué es | Cómo se encontró | Uso |
|---|---|---|---|
| `0x0038ECA8` | puntero al dispositivo D3D (valla) | global más leída de D3D | `xbox_Nv2aMirrorFence` |
| `sub_0041C323` | `XInputGetState` del XDK (stdcall, `ret 8`) | mismos bytes que `sub_003C88B4` de RDR, en XPP | `src/mando_directo.c` (`YGO_MANDO_USB=1` vuelve al USB emulado) |
| `sub_0035614F` | `doexit` del CRT (atexit → `KeBugCheck`) | llamada a `KeBugCheck` | — |
| `sub_00356213` | `exit(código)` | llama a doexit; 7 llamadores del juego | `src/fatal.c`: `[FATAL] exit(n) from 0x...`, cartel y cierre |
| `sub_0035621F` | salida interna del CRT (abort, errores del runtime) | llama a doexit | `src/fatal.c` |
| `sub_0001177E` | arranque: crea el dispositivo 3D (640x480) y llama `exit(1)` si falla | llamador de exit | — |
| `0x0011EF9A`, `0x0012486A`, `0x001250D0` | llamadas a `HalReturnToFirmware` (volver al dashboard) | imports | — |

El juego **no tiene un `Quitf` con mensaje** como RDR: sale con `exit(1)` sin texto.

## Datos del disco

- 5 videos `.xmv` (143 MB): los decodifica la propia librería XMV enlazada en el XBE (no hace falta un
  reproductor del runtime para que se vean). Para mejorarlos: pasar a MP4 y envolver el arranque del XMV.
- 40 `.xau` (379 MB, audio), 846 `.igb` (1,1 GB, modelos Alchemy), 5946 `.tga` + 329 `.png` (cartas,
  monstruos, `euro\` con 6100 archivos: probablemente las cartas por idioma).

## Pendiente (lista de la guía, §15)

0. **2026-11-08: compilado** `ygo-apex\build\Release\ygo_recomp.exe` (139 MB) con `fatal.c` y
   `mando_directo.c`. **Sin probar todavía**: arrancar con `ygo-apex\run.bat`; el registro queda en
   `ygo-apex\runs\last_out.txt` y `last_err.txt`.
1. ~~Pipeline~~; ~~compilar~~; que llegue al menú (lo prueba el usuario).
   - **Primer arranque (2026-11-08): se cuelga sin ventana.** Kernel, memoria, APU, OHCI y las carpetas
     TDATA/UDATA bien; `contentmeta.xbx` no existe (normal). Después de 7 `RtlInitializeCriticalSection`
     del CRT (`sub_00361D88`, desde 0x00361D92) no hay más llamadas al kernel: el hilo del juego gira
     en código propio y nunca crea el dispositivo D3D. Los vblanks siguen llegando.
   - Al reintentar "se abría y cerraba": la instancia colgada tenía tomados `runs\last_*.txt`. Ahora
     `run.bat` y `run_debug.bat` hacen `taskkill` antes.
   - **Causa (watchdog):** 327 millones de `NtQueryVirtualMemory` (ordinal 217) en 15 s. El juego
     (`sub_0018D690`, estadística de memoria) recorre el espacio desde 0 con `addr += RegionSize` mientras
     `VirtualQuery` (`sub_001214DF`, XAPI) tenga éxito. El kit respondía "libre, 4 KB" para cualquier
     dirección y nunca error: el recorrido daba la vuelta en 0xFFFFFFFF y no terminaba.
   - **Arreglo (en el kit, genérico):** `xboxrecomp-apex/src/kernel/kernel_bridge.c`,
     `bridge_NtQueryVirtualMemory`: por encima de 0x7FFEFFFF devuelve `STATUS_INVALID_PARAMETER`, como el
     kernel real. **Probado: funciona** (el arranque pasa la estadística de memoria). Sin commit en el
     fork todavía (sería el parche 0073).
   - **Siguiente problema (2.º arranque):** una llamada indirecta a `0x006288B0` (en `.data`, ceros, nadie
     la referencia directo) falla ~7000 veces; antes, una a `0xD4C2E800` (basura). La última función real
     antes del fallo es `sub_00185C50`, que recorre dos listas de objetos (`[0x670D28]`, `[0x670D24]`)
     llamando a los virtuales `+0x68` y `+0x50`. Sospecha: un objeto con tabla virtual mal armada.
   - `recomp_manual.c`: el registro de ICALL fallidos ahora dice **a dónde vuelve la llamada** (el sitio),
     los registros, las direcciones de código de la pila en el primer fallo, y se limita a 1, 10, 100...
   - **Causa real (3.ª–8.ª pruebas, corridas por mí con `tools\prueba.ps1`):** el heap del motor Alchemy
     no conseguía memoria. Cadena:
     - administrador de memoria global `[0x670D1C]` (objeto en el heap del CRT, vtable `0x46AE80`;
       `+0xA4` alloc → `+0x144` `sub_001AF320`) — bien creado;
     - su backend es dlmalloc con morecore `sub_0019B400` (vtable de plataforma `0x466DE8` `+0x6C`):
       busca con `VirtualQuery` una región `MEM_FREE` de ≥ 64 KB y la reserva con `VirtualAlloc`
       (`sub_00121449`) en esa dirección;
     - el kit sin `RECOMP_EXT_VMA` responde lo libre en trozos de 4 KB → nunca halla 64 KB → todas las
       reservas devuelven NULL → la tabla de textos `[0x670D08]` queda nula → llamadas por punteros nulos
       (`0x006288B0`, `0xD4C2E800`) y el bucle eterno de `sub_00195E60`.
   - **Arreglo:** `RECOMP_EXT_VMA=1` (en `env.bat` y en los valores por defecto de `main.c`). Probado: la
     primera reserva da `0x010D1000` y el motor sigue.
   - `src/diag_memoria.c` (diagnóstico, `YGO_MEM_TRACE=0` lo calla): envuelve `sub_001AF320`,
     `sub_0019B400` y `sub_00121449` y anota pedidos y resultados.
   - **Siguiente:** llamadas a `0x00197FD0` (free del administrador) "no resueltas": funciones que solo se
     llaman por vtable y el desensamblador no vio. `tools\semillas_vtables.py` busca en `.rdata`/`.data`
     punteros a código que no son inicio de función → `analysis\semillas_vtables.json` (26). Se regenera
     todo con `tools\regenerar.ps1` (usa toda `analysis\semillas_*.json`).
   - `tools\rename_pads.py` ahora busca solo en qué `recomp_00xx.c` está cada función.
   - **Error en mis pruebas (corregido):** `tools\prueba.ps1` no encontraba `env.bat` (cmd no arrancaba en
     la carpeta, por los espacios de la ruta), así que las pruebas corrían sin sus variables. Ahora usa
     `cd /d` y rutas absolutas. Con `env.bat` aplicado: D3D11 activo, memoria bien, >1 millón de llamadas.
   - **Siguiente (9.ª prueba):** 4 destinos no traducidos (`0x002A8AA0`, `0x003621BE`, `0x0035AD2E`,
     `0x00137570`: punteros a función usados como inmediatos en el código) y luego `abort()` del CRT
     (`raise` → `_exit(3)` desde `0x00360BE5`, atrapado por `fatal.c`).
   - Semillas: `semillas_vtables.py` ahora también toma los inmediatos del código (`XREF (data_imm)`) y
     acepta direcciones dentro de un cuerpo si antes hay relleno o `ret` (213). `semillas_runtime.py`
     (lo corre `prueba.ps1` al final) acumula los "Failed to resolve" de cada prueba en
     `analysis\semillas_runtime.json`. Regeneración 2 en curso.
   - Las semillas no se acumulaban (una función hallada en una pasada ya figuraba en `functions.json` y no se
     volvía a escribir; la siguiente regeneración la perdía: `0x00197FD0` → cierre por acceso inválido).
     `semillas_vtables.py` ahora conserva las anteriores (244 en total).
   - **Hito (regeneración 3, 2026-11-08): primer cuadro en pantalla.** `device up: feature level 0xB000,
     swap chain 640x480`, render target 640x480 (1280x960 host), ~49.500 presents en 25 s (1 dibujo y 1
     textura por cuadro: una pantalla fija; el ritmo no está limitado). Los `FILE_NOT_FOUND` de
     `texdb0NN.txt`/`nmtexdb.txt` son normales (el juego prueba el siguiente número).
   - `prueba.ps1` pone `RECOMP_PAUSE_UNFOCUSED=0`: si no, el runtime pausa el juego porque la ventana de
     prueba nunca está al frente.
   - Siguiente: `0x0041B320` (callback de la tabla de tipos de dispositivo de XPP, `0x41B174..0x41B184`,
     ni siquiera desensamblado) y después se queda en esa pantalla. Regeneración 4 en curso.
2. Envoltura del error fatal del juego (`[FATAL]`), además del filtro de crash que ya trae `main.c`.
3. Mando directo: reemplazar el `XInputGetState` del XDK (buscarlo en XPP / .text).
4. Panorámico: `XGetVideoFlags` y las variables de aspecto del juego.
5. Mouse: en un juego de cartas, más bien cursor/menús que cámara — a decidir.
6. Audio: medir mixbins (`RDR_AUDIO_BINS=1`).
7. Videos: ver si son XMV y si conviene reemplazarlos por MP4.
8. Texturas: muchas ya son `.png`/`.tga` sueltas en la carpeta del juego (cartas, fondos).
9. `shaders.pak`, F2 y launcher, paquete y Release.

## 2026-11-08: pasa la pantalla de "disco sucio" y el video de apertura

- **"Hay un problema con el disco"**: NtReadFile de `sound\sd_list.lst` rechazaba el búfer en 0x74851EA8
  (el heap del motor vive en la VMA extendida, 0x74000000+) porque `bridge_va_mapped` no conocía ese rango.
  Kit: `guest_vmem_committed()` nuevo (guest_vmem.c/.h) y usado en `bridge_va_mapped` (kernel_bridge.c).
  Sin commitear (sería el parche 0074 junto con el de NtQueryVirtualMemory).
- **Video de apertura**: `sub_0002C690(path, flag)` (cdecl) es el reproductor XMV síncrono; con XMV el juego
  deja de presentar y espera para siempre. `src/videos.c` lo salta (al = 0, como archivo ausente);
  `YGO_XMV=1` usa el reproductor original. Pendiente: MP4 (guía §7).
- Resultado: 60 fps estables, 28 draws por cuadro, 12 texturas: pantalla de título esperando START (a confirmar).
- 2026-11-08: el usuario llegó jugando hasta un duelo (título, menús y duelo funcionan).

## 2026-11-08: ratón nivel 1 (el ratón maneja el cursor del juego)

- `mando_directo.c`: `mouse_dpad()` convierte el movimiento del ratón (modo directo, sin stick) en toques
  de la cruceta: un paso cada `YGO_MOUSE_STEP` cuentas (110/70/40 = baja/media/alta), pulsación de 45 ms
  y 40 ms de pausa, solo el eje dominante, como mucho 2 pasos en cola.
- `main.c` (`rdr_mouse_init`, reescrita): clic izq. = A, clic der. = B, central = Y, rueda = arriba/abajo;
  Espacio = A, Esc = B, Tab = BACK. Quitado lo de RDR (gatillos, sensibilidad al apuntar).
- F2 › Controles: "Ratón mueve el cursor" (raton_cursor) y "Sensibilidad del cursor" (raton_sensibilidad),
  en 6 idiomas (f2_idiomas.c). **Launcher: falta** (todavía no hay launcher de este port; agregarlo ahí).
- Siguiente (nivel 2): apuntar de verdad en el duelo (buscar la variable de la casilla seleccionada).

## 2026-11-08: audio (sin sonido)

- La música (voz 68, ADPCM estéreo, stream de .xau) vive en el heap de la VMA extendida (0x747xxxxx).
  `MmGetPhysicalAddress` devuelve la dirección tal cual y `mcpx_apu_phys` la cortaba a 28 bits → leía ceros.
- Kit `apu/apu_core.c`: si la dirección está comprometida en la VMA extendida (`guest_vmem_committed`),
  se lee directo de la ventana del guest. Después: rms ~3000 en la voz, bins 0/1 a −30 dB (antes −98).
- Solo usa los bins 0 y 1 (estéreo): no hace falta la bajada 5.1. Falta oírlo: ¿velocidad/tono correctos?
  (la voz dice rate=2.177; verificar de oído).

## 2026-11-08: línea en el cuadro de diálogo, voces, búsqueda del cursor

- Línea vertical en el centro del cuadro de diálogo (dos mitades espejadas, escala 2x): `snap_2d` activado
  por defecto (`YGO_SNAP_2D=0` lo apaga) y F2 › Imagen › "Corregir líneas en 2D" (lineas_2d). A confirmar.
- Voces: el disco solo trae `sound\BGM` (música) y `sound\SE\Static.xau` (efectos); no hay voces grabadas.
- Ratón estilo Joey the Passion, paso 1: `src/buscar_cursor.c` + `buscar_cursor.bat` (YGO_SCAN=1).
  F10 = foto de memoria; entre fotos el cursor se mueve una casilla igual; quedan los valores que cambian
  con el mismo paso. Resultado en runs\last_err.txt ("[SCAN]").
- **Cursor del duelo encontrado:** `sub_00086F50(jugador, zona, casilla)` lo coloca y guarda
  [0x6633D4] jugador, [0x6633D8] zona (0 monstruos, 5 magias/trampas, 10 especial…), [0x6633DC] casilla.
  ([0x657138] también siguió la mano.) Kit: `nv2a_d3d11_request_dump(path)` (nv2a_pb_d3d11.c) guarda el
  próximo cuadro; `buscar_cursor.c` saca una foto por casilla en ygo-apex\fotos_cursor\ para medir posiciones.
- **Ratón modo Puntero** (`src/raton.c`, por defecto): el puntero de Windows queda visible; sobre el campo
  del duelo, apuntar a una casilla lleva el cursor del juego ahí (aprieta la cruceta hacia la casilla y mira
  [0x6633D4/D8/DC]; si 3 toques no lo mueven, hay un menú abierto y el puntero mueve la selección por pasos).
  Clics: izq. A, der. B, central Y (leídos con GetAsyncKeyState, sin captura). Posiciones medidas de
  fotos_cursor (640x480): monstruos x=241+44i y=263; magias x=238+45.5i y=300; mano x=100+87i y=365;
  zonas 10/12-15 a los lados; el rival es el espejo ×0.88 alrededor de x=330, filas 154/121.
  F2 › Controles › "Modo del ratón": Puntero / Cursor por pasos / Solo clics (raton_modo, 6 idiomas).
  Pendiente: la rueda en modo Puntero, las zonas del rival sin foto (magias), confirmar de oído/vista.
- Desfase del puntero (2026-11-08): con "Panorámico (nativo del juego)" el cuadro 640x480 se muestra
  estirado a 16:9 y el puntero se calculaba en 4:3; ahora usa `xbox_VideoWidescreen()`. Posiciones
  re-medidas sobre la casilla (la manito queda ~10 px abajo a la derecha): monstruos y=252, magias y=287,
  rival 142/110, el rival numera de derecha a izquierda (supuesto).
- Arranque siempre en ventana (pedido del usuario): main.c, tras `xbox_MenuLoad`, pide fullscreen=0
  (gana sobre el "Pantalla completa" guardado). F11/Alt+Enter/F2 siguen cambiando en el juego.
- Puntero "ultra desfasado" todavía: diagnóstico en raton.c, cada clic izq. registra
  `[RATON] click client x,y of WxH -> game gx,gy -> square ... cursor ...` y las primeras 40 guardan
  ygo-apex\fotos_raton\click_NN.bmp (píxeles de la ventana). Esperando una prueba del usuario.
- Prueba del usuario (clics + vídeo, 2026-11-08): el mapeo puntero→juego era **correcto** (en las fotos
  el punto cae sobre la carta apuntada, ventana 853x480 en 16:9). El fallo: (1) solo se esperaban 85 ms a
  que el cursor cambiara; con la animación de la mano eran "fallos" y a los 3 pasaba a un modo menú que
  movía la selección con el movimiento relativo → desfase loco; (2) la copia [0x6633D4] queda vieja al
  empezar el turno en la mano. Arreglo: espera de 300 ms por toque, sin modo menú relativo (3 fallos =
  deja de insistir hasta otra casilla), cursor = la copia que cambió último entre [0x6633D4] y
  [0x657130] (sub_00046630), rueda vía gancho WH_MOUSE_LL (arriba/abajo).
- Usuario (2026-11-08): "quedó igual o peor; no podemos tener 2 mouse, un mouse solo en el juego".
  Registro: al empezar el turno en la mano, [0x6633D4] y [0x657130] valen 0/0/0 (sub_00046630 solo lo llama
  sub_00086F50: no son independientes); la mano en ese estado usa otra variable, desconocida.
  Cambio: por defecto **"Cursor del juego"** (la manito es el único puntero; Windows oculto/capturado; el
  ratón mueve la cruceta), pasos más largos (180/120/70 cuentas) y como mucho 1 paso en cola. El modo
  Puntero queda como "Puntero (experimental)" en F2. Pendiente para el Puntero: la variable de la mano.
- 2026-11-08 (sin abrir el juego, a pedido del usuario): modo **Puntero (estilo PC)** por defecto otra vez,
  con UN solo puntero: el de Windows dibujado como la manito del juego (comm_13_00 del atlas
  euro\<idioma>\comm\2d\texdb\texdb000.png, punta en 22,14; src/raton_guante.h) y la manito del juego borrada
  en una copia del atlas (ygo-apex\mods_puntero\, tools/guante.py, PNG paletado como el original; main.c
  apunta RECOMP_MODS_DIR ahí en modo Puntero). Si el cursor vale 0/0/0 y 3 toques no lo mueven, un toque
  lateral lo "despierta". **Sin probar.** No volver a correr el juego por mi cuenta (memoria actualizada).

### 2026-11-08: the pointer was slow to catch up (09:17 video)
- In the 09:17 run the game's cursor copies ([0x6633D4] and [0x657130]) stayed at 0/0/0 on almost every click, even though the game's cursor was moving.
- Each press therefore waited the full 300 ms timeout, which is why the cursor was slow.
- `pointer_mode` (raton.c) now counts the steps itself:
  - each press moves the estimate one square, using `next_slot` on the same `slot_place` squares;
  - the next press goes right away (45 ms held plus 50 ms gap);
  - the copies only correct the estimate after 300 ms with no presses;
  - each target is limited to 14 presses.
- The click log now shows the estimate.
- The wake press and the click pictures were removed.
- Not tested. Pending: at the start of a turn the copies read 0/0/0 while the cursor is on the hand, so the estimate starts from monster 0. Look for the real hand variable.

### 2026-11-08: the pointer follows the game's real cursor (10:50 video: "unplayable")
- The 10:50 log showed that [0x6633D4] and [0x657130] stay at 0/0/0 all the time. Counting steps can't work without a real position.
- The game's glove is an object with vtable 0x454FB0, built by `sub_000EE550` at (320,240).
  - `sub_000EE6F0` (vtable +0x34, thiscall, x and y as floats) is "point at x,y". It stores the point at +0x4C/+0x50 and animates the glove there (`sub_000EE7B0`; target at +0x20/+0x24, current position at +0x38/+0x3C).
- `sub_000EE6F0` is now wrapped in raton.c (added to the PADS list in rename_pads.py). It saves the last point, which is the game's real cursor in the 640x480 frame.
- `pointer_mode` now works in a closed loop:
  - the target is the centre of the square under the pointer, or the pointer itself outside the field (menus);
  - it presses up/down first, then left/right;
  - if a press doesn't move the cursor within 250 ms, or moves it farther away, that axis is dropped for this target;
  - if a press made it farther, it presses once back.
- The click log shows the pointer and the game's cursor (`[RATON] click game x,y; game cursor x,y`).
- Not tested. If something is off, the user's click log will show how far the point given to the glove is from my square centres.
- 10:57 run: `sub_000EE6F0` was never called (the game's cursor stayed at -1,-1). The duel moves the glove directly with move-to (+0x2C).
- `sub_000EE7B0` (the glove's per-frame update, vtable +0x28) is now wrapped as well. It reads the destination at +0x20/+0x24 and keeps the glove whose destination changed last.
- The click log shows `updates N`. If N stays at 0 in the duel, the duel glove is another class: look for the class whose texture is comm_13_00 (table at 0x608878).
- 11:00 run: the cursor moves now (updates > 0), but it was off and slow between rows ("it goes through every letter").
  - The glove is drawn at the point + 15 (float at 0x42A200), so `GLOVE_OFS` is now subtracted.
  - Each axis now moves until a press no longer brings that axis closer. That press is undone, so the cursor stops at the nearest reachable spot whatever the spacing. The thresholds are 4 px (they were 18/14).
- 11:03 run: the 0x454FB0 glove stayed at 320,240 for the whole duel, so the visible cursor was a different object.
- Rewrite: every glove sprite is now tracked through the sprite base class.
  - `sub_000F80D0` (vtable +0x0C, build from a picture; called by `sub_000F80A0`, vtable +0x14) registers every sprite whose picture contains `comm_13_`.
  - `sub_000F85F0` (move-to, all sprites) records the destination of those sprites.
  - The fingertip is the destination + (22-32, 14-32): the sprite centre plus the tip of the 64x64 picture. In the 11:00 run it matches the pointer to within 3 px.
  - The `sub_000EE6F0` and `sub_000EE7B0` wrappers were undone (names restored in recomp_0020.c, removed from PADS).
- `pointer_mode`:
  - the target is the pointer, with no snapping to squares;
  - an axis is done when the distance is less than half the last step seen on it;
  - a press that goes past is undone.
- Click log: `game fingertip x,y (gloves N, moves M, steps h/v)`.
- 11:10 run: the name screen is fine (fingertip within 3-12 px). Two problems in the duel:
  - a jump between two gloves was counted as a 150 px "step", so the cursor counted as arrived 75 px away (the user reported the right-hand action button being picked);
  - the field glove got stuck at 310,222, because it isn't always moved with move-to.
- Fixes:
  - registry of up to 64 gloves, each with its vtable as a liveness check;
  - `glove_poll` reads every glove's +0x20/+0x24 on each pad read, and the one that changed last is the cursor;
  - steps are per glove (reset when the glove changes) and only count when they are under 160 px.
- Ctrl+F8 switches between 60 and 90 fps: `xbox_VblankHzSet` in kernel_pacing.c (new kit API), with the hotkey in raton.c.
  - The game times by vblanks (sprite move-to counts frames), so at 90 it probably also runs 1.5x faster. Not verified.
  - Still to do: an F2 menu and launcher option.
- 11:15 run: 52 gloves were built in the duel, but none were followed. Bug: the vtable was saved at build time, and the derived classes set theirs afterwards, so every glove was dropped at the first poll.
- Fixes:
  - the vtable is now taken at the first poll;
  - each glove is watched at its drawn position (+0x38/+0x3C, not only the destination); a move only counts above 6 px, so the glove's wobble is ignored;
  - the glove that moved last is the cursor; if none has moved yet, the last one built is used;
  - a press is judged once the glove has stopped (70 ms without change, at most 700 ms).
  - The `sub_000F85F0` wrapper is now a pass-through.
- After each click the log lists the 4 gloves that moved last (obj, vt, pos, dest, frames).
- 11:24 run, video watched (ffmpeg frames) together with the log.
  - In the duel the hand highlight stayed on card 3 while the pointer moved across the hand.
  - The log shows the new screen's gloves unplaced (pos 0,0, dest 320,240): the game only places its glove after the first d-pad press. Since the cursor position was unknown, nothing was pressed (s_gx = -10,-18).
- Fixes:
  - unplaced gloves (pos 0,0) are ignored; with no placed glove the position is "unknown";
  - when it is unknown, a probe press (right, left, down, up, at most 4 per screen) makes the game place its glove;
  - positions come from the destination (+0x20/+0x24, set on the press) and the settle time is 30 ms;
  - the press back after going too far is waited for.
- 11:29 run: the cursor was now located (gloves 52, fingertip followed), but steering one axis at a time left it on the wrong row: pointer on the hand at y 366, cursor on the field at y 287. Once a key didn't bring it closer, that axis was dropped.
- Rewrite of `pointer_mode`: a map of places that is learnt as it goes.
  - Places are the glove's destinations (within 4 px), each with what each d-pad key does from there: unknown, another place, or "nothing". "Nothing" is forgotten after 3 s.
  - Each decision runs a breadth-first search over the known keys from the cursor's place. The reachable place nearest the pointer is the target, and the first key of the path is pressed.
  - At the target, it tries unknown keys that point toward the pointer, provided the pointer is more than half the smallest step seen on that axis away.
  - When the map turns out wrong, it is relearnt.
- A left click waits until the cursor is there (at most 700 ms) before pressing A.
- The probe press for an unplaced glove is kept.

### 2026-11-08: Joey-style pointer for real (the field cursor is set directly)
- Code analysis, no game runs.
- `sub_00092FA0` is the duel field cursor's d-pad handler. `sub_00099660` (the field's per-frame update) calls it whenever the field cursor has the pad.
  - It reads this frame's keys from [0x6589E6]: 0x10/0x20/0x40/0x80 are the directions and 1 is A.
  - It reads the cursor from [0x6633D4]/[0x6633D8]/[0x6633DC] and switches on the zone to find the neighbour.
  - It then calls `sub_00086F50(p,z,q)`, plays the tick `sub_0007E280(0x10000001)`, and returns 0 (1 when A is pressed).
- The duel board positions table: the names `v_pos_*` and `*_c_point` come from `sub_0005D4D0`, and the table lives at [[0x656F8C]+0x28]: monsters at +0x20/+0x5C, m&t at +0x98/+0xD4, tomb at +0x110, deck at +0x170 (vec3 world positions). Not used yet.
- `sub_00092FA0` is wrapped in raton.c (added to the PADS list). While it runs, the field is "live".
  - When the pointer moves onto another square and no key is pressed this frame, it does what the game does for a press, but to that square: `sub_00086F50` + tick + return 0.
  - Otherwise the game's own handler runs.
- `field_pointer`:
  - the hovered square comes from `slot_at`; the hand is limited to the card count at 0x65D0A8 + p*0x13F8;
  - a click presses A once the move has gone through (at most 150 ms).
- `field_learn`: once the cursor has stayed on a place for 150 ms, the glove's fingertip there is saved as that place's position. A hand's positions are relearnt when its card count changes.
- Outside the field (menus, name screen), the place-map method still runs.
- 11:47 run: progress, but the cursor was still off, and with a still pointer it went back and forth between 2 cards.
  - Cause: places were relearnt from the glove, sometimes mid-move or from another glove, so a place ended up between two cards.
- Fixes:
  - the field uses only the measured centres (no learning);
  - each hand card is learnt once per card count, with the glove at rest for 400 ms and within 60/50 px of where it should be;
  - hand cards not yet seen are extrapolated from the ones seen, at their spacing;
  - hysteresis: while the pointer stays inside the box of the wanted square, the target doesn't change.
- 11:53 run: the field works ("big step forward"). Still missing: the card command buttons (they were off) and the main menu wheel.
- Command buttons, from the code:
  - `sub_00080D40` (called from `sub_00099660`) handles them; step [0x65F8FC] (2 = waiting for keys), mask [0x65F8FA] (13 commands), chosen command = bits 2-5 of [0x65F8F8].
  - The row is the object [[0x656F88]+0x4C]: +8 chosen button, +0xAC count, +0x84[k] command of button k; the button object is at +0xC[command], with its centre at +0x10/+0x14.
  - Selecting a button: `sub_000ED100(k, 1)` (thiscall, ret 8), plus the tick.
- The wrapper of `sub_00080D40` (in PADS): during step 2, with no keys pressed this frame and a different button under the pointer, it writes the command bits, calls `sub_000ED100`, plays the tick, and then runs the game's own code. `cmd_pointer` hit-tests within 34 px against the real positions read from memory; a click presses A once the selection has gone through.
- Wheel (Duelo individual/triple…): still to find. It isn't the glove (fingertip -1 on that screen). `sub_000EB0C0` is a slider; `sub_0010F790` (vars 0x66E68x) is a menu with 9 ticks, not yet confirmed to be the wheel.

### Ratón: rueda del menú principal (2026-11-08)
- La rueda de modos (Duelo individual, triple…) no tiene manito: son íconos 3D en medio círculo y el sol marca el elegido.
- Objeto de la rueda (update thiscall `sub_0010D6C0`, ahora envuelto en raton.c y en PADS):
  - +0x6C estado (0 = espera teclas); +0x9C animación del paso (0 = quieta).
  - +0xA0 ícono elegido; +0xA4 destino del paso; +0xAC cantidad.
- Ícono k en ángulo 180 − k·180/(n−1) grados, punto (8cos, 8sin, 0). `sub_0010CA40` (thiscall sobre [rueda], in, out; ret 8) lo pasa al cuadro 640×480; así coloca el sol `sub_0010D3B0`.
- `sub_0010D460` / `sub_0010D590` arrancan un paso a siguiente/anterior (tick, texto de ayuda); el estado 2 del paso copia +0xA4 a +0xA0. Se llama al que corresponda y se pone +0xA4 = ícono del puntero: llega directo con la animación y el sonido del juego.
- `wheel_pointer`: ícono más cercano dentro de 0.6×separación (máx 70 px), con histéresis; clic = A cuando la rueda llegó y está quieta (o 800 ms). Log `[RATON] wheel …`.
- Compilado (rc=0), sin probar.

### Ratón: selección trabada, flechas (2026-11-08, tarde)
- Log 12:15: la rueda eligió bien (1 clic). Los 3 clics de botones fueron a x 41–164, lejos de todo botón (ahora el log da dónde está cada botón).
- **Trabado:** el campo, los botones y la rueda gastaban el pedido del puntero aunque ese cuadro no se pudiera aplicar (tecla apretada, animación). Además nada lo reenviaba hasta pasar a otro objetivo.
  - Ahora queda pendiente hasta hacerse.
  - Se reenvía cada vez que el puntero se mueve (>3 px) sobre un objetivo, así el cursor vuelve bajo el puntero si el juego lo movió.
- **Pantallas con flechas sin manito** (envueltas, en PADS):
  - `sub_00109EB0`, preparación del duelo: estado 3 = piedra-papel-tijera (0x800 → `sub_00107C30`, 0x400 → `sub_00107BD0`, A); estado 9 = otra elección izq/der (`sub_00108A70`/`sub_00108A10`).
  - `sub_00087A80`: elección de 3 del duelo (0x660B8A, espera con [0x660B9A]=0).
  - Clic a más de 60 px a la izquierda del centro = izquierda; a la derecha = derecha; centro = A. La rueda del mouse también. Log `[RATON] arrows click …`.
- Antes, en esas pantallas el puntero caía en el mapa de lugares y sus "sondas" apretaban direcciones solas (cambiaban la mano en piedra-papel-tijera).
- Selección de personajes: pantalla no identificada todavía (¿estado 9?). Falta captura o log de esa pantalla.
- Compilado (rc=0), sin probar.

### Ratón: botones de acción, el clic no confirmaba (2026-11-08)
- Log 12:26:
  - Botones en 272/322/372, y 260: coinciden con la captura.
  - 3 clics sobre "Llamar" (botón 2, ya elegido), ninguno tomado; después de cada uno, B.
- Causa: el paso 2 de `sub_00080D40` ignora A mientras la fila se desliza (`sub_000482C0` → `sub_000EC9D0`), y cada hover arranca ese deslizamiento con `sub_000ED100`. El A de 60 ms enviado desde el lado de la entrada se perdía.
- Ahora el clic marca `s_cmd_click`. El wrapper pone A (byte 0x6589E6 = 1) solo para el handler, cuadro a cuadro, hasta que el paso deja de ser 2 (log `[RATON] buttons: A taken`) o pasa 1.5 s. Después restaura el byte.
- Compilado (rc=0), sin probar.

### Ratón: botón del medio nunca elegido (2026-11-08, video 12:31)
- Video: el botón izquierdo (info) y el derecho ("Llamar") andaban; con la manito sobre el del medio seguía marcado "Llamar". Log: pedido botón 1, elegido seguía 0 o 2; el clic confirmaba el viejo.
- Causa: se escribía como número de comando el +0x84[k] de la fila (número de dibujo). Los pasos del pad del juego recorren los bits de [0x65F8FA]: el botón k es el k-ésimo comando ofrecido. El bit del número del medio no estaba ofrecido, así que se descartaba.
- Ahora comando = k-ésimo bit puesto de [0x65F8FA]. El log del clic agrega el comando y la máscara ofrecida.
- Compilado (rc=0), sin probar.

### Ratón: clic fuera de los botones = volver; selección de oponente (2026-11-08)
- El usuario confirmó que los botones de acción ya funcionan.
- Clic izquierdo fuera de los 3 botones = B (volver al duelo), entregado desde el wrapper igual que A (byte 0x6589E6 = 2 hasta que el paso cambia). Log `[RATON] buttons: B taken`.
- Menús con izq/der sin envolver: `sub_000F3DE0`, `sub_00101FD0`, `sub_00106CE0`, `sub_0010B0C0` (en PADS). Uno es "Seleccionar oponente" (sin manito; los clics del log tenían fingertip −1). Con esos activos y sin manito rastreada (`s_gx < 0`), los clics van a las flechas (izq/der/A), como piedra-papel-tijera. Log `[RATON] arrows click … (screen F3/101/106/10B)` dice cuál era.
- Compilado (rc=0), sin probar.
- **Regresión 12:41:** la rueda del menú principal dejó de responder. Uno de los 4 menús nuevos también corre bajo el menú principal (sin manito), y `side_pointer` iba antes que `wheel_pointer`, así que le sacaba el hover a la rueda.
  - Ahora la rueda va primero.
  - `side_if_gloveless` no se activa si la rueda corrió hace menos de 150 ms.
  - Regla para lo próximo: cada pantalla nueva se agrega después de las que ya andan y no se activa si hay otra activa.

## 2026-11-08: resolución real y revisión del menú F2 (venía de RDR)

- **Resolución interna:** sí se aplica. El log muestra los targets del juego a 2560x1920 en x4, y la copia a la ventana ya promedia 4x4 muestras (supermuestreo) cuando el cuadro es más grande.
- **Lo que la anulaba:** `snap_2d` ("Corregir líneas en 2D", encendido por defecto desde la mañana).
  - `nv2a_snap_uv2d` lleva el uv al centro de cada bloque NxN, así que todo dibujo 2D plano (cartas, menús, textos, tablero: casi todo el juego) se muestreaba como a 1x y se veía 480p a cualquier escala.
  - Ahora está apagado por defecto (`YGO_SNAP_2D=1` lo prende).
  - La fila F2 nueva es "Imagen 2D (cartas, menús, textos)": Alta resolución / Original (480p, sin líneas). Clave nueva `imagen_2d`, para que el `lineas_2d=Sí` guardado no lo deje prendido.
  - Costo: puede volver la línea fina en el medio del cuadro de diálogo. Falta un arreglo propio (recorte de medio texel en el borde de la textura) en vez del snap.
- **Revisión fila por fila:**
  - Ventana, Pantalla completa, Anisotrópico, VSync, Mostrar FPS, Volcar texturas, Modo experimental (run_ahead), Idioma (el juego sale en español con `es`) y las filas del ratón: genéricas o propias del port, se aplican.
  - **Panorámico:** era de RDR. Acá XC_VIDEO dice 4:3 y el `widescreen.c` de RDR no está, así que solo estira el 640x480 a 16:9. Ahora dice "Panorámico (estira la imagen)": No (4:3) / Estirar a 16:9. Se quitó "Ultrapanorámico". Panorámico real (hor+) queda pendiente: buscar la proyección y las variables de aspecto del juego.
  - **Videos pregrabados:** quitada; los videos se saltean hasta que esté el reproductor MP4.
  - **Ver controles:** quitada; mostraba `controles.bmp` del launcher de RDR, que acá no existe.
  - Texturas mejoradas / Botones: solo aparecen si hay pack, así que no se ven.
  - Velocidad: oculta (`RDR_VELOCIDAD`).
- Textos nuevos traducidos en f2_idiomas.c. El launcher de YGO todavía no existe: cuando se haga, lleva las mismas filas.
- Compilado (rc=0), sin probar.
- **12:52, con 2D sin corregir:** volvieron líneas (cuadro Sí/No del fin de turno, recuadro del nombre, globo de diálogo), por el muestreo que pasa el borde de la textura.
  - Modo nuevo `snap_2d = 2`, por defecto. El kit guarda el modo entero (`nv2a_backend_common.c`).
  - `nv2a_uv_snap` marca la etapa 2D plana con -(1000+n). En HLSL, `nv2a_snap_ok` no ajusta a la grilla 1x (resolución alta) y `nv2a_edge(tex, uv, z)` recorta medio texel dentro del borde donde uv está a medio texel de 0..1 (si se repite como mosaico no se toca).
  - Aplicado en `NV2A_HLSL_PS_BASIC_SNAP` y `d3d8_combiners_hlsl.c`. Validado con d3dcompiler_47 (hr=0).
  - F2 "Imagen 2D": Alta resolución (2) / Original (480p) (1) / Sin corregir (0). `YGO_SNAP_2D=0/1/2`.
  - Metal (`.msl`) no se tocó: solo Windows.
- Los cuadros de 0.5–1.1 s del log 12:52 coinciden con la ventana en segundo plano (pausa a propósito), no con x4.
- "En el duelo se mueve raro": sin identificar todavía.
- **13:02, las líneas seguían con el recorte en el borde de la textura** (el log muestra el modo 2 activo y ningún shader fallido): no vienen del borde de la textura sino de cada pieza dentro de un atlas.
  - `nv2a_edge(tex, uv, z, pos, inout g)` ahora decide por píxel:
    - una guest pixel cubre ≥0.75 texel en x e y (panel, cuadro, texto a su tamaño o achicado) → se ajusta a la grilla 1x como el original, con gradientes 1x;
    - agrandado (cartas grandes, arte) → resolución alta con el recorte de medio texel.
  - Validado con d3dcompiler_47 (hr=0). Compilado (rc=0), sin probar.
- **13:10:** las cartas habían perdido resolución por el ajuste 1x por píxel de las piezas a su tamaño, y la línea del globo seguía.
  - Revertido: `nv2a_edge` ya no ajusta a la grilla 1x; solo resolución alta más el recorte de medio texel en el borde.
  - La línea del globo apareció en todos los modos de muestreo, así que es geometría: dos mitades que se tocan entre píxeles (ej. 319.75 / 320.25). A 1x no se ve; arriba de 1x queda una columna sin cubrir.
  - `s_vs_passthrough_src` (lotes sin programa, ya en píxeles de pantalla): después del 1/16 hace `p = ceil(p - 0.5)`, el borde de cobertura del raster 1x. Igual a 1x por construcción. Reemplaza el corrimiento x.5 de RDR (`vp_off.w`).
  - Si el globo se dibuja con programa de vértices, esto no lo alcanza: verlo en la prueba.
  - VS y PS validados con d3dcompiler_47 (hr=0). Compilado (rc=0), sin probar.

## 2026-11-08: clics estilo Joey the Passion en las cartas de la mano

- Joey the Passion, según guías de la época:
  - al pasar el mouse por una carta de la mano, un globo muestra la acción;
  - clic izquierdo = elegir / atacar / sí;
  - clic derecho = colocar boca abajo / no / cancelar;
  - clic derecho en un lugar vacío = menú.
- Comandos de la fila de botones, según los logs de 12:31–12:52 y las etiquetas del XBE (Llamar, Colocar, Activar, Volteada, Pos. Ataque, Pos. Defensa, Atacar, Fusión, Robar, Rendirse, Llamada Esp.):
  - 0 = Información de la carta.
  - 4 = Colocar: con la máscara 0x0011, elegirlo bajó la mano de 6 a 5.
  - 5 = Llamar: botón derecho con la máscara 0x0031.
  - El resto se reparte por eliminación.
- Implementado en raton.c:
  - `field_pointer`: el clic izquierdo en una carta propia de la mano (p0, z11) pone `s_auto = AUTO_MAIN`. El clic derecho ahí pone `AUTO_SET` y aprieta A en vez de B. En cualquier otro lugar el clic derecho sigue siendo B.
  - El wrapper de `sub_00080D40`, cuando los botones se abren dentro de 700 ms, elige el comando y confirma con A desde el lado del juego (la misma vía que el clic en un botón):
    - principal: 5, si no el menor ofrecido distinto de 4, si no 4, si no 0;
    - colocar: 4, o B si no se puede.
  - Log `[RATON] hand card …`.
- Pendiente: ataque con clic en un monstruo propio y después en el objetivo.
- Compilado (rc=0), sin probar.

## 2026-11-08: info de carta con el botón del medio; diagnóstico de la línea del globo

- Botón del medio del mouse sobre cualquier carta (campo o mano): abre "Información de la carta" de una. A abre los botones y el wrapper elige el comando 0 (`AUTO_INFO`). El botón del medio ya no es Y en el campo.
- La línea del globo siguió con el recorte de borde y con el redondeo de bordes en `s_vs_passthrough_src`, así que ninguno alcanza ese dibujo.
- Diagnóstico nuevo en `nv2a_pb_d3d11.c`: Ctrl+F9 registra todos los lotes del cuadro siguiente con `[DUMP]`:
  - con o sin programa, cantidad, topología;
  - rango de posición y de uv;
  - textura 0 (modo, tamaño, formato, direccionamiento, filtro, offset);
  - two_d / vary_z / place / flat2d.
- Con el globo en pantalla, las dos mitades se reconocen por x que termina y empieza cerca de 320.
- Compilado (rc=0), sin probar.

## 2026-11-08 (tarde): línea del globo según el volcado, panel de info, jugadas rápidas

- **Volcado Ctrl+F9 (13:29):**
  - Todo el 2D del juego va con programa de vértices (`prog 1`); por eso el redondeo en `s_vs_passthrough_src` no tuvo efecto.
  - Las dos mitades del globo son lotes de 23 vértices del mismo atlas de 1024x512 (off 81200000), con u 0..0.8916 y vary_z 1 → flat2d 0. Ni `nv2a_edge` ni el ajuste 2D las tocaban.
  - Arriba de 1x, la costura (u 0.8916) mezcla el texel vecino del atlas.
- **Recorte por lote** (`uvc_batch` en nv2a_pb_d3d11.c):
  - Se aplica a lotes two_d de ≤64 vértices con etapa 0 bilineal, textura del juego y uv dentro de la textura (sin mosaico).
  - El rango de coordenadas (entrada 9) menos medio texel va a `nv2a_ps_consts.uv_clamp`, campo nuevo también en `NV2APSConstants`, en el cbuffer HLSL y en el de MSL.
  - Los shaders de snap_2d hacen `uv = clamp(uv, uv_clamp[n].xy, uv_clamp[n].zw)` antes de la escala.
  - Fuera de esos lotes el rango es -1e30..1e30. Solo con `snap_2d = 2`.
  - Validado con d3dcompiler_47 (hr=0).
- **Panel de información de carta** (paso de botones 0xA):
  - Mientras está abierto, cualquier clic = B (`info_pointer`).
  - Durante 500 ms después de cerrarlo, los clics sobre la mano no hacen nada. Antes, al salir del panel se invocaba la carta de abajo.
- **Jugadas rápidas** (F2 "Jugar cartas rápido (animaciones)", `cartas_rapidas`, Sí por defecto):
  - `xbox_SpeedSet(4)` desde un comando distinto de info confirmado en los botones, o desde un clic en casilla propia (zona 0/5).
  - Vuelve a x1 cuando el cursor del campo corre 120 ms seguidos o los botones esperan (mínimo 250 ms, máximo 4 s). Log `[RATON] fast play on/off`.
- Compilado (rc=0), sin probar.

## 2026-11-08 18:45 — cartas colocadas invisibles en el campo
- Video 18-37-31: al colocar boca abajo (monstruo y trampa) la carta vuela al campo y desaparece; las zonas quedan vacías.
- Log: `[VSH] a fixed-function program (lighting 1, mask 0x95) needs more than 136 slots: its batches are skipped`. Con 4 luces el programa fijo no cabe y el objeto no se dibuja.
- Arreglo (xboxrecomp-apex nv2a_pb_exec.c): si no cabe, se rearma sin especular y luego quitando las últimas luces; log `[VSH] ... too long: built with spec .. mask ..`.
- Compilado (rc=0), sin probar. Si sigue invisible, la causa es otra (buscar esa línea del log).

## 2026-11-08 19:00 — la selección de la mano daba vueltas sola (lo que mostraba el video 18-37-31)
- Corrección: lo de las cartas "invisibles" de la entrada anterior fue una mala lectura del video. Lo que el usuario mostraba: con el puntero quieto sobre Ooguchi, la selección recorría Fantasma, Abuchee y las 2 cartas colocadas durante segundos; y entre 2 cartas la selección iba y venía.
- Causa: field_learn aprendía la posición de cada carta de la mano desde el guante del juego. Una carta aprendida mal mandaba el cursor a otra, que a su vez se aprendía mal, y así en bucle.
- Arreglo (raton.c slot_place): la mano propia usa siempre posiciones fijas, x = 100 + 87·i, y = 365 (medido en el video con 4, 5 y 6 cartas); con más de 6 cartas se reparten en 435 px (supuesto). Margen: se cambia de carta 8 px después de la mitad (keep 52 × 55).
- El arreglo de iluminación de la entrada anterior queda igual (no hace daño).
- Compilado (rc=0), sin probar.

## 2026-11-08 19:20 — video 18-50-57: la mano sigue saltando entre 2 cartas
- Video: 6 cartas en la mano; con el puntero cerca del borde entre dos cartas, la selección va y viene (Mano/Duende, Tao/Macis, Macis/Yaiba).
- Medido en el video a 1920 px: hasta 5 cartas, centros en 100 + 88·i (unidades del juego); con 6 cartas, 115 + 81,7·i. Cada carta mide ±37 y entre cartas hay ~9 px de hueco. La fórmula anterior (100 + 87·i) erraba hasta 15 px con 6 cartas.
- Arreglo (raton.c):
  - posiciones exactas según cuántas cartas hay (7 o más: repartidas en 115..524, sin ver);
  - una carta se elige solo si el puntero está dentro de su dibujo (±37);
  - la carta elegida se mantiene a lo largo del hueco (±45), así que ya no salta de una a otra.
- Log nuevo:
  - `[RATON] hover game x,y -> p z i` en cada cambio de carta;
  - `[RATON] the game moved the cursor ...` cuando el juego mueve el cursor por su cuenta (hasta 60 veces).
- Compilado (rc=0), sin probar.

## 2026-11-08 19:45 — la causa real del salto entre cartas (evidencia del log de 18:57)
- Log de 18:57 (con el registro de hover): `the game moved the cursor p0 z11 i1 -> p0 z11 i2` diez veces seguidas con el puntero quieto en i1. Además, "hover i4, cursor was i2": mi movimiento a i3 no llegó a aplicarse.
- Código del juego:
  - sub_00086F50 es lo único que escribe el cursor [0x6633D4..DC].
  - El update del campo, sub_00092FA0, mueve la mano ±1 solo con una dirección (bit 0x20 de [0x6589E6]).
  - [0x6589E6] son los flancos de [0x658548], que arma sub_0007D8C0/sub_0007D9E0 desde el mando.
  - Conclusión: al juego le llegaba una dirección. Además, esos flancos hacían que mi wrapper postergara su movimiento ((0x6589E6 & 0xFF) != 0).
- Arreglo (raton.c):
  - mientras el mouse maneja el campo, se borran todas las direcciones del estado del mando (d-pad, los dos sticks) y nuestras propias pulsaciones de d-pad; se mantienen solo los pasos de la rueda (s_held_wheel);
  - el recorrido del guante (place-map) no pulsa nada si el campo corrió hace menos de 2 s.
- Log: `[RATON] field: direction input dropped (...)` (como mucho 1 por segundo) y `our own d-pad press dropped`, para saber de dónde venía.
- Compilado (rc=0), sin probar.

## 2026-11-08 20:00 — elección por la mitad y sin clics de Joey the Passion
- Captura del usuario (1920 px): centros de las cartas 344/590/835/1082/1329/1577 px = 115/197/278/361/443/526 en unidades del juego (÷3); confirma la fórmula de 6 cartas.
- Queja: con el puntero en el medio, entre dos cartas, seguía marcada la anterior. Era el margen de ±45, que hacía que el hueco entre cartas quedara para la carta ya elegida.
- Ahora (raton.c, hand_step):
  - gana la carta más cercana al puntero;
  - el cambio es en la mitad entre dos cartas (media separación), con 2 px de margen.
- Clics de Joey the Passion quitados, a pedido del usuario ("se siente horrible"):
  - el izquierdo sobre la mano vuelve a ser A normal (abre los botones del juego);
  - el derecho vuelve a ser B;
  - el del medio (información de la carta) y las jugadas rápidas siguen igual.
- Compilado (rc=0), sin probar.

## 2026-11-08 20:20 — clics fantasma, textura de las flechas, nombre con teclado
- Log 19:10: `the game moved the cursor` desapareció (solo 1, al empezar el duelo), así que el arreglo de las direcciones funcionó.
- **Clics fantasma.**
  - Evidencia: una sola pulsación dejaba en el log `field click` y `click game` a la vez.
  - Causa: el campo y el clic genérico llevaban cada uno su propio "was_down". Cuando el campo se pausaba, el genérico tomaba de nuevo la misma pulsación y mandaba un A hasta 700 ms después (CLICK_WAIT_MS).
  - Arreglo (raton.c pointer_mode): un solo flanco por lectura (`ledge`) para todos, y el A pendiente del genérico se cancela cuando el campo toma el control.
- **Flechas de la pantalla del nombre con un recuadro.**
  - Causa: uv_clamp se aplicaba a sprites de 4 a 6 vértices y dejaba a la vista el texel del borde.
  - Arreglo: uvc_batch solo con 8 vértices o más (las mitades del globo tienen 23).
- **Nombre con teclado.**
  - Cómo funciona el juego: sub_00110FD0 es el teclado en pantalla (tabla 0x456460, 15 columnas, página +0x144, fila +0x138, columna +0x134). sub_001103D0(letra) agrega (thiscall, ret 4; nombre en +0x158, largo en +0x140, máximo 12) y sub_00110480() borra.
  - Wrapper en raton.c (agregado a PADS en rename_pads):
    - las letras, números, espacio y ñ que se escriben se agregan; Backspace borra; Enter sigue siendo START;
    - mientras se escribe, el mapa de teclas del kit no llega al mando;
    - log: `[RATON] name: typed 0x..`.
- Compilado (rc=0), sin probar.

## 2026-11-08 20:40 — pantalla del nombre: teclado, Enter y mouse quieto
- Botón final del juego (A en el botón 2, 0x11183D):
  - sub_00110050 (nombre no vacío);
  - sonido 0x10000002, sub_00117F70(0x453B8000), sub_0002DB90(obj+0x158);
  - +0x16C = 2, +0x168 = 4.
- Ahora (raton.c):
  - Enter o un clic izquierdo hacen esa misma secuencia desde el wrapper de sub_00110FD0;
  - en esa pantalla el mouse se mueve libre y no mueve el cursor del teclado del juego;
  - START del kit queda afuera en esa pantalla, así Enter no actúa dos veces.
- Log: `[RATON] name: Enter -> finish`, `name: finished`, `name: empty, not finished`.
- Textura de las flechas: sigue rota. Sin datos no hay arreglo: hace falta un Ctrl+F9 en esa pantalla para ver ese dibujo (vértices, uv, textura). Sospecha: el sprite toma texels vecinos del atlas en alta resolución, como pasaba con el globo, pero dibujado en un lote junto con el texto.
- Compilado (rc=0), sin probar.

## 2026-11-08 21:00 — menú del título, botones del nombre, letra marcada, textura de las flechas
- Textura de las flechas.
  - Volcado Ctrl+F9 del usuario (19:25): las flechas son un lote de 11 vértices en x 321..337, y 79..114, que usa u 0..0.986 de una imagen de 1024x512. Es una imagen muy reducida: a 480p no se muestreaba su borde, y a escala n sí, por eso salía un recuadro.
  - Arreglo en nv2a_edge (d3d8_hlsl.h): el margen pasa a ser el mayor entre medio texel y medio píxel de 1x (n = -z-1000).
  - Validado con d3dcompiler_47 (hr=0).
- Menú del título (Nueva partida / Cargar partida).
  - Es sub_0010E8E0 en el estado +0x54 = 10: sub_000EF040 lee la opción y sub_000EEFE0(i) la cambia, con el sonido 0x10000001.
  - Wrapper (en PADS): la mitad donde está el puntero (y ≥ 330) elige la opción, y el clic manda A.
- Pantalla del nombre.
  - Clic en los botones, en coordenadas del juego medidas en la captura: Atrás x 95..240 (página -1), Próximo 250..390 (página +1), Aceptar 405..545 (terminar); y 365..415. Las páginas cambian como lo hace el juego (sonido 0x10000002, +0x144, sub_001104F0).
  - Cada letra que se escribe mueve el cursor del juego a su lugar en la tabla 0x456460 (+0x134 columna, +0x138 fila, cambiando de página si hace falta), así el recuadro verde queda sobre la letra.
- Compilado (rc=0), sin probar.

## 2026-11-08 21:30 — por qué la prueba de 19:33 no mostró cambios
- El usuario corrió la versión nueva: el log tiene `name: click button 1/0` y `typed`. El código se ejecutaba, pero estaba mal apuntado.
- **Menú del título.** sub_0010E8E0 no era el título. El de verdad es sub_001125C0 en el estado +0x4C = 8, con el selector en +0x48: sub_000EF040 lo lee, sub_000EEFE0(i) lo cambia con el sonido 0x10000001, y A (LO8 2) confirma. El wrapper pasó ahí; sub_0010E8E0 volvió a su nombre original.
- **Recuadro verde.** Escribir +0x134 y +0x138 no lo movía. El juego lo mueve con sub_001102A0(col, fila) (thiscall, ret 8; 0x11152F), que también recolorea la letra vieja y la nueva. Ahora se llama a esa función, y solo en el estado +0x168 = 2 (letras; el 3 es la fila de botones).
- **Atrás / Próximo.** Ahora anotan en el log la página antes y después (`name: page a -> b (state s)`) para verificar.
- **Textura de las flechas.** El margen de nv2a_edge no la arregló. El volcado ahora imprime vértice por vértice los lotes de 16 vértices o menos (`[DUMP]   vN pos .. uv ..`). Hace falta otro Ctrl+F9 en esa pantalla.
- Compilado (rc=0), sin probar.

## 2026-11-08 21:50 — el menú del título, ahora en el estado correcto
- Log de 19:38: en el título no apareció ninguna línea `title:`, y los clics fueron al manejo genérico. El estado 8 de sub_001125C0 no es el menú.
- El código del juego muestra cómo es el flujo: la escena (0x1146B0) ejecuta sub_001125C0, y cuando +0x50 = 1 abre la pantalla del nombre.
- El menú Nueva/Cargar es el estado 11 (0x112AA2):
  - sub_00111A10 dibuja el triángulo en x 85 o 340 según +0x70 (0 = Nueva, 1 = Cargar);
  - izquierda/derecha hacen: sonido 0x10000001, +0x70 = i, sub_00111A10 (0x112C46 / 0x112C7B);
  - A o START (0x22) confirma.
- El wrapper ahora hace exactamente eso con el hover; el clic manda A.
- Pantalla del nombre: el log de 19:38 prueba que se ejecutaron sub_001102A0 (recuadro, fila y columna correctas) y el cambio de página 0 -> 3. El usuario no vio cambios; causa sin encontrar.
- Textura de las flechas: sin cambios nuevos; hace falta el volcado por vértice.
- Compilado (rc=0), sin probar.

## ✅ FUNCIONA, CONFIRMADO POR EL USUARIO: NO ROMPER (2026-11-08)
Cualquier cambio en raton.c o en los wrappers tiene que mantener esto tal cual:
- **Menú del título con el mouse** (confirmado a las 19:51: `title: hover -> 0/1`, `title: click`).
  - sub_001125C0 en el estado +0x4C = 11; la opción está en +0x70.
  - Cambio de opción: sonido 0x10000001, +0x70 = i, sub_00111A10.
  - Hover y clic solo sobre cada opción (flecha + texto), no por mitad de pantalla (cambiado el 2026-11-10: antes un clic en cualquier lado de abajo elegía una). El clic manda A.
  - NO usar el estado 8 ni sub_0010E8E0 para el título (eran los equivocados). `sub_0010E8E0` es la página Guardar del menú principal y desde el 2026-11-10 está envuelta para eso.
- **Escribir el nombre con el teclado + recuadro verde en cada letra** (confirmado por el usuario 19:51).
  - Wrapper de sub_00110FD0: la letra se agrega con sub_001103D0 y el recuadro se mueve con sub_001102A0(col, fila), solo en el estado +0x168 = 2.
  - Escribir +0x134/+0x138 a mano NO mueve el recuadro.
  - Tabla de letras 0x456460: página·4 + fila, 15 columnas, 4 páginas.
  - Backspace usa sub_00110480.
  - En esa pantalla el mouse no mueve el cursor del teclado.
  - Mientras se escribe, el mapa de teclas del kit queda afuera.
- **Los 3 botones de la pantalla del nombre (Atrás, Próximo, Aceptar) con el mouse** (confirmado por el usuario a las 22:35, captura).
  - Zona: solo la barra verde, x 122+150·b .. 247+150·b, y 345..400; los íconos no cuentan.
  - Fuera de los botones el mouse no hace nada; a las letras se vuelve solo escribiendo ("arriba" un cuadro, 0x100).
  - Hover: cualquier movimiento sobre un botón lo pide (s_name_hover_seq). Desde las letras: sub_001102A0(col, 3), "abajo" un cuadro (0x200) y sub_00110190(b). Sobre los botones: sub_00110190(b) con el sonido 0x10000001.
  - Clic: A durante un cuadro (name_keys(obj, 2)), el propio juego hace el botón.
- **Flechas verdes junto al nombre sin marco** (confirmado por el usuario a las 22:50).
  - Kit, nv2a_pb_d3d11.c, uvc_groups: una tanda 2D con piezas separadas del atlas se dibuja pieza por pieza, cada una con su propio recorte de medio texel. Las piezas que se tocan siguen juntas.
- **Biblioteca con el mouse y la rueda** (confirmado por el usuario a las 21:00 y 21:40).
  - Puntero → carta: wrapper de sub_0010F790; columna [0x66E68C], fila [0x66E68E], fila de arriba [0x66E690], [0x66DD18] |= 0x288|8; solo dentro de la carta (35×51 en x 76,5+40c, y 94,5+57f); el sonido 0x1000009A una sola vez cuando el puntero queda quieto 90 ms, y solo en cartas propias.
  - Rueda: wrapper de sub_00038220 (escena en ecx): con el estado [0x66E694] en 0, fila de arriba ±1 por cuadro (máximo 4 pendientes), |= 8, sub_000381E0(escena), |= 0x4000; un sonido por giro. La rueda del kit (pulsos de cruceta) se saca en modo puntero (ygo_mouse_apply).
- **Cartel de salir de la Biblioteca (Aceptar/Cancelar) con el mouse** (confirmado por el usuario a las 22:35).
- **Menú de la carta abierta en la Biblioteca (lupa / cámara) con el mouse** (modo 2, [0x66E689]; confirmado por el usuario 9/11).
- **Elegir el objetivo del ataque con el mouse** (wrapper de `sub_00092E20`: la casilla bajo el puntero si el juego la acepta con `sub_00092620`; un clic en una casilla no válida no confirma; confirmado por el usuario 9/11).
- **Sin la velocidad al jugar cartas** (la de Joey): `s_fast_cards` = 0 fijo y sin fila en F2; es el comportamiento por defecto (confirmado por el usuario 9/11).
- **Cuadros de fase y de Sí/No del duelo con el mouse** (widget `[[0x656F88]+0x58]`, posiciones de cada línea en +0x1B0+4k → +0xC/+0x10, barra 450×30; `ask_box`/`ask_pointer`; confirmado por el usuario 9/11).
- **Barra de la Biblioteca que sigue a la rueda** (`sub_000E3510(fila)` sobre `[escena+0x1260]` en el wrapper de `sub_00038220`; confirmado por el usuario 9/11: "quedó de 10").
- **Clic derecho de una vez para terminar turno / atacar** (gancho `s_rpress` + B mínimo 80 ms; en uso desde 9/11, el usuario lo da por bueno en la "mejor versión").
  - Es el modo 4 del manejador de teclas ([0x66E684] = 4); la opción es [0x66E688] (0 Aceptar, 1 Cancelar).
  - En el wrapper de sub_0010F790 se marca la línea bajo el puntero como lo hace la cruceta: opción, [0x66DD18] |= 0x40000 y sonido 0x10000001 por sub_0007E280.
  - Zonas (video 22-15-51, escala 2,803 × 2,102): Aceptar y 231–262, Cancelar 262–294, x 92–548. Clic: marca la opción y aprieta A cuando queda marcada.
- Antes ya confirmados: botones de comando del duelo, flechas de piedra-papel-tijera, rueda del menú principal, la mano sin saltos (sin direcciones en el duelo) y la línea del globo.
- **Confirmados el 2026-11-10:**
  - Pausa con F7 (y F6 para comparar texturas en pausa).
  - `Yu-Gi-Oh.exe` arranca solo leyendo `ygo-apex\env.bat` (`main.c` `env_bat_defaults`). Sin exe intermedio: Malwarebytes marcó dos veces el lanzador chico.
  - Título con el mouse: "Presiona START" (estados 3/6/10 de `sub_001125C0`) y Nueva/Cargar (estado 11) avanzan solo con clic sobre el texto. `mouse_a_off` bloquea el A propio del kit.
  - Opciones (`sub_0010C2E0`: fila al pasar el mouse, flechas por clic), Guardar (`sub_0010E8E0`: clic en la ranura) y la lista del cementerio (`list_pointer`: carta del medio = A, cartas de los costados = paso). "Parece que funciona todo bien."
  - Chispas del título y del menú principal con F6: `texturas_pack.py` `KEEP_PIECES` (menu_06_00/01 sin IA). "Quedó genial."
  - Papel debajo del ATK/DEF de las cartas: `texto_cartas.py` `rellenar_papel` (papel de la misma carta, x 16..54 en espejo), bien en todos los colores. "Quedó genial."
  - La voz se desvanece cuando un clic cierra el globo con el texto completo (wrapper `sub_0002B9D0`, estado `[obj+0x17C0]` = 2): visto en el registro.
  - Confirmado por el usuario ("quedó bien todo"): Target List con el mouse (`sub_000920B0`, `tl_pointer`) y lupa/engranaje de la lista del cementerio (paso 1, `[0x658558]`); boca de los retratos con el volumen de la voz (`sub_00018790`, N.env); F2/Configuración "Voces" con "No"; botón "Buscar actualizaciones"; cara del Mago Oscuro (`KEEP_PIECES` con fundido) y costuras de ojos/boca de los 81 retratos (`mejoradas/caras/`, `costuras_caras.py`).
- Voces y pack de texturas son **opcionales** para el jugador: el juego nunca debe depender de ellos.

## 2026-11-08 21:58 — los 3 botones de abajo, por el camino del propio juego
- Antes (name_page, que imitaba el cambio de página a mano): el log mostraba `page 0 -> 3`, pero el usuario no veía ningún cambio.
- Ahora, al hacer clic en un botón, durante un solo cuadro:
  - la pantalla pasa al estado de botones (+0x168 = 3) con el botón en +0x13C (0 Atrás, 1 Próximo, 2 Aceptar);
  - en la palabra de teclas [0x650424 + pad·0x16C] (sub_00028DE0, pad en [0x5FC1B0]) queda solo A, y [0x650420 + ...] queda en 0;
  - corre el propio update del juego (sub_00110FD0_pad);
  - después se restauran las teclas, y el estado vuelve al de letras (salvo con Aceptar, que avanza).
- Log: `[RATON] name: button b by the game: page a -> b, state x -> y`.
- Compilado (rc=0), sin probar.

## 2026-11-08 22:15 — botones del nombre: el ícono cuenta y se ponen verdes al pasar el mouse
- El usuario confirmó (captura) que Atrás/Próximo/Aceptar funcionan con clic, pero solo sobre el texto, no sobre el ícono, y no se iluminan al pasar el mouse.
- Posición de los botones según el propio juego (sub_00110190): la barra verde del botón b está en x 127+150·b, y 355. Las zonas de clic ahora van de x 90+150·b a 240+150·b, y 345..420, con el ícono incluido. Antes los íconos de Próximo y Aceptar quedaban afuera.
- Hover hecho con los movimientos del propio juego (wrapper de sub_00110FD0, name_keys):
  - de las letras a un botón: el recuadro baja a la última fila (sub_001102A0(col, 3)), se manda "abajo" un cuadro (0x200, 0x1114BA: el juego oculta el recuadro, muestra la barra verde y pasa al estado 3) y después sub_00110190(b);
  - sobre los botones, la barra sigue al puntero: sub_00110190 con el sonido 0x10000001;
  - clic: A durante un cuadro;
  - al escribir, o con el puntero de vuelta sobre las letras: "arriba" un cuadro (0x100, 0x1116C7), y en el cuadro siguiente se agregan las letras como antes.
- No se tocó lo confirmado: escribir, el recuadro verde en cada letra y Enter siguen igual.
- Log: `name: to the buttons, button b`, `name: back to the letters`, `name: button b by the game: page a -> b`.
- Compilado (rc=0), sin probar.

## 2026-11-08 22:30 — botones del nombre: solo la parte verde, el mouse no toca las letras, sin demora
- Pedido del usuario: el ícono no debe contar (queda feo, porque solo se ilumina la barra verde); el mouse solo debe actuar sobre los 3 botones (antes podía tocar la última letra escrita); y al escribir y luego pasar el mouse por un botón había una demora.
- Zona de cada botón: solo la barra verde, desde x 122+150·b hasta 247+150·b, y 345..400. La barra empieza en x 127+150·b (sub_00110190). Los íconos quedan afuera.
- Con el puntero fuera de los botones no pasa nada. Se quitó la vuelta a las letras por hover; ahora solo se vuelve a las letras al escribir.
- Demora: antes el hover solo actuaba cuando el puntero cambiaba de botón, así que después de escribir había que salir y volver a entrar. Ahora cualquier movimiento del puntero sobre un botón lo pide, y la barra vuelve en el mismo cuadro. Si se está escribiendo en ese cuadro, gana la escritura.
- Escribir, el recuadro verde en cada letra y Enter no se tocaron.
- Compilación pendiente: el link falló porque ygo_recomp.exe estaba abierto (el juego corriendo).

## 2026-11-08 22:45 — flechas del nombre: el marco (costuras)
- Volcado Ctrl+F9 del usuario (20:09, flip 1648): las flechas son UNA tanda de 11 vértices con dos cuadrados de 16×16 en x 324.5..340.5, y 79.5..114.5. Salen de un atlas de 1024×512 (off 81200000): flecha 1 en u 992..1008, v 441..457; flecha 2 en u 994..1010, v 496..512. El mapeo es 1:1 texel/píxel a 480p.
- Causa: el recorte de medio texel (uvc_batch, snap_2d 2) se calculaba sobre la tanda entera, abarcando las dos flechas. Al escalar, el filtro bilineal leía los texels del atlas que están fuera de cada flecha y se veían como un marco.
- Arreglo (kit, nv2a_pb_d3d11.c, uvc_groups):
  - los triángulos de una tanda 2D se agrupan por su caja uv; los que se tocan o se superponen quedan juntos, así un panel armado en partes sigue siendo un solo grupo y no aparecen costuras nuevas;
  - las uniones de la tira (área 0 en pantalla) se ignoran;
  - si hay 2 o más grupos, cada uno se dibuja aparte (DrawIndexed por rango) con su propio recorte;
  - si los grupos no siguen el orden de los índices, todo queda como antes (un solo recorte).
- Sin tocar: los sprites de menos de 8 vértices, nv2a_edge y todo lo de raton.c.
- Compilado (rc=0), sin probar.

## 2026-11-08 22:55 — Backspace mantenido sigue borrando
- type_poll (raton.c): con Backspace apretado, a los 400 ms empieza a borrar una letra cada 50 ms, como en Windows. La primera pulsación funciona igual que antes.
- Compilado (rc=0), sin probar.

## 2026-11-08 23:05 — partidas guardadas dentro de la carpeta del port
- Antes las partidas iban a %LOCALAPPDATA%\xboxrecomp\UDATA. Ahí de Yu-Gi-Oh (4b4e001e) solo estaban los archivos del juego (TitleMeta/TitleImage/SaveImage), nunca una partida; la de SWAT sí estaba, así que el guardado del kit funciona.
- Ahora RECOMP_SAVE_DIR apunta a la carpeta del port:
  - run.bat / env.bat: `Yu-Gi-Oh Port\Partidas` (verificado: env.bat da `C:\Portear Juegos Xbox Clasica\Yu-Gi-Oh Port\Partidas`);
  - paquete (main.c, junto al exe): `<carpeta del juego>\Partidas`.
- Ahí van UDATA, TDATA y TitleCache. El kit crea la carpeta si no existe. Las imágenes de partición siguen en %LOCALAPPDATA%.
- Compilado (rc=0), sin probar.

## 2026-11-08 23:20 — líneas de los carteles, retrato antes del duelo, voces
- **Carteles** (volcados Ctrl+F9 de las 20:22):
  - el panel del cartel es UNA tanda de 173 vértices: franjas horizontales de unos 11 px, cada una de su propia banda del atlas 1024×512 (off 81000000);
  - las líneas de la captura del usuario están cada ~11,5 px del juego, una en cada unión de franjas;
  - la tanda tenía más de 64 vértices, así que no recibía ningún recorte de medio texel.
- **Arreglo** (kit, uvc_groups):
  - las piezas ahora se separan por sus vértices: un triángulo que comparte arista (2 índices) con el anterior es de la misma pieza, y si no la comparte empieza otra;
  - cada pieza lleva su propio recorte;
  - el límite pasó a 512 vértices y 64 piezas (si hay más, todo queda como antes);
  - el recorte único para la tanda entera sigue igual hasta 64 vértices;
  - las flechas del nombre siguen siendo dos piezas, igual que antes;
  - el volcado ahora muestra los vértices de tandas de hasta 200.
- **Retrato** (Kaiba cerrando los ojos): son dos cuadrados de pantalla completa (5 vértices cada uno) de una imagen de 320×240 en el atlas 81200000, u 0..320 y 322..642, v 272..512, mezclados para el parpadeo. Tienen menos de 8 vértices, así que ningún recorte mío los toca. Falta ver las líneas: pedir un video corto o una captura durante el parpadeo.
- **Voces**: sound\sd_list.lst lista solo SE\Static.xau (efectos, 2,6 MB) y 39 BGM\*.xau (música). El juego no trae ningún archivo de voz, y el log solo abre música. No hay voces que arreglar.
- Compilado (rc=0), sin probar.

## 2026-11-08 23:40 — líneas en el parpadeo de Kaiba (video 20-30-00)
- Visto en el video: mirado entero (contacto a 4 fps), y la cara recortada a resolución nativa con diferencias cuadro a cuadro. El parpadeo está en 8,9–9,1 s. En los cuadros con los ojos cerrados se ven dos líneas finas horizontales, en el borde de arriba y en el de abajo del parche de los ojos (a lo ancho del parche). Con los ojos abiertos no aparecen. En 12,9–13,7 s está la transición al duelo, sin líneas.
- Causa: el parche es un sprite suelto (4 a 7 vértices) agrandado 2× desde un atlas, y el recorte de medio texel solo se aplicaba a tandas de 8 vértices o más. El filtro bilineal leía las filas del atlas pegadas al parche.
- Arreglo (kit, uvc_batch):
  - un sprite 2D plano (two_d && !vary_z) de 4 a 7 vértices también recibe el recorte, pero solo si se muestra a su tamaño o más grande en los dos ejes;
  - los sprites achicados (el caso 16×35 de las 19:25) quedan como estaban;
  - la caja uv sale solo de los triángulos con área, porque la unión de la tira trae uv 0,0 basura.
- Compilado (rc=0), sin probar. Pendiente de confirmar: carteles (franjas por pieza) y parpadeo.
- 20:34: el usuario probó con el build nuevo y siguen las 2 líneas arriba y abajo de los ojos. El recorte de sprites no era la causa, o el parche no es un sprite chico. El volcado de las 20:22 no se tomó durante el parpadeo: solo mostraba los dos cuadrados de pantalla completa (u 0..320 y 322..642).
- Se agregó el registro `[SPR]` (spr_log, kit): cada tanda 2D nueva de hasta 12 vértices se anota una sola vez, con flip, posición, uv en texels, textura (off, tamaño), filtro y el recorte aplicado. Máximo 400 por corrida. Siguiente paso: que el usuario llegue al retrato de Kaiba, y ver en el log qué pieza dibuja los ojos cerrados.
- 20:36: la corrida con `[SPR]` no sirvió. El tope de 400 se llenó en el flip 971 con los menús animados, porque deduplicaba por posición, y el retrato llegó después. Ahora `[SPR]` deduplica por textura + caja de texels, ignora las tandas sin triángulos con área, y se agregó `[TEXUP]` (subidas de texturas de 256 o más de ancho, con flip y hash, máximo 600) para ver si el juego reescribe la textura del retrato durante el parpadeo.
- 20:38 ([SPR]/[TEXUP], datos reales): el retrato de Kaiba es la textura 512×512 en 81929000, dibujada 1:1 (pos 144.5..656.5). Los ojos cerrados son una pieza de 128×32 (texels 0..128 × 72..104; los cuadros del parpadeo van apilados cada 32 filas), en pos 193.5..321.5 × 181.5..213.5, dentro de una tanda de 11 vértices (vary_z 1) con solo 2 triángulos reales. El recorte se calculaba con los 11 vértices, y los de unión traen uv 0,0, así que quedaba en v 0.001..0.202 (filas 0..104) y no protegía las filas 71 y 104, que son los cuadros vecinos. Esas son las 2 líneas.
- Arreglo: en uvc_batch, la caja uv sale ahora solo de los triángulos con área, en todas las tandas (antes solo en las de menos de 8 vértices). La prueba de "sprite achicado" sigue aplicándose solo a las de menos de 8.
- Compilado (rc=0), sin probar. Los registros [SPR] y [TEXUP] quedan (con tope; sirven para la próxima).

## 2026-11-08 23:59 — Biblioteca: ruido carta por carta (video 20-40-58)
- Visto en el video (19 s; contacto + envolvente de audio): en la Biblioteca, al hacer clic en una carta, el guante del juego recorre carta por carta hasta la elegida, con el sonido del cursor en cada paso (ráfagas continuas de 7 a 18 s). La causa es la caminata genérica del puntero (pointer_mode: d-pad paso a paso, ~125 ms por paso).
- Arreglo inmediato: wrapper de sub_0011A400 (el sonido de menú; PADS). El tick del cursor (0x10000001) se silencia en los pasos de la caminata que siguen a otro paso de menos de 400 ms (s_walk_last / s_mute_until): solo suena el primero.
- Para que vaya directo hace falta la grilla de la Biblioteca. Candidato: sub_000D9A10 (manejo de teclas, 16 ticks; globales 0x66DB74/76/78, lista 0x66DB7A), sub_000D97F0 (update, vtable). No está confirmado: el log `[RATON] tick from <ret> (66DB74 .. 66DB76 .. 66DB78 ..)` (máximo 120) dirá quién toca el tick en la Biblioteca y qué variable cambia.
- Compilado (rc=0), sin probar.
- 20:47 (log del usuario): en la Biblioteca no hubo ningún tick por sub_0011A400 (solo del título y la rueda), así que ese silencio no actuaba, y sigue el ruido. El guante seguido quedó en 310,216 con "moves 1" pese a los clics en 176,183: la caminata no está viendo bien el guante de esa pantalla.
- Ahora: el silencio está en sub_001197F0 (entrada de comandos del motor de sonido, cmd 0x100/0x101 = reproducir; PADS), para todo sonido dentro de la ventana de un paso intermedio. Registros nuevos: `[RATON] sound cmd .. id .. from ..` (cada par llamador/id una vez) y `[RATON] move-to from ..` (quién mueve cada sprite, marcado "the cursor glove" si es el seguido). Con eso se busca el código de la grilla. También pedido: cada carta tiene que responder dentro de su contorno.
- Compilado (rc=0), sin probar.
- 20:51 (log): los sonidos de la Biblioteca salen de sub_0010F790 (llamador 0x10FE9D; ids 1000009A mover, 10000004, 10000003), por sub_0007E280, no por sub_0011A400. sub_0010F790 es el manejo de teclas de la Biblioteca (switch sobre la tecla en [jugador+0x1330], 0x10 = ninguna). En el modo grilla ([0x66E684] = 0):
  - columna [0x66E68C] 0..9 (10 columnas, da la vuelta);
  - fila mostrada [0x66E68E] 0..4 (5 filas); en la 4, "abajo" desplaza la fila de arriba [0x66E690] (hasta 0x61);
  - carta = lista 0x66DD20[(top+fila)*10 + col];
  - al moverse: [0x66DD18] |= 0x288 y |= 8, y sonido 0x1000009A.
- Salto directo (wrapper de sub_0010F790, PADS): el puntero escribe col/fila de la carta que tiene debajo, ORea las banderas y toca el sonido una sola vez. Zona: solo dentro del contorno de la carta, medido en el cuadro del video (imagen 1794×1009 desde x 63): centros x 94.6+40·col, y 120.4+57.1·fila, mitad 17 × 24.5 unidades del juego. En los huecos no cambia nada. Clic: A cuando el cursor ya está en esa carta. Clic derecho: B. Rueda: arriba/abajo (desplaza).
- Log: `library: key handler runs`, `library: cursor to col c row r`, `library: click card`.
- Compilado (rc=0), sin probar.

## 2026-11-08 21:00 — Biblioteca: confirmado que funciona; rueda, sonido, revisión del audio
- El usuario confirmó que el salto directo a la carta funciona muy bien. Pidió: la rueda para recorrer todo el catálogo; menos sonido al pasar por las cartas; y revisar que no se haya roto nada del audio.
- Pasar por encima: ya no suena (el wrapper no llama a sub_0007E280). El clic usa el sonido propio de A.
- Rueda: cada muesca desplaza una fila, igual que el desplazamiento del propio juego:
  - abajo (0x10FA47): fila de arriba [0x66E690] + 1, hasta 0x61;
  - arriba (0x10F8A5): fila de arriba − 1, y [0x66E698] |= 0x4000;
  - en los dos casos: [0x66E694] = 7, [0x66E69C] = 1, [0x66DD18] |= 8;
  - una por cuadro, mientras el 7 anterior ya fue tomado (o pasaron 150 ms), y sin sonido;
  - el cursor se queda en su fila de pantalla.
- Audio revisado: lo único tocado es el registro de sub_0011A400 (no cambia nada) y sub_001197F0. El silencio de esta última ahora aplica solo a los ids de cursor (0x10000001, 0x10000004, 0x1000009A) y solo dentro de la ventana de un paso intermedio de la caminata del puntero. Los demás sonidos pasan siempre.
- Compilado (rc=0), sin probar.
- 21:10 (usuario): tiene que sonar una vez al elegir la carta. A veces el sonido quedaba en bucle al bajar y subir, y bajando y subiendo con la rueda y dejando el mouse sobre una carta, la carta se daba vuelta ~10 veces. Causa: las muescas se encolaban y se aplicaban de a una (una por refresco, esperando que se tomara el 7), mucho después de soltar la rueda. Ahora lo pendiente se aplica en un solo salto (s_lib_scroll se toma entero, limitado a 0..0x61), sin cola. Al elegir una carta con el puntero vuelve a sonar 0x1000009A, una vez por carta nueva. Compilado (rc=0), sin probar.
- 21:20 (usuario): el sonido se repetía muchas veces con el mouse quieto sobre una carta, y la rueda se sentía "ultra trabada" (antes, con la cruceta, se sentía libre). Causa: el puntero pedía su carta en cada cuadro, y al desplazar la lista el juego vuelve a acomodar el cursor, así que se peleaban (sonido repetido, carta dándose vuelta). Ahora:
  - el puntero pide la carta solo cuando el mouse se mueve o cambia la carta que tiene debajo;
  - la rueda volvió a ser cruceta arriba/abajo (el propio juego mueve y desplaza);
  - el código de desplazamiento propio queda sin uso (s_lib_scroll siempre en 0).
- Compilado (rc=0), sin probar.
- 21:25 (usuario: "se escucha muchas veces y al bajar va ultra lento"). Log de las 21:06: "cursor to col 5 row 3" repetido mientras subía la fila de arriba. La rueda (cruceta) bajaba el cursor a la fila 4, el puntero (la mano mueve un poco el mouse al girar la rueda) lo devolvía a la fila 3, y así cada muesca costaba dos movimientos con su sonido y la lista apenas avanzaba. Ahora:
  - cada muesca pone primero el cursor, en silencio, en la fila del borde (4 abajo, 0 arriba; wrapper, s_lib_edge, válido 60 ms), así la cruceta del propio juego desplaza la lista con un solo sonido;
  - el puntero no pide carta durante 500 ms después de la rueda;
  - la rueda usa su propia pulsación (34 ms apretado, 25 ms suelto), y se encolan como máximo 3 muescas.
- Compilado (rc=0), sin probar.
- 21:12: el usuario dio por bueno el avance de la Biblioteca y pidió guardarlo. Respaldo en `respaldos/2026-11-08_2112_biblioteca/`:
  - raton.c, main.c, env.bat, rename_pads.py y ESTADO;
  - el exe compilado;
  - nv2a_pb_d3d11.c y d3d8_hlsl.h del kit, más `git diff` del kit (xboxrecomp-apex.diff).
  La carpeta del port no es un repo git; el kit sí, pero no se hizo commit.
- Sigue apareciendo el sonido sin fin, y la rueda tiene que ser más rápida y suave. Ahora:
  - 2 filas por muesca, con un máximo de 6 filas pendientes;
  - solo la primera fila de un giro suena (las siguientes se silencian con s_mute_until, si la muesca anterior fue hace menos de 400 ms);
  - registro `[RATON] tick <id> at <ms> from <llamador> lib mode/col/row/top` de cada sonido de cursor (máximo 600), para encontrar el sonido sin fin si sigue.
- Compilado (rc=0), sin probar.
- 21:16 (usuario: hay un retraso entre girar la rueda y que avance; y el sonido de ir pasando por las filas hasta llegar a la carta sigue). Log de las 21:14 con `[RATON] tick`:
  - al mover el puntero hacia una carta se suceden "cursor to col 7, 5, 3, 2..." con un tick cada ~80 ms: suena una vez por cada carta que el puntero cruza;
  - el juego desplaza una fila cada 110–125 ms (su propia animación), así que 6 filas en cola seguían corriendo ~0,7 s después de soltar la rueda.
- Ahora:
  - el puntero mueve el cursor en silencio, y el sonido 0x1000009A suena una sola vez cuando el puntero lleva 90 ms quieto en su carta nueva (s_lib_tick_at, en el wrapper);
  - la cola de la rueda queda en 3 filas como máximo (sigue siendo 2 por muesca).
- Para que el desplazamiento vaya más rápido que la animación del juego habría que saltarse esa animación ([0x66E694] = 7 se lee por puntero; no se encontró dónde).
- Compilado (rc=0), sin probar.

## 2026-11-08 21:30 — Biblioteca: rueda instantánea, sonido solo en cartas propias
- Respaldo antes del cambio: `respaldos/2026-11-08_2120_biblioteca_sonido/` (pedido del usuario: "guarda este cambio").
- Investigación de la rueda:
  - el estado de la escena está en [0x66DD18 + 0x97C] = [0x66E694], y lo lee sub_00038220 (máquina de estados, thiscall con la escena en ecx; sub_0010F490 devuelve la base 0x66DD18);
  - estado 7 (0x38398): espera la transición de cada fila (sub_000353D0, objeto +0x139C) y después sub_000381E0. Por eso cada fila tardaba ~120 ms;
  - estado 8 (0x383B3): la barra de desplazamiento; pone la fila de arriba (+0x978 = 0x66E690), |= 8, y llama sub_000381E0, que redibuja al instante (estado → 0; limpia +0x980/984/988; sub_00037780 + sub_00035220).
- Rueda nueva: wrapper de sub_00038220 (PADS). Con el estado en 0 y modo grilla:
  - fila de arriba ± filas pendientes (1 por muesca, entre 0 y 0x61), [0x66DD18] |= 8, sub_000381E0(escena), y después [0x66E698] |= 0x4000 (la vista previa sigue a la carta nueva);
  - un sonido por giro (la primera muesca después de 400 ms sin rueda);
  - sin cola: se aplica todo lo pendiente;
  - el puntero sigue en pausa 500 ms después de la rueda;
  - se quitó la rueda por cruceta (fila del borde) y el desplazamiento por estado 7.
- Sonido al pasar sobre una carta: solo si la carta es del jugador. Es la misma prueba que hace A (0x10FB5E): id = 0x66DD20[(top+fila)*10+col], cantidad = [0x651E70 + id*2 + 0x3DA] ≠ 0.
- Pendiente: el título "Biblioteca" se deforma (la B) cuando el cursor no tiene carta (captura del usuario). Hace falta un Ctrl+F9 en ese estado.
- Compilado (rc=0), sin probar.

## 2026-11-08 21:35 — Biblioteca: rueda sin "infinito" ni frenazo; medidas exactas; la B del título
- Video 21-25-51 (26 s, contacto a 3 fps): la rueda baja la lista con fluidez, pero se detiene de golpe, y con giros fuertes sigue bajando sola.
- Causa (log de las 21:26): aparecen `[KEY] wheel -1` del kit. En modo puntero el kit también toma la rueda (captura NEVER) y la convierte en pulsos de cruceta de 100 ms por muesca, desde su propia cola (main.c ata la rueda a arriba/abajo). Esos pulsos se sumaban al desplazamiento de raton (la lista corría sola con la animación lenta del juego, y raton saltaba de golpe). En el log, los saltos de "wheel top" de a 2 son la cruceta del kit metida en el medio.
- Arreglo:
  - ygo_mouse_apply en modo puntero saca DPAD_UP/DOWN mientras xbox_InputMouseHeld(WHEEL_UP/DOWN) (raton ya maneja la rueda con su hook). Vale para todas las pantallas en modo puntero: antes la rueda daba doble paso;
  - el wrapper de sub_00038220 aplica una fila por cuadro, con un máximo de 4 pendientes, así un giro rápido se desliza y se detiene enseguida.
- Medidas exactas de las cartas, del volcado Ctrl+F9 (quads del juego): 35×51 en x 76,5 + 40·col, y 94,5 + 57·fila (centros 94 + 40c, 120 + 57f; mitad 17,5 × 25,5). Antes estaban medidas del video, con menos de 1 px de diferencia.
- La B de "Biblioteca": es un sprite suelto de 5 vértices, pos 128.5..158.5 × 44.5..70.5, sacado de la página de letras 512×512 en 80B88000, texels 90..120 × 434..460, 1:1, así que recibe el recorte de sprites (uv_clamp[0], solo etapa 0). El recorte de medio texel no explica la deformación. Para comparar falta un Ctrl+F9 con la B bien.
- Compilado (rc=0), sin probar.
- 21:40: el usuario: "la rueda quedó increíble, guardá esto". Respaldo: `respaldos/2026-11-08_2140_rueda/`. Rueda CONFIRMADA (ver la sección NO ROMPER).
- Clic mientras se gira la rueda: tardaba ~0,5 s (la pausa del puntero después de la rueda también retenía el clic). Ahora el clic toma su carta en el acto: pone s_lib_want, borra la pausa y descarta las filas pendientes.
- La B: los dos volcados (21:33, flips 1644 bien y 2020 mal) tienen el quad de la B, el texto de 89 vértices y la textura 80B88000 idénticos, vértice por vértice, y sin re-subidas. Lo que cambia es la barra de información de abajo: en el estado malo sus iconos (80C08000, 80ECC000) se dibujan sin posición en la entrada 0 (salen 0,0), así que no se sabe dónde caen; la sospecha es que caen sobre la B. Se agregó la sonda de píxel al Ctrl+F9 (sin RECOMP_D3D11_PX: píxeles del juego 135,57 y 141,50 en el cuadro del volcado). Anota cada tanda que pinta esos píxeles, con color antes/después, texturas y entradas (`[D3D11-PX]`).
- Compilado (rc=0), sin probar.
- 21:50 PRUEBA "saltar filas" (el usuario decide si queda; si no, volver a `respaldos/2026-11-08_2150_antes_de_saltar_filas/raton.c`). Log de las 21:38: 380 imágenes de cartas distintas cargadas teniendo 43 cartas. El juego carga la imagen de cada lugar que se muestra, una por una (D: y copia a Z: la primera vez), y el clic espera detrás de esa fila. Así que con todas las cartas sería igual. Prueba:
  - con muescas a menos de 100 ms entre sí, cada muesca baja 3 filas;
  - con 3 o más filas pendientes, la grilla se redibuja cada 80 ms con todo lo acumulado (hasta 30), así que las filas que se saltan nunca se muestran ni cargan;
  - girando despacio sigue igual: una fila por muesca, una por cuadro.
- La B: el usuario dice que solo pasa con el mouse en una zona vacía (fuera de las cartas). Ahí el juego no cambia nada, y eso explica que los volcados salgan iguales. Sospechoso: algo del lado del port (el guante del puntero o similar). Va a grabar 5 s.
- Compilado (rc=0), sin probar.
- 21:55: la prueba de "saltar filas" se DESCARTÓ a pedido del usuario. raton.c volvió a `respaldos/2026-11-08_2150_antes_de_saltar_filas/raton.c`: la rueda queda como estaba (una fila por cuadro).
- Video 21-48-27 (24 s, contacto a 4 fps más el tramo 0,8–5,3 s a 6 fps):
  - el mouse sobre "Receta de hamburguesa" (col 0, fila 1) muestra la vista previa, y al bajar al hueco de abajo vuelve el dorso (correcto);
  - lo que encontró el usuario: después de girar la rueda, con el mouse quieto sobre una carta, la vista previa no aparece hasta mover el mouse. El log muestra que la columna y fila del cursor no cambian con la rueda (col 4 fila 2 con top 64→65): cambia la carta debajo del mismo lugar, pero el juego no lo toma como movimiento del cursor.
- Arreglo: 150 ms después de la última muesca, el puntero vuelve a pedir su lugar (o el del cursor, si está fuera de las cartas) como un movimiento (s_lib_force): [0x66DD18] |= 0x288|8, [0x66E698] |= 0x4000, y el sonido si la carta es propia. La pausa del puntero después de la rueda bajó de 500 a 150 ms.
- Compilado (rc=0), sin probar.
- 22:00 la B: en el video 21-48-27 medí el brillo de la B (x 420..445, y 95..140 del video) y del centro de la vista previa a 5 fps durante los 24 s. La B oscura (82) coincide en todos los cuadros con el dorso en la vista previa (8,5), y la B normal (111) con una carta mostrada (40–106). O sea: la B se deforma cuando no hay carta en la vista previa. El quad de la B es el mismo en los dos estados (volcados de las 21:33), así que debe cambiar lo que muestrea: la página de letras 512×512 en 80B88000 (el registro [TEXUP] tenía tope de 600 y se agotó con las cartas, no sirve para esto). Se agregó `[FONTPAGE]`: cada contenido nuevo de 80B88000 se guarda en `ygo-apex/runs/font_NN_<hash>.png` (máximo 12).
- Compilado (rc=0), sin probar.
- 22:05 LA B, CAUSA ENCONTRADA:
  - `[FONTPAGE]` muestra un solo contenido de 80B88000 en toda la sesión (la página no cambia). En esa página, la B ocupa la celda que usa el quad (u 90..120, v 434..460);
  - zoom del video: en el estado malo, la mitad izquierda de la B es un bloque sólido, o sea una columna repetida (el recorte corre el borde izquierdo hacia adentro);
  - causa: uvc_batch lee el tamaño de textura de s_stage, pero build_stages() de este dibujo recién corre después, en setup_pixel. Se usaba el tamaño de la textura del dibujo anterior. Sin carta en la vista previa, antes de la B se dibuja un ícono de 64×64 (80ECC000), y el medio texel salía 0,5/64 en vez de 0,5/512: margen 8 veces mayor (≈4 texels), y la izquierda de la B quedaba aplastada contra su trazo.
- Arreglo: build_stages() al principio de uvc_batch (está memorizada, así que la segunda llamada en setup_pixel no cuesta). Esto también corrige todos los recortes anteriores (flechas, globo, carteles, parpadeo), que dependían de qué se había dibujado justo antes.
- Queda: s_stage_rt también se arma en setup_pixel, así que en uvc_batch puede venir del dibujo anterior (como mucho, un recorte de más o de menos en una textura de render target).
- Compilado (rc=0), sin probar.

## 2026-11-08 22:20 — cartel Aceptar/Cancelar: la marca no sigue al puntero
- Causa (código): la salida de la Biblioteca pone la escena en estado 4 (sub_0010F790 caso 0x10FE2D), y ese estado abre el cartel con sub_00035090 sobre el objeto [0x66DCD8] (abierto mientras su +0x10 vale 1; sub_00035110). En el estado 4 el manejador de teclas de la Biblioteca (sub_0010F790, llamado solo en el estado 0 vía sub_0010FF20) no corre, así que el bloque del puntero de la Biblioteca se apaga y queda el paseo genérico. Ese paseo busca la mano del juego, y este cartel no tiene mano: aprieta flechas a ciegas, por eso "a veces sí, a veces no".
- Falta saber qué campo guarda la opción marcada: el cartel usa un componente genérico ([obj+8], interno en +0xB0) y no quedó claro leyendo el código.
- Diagnóstico agregado (solo registra): dlg_diag en raton.c, líneas [DLG] en last_err.txt con cada dword que cambia en los tres objetos mientras el cartel está abierto, las teclas y los clics. Compilado (rc=0).
- 22:10: la primera prueba no registró nada. dlg_ok solo aceptaba direcciones < 64 MB, y los objetos del juego viven arriba (0x74B5xxxx). Ahora pregunta a la página del host (VirtualQuery). Además se registra [0x66DCD8] y su +0x10 cada vez que la escena está en el estado 4. Compilado (rc=0).
- 22:25 CORRECCIÓN y arreglo, sin segunda prueba. En el registro de la prueba anterior, la flecha arriba con el cartel abierto tocó el sonido 0x10000001 desde 0x10FE9D con "lib mode 4". El cartel NO es el objeto [0x66DCD8] ni el estado 4 de la escena: es el modo 4 del manejador de teclas de la Biblioteca ([0x66E684] = 4). La opción marcada es [0x66E688] (0 Aceptar, 1 Cancelar); al abrirse vale 1. Arriba/abajo la invierten (0x10F839 / 0x10F9CC), piden redibujarla con [0x66DD18] |= 0x40000 y tocan 0x10000001. A (0x10FE2D): con 0 sale (estado 4), con 1 cierra el cartel.
- Por qué fallaba: el bloque del puntero de la Biblioteca seguía buscando cartas detrás del cartel; si el clic caía sobre la carta del cursor, apretaba A sobre lo marcado (Cancelar).
- Arreglo (raton.c): en el modo 4, la línea bajo el puntero se marca en el wrapper de sub_0010F790 igual que con el d-pad (s_dlg_want). Un clic la marca y aprieta A cuando queda marcada; el clic derecho es B; la rueda no hace nada. Zonas medidas sobre la captura del usuario (grilla de fondo, escala 2,76): Aceptar y 198–231, Cancelar 231–265, x 85–556 (coordenadas de juego). Diagnóstico dlg_diag quitado. Compilado (rc=0), sin probar.
- 22:30 (video 22-15-51): funciona, pero las zonas quedaban unos 24 px de juego más arriba. La imagen completa del video mide 1794 × 1009 desde x 63: escala 2,803 en horizontal y 2,102 en vertical (estirada). La captura de las 22:25 estaba recortada y usé una sola escala para los dos ejes. Barras verdes reales: Aceptar y 233–262, Cancelar 262–292, x 95–545. Zonas nuevas: Aceptar 231–262, Cancelar 262–294, x 92–548. Compilado (rc=0).

## 2026-11-08 22:45 — texturas mejoradas: prueba chica (preparado)
- El usuario confirmó el cartel de salir (anotado en NO ROMPER) y pidió escalar texturas como en Red Dead (F6 original/mejorada).
- Lo que ya había (heredado de rdr-port y del kit):
  - la carga del pack en main.c (RECOMP_TEXTURES_PACK);
  - F6 en vivo (nv2a_pb_d3d11.c wnd_proc, solo si hay pack);
  - la fila de F2 "Volcar texturas a carpeta" (host_menu.c), que guarda HASH_ANCHOxALTO.png en modding\texturas\dump;
  - Upscayl instalado.
  Faltaba todo el contenido: no existía la carpeta modding.
- Nuevo:
  - `tools/escalar_texturas.py`: Upscayl x4, digital-art-4x. Deja afuera las de ≤32 px y las de `modding/texturas/no_escalar.txt` (para las páginas de letras).
  - `tools/texturas_pack.py`: la parte general de la de Red Dead, sin sus letras ni botones. XRTP v2 sin comprimir, mipmaps LANCZOS, BGRA, alfa restaurado del volcado. Probado con una imagen de prueba: encabezado, índice, 9 niveles y bytes BGRA correctos.
  - `modding/escalar_texturas.bat` y `modding/crear_pack_texturas.bat`.
- env.bat: RECOMP_TEXTURES_PACK apunta a `modding\texturas\textures.pack` (antes `textures_bc3.pack`, que no existe). Sin BC3 por ahora; si la prueba gusta, se copia bc3pack de rdr-port.
- Paso del usuario: F2 > Volcar texturas = Sí, recorrer el menú principal y la Biblioteca (y ver unas cartas), cerrar el juego. Después escalo yo, armo el pack y el usuario compara con F6.
- Ojo: el recorte uv (uvc_batch) usa el tamaño de la textura del invitado; con la textura mejorada el host la sube más grande. Revisar en la prueba que las flechas, los carteles y el título no vuelvan a mostrar líneas.
- 22:30: el usuario volcó 40 texturas (menú, Biblioteca, cartas). El volcado ya saltea las que existen en dump (GetFileAttributes en tex_dump), así que cada vuelta agrega solo las nuevas; el escalado saltea las ya hechas.
  - Fuera del escalado (no_escalar.txt): 3 páginas de letras de 512×512 (9BF7092A495E4E38, F867714BEE53B873, FED7C5881DA64D4A) y las 2 hojas de miniaturas de la grilla (A88B56…, FB7CBF…), que la IA vuelve manchas.
  - Escaladas 36 con digital-art-4x (0 fallos). Las cartas grandes y los retratos mejoran; el nombre de la carta queda nítido.
  - textures.pack: 34 texturas, 515 MB, sin comprimir. Sin probar en el juego (F6).
- 22:40 (capturas del usuario, F6 sí/no en el menú principal): con el pack desaparecen las chispas de las antorchas. Causa: la textura 553E66B4DD668253 (64×64, 36 puntos de brillo 128 sobre negro) salió de digital-art-4x toda negra (brillo máximo 0): el modelo tomó los puntos por ruido. Es la única: las demás conservan el brillo (comparación escalada vs. original).
  - Arreglo: 553E66 va a no_escalar.txt (quedan las chispas originales). texturas_pack.py deja afuera, con aviso, toda escalada cuyo brillo máximo cae a menos de la mitad del original. Pack rehecho: 33 texturas. Sin probar.
- 23:10 nuevo volcado (85 texturas, 45 nuevas: 37 cartas grandes 128x256 y 8 hojas grandes). Escaladas con digital-art-4x. Revisión lado a lado: las cartas quedan bien (más nítidas, el texto igual). 7 hojas nuevas (1AA3A6, 764B56, B80D9E, C8DDD6, D2DC48, D70D2D, E222F3) son grillas de miniaturas de cartas como A88B56/FB7CBF: van a no_escalar.txt y sus escaladas a descartadas/. Pack rehecho: 71 texturas, 656 MB. Sin probar.
- 23:40 descripción nítida en las cartas grandes (tools/texto_cartas.py, modding/texto_cartas.bat): cada carta volcada 128x256 se identifica por igualdad exacta de píxeles con euro/sp/card/normal/*/ID.tga (el volcado está invertido en vertical). Textos en default.xbe: la lista de nombres empieza en " " + "Dragón blanco azul - ojos" (1139 entradas, índice k), las descripciones siguen en 1139+k; la tabla uint16 en el archivo 0x5EECB2 da el ID de imagen de cada k (el ID NO es 4006+k: 4422 es Blocker, k=403). %cNNNN = nombre de la carta con imagen NNNN. Recuadro (px originales): monstruo x12..116 y148..167; mágica/trampa x12..116 y142..173 (monstruo = raya de ATK/DEF en y168). Se tapa con el color del papel y se escribe con Calibri (máx. 24 px en la escalada x4). Salida en mejoradas/texto_cartas/, que texturas_pack.py prefiere. 43 cartas, pack 71 texturas (verificado: la entrada del pack es la versión con texto). Sin probar en el juego. Pendiente: la línea de tipo [GUERRERO BESTIA], ATK/DEF y el nombre siguen siendo los de la IA (algo deformados).
- 00:20 (usuario: la descripción se lee bien; el nombre y el tipo deformados por la IA no) texto_cartas.py ahora reescribe también nombre (x12..101 y11..23, Arial Bold), tipo de monstruo [TIPO/EFECTO|FUSIÓN|RITUAL] (y142..147), ATK/DEF (x56..113 y169..173) y la etiqueta de mágica/trampa (x30..116 y26..34). Datos: uint32 por índice k en el archivo 0x5EF598: DEF/10 bits 0-8, ATK/10 bits 9-17, clase bits 18-19 (monstruo 0 normal 1 efecto 2 fusión 3 ritual), tipo bits 20-24 = índice en la lista de strings "Monstruo, Dragón, Zombi..." (21 trampa, 22 mágica), nivel bits 25-28. Mágicas: clase 1 mezcla equipo y campo (campo si la descripción dice "todos los monstruos"/"considerado como" o es Puerta de fusión), clase 2 mezcla continua/rápida/normal (va solo [CARTA MÁGICA]), 3 ritual; trampa 2 = continua. Relleno: cada renglón toma la mediana de su mitad clara (tomar el borde dejaba manchas oscuras). Pack 71. Sin probar en el juego.
- 00:50 (usuario: "se ven increíbles", seguir con todas las cartas) tools/cartas_volcado.py: la huella del port se reproduce sin el juego. Carta = P8 128x256 sin mipmaps; índices = .tga invertido en vertical + swizzle Morton (x primero), paleta 256 x BGRA con A=255; huella = hash(índices) ^ hash(paleta)*31 (nv2a_tex_hash). Coincide con las 43 volcadas por el juego. Escribe en dump/ las 1146 restantes. BC3: tools/bc3/bc3pack.exe (copiado de rdr-port) comprime el pack de 71 de 0,64 a 0,16 GB y el texto de las cartas queda nítido (decodificado y comparado). Escalado de las 1146 en curso.
- 00:30 (9/11) todas las cartas: 1191 escaladas (00:16), texto_cartas.py sobre todas: 1137 con texto nuevo; 52 imágenes (5020, 5162, 5303-5530...) no están en la lista de cartas del juego (sin datos): quedan escaladas sin texto. Mejora: nombres y descripciones largas se angostan (capa con resize horizontal, mínimo 0,6 en nombres; 0,88/0,78 en descripción solo si la letra gana 3 px o más). Pack: 1217 texturas, 3,7 GB -> textures_bc3.pack 0,91 GB (bc3pack). env.bat ahora carga textures_bc3.pack; crear_pack_texturas.bat arma y comprime. Sin probar en el juego.
- 01:00 (usuario: la vista de miniaturas del tablero/baúl se ve mal) tools/miniaturas.py: las 10 hojas card/mini/texdb/texdbNNN.png (P8 512x512, ~126 cartas) se rearman a 2048 con las cartas grandes mejoradas. texdbNNN.txt: "ID.png 35 51 u0 v0 u1 v1 girada" (v desde abajo; girada = carta acostada, arriba hacia la derecha). Huella de la hoja = como las cartas pero el color 0 de la paleta va en 0,0,0,0 (transparente): coincide con las 9 volcadas; texdb002 = 0AF5D8BCA9098160 (calculada). Salida en mejoradas/miniaturas/, opaca (texturas_pack.py no le aplica restore: el color 0 deja agujeros en el dibujo). Arreglo: etiqueta CAMPO solo si la descripción empieza con "Aumenta" y dice "todos los monstruos" (Anillo del magnetismo y Hoja de relámpago son de equipo). Pack 1227 texturas, BC3 0,96 GB. Sin probar en el juego.
- 01:20 reverso nuevo (imagen del usuario, reverso japonés, en modding/texturas/reverso/reverso_fuente.png): tools/reverso.py. Textura card/normal/card_ura.tga (P8 128x256, huella E86E1EE8D605D332, la carta ocupa 128x185). Remolino con Upscayl x4; el logo de kanji sin IA (LANCZOS de la fuente); KONAMI® redibujado con Arial Black (antes se tapan las letras viejas con el oscuro de alrededor, si no queda halo); los dos renglones japoneses con Yu Gothic Bold sobre franjas negras (el renglón 2 empieza en x 248 de la fuente). Salida mejoradas/reverso/; texturas_pack.py da prioridad a texto_cartas, reverso y miniaturas. Pack 1227, BC3 0,96 GB, verificado que la entrada del pack es la nueva. (Un corte de luz interrumpió el primer armado; se rehízo.) Sin probar en el juego. Pendiente: el usuario elige el fondo de la Biblioteca (A apagado / B liso oscuro, con borde dorado en las miniaturas).
- (9/11) fondo de la Biblioteca: el usuario no quiso A ni B; de 6 opciones más eligió D (cuero marrón con costura dorada). tools/fondo_biblioteca.py pinta la franja libr_00_02 (x0..518, y114..171 de euro/sp/libr/2d/texdb/texdb000.png, hoja 7736AB5A3CB3943D) sobre la hoja ya escalada: ruido FFT que empalma arriba/abajo, costura cada 24 px a x4 en el borde de cada una de las 6 filas, las marcas naranjas (14 px de la izquierda) quedan. Salida mejoradas/fondo/ (prioridad en texturas_pack.py). miniaturas.py: borde dorado (235,195,80), K+2 px, por dentro de cada casillero. Pack 1227, BC3 0,96 GB, entrada verificada. El ojo del Milenio translúcido es otra capa, no cambia. Sin probar en el juego.
- 01:50 (captura: el menú de la carta abierta "quedó desfasado, las opciones no se pueden elegir") raton.c: la carta abierta desde la grilla es el modo 1 (abriendo, 0x10FB55) y luego 2: dos botones redondos, elección [0x66E689] (0 lupa "Información de la carta", 1 cámara; el pad la cambia en 0x10F94D con [0x66DD18] |= 0xB80 y tick 0x10000001; A en 0x10FD67). El puntero de la grilla seguía buscando cartas detrás y un clic apretaba A sobre el botón marcado. Ahora: con modo != 0 y != 4 no se toca la grilla; en modo 2 el botón bajo el puntero se marca (wrapper sub_0010F790, s_btn_want) y el clic aprieta A cuando quedó marcado; en modo 1 el clic se ignora; en modo 3 es A. Botones medidos en la captura (escala 2,81 x 2,14 tomada de las cartas de la grilla y el centro del encabezado): círculos r 24 en (292,6, 316) y (343,1, 316). Log: "[RATON] library card: click button N at x,y" / "click off the buttons". Compilado rc=0. Sin probar. La captura del usuario tenía todavía el fondo viejo: el juego arrancó antes de que terminara el pack de 01:44.
- 02:05 menú de la carta (lupa/cámara) CONFIRMADO por el usuario. Nuevo: "a veces no detecta el clic en la carta hasta apretarlo 2 veces, sobre todo al bajar". Log 01:50: click col 7 row 1 con el cursor ya ahí, sin abrir; segundo clic igual; después sí. Causa: el manejador de teclas de la Biblioteca (sub_0010F790) descarta la tecla mientras la grilla todavía se desliza (sub_00028F50 en 0x10F7B4: [0x66E694] = 9 y sale), y el mouse mandaba un solo A de 45 ms. Arreglo (raton.c): el clic repite A cada 165 ms hasta que la carta se abre (modo sale de 0), suena el "no" 0x10000004 (casillero vacío; s_buzz_at en el hook de sonido sub_001197F0), el cursor deja esa carta o pasan 900 ms. Log: "[RATON] library: card opened after N presses" / "open ... stopped (...)". Compilado rc=0. Sin probar.
- 02:20 el usuario: "elimina lo que hiciste": la repetición de A quitada entera (raton.c vuelve al A único). Su hallazgo: el clic falla si el mouse se está moviendo; quieto sobre la carta funciona. Causa real: el kit pone el movimiento del mouse en la palanca izquierda (xboxrecomp keyboard.c, mouse look), y la primera prueba del manejador de teclas en la grilla (0x10F7AE: sub_00028F50(0) = palanca X del mando 0 [0x650514], contra ±0,3 en 0x427C18/0x4289A0) toma la palanca pasada de 0,3 como desplazamiento con palanca: [0x66E694] = 9 y sale sin leer la tecla. Arreglo: en la grilla (modo 0) las palancas van a cero mientras el mouse se mueve y 200 ms después, o con un clic en curso (el duelo ya hacía lo mismo). Palanca de un mando con el mouse quieto sigue andando. Compilado rc=0. Sin probar.
- 02:05 (reloj real; video 2026-11-09 01-59-06, con la palanca ya en cero): todavía clics sin abrir. Video + log: cada clic en una carta a la que el cursor recién llegaba mueve el cursor y la escena carga la vista previa de esa carta; en ese lapso el A del pad (45 ms) se perdía; con el mouse quieto la vista previa ya estaba cargada y el A entraba. Arreglo (raton.c): el clic en la grilla ya no aprieta A en el pad: s_lib_open guarda la carta y el wrapper de sub_0010F790, justo antes de llamar al manejador, pone la tecla 12 (A: tabla 0x10FEB0 -> 0x10FB55, abre la carta en modo 0) en la ranura de teclas del juego [0x66C880 + 0x1330] = 0x66DBB0 (0x10 = ninguna), con el cursor ya en esa carta. Se descarta si el modo deja de ser 0 o pasan 1,5 s. Log: "[RATON] library: open col C row R (key A into the game slot, N ms after the click)". La palanca en cero del cambio anterior queda. Compilado rc=0. Sin probar.

## 2026-11-09: ataque con el mouse, menú de fases, sin la velocidad de Joey

- **Velocidad al hacer clic en las cartas, quitada** (el usuario: se sacó lo de Joey pero no esa velocidad): `s_fast_cards` = 0 fijo, `ygo_fast_cards_set` no hace nada (un "Sí" guardado no la prende) y la fila de F2 "Jugar cartas rápido" ya no se agrega (main.c).
- **Objetivo del ataque "desfasado"**: el cursor para elegir carta (objetivo de ataque o de efecto) no es `sub_00092FA0` sino `sub_00092E20(máscara)` (lo llaman 30 lugares). Solo se detiene en las casillas que acepta `sub_00092620` (eax = zona; jugador, casilla, máscara). El mouse no lo alcanzaba: el cursor quedaba en el primer objetivo del juego y el clic confirmaba ese.
  - Wrapper nuevo `sub_00092E20` (raton.c, rename_pads.py): la casilla bajo el puntero, si el juego la acepta, pasa a ser el cursor (`sub_00086F50` + tic, devuelve 0).
  - Un clic en una casilla que el juego no ofrece no aprieta A (log `[RATON] target: …`).
- **Menú de fases (B en el campo: Fase principal 2 / fin del turno / batalla)**: es el cuadro de `sub_00087FA0` (bloque 0x660B70). Datos: [0x660B8A] es la línea marcada; [0x660B9A] es el paso (0 = espera teclas). Teclas: 3 líneas `sub_00087A80`, 2 líneas `sub_000878C0`.
  - El marcador de la línea k está en x = [0x660B78]·8+8, y = [0x660B82+2k] + ([0x660B99]−[0x660B7E])·8 − 14. Las líneas van una debajo de la otra.
  - Antes se trataba como flechas izquierda/derecha (side_pointer, estado 100): por eso no andaba.
  - Ahora `ask_box`/`ask_pointer`: la línea bajo el puntero se marca y el clic la responde (A al handler en esa llamada). El clic derecho es B. Sirve también para los cuadros de sí/no de 2 líneas.
  - Log por cuadro: `[RATON] box of N lines …, lines y …`; por clic: `[RATON] box click game x,y -> line k (…)`.
  - Si la franja de cada línea queda corrida, se corrige con esos números.
- Compilado (rc=0), sin probar.
- Prueba del usuario (capturas): el objetivo del ataque y la velocidad quitada, confirmados (anotados en NO ROMPER). Los cuadros "¿Terminar tu Fase Principal?" (3 líneas) y "¿Terminar tu turno?" (Yes/No) no respondían.
  - El log daba y = −142 en todas las líneas: los números del marcador viejo no son este cuadro.
  - El cuadro en pantalla es el widget de lista `[[0x656F88]+0x58]`, el mismo que el cartel de salir de la Biblioteca. Su dibujo es `sub_000F0040`.
  - Datos del widget: +0xBC4 es la cantidad de líneas. La línea k es el objeto en +0x1B0+4k; su lugar está en +0xC/+0x10 (floats, px del juego; arriba a la izquierda de la barra). La barra mide 450×30 (constantes 225 = mitad del ancho y 30 = paso entre líneas en el XBE).
  - `ask_box` lee esas posiciones; `ask_pointer` toma la barra x..x+450, y..y+30.
  - Compilado (rc=0), sin probar.
- Usuario (2026-11-09): "ahora parece que sí": los cuadros de fase y de Sí/No funcionan con el mouse (anotado en NO ROMPER). El log mostró que las líneas se deslizan al abrir (y 232 → 169): por eso se leen en cada cuadro.
- **Clic derecho que había que apretar 2 veces** (fin de turno, atacar). En el log de 02:26, un clic derecho rápido (abajo y arriba en la misma lectura) no abría nada y uno sostenido sí: B pasaba al juego solo mientras el botón estaba abajo, según GetAsyncKeyState.
  - Ahora el gancho WH_MOUSE_LL cuenta cada WM_RBUTTONDOWN (`s_rpress`), aunque se suelte antes de la lectura.
  - El B dura al menos `B_HOLD_MS` (80 ms) desde que se aprieta, como el A.
  - Una pulsación del gancho solo cuenta si tiene menos de 150 ms; así no queda una de otra pantalla.
  - Log: `[RATON] field right click -> B (held | quick, from the hook)`.
  - Compilado (rc=0), sin probar.
- Texturas nuevas del volcado (2026-11-09): 191 escaladas (falló solo C713A94082B5B00B_64x8, queda la original); pack rehecho: 1417 texturas, textures_bc3.pack de 1.08 GB.
- **Barra de la Biblioteca con la rueda** (pedido: "que la flecha amarilla de la izquierda suba o baje al mismo nivel"): la rueda ponía la fila de arriba y redibujaba, pero no movía el marcador de la barra.
  - El juego lo ubica desde la fila con `sub_000E3510(fila)` (thiscall sobre la barra `[escena+0x1260]`, ret 4) antes de redibujar (0x38332).
  - El wrapper de `sub_00038220` ahora lo llama con la fila nueva.
  - Compilado (rc=0), sin probar.

## 2026-11-09: "la mejor versión por ahora" — respaldo, F8, armado del mazo

- El usuario: la barra de la rueda "quedó de 10" y pidió anotar todo, porque es la mejor versión hasta ahora. Anotado en NO ROMPER.
- **Respaldo:** `respaldos/2026-11-09_mejor_version/` contiene ESTADO, tools/*.py, ygo-apex/src/*.c *.h, env.bat, CMakeLists, `exe/ygo_recomp.exe` y el diff del kit.
  - El exe ya incluye los dos cambios de abajo, que todavía no están probados.
  - Para volver atrás: correr rename_pads.py y compilar. Si el armado del mazo falla, alcanza con sacar `sub_000D9A10` de rename_pads.
- **F8 solo** (antes Ctrl+F8, solo 60/90): cicla 60 → 90 → 120 → 60 FPS (`xbox_VblankHzSet`) y muestra "NN FPS" 1,5 s en pantalla (`xbox_MenuToast` del kit). Está en `fps_key`, raton.c.
- **Armado del mazo con el mouse:**
  - Manejador de teclas: `sub_000D9A10` (lo llama `sub_000DBA20`). Lee la tecla de 0x66DBB0 igual que la Biblioteca. Teclas: 4 arriba, 6 abajo, 5/7 pestaña, 12 A, 13 B.
  - Variables:
    - [0x66D1EC]: parte con cursor (1 = lista);
    - [0x66DB76]: fila 0..5;
    - [0x66DB78]: primera carta visible;
    - [0x66D1E8]: total de cartas;
    - [0x66DB74]: pestaña 0..6.
  - Por qué "se movía para todos lados": el puntero genérico caminaba la cruceta hacia el puntero, y el manejador toma el stick izquierdo (el movimiento del mouse, en el kit) como scroll.
  - Ahora (`deck_pointer` + wrapper):
    - no hay caminata y los sticks quedan en 0;
    - la fila bajo el puntero se marca y el clic es A en la ranura;
    - la rueda baja o sube una carta (cursor al borde y el paso del propio juego);
    - un clic en una pestaña la elige.
  - Lugares medidos en la captura:
    - filas: y 118+50·r, alto 50, x 52..608;
    - pestañas: centro x 101+72,2·k, y 86..110.
  - Fuera de la lista, el clic es A tal cual. Log: `[RATON] deck …` con la parte (d1ec) en cada clic, para ajustar las otras partes (baúl/mazo I-II-III, Cartas nuevas).
  - Compilado (rc=0), sin probar.
- **Prueba del usuario (video 02-50-05):** "mejoró bastante". Faltaba suavizar el desplazamiento y poder usar todos los botones: las pestañas, "Cartas nuevas" y los cuadros de arriba actuaban sobre la carta marcada. También faltaban los botones redondos de la carta. El log mostró que las filas sí tomaban el clic y que las pestañas solo movían el marco.
- **Lo que hay en la pantalla** (RE, ver comentario en raton.c, sección "deck builder"):
  - Hay dos manejadores según [0x66D1EA]: 0 = baúl (`sub_000D9A10`), 1..2 = lado del mazo (`sub_000DA730`, ahora también envuelto).
  - Teclas (`sub_000D87D0`): 12 = info de la carta, 13 = botones redondos (parte 11, elección [0x66D1F6]; 13 ejecuta), 14 = aplica la pestaña desde la fila de pestañas (parte 0), 15 = menú "Cartas nuevas" (parte 10, [0x66D1F3]), 8/9 = mazo I/II/III ([0x66D1F0]), 10 = baúl ↔ mazo.
- **Cambios:**
  - Rueda: un solo deslizamiento del propio juego de N filas (como su avance de página: tope += N, [0x66DBC0] = N, [0x66DBBC] |= 0x100000 (|0x4000 subiendo), [0x66DBB8] = 0xB). El cursor no salta.
  - Clic izquierdo en una carta abre sus botones redondos (13).
  - Pasar el mouse por un botón redondo lo marca y el clic lo ejecuta. Medidos en el video: y 317, x 320 + 50,25·(k−2), radio 24.
  - Pestaña: marco ahí, parte 0, tecla 14 del juego, cursor de vuelta a la lista.
  - Cuadros de arriba: baúl / mazo (10), mazo I/II/III (8/9 hasta llegar), "Cartas nuevas" (15).
  - Un clic fuera de todo no hace nada. El clic derecho sigue siendo el B del mando.
  - Zonas medidas en el video (imagen x 63..1857, y 0..1009 → 2,803 × 2,102).
- **Log:** cada tecla que recibe el manejador y la parte en que queda el cursor (`[RATON] deck chest|side: key N -> part P …`), y cada clic con lo que tocó.
- **Pendiente:** el lado del mazo usa otras variables (valores cíclicos 0..9 por mazo); por ahora tiene rueda = arriba/abajo y clic = 13 sobre lo marcado.
- Compilado (rc=0), sin probar.
- **Prueba del usuario (video 03-01-16):** "avanzamos bastante". Las pestañas filtran, pero después la carta "se selecciona sola", y el lado del mazo no anda.
  - **Causa (log):** el kit también convierte el clic izquierdo en el A del mando, que en esta pantalla es la tecla 14. En la lista, 14 abre los botones de la carta (parte 9). Además, mis teclas estaban mal asignadas: 13 (B) abre la parte 11, no los botones. Y 8/9 mandan la carta al Deck Principal o al secundario (`sub_000DEF20`): los clics en I/II/III estaban agregando cartas.
  - **Arreglo:**
    - En esta pantalla se descartan el A y el B que el kit saca del mouse (Espacio y Esc siguen andando).
    - Clic en una carta = 14, que abre sus 5 botones (parte 9, [0x66D1F3]).
    - Los botones se marcan al pasar el mouse y el clic los ejecuta (14). Están a y 317, x 320+50,25·(k−2).
    - "Cartas nuevas" (15) abre 6 botones (parte 10, [0x66D1F6]), a y 238, x 320+50,4·(k−2,5).
    - El clic derecho vuelve atrás (13) desde menús e info; en la lista no hace nada.
    - Clic en la info = volver.
    - Mazo I/II/III (lado baúl): "Cartas nuevas" y luego el botón 2 (mazo siguiente, `[0x66D1F0]` + 1 mód 3) hasta llegar.
  - **Lado del mazo** (`sub_000DA730`, grilla):
    - i = [0x66DB7A+2d] + 2d; columna [0x66DB80+2i], fila [0x66DB8C+2i], primera fila [0x66DB98+2i].
    - Pasar el mouse marca la carta (|=0x288 y tic). El clic es A (14).
    - La rueda pone el cursor en el borde y usa el paso del juego.
    - Sus 2 pestañas se cambian con la tecla 11.
    - Medidas: grilla x 72+40·c, y 147+74,7·r; pestañas x 78..132 / 146..203, y 87..111.
    - "Nada" = 15. Baúl = 10.
    - I/II/III en este lado todavía no.
  - Compilado (rc=0), sin probar.
- **Capturas 03:11 + Ctrl+F9:** "en ese menú lo único que falla son esos botones" (los botones redondos de una carta del lado del mazo).
  - El volcado confirmó la grilla del mazo: cartas de 35×51 en x 54,5+40·c y filas cada 74 px, como la medición.
  - En la captura, los botones están en x 220..420 (paso 50) e y 319, igual que el hitbox.
  - El fallo era otro. El manejador del mazo guarda la elección en [0x66D1F3 + lado] y [0x66D1F6 + lado] (lado = [0x66D1EA]; 0xDAABE). Yo escribía la copia del baúl (+0), así que del lado del mazo el botón marcado no seguía al puntero y el clic esperaba un botón que nunca se marcaba.
  - Además, el menú "Nada" de ese lado tiene 7 botones (0..6).
  - `deck_menu` ahora usa la copia de cada lado.
  - Compilado (rc=0), sin probar.
- **Prueba del usuario (capturas 03:3x):** los botones redondos del lado del mazo ya andan, pero no se iluminan. La carta queda "anclada": después de abrirla, otro botón no responde. Un clic en un lugar vacío debería deseleccionar.
  - **Causa del anclaje (log 03:20):** cada clic "en nada" (537,361; 500,85; 522,21; I/II/III) terminaba igual en la tecla 14, que abre los botones de la carta. El A del kit sale de los mensajes de la ventana y llega un poco después del botón físico. Un clic rápido ya estaba suelto para GetAsyncKeyState, pero el A seguía apretado y se colaba.
  - **Arreglo:**
    - En esta pantalla, el A y el B del kit quedan fuera hasta 300 ms después de soltar el botón del mouse.
    - Con un menú abierto (botones de la carta, "Cartas nuevas"/"Nada", parte 11), un clic fuera de los botones lo cierra (13). Ese mismo clic se repite cuando el cursor vuelve a las cartas, dentro de 1 s: si cayó en otra carta o en una pestaña, la toma directo. Si cayó en un lugar vacío, solo queda cerrado.
  - **Brillo:** el juego enciende el botón en la escena (`sub_0003B9E0`). Cuando [0x66C880] tiene 0x800, llama a `sub_000ED100(botón, 1)` con [0x66D1F3/F6 + lado], que apaga el encendido ([this+8]) y enciende el nuevo. Mi código marca el botón igual que las flechas del juego (byte + |= 0xB80 + tic), así que en el código no se ve la diferencia.
    - Para verlo, `sub_000ED100` está envuelto y anota en el log `[RATON] deck light: button N animate A (lit L of C, part P)` mientras se está en el armado del mazo.
  - Compilado (rc=0), sin probar.
- **Capturas 2026-11-09 tarde:** en el armado del mazo, el clic derecho no volvía atrás. Con el mando, B en las cartas abre "Terminar construcción de Deck. ¿Estás seguro?" (Aceptar/Cancelar), y ese cuadro no se manejaba con el mouse. Además: línea en el medio del globo de diálogo con F6 (texturas mejoradas) y texturas nuevas para escalar.
  - **Cuadro "Terminar construcción" = parte 11** (0xDA0A8 en el baúl, 0xDB10E en el mazo):
    - elección [0x66D1F2] (0 Aceptar, 1 Cancelar; arranca en 1), un solo byte para los dos lados;
    - el juego la cambia con [0x66C880] |= 0x40000 y tic 0x10000001 (0xD9B5C);
    - 14 ejecuta: Aceptar sale (0xDA538), Cancelar vuelve (0xDA560).
  - **Arreglo (raton.c):**
    - `deck_menu` conoce la parte 11 (2 opciones, [0x66D1F2]); el marcado usa |= 0x40000 ahí.
    - Barras de 450×30 centradas en x 320 (como las cajas del duelo): Aceptar y 231..261, Cancelar 261..291. Medidas en la captura contra la grilla del mazo (escala 2,8 × 2,1).
    - El clic derecho manda 13 en todas las partes salvo la 0: en las cartas abre este cuadro; dentro del cuadro es Cancelar.
  - **Línea del globo con F6:** en el atlas 0E1804B57B783074 (1024×512), las dos mitades del globo terminan en la columna 913 (u 0,8916) y se tocan en x 320. En la escalada, las últimas columnas salen más claras, y `restore()` mezcla la columna transparente 913 (alfa bicúbico): queda un borde oscuro semitransparente → línea.
    - `texturas_pack.py`: `SEAMS` + `fix_seams()`. En las filas donde el original es opaco hasta el borde, las columnas escaladas del último texel (y 3 texels antes) toman el color de adentro, opacas; después del borde queda transparente. Verificado en los píxeles de la escalada.
  - Texturas nuevas: 110 escaladas (C713A94082B5B00B_64x8 vuelve a fallar y queda la original). Pack rehecho con la costura corregida: 1526 texturas, textures_bc3.pack de 1,14 GB (la costura cae en 3652, borde de bloque BC3).
  - Compilado (rc=0), sin probar.
- **Todas las texturas del disco (pedido del usuario, 2026-11-09):**
  - Inventario (sin los otros idiomas): 5946 tga (las cartas grandes, ya hechas por `cartas_volcado.py`), 329 png, 7 jpg (artwork de `_resources`, no se usa en el juego) y 846 `.igb` (modelos de Alchemy: monstruos 3D, escenarios bg/, menús, efectos), 1,19 GB.
  - `tools/analizar_disco.py` prueba varias formas de subida para cada png/tga y busca la huella en dump/. Hubo 27 coincidencias, todas con una sola regla por tipo:
    - PNG con paleta → P8, índices invertidos en vertical y en orden Morton, paleta BGRA con el alfa del tRNS;
    - PNG RGBA → A8R8G8B8 invertido y en orden Morton.
    - La imagen del volcado es la del disco invertida (diferencia 0).
  - `tools/texturas_disco.py` aplica esa regla y pone en dump/ las que faltaban: **223 nuevas**. Son el final (ending/bg, 97), las animaciones de los retratos (cf_anim: 81 grandes, 28 chicas, 27 fondos), atlas 2D que faltaban y el atlas común sin guante (modo puntero). Quedan fuera la máscara del duelo (LA) y la sombra (L): su formato en GPU no está comprobado.
  - **Los `.igb` no se pudieron todavía:** las texturas de los volcados no aparecen crudas como ARGB adentro (búsqueda de 595 muestras en los 846 archivos: 0). La cabecera es la de Alchemy (12 u32, 0xFADA, versión 0x80000005: entradas, meta-objetos, objetos, memoria 0x1BCC90 al final en m000), y los objetos van empaquetados sin alinear. Hace falta un lector de IGB (meta-campos → objetos igImage: ancho, alto, formato, bloque de memoria) y averiguar el formato de píxeles (¿DXT/P8?) para reproducir la huella.
  - Escaladas las 224 (C713A94082B5B00B_64x8 sigue fallando). `texturas_pack.py` dejó fuera 61E0FAE81FBA1B4B_32x128, porque la IA le bajó el brillo de 192 a 58; queda la original. Pack: 1749 texturas, textures_bc3.pack de 2,18 GB. El cargador del kit usa `_fseeki64`, así que pasar los 2 GB no es problema. Sin probar en el juego.

## 2026-11-09 16:50 — cementerios con el mouse; texturas del duelo
- Usuario: "en el campo de batalla los cementerios no se pueden usar con el mouse". Log del duelo: el puntero sí lleva el cursor al cementerio (zona 14, p0 y p1, hover + tick), pero nunca hubo un clic ahí.
- Código: en `sub_00099660`, A sobre las zonas 12/14/15 (tabla de casos en 0x99928) abre una lista con `sub_000C5AE0` si tiene cartas (cuenta en [0x65D0AA/AB/AC + jugador*0x13F8]; vacía = sonido de error 0x10000004). Lista: cuántas [0x6589DC], 8 bytes c/u desde 0x65855C, carta [0x658556], paso [0x658552] (tabla 0xC5A4C: 0 recorrer, 1 carta elegida). `sub_000C5710` la maneja: tecla 0x20 = carta anterior, 0x10 = siguiente, 1 = A, 2 = B cierra.
- La rueda mandaba arriba/abajo, que esa lista ignora. Arreglo (raton.c): `sub_000C5710` envuelto (rename_pads): un paso de rueda se vuelve 0x20/0x10 por una llamada (movimiento, límites y sonido del juego; si no lo tomó, el paso espera). `list_pointer` (antes del campo): rueda → pasos de la lista; clic izq. = A, der. = B. Log `[RATON] list ...`. No hay posiciones en pantalla de las cartas de la lista: el puntero no elige carta, solo la rueda.
- Texturas: el duelo volcó 52 nuevas (modelos de monstruos, DXT1/fmt19/0B); 51 escaladas, C713A94082B5B00B_64x8 sigue fallando. Pack rehecho.
- Compilado rc=0. Sin probar.

## 2026-11-09 17:30 — texturas de los modelos 3D (.igb) sin jugar
- `tools/texturas_igb.py`: lector de IGB de Alchemy (meta-campos, alineaciones, meta-objetos, entradas, objetos; la memoria al final son los bloques de cada igMemoryDirEntry seguidos, sin relleno). Las 846 .igb se leen sin error.
- igImage: ancho/alto/bytes por pixel en +0/+4/+8, formato +36, bloque de pixeles +44. Formatos: 5 RGB (4018), 0 gris _s (3059), 65536 paleta, 7 RGBA, 14 DXT, 3/8/10 (fondos).
- Huella comprobada: B,G,R,0 (formato 5) o B,G,R,A (7), swizzle, sin dar vuelta, un nivel. Las 218 que el juego ya había volcado coinciden pixel por pixel. El volcado las guarda sin dar vuelta (5 opaca, 7 con su alfa).
- Resultado: 3614 texturas distintas, 3396 nuevas en dump/ (monstruos, efectos, menús). Fuera: los mapas _s (gris) y los formatos de los fondos (GPU sin comprobar).
- Escalado de las 3396 + pack en marcha.
- 18:21 terminado: 3331 escaladas en 69 min (fallan 5 tiras de 8 px de ancho/alto, quedan las originales). Pack: 5129 texturas, 15,6 GB → 3,89 GB en BC3. Excluidas por brillo (la IA las borró): 303B369A3BA07F7A, 84BAC4F870CCC14A, más las de antes. Sin probar en el juego.

## 2026-11-09 18:40 — el resto de las texturas de los .igb
- `texturas_igb.py` ahora saca también (reglas comparadas con el volcado, ver su cabecera): gris de los mapas _s (byte en swizzle, 214 iguales), DXT1 (bloques tal cual, 6), paleta (índices en swizzle ^ paleta del igClut R,G,B,A→B,G,R,A ×31, 24 iguales). Un ObjectRef es el número de su entrada del directorio (`object_refs`).
- Total: 6509 texturas distintas en los igb; 2752 nuevas en dump/ (casi todas grises).
- Fuera, sin evidencia: fondos de 16 bits (formatos 3/8/9/10, 27 imágenes; ninguna conversión 565/1555/4444/88 coincide y no están en el volcado), niveles de las 3 cadenas de mipmaps, `posteffect` (L 256x256, nunca volcada). Imágenes sueltas L/LA del disco: solo 2 (la máscara del duelo ya estaba volcada).
- Escalado + pack en marcha (~1 h).
- 19:2x `tools/revisar_texturas.py` (nuevo): achica cada escalada al tamaño original y la compara (diferencia media, color, energía de borde fino, tinte en grises). Límites elegidos mirando hojas de comparación: cambio > 14 o detalle < 0,25 = contenido perdido (ruido, follaje, tramas, rayas) → `revision/fuera_ia.txt`, y `texturas_pack.py` deja el original. Resultado: 7797 revisadas, 254 fuera, mediana de diferencia 4,35, detalle mediana 1,00, 0 grises teñidos. Hojas en modding/texturas/revision/.
- Pack final: 7557 texturas, 19,35 GB → 4,84 GB en BC3. Sin probar en el juego.

## 2026-11-09 20:00 — letras nítidas (sin IA)
- `tools/letras_nitidas.py`: las 3 páginas de letras (9BF7092A495E4E38, F867714BEE53B873, FED7C5881DA64D4A; 512x512, blancas, forma en el alfa, 16 por fila en orden de código dadas vuelta) agrandadas x4: alfa bicúbico, borde donde cruza la mitad del máximo local, rampa de ~2 px. Misma forma y lugar; tinta igual (10684→10700, 11875→11859, 16812→16990). Hoja: revision/letras_zoom.png.
- `texturas_pack.py`: carpeta `mejoradas/letras/` primero y sin `restore()` (le volvería a poner el alfa borroso).
- Pack nuevo armándose como `textures_bc3_nuevo.pack` (el usuario juega con el actual); cambiarlo por `textures_bc3.pack` con el juego cerrado. Sin probar.

## 2026-11-09 — pausa en seco (F7)
- Kit `src/d3d/nv2a_pb_d3d11.c`: F7 (o la tecla Pausa) congela el juego. Al final del flip, después del Present, `freeze_wait()` retiene el flip con `xbox_PauseSet(1)` (relojes y sonido parados) y bombea la ventana. El título dice "PAUSA ... texturas mejoradas/originales".
- F6 congelado: cambia las texturas y deja pasar UN cuadro (dibujado ya con las otras) y vuelve a congelar. No muestra el cartel, que quedaría pegado en la captura.
- Con el menú F2 abierto los cuadros corren (si no, no se vería); al cerrarlo vuelve a congelar.
- Log `[d3d11] [PAUSA] congelado/sigue`. Compila (rc=0). Sin probar.

## 2026-11-09 — línea clara en el medio del globo de diálogo
- La pausa F7 funcionó (captura del usuario).
- Captura: línea clara en el medio del globo (0E1804B57B783074), con el valor (255,254,214). Era la mip 3 del pack: LANCZOS hace "ringing" en el borde del alfa (de 237 a 254). `mip_chain` ahora usa BOX, cada nivel a partir del anterior, para todas las texturas.
- SEAMS suma CAE5437E911B62D9: borde 473 (globo liso) y 690 (globo de grito). Las 2 columnas que siguen al borde (transparentes y sin geometría) toman el color de adentro y quedan opacas.
- Verificado mip 0–5: sin banda clara en las 3 costuras. Pack rearmándose como textures_bc3_nuevo.pack. Sin probar en el juego.

## 2026-11-09 — pack nuevo puesto + diálogos por personaje
- Pack rearmado (letras nítidas, costuras de los 3 globos, mipmaps BOX): 7560 texturas, 4.85 GB, puesto como textures_bc3.pack. El anterior quedó como textures_bc3_anterior.pack. Sin probar.
- Diálogos: default.xbe tiene 3680 textos por idioma (tabla: cantidad + offsets, después los textos latin-1 con 0 al final). Tablas en el archivo: es 0x4A1FB6, it 0x4D9CCE, fr 0x514464, de 0x54D076, en 0x58777C, jp 0x5BA0CC. Nombres de los 28 personajes en 86–113; diálogos de duelo en 317–848, 19 por personaje en el mismo orden. Tipos de frase deducidos del texto: 3 de inicio, fusión, mágica, trampa, 3 de daño, fusión/mágica/trampa del rival, 3 de gana, empate, 3 de pierde.
- `tools/dialogos.py` → modding/dialogos/dialogos_por_personaje.csv (Excel, ;) y un .txt por personaje (ES + EN).
- Pendiente si el usuario junta audios: enganchar cuando el juego muestra el texto N y reproducir el audio N.

## 2026-11-09 — doblaje con ElevenLabs
- `tools/doblaje_elevenlabs.py` es una ventana Tkinter que usa solo la biblioteca estándar:
  - clave: la pega el usuario, se guarda en modding/dialogos/.clave_elevenlabs;
  - Conectar: GET /v1/voices (primero las del usuario) y GET /v1/models;
  - voz por personaje, modelo (eleven_v3 o v4: emociones en corchetes) y estabilidad (Creativa 0 / Natural 0.5 / Robusta 1);
  - editor de cada frase;
  - generar una frase, un personaje o todo lo que falta: POST /v1/text-to-speech/{voz}?output_format=mp3_44100_128 con language_code "es". Antes muestra cuántos caracteres va a gastar y tiene botón Parar;
  - escuchar: abre el mp3.
- Guion: modding/dialogos/guion.json, 532 frases (507 con voz; Strings dice "..."). Cada frase arranca con una emoción según el momento, y las notas del juego "(REACCIÓN: pánico)" pasan a "[pánico]". Audios: modding/dialogos/audio/N.mp3. Si se edita una frase ya generada queda "cambió" y se rehace.
- `--revisar` arma el guion y muestra un pedido de ejemplo sin conectarse: probado. La conexión real no está probada (no hay clave ni créditos).

## 2026-11-09 — guion en español latino
- A pedido del usuario se borraron los 57 audios generados con el texto de España (Yami Yugi, Seto, Joey). Las 3 voces elegidas siguen en guion.json.
- Las 532 frases se adaptaron al español latino con emociones entre corchetes, en modding/dialogos/latino/01-03.txt (`N<TAB>texto`), y se cargaron en guion.json. Quedan 509 con voz y unos 34.000 caracteres; Strings, más los silencios de Ishizu, Odion y Shadi, quedan "...".
- Nombres como en el doblaje latino: Tristán, Ojo/Artículo del Milenio, Juego de las Sombras, Duelo de Monstruos, Cazador de Cartas Raras, Computadora de Duelo (en pantalla el juego dice "Duel Computer").

## 2026-11-09 — Lumis & Umbra con dos voces
- `doblaje_elevenlabs.py`: tabla DUOS {"Lumis & Umbra": ("Lumis", "Umbra")}.
  - Para ese personaje se eligen dos voces, guardadas en voces["Lumis"] y voces["Umbra"].
  - Cada frase se marca por partes con {Lumis}, {Umbra} o {Ambos}. Cada parte sale con su voz; {Ambos} pide las dos voces y las mezcla (amix).
  - Las partes se juntan en un solo N.mp3 con ffmpeg (concat; búsqueda en el PATH o en la carpeta de WinGet).
  - "generado" guarda las dos voces unidas con "+".
- Las 19 frases (678–696) se repartieron entre los dos en latino/04_lumis_umbra.txt (se sacaron de 02.txt) y ya están en guion.json.
- Probado sin conexión: el reparto de partes y la mezcla con tonos de prueba (3 partes → mp3 de 1.6 s). Con ElevenLabs sin probar.
- Al mostrar el texto en el globo hay que sacar {…} además de […].

## 2026-11-09 — voces y textos latinos en el juego (mod)
- Textos del juego: punteros de las 6 tablas en 0x5FC2DC (jp, en, de, fr, it, es), bases en 0x5FC2C4, idioma actual en 0x5FC2C0 (5 = es). `sub_0002C5C0(n)` devuelve el texto n en el idioma actual y `sub_0002C590(n, idioma)` en el idioma pedido. Los dos están envueltos en `src/voces.c`, agregado a PADS y renombrados.
- `voces.c`, con YGO_VOCES = modding\voces_latino (env.bat):
  - la primera vez que se pide un texto, escribe textos.txt (latin-1) encima de los textos 317–848 de la tabla española, en el mismo lugar (26.197 de 27.125 bytes; si no entra, no toca nada), y mueve los offsets;
  - cuando el juego pide en español un texto entre 317 y 848 (otro distinto, o el mismo después de 2 s), reproduce N.mp3 por MCI en un hilo aparte; una frase nueva corta la que está sonando;
  - log `[VOZ] texto N`.
- `tools/voces_mod.py` y `modding/meter_voces.bat`: copian los audios "listos" y escriben textos.txt (el texto del guion sin [..] ni {..}). Hoy: 532 textos y 490 voces. A Trusdale (431–449) le falta la voz; su texto ya está en latino.
- Compila (rc=0). Sin probar. Riesgo: si el juego pide el texto antes de mostrar el globo (por ejemplo, para medirlo) o varias veces seguidas, el audio podría adelantarse o repetirse; mirar `[VOZ]` en el log.

## 2026-11-10 — Duel Computer: robot en el duelo, Kaiba al inicio y al final
- `doblaje_elevenlabs.py`: la Duel Computer pasa a DUOS ("Robot", "Seto").
  - QUIEN_POR_MOMENTO elige quién habla en las frases sin marca: Kaiba (la voz de Seto, la misma del personaje) en "Empieza", "Gana", "Pierde" y "Empate"; el Robot en lo demás.
  - `{Robot}` o `{Seto}` en el texto cambian quién dice esa parte.
  - `voz_de(g, ln)` ahora toma la frase y lleva solo las voces que esa frase usa. Lumis & Umbra siguen como antes (sus 19 frases continúan "listo").
- Sus 19 frases quedaron "cambió": falta elegir la voz del Robot, regenerarlas y correr `meter_voces.bat`.

## 2026-11-10 — Voces sin silencios de más, y se cortan al saltar el diálogo
- Duel Computer regenerada y en el mod: 532 textos y 509 voces.
- `voces_mod.py` recorta los silencios al copiar a `voces_latino/` (los originales de `dialogos/audio` no se tocan):
  - filtro ffmpeg `silenceremove`, detection=peak, -45 dB;
  - nada de silencio al principio;
  - pausas internas de más de 0,35 s quedan en ~0,4 s;
  - uno ya recortado y más nuevo que su original no se rehace.
  - Semántica en ffmpeg 9, medida: `stop_duration` es el silencio que se detecta y `stop_silence` lo que queda (794.mp3: 0,73 → 0,37 s).
  - Antes había 6 audios con pausas de más de 0,6 s; el resto, poco.
- `voces.c` + `mando_directo.c`: A o B (mando, teclado o clic, lo que lee el juego) mientras suena una voz la corta.
  - Va por reloj, no por frames: igual a 60, 90 o 120 fps.
  - Se ignora la pulsación de los primeros 250 ms de cada frase (la que la trajo).
  - Log: `[VOZ] N saltado: voz cortada`.
- Diagnóstico en el log: al pasar a otra frase, `[VOZ]   (N pedido K veces en M ms)`. Dice si el globo pide su texto cada frame; si es así, se puede cortar también cuando el globo se cierra solo.
- Sin probar en juego. Compila (rc=0).

## 2026-11-10 — Las voces siguen los fps del juego (F2)
- F2, sección "Voces", fila "Velocidad de las voces": "Sigue los FPS" (por defecto) o "Normal". Se guarda como `voces_velocidad` en rdr_menu.ini; traducida en f2_idiomas.c. Este port no tiene launcher.
- `voces_mod.py` genera `voces_latino/x1.5/N.mp3` y `x2/N.mp3` desde el audio recortado, con ffmpeg `atempo` (más rápido, mismo tono). 509 de cada uno; tarda ~1 min la primera vez y después solo rehace los cambiados.
- `voces.c` mide los fps reales cada 0,5 s en el hilo de la voz, con `nv2a_d3d11_presents()`. Es una función nueva del kit, en `nv2a_pb_d3d11.c`, y solo agrega.
  - Al empezar cada frase elige: menos de 75 fps → x1; menos de 105 → x1.5; 105 o más → x2.
  - Si falta un archivo, usa el más lento.
  - Log: `[VOZ] N a x1.5 (92 fps)`.
  - Una pausa sin frames (F7, cargas) conserva la última medida.
- Compila (rc=0). Sin probar en juego.

## 2026-11-10 — Configuración.exe (launcher) y ajustes que sobreviven a una actualización
- **Dónde se guardan:** `Yu-Gi-Oh Port\Ajustes\` (`ygo_menu.ini` y `launcher.ini`), al lado de `Partidas\`. Un zip de una versión nueva no trae esa carpeta, así que no la pisa.
  - `env.bat` define `YGO_AJUSTES`. `main.c` carga de ahí, y la primera vez copia el viejo `ygo-apex\ygo_menu.ini`. `f2_idiomas.c` lee y escribe ahí `launcher.ini`.
- **Kit (`host_menu.c`):** nueva función `xbox_MenuPoll()`, llamada desde `pump()` en `nv2a_pb_d3d11.c`, en el hilo de la ventana, que es el mismo del menú.
  - Cada 0,5 s mira la fecha del archivo. Si otro programa lo cambió, aplica solo las filas distintas y redibuja el panel.
  - Lo que guarda el propio juego no se relee (`s_file_time`).
- **`tools/launcher/Configuracion.cs`** (`compilar.ps1` lo deja como `Yu-Gi-Oh Port\Configuración.exe`):
  - Mismas claves y valores que F2:
    - Imagen: `render_scale`, `widescreen`, `window`, `fullscreen`, `aniso`, `vsync`, `fps`, `imagen_2d`.
    - Controles y texturas: `raton_modo`, `raton_sensibilidad`, `botones`, `pack_art`, `tex_dump`.
    - Juego: `language`, `voces_velocidad`, `modo_rapido`.
  - 6 idiomas.
  - Guarda en cada cambio, escribiendo a un `.tmp` y reemplazando, para que el juego nunca lea medio archivo.
  - Un `FileSystemWatcher` muestra los cambios de F2 en vivo y solo toma las claves presentes.
  - JUGAR abre `ygo-apex\run.bat` sin consola. El launcher queda abierto.
  - Estado: `run.bat`, pack de texturas y voces.
  - Todavía sin botón de actualizar (no hay releases de GitHub).
- Compila: launcher rc=0; el juego compila, pero el enlace queda pendiente hasta que se cierre (exe bloqueado). Sin probar.

## 2026-11-10 — Sin "Botones en pantalla", Jugar Yu-Gi-Oh.exe y mano en alta resolución
- "Botones en pantalla" sale de F2 (`k_pack_rows`) y del launcher: el usuario dijo que es de Red Dead Revolver. El estilo automático (`btn_auto_thread`) sigue funcionando.
- `Jugar Yu-Gi-Oh.exe` (`tools/launcher/Jugar.cs`, en `compilar.ps1`): abre `ygo-apex\run.bat` sin consola y se cierra.
  - Ícono de los dos .exe: `tools/launcher/yugioh.ico`, hecho por `tools/icono.py` (la mano HD sobre un círculo dorado).
- Mano del cursor:
  - `tools/guante.py` ahora escala la mano x4 (256×256) con Upscayl digital-art-4x, el mismo modelo del pack; si no está Upscayl, usa nearest.
  - `raton_guante.h` define `GUANTE_HD`. `GUANTE_SIZE` y la punta siguen en unidades del juego (64).
  - `raton.c` `glove_make` reduce la mano al tamaño del puntero promediando el área (premultiplicado), en lugar de nearest desde 64: queda suave a cualquier tamaño de ventana.
- Compila (rc=0). Sin probar en juego.

## 2026-11-10 — Jugar Yu-Gi-Oh.exe sin cmd.exe (Malwarebytes lo marcó como malware)
- Malwarebytes puso en cuarentena el primer `Jugar Yu-Gi-Oh.exe`. Era un .exe chico, sin firma y sin datos de versión, que abría `cmd.exe` oculto: un patrón típico de malware.
- Ahora `Jugar.cs` abre `ygo-apex\build\Release\ygo_recomp.exe` directamente:
  - toma las variables leyendo las líneas `set` y `for … do set` de `env.bat`. Probado sin abrir el juego: las 26 variables salen iguales;
  - usa `ygo-apex` como carpeta de trabajo;
  - escribe la salida en `runs\last_out.txt` y `last_err.txt` hasta que el juego cierra;
  - si el juego ya está abierto, avisa en vez de abrir otro.
- El JUGAR de `Configuración.exe` ahora abre `Jugar Yu-Gi-Oh.exe` (ya no `cmd.exe`).
- Los dos .exe llevan datos de versión (producto, descripción, versión 1.0.0.0).
- Pendiente para GitHub: firmar los .exe, o subirlos a VirusTotal y reportar el falso positivo a Malwarebytes.

## 2026-11-10 — Yu-Gi-Oh.exe: el juego mismo se abre solo (sin programa intermedio)
- Malwarebytes marcó también la segunda versión de `Jugar Yu-Gi-Oh.exe`: un exe chico, sin ventana, que lanza otro proceso y se queda leyendo su salida (patrón de "loader"). Red Dead no tuvo el problema porque `Configuración.exe` abre directo `RedDeadRevolver.exe`, que se configura solo (`portable_defaults`).
- Se borraron `Jugar Yu-Gi-Oh.exe` y `Jugar.cs`.
- `main.c` `env_bat_defaults()`, al principio de `WinMain`:
  - Si `RECOMP_PB_BACKEND` no está (no lo abrió `run.bat`), busca `env.bat` en: la carpeta del exe, `ygo-apex\` y `..\..\`.
  - Lee sus líneas `set` y `for … do set`, con `%~dp0` y `%VAR%`, y las aplica con `set_default`.
  - Hace `chdir` a `ygo-apex` y pone el log en `RECOMP_STDIO_LOG = ygo-apex\runs\last_err.txt`.
- CMake:
  - `src/icono.rc`: ícono (`tools/launcher/yugioh.ico`) y datos de versión.
  - Post-build: copia el juego como `Yu-Gi-Oh Port\Yu-Gi-Oh.exe` y `SDL3.dll` al lado (el exe la importa).
- El JUGAR de `Configuración.exe` abre `Yu-Gi-Oh.exe` directo (`UseShellExecute`). Si ya está abierto, no abre otro.
- `run.bat` sigue funcionando igual.
- Compila (rc=0). Falta probar: doble clic en `Yu-Gi-Oh.exe` (que arranque, tome `env.bat` y deje `runs\last_err.txt`) y ver si Malwarebytes lo marca.

## 2026-11-10 — Onda al hacer clic, y voces/texturas opcionales
- `Yu-Gi-Oh.exe` confirmado por el usuario: abre bien.
- `src/onda.c`: al hacer clic izquierdo, un anillo sale de la punta del dedo.
  - Copiado del video del usuario de Power of Chaos (Joey the Passion): un anillo fino y claro que crece unas 5 veces y se desvanece en ~0,2 s, y queda donde se hizo clic.
  - Se dibuja en una ventana propia en capas (UpdateLayeredWindow, alfa por píxel), siempre encima, que no recibe clics ni foco. No es hija de la ventana del juego, para no atar la entrada de dos hilos.
  - Un hilo vigila `GetAsyncKeyState(VK_LBUTTON)`. Solo actúa si el clic es en el área del juego, con el juego al frente y F2 cerrado.
  - El tamaño sigue a la ventana: la mano mide 64 líneas en una imagen de 480. El radio va de 0,12 a 0,42 de eso. El anillo es blanco con un borde oscuro suave, para que se vea también sobre blanco.
  - F2 "Onda al hacer clic" (`onda_clic`, Sí/No), en Controles. También está en el launcher y traducida.
- Voces y texturas son opcionales:
  - sin `modding\voces_latino`, quedan los textos originales y no suena nada; la fila F2 "Velocidad de las voces" ahora no aparece;
  - sin pack de texturas, el juego arranca igual y las filas de Texturas no aparecen (ya era así).
- Compila (rc=0), también el launcher. Sin probar en juego.

## 2026-11-10 — Menú del título: el clic solo sobre "Nueva partida" / "Cargar partida"
- Error que vio el usuario: un clic en cualquier lado elegía y confirmaba una opción. Causa: `raton.c` tomaba toda la mitad de abajo (y ≥ 330), partida en x 320.
- Ahora cada opción es un rectángulo (flecha + texto), en coordenadas del juego:
  - Nueva partida: x 78..300; Cargar partida: x 333..555; las dos en y 352..412.
  - Fuera de los rectángulos, el puntero no cambia la opción y el clic no hace nada (log: `title: click off the options`).
- De dónde salen las medidas:
  - flecha `titl_07_00` (32×24) en x 85 / 340, y 371 + 0..7 (`sub_00111A10`: 0x42AA0000 / 0x43AA0000, y la constante 371.0 en 0x45659C);
  - textos `titl_06` "Nueva partida" 145×33 (160×36 resaltado) y `titl_05` "Cargar partida" 140×33 (155×36), del atlas `euro/sp/title/2d/texdb/texdb000.txt` (v se cuenta desde abajo).
- Para afinar: el log escribe una vez `title piece +XX` con los floats de cada pieza del menú (+0x14..+0x2C), para ver dónde caen de verdad los textos.
- Compila (rc=0). Sin probar.

## 2026-11-10 — Título: clic libre, solo los textos avanzan; cajas con las posiciones reales
- El usuario confirma que el hover de cada opción quedó bien. Pidió que en la pantalla se pueda hacer clic libre (y ver la onda), y que solo un clic sobre un texto avance.
- Posiciones reales del log (`title piece`; +04 = x, +08 = y de cada pieza):
  - "Presiona START": +14 278×27 en (183, 371) y +18 resaltado 330×29 en (155, 365);
  - Nueva: +1C (120, 371) y +20 resaltado (100, 366);
  - Cargar: +24 (375, 371) y +28 resaltado (380, 366);
  - flecha: +2C (85, 371).
- Cajas del menú (estado 11): Nueva x 82..268, Cargar x 337..538, y 362..406.
- "Presiona START": se supone que es el estado 7 (`sub_0002DBA0` espera la tecla; de ahí `sub_00112310` pasa a 8 y después al menú).
  - El clic solo manda A sobre el texto (x 152..488, y 361..398). En el resto de la pantalla no hace nada.
  - Log: `press start: click … on the text / nothing`.
  - Para confirmar el número de estado, el log escribe `title: state N` en cada cambio.
- Los logos (estados 3-5) se siguen salteando con clic, como antes.
- Compila (rc=0). Sin probar.

## 2026-11-10 — Título: el clic del kit bloqueado y los estados correctos; la pantalla completa se respeta
- El clic seguía avanzando en cualquier lado. Log de 02:07:
  - "click off the options (474,52), nothing" y aun así pasó al estado 12: el kit manda el clic izquierdo como A por su cuenta (binding por defecto);
  - "Presiona START" es el estado 10, no el 7: el clic en (524,97) sonó 0x10000002 desde 0x112A93, y después vinieron el 7 y el 11;
  - el estado 3 es la entrada del título (un clic lo pasó al 6).
- `raton.c` `mouse_a_off()`: deja A del ratón en 0 mientras el botón está apretado y 300 ms después, como `deck_pointer`. Solo pasa el A que pone `raton.c`.
  - Se usa en el bloque de "Presiona START" (estados 3, 6 y 10, `s_start_at`) y en el del menú (11).
  - En esas pantallas, solo el clic sobre un texto avanza; el teclado y el mando siguen igual.
- Pantalla completa (el usuario: "puse pantalla completa y me lo abrió en ventana"):
  - se sacó el arranque forzado en ventana del 2026-11-08 en `main.c`; ahora vale lo guardado en F2 o en Configuración;
  - el `fullscreen=No` del ini lo había escrito el juego mientras estaba forzado: hay que elegir Sí de nuevo;
  - F11 ahora también guarda: kit `xbox_MenuSaveSoon()`, que guarda 400 ms después (el cambio se aplica en el flip siguiente), hecho en `xbox_MenuPoll`;
  - se corrigió la ayuda del launcher ("el juego arranca así; F11 cambia").
- Compila (rc=0), launcher también. Sin probar.

## 2026-11-10 — La pantalla completa elegida en F2 no se guardaba
- El usuario confirma que el clic en el título ya funciona.
- Log de 02:12: `[MENU] fullscreen = Sí` y `present.fullscreen=on`, pero el archivo quedó con `fullscreen=No`.
  - Causa: el menú guarda en el mismo instante del cambio, y la pantalla completa (como la resolución) se aplica en el flip siguiente (`nv2a_host_opts_request`). Al guardar, `get_full()` todavía leía el valor viejo.
- Kit `host_menu.c`: después de cada cambio en el panel se vuelve a guardar 400 ms más tarde (`xbox_MenuSaveSoon`), con el valor ya aplicado. F11 lo hace desde el arreglo anterior.
- Compila (rc=0). Sin probar.

## 2026-11-10 — Voz: un toque solo llena el globo; doble toque la desvanece
- El usuario: con un toque se llena el diálogo (eso queda así), pero la voz no se tiene que ir con un solo clic, solo con doble clic; y al irse tiene que bajar de volumen rápido pero suave.
- `voces.c` `ygo_voz_botones()`:
  - un toque de A/B (mando, teclado o clic) ya no corta la voz; el juego llena el globo como siempre;
  - dos toques con menos de 400 ms entre sí (`VOZ_DOBLE`), con la voz sonando, la desvanecen;
  - se mantiene la gracia de 250 ms: el toque que trajo la línea no cuenta.
- Hilo de la voz: antes de cerrar, baja el volumen MCI (`setaudio ygovoz volume`) en 12 pasos durante 300 ms (`VOZ_FUNDIDO`), con curva (1-t)²: cae rápido al principio y termina suave, sin chasquido.
- Log: `[VOZ] N saltado (doble pulsación): voz desvanecida`.
- Si el doble toque además pasa al globo siguiente, la línea nueva reemplaza a la vieja como antes (sin fundido).
- Compila (rc=0), `Yu-Gi-Oh.exe` actualizado. Sin probar.

## 2026-11-10 — Revisión general: pesos, mouse, traducciones, pendientes
- Pesos:
  - voces `modding/voces_latino` 73 MB: 33 MB normales, 23 MB x1.5 y 18 MB x2; 509 voces. Las 23 líneas sin audio son "..." (697–715, 725–727, 744–745, 798);
  - pack de texturas que usa el juego `textures_bc3.pack` 4,9 GB; fuentes escaladas `mejoradas/` 4,2 GB; `dump/` 330 MB (7978 volcadas);
  - sobran para jugar: `textures.pack` 20 GB (intermedio sin comprimir) y `textures_bc3_anterior.pack` 4,9 GB (respaldo);
  - juego original 1,9 GB (lo pone el jugador).
- F2: todas las filas y opciones tienen traducción (lo que falta son números, resoluciones y nombres de idioma). El juego no tiene portugués: el panel y el launcher sí.
- Mouse hecho: título, nombre, menú principal (rueda), piedra-papel-tijera y pantallas de flechas, duelo (campo, botones, cajas, ataque, cementerio), armado del mazo, Biblioteca.
- Mouse sin revisar (andan con clic = A del kit, sin hover propio): Resultados, Opciones del juego, Guardar ("¿Dónde guardar?"), Créditos, Seleccionar oponente / Deck y personaje (posiblemente la pantalla de flechas, estado 9).
- Pendientes conocidos: listas del cementerio sin elegir carta con el puntero (solo rueda); mazo I/II/III desde el lado del mazo; texturas de los `.igb` del disco (las del duelo salen del volcado); C713A94082B5B00B_64x8 no se escala; cortar la voz si el globo se cierra solo; firma o VirusTotal de los .exe; botón Actualizar del launcher cuando haya repo.

## 2026-11-10 — Voz al cerrarse el globo, mazo I/II/III desde el mazo, texturas
- Texturas: el pack que usa el juego ya está comprimido igual que en RDR (BC3, `tools/bc3/bc3pack`): las 7560 entradas en BC3, 4,9 GB. `textures.pack` (20 GB) es la fuente sin comprimir y no se toca. Sin pack (o sin una textura en el pack), el juego usa las texturas originales del disco del jugador; eso ya era así.
- Voz (`voces.c`): el duelo elige la frase en `sub_001008F0` (tabla 0x60F678 + 16·personaje, más un azar entre 3) y se la pasa al globo por número (método +4 de [[obj+0x18]+0x24]), que pide el texto él mismo.
  - Si la línea que suena se pidió 3 o más veces (el globo la redibuja) y deja de pedirse por 300 ms, el globo se cerró: la voz se desvanece (el mismo fundido de 300 ms). Si se pidió una sola vez, no se corta nada.
  - Log: `[VOZ] N: el globo se cerró solo`, `[VOZ] doble pulsación`, `[VOZ] N: voz desvanecida`.
  - El hilo de la voz revisa cada 100 ms (antes 250).
- Mazo I/II/III desde el lado del mazo (`raton.c`): el manejador del mazo (`sub_000DA730`) nunca cambia [0x66D1F0]; solo el menú del baúl lo hace (0xDA484). Un clic en I/II/III del lado del mazo va al baúl (10), cambia el mazo como del lado del baúl ("Cartas nuevas", botón 2) y vuelve (10, `s_deck_back`). Clic en el mazo ya elegido: nada. Log `[RATON] deck: back to the deck side`.
- Sin hacer, falta evidencia: las cartas de la lista del cementerio y las pantallas Resultados, Opciones, Guardar, Créditos y Seleccionar oponente. Las cajas del duelo usan el widget [[0x656F88]+0x58], pero esos menús no (los llamadores de sub_00047850/47A40 son todos del duelo), y no hay capturas de esas pantallas. Hace falta un Ctrl+F9 y una captura en cada una.
- Compila (rc=0), `Yu-Gi-Oh.exe` actualizado. Sin probar.

## 2026-11-10 — Borrado textures.pack (20 GB)
- Pedido del usuario. Era el paso intermedio sin comprimir: `texturas_pack.py` lo rearma entero desde `mejoradas/` cada vez, así que no se pierde nada.
- `crear_pack_texturas.bat` lo vuelve a crear al rearmar el pack y lo borra solo después de un `bc3pack` exitoso (si falla, queda para revisar).
- Quedan `textures_bc3.pack` (4,9 GB, el del juego) y `textures_bc3_anterior.pack` (4,9 GB, respaldo).

## 2026-11-10 — Sin portugués
- Pedido del usuario: el juego no tiene portugués y las voces son en español.
- Las voces y los textos latinos ya iban solo con el español del juego (`voces.c`: idioma 5).
- El idioma del juego nunca tuvo portugués. Se saca de la interfaz:
  - `f2_idiomas.c`: Windows en portugués y un `idioma=pt` viejo en `launcher.ini` dan español;
  - `Configuración.exe`: la lista de idiomas muestra Español, English, Français, Deutsch, Italiano (`LangShown`); `pt` guardado o de Windows → Español. Los textos en portugués quedan en `Tr` para no mover los índices.
- Compila: juego rc=0, launcher sin errores. Sin probar.

## 2026-11-10 — Mouse en Opciones, Guardar y la lista del cementerio (capturas del usuario)
- Capturas 1919×1079 sin franjas = 640×480 px del juego (×3,0 / ×2,25).
- **Lista del cementerio** (`list_pointer`, `sub_000C5710`): carta mostrada x 273..368, y 70..208; la siguiente a la derecha x 395..490, y 91..229; la anterior, lo mismo a la izquierda (x 151..246). Clic en la del medio = A; en la de un costado = un paso hacia ella (el paso propio del juego, como la rueda); en otro lado nada. Fuera del paso 0 (carta ya elegida) el clic sigue siendo A. El A propio del kit queda afuera (`mouse_a_off`). Log `[RATON] list click game x,y -> ...`.
- **Opciones** = `sub_0010C2E0` (thiscall, página = ecx), envuelto. Estado [página+0x40] 3 = espera teclas (0x100/0x200 filas, 0x400/0x800 valor, B sale). Modelo [página]: fila [modelo+0x3C] 0..4; `sub_0010BF20(k)` la marca; sonido `sub_0011A400(0x10000001)`.
  - Filas en px del juego: fila k desde y 173,5 + 44k, alto 44, x 97..543; barra de la fila marcada y 194..219 + 44k, flechas x 97..120 y 520..543.
  - Puntero: la fila bajo el puntero se marca; clic en una flecha = izquierda/derecha; rueda = filas; clic derecho = B. Log `[RATON] options click ...`.
- **Guardar** = `sub_0010E8E0` (thiscall), envuelto. Estado [página+0x54]: 6 espera teclas, 7/8 deslizando; ranura [página+0x48] 0..4; 0x400/0x800 = siguiente/anterior, A guarda, B sale.
  - La ranura elegida está en el medio, x 258..382, y 133..333; las demás cada 150,7 px.
  - Puntero: clic en la del medio = A (guardar); en otra = desliza hasta ella, de a una (la siguiente cuando el juego ya tomó la anterior); rueda = desliza; clic derecho = B. El cuadro Sí/No (estado 10) sigue con el clic de siempre. Log `[RATON] save click ...`.
- Seleccionar oponente ya estaba (flechas por clic, `side_if_gloveless`). Resultados y Créditos siguen con el clic de siempre (sin capturas).
- FPS: todo esto usa el reloj (pulsaciones de 45 ms, B de 80 ms) o se aplica dentro de la función del juego en cada llamada, así que no depende de 60/90/120 fps.
- `rename_pads.py`: + sub_0010C2E0, sub_0010E8E0. Compila (rc=0). Sin probar.

## 2026-11-10 — El clic del mouse cae bien con cualquier resolución, aspecto u opción de imagen
- El usuario: "parece que funciona todo bien" (Opciones, Guardar, cementerio; captura en 16:9 a pantalla completa). Pidió que quede así con cualquier resolución, aspecto u opción.
- Antes `raton.c` (`pointer_pos`) suponía la imagen centrada en 4:3, o 16:9 con "Panorámico". El kit además puede:
  - escalar en múltiplos enteros, con bordes;
  - recortar overscan;
  - agregar bandas hor+ a los costados (el 2D del juego queda centrado entre ellas, `nv2a_aspect_place`);
  - presentar con la cadena de shaders o copia directa.
  En esos casos el clic caía corrido.
- Kit `nv2a_pb_d3d11.c`: cada presentación (`present_blit`, `present_chain`, la copia directa) guarda dónde quedó el cuadro (`pick_set`: rectángulo en el back buffer, parte del cuadro mostrada, escala n, pad, tamaño del juego). Esto es nuevo en el kit compartido; RDR no lo usa.
  - `nv2a_d3d11_window_to_guest(x, y, cw, ch, &gx, &gy, &dev)`: punto de la ventana → px del juego (menos el pad) y px de pantalla por px del juego. Devuelve -1 antes del primer cuadro.
- `raton.c` `pointer_pos` lo usa (y el tamaño del guante sale de `dev`). La cuenta vieja queda solo para antes del primer cuadro.
- `onda.c` hace el tamaño de la onda igual.
- Compila (rc=0). Sin probar con otras opciones.

## 2026-11-10 — Chispas del título que se pierden con F6 (en búsqueda)
- Capturas del usuario: en el título, con texturas originales se ven chispas anaranjadas chiquitas (1–2 px del juego); con F6 (mejoradas) no se ven.
- Descartado, con evidencia:
  - el fondo `titl_02_00` (= dump FDE76864287C3300, el disco invertido en vertical) no tiene esas chispas;
  - `553E66B4DD668253` (las chispas de las antorchas, 2026-11-08) no está en `textures_bc3.pack` (7560 entradas revisadas): se usa la original;
  - búsquedas en todo el volcado (puntos sueltos perdidos al escalar, texturas negras con puntos, alfa con puntos): ninguna son estas chispas;
  - `comm_12_01` (6×6) es una esquina de marco.
- Kit: Ctrl+F9 ahora anota por lote `[DUMP] tex0 hash <hash> up WxH [bc3]`: el hash de la textura (nombre en dump/ y en el pack) y el tamaño subido (más grande = del pack). Con un Ctrl+F9 en el título con F6 activo se sabe qué textura dibuja las chispas.
- Compila (rc=0).
- **Encontradas (Ctrl+F9 del usuario, con y sin F6, 04:01):**
  - el fondo FDE768 es opaco y tapa lo 3D de atrás;
  - encima va un lote de 899 vértices repartido por toda la pantalla (pos 0..654 × 8..494) sobre `F937F533CB06E816` = `title/2d/texdb000` (con F6: 4096×2048 bc3 del pack).
  - Su pieza `menu_06_00` (16×16, dump x 974..990, y 441..457) es un cuadrado negro con una cruz de 5 texels (centro 255, brazos 71) que se suma a la imagen: son las chispas. La IA la dejó borrosa (máximo 202) y en el juego no se ve.
  - El atlas del menú principal `603BA796E198985F` tiene el mismo punto dos veces (`menu_06_00`, `menu_06_01`; dump x 1002..1018, y 427..443 y 445..461).
- `texturas_pack.py`: `KEEP_PIECES` + `keep_pieces()`: esas piezas vuelven como el original agrandado sin IA (bicúbico) después de `restore()`. Probado sobre la escalada: el punto queda con máximo 243 (original 255).
- Pack rehecho: `texturas_pack.py` → `bc3pack` → `textures_bc3.pack`, y se borra el `textures.pack` intermedio. Sin probar en el juego.
- **Confirmado por el usuario:** "quedó genial" (chispas del título con F6). Borrado `textures_bc3_anterior.pack` (respaldo, 4,9 GB) a pedido; queda solo `textures_bc3.pack`.

## 2026-11-10 — Paquete para jugadores y carpeta en C:\Emuladores y Juegos\Yu-Gi-Oh Port
- Pedido del usuario: un .zip para un jugador nuevo y una carpeta lista para jugar, como la de RDR.
- **Modo portable** (`main.c` `portable_defaults`, si junto al exe hay `Game\` con `default.xbe`). Antes
  traía las variables de RDR. Ahora:
  - suma `RECOMP_EXT_VMA=1` (sin ella el heap de Alchemy no consigue memoria y el juego no arranca),
    `YGO_MOUSE_MODE=puntero` y `YGO_MOUSE_STEP=120`;
  - `YGO_AJUSTES` = `<carpeta>\Ajustes`;
  - pack: `textures_bc3.pack` o `textures.pack` en cualquier subcarpeta de `Mods-Extras\`;
  - voces: la carpeta con `textos.txt` en cualquier subcarpeta de `Mods-Extras\`;
  - el guante del puntero sale de `mods_puntero\` junto al exe;
  - en la carpeta de desarrollo nada cambia: no hay `Game\` y vale `ygo-apex\env.bat`.
- Arreglos de paso:
  - el cartel de error decía "junto a RedDeadRevolver.exe";
  - el modo rápido leía `ygo_menu.ini` de la carpeta de trabajo y no de `Ajustes\`.
- **Launcher:** las líneas de estado de texturas y voces buscan también en `Mods-Extras\` (`FindIn`, 4 niveles,
  sin `dump`).
- **`tools\armar_paquete.ps1`** (textos en `tools\paquete\`):
  - `dist\YuGiOh-TDOD-PC.zip`: el programa para un jugador nuevo: `Yu-Gi-Oh.exe`, `SDL3.dll`,
    `Configuración.exe`, LEEME, `ygo_menu.default.ini` (tus ajustes con resolución x2), "Instalar juego desde
    ISO.bat", `Herramientas\extract-xiso`, `mods_puntero\`, `Game\` y `Mods-Extras\` vacías con instrucciones.
    Sin juego, ajustes ni partidas.
  - `dist\YuGiOh-Voces-Latino.zip`: las voces, extra opcional.
  - El pack de texturas (4,85 GB) va aparte, como en RDR.
  - `-Completo <carpeta>`: además, la carpeta para jugar: el programa + `Game\` (copia de `Yu-Gi-Oh TDOD`) +
    `Mods-Extras\texturas\textures_bc3.pack` + `Mods-Extras\voces\` + `Ajustes\` y `Partidas\` de esta
    carpeta.
- **Videos:** el juego no tiene reproductor MP4 todavía. El video de apertura (`.xmv`) se saltea (`videos.c`),
  así que no hay videos para llevar.
- Compila (juego y launcher). Sin probar desde la carpeta nueva.
- **Armado (04:30):**
  - `dist\YuGiOh-TDOD-PC.zip` (40,7 MB) y `dist\YuGiOh-Voces-Latino.zip` (67,5 MB);
  - carpeta `C:\Emuladores y Juegos\Yu-Gi-Oh Port`, verificada contra el original: `Game\` 7272 archivos y
    `default.xbe` igual, pack y exe iguales, 1527 mp3, ajustes y partidas copiados; el zip del programa
    también va ahí, como en RDR.
- Los zip los hace Python (`Zip` en el script): `Compress-Archive` de PowerShell 5.1 guardaba las carpetas
  con `\`. La "ó" de `Configuración.exe` queda bien (0xF3 con la marca UTF-8).

## 2026-11-10 — Revisión para discos mecánicos (lo que se aprendió en RDR)
- El usuario recordó dos arreglos de RDR: abrir y cerrar el pack por cada textura, y liberar RAM en partidas
  largas. Los dos están en el kit compartido (`xboxrecomp-apex/src/kernel/nv2a_tex_pack.c`), así que valen
  para este port. Verificado en el código y en el registro:
  - el pack queda abierto con dos handles (`s_file` para el cuadro, `s_file_bg` para el hilo); antes se abría
    y cerraba por textura;
  - hilo de carga en `THREAD_MODE_BACKGROUND_BEGIN` (prioridad baja de CPU y de disco): mientras tanto se
    dibuja la original;
  - precarga desde `<pack>.usadas` (se reescribe cada minuto) hasta 3/4 del presupuesto;
  - presupuesto `RECOMP_TEXPACK_MB` (1536 MB por defecto): pasado eso, cada 2 s se liberan las texturas sin
    uso hace más de 30 s;
  - los candados ceden (`SwitchToThread`, después `Sleep(1)`): no se mueren las teclas en una PC lenta;
  - todo el pack es arte (tipo 0, BC3), así que nada se lee en el cuadro.
- Registro de la última partida:
  - `[TEXPACK] loader thread on; budget 1536 MB; 768 to preload`;
  - `en el cuadro 0 lecturas; precargadas 762; cambios tardios 7; leido 640 MB`;
  - ningún `[HITCH]`.
- El registro se vacía en cada arranque (`host_stdio_setup`: `"w"` y después `"a"`).
- **Faltaba:** la carpeta `C:\Emuladores y Juegos\Yu-Gi-Oh Port` no tenía `textures_bc3.pack.usadas`, así que
  el primer arranque no precargaba. Copiado, y `armar_paquete.ps1 -Completo` ahora lo lleva junto al pack.
  Al publicar el pack, subirlo con su `.usadas`.
- Sin probar en un disco mecánico real.

## 2026-11-10 — Voz que seguía al cerrar el globo; F2 "Voces" con "No"; botón Actualizar; caras
- **Voz que no se cortaba** (usuario: "si presionas para que se llene el texto y luego haces el click se va
  todo menos la voz"). El registro mostraba cada línea pedida 1 sola vez (`pedido 1 veces`): la detección
  de "globo cerrado" por pedidos seguidos nunca saltaba.
  - El globo es un objeto de texto: `sub_0002B9D0` es su SetText base (vtable 0x427A30 +8, `this` = ecx);
    `[obj+0x17C0]` 0 nuevo, 1 escribiendo, 2 completo (`sub_0004DE00` / `sub_0004DED0`).
    `sub_001008F0` elige la línea (0x13D + tabla 0x60F678.. + azar) y la manda al retrato (vtable 0x4551D0,
    +4 `sub_000F1510` = pedir el texto y SetText).
  - `voces.c`: se guarda el puntero que devuelve `sub_0002C5C0` para la línea con voz; el wrapper de
    `sub_0002B9D0` anota qué objeto la recibió (mismo puntero o mismo texto). En `ygo_voz_botones`, A/B/clic
    con ese globo en estado 2 (texto completo) → la voz se desvanece (`[VOZ] N: globo completo cerrado`).
    Un clic mientras escribe sigue llenando el globo sin cortar la voz; el doble clic sigue igual.
  - `rename_pads.py`: + `sub_0002B9D0`.
- **F2 y Configuración:** "Velocidad de las voces" pasa a ser **"Voces"**: Sigue los FPS / Normal / **No**
  (sin voces; los textos latinos quedan). La clave del ini sigue siendo `voces_velocidad` (no se pierde lo
  guardado).
- **Botón "Buscar actualizaciones"** en Configuración (abajo, entre "Valores por defecto" y "Abrir Ajustes"),
  y la versión debajo del idioma. `tools/launcher/Actualizar.cs` sale del de RDR: Release más nueva de
  GitHub contra `version.txt`; baja el zip, lo copia encima al cerrar Configuración y la vuelve a abrir.
  No toca `Ajustes\`, `Partidas\`, `Game\`, `Mods-Extras\`, `ygo_menu.ini`, `launcher.ini`.
  **Falta el enlace:** `Repo = ""` → el botón dice que las actualizaciones todavía no están disponibles.
  `VERSION` = 1.0 (`armar_paquete.ps1` escribe `version.txt` en el zip). `compilar.ps1` suma Actualizar.cs.
- **Cara del Mago Oscuro deformada por la IA** (fondo detrás de Yami Yugi). F6 en el registro dio lo que
  había en pantalla: `48E137E53FCE3B33` = `cf_anim/bg` (512×256, dado vuelta). `texturas_pack.py`
  `KEEP_PIECES` acepta un 5º valor (borde fundido en px del original): la cara (x 94..148, y 152..194) vuelve
  del original con Lanczos + realce, fundida 5 px con lo escalado.
- **Costuras en ojos y boca** de los retratos. Son `cf_anim/large/png/cflNN_E.png` (27 duelistas × 3
  expresiones): el retrato en y 0..408 y abajo recuadros de ojos/bocas que se dibujan encima. Tabla del juego
  `0x60F518[E*27+NN]` → {rects (y0,x0,y1,x1) int16, cabecera, comandos de 8 bytes (b0, b1, rect, x, y);
  b1 & 0x80 = control}. Cada recuadro se escalaba solo y su borde salía distinto al retrato.
  - `tools/costuras_caras.py`: en la escalada, el borde de cada recuadro (10 px escalados) pasa a la escalada
    del retrato en el lugar donde va, en cada pixel tanto como el original se parecía ahí. Los 81 retratos →
    `mejoradas/caras/` (prioridad en `texturas_pack.py`). Comparado en Yami Yugi: sin líneas.
- Pack BC3 rehecho con las dos cosas. Sin probar en el juego.
- **Sincronía de labios (evaluado, no hecho):** la animación del retrato es un guion de comandos con duración
  al azar (callbacks `[obj+0x58C..]`: `sub_001008B0` 30..329 cuadros, `sub_001008D0` 8..15). Se puede: sacar
  la envolvente de cada mp3 (ffmpeg, 30 por segundo), leer la posición de la voz (MCI `status position`) y,
  mientras suena, elegir el recuadro de boca (cerrada / abierta) por el volumen en el intérprete del guion.
- Confirmado en el registro siguiente: `[VOZ] N: globo completo cerrado` → `voz desvanecida` (el corte anda).

## 2026-11-10 — Target List y botones de la lista del cementerio con el mouse; boca sincronizada con la voz
- **Target List** (usuario: "no anda el click ni en esta ni los botones de abajo"). La maneja `sub_000920B0`
  (el mismo widget de lista que el cementerio, `sub_00047xxx`): paso `[0x65F92E]`, cartas en 0x65F91E,
  teclas `[0x6589E6]`. Paso 2: 0x20 atrás, 0x10 adelante, A → 3. Paso 3 (los dos botones redondos):
  0x10/0x20 cambian, B → 2, A con la lupa (0) → 0xA (información), con el engranaje (1) toma la carta.
  0xA: A o B vuelve. Botón marcado = `sub_00047C60()`.
  - `raton.c` wrapper + `tl_pointer`: carta del medio (x 260..380, y 53..227) → A; costados (150..245 /
    395..490, y 91..228) → paso; rueda → pasos; en el paso 3 pasar el puntero sobre lupa (x 272..315) o
    engranaje (324..367, y 238..280) los marca y el clic los elige (si no está marcado, primero 0x10 y
    después A); clic en la carta = engranaje; clic derecho = B. Registro `[RATON] target list ...`.
- **Lista del cementerio (revivir), paso 1:** los mismos dos botones (`[0x658558] & 3` el marcado, bit 0x20
  = hay dos). Antes cualquier clic era A (elegía el marcado, fuera cual fuera). Ahora puntero encima los
  marca y el clic elige el que está debajo (`[RATON] list buttons: ...`).
- **Boca con la voz:** `tools/voces_envolvente.py` escribe N.env junto a cada mp3 (1527: x1, x1.5, x2), un
  byte de volumen cada 20 ms (255 = percentil 98 de esa voz). `voces.c` lo carga al empezar la voz.
  - Los retratos los anima el animador genérico `sub_00018790` (`this` = ecx): `[+0x4B4]` el guion,
    `[+0x508]` la animación, `[+0x50C]` bits que dejan correr cada pista (bit 1 = boca, lo prende el juego
    mientras escribe el globo); por pista de control `[+0x53C+4t]` el comando y `[+0x514+4t]` los cuadros
    que faltan (la pista fija de la cara no ocupa lugar: boca = lugar 1). Los 81 guiones: boca = control
    bit 1, ciclo reposo → media → abierta → media.
  - Wrapper: mientras suena la voz, en el retrato que el juego hizo hablar en esa línea, el bit 1 queda
    prendido y la boca se elige por volumen (< 40 reposo, < 140 media, si no abierta; cada una al menos
    60 ms). Al terminar o cortarse la voz vuelve la animación del juego (boca cerrada).
  - `rename_pads.py`: + `sub_000920B0`, `sub_00018790`. Copiados exe y .env a la carpeta del jugador;
    zips rearmados (voces 67,8 MB con las envolventes). Sin probar en el juego.

## 2026-11-10 — Papel debajo del ATK/DEF de las cartas
- Usuario: "donde esta el ataque se ve diferente el color que le pusiste para tapar el original". `rellenar()`
  pintaba cada renglón con la mediana de su mitad más clara: salía un recuadro más claro y naranja.
- `texto_cartas.py`: `rellenar_papel()` tapa la franja del ATK/DEF (x 56..114, y 169..174) con el papel de
  la misma carta a esa altura (x 16..54, sin letras), repetido en espejo: mismo color y manchas en cada
  carta (normal, efecto, fusión violeta, ritual azul; comparación en `revision/prueba_atk_colores.png`).
  Nombre, tipo y descripción siguen como estaban.
- 1137 cartas rehechas, pack BC3 rehecho y copiado a la carpeta del jugador.
- **Confirmado por el usuario:** "quedó genial".

## 2026-11-10 — Cara del Mago Oscuro con otro modelo de IA
- Usuario: la cara sin IA (KEEP_PIECES con fundido) "queda fea", baja de resolución al lado del resto.
- Comparados 6 modelos de Upscayl sobre `48E137E53FCE3B33` (`revision/mago_oscuro_modelos.png`):
  digital-art deforma la cara; remacri-4x y ultramix-balanced-4x mantienen ojos y expresión, nítidos.
- `tools/zonas_otro_modelo.py`: por textura, una zona sale de otro modelo (guardado en
  `modding/texturas/modelos/<modelo>/`) y se funde 8 px con la escalada de digital-art →
  `mejoradas/caras/`. La cara (x 84..160, y 140..205) con ultramix-balanced-4x. Sacada de `KEEP_PIECES`.
- Pack rehecho y copiado. Sin probar en el juego.

## 2026-11-10 — Icono del atributo de las cartas en HD
- Usuario: el icono de arriba a la derecha (atributo) se ve deformado; que estén en HD y bien con su carta.
- Revisado: en las 1137 cartas el icono original coincide con su atributo (tabla 0x5EF598, bits 29-31:
  1 luz 光, 2 oscuridad 闇, 3 agua 水, 4 fuego 炎, 5 tierra 地, 6 viento 風; mágicas 魔, trampas 罠). Lo
  que estaba mal era la IA: deformó los kanji.
- `texto_cartas.py` `icono()`: dibuja de nuevo el icono en la escalada. Círculo de 13 px con centro en
  (109.5, 17.5) del .tga (+0,45 px para tapar el halo de la IA), degradé con los colores medidos en los
  originales, brillo arriba a la izquierda, kanji en Yu Gothic Bold con contorno. El liso del nombre ahora
  tapa el recuadro entero (x 12..117), también detrás del icono: antes se veía el corte en x 102 (usuario:
  "tiene otro color el fondo"). Prueba de cada atributo en
  `revision/prueba_iconos.png`.
- 1137 cartas y miniaturas rehechas, pack BC3 rehecho y copiado a la carpeta del jugador. Sin probar en el juego.
- **Confirmado por el usuario** (con 5 cartas: normal, efecto, fusión, mágica, trampa): "quedaron bien".

## 2026-11-11 — Mano de 10 cartas, tirones a 90 fps, clics rápidos, ojos del duelo
- Usuario: tras una invocación por ritual (10 cartas en la mano) el mouse no pasaba de la carta
  seleccionada. `raton.c` conocía 8 lugares en la mano: ahora `HAND_MAX` 16 (arrays de lugares
  aprendidos, `slot_at`, `cursor_now`, `field_learn`). Medido en la captura (10 cartas, centros 97 + 49 i):
  desde 6 cartas el paso es 490 / n, centrado en x 319,5 (coincide con 6 cartas: 115 + 81,7 i). Sin probar.
- Tirones a 90 fps (registro.txt de 187 MB): el juego llama NtWaitForSingleObjectEx ~1,4 millones de
  veces por segundo; el `static int logged; if (logged++ < 20)` del kit se desbordó a los ~26 min (cuadro
  139800) y desde ahí escribía cada llamada con fflush: 2 millones de líneas y tirones de 0,2-0,7 s cada
  vez más seguidos (todos los del final son después de ese punto). Arreglo en el kit (compartido con
  Red Dead): los 41 `x++ < N` de los registros pasan a `(x < N && ++x)`, que dejan de contar.
- Shaders: se compilaban en el cuadro la primera vez (8-270 ms). `tools/armar_shaders.py` arma
  `shaders.pak` (junto al exe) con el cache de esta PC sin los que ya están en el de Red Dead: 131
  shaders. El kit los compila en segundo plano al arrancar. `armar_paquete.ps1` lo incluye.
- Clics: el izquierdo se leía solo con GetAsyncKeyState (si está apretado ahora); un clic corto dentro de
  un cuadro lento se perdía. El gancho del mouse cuenta WM_LBUTTONDOWN y `LDOWN()` (un lugar por
  llamador) da el clic aunque ya se haya soltado, si ese lugar miró en los últimos 250 ms. Seis lugares
  (campo, botones de comando, rueda, flechas, info, el modo puntero general).
- Ojos del duelo: los retratos del duelo son `cf_anim/small/png/cfsNN.png` (256x512, 28), con las 3
  expresiones en el mismo archivo. Tabla 0x60BA48[NN] -> {rects, cabecera, comandos, 3}: rects 0..2 son
  las caras, la cabecera da (cantidad, primer comando) por expresión y cada recuadro va en x, y de la cara
  de su expresión. `costuras_caras.py` ahora hace también estos (28; 109 retratos en total).
  Antes/después en `revision/ojos_duelo.png`.
- exe, shaders.pak y pack BC3 (con los retratos chicos) copiados a la carpeta del jugador. Sin probar en el juego. Red Dead no se recompiló con el arreglo del kit.

## 2026-11-11 — Icono de luz como el oficial
- Usuario: el icono de luz no se distinguía (disco marrón oscuro, el color del juego original). Ahora
  amarillo claro arriba → dorado abajo, kanji marrón oscuro con contorno claro (`ATRIB[1]`, 5º valor = color
  del contorno). Los otros atributos sin cambios (el usuario los ve bien). Cartas, miniaturas y pack rehechos.

## 2026-11-11 — ⭐ VERSIÓN MÁS JUGABLE guardada
- `respaldos/2026-11-11_version_mas_jugable/` (5,2 GB): exe, shaders.pak, Configuración.exe, SDL3.dll,
  fuentes de ygo-apex, diff del kit sobre su commit, herramientas, ESTADO, guía y el pack BC3 (+ .usadas).
  LEEME.txt dice qué tiene. **Es la referencia: si algo se rompe, volver a esta.** (La anterior era
  `2026-11-09_mejor_version`.)

## 2026-11-11 — Turno del rival rápido (F2 / Configuración)
- Fila "Turno del rival rápido": No (por defecto) / x2 / x4; `rival_rapido` en ygo_menu.ini; 5 idiomas en
  F2 y en el launcher (grupo Juego, con explicación).
- `raton.c` `rival_tick` (en cada lectura del mando, cualquier modo de ratón): el reloj del juego va x2/x4
  (`xbox_SpeedSet`, el mismo de las cartas rápidas que ya andaba) mientras, en un duelo, la palabra de acción
  [0x65F948] tiene el bit 15 (el jugador que actúa: bits 0-11 la acción, `sub_000811B0` salta según ellos;
  el bit 15 elige el bloque de jugador de 0x13F8) y nada del jugador espera teclas (cursor del campo, de
  objetivo, botones, cuadros, listas, info: si el rival activa algo y te pregunta, vuelve a normal al
  instante). "En duelo" = alguno de esos corrió en los últimos 20 s. Se apaga 500 ms después de que el rival
  deja de actuar. Registro: `[RIVAL] fast x2 (action ...)` / `[RIVAL] normal after N ms (motivo)`.
- No hay variable de turno confirmada; que bit 15 = 1 sea el rival sale del uso del código, no del juego.
  Si no acelera o acelera tu turno, el registro lo dice. Sin probar.

## 2026-11-11 — Cómo funcionan los efectos de las cartas (investigación, sin cambios)
- Datos de cada carta (índice k 0..1138): nombre y descripción (strings en el xbe), número de imagen
  (u16 en el archivo 0x5EECB2), ATK/DEF/clase/tipo/nivel/atributo (u32 en 0x5EF598). Cambiarlos es fácil.
- **No hay tabla de efectos:** buscado un arreglo de 1139 entradas (u8/u16/u32) que sea 0 en los 533
  monstruos normales y no 0 en las 534 con efecto: no existe. Los efectos están en el código, que compara el
  **número de imagen** de la carta (4007...): 447 números de carta distintos aparecen en comparaciones o
  argumentos, repartidos en ~100 funciones (las que más: `sub_000D5DD0` 87, `sub_0008D420` 45,
  `sub_0008DDC0` 34, `sub_0008E8C0` 33, `sub_000A5F60` 30...: invocación, ataque, fases, IA).
- Qué se puede:
  1. **Reemplazar una carta** (nombre, imagen, ATK/DEF, nivel, atributo, tipo, descripción): parche de los
     datos al arrancar + texturas. Seguro. Un monstruo normal sigue sin efecto.
  2. **Darle a una carta el efecto de otra que ya existe:** en cada comparación con el número de A, aceptar
     también B (el código recompilado es C: un script puede tocar cada `_fb = (uint32_t)(A)`). Factible
     carta por carta; hay que revisar listas de números en datos (IA, mazos) y probar cada una.
  3. **Efectos nuevos:** escribir el efecto en C en la función de la fase que corresponda. Posible, pero
     hay que entender esa parte del motor del duelo; la IA no lo usaría bien.
  4. **Cartas además de las 1139:** no (tablas fijas, partida guardada, sobres, colección).

## 2026-11-11 — Datos de cartas, límites y premios (investigación, sin cambios)
- Direcciones en memoria (.data: archivo 0x5EC000 = VA 0x5FB9E0; .rdata: archivo 0x413000 = VA 0x423080).
  - Imagen por k: u16 en 0x5FE692 (archivo 0x5EECB2). Datos por k: u32 en 0x5FEF78 (archivo 0x5EF598).
  - Ambas tablas se leen de memoria en cada uso (862 y 328 accesos), así que se pueden cambiar al arrancar.
- **Límite de copias:** `sub_0007EF00(id)` busca en 61 u16 en 0x450BC0 (.rdata): bits 0-13 = id, bits 14-15 = copias.
  Si la carta no está en la lista, devuelve 3. Hoy hay 52 cartas con 1 copia, 9 con 2 y ninguna prohibida (el 0 existe).
  La usan el editor de mazo y la IA al armar mazos (`sub_0008C5E0`). Opción del juego "Conf. Las Limi." (Estándar/Personalizado): no revisada.
- **Mazos de la IA:** tabla de 16 bytes por duelista en 0x453630 (mazo variable) y 0x453830 (mazo fijo): {puntero, puntero, cantidades}.
  Cada entrada es u32: u16 id + u16 (bits 13-15 = en qué nivel, 1<<(15-nivel); bits 0-12 = peso).
- **Premio al ganar:** `sub_0008C7E0(duelista, nivel 0..2)` (lo llama 0x8CBD5).
  - Por cada carta de las dos listas del duelista, con su bit de nivel: chance % = máx(5, peso × factor[copias que tenés]).
    Factor en 0x454358: 1,0 / 0,7 / 0,4 / 0,1 (0, 1, 2, 3+ copias).
  - Si sale random(100) < chance, va al bolillero. Se repite hasta juntar 10 o más y se elige una al azar. Peso 0 = nunca sale.
- Lista de 13 cartas "fuertes" en 0x6004E8: la IA del duelo (`sub_00089CB0`), no son límites.

## 2026-11-11 — Mod de cartas (opcional): primera carta, Prisión dimensional (compilado, sin probar)
- Pedido: reemplazar una trampa que casi no se usa por Prisión dimensional, con la ilustración del usuario
  (Escritorio\Cartas que quiero Agregar) rearmada con el estilo del juego.
- Reemplazada: **El Lawbook de Solomon** (imagen 4990, k 879; "Pierdes tu Fase de Espera").
- Fuente: `modding/cartas/cartas.txt` (`[ID]`, nombre, descripcion, ilustracion, efecto) más las ilustraciones.
  `tools/cartas_mod.py` arma `modding/cartas/salida/cartas/` = `Mods-Extras\cartas\` del jugador:
  - `archivos\euro\{sp,en,fr,gr,it}\card\normal\4900\4990.tga`: la carta grande. Sale de la HD de texto_cartas
    con el dibujo en x 18..109, y 40..131 y el texto reescrito, achicada a 128x256 P8.
  - `archivos\card\mini\texdb\texdb000.png`: la hoja de miniaturas; solo cambia el casillero (paleta original sin el color 0).
  - `cartas.pack`: la grande y la hoja en HD (BC3, 6 MB), con las huellas nuevas.
  - `cartas.txt`: nombre, descripción y efecto para el juego.
- Kit (también lo usa Red Dead; los cambios solo agregan):
  - `RECOMP_MODS_DIR` acepta varias carpetas separadas por ';' (kernel_path.c).
  - `RECOMP_TEXTURES_PACK_EXTRA`: un pack chico que se suma al de texturas (nv2a_tex_pack_set_extra). Se lee entero
    al cargar y nunca se libera; si el hash está en los dos, gana el extra. Sin pack principal, carga solo el extra (enhance.c).
- Juego, `src/cartas.c`:
  - `ygo_cartas_init` corre antes del entry point. Pone nombre (texto 849+k) y descripción (1988+k) en los 5 idiomas,
    con el texto en el heap del juego. También cambia la entrada de efectos de la carta.
  - Tabla de efectos: 742 entradas de 24 bytes en 0x601318, ordenadas por ID: {ID, resolver, ?, condición, ?, ?}.
- Efecto `prision_dimensional`: la entrada pasa a {0xAF600, 0, 0xA6810, 0, 0}, que son el resolver y la condición
  de Cilindro mágico. Las dos funciones van envueltas (rename_pads):
  - **Condición:** la carta pasa por Cilindro mientras dura la llamada (0x5FE692[k] = 5124). Para 0x1404 devuelve 1
    si el adversario declaró un ataque.
  - **Resolver:** propio. El contexto trae el atacante en +6 (jugador) y +7 (casilla). Si sigue boca arriba
    (+0x65D0D2 bit 1), llama `sub_000A1F20(k,p,z)` y después `sub_000C69A0(p,z,0)`, que destruye y retira de la
    partida (el mismo camino que Agujero sin Fondo, `sub_000B83E0`). Log `[CARTAS]`.
- F2 y Configuración: "Cartas nuevas" Sí/No, en 5 idiomas. Se aplica al reiniciar y la fila solo aparece si está la carpeta.
  Con No: no cambia los datos ni suma `archivos\`; el pack extra queda cargado, pero sus huellas no aparecen.
- Copiado a la carpeta del jugador: exe, Configuración.exe y Mods-Extras\cartas.
- Para probar:
  - en la Biblioteca y el Editor de Baraja: nombre, imagen, miniatura y descripción;
  - en duelo: colocarla, que el rival ataque y activarla. El atacante se va a "Fuera de juego".
  - Hay que tener la carta. Si no la tenés, el siguiente paso es dar copias por el mod.

## 2026-11-11 — Mod de cartas: Krebons (compilado, sin probar)
- El usuario no tiene El Lawbook de Solomon (Prisión dimensional queda sin probar). Pidió Krebons en lugar de
  **Control de clima** (imagen 4136, k 128; Luz/Hada normal, nivel 2, 600/400), que sí tiene.
- cartas.txt acepta además: `base` (la carta del juego cuyo marco se usa: acá 4634 El aprendiz de bruja,
  Oscuridad/efecto/nivel 2, marco naranja), `atk`, `def`, `atributo`, `tipo`, `clase`. El juego recibe
  `datos = 0x...` (el u32 de 0x5FEF78[k]). Krebons: Oscuridad, Lanzador de Conjuros (no hay Psíquico), efecto,
  1200/400 = 0x4524F028.
- Efecto `krebons`, sin tabla de efectos (el juego no ofrece activar un monstruo al ser atacado):
  - Combate: estado en 0x65F8DE, tabla de handlers en 0x6010F8 (0 = 9A4C0, 1 = 9A540, ..., 6 = 9B5B0, 0xB = 9DD70).
    `sub_0009A290(jugador que ataca)` declara el ataque (6 -> 1).
  - 0x6666E0: bit 0 ataque directo, bits 10-12 casilla del atacante, 13-15 casilla atacada (del otro jugador).
  - LP: u32 en 0x65D0A4 + jugador*0x13F8. `sub_000C8BC0(p, n)` = perder n LP (orden 0x27).
  - Órdenes: `sub_00081150(cmd, a, b, c)` las encola (0x65F950, cantidad en 0x660150). cmd = código | 0x8000 si
    es el jugador 1. 0x1F termina la Fase de Batalla (Anule el ataque), 0x23 niega el ataque de la casilla a
    (Cilindro mágico; estado 0xB).
  - Wrapper `sub_0009A290` (rename_pads): después de declarar, si el atacado es la carta boca arriba y su dueño
    tiene más de 800 LP: pierde 800 y se encola 0x23. Siempre se activa (no hay pregunta). Log `[CARTAS] Krebons`.
- Para probar: en la Biblioteca (imagen, Oscuridad, Lanzador de Conjuros/efecto, 1200/400, texto) y en duelo, con
  Krebons boca arriba atacado: -800 LP y el ataque se cancela.

## 2026-11-11 — Miniaturas borrosas con el mod de cartas, y el Editor de cartas
- Usuario: con el mod, Krebons y las miniaturas de alrededor se veían borrosas.
  Causa: el pack HD del mod no se cargaba nunca. Este build no tiene la capa enhance, así que el pack lo carga
  `host_enhance_init` en main.c (rama `#else`), y ahí no se llamaba `nv2a_tex_pack_set_extra`. El registro decía
  `[TEXPACK] miss 918D27AE091A2203 512x512` y `miss FB1723C4A1E910EA 128x256`: las huellas estaban bien, faltaba el pack.
  Arreglo: main.c llama `set_extra(RECOMP_TEXTURES_PACK_EXTRA)` antes de cargar; sin pack principal, carga el extra solo.
  Las dos hojas texdb000 y texdb006 quedaban enteras sin HD.
- Además, cartas_mod.py escribe las hojas con los mismos bloques del PNG original (`png_igual`: sBIT, PLTE, tRNS de
  256 bytes, tEXt) y solo cambia el IDAT. Antes PIL escribía un tRNS de 1 byte; no era la causa, pero así es más seguro.
- **Editor de cartas.exe** (carpeta del port; `tools/editor_cartas/EditorCartas.cs`, se compila con compilar.ps1, C# .NET 4).
  Es para el usuario (necesita el entorno: Python 3 con Pillow y numpy, el juego en ..\Yu-Gi-Oh TDOD y modding/texturas/mejoradas).
  - Lista las cartas del mod. Para cada una: buscar la carta del juego a reemplazar (muestra su texto y avisa si tiene
    efecto en el juego), nombre, descripción, ilustración (la copia a modding/cartas), atributo, tipo, clase, nivel,
    ATK, DEF y efecto (solo los programados de esa categoría).
  - Guarda en cartas.txt solo lo que cambia respecto de la original. "Crear el mod e instalarlo" corre cartas_mod.py,
    borra y copia `salida/cartas` a `<juego>\Mods-Extras\cartas` y muestra la carta HD creada.
    La carpeta del juego queda en `editor_cartas.ini`.
- cartas_mod.py:
  - `--lista` da el JSON de todas las cartas, los tipos, los atributos y los efectos (con su categoría y su texto).
  - `--vista ENTRADA SALIDA [hd]` convierte cualquier imagen a PNG (.NET no abre .webp).
  - Nuevo campo `nivel`.
  - Sin `base`: si cambia el marco (trampa / mágica / monstruo con su clase y nivel), elige sola una carta del juego
    con ese marco y su versión escalada, prefiriendo el mismo atributo.
- Sin probar: el juego (miniaturas HD, Krebons en duelo) y la ventana del editor (solo compilado).

## 2026-11-11 — Mod de cartas quitado del juego del usuario
- Pedido del usuario: las cartas nuevas no tienen su muñeco 3D en el duelo. Se borró Mods-Extras\cartas de la carpeta del jugador: el juego queda original. Las herramientas y modding/cartas siguen en el port.


## 2026-11-11 — Cierre (0xC0000005) en "Siguiente oponente" (sin arreglar)
- Captura del usuario: Supervivencia, después de ganar ("Siguiente oponente"). Pasa justo al cambiar la música
  (jngl_win1.xau -> sys_select.xau).
- Primero `[STRAY] write at guest 0xFFFFFFFF` y después una escritura en la VA 0, dentro de `sub_00422B22`
  (biblioteca DirectSound: listas de voces/mixbins en memoria contigua 0x80E0xxxx; esi=0x80E01F00, eax=-1).
- Cadena: juego 0x1FE479 (pone volumen/pan, [esi+0xA4/0xA6]) -> 0x41C396 -> 41D39A -> 41DEA4 -> 4207F7 -> 423020 -> 422FB2 -> 422F01.
- Sospecha: eax=-1 es "no hay voz libre" (¿se pierden voces al cambiar de stream?), sin comprobar.
  No tiene que ver con el mod de cartas (el mod no estaba).
- Frecuencia: al usuario le pasó 2 veces desde que juega el port (raro, intermitente).
- Siguiente paso: ver qué devuelve -1 antes de 422B22 y si la cantidad de voces del APU emulado se agota.
- **Corrección del diagnóstico** (misma fecha): 0x41B160-0x423080 es la sección **XPP** (controles USB), no DSOUND
  (secciones del XBE: D3D 0x36DFA0, XNET 0x3914A0, XMV 0x39D9E0, DSOUND 0x3C5CC0, D3DX 0x3EC040, XGRPH 0x3F7060,
  XPP 0x41B160, .rdata 0x423080). 0x41C396 = **XInputSetState** (vibración); la música era coincidencia.
  - Juego: `sub_001FE410` (thiscall, objeto del control: handle +0x44 de XInputOpen del puerto +0x48, motores en
    float +0x2C/+0x30, XINPUT_FEEDBACK en +0x62, rumble en +0xA4/+0xA6 ×65535). Solo llama si el pedido anterior
    terminó (dwStatus != 0x3E5): el juego está bien, falla la pila USB de XPP sobre el OHCI emulado.
  - Arreglo, `src/vibracion.c`: wrapper de sub_001FE410 (rename_pads). Hace lo mismo, pero manda la vibración directo
    con `usb_gamepad_rumble(puerto, izq, der)`, como hace ohci.c al recibir el pedido. Deja dwStatus = 0 y no llama
    a XInputSetState. Respeta RECOMP_RUMBLE=0. Compilado y copiado; sin probar (el cierre era raro: 2 veces).

## 2026-11-12 — Icono de luz con el kanji del usuario; cómo funcionan los modelos 3D
- Usuario pasó la imagen del icono oficial (disco marrón oscuro, aro dorado, 光 blanco). `texto_cartas.py`:
  `ATRIB[1]` = marrón (92,70,46)→(24,19,13), kanji blanco, 6º valor = color del aro (naranja adentro → amarillo
  afuera). El 光 sale de `tools/kanji_luz.png` (recortado de la imagen del usuario; con Yu Gothic Bold los trazos de
  arriba se pegaban y con Medium quedaba muy fino). **Aprobado por el usuario** ("ahí me gustó más").
- 1137 cartas y 10 hojas de miniaturas rehechas; vistazo a las 1137 en hojas de contacto: todas bien. Pack BC3 rehecho.
- **Modelos 3D (investigación, sin cambios):**
  - Archivo: `monster/NNN/mNNN.igb` (carpeta = número/100*100; `sub_0003E9E0(n)` arma la ruta, cadena en .rdata
    0x428B90). Alchemy IGB (ver `texturas_igb.py`).
  - Carta → modelo: tabla en 0x44FBB0, pares u16 (ID de imagen, número de modelo), 971 entradas; `sub_0007E690(id)`
    la recorre (devuelve 0xFFFF si no está). 706 de los 828 monstruos tienen modelo; 122 monstruos no están en la
    tabla; las mágicas/trampas están pero sin archivo (salvo 1).
  - Contenido de un modelo (m001): esqueleto (`igJoint` ~34-49 huesos, `igVertexBlendMatrixListAttr` = piel con
    pesos), animaciones (`igTransformSequence1_5` + listas de cuaterniones/vectores: ~27 pistas), geometría
    (`igGeometry` + `igVertexArray1_1` + `igGeometryAttr1_5` tipo 3 = triángulos), shaders propios de Xbox
    (`igCustomVertex/PixelShaderAttr`) y texturas `igImage` (color 128-256 px; mapas `_s` grises; `ref_map` reflejo).
  - Tamaño: mediana ~5.500 triángulos por monstruo (máx. ~6.900); texturas de 256 px (máx. 512).
  - Lo que ya está mejorado: todas sus texturas (color, gris, paleta, DXT1) escaladas x4 en el pack (2026-11-09).
    Antes/después de Ojos azules (m000) mostrado al usuario.
  - Para una carta nueva: lo fácil es apuntar su ID en la tabla 0x44FBB0 a un modelo que ya existe (o copiar un .igb
    y cambiarle las texturas). Un modelo nuevo de cero necesita exportar a IGB con esqueleto y animaciones de
    Alchemy: no hay herramienta para eso.
- **Exportador de modelos a Blender** (pedido del usuario: meter su propio modelo, 2.500 triángulos, en lugar de uno
  del juego; paso 1 = sacar un modelo del juego con su esqueleto):
  - `tools/igb.py`: lector IGB con los campos de cada objeto (strings = u32 largo + bytes; cada campo alineado a
    min(tamaño,4); el short ocupa 4). **Los bloques de memoria van alineados a 4** (antes se leían seguidos: en los
    archivos con bloques de largo impar, p. ej. 3 huesos por vértice, todo lo siguiente quedaba corrido 1-3 bytes;
    `texturas_igb.py` tiene el mismo error y puede haber dejado afuera texturas de esos modelos: sin revisar).
  - `tools/igb_gltf.py modelo.igb salida`: glTF con mallas, UV, texturas, esqueleto (igJoint, matriz local campo 8)
    y pesos (paleta de huesos por malla = igVertexBlendMatrixListAttr → igMatrixObject de cada hueso, campo 13).
    Triángulos sueltos (tipo 3) y tiras (tipo 4, largos en campo 13); 3, 4 o 5 huesos por vértice.
  - Por ahora solo se exportó **un modelo**: m001 = Duende místico (carta 4008), en `modding/modelos/m001/`
    (21 mallas, 5.238 triángulos, 49 huesos, 5 texturas). Abierto en Blender 5 (render en `vista_previa.png`): se ve
    bien, la piel y el esqueleto encajan. El paso de "todos los modelos" se paró a pedido del usuario.
  - Falta: animaciones (igTransformSequence1_5) y el camino de vuelta (glTF → .igb).

## 2026-11-12 — Sincronizador de Basura: modelo del usuario con huesos y animaciones (vista previa)
- Pedido: la carta Sincronizador de Basura (reemplaza Control de clima) con efecto y su modelo 3D (hecho por el
  usuario con Tripo: 2.293 triángulos, textura 2048, `Escritorio\Cartas que quiero Agregar\Proyecto Sincronizador
  Basura.blend`; la .glb de esa carpeta está vacía, 132 bytes). Mostrar antes de meterlo.
- Copiado a `modding/modelos/sincronizador/original.blend`; carta e imagen a `modding/cartas/`.
- Animaciones del juego: cada modelo trae **una sola** animación en bucle (igTransformSequence1_5 por hueso:
  tiempos ns campo 12, traslación 3, rotación 4 = cuaternión x,y,z,w de la matriz D3D, escala 5; m373 = 2 s, 61
  claves). `igb_gltf.py` ahora la exporta (rotación conjugada en glTF).
- Esqueleto: el de m373 (Tutor de recaudación, Guerrero, 19 huesos: Spine1-3, brazos, piernas con ball/toe/heel),
  mismos nombres y jerarquía, puesto sobre el modelo (x70, tamaño de monstruo del juego):
  `tools/modelos/riggear_sincronizador.py`. Pesos automáticos de Blender fallan (malla de IA, también remallada):
  pesos por distancia a cada hueso (1/d^4, 4 mayores). → `riggeado.blend`.
- `tools/modelos/animar_sincronizador.py`: reposo (60 cuadros en bucle), ataque (30) y daño (24) → `animado.blend`
  y `vista_animaciones.gif`. Mostrado al usuario; esperando su aprobación.
- Falta: carta + efecto en el mod (reemplaza 4136 Control de clima), escribir el .igb (sobre m373: huesos, pesos,
  mallas, texturas, animación de reposo) y enlazar la carta al modelo nuevo (tabla 0x44FBB0). En el juego solo
  se usaría la animación de reposo (es la única que traen los modelos).
- **Aprobado por el usuario** ("Sí, sigue"). Hecho después:
  - **Carta:** [4136] Sincronizador de Basura en `modding/cartas/cartas.txt` (reemplaza Control de clima; antes era
    Krebons): Oscuridad, Guerrero (no hay Cantante), efecto, nivel 3, 1300/500 (`datos = 0x46F50432`), marco de
    4027, ilustración `Foto_sincronizador_de_basura.webp`, texto de la carta del usuario.
  - **Lectura correcta de la piel:** el campo 12 de cada igJoint es la **inversa de bind** (vértice × campo 12 ×
    mundo del hueso). El campo 8 es el primer cuadro de la animación, que puede no ser el bind (m373 salía en T).
    `igb_gltf.py` usa el campo 12.
  - **Escritor:** `igb.IGBEdit` (cambia campos en su lugar y rearma la memoria alineada a 4; cabecera campo 6 =
    largo de la memoria con relleno). Leer y guardar sin cambios da el mismo archivo byte a byte (m373, m004).
  - `tools/modelos/volcar_blend.py` (Blender: malla por esquina, pesos, huesos, 61 cuadros del reposo, textura) y
    `tools/modelos/armar_igb.py` (sobre m373): huesos (campos 8, 12, igMatrixObject), claves de animación (61, 2 s),
    m373all lleva el movimiento de Spine1, igTransform intermedios en identidad; malla en las dos mallas del
    donante con varios huesos (formato 0x30223: 4 pesos 1,0,1,0 = 2 repetidos), partida en 2 grupos (9 y 12
    huesos; máximo del juego 16), las otras 36 fuera de la escena; textura asi.png 512x512 y asi_s gris.
  - Verificado releyendo el .igb nuevo con `igb_gltf.py` y Blender: modelo, textura y animación bien.
  - `cartas_mod.py`: campos `modelo` (el .igb va a `archivos/monster/NNN/mNNN.igb`, NNN de la tabla 0x44FBB0: 4136
    → m129) y `modelo_hd` (textura 2048 al cartas.pack con la huella de la de 512).
  - Instalado en `Mods-Extras\cartas` del jugador (carta, miniatura, modelo, pack HD). Exe sin cambios.
  - **Efecto `sincronizador`: NO programado.** El juego lo ignora (registro "efecto desconocido"). Investigado:
    Monstruo renacido (4842) resuelve con `sub_000ADCF0` y encola la orden **0xA0** (a = u16 bajo de la entrada del
    Cementerio = id | dueño<<15, b = u16 alto). Su handler `sub_000CD130` busca esa entrada (Cementerio: cantidad
    u8 0x65D0AA + p*0x13F8, entradas de 8 bytes en 0x65DC7C + p*0x13F8, compara el u32), llama
    `sub_00046FD0(&lugar)` (lugar = índice<<6 | 0x1C | jugador | bits 14-15) y `sub_000D4EF0` (lo saca del
    Cementerio). Falta: detectar la Invocación Normal de la carta, cómo se elige defensa y casilla, y negar efectos.
  - Sin probar en el juego. Si el modelo da problemas: F2 > Cartas nuevas: No.
- **Efecto `sincronizador` programado** (compilado, copiado; sin probar). "Llamar" = invocar en esta traducción.
  - Disparo: la máquina de la llamada `sub_00094610` (estado 0x66BA8E bits 5-11; k 0x66BA80; descripción
    0x66BA7C: bit 0 jugador, 1-5 casilla, **14 boca arriba, 15 defensa**). En el estado 4 compara la imagen con las
    cartas "al ser llamada" (0x1334 Senju, 0x1293, 0x119A Buscador de dragón, 0x128A Invitación, 0x12BB, 0x1341,
    0x163F) y registra el disparo con `sub_00081EF0`. Envuelta: con la carta, pasa por Senju (4916) durante ese estado.
  - `sub_000C4DE0(k)` (búsqueda del efecto por la imagen de k, envuelta): la carta pasa por Senju → entrada de Senju.
  - `sub_000B1220` (resolver de Senju, envuelto): ctx +0 k, +2 jugador, +4 bit 2 = no; estado 0x66BFBC (0x7D..0x80;
    devuelve el siguiente, 0 = fin). Con la carta: el monstruo de Nivel 1-2 con más DEF del Cementerio propio,
    `sub_000D39D0(p)` (casilla libre), orden 0xA0 y `sub_000961F0(p, &entrada, 1 boca arriba, 1 defensa, 0x20)`
    (lo de Llamada del encantado `sub_000B0970`, que usa 1, 0 = ataque). Devuelve 0x7F y 0 en la vuelta siguiente.
    Siempre se activa si hay a quién (sin pregunta). No niega los efectos del llamado.
  - Registro: `[CARTAS] Sincronizador de Basura ...` (llamado / qué carta llama / sin objetivo / sin casilla).
  - rename_pads: + sub_00094610, sub_000C4DE0, sub_000B1220.

### Sincronizador: modelo roto en el juego (corregido, sin probar)
- Causa: en los .igb los pesos e indices de hueso van k por vertice SEGUIDOS
  (k = digito 0xF00 del formato: 0x?223 = 2, 0x?333 = 3, 0x?443 = 4); el bloque
  mide 4 por vertice y el resto queda en cero. armar_igb.py los escribia de a 4
  (w0,w1,w0,w1) => cada vertice tomaba los huesos de otro (pinchos) y la mitad
  quedaba con peso 0. Arreglado en armar_igb.py y en igb_gltf.py.
- Altura: los pies van en 0 (SUELO). El donante m373 tiene los talones en -22.6
  en el bind, pero su animacion lo sube a 0. Comparado en render con m373.
- Segunda prueba (captura del usuario): el modelo era gigante; la carta entraba
  entre sus pies. El m129 original (Control de clima) mide 22.8 de alto en pose
  y el nuestro 67. Ahora armar_igb.py lo escala a ALTO = 22.8, con los pies en 0.
  Los tamanos de los modelos varian mucho (m001 21, m373 101), asi que
  probablemente el juego escala cada uno segun su numero (sin confirmar en el
  codigo). Por eso se iguala al modelo que se reemplaza.
- igb_gltf.py: lee las tiras de igPrimLengthArray1_1 (3 memoria, 4 cantidad,
  5 bits); antes fallaba con el m129 original.
- Tercera prueba: tamano bien, pero la textura salia en parches. Coincidia con
  un render de prueba con la coordenada v invertida. Las texturas del juego se
  ven bien de las dos formas (no sirven para decidir), y el volcado de la GPU
  tiene las filas en el mismo orden que el .igb. Conclusion: el juego usa v
  como Direct3D (v=0 = primera fila). armar_igb.py ahora escribe 1-v (Blender
  tiene v=0 abajo) e igb_gltf.py ya no voltea v. Sin probar en el juego.
- CONFIRMADO POR EL USUARIO (2026-11-12, "se ve bien el bicho"): en el duelo el
  modelo del Sincronizador se ve bien (forma, tamaño, altura y color). El verde
  era la luz del visor de la Biblioteca, que tiñe a todos los monstruos.

### Sincronizador: efecto como la carta real (2026-11-12, sin probar)
- Ahora pregunta Si/No y deja elegir el monstruo, igual que Senju con su ritual.
  El resolver envuelto (sub_000B1220) sigue los pasos de Senju:
  0x80 lista + pregunta (sub_000880B0) -> 0x7F respuesta en 0x660B8C, mensaje
  (sub_00087E20) -> 0x7E lista en pantalla (sub_000C5AE0) -> 0x7D lo elegido
  -> Llamada Especial en defensa -> 0x7C fin.
- La lista: la de Llamada del encantado (4989) armada por sub_000C1B80
  (envuelto, en rename_pads), filtrada a Nivel 1-2 mientras elige el
  Sincronizador. Entradas de 8 bytes en 0x65855C, cantidad u16 0x6589DC.
- Negar efectos: ninguna carta del juego niega los efectos de un monstruo
  (solo Jinzo, trampas; Orden imperial, magicas). Ese texto se saco de la
  carta, como pidio el usuario.
- Elegir objetivo en el Cementerio (Llamada del encantado, Entierro prematuro,
  Monstruo ojo): campo 5 de la tabla de efectos = sub_000BABA0. Campo 4
  sub_000A3D00 = pagar LP.
- tools/nombres_cartas.py: nombre de una carta por su ID, leido del XBE
  (lang 5 espanol, 1 ingles).
- Textos de la pregunta y del mensaje: propios, solo en espanol.

### Sincronizador: el efecto no salía al Invocar de Modo Normal (corregido, sin probar)
- El usuario lo invocó de Modo Normal desde la mano y no pasó nada. El registro no tenía la línea
  "Sincronizador de Basura llamado".
- Causa: `sub_00095A30` elige la máquina de la llamada por el tipo (0x66BA8E bits 2-4). La
  **Llamada Normal es `sub_00093FA0`** (tipo 1). En su estado 2 compara con Senju
  (0x12BB + 0x79 = 0x1334) y registra el disparo. Solo estaba envuelta `sub_00094610` (tipo 2, estado 4).
- Arreglo (`cartas.c`, `rename_pads.py`): `sub_00093FA0` también está envuelta. En el estado 2, con
  0x66BA80 == k, la carta pasa por Senju. El registro ahora dice el tipo:
  `[CARTAS] Sincronizador de Basura llamado (jugador p, tipo t)`. Compilado y copiado.
- Para probarlo: el usuario tiene **una sola copia** del Sincronizador y tarda mucho en robarla (media hora la
  última vez). Le ofrecí una opción de prueba con copias extra y **dijo que no**: no hacerla ni volver a
  ofrecerla. Antes de pedirle otra prueba, revisar bien el código para no hacerle perder tiempo.

## 2026-11-12 — Escudo drenador en lugar de Raigeki de inconformista (compilado, sin probar)
- Pedido: Techo de agujas no se agrega porque ya está en el juego como "Cielo Raso con Pinchos" (5442, mismo
  efecto). En su lugar va **Escudo drenador** sobre **Raigeki de inconformista** (4855, k 753), que el usuario tiene.
- Efecto `escudo_drenador` (`cartas.c`): la entrada de efectos pasa a {0xAF600, 0, 0xA6810, 0, 0}, la de Cilindro
  mágico, igual que Prisión dimensional (la condición ya envuelta deja pasar las dos cartas). El resolver es propio:
  - orden 0x23 con la casilla del atacante (niega el ataque, como Cilindro);
  - `sub_000D8130(p, z)` da el ATK del atacante;
  - `sub_000C8DF0(dueño, ATK)` = ganar LP (el camino de Medicina roja).
  - Log: `[CARTAS] Escudo drenador: ...`.
- Mod rehecho (3 cartas: 4990, 4136, 4855) y copiado con el exe a la carpeta del jugador.
- Para probar: colocarla y, cuando el rival declare un ataque, activarla. El ataque se cancela y se suman LP iguales
  al ATK del atacante.

## 2026-11-12 — Ciclón del Cosmos en lugar de El Guerrero que Regresa Vivo (compilado, sin probar)
- Reemplaza **El Guerrero que Regresa Vivo** (5330, k 1056; mágica).
- Efecto `ciclon_cosmos` (`cartas.c`): copia la entrada de **Tifón de espacio esotérico** (4909):
  {0xAB150, 0xA35D0, 0xA6C90, 0, 0xBD420}. Con eso el juego ya sabe elegir la carta del campo.
  - Condición `sub_000A6C90`, envuelta: con esta carta, además, el dueño tiene que tener más de 1000 LP.
  - Resolver `sub_000AB150`, envuelto: el objetivo está en ctx +0xC (jugador) y +0xD (casilla). Hace las mismas
    comprobaciones que Tifón (anulada; ctx +0xA & 7 == 1; casilla con carta). Paga 1000 LP con
    `sub_000C8BC0(dueño, 1000)` y destierra con `sub_000C69A0(p, z, 0)`. Ese camino también sirve para las
    casillas 5-10 (mágicas y trampas), así que destierra de verdad y no hizo falta mandarla al Cementerio.
  - Los LP se pagan al resolver, no al activar: en el juego da lo mismo, salvo que la nieguen.
- Mod: 4 cartas (4990, 4136, 4855, 5330). Exe y mod copiados.
- Para probar: con una mágica o trampa en el campo, activarla y elegir la carta. Tiene que restar 1000 LP y la
  carta tiene que ir a "Fuera de juego". Log `[CARTAS] Ciclon del cosmos`.

## 2026-11-12 — Ciclón: los 1000 LP al activarla; Golpe de Viento de Ala de Fénix (compilado, sin probar)
- El usuario confirmó que en la carta real los 1000 LP se pagan al activarla, aunque después la nieguen.
- **Campo 4 de la tabla de efectos = el costo**, que el juego paga al activar. Ejemplos:
  - `0xA3D00`: LP según la carta (Criterio solemne, Monstruo ojo...);
  - `0xA3F90`: Mundo de Toon, 1000 LP con `sub_000C8EC0`; no depende de la carta;
  - `0xA3FB0`: descartar según la carta; Magic Jammer (4862) = 1, Destino final = 5. Usa `sub_00084E10`;
  - `0xA3DD0`: ofrecer como tributo.
- Ciclón: campo 4 = 0xA3F90. El resolver ya no paga, solo destierra.
- **Golpe de Viento de Ala de Fénix** reemplaza a **Investigación mística** (4944, k 837; trampa). Efecto
  `golpe_fenix`: la entrada de Tifón con el campo 4 = 0xA3FB0. Funciones envueltas:
  - `sub_000A3FB0`: la carta pasa por Magic Jammer (descarta 1).
  - `sub_000A6C90` (condición): el adversario controla alguna carta (casillas 0-10) y el dueño tiene mano
    (u8 en 0x65D0A8 + p*0x13F8).
  - `sub_000A35D0` (casilla válida; args ctx, u16 p | z<<8): cualquier carta del adversario.
  - `sub_000BD420` (elegir; devuelve 1 cuando `sub_000BAA20(ctx,p,z)` anotó el objetivo):
    - la CPU (bit 0 de ctx+2 = 1) toma la primera carta del adversario;
    - el jugador: como Tifón (estado 0x66BFC1; B = 0x6589E6 & 2; `sub_00092E20(máscara)`; cursor en
      0x6633D4/D8/DC). Cambia el texto propio, el cursor arranca en un monstruo del adversario si lo hay
      (`sub_00086F50(p, zona 0|5, casilla)`), y la máscara es 0xFE << (16 * adversario).
    - Máscara de `sub_00092620`: 16 bits por jugador. 0xF0 = monstruos (0x10/0x20 boca abajo/arriba,
      0x40/0x80 posición), 0x0E = mágicas/trampas.
  - `sub_000AB150` (resolver): `sub_000C8470(p, z)` devuelve la carta del campo a la Baraja. Monstruo
    recuperación y otras mandan después la orden 0x3F (barajar); sin ella queda arriba. **Sin comprobar.**
- Mod: 5 cartas (4990, 4136, 4855, 5330, 4944). Exe y mod copiados.
- Para probar:
  - Ciclón: los LP bajan al activarla.
  - Golpe: activarla en el turno del rival o en el propio, descartar 1, elegir un monstruo o una mágica/trampa
    del rival y ver que vuelve a su Deck (que la robe en su próximo turno).
  - Log: `[CARTAS] Golpe de viento...`.

## 2026-11-12 — Pruebas del usuario: Escudo drenador funciona; Ciclón al Cementerio
- Registro de la partida: `[CARTAS] Escudo drenador: ataque del jugador 1 (casilla 2) negado; el jugador 0 gana
  1700 LP`. **Escudo drenador funciona** (pendiente que el usuario lo confirme).
- Ciclón del Cosmos: el usuario pidió que mande la carta al **Cementerio**, no que la destierre. Ahora el resolver es
  el de Tifón tal cual (destruye) y el texto dice "envíala al Cementerio". El costo de 1000 LP sigue al activarla.
- Mouse, reportado por el usuario y **sin arreglar**:
  - al entrar a elegir personaje, a veces la selección sube una fila;
  - después de estar un rato en el editor de mazo, al ir a elegir oponente los personajes no se mueven con el
    mouse (con el teclado sí).
  - El registro no alcanza para saber en qué pantalla fue. Pantallas sin guante: `side_if_gloveless` (raton.c)
    solo activa las flechas si no hay guante (`s_gx < 0`). Sospecha sin comprobar: queda un guante del editor
    de mazo.
- Pendiente: límite de 1 copia para Golpe de Viento (envolver `sub_0007EF00` con un campo `copias` en
  cartas.txt).
- **Límite de copias (hecho, sin probar):** cartas.txt acepta `copias = 0..3` (cartas_mod.py lo pasa al juego).
  `sub_0007EF00` está envuelta: para esas cartas devuelve el límite del mod, y si no, la lista del juego. Golpe de
  Viento: `copias = 1`. Log al arrancar: `[CARTAS] 4944: hasta 1 copias en el mazo`.

## 2026-11-12 — Cerezas Naturia (reemplaza Kageningen 4113, modelo m106) — sin probar
- Carta: [4113] en `modding/cartas/cartas.txt`: Tierra, Planta (no hay Cantante), efecto, nivel 1, 200/200,
  ilustración `Foto_cerezas_naturia.webp`. Kageningen era un monstruo normal flojo (800/600).
- **Efecto `cerezas`** = el de **Momonga ágil (4922)** (idea del usuario): al ir al Cementerio por batalla,
  copias del Deck boca abajo en defensa. Diferencias: sin los 1000 LP y tope de 2.
  - Disparo: `sub_000C7C50` (carta al Cementerio; arg 4 = puntero a la entrada, k bits 0-14) compara la imagen
    con 0x133A. Envuelta: con las cerezas, la carta pasa por Momonga mientras corre.
  - `sub_000C4DE0`: con las cerezas, la entrada de Momonga (resolver `sub_000B13A0`, compartido con 0x1339 y
    Plaga de Bubonic 0x136A).
  - `sub_000B13A0` (estado 0x66BFBC; tabla de saltos 0xB1738 = 0x7D,0x7E,0x7F,0x80): 0x80 da 1000 LP → 0x7F
    lista del Deck con la misma imagen (`sub_000C1B80`), casilla libre, pregunta (CPU: sí) → 0x7E saca la carta
    (`sub_000C7070` → 0x66C130) o baraja (orden 0x3F) si dijo No → 0x7D `sub_000961F0(p, 0x66C130, 0 boca abajo,
    1 defensa, 0)` → 0x7F (repite). Envuelta: 0x80 sin LP; los demás con la carta como Momonga y la Momonga real
    sin imagen (no entra en la lista); a la 2ª invocación encola barajar y devuelve 0.
  - Solo por batalla (la condición de Momonga). La carta real también se activa por efectos del rival: no hecho.
  - Registro: `[CARTAS] Cerezas Naturia ...`. rename_pads: + sub_000C7C50, sub_000B13A0.
- **Modelo** (`modding/modelos/cerezas/`, .blend del usuario hecho con Tripo, 3.410 triángulos, textura 4096
  JPEG empaquetada): no es humanoide. `tools/modelos/riggear_cerezas.py` usa los mismos 19 huesos (nombres y
  padres de m373) pero pesa solo 3: Lbicep = cereza izquierda (+Y), Rthigh = la derecha (un hueso en cada grupo
  de malla de armar_igb), Spine2 = tallos de arriba y hojas; las dos cerezas cuelgan del cruce de los tallos.
  `animar_cerezas.py`: reposo = flotan (Spine1) y se hamacan a destiempo. `volcar_blend.py` no guardó la textura
  (estaba empaquetada en JPEG): sacada a mano a `color.jpg` → `textura.png` 2048.
  `armar_igb.py` ahora acepta `--alto=N`: las cerezas van con 16 (m106 de Kageningen mide 21, pero las cerezas son
  más anchas que altas). Releído con igb_gltf y renderizado: forma, textura y animación bien.
- Compilado, mod e exe copiados al jugador. Sin probar en el juego.
- Primera prueba del usuario: el modelo se ve bien y la animación también, pero las cerezas quedaban pegadas en el
  medio (la mezcla de pesos en |y| < 0,04 estiraba un puente). Las dos cerezas y los tallos son UNA malla soldada
  donde se besan, y la cara de una cruza y=0. Arreglo en `riggear_cerezas.py`: cada vértice es de la cereza a la
  que llega primero caminando por la superficie (Dijkstra, vértices de igual posición unidos, semillas = la mitad
  de afuera de cada una); sin mezcla; cada cereza 0,03 hacia afuera; los 59 triángulos de la unión, fuera.
  Revisado en render: separadas, sin estirarse. Mod copiado. Sin probar en el juego.

## 2026-11-12 — Cartas del mod en los 5 idiomas (sin probar)
- Pedido: que las cartas nuevas se vean en el idioma del juego (imagen y texto).
- Textos oficiales en inglés, francés, alemán e italiano de las 6 cartas, sacados de la API de YGOPRODeck
  (`db.ygoprodeck.com/api/v7/cardinfo.php?id=...&language=fr|de|it`; Yugipedia da 403). Adaptados a lo que
  hace nuestra versión, igual que el español: Ciclón al Cementerio (no destierra), Cerezas solo por batalla,
  Sincronizador sin anular efectos. Comillas rectas y "Phoenix" (sin „“ ni œ por las dudas con la letra del juego).
- `cartas.txt`: claves `nombre_en/fr/de/it` y `descripcion_en/fr/de/it` (opcionales; si faltan, el español).
  El Editor de cartas conserva las claves que no conoce.
- `cartas_mod.py`: dibuja una carta por idioma (euro/sp, en, fr, gr, it) con su texto; el tipo sale de las tablas
  del juego (índice 3432 + tipo; tablas 1 en, 2 de, 3 fr, 4 it, 5 es) y las etiquetas ([WARRIOR/EFFECT],
  [CARTE PIÈGE], [ZAUBERKARTE]...) son las del juego (tabla ETIQUETAS). Cada idioma tiene su HD en el pack.
  Las miniaturas (texdb, sin idioma) siguen con la de español. `texto_cartas.escribir` acepta `sufijo` y `etiqueta`.
- `cartas.c`: lee esas claves y pone cada texto en su tabla (`poner_texto_en`), sobre el español.
- Compilado, mod e exe copiados. Revisadas las 30 imágenes. Sin probar en el juego.

## 2026-11-12 — Texturas por idioma: un pack por idioma (sin probar)
- Problema (usuario, jugando en inglés): casi nada mejorado salvo las cartas del mod. Causa: el pack tenía solo
  lo del español; en cada idioma las cartas grandes (otro texto) y 8-10 hojas de menús (título, menú, duelo,
  mazo, comunes y la del modo puntero) tienen otra huella.
- **Menús:** `texturas_disco.py` ahora vuelca también euro/en, fr, gr, it (no las cartas) y
  mods_puntero/euro/<todos>: 39 hojas nuevas, escaladas con digital-art-4x y pasadas de `mejoradas/` a
  `modding/texturas/menus_idiomas/` (para que no entren en el pack grande si se rehace).
- **Cartas:** `tools/idiomas_pack.py`: no se escalan de nuevo; la carta en español escalada con nombre, tipo,
  etiqueta y descripción escritos con los textos del juego en ese idioma (tablas: nombre 849+k, desc 1988+k,
  tipo 3432+tipo; `%cNNNN` con los nombres de ese idioma). 1137 por idioma (52 sin datos, como en español). La
  original del idioma va a dump/ (transparencia del pack). Salida `modding/texturas/idiomas/<xx>/` y
  `textures_<xx>.pack` (texturas_pack + bc3pack, ~3,4 GB de temporal, no 20).
- **Kit** (`nv2a_tex_pack.c/.h`, `enhance.c`): `RECOMP_TEXTURES_PACK_MORE` = packs separados por `;` (hasta 8),
  leídos como el grande (solo el índice al cargar, cada textura de su archivo al usarla: campo `src` de la
  entrada, archivos propios por pack); si una huella está en dos, gana el adicional; el `EXTRA` (cartas.pack del
  mod, entero en memoria, ≤512 MB) sigue entrando último. `main.c` busca textures_es/en/fr/de/it.pack en
  Mods-Extras.
- **Español aparte:** `tools/separar_espanol.py` copia del pack grande (sin descomprimir, bytes BC3 + índice
  nuevo) las texturas cuya huella está en euro/sp y en ningún otro idioma → `textures_es.pack` (1198, 883 MB);
  el grande queda con lo común (6362, 3,99 GB). Si se rehace el grande con crear_pack_texturas.bat, volver a
  correrlo.
- Packs: bc3 (común) 4,0 GB; es 0,88; en 0,85; fr 0,84; de 0,85; it 0,84. Verificados (índice ordenado, cada
  textura dentro del archivo). Copiados con el exe al jugador. Sin probar en el juego.
- Falta: el armado del zip/paquete para distribuir tiene que llevar los packs nuevos (cada idioma opcional).
- **Primera prueba del usuario (inglés):** cartas y logo de Konami borrosos, y las miniaturas con los nombres en
  español. Causas: (1) este exe no tiene la capa enhance; el pack lo carga `main.c` (host_enhance_init) y ahí
  faltaba `nv2a_tex_pack_set_more` (el registro solo decía "extra pack"); (2) las 10 hojas de miniaturas
  (card/mini) son las mismas para todos los idiomas (misma huella), y las HD tenían las cartas en español.
- Arreglo: `main.c` carga **solo el pack del idioma elegido**: lee `language=` de `YGO_AJUSTES\ygo_menu.ini`
  (English/Español/Français/Deutsch/Italiano; el menú se carga después de los packs) o `RECOMP_LANGUAGE`;
  `RECOMP_TEXTURES_PACK_MORE` = `textures_<xx>.pack;cartas_<xx>.pack` (el segundo gana). Cada pack de idioma
  lleva sus 10 hojas de miniaturas (`miniaturas.py --idioma=xx`, a `idiomas/xx/miniaturas/`, opacas), que ganan
  sobre las del pack grande (en español quedan las del grande).
- **Mod de cartas en los 5 idiomas:** `cartas_mod.py` arma las 4 hojas de miniaturas con cartas nuevas una vez
  por idioma (las cartas nuevas de ese idioma y las demás de `idiomas/xx/` o texto_cartas) → `cartas_es/en/fr/
  de/it.pack` (22 MB c/u) junto a `cartas.pack` (cartas grandes de los 5 idiomas y texturas de los modelos).
- Packs: bc3 4,0 GB; es 0,88; en 0,90; fr 0,89; de 0,90; it 0,89 GB. Todo copiado con el exe. Sin probar.
- **CONFIRMADO POR EL USUARIO (2026-11-12, "se ve bien ahora"):** en inglés las cartas, menús y miniaturas se ven mejorados con los packs por idioma.

## 2026-11-12 — Limpieza de espacio (pedido del usuario: 30 GB entre las dos carpetas)
- Los 6 packs (textures_bc3/es/en/fr/de/it.pack) estaban dos veces (modding\texturas y la carpeta del jugador
  Mods-Extras\texturas): verificados iguales (MD5) y la copia de modding\texturas reemplazada por un **enlace
  duro** al del jugador (un solo archivo en disco, dos rutas): −8,4 GB. Ojo: rehacer un pack que se escribe
  como archivo nuevo (separar_espanol.py usa replace; bc3pack puede recrearlo) corta el enlace: después de
  rehacerlo, copiarlo al jugador y volver a enlazar (`New-Item -ItemType HardLink`).
- `respaldos/2026-11-11_version_mas_jugable/texturas` (el pack de antes de separar el español, 4,9 GB) borrado
  con permiso del usuario; el respaldo queda con exe, código, kit y herramientas (256 MB). Su contenido está en
  textures_bc3 + textures_es; el exe viejo de ese respaldo solo carga textures_bc3.pack (sin el español).
- Quedan: carpeta del jugador ~11 GB, carpeta de trabajo ~12 GB en disco (sin contar los packs enlazados).

## Pendiente para publicar (decidido 2026-11-12: se arma al final, cuando esté todo y con más cartas)
- Se sube a Google Drive o MediaFire (como Red Dead). MediaFire gratis: tope por archivo de unos 4 GB; el pack
  común pesa 4,29 GB → conviene achicar antes (tope 1024 en las de 2048 y BC1 en las opacas: ~2-2,5 GB por
  jugador con su idioma).
- Archivos separados, cada zip con su carpeta Mods-Extras\... adentro: programa (~41 MB), texturas común,
  texturas por idioma (es/en/fr/de/it), voces latino, mod de cartas (con cartas.pack y cartas_<xx>.pack).
- Falta actualizar `tools/armar_paquete.ps1` y `tools/paquete/LEEME*.txt` (hoy: un solo pack, sin el mod de cartas).

## 2026-11-12 — Descripción y tipo de las cartas sin "sticker" (todas rehechas, sin probar en el juego)
- Usuario (cartas en inglés): la descripción y la línea del tipo se veían de otro tono, como pegadas: se tapaban
  con un color liso (la mediana del papel). Igual en español (no lo había notado).
- `texto_cartas.py` `limpiar_papel()`: borra las letras de la IA con un cierre (MaxFilter y MinFilter de 4k+1,
  por canal) y un GaussianBlur(k): queda el papel de esa carta (tono, degradé, manchas). Se usa en la
  descripción, en la línea del tipo ([FIEND]) y en la etiqueta de mágicas/trampas. El nombre y el ATK/DEF
  siguen como estaban. Primera prueba: se borraba la línea del borde derecho (el recuadro llegaba a x 116,
  donde empieza el borde: 116-118); ahora x 12..115 (etiqueta 30..115). Ejemplos de los 6 tipos aprobados.
- `idiomas_pack.py` ahora también arma el español (`es`): texto_cartas.py, miniaturas.py y las 1198 texturas
  solo del español desde su mejor versión en mejoradas/ → textures_es.pack (con sus miniaturas, que ganan sobre
  las del pack grande). Sin argumentos hace los 5 idiomas.
- Rehechos: textures_es (1208), en (1157), fr (1156), de (1157), it (1156) y el mod de cartas. bc3pack escribe
  sobre el mismo archivo: los enlaces duros con la carpeta del jugador siguieron (packs ya actualizados ahí).
  Packs verificados; mod copiado.
- Pendiente (al achicar antes de publicar): sacar del pack grande las 10 hojas de miniaturas en español (~55 MB),
  que ahora están en textures_es.

## 2026-11-12 — Versión jugable con idiomas (respaldo)
- Usuario: "lo probé y parece bien… ponela como jugable". Verificado sin el juego: cada idioma tiene sus menús,
  sus 10 hojas de miniaturas y las 4 del mod; cartas: todas en español, en/fr/de/it menos las mismas 52 imágenes
  sin datos (no están en la lista de cartas del juego: no aparecen). Registro: textures_de + cartas_de cargados.
- Respaldo: `respaldos/2026-11-12_version_jugable_idiomas` (328 MB: exe, kit diff, tools, fuentes, mod de
  cartas y modelos; sin los packs, que se rehacen con las herramientas). Ver su LEEME.txt.

## 2026-11-12 — Mouse: caja de posición, pestañas Principal/Auxiliar del mazo, barras de desplazamiento
- Pedido del usuario (4 capturas): la caja "Selecciona la posición de la carta" no tomaba el mouse al abrirse (solo la rueda la movía); en el armado del mazo, después del botón "Deck Principal/Deck Auxiliar" el mouse no hacía nada ni volvía atrás; y poder arrastrar la barra de la izquierda.
- **Caja de posición:** no usa los manejadores de las cajas del duelo (sub_00087A80/878C0). `sub_00088160(flags, texto, dibujo, teclas)` abre cajas con manejadores propios: posición = texto 0xD2C, teclas `sub_00093EE0` (paso [0x660B9A] 0 llena el widget con Pos. Ataque / Pos. Defensa, 1 espera teclas: 0xC0 cambia [0x660B8A], 1 = A). Envuelta (rename_pads) con `ask_box(2, 1, …)`: el mismo marcado y clic que las otras cajas, esperando en el paso 1. En esta caja la rueda ya no mueve la barra (como en las otras cajas del duelo), así que no se desfasa.
  - Otras cajas con manejador propio, sin mouse todavía: Tipo (0xD26, 0x90030), Nivel (0xD35, 0x90300), Atributo (0xD27, 0x904B0), Descartar (0xD20, 0x911C0), Orden de cartas (0xD2A, 0x91A90). Tienen más de 5 líneas (el widget se desplaza) o son otra cosa; faltan capturas.
- **Fila de pestañas del mazo** (lado del mazo, parte 0): la pone el botón 1 del menú "Nada" (0xDB684, guarda la pestaña en [0x66D1FC]). Teclas: 5/7 dan vuelta la pestaña mostrada ([0x66DB7A + 2d] = 1 − ella), 14 la deja (estado 9), 13 vuelve a la que había y al menú (parte 10). Mouse: la pestaña bajo el puntero se muestra (tecla 5), clic en una pestaña = esa (14), clic en otro lado o clic derecho = atrás (13).
- **Barras de desplazamiento** (clic sostenido y arrastrar): objeto de barra (sub_000E3480(filas, visibles) / sub_000E3510(primera)): +0x1C x, +0x2C arriba del recorrido, +0x38 largo, +0x3C largo del cursor, +0x40 filas, +0x44 visibles. Baúl [escena+0x1264], grilla del mazo [escena+0x1340] (escena = edi del manejador), Biblioteca [escena+0x1260].
  - Baúl: hasta 6 filas desliza como la rueda; más lejos pone la fila de arriba y rellena la lista como al cambiar de pestaña ([0x66DBC0] = [0x66DBC4] = 8, [0x66DBBC] |= 0x20000000, estado 0xC). Sin números válidos de la barra, la medida en la captura (x 35..42, y 114..414).
  - Grilla del mazo: el paso de la rueda, una fila a la vez.
  - Biblioteca: salto directo con el redibujado instantáneo de la rueda (wrapper de sub_00038220).
  - Log: `[RATON] <lugar>: scroll bar grabbed at …` con los números de la barra, para ajustar si no coincide.
- Compila (rc=0); `Yu-Gi-Oh.exe` copiado a las dos carpetas. Sin probar.
- **Las otras cajas de `sub_00088160`** (revisadas en el código, a pedido del usuario: "no puedo escanear cada centímetro del juego"):
  - "Selecciona Tipo." (0xD26, `sub_00090030`, 20 líneas) y "Selecciona Atributo." (0xD27, `sub_000904B0`, 6 líneas): esperan con [0x660B9B] == 1 y [0x660B9A] ≠ 0; 0x40 línea anterior, 0x80 siguiente (sin dar la vuelta), A responde si el widget está quieto (`sub_00047A20`). Con más de 5 líneas el widget (0xF0384) muestra solo la línea marcada, en su objeto de línea 0, con una flecha arriba y otra abajo.
  - "Declara el nivel del monstruo" (0xD35, `sub_00090300`): número 1..12 en [0x660B8A]; 0x20/0x40 uno menos, 0x10/0x80 uno más, A responde. No mientras elige la CPU ([0x65F91C] & 4).
  - Mouse (`spin_box` / `spin_pointer`): rueda arriba = línea anterior (o un nivel más); clic en la línea mostrada = responder, encima = anterior, debajo = siguiente (45 px); en el nivel, clic = responder. El A del kit (botón izquierdo) queda afuera, así un clic nunca responde lo que estaba marcado antes. El A inyectado se repite hasta que el widget lo toma ([0x660B8C] marcado con 0xFFFF antes de la llamada).
  - Log: `[RATON] list|level box: shown line at …` y `spin box click …`, para ajustar las zonas de las flechas.
  - "Descarta de tu mano." (0xD20) elige con `sub_00092E20`, el cursor de objetivo que ya tiene mouse. "Cambia el orden de tus cartas" (0xD2A, `sub_00091A90`: 5 cartas, [0x66C124] la elegida, 0x20/0x10 elegir, 0x200/0x100 mover, A listo): sin lugares en pantalla todavía, queda con teclado.
  - Compila (rc=0); exe copiado a las dos carpetas. Sin probar.

## 2026-11-12 — Cerezas Naturia: modelo más chico (sin probar)
- Captura del usuario en batalla (registro: m106 contra m488 del rival, efecto be110): las cerezas tapaban la pantalla.
  En la escena de batalla el juego muestra los dos monstruos de cerca (el m488 original, 35 de alto, también sale grande).
- Medidas (igb_gltf, pose de bind): Kageningen original 19×20×4 (casi plano); cerezas con `--alto=16`: 20×16×14,
  mucho más hondas, así que se acercaban a la cámara.
- Rearmado con `--alto=10`: 12,8 × 10 × 8,8. Misma forma y textura (render revisado).
  Copiado a `modding/cartas/salida/.../monster/100/m106.igb` y al Mods-Extras del jugador.
  El de 16 quedó en el temporal por si hay que volver.

## 2026-11-12 — Cerezas Naturia: más chicas otra vez (sin probar)
- Captura del usuario 18:33 (registro: m106 contra m590, be110), ya con el modelo de alto 10. La bola verde con cara
  es el m590 del rival (original, solo 12 de alto, render revisado): el juego muestra la batalla muy de cerca.
  Las cerezas son las hojas de abajo a la izquierda: el monstruo propio queda en primer plano, cortado por el borde.
- Comparación (igb_gltf): Kageningen original 11,6 × 5,6 × 21 de alto; las cerezas de alto 10 ya eran más chicas.
- Rearmado con `--alto=6.5` (7,9 × 5,7 × 6,5). Instalado en salida y Mods-Extras. El de alto 10 quedó en el
  temporal (`m106_alto10.igb`).

## 2026-11-12 — Cerezas Naturia: flotan alto (sin probar)
- Captura 18:48 con alto 6,5 (registro: be105, m106, m918, be110): las cerezas ya no se veían. Las tres capturas
  muestran lo mismo: el monstruo propio sale en primer plano abajo y la cámara mira desde arriba. Solo entra en
  cuadro lo que está alto (Kageningen mide 21); las cerezas, a ras del piso, quedaban cortadas o afuera.
  Achicarlas no servía. Los modelos del rival se ven bien (m590 de 12 de alto, m918).
- `armar_igb.py --suelo=N`: la base del modelo a esa altura (huesos y animación incluidos).
- Rearmado con `--alto=11 --suelo=8`: las cerezas flotan de 8 a 19 (más o menos donde está el cuerpo del Kageningen),
  13 × 9,7 de ancho y fondo. En el campo van a flotar sobre la carta. Instalado en salida y Mods-Extras.
  Anteriores en el temporal: `m106_alto10.igb`, `m106_alto65.igb`, `m106_alto16.igb`.
- Prueba del usuario (captura 117): "casi listo", se ven las dos cerezas pero cortadas abajo a la izquierda; pidió
  un poco más arriba y un poco más chicas. Rearmado con `--alto=8.5 --suelo=11` (de 11 a 19,5). Instalado. Anterior:
  temporal `m106_alto11_suelo8.igb`.

## 2026-11-12 — Batalla: cámara y cómo están hechos los monstruos (investigación, sin cambios)
- Tablero `duel/model/duel_m_00.igb`: nodos `monster_p0..4_c_point` (x -8,6 / -4,3 / 0 / 4,3 / 8,6; z +7,7, del
  jugador) y `monster_e0..4_c_point` (z -7,7, giradas 180°), magias en z ±12,2, cementerio/mazo/fusión/destierro.
  `null7`/`null8` en (0, 22,7, 38) giradas -46° en X: la vista normal del tablero desde el lado del jugador.
- La batalla carga los dos modelos en `sub_00077860` (objeto +0x350/+0x354, `sub_0001ADF0` = cargar igb), y
  `sub_0007D230` (estado [obj+0xCC], tabla 0x7D848) los pasa a los visores [0x656F88]+0x74/+0x78 (`sub_0005A670`
  los cuelga del nodo +0x30). Efecto: `sub_0005F8C0`, be = (t-3)/6·10 + (t-3)%6 con t = `sub_0005D250(carta)`
  (efecto por atributo/tipo; no tiene cámara propia). Los monstruos se ven parados en su casilla del tablero.
- La cámara de la batalla no se encontró todavía (no está en archivos: duel/, effect/battle_igb no tienen
  cámaras; sería código del módulo 0x77000-0x7E000).
- **Cómo están hechos los originales** (64 modelos, pose del primer cuadro, Blender; tabla en
  `modding/modelos/medidas_originales.txt`): la mayoría apoya en el piso (base ≈ 0), de 7 a 51 de alto (la mitad
  entre 15 y 27). Varios flotan: m590 de 2,7 a 11; m919 de 7,8 a 16,3; m900 de 8,2 a 29,9; m874 de 17,9 a 19,9;
  m448 desde 12,8. Kageningen (m106) de 0,1 a 19,9, 4,7 de fondo. Las cerezas (11 a 19,5) siguen esa misma idea.
- Ojo al medir: `render_gltf.py` mete en los límites una "Icosphere" de Blender en -1..1 (no es del modelo), y los
  valores crudos del glTF (accesores) no sirven: hay que medir la malla ya deformada.

## 2026-11-12 — Tres cartas nuevas con efecto: Golem Centinela, Hombre de Nieve Comilón, Des Lacooda (sin probar)
- Reemplazan (las de las fotos del usuario): Espejo perverso 4267 (modelo m260), Sectario de los secretos 4274
  (m267), Víbora de llama 4650 (m643). Ilustraciones `modding/cartas/Foto_*`; textos en 5 idiomas en cartas.txt.
- **Efectos "como otra carta"** (`cartas.c`, `alias()`): mientras corren ciertas funciones del duelo, la imagen de la
  carta (0x5FE692[k]) pasa a ser la de una carta del juego; fuera de ellas (dibujo, mazo, partida) es la suya.
  - fx: `sub_000C4DE0` (la búsqueda en la tabla de efectos; casi todos los lectores de la tabla —C4E50, A1990…— le
    pasan k y la usan), la Llamada de Volteo `sub_00094AF0` (máquina de llamada tipo 3; switch por imagen),
    el resolver de Des Lacooda/Skelengel `sub_000ADDE0`, las casillas válidas de Come Hombres `sub_000BB9E0` y del
    Guardia del Guardatumba `sub_000BAFE0`.
  - turno: `sub_000CFE70` (repaso del comienzo del turno): a las cartas 0x1527-0x1528, 0x152A-0x152E, 0x1533, 0x1535,
    0x158E les pone 0x2000 en su casilla (+0x65D0D2): se pueden poner boca abajo ese turno.
  - Des Lacooda (4650) = 5427 (fx y turno). Hombre de Nieve (4274) = Come Hombres 4507 (pedido del usuario: mismo
    efecto; el del juego destruye cualquier monstruo, también boca abajo). Golem (4267) = Guardia del Guardatumba
    5511 (VOLTEO: 1 monstruo del adversario a la mano) y en el turno, Des Lacooda (boca abajo 1 vez por turno).
  - rename_pads: + sub_00094AF0, sub_000ADDE0, sub_000BB9E0, sub_000BAFE0, sub_000CFE70. cartas_mod.py: efectos
    `des_lacooda`, `hombre_nieve`, `golem_centinela`.
  - Riesgos a mirar: si la carta se ve con el dibujo de la otra mientras se voltea o resuelve; si el menú ofrece
    "boca abajo" al Golem (depende de que el menú solo mire la marca 0x2000).
- **Modelos** (.glb de Tripo del usuario, ~3.300 triángulos, miran a +X): `tools/modelos/riggear_rigido.py` (19 huesos
  de m373, el modelo rígido: abajo Lthigh, arriba Spine3; reposo = Spine1 sube y baja y se balancea un poco) →
  volcar_blend.py → armar_igb.py. Altos: golem 22, nieve 15, lacooda 17 (base en el piso). En
  `modding/modelos/golem|nieve|lacooda/`. Renders revisados.
- Compilado (rc=0), mod rearmado (cartas_mod.py) y copiado con el exe a la carpeta del jugador.
- **Corrección (usuario):** Des Lacooda ya está en el juego (5427): no se agrega repetida. Sacada del mod (4650 vuelve
  a ser Víbora de llama original; sus archivos y m643 borrados de Mods-Extras). El modelo queda en
  `modding/modelos/lacooda/` por si se usa para la 5427 (no tiene modelo en el juego). Quedan Golem y Hombre de Nieve.
- **REGLAS del usuario:** NUNCA agregar una carta que ya esté en el juego (buscarla antes); el texto de la carta tiene
  que coincidir con el efecto programado (Hombre de Nieve: "selecciona 1 monstruo en el Campo, sin importar su
  posición", como Come Hombres; cambiado en los 5 idiomas). Mod rearmado y copiado.

## 2026-11-12 — Cartel de premios del rival en "Seleccionar oponente" (compilado, sin probar)
- Pedido del usuario (opción 1 de 3): un cartel chico al elegir rival con cuántas cartas puede darte, cuántas te
  faltan y de cuántas tenés menos de 3. `src/premios.c` (nuevo, en CMakeLists).
- Datos (leídos del código): premio `sub_0008C7E0(d, nivel)`: listas 0x453830 y 0x453630, 16 bytes por rival
  (u32 lista[2] +0, u16 cantidad[2] +8), índice r = `sub_0008C530(d)` (0/1 por el avance: filas de 0x4541F0 que
  nombran a d, menos 1); entradas u16 ID | u16 (bits 0-12 peso, bit 15-nivel: en qué nivel del rival puede salir);
  chance = máx(5, peso × factor[copias]) %. Tus copias: u8 0x651E70 + 0xCC1 + k (`sub_000D95C0`).
  El premio lo llama `sub_00025BC0` con d = s8 0x423D60[personaje] (modo 0: [0x66DCDC] == 0; si no, sub_0008C9B0).
- Pantalla de elegir rival = `sub_00106CE0` (thiscall; la ayuda 0x10A/0x10B "Arriba/Abajo: nivel"): casilla
  +0x3F0 (+0x3EC), personaje +0x424 + 4·casilla, nivel +0x4DC + 4·[+0x3F4] (`sub_001037D0` los pasa al duelo).
  El envoltorio de raton.c llama `ygo_premios_tick(objeto)` en cada cuadro; cuenta las cartas distintas con peso y
  el bit del nivel. Registro: `[PREMIOS] slot .. personaje .. rival .. nivel .. variante ..: N cartas, faltan ..`.
- Dibujo: ventanita propia por capas (como onda.c), arriba a la derecha del juego, panel marrón con borde dorado,
  2 renglones en el idioma del F2 (f2_idiomas.c: `f2_idioma()`), solo mientras esa pantalla corre, el juego al
  frente y F2 cerrado. "Este rival no da cartas" si la lista queda vacía.
- Opción "Cartel de premios del rival" (cartel_premios, Sí/No) en F2 y Configuración.exe, 5 idiomas.
- Sin probar: que los campos de la pantalla sean esos (ver el registro [PREMIOS] al mover rival y nivel) y la
  posición del cartel. Exe y Configuración.exe copiados al jugador (el juego estaba cerrado).
- **Prueba del usuario (2026-11-12):** el cartel le gustó. Pidió "Ya tienes" en lugar de "Con menos de 3": ahora
  "Te faltan: N    Ya tienes: M" (M = cartas distintas del rival que ya tenés), 5 idiomas. El registro mostró que
  +0x3F4/+0x4DC no es el nivel (siempre fuera de 0..2: es otra lista de la pantalla): el cartel cuenta lo que el rival
  puede dar en cualquier nivel. Anduvo con todos los rivales de la pantalla (casillas 0-9); se recalcula en cada
  cuadro, así que suma solo al ganar cartas. La opción ya estaba en F2 ("Cartel de premios del rival").
- **Golem: efecto 1 (VOLTEO, a la mano) CONFIRMADO por el usuario.** Efecto 2 (boca abajo) no andaba: los comandos de
  una carta del campo salen de `sub_00098EF0` (bit 0x10 "boca abajo" si la imagen está en 0x1527..0x1535, tabla de
  saltos 0x991F8, y la casilla tiene 0x2000), que lee la imagen por su cuenta. Envuelta con el alias de turno (el
  Golem pasa por Des Lacooda); rename_pads + sub_00098EF0. Compilado y copiado, sin probar.
- Cartel (pedido del usuario: "suavizarle los bordes"): el panel se dibuja por la distancia al borde de un
  rectángulo redondeado (contorno y esquinas con antialias, borde dorado de fh/16 px); las letras con antialias en
  gris (sin ClearType), mezcladas sobre el panel. Compilado y copiado, sin probar.

## 2026-11-12 — Vidente (reemplaza Víbora de llama 4650, modelo m643; antes Macis de tecla 4198, que el usuario no tenía) — compilado, sin probar
- "Vidente" no está en el juego (solo "Kappa vidente", otra carta). La original del usuario (excavar 2, 1 a la mano,
  la otra abajo) no existe en el juego; el usuario aceptó "roba 2 y devolvés 1 al Deck barajado" (opción 1).
  Texto en los 5 idiomas igual a lo que hace: "VOLTEO: Roba 2 cartas, y después devuelve 1 carta de tu mano al Deck
  y barájalo." Nivel 1, Agua, Lanzador de Conjuros, 100/100. Macis de tecla: normal floja, nada depende de ella.
- Efecto (`cartas.c`, `vidente()`): alias fx a Skelengel 4546 (VOLTEO, entrada con resolver sub_000ADDE0); el
  envoltorio de sub_000ADDE0 corre el resolver propio para su k. Estados en 0x66BFBC: 0x80 roba 2
  (`sub_000C8EF0(p, 2)`, como la Caridad sub_000AD620 roba 3); 0x7F pedido modo 6 `sub_00084D00(p, 6, 0, 0)`
  (el de El centinela enérgico sub_000B0110); 0x7E `sub_000C6D90(p, u16 0x65F930, 1)` (mano → Deck) y orden 0x3F
  (| 0x8000 si p = 1) = barajar; devuelve 0x64.
- Pedidos ([0x65F91C]: jugador bit 2, modo bits 4-9; cantidad 0x65F91E; banderas 0x65F920) los atiende
  `sub_00085350` (switch 0x85668): modo 1 descartar (`sub_00091280`, Caridad), modo 6 `sub_00090AA0` = elegir de la
  mano del ADVERSARIO (CPU: sub_0008A690; jugador: texto 0xD29, orden 0x8098, cursor sub_00092E20 máscara 0x10000,
  lo elegido 0x6633DC → 0x65F930). Envuelta: mientras Vidente elige, la propia mano (CPU: su última carta; jugador:
  texto propio, máscara 0x1, sin 0x8098). rename_pads + sub_00090AA0.
- Modelo: Vidente.glb (3.377 triángulos) con riggear_rigido.py, alto 18. Render revisado.
- Riesgo a mirar: que el cursor vaya a la mano con la máscara 0x1 y que con el mouse se pueda elegir una carta de
  la mano (si no, con teclado/mando).

## PENDIENTE (decidido con el usuario 2026-11-12): 3 mejoras, cada una OPCIONAL (Sí/No en F2 y Configuración, 5 idiomas)
1. **Quién da cada carta:** en la Biblioteca, al mirar una carta que no tenés, un cartel "La dan: Mako, Bandit Keith…"
   (mismas listas que el cartel de premios: 0x453830/0x453630 por rival, `premios.c`; nombres de los rivales en los
   textos 86+d).
2. **Premios sin repetidas:** al ganar, no dar cartas de las que ya tenés 3 (salvo que no quede otra). Lugar:
   `sub_0008C7E0` (factor por copias en 0x454358: 1,0/0,7/0,4/0,1, mínimo 5 %).
3. **Aviso de carta nueva:** en la pantalla del premio, una marca "¡Nueva!" si no la tenías.

## 2026-11-12 — Las 3 mejoras de premios (compiladas, SIN PROBAR) — `premios_extra.c` + `cartel.c`
Cada una con su fila en F2 y en Configuración (6 idiomas):
1. **Quién da cada carta** (`quien_da`: "Nombres y foto" por defecto / "Solo nombres" / "No"; pedido del usuario: opción de
   nombres solos o nombre + foto). En la Biblioteca (modo grilla, `[0x66E684]` = 0), sobre un lugar de una carta que NO
   tenés, cartel arriba (13 % del alto) del lado contrario a la columna del cursor: "Te la pueden dar:" + rivales.
   - Lugar de la grilla → carta: el lugar n es la n-ésima i (0..0x472) con u16 `save+0x3DC+2i` ≠ 0 (así arma
     0x66DD20 el código en 0x10F4B6); k = i+1; tenida si u8 `save+0xCC2+i` ≠ 0 (mismo test del juego).
   - Rivales: personajes 0..27 (nombres = textos 86-113 en el idioma del juego), d = s8 `0x423D60[pers]`, sus 2
     listas de premios (variante actual, cualquier nivel). Los personajes 28+ (d 27/29/30/31) no tienen nombre: solo log.
   - Fotos: `Game/cf_anim/small/png/cfsNN.png` (NN = personaje, misma orden que los nombres), primera cara = 256x128
     de arriba, leída con WIC. Log `[QUIEN]` y `[CARTEL] retrato N`.
   - Llamado desde el wrapper de `sub_0010F790` (raton.c) después del pad.
2. **Premios sin cuarta copia** (`premios_sin_cuarta`, Sí por defecto). Descubierto: la pantalla de premio
   (`sub_00025BC0`) sortea `[obj+0x3C]` ranuras (1/2/3/5) llamando `sub_0008CBB0(flag, d, ranura)` (→ `sub_0008C7E0` o
   `sub_0008C9B0`); el "nivel" de las listas es en realidad la ranura. Wrapper de `sub_0008CBB0` (rename_pads): si sale
   una carta con copias ≥ 3 (u8 0xCC1+k) vuelve a sortear (mismo esp: ret y args siguen) hasta 200 veces; si siempre
   sale repetida queda la última. Log `[PREMIOS] ranura ...`.
3. **Aviso de carta nueva** (`aviso_nueva`, Sí por defecto): las ranuras con copias 0 quedan anotadas; un hilo mira el
   cofre cada 100 ms y cuando esa carta pasa a > 0 muestra arriba al centro "¡Carta nueva!" + nombre (texto 849+k) 4 s.
- `cartel.c`: motor genérico de carteles (mismo estilo que premios.c), 2 ranuras, ventanas en capas propias.
- A probar: si el cartel tapa algo en la Biblioteca, si las fotos se ven, si el aviso sale en el momento correcto.

## 2026-11-12 — Clic en las flechas de "Seleccionar oponente" entraba al duelo (video 22-53-27) — compilado, sin probar
- Causa: en las pantallas de flechas sin guante (`side_pointer`: elegir oponente, piedra-papel-tijera) el kit manda el
  clic izquierdo como A por su cuenta; raton.c mandaba la flecha (d-pad) pero no sacaba ese A → mover + confirmar.
- Arreglo: `mouse_a_off(n, t)` en `side_pointer` antes de su propio A (el del clic en el medio sigue andando).
- En esa pantalla no hay guante del juego: el mouse solo sirve con clics (flechas a los lados, medio = elegir).

### 2026-11-12 — Cartel "Te la pueden dar" (Biblioteca): ahora en cartas que TENÉS, siempre a la izquierda
- Pedido del usuario: en las cartas que no tenés se ve el dorso sin nombre, así que el cartel no servía. Ahora sale solo en las cartas que tenés (quién te la dio / quién te da más copias), y siempre del lado izquierdo (`c.lado = 1`). premios_extra.c `ygo_quien_da_tick`. Sin probar.

### 2026-11-12 — Voz del Robot (Duel Computer) con efecto de máquina
- Elegida por el usuario entre 4 muestras: "Robot clásico" (ffmpeg `afftfilt` con la fase en 0: voz plana, sin entonación; +8.5 dB y limitador para quedar pareja).
- `voces_mod.py`: las frases que dice solo el Robot (`es_robot`: todas sus partes son "Robot"; hoy 833–841) llevan `ROBOT` después del recorte de silencios, también en x1.5/x2. `voces_latino/robot.txt` anota cuáles ya lo tienen, para rehacerlas si cambia. Las de Kaiba (830–832, 842–848) no se tocan. Los originales de `dialogos/audio` no se tocan.
- Envolventes rehechas; las 9 (y sus x1.5/x2) copiadas a `Mods-Extras/voces` del jugador. Sin probar en el juego.

### 2026-11-12 — Confirmado por el usuario
- Cerezas Naturia (carta del mod, [4113]): "se ve bien". La "puerta" funciona en todos los turnos (el usuario no dijo el nombre; si es Golem Centinela, su vuelta boca abajo cada turno anda). Falta probar Vidente.

### 2026-11-12 — Hombre de Nieve se destruyó a sí mismo (investigando)
- Registro de esa partida: volteo (comando 7) → al elegir objetivo, el clic sobre el propio Hombre de Nieve (p0 z0 i0) fue rechazado ("target: click ... dropped"), y el usuario eligió el monstruo del rival (p1 z0 i0, clic + A). No fue un clic sin querer. Después habló el Robot (836, "recibe daño").
- El resolver de Come Hombres es el de Tifón (`sub_000AB150`, destruye la carta de ctx +0xC jugador / +0xD casilla, puesta por `sub_000BAA20`).
- Agregado registro `[CARTAS] resolver de la carta k … (como …)`: dueño, objetivo y carta en esa casilla, para las cartas "como otra". Exe copiado. Falta que el usuario lo repita y mande el registro.

### 2026-11-12 — Hombre de Nieve "corrido": centrado por el cuerpo
- El modelo estaba centrado por su caja, y un brazo/rama larga de un lado la corría: el cuerpo quedaba ~7 % del alto hacia -Y (cabeza y bola de abajo fuera del centro).
- `armar_igb.py --cuerpo`: centra X/Y por la mitad entre los percentiles 5 y 95 de los vértices (mueve malla y huesos igual). Nieve: x -0.009, y -0.068 (unidades de alto 1). Rearmado con `--alto=15 --cuerpo`; el anterior quedó en `modding/modelos/nieve/m267_antes_centrar.igb`.
- Mod de cartas rearmado y copiado a Mods-Extras/cartas (igual a salida/cartas). Sin probar.

### 2026-11-12 — REGLA de tamaño de los modelos nuevos: la silueta del original (`armar_igb.py --como=mNNN`)
- Capturas del usuario en batalla: el Hombre de Nieve (alto 15) tapaba media pantalla. Medidos (Blender, primer cuadro): Sectario m267 12,2 × 9,1 de frente; el muñeco 15 × 16,1 (casi el doble de ancho).
- Regla: el modelo nuevo ocupa la misma silueta de frente (alto × ancho) que el original que reemplaza, centrado por el cuerpo (`--cuerpo`, lo pone `--como`), pies en 0. La cámara de batalla está pensada para el original. Con esta regla el Sincronizador (aprobado) habría salido 20,4 de alto: estaba en 22,8.
- `--como=mNNN` lee `modding/modelos/medidas_originales.txt` (agregados m260, m643; medidor: igb_gltf + Blender, `bounds_bl.py` en el temporal). Si falta el original, la herramienta para y pide medirlo.
- Rearmados: Hombre de Nieve `--como=m267` → alto 10,2 (antes 15); Golem `--como=m260` (Espejo perverso, 28,8 × 30,2) → alto 29,8 (antes 22); Vidente `--como=m643` (Víbora, 15 × 16,2) → alto 16,7 (antes 18). Anteriores: `*_antes_como.igb` en cada carpeta. Cerezas (flotan, aprobadas) y Sincronizador no se tocan.
- Mod rearmado y copiado a Mods-Extras/cartas. Sin probar.

### 2026-11-13 — Hombre de Nieve: flota y a la derecha (pedido del usuario)
- Captura con `--como=m267` (alto 10,2): ya no tapaba la pantalla, pero en la batalla salía abajo a la izquierda, cortado. Las cerezas (flotan) se ven enteras.
- `armar_igb.py --derecha=N`: corre malla y huesos N a la derecha de la cámara propia (juego +X = Blender +Y).
- Rearmado: `--alto=9 --cuerpo --suelo=8 --derecha=4` (medido: de 8 a 17, centro de ancho +4,6). En el tablero flota sobre la carta, corrido a la derecha. Anterior: `m267_como.igb`. Mod copiado. Sin probar.

### 2026-11-13: ajuste en batalla (capturas del usuario)
- Hombre de Nieve: se cortaba arriba → `--alto=9 --cuerpo --suelo=6.5 --derecha=4` (de 6,5 a 15,5).
- Golem (puerta): se cortaba abajo → `--alto=27 --suelo=2.5` (de 2,5 a 29,5; antes 0 a 29,8). Anterior: `golem/m260_antes_subir.igb`. Mod copiado. Sin probar.

### 2026-11-13: Carga del Poderoso (Burden of the Mighty) reemplaza a Mooyan Curry (4344, k 327)
- No existía en el juego. Mooyan Curry (mágica normal, +200 LP) pasa a Mágica Continua: clase 2 en 0x5FEF78 y su entrada de efecto con el resolver vacío de Estandarte del Valor (0xC4DC0, el de todas las continuas) (`cartas.c`, efecto `carga_poderoso`).
- El ATK: envoltorio de `sub_000D5DD0` (cálculo de ATK/DEF de una carta del campo; ahí suma Estandarte con `sub_000D3320(p, 0x15A2)`). Arg2 = jugador | casilla<<8, arg3 salida (+0 k, +4 ATK, +8 DEF), arg4 bit 0 = ATK. Si el monstruo (casilla 0-4) está boca arriba, pierde 100 × Nivel × (Cargas boca arriba y no negadas del otro jugador, contadas como `sub_000D32A0`), mínimo 0. También vale para la CPU contra vos.
- Ilustración `modding/cartas/Foto_carga_del_poderoso.webp` (foto del usuario); textos oficiales en 5 idiomas. Exe y mod copiados. Sin probar.
- Limitada a 1 copia en el mazo (`copias = 1`, pedido del usuario: rompía el juego).

### 2026-11-13: marca "Nueva" en la Biblioteca (pedido del usuario)
- Cada carta que te dan y no tenías (la misma detección que el aviso "¡Carta nueva!") queda marcada; en la grilla de la Biblioteca (modo 0) lleva una pastilla roja con borde dorado "Nueva/New/Nova/Nouvelle/Neu/Nuova" centrada en su borde de arriba (carta col c, fila r: centro 94+40c, 120+57r px del juego, 35×51; raton.c). Se borra al abrir esa carta (modo 2). Guardadas en `YGO_AJUSTES\cartas_nuevas.txt`. Mismo interruptor que "Aviso de carta nueva".
- `cartel.c`: `ygo_cartel_marcas()` + una ventana en capas del tamaño del juego; la conversión px del juego → ventana sale de `nv2a_d3d11_window_to_guest` (afín: dos esquinas). `premios_extra.c`: `marcas_tick()` desde `ygo_quien_da_tick`.
- Solo marca cartas ganadas desde ahora. Exe copiado. Sin probar.
- Pendiente: "Nueva" sobre la carta en la pantalla de premios: falta captura de esa pantalla.

### 2026-11-13: Drillago reemplaza a Mapache de tortuga (4509, k 488, modelo m502)
- No existía en el juego. Oscuridad / Máquina / Efecto, nivel 4, 1600/1100. Ilustración `Foto_drillago.webp`, textos oficiales en 5 idiomas.
- Efecto (`drillago`): envoltorio de `sub_00099980(jugador, casilla)` (¿puede atacar directo? switch por imagen: Lámpara mística 4393, Jinzo #7 4428...; lo usan jugador y CPU). Devuelve 1 si las casillas 5..10 del adversario están vacías y cada monstruo suyo está boca arriba con ATK (`sub_000D8130`) ≥ 1600.
- Modelo: `modding/modelos/drillago` (Drillago.glb del usuario, 3.396 triángulos), riggear_rigido → volcar → `armar_igb.py --como=m502 --cuerpo` (m502 medido: 15,3 × 15,2 → alto 17,0, ancho 13,7). Render revisado.
- Exe y mod copiados. Sin probar.
- Cambio (usuario): solo cuentan los monstruos del adversario (todos boca arriba con 1600 ATK o más); sus mágicas y trampas no. Texto cambiado en los 5 idiomas. Exe y mod copiados.
- Vuelta a la versión oficial (usuario, con la página de Konami): cuentan también las mágicas y trampas del adversario. Texto oficial en los 5 idiomas. Exe y mod copiados.

### 2026-11-13: Goblin Entrenador (Coach Goblin) reemplaza a Fiend's Hand / La mano de adicto (4141, k 133, modelo m134)
- No existía en el juego. Tierra / Demonio / Efecto, nivel 4, 1200/1500. Textos en 5 idiomas, ilustración `Foto_goblin_entrenador.webp`.
- Disparo: pasos de la End Phase `sub_0009EC80` (u8 0x65F8EC). En el paso 5 el juego mira a Garuda (5255) del jugador 1 - ((0x65F8DA>>1)&1); ahí mismo (una vez por End Phase) registramos el disparo del Goblin como lo hace Garuda: `sub_00081EF0(((casilla|0x840|dueño<<15)<<16)|k)`, si está boca arriba sin negar y hay un monstruo normal en la mano (mano: u8 0x65D0A8+p*0x13F8; carta i = u32 0x65D77C+p*0x13F8+8i).
- Entrada: alias Goblin → Garuda en la búsqueda (sub_000C4DE0); resolver `sub_000B6DA0` envuelto: 0x80 (CPU: el normal de menos ATK al fondo) / Sí-No con el texto del juego 0xC55 → 0x7F mensaje 0xC56 → 0x7E cursor de la mano `sub_00092E20(1)` (elegido en 0x6633DC; si no es normal, vuelve a pedir) y `sub_000C6D90(p, i, 0)` (0 = al fondo: así lo usa la carta 4503, sub_000AB030) → 0x7D robar 1 → fin.
- Dudas a mirar en la prueba: si el paso 5 es de verdad tu End Phase (Garuda se mira para 1 - jugador de 0x65F8DA); que el Sí/No y el cursor de la mano salgan bien. Log: líneas "[CARTAS] Goblin Entrenador".
- Modelo: Goblin_entrenador.glb, `--como=m134 --cuerpo` (m134: 22,0 × 14,1 → alto 19,4, ancho 16,0). Render revisado. Exe y mod copiados. Sin probar.
- Corrección: Garuda actúa en la End Phase del turno del ADVERSARIO (su texto), por eso mira a 1 - jugador del turno. El Goblin ahora mira al jugador del turno ((0x65F8DA>>1)&1). Exe copiado.

### 2026-11-13: Soldado del Brillo Negro - Enviado del Principio reemplaza a Leogun (4409, k 391, modelo m402)
- No existía (solo el de ritual, 4370). Luz / Guerrero / Efecto, nivel 8, 3000/2500. Textos en 5 idiomas; marco de Great Moth (`base = 4063`, nivel 8 de efecto).
- Invocación (pedido del usuario: usar las del juego): la de **Alma de pureza y luz** (0x1483, destierra 2 LUZ de a uno). La carta pasa por Alma (0x5FE692[k]) en `sub_00093A30` (¿se puede invocar desde la mano?) y `sub_000968D0` (máquina de la Invocación, paso 0x65F8FD: 0x3C..0x41). Propios: `sub_000937E0` (hay 1 LUZ y 1 OSCURIDAD en el Cementerio), `sub_000D28B0` (no se Invoca de Modo Normal), la lista (`sub_000C1B80`): LUZ/OSCURIDAD del Cementerio; en la segunda elección solo el atributo que falta (el primero se anota en el paso 0x3E).
- Efecto 1: alias a **Barrel Dragon** (4749; fx y turno: su marca 0x2000 de una vez por turno). Casillas válidas de Come Hombres (cualquier monstruo; `sub_000BAFE0`), condición `sub_000A5870` (no si ya atacó o usó efecto), resolver `sub_000B3510`: A1F20 + C69A0(p, z, 0) (desterrar, como Prisión) y queda marcado "ya atacó" (casilla +0xB bit 2, bit z de 0x65D0C4).
- Efecto 2: se anota su ataque en `sub_0009A290`; antes de los comandos (`sub_00098EF0`) y de `sub_00099980`, si el atacado ya no está y el Soldado sigue, se le quita la marca de "ya atacó" (lo del Caballero Hayabusa), una vez por turno; se limpia en la End Phase (paso 0 de `sub_0009EC80`).
- `texto_cartas.escribir`: el mod le dice si es monstruo (`mon`); con la carta base de texto largo la detección por la raya de ATK/DEF fallaba (salía "[CARTA MÁGICA DE EQUIPO]" sin estrellas). Solo cambió esta carta.
- Modelo: Soldado del Brillo.glb, `--como=m402 --cuerpo` (m402: 25,5 × 16,1 → alto 20,9). Render revisado. Exe y mod copiados. Sin probar. Log: "[CARTAS] Soldado del Brillo Negro".
- Herramienta (el usuario: que las próximas cartas salgan bien a la primera): `cartas_mod.elegir_base` ordena los marcos candidatos: primero los que la detección de monstruo reconoce bien, sin Toon, con texto corto; después mismo atributo. El Soldado toma Great Moth solo (sin `base`). Solo cambió el marco del Hombre de Nieve (4274), visto igual. Mod copiado.

- Goblin Entrenador: el mensaje de elegir ahora es propio, en 5 idiomas: "Selecciona un monstruo de tu mano." Antes usaba el 0xC56 del juego, que dice "carta". Si falla, vuelve al 0xC56. Compilado, falta copiarlo (el juego estaba abierto).
- Goblin y Soldado se cortaban abajo a la izquierda en la batalla. Rearmados con `--suelo=3 --derecha=4` (Goblin) y `--suelo=3 --derecha=5` (Soldado). Los anteriores quedaron en `*_antes_mover.igb`.
- Soldado: el cartel de la invocación usaba los textos 0xD0C/0xD0E del juego ("...%d monstruo(s) LUZ"). Ahora, solo durante su invocación, dice "1 de LUZ y 1 de OSCURIDAD" y después el atributo que falta, en 5 idiomas. Para eso `voces.c sub_0002C5C0` llama a `ygo_cartas_texto(n, txt)`.
- Registro de la prueba: efecto 1 usado 2 veces, en turnos distintos. No hay ningún ataque del Soldado en el registro, así que el efecto 2 todavía no se probó.
- Soldado, prueba del usuario: (1) solo podía desterrar desde el turno siguiente a invocarlo (la bandera 0x2000 de Barrel Dragon, que se pone al pasar turno); (2) después de desterrar igual podía atacar; (3) destruyó un monstruo atacando y no tuvo segundo ataque (en el registro no había ningún "ataca": el ataque del jugador va por sub_0009A7B0 → sub_00099940, no por sub_0009A290).
  Arreglos:
  - A5870: para el Soldado devuelve 1 sin mirar la bandera.
  - Nuevo `s_bls_ef1`: lo pone el resolver y se borra en el paso 0 de la End Phase.
  - Envoltorio de `sub_00099BC0(p,z)` ("¿puede atacar?"): primero llama a bls_segundo; después devuelve 0 si usó el efecto 1.
  - Envoltorio de `sub_00099940(p,z)` (marca el ataque): llama a bls_ataque con 0x6666E0.
  Sin probar.
- Modelos: Goblin `--como=m134 --suelo=3 --derecha=1.5` (con 4 quedaba al centro). Soldado más grande: `--cuerpo --alto=25 --suelo=1 --derecha=3.5`. Las versiones anteriores quedaron en `*_derecha4/5.igb`.

- Soldado del Brillo Negro: CONFIRMADO en juego que los 2 efectos andan (desterrar sin atacar; segundo ataque al destruir). Faltaba desterrar el turno de la invocacion: el comando del efecto (sub_00098EF0) pide el bit 0x20 de la casilla +0xB, que solo pone el repaso del turno; ahora bls_habilitar() lo pone para el Soldado si no ataco ni uso un efecto (sin probar). Modelo m402 mas grande: --cuerpo --alto=32 --suelo=0 --derecha=4.5 (respaldo m402_alto25.igb).
- REVERTIDO bls_habilitar (poner el bit 0x20 al Soldado): el usuario reporto que con eso dejaron de andar ambos efectos. Vuelve a la version confirmada (desterrar recien desde el turno siguiente). Queda el modelo alto 32.
- Causa real de "desterrar recien el turno siguiente": ya_ataco() miraba la casilla +0xB bit 2, que tambien ponen la invocacion/colocar/cambio de posicion (sub_00098230/985B0/98620). Ahora mira el bit z de u16 0x65D0C4, que solo ponen los ataques (99940, A290). Unico uso: la condicion A5870. Sin probar.
- 2026-11-13: Ciclón del Cosmos vuelve a DESTERRAR (pedido del usuario; antes al Cementerio). Resolver: rama cic con sub_000C69A0(p, z, 0). Texto en 5 idiomas ("destiérrala"). Compilado; sin copiar todavía porque el juego estaba abierto.
- Soldado: con el bit de u16 0x65D0C4 el efecto 1 no salia nunca (ese registro no se limpia cada turno como creia). Ahora la condicion A5870 usa un flag propio s_bls_ataco (lo pone bls_ataque al atacar el Soldado, se limpia en el paso 0 de sub_0009EC80 junto con s_bls_usado). ya_ataco() quitada. Sin probar.
- ERROR MIO 2026-11-13: desde las 11:55 copiaba el build como ygo_recomp.exe, pero el juego corre Yu-Gi-Oh.exe; el usuario probo siempre el build de las 11:55 (por eso Ciclon seguia "destruye"). Ya copiado como Yu-Gi-Oh.exe (cmp ok) y borrado el ygo_recomp.exe suelto. Llegan juntos: Ciclon destierra, flag s_bls_ataco del Soldado, modelo alto 32 (el .igb si estaba copiado).
- 2026-11-13: tools/copiar_al_juego.sh = UNICA forma de entregar (Yu-Gi-Oh.exe + cmp + mod + diff; no copia si el juego esta abierto).
- Premios sin cuarta copia: el usuario recibio un 4º Tomate de Mystic (k 824). Hueco encontrado: las ranuras de un premio se sortean todas antes de llegar al baul, asi que dos iguales en el mismo premio pasaban el filtro. Ahora copias_premio() suma las ya sorteadas en este premio (se reinicia en la ranura 0 o a los 30 s). Las lineas [PREMIOS] ranura ... tambien van a Ajustes\premios_registro.txt (no se borra al arrancar) para revisar la proxima victoria. Sin probar; no confirmado que ese fuera el caso del usuario (el registro de esa victoria ya no estaba).

### 2026-11-13: Guerrero de la Fortaleza y Caius, el Monarca de las Sombras (pedido del usuario)
- Ninguna existía en el juego. Fotos y modelos del usuario (escritorio "Cartas que quiero Agregar"). Textos oficiales en 5 idiomas.
- **Guerrero de la Fortaleza** reemplaza a Espectro de sombra (4015, k 9, modelo m008). Tierra / Guerrero / Efecto, nivel 2, 600/1200 (no hay Cantante). Marco de Flor de arco iris.
  - Efecto `fortaleza`: envoltorio de `sub_0007F8D0` (resultado de la batalla, B = 0x6666E0; entrada por jugador en B+6+12p: +0 casilla y bit 3 "destruido", +2 k, +0xA daño de batalla de ese jugador; ahí mismo el juego salva a Parca de Espíritu 0x1596). Daño de su dueño a 0 siempre; bit 3 borrado una vez por turno (clave del ataque, por las llamadas repetidas y la vista previa de `sub_0009B5B0`; se limpia en el paso 0 de `sub_0009EC80`).
  - Modelo: `modding/modelos/fortaleza`, riggear_rigido → volcar → `--como=m008` (m008 medido: base 3,6, alto 16,4, ancho 5,7 → alto 9,5).
- **Caius** reemplaza a Orion el rey de lucha (4132, k 124, modelo m125). Oscuridad / Demonio / Efecto, nivel 6, 2400/1000. Marco de Buscador de dragón.
  - Efecto `caius`: el disparo de Buscador de dragón (4506, "al ser llamado, excluyendo Llamada Especial"; Nivel 6 = siempre por Tributo): la carta pasa por 4506 en `sub_00093FA0`/`sub_00094610` (estado del disparo) y en la búsqueda `sub_000C4DE0`. Las funciones de Buscador envueltas para Caius: casilla válida `sub_000A2CB0` (cualquier carta), elección `sub_000BB830` (como Golpe de fénix con las casillas de los dos jugadores; CPU: primero cartas del adversario), resolver `sub_000AB0F0` (`sub_000C69A0(p, z, 0)` destierra; si era monstruo de OSCURIDAD `sub_000C8BC0(adversario, 1000)`).
  - Modelo: `modding/modelos/caius`, `--como=m125` (alto 12,0).
- Exe y mod copiados con `copiar_al_juego.sh`. Sin probar. Dudas: si Buscador pregunta Sí/No antes del efecto (Caius es obligatorio); tamaño de los modelos. Log: "[CARTAS] Guerrero de la Fortaleza", "[CARTAS] Caius".
- Prueba del usuario: Caius funcionó (desterró la carta k 343, no era de OSCURIDAD). "Me pidió activar": falta saber qué texto salió (la Cadena 0xC3E "El efecto %s está activado. ¿Resolverlo como parte de una Cadena?" de `sub_000C07F0` es general, sale con todos los efectos). El Guerrero no fue atacado: sin probar.
- Modelos "muy bajos" y Caius más grande: Guerrero `--como=m008 --suelo=5 --derecha=1.5` (5–14,5); Caius `--cuerpo --alto=15 --suelo=4 --derecha=1.5` (4–19). Anteriores: `fortaleza/m008_como_suelo0.igb`, `caius/m125_alto12_suelo0.igb`. Mod copiado.
- Tabla de ajustes de todos los modelos propios y lo aprendido: `modding/modelos/ajustes_modelos.md` (actualizarla con cada modelo).
- Prueba del usuario: "parece que funciona bien ahora" (con captura de la Tortuga gigante = la carta a reemplazar por Isaac).

## Isaac, Caballero Divino (2026-11-13, reemplaza a Tortuga gigante que se alimenta de Flames 4599)
- Luz / Guerrero / efecto, nivel 6, 2300/1800. Textos oficiales en 5 idiomas (Divine Knight Ishzark). Ilustración `modding/cartas/Foto_isaac_caballero_divino.webp`.
- Efecto `isaac` (cartas.c): en el envoltorio de `sub_0007F8D0`, solo en la batalla de verdad (retorno de `sub_00080170` en +20 distinto de 0x9B860, la vista previa de `sub_0009B5B0`), si un lado es Isaac y el otro queda con el bit 3 (destruido), se anota (p, z, k). En `sub_000C7C50` (va al Cementerio; arg 4 = casilla 0x65D0C8 + p*0x13F8 + z*0x9C) esa carta se destierra con `sub_000C69A0(p, z, 0)` en vez del pad. La anotación se borra en el paso 0 de `sub_0009EC80`.
- Modelo del usuario (`Descargas\Isaac caballero.glb`) en `modding/modelos/isaac`, m592: `--cuerpo --alto=18 --suelo=3 --derecha=1.5` (3–21; la Tortuga mide 0–22,2).
- Exe y mod copiados (LISTO). Sin probar. Log: "[CARTAS] Isaac".
- Prueba 1: "[CARTAS] Isaac ... destruye en batalla la carta k 807 (Cyber pote)" salió, pero se fue al Cementerio: `sub_000C7C50` recibe una copia de la carta (0x666700 + p*0x9C), no la casilla, y solo anota los disparos. Arreglo: el destierro pasa a `sub_000C6960(p, z, copia)` (orden 0x5A, la mueve del Campo al Cementerio; solo la llama el final de la batalla `sub_0009D350`, después de C7C50). Exe copiado (LISTO 15:19). Sin probar.
- Caius en el ataque se veía grande y bajo: `--cuerpo --alto=13.5 --suelo=6 --derecha=1.5` (6–19,5); anterior `caius/m125_alto15_suelo4.igb`. Mod copiado.

## Visor de modelos 3D (2026-11-13, pedido del usuario: ajustar sin entrar al juego)
- `Visor de modelos.bat` (raíz del port) = `py -3 tools/visor_modelos/visor.py`: servidor en http://localhost:8765 y página three.js (`tools/visor_modelos/index.html`, three@0.160.0 de jsdelivr).
- Lista las cartas con `modelo` de `modding/cartas/cartas.txt`; exporta los .igb con `tools/igb_gltf.py` a `modding/modelos/_visor/` (cache por fecha). Muestra el propio con su animación de reposo al lado del original que reemplaza (y otros propios u originales mNNN que se agreguen), en la misma escala, con regla cada 5 de alto.
- Alto / suelo / derecha se ven en vivo (escala k = a/a0 y posición (d − d0·k, s − s0·k), lo mismo que hace armar_igb.py). "Armar" corre armar_igb.py (`--cuerpo` opcional), guarda el anterior en `<carpeta>/anteriores/`, escribe `<carpeta>/ajuste.json` y actualiza la fila de `ajustes_modelos.md`. "Deshacer" vuelve al último anterior. "Copiar al juego" corre cartas_mod.py y copiar_al_juego.sh.
- Los datos del juego son de mano izquierda: en la página todo va con X invertida (la "derecha" del armado queda a la derecha de la pantalla). La vista "Batalla" es aproximada: la cámara del combate está en el código, no en los archivos (los be0NN.igb solo traen efectos).
- Probado sin juego: lista, exportación, armar con los mismos valores de Caius (igb idéntico, md5), deshacer, ajuste en vivo. Valores de los armados con --como: los calcula como armar_igb.py.

## Isaac y Soldado, prueba 2 (2026-11-13)
- Isaac: anotó "destruye en batalla la carta k 155" pero sub_000C6960 nunca recibió esa casilla: el monstruo se fue al Cementerio. Ahora el destierro también está en `sub_000C8620(p, z, ?)` (destruir del Campo; la batalla lo llama para el atacado en 0x9C379 y 0x9B9A0), y se quitó la excepción de sub_0009B5B0 (es un paso real del combate, va por la tabla de pasos igual que sub_0009BA10). La anotación se borra al declarar otro ataque (sub_0009A290) y en el paso 0 del turno; si la carta ya se desterró, la otra llamada (C6960 con la copia) se saltea para no duplicarla en el Cementerio. Log: "[CARTAS] Isaac: ... se destierra en vez de ir al Cementerio (sub_000C8620|sub_000C6960)".
- Soldado: "atacó y no pude atacar 2 veces". El atacado fue k 807 Cyber pote (Cyber Jar): boca abajo, al voltearse destruye todos los monstruos, el Soldado incluido (después Isaac aparece en la misma casilla 1). No se encontró error en el código; se agregaron líneas al log: "ya no esta en el Campo, sin segundo ataque" y "destruyo al atacado, pero ya uso un efecto este turno".
- Exe copiado (LISTO 17:08). Sin probar.
- Visor: el usuario armó Caius (alto 18, suelo 5,5, derecha 2), Goblin (19,4 / 3 / 3) e Isaac (25 / 0 / 2) a las 17:03-17:11, pero no llegaron al juego: copiar_al_juego.sh copiaba `modding/cartas/salida` sin rearmarla (salida de las 16:50). Ahora `copiar_al_juego.sh` corre `cartas_mod.py` antes de copiar. Copiado y comprobado con cmp (los 3 .igb iguales en Mods-Extras).
- Prueba del usuario (17:21): Caius con un monstruo de OSCURIDAD: "destierra la carta k 807 ... era de OSCURIDAD: 1000 de dano al adversario" (funciona). Goblin Entrenador "no manda las cartas nuevas al Deck": el log dice "la carta 2 de la mano (k 128) no es un monstruo normal" (Sincronizador, de efecto) y k 837 (Golpe de viento, mágica); con un monstruo normal (k 452) funcionó. Es lo que dice su texto: solo monstruos normales. Sin cambios.
- Visor: "Batalla (exacta)" reemplaza la aproximada. Cuadro 16:9 con la captura del juego encima (opacidad, se puede cargar otra), etiquetas ocultas. Cámara calibrada con la captura del usuario (Goblin m134 contra m587; guardada en `tools/visor_modelos/calibracion.webp`): superponiendo el render del visor a 1920x1080 (`window.capturar()` -> POST /api/captura -> `modding/modelos/_visor/_captura.png`) con la captura. EXACTA = fov 40, cámara (-14, 14, 52) mirando a (-14, 16, 0); propio en x -12, z 8, girado -40°; rival en x -24, z 18, girado 30°, alto 5 (coordenadas del juego). El Goblin coincide en torso y pies; la planta en ancho y trompetas (la pose de la captura es otro momento de la animación). El rival es el primero marcado en "Comparar con".
- Usuario: no quería la captura encima, solo la cámara igual al juego con el propio a la izquierda. Se sacó la captura de la página (queda calibracion.webp y window.capturar para recalibrar).
- Visor (pedido: centrar como el juego, izquierda/derecha, comparar con Tomate y plantas, tamaños exactos):
  - Botón "Centrar como el juego" (vuelve a la cámara calibrada), control "Izq. / Der." de -15 a 15, botones de rival rápido: Tomate de Mystic m918, Florero viviente m587 (la planta verde de la captura 1), Planta oscura m122 (captura 2).
  - ERROR CORREGIDO: el visor espejaba X. Los datos ya son de mano derecha para glTF (el monstruo mira a +Z, su izquierda es +X): con el espejo, la espada de Isaac salía del lado contrario. Sin espejo, el Goblin queda como en el juego (trapo a la izquierda). Recalibrado: cámara (14, 14, 52) mirando a (14, 17.5, 0), fov 40; propio x 4.5 z 8 girado 40°; rival x 25 z 18 girado -30° alto 8.
  - SIN RESOLVER: el juego cambia el tamaño de cada monstruo según su número de modelo. Con la cámara del Goblin: Isaac (m592) se ve ~0,65 y además más arriba y a la izquierda; Planta oscura (m122) ~0,67 con la base en el mismo lugar. Medidas: con nuestra geometría Isaac/Goblin da 0,89 (radio de la caja); con los originales, raíz(alto×ancho) da 0,67 (como --como) y el radio 0,54. No se encontró la tabla (no hay tira de floats ni archivo de parámetros; sub_00040300 es una cámara orbital). Para tamaños exactos falta encontrar esa regla en el código o medirla en el juego.
- Búsqueda de la escala por modelo (2026-11-13, "sí, buscala"):
  - Recorrido: `sub_00075B30` arranca la batalla y guarda los números de modelo del atacante y del atacado en `[0x658524]+0x18/+0x1C`; la batalla tiene cámaras de 0x4C bytes en `[0x658524]+0x20+i*0x4C` (`sub_00075FD0` las lee, `sub_0005A810` las interpola en 45 cuadros; vienen de la escena: `sub_0005B7E0` copia pos/destino/arriba de `[esi+0x2C]+0x8C/0x98/0xA4`). La caché de modelos (`sub_00058890` carga, `sub_0005AB00` libera) no escala.
  - Transformación de objetos: Alchemy `sub_001E55B0` identidad, `sub_001F13C0` traslación, `sub_001E62D0` escala (`matriz *= (x, y, z)`), `sub_001E6060` rotación. Dos clases arman así su matriz con posición +0x20, escala +0x2C y rotación +0x14: `sub_0004A950` (la posición × la constante [0x423640]) y `sub_0005BC10` (`sub_0005BC80` fija la posición, `sub_0005BCA0` lleva la escala a un valor en [obj+8] cuadros; `sub_0005C520` coloca un objeto con rotación/posición/escala). Las cartas del tablero usan esa API; no se identificó todavía cuál es el monstruo.
  - Diagnóstico: `src/diag_escala.c` envuelve `sub_001E62D0` (11 llamadores) y anota en el log "[ESCALA] desde <retorno> obj <objeto> escala x y z pos x y z rot x y z (batalla: modelos A y B)" solo cuando cambia (tope 3000 líneas). Exe copiado (LISTO 17:59). Falta un duelo del usuario con batallas (Goblin, Isaac, Caius contra Tomate/plantas) para leer la regla.
- Duelo del usuario 1 (5 batallas: Goblin m134 contra m509, m254-m237, m432-m236, Golem m260-m237, Goblin-m007): el log llegó al tope (3000) por `sub_00068FE0` (retorno 0x69031), que aplica escalas 0,93-0,98 a muchos objetos durante la batalla; solo entró la primera batalla. Ninguna escala por modelo (tipo 0,65) pasó por `sub_001E62D0`. Corrección: `sub_001F13C0` es la ROTACIÓN (la constante [0x423640] = 0,01745, grados a radianes) y `sub_001E6060` la TRASLACIÓN. `sub_00068FE0`: rotación en grados +0x50, escala +0x84, posición +0x1C, nodo +0x108.
- diag_escala.c v2 (exe 18:07): una entrada por objeto (256), 0x69031 solo durante batallas, posición/rotación de los 3 tipos, y al empezar cada batalla las 6 primeras cámaras de `[0x658524]+0x20+i*0x4C` (19 floats). Tope 6000. Falta otro duelo del usuario.
- Duelo del usuario 2 (último; Caius ataque directo y Tomate m918 contra m353): las 6 cámaras de la batalla son IGUALES en las dos batallas (fijas, no dependen del modelo). Cada una 19 floats: 0, posición A, destino A, arriba A, posición B, destino B, arriba B (se mueve de A a B). Cámara 0: (-13.03, 24.16, 34.84)->(8.20, 24.46, 2.17) hasta (-7.41, 27.13, 10.34)->(0.95, 27.25, -2.52); el resto en el log del 18:13 (copiadas en diag_escala.c la 0).
- CONCLUSIÓN: el juego no escala los monstruos por modelo. En la captura de Isaac contra Planta oscura los dos salían a ~0,65-0,67 (achicados igual): era otra cámara/momento del ataque. Comparar tamaños en el visor es exacto (mismas unidades); la vista Batalla del visor reproduce el momento de la captura del Goblin. Los objetos 0x5BC41 con posiciones (±8.6/±4.3/0, 0, 12.2/-7.7) son las cartas del tablero.
- diag_escala.c queda apagado salvo YGO_DIAG_ESCALA=1 (exe 18:14, copiado). Visor: nota actualizada.
- Colocación del monstruo en batalla (pedido: visor 100% fiel), SIN RESOLVER:
  - Las cartas del tablero están en x ±8.6/±4.3/0, y 0, z 12.2 o -7.7 (objetos de `sub_0005BC10`). Hipótesis "el monstruo está parado en su casilla, visto con una de las 6 cámaras reales": `tools/visor_modelos/ajuste_camara.py` prueba cámara, momento (t), fov 25-75 y casillas contra los pies/cabeza del Goblin y base/trompetas de la planta de la captura 1: el mejor error es ~160 px (cámara 2, t 0.5, fov 75). No encaja.
  - Revisadas sin éxito: `sub_00063D90`/`sub_00064630` (cargan modelos y trasladan, parecen una animación de 3 objetos), `sub_00067xxx` (solo trasladan), `sub_00068FE0` (muchos objetos chicos, partículas).
  - Lo que falta: encontrar el objeto que dibuja al monstruo en la batalla (su nodo raíz y su matriz). Camino seguro: registrar en tiempo de ejecución la matriz del nodo del modelo mNNN cargado (p. ej. desde la caché `sub_00058AA0` seguir el nodo hasta su igTransform). Requiere otra partida del usuario.
- RESUELTO (2026-11-13): cómo dibuja el juego a los monstruos en la batalla, leído del código (sin ajustes a mano):
  - `sub_0005A670` guarda el número de modelo en +0xD8; `sub_0005A490` busca en el modelo los nodos "m%03d" (el igGroup raíz; los originales lo tienen, los nuestros se llaman m373 por el donante) y "NULL".
  - `sub_0005A810` toma la cámara número = modelo de la tabla del objeto de batalla (`sub_00075FD0`); esa tabla se llena en `sub_00076540` desde **default.xbe 0x42A718: 1000 entradas de 0x98 bytes = 19 doubles: 0, destino A, posición A, arriba A, destino B, posición B, arriba B** (`sub_00076460` las pasa a float como posición, destino, arriba). `sub_000585E0` la mueve de A a B en 45 cuadros y `sub_000583A0(pos, destino, arriba)` la aplica; con la bandera +0x44 (el atacado) multiplica la X de posición y destino por [0x429884] = -1.
  - `sub_00059AA0`: perspectiva `sub_0004ABD0(45°, 480, 480, 1, 10000)`: textura cuadrada 480x480 (`sub_0005AF00(480,480)`).
  - `sub_00057D60`: la textura del atacante va en x 0..480 de la pantalla de 640x480 y la del atacado en x 160..640 (entran deslizándose 480·(1-progreso)); toda la altura. Con "Panorámico: estirar" el port estira 640x480 a 16:9.
  - El monstruo va en el origen, sin girar ni escalar. Por eso Isaac (m592) sale chico: usa la cámara pensada para la tortuga (posición A (-27.6, 25, 50.5)).
  - Visor: la vista "Batalla" usa esto (servidor `/api/camara?m=mNNN` lee la tabla; dos pasadas con viewport/scissor; control "Cámara" 0..45 y "Reproducir el movimiento"; casilla "Estirado a 16:9"). Comprobado superponiendo las 2 capturas del usuario: Goblin m134 y Florero m587 (cámara 0) e Isaac m592 y Planta oscura m122 (cámara 0) coinciden; las diferencias son la pose de la animación. Se borró la calibración a mano (EXACTA) y `ajuste_camara.py`.

### Isaac: destierro (2026-11-13, 19:05)
- En 2 partidas del usuario `isaac_batalla` (resultado de batalla sub_0007F8D0, B=0x6666E0) no anotó nada nunca: sin línea de Isaac en registro.txt. El código del resultado se ve correcto (bit 3 = destruido, k con & 0x7FFF); causa sin confirmar.
- Cambio: `isaac_ataque` en la declaración del ataque (sub_0009A290, la misma que usa el Soldado y que sí aparece en el registro) anota al rival de Isaac (atacado si Isaac ataca; atacante si atacan a Isaac). Si esa carta pasa por sub_000C8620/sub_000C6960 antes del próximo ataque, se destierra.
- Registro nuevo: "[CARTAS] batalla: ..." (cada resultado de batalla con casillas, k y DESTRUIDO), "Isaac en batalla ...", "Isaac: sub_000C8620/C6960 (...)" en cada llamada mientras hay algo anotado. Exe copiado 19:05, sin probar.
- 19:16 CAUSA ENCONTRADA (registro del usuario): isaac_batalla SÍ anota ("Isaac (jugador 0, casilla 0) destruye en batalla la carta k 285 ... se destierra"; antes no lo vi porque mi grep filtraba "): "). El atacado destruido sale del Campo por sub_000C7C50 (0xC80BA/0xC80F5 -> sub_000C6390), no por C6960 ni C8620. Arreglo: wrapper de sub_000C6390(p, z, 1) (la que saca cualquier carta del Campo; también la usa el destierro sub_000C69A0 desde 0xC6A2D): si es la carta anotada, la destierra con sub_000C69A0. Se quitó el borrado de la anotación en sub_0009A290. Exe copiado 19:16, sin probar. Queda la orden 0x5A ("al Cementerio") que C7C50 manda antes de sacarla: sin verificar si tiene efecto visible.
- 19:24: el wrapper de C6390 tampoco se activó (registro: Isaac anota k 268, nada más). C6390 solo desengancha equipos; el pase al Cementerio lo hace la orden 0x5A con la carta ya fuera de la casilla. Arreglo nuevo, independiente del camino: `isaac_cementerio()` (desde bls_segundo y cada paso de sub_0009EC80): cuando la carta anotada ya no está en el Campo y aparece en el Cementerio (0x65DC7C, cuenta 0x65D0AA, entradas 8 bytes), la saca de ahí y la agrega a desterrados (0x65DEFC, cuenta 0x65D0AB); listas vistas en sub_000C5AE0. Log "Isaac: la carta k ... pasa del Cementerio a desterrados". Exe 19:24, sin probar.
- 19:39: el 19:24 movía la carta a 0x65DEFC/0x65D0AB, que NO son los desterrados (el usuario nunca la vio desterrada). Desterrados = 0x65E17C con cuenta 0x65D0AC: lo agrega sub_000D4B70(&carta) (bit 15 = dueño), la misma que usa la orden de destierro 0x57 (sub_000CAD20 -> sub_000D3F60, la de Ciclón). Listas por jugador (sub_000C5AE0): 0x65DC7C/AA Cementerio, 0x65DEFC/AB ?, 0x65E17C/AC desterrados, 0x65D9FC/A9, 0x65D77C mano. Ahora isaac_cementerio saca la carta del Cementerio y llama a sub_000D4B70. Exe 19:39, sin probar. La carta pasa un instante por el Cementerio (la orden 0x5A, sub_000CA450, la manda ahí).
- 19:48: el usuario confirmó que destierra (registro: desterrados 4), pero el Cementerio seguía mostrando la carta arriba. Ahora isaac_cementerio llama a sub_00088970 (redibuja las pilas; lo hacen las órdenes 0x5A/0x57 al terminar). Exe 19:48, sin probar.
- 19:56: la pila del Cementerio seguía mostrando arriba la carta desterrada (la cuenta sí bajaba). Ahora, después de desterrar, isaac_cementerio redibuja las pilas (sub_00088970) en cada llamada hasta el paso 0 del turno siguiente (la animación de ida al Cementerio la vuelve a pintar después). Exe 19:56, sin probar. Nota: Cementerio = 0x65DC7C/AA (lo confirma el Soldado y el globo de la cuenta), desterrados = 0x65E17C/AC; 0x65DEFC/AB es el Deck (en la versión 19:24 la carta iba al Deck).
- 20:03: el redibujado de 19:56 solo tocaba el radar de colores (sub_00088630 escribe el mapa 0x65A83E con colores por tipo, sub_000891E0), no la pila que se ve: no servía. Ahora, como Ciclón: wrapper de sub_000C7C50 (la que manda al Cementerio al destruido en batalla, 0x9D772/0x9DCC4, y la que ya usan las Cerezas): si la entrada es la carta anotada por Isaac, no se llama; se destierra con sub_000C69A0(p, z, 0) si sigue en la casilla, o con sub_000D4B70(entrada) si no. No pasa por el Cementerio. isaac_cementerio queda de respaldo. Corregido además: los wrappers que no llaman a la original no sacaban la dirección de retorno (C6960/C8620; nunca se activaron). Exe 20:03, sin probar.
- 20:21: registro del usuario: en sub_000C7C50 la casilla ya está vacía (camino sub_000D4B70: destierra sin animación ni pila). Ahora se vuelve a poner la casilla desde la entrada (copia de 0x9C bytes de sub_0007F8D0) y se destierra con sub_000C69A0 como Ciclón. Exe 20:21, sin probar.
- 21:52 SOLUCIÓN (leída en el código, sin suposiciones): el juego ya tiene "destruido -> desterrado" para Macrocosmos (imagen 0x1332, consulta sub_000D34B0(jugador, 0x1332)). Orden 0x54 = sub_000CA130 (destrucción en batalla, la encola el combate en 0x9BDED): paso 0 anima la carta hacia desterrados (0x1E) con Macrocosmos o al Cementerio (0x1C); paso 2 vacía la casilla (por eso Isaac encontraba la casilla vacía en C7C50). sub_000C7C50 (fin de batalla): con Macrocosmos salta a 0xC80C2 y encola 0x5A (sub_000CA450 -> sub_000D4B70, desterrados 0x65E17C); si no, 0x59 (sub_000CA310 -> sub_000D4B10, Cementerio 0x65DC7C). sub_000D3DF0(p, z, f): f!=0 destierra, f==0 Cementerio. Isaac ahora: wrappers de sub_000D34B0 (da 1 para 0x1332 solo con s_isaac_macro), sub_000CA130 (paso 0 de la 0x54 de la carta anotada) y sub_000C7C50 (entrada de la carta anotada). Se quitaron isaac_ataque/isaac_desviar y los desvíos de C6390/C6960/C8620 (quedan de paso). Exe 21:52, sin probar.
- 2026-11-13 23:00: Isaac CONFIRMADO en el registro (22:49, dos batallas): orden 0x54 hacia desterrados y fin de batalla como Macrocosmos. Carpeta del jugador limpiada: se movieron a sobrantes_carpeta_juego_2026-11-13/ el zip viejo de la versión (distinto al de dist/), Herramientas\ e Instalar juego desde ISO.bat (hay copia en dist/Yu-Gi-Oh Port), registro.txt y xbox_kernel.log (el juego los vuelve a crear). Quedan: Game, Mods-Extras (cartas, texturas, voces), mods_puntero, Partidas, Ajustes, Yu-Gi-Oh.exe, Configuración.exe, SDL3.dll, shaders.pak, version.txt, ygo_menu(.default).ini, LEEME.txt.

## 2026-11-14 — Límites de copias y premios (torneo)
- Límites pedidos por el usuario (campo `copias` de modding/cartas/cartas.txt, sub_0007EF00): Soldado 2, Caius 2, Isaac 2, Hombre de Nieve 2, Golem Centinela 1 (más los que ya estaban: Golpe Fénix 1, Carga del Poderoso 1). Copiado al juego, sin probar.
- Premios, leído en el código: `sub_0008CBB0(modo, d, ranura)` -> modo 0 `sub_0008C7E0` (listas por rival 0x453830/0x453630) o modo != 0 `sub_0008C9B0` (torneo, según el usuario: 3 duelistas, 5 cartas). C9B0 usa la variante r de sub_0008C530(d), una lista común por nivel de torneo t = clamp((u32 [0x66DCDC+0x14] - 1) / 3, 0..4) en 0x453F78 + 16t (u32 lista[2], u16 cant[2]: 0x453a30 78, 0x453b68 113, 0x453d30 63, 0x453e30 81, 0x453e30 81) y una lista por rival en 0x453FC8 + 16d. Mismo sorteo que C7E0: entrada u16 ID | u16 (peso bits 0-12, bit 15-ranura), chance por pasada máx(5, peso × factor[copias]) % (factor 0x454358: 1, 0.7, 0.4, 0.1), peso 0 = nunca; junta >= 10 y elige una al azar.
- Agregadas en el torneo: nivel 1 común: Cerezas, Golem, Goblin, Hombre de Nieve, Sincronizador, Vidente, Fortaleza (peso 35, ~1,5-2 % por premio); niveles 3-5 comunes: Caius, Soldado (peso 50, premios 1-2, ~2 %), Isaac (peso 50, premios 2-3, ~2 %). Por rival: Carga (Yugi peso 100 premio 2 ~5,5 %; rival 31 peso 50), Ciclón (Joey 50, premio 1 ~2,5 %), Prisión (Arkana 50, premio 2 ~2,6 %). Golpe Fénix no está en ninguna lista. Cálculo: scratchpad premios/torneo*.py (simulación del sorteo).
- Se puede cambiar el % de cualquier carta en cualquier modo reescribiendo esos pesos (y bits de ranura) al cargar el mod. Pendiente de que el usuario elija valores.
- 2026-11-14 (aplicado, sin probar): campos nuevos del mod `premio_peso`, `premio_ranuras`, `premio_rival` (cartas_mod.py los valida y los pasa; cartas.c `premios_ajustar()` al final de `ygo_cartas_init` reescribe cada entrada de la carta en todas las listas: rivales 0x453830/0x453630 ×2 variantes, torneo por nivel 0x453F78, torneo por rival 0x453FC8; `premio_rival` ocupa la primera entrada de peso 0 de la lista 0x453830 del rival). Log `[PREMIOS] pesos del mod: ...` y `[PREMIOS] 4944 se suma al rival 26 ...`. Valores (simulados con scratchpad premios/ajuste.py, % por premio): Soldado/Caius/Isaac peso 25 solo 3.º premio (~1-1,7 %), Cerezas 100 (2,4-6 %, a pedido: necesita 3), Hombre de Nieve 15, Escudo drenador 15, Golpe Fénix 7 en Yami Marik (rival 26, 3.º premio, ~1 %; antes nadie la daba), el resto 35 (~1-2 %).
- Filtro sin cuarta copia (premios_extra.c): envuelve sub_0008CBB0, o sea duelos comunes (modo 0) y torneo (modo 1): comprobado en el registro. Arreglos: la memoria de cartas ya sorteadas se reiniciaba en cada ranura 0 y el torneo sortea la ranura 0 cinco veces (podía dar dos veces la misma); ahora se reinicia cuando la ranura baja o tras 15 s (NSORTEO 32). Si una ranura no tiene ninguna carta que falte (rival 29 ranura 0: 200 sorteos), prueba las otras ranuras del rival.

## 2026-11-14 — Mouse en "Seleccionar oponente" después del editor de mazo (compilado, sin probar)
- Reporte del usuario (otra PC, igual que el del 2026-11-12): después de editar el mazo, en la pantalla de elegir
  rival el mouse a veces no movía los personajes.
- Lo que está en el código: la pantalla (`sub_00106CE0`) activaba las flechas del mouse solo si no había ninguna
  manito rastreada (`side_if_gloveless`: `s_gx < 0`).
  - El editor de mazo sí tiene manito rastreada: registro de esta PC (18:26), clic dentro del editor con
    `fingertip 408,342`.
  - `glove_poll` solo descarta una manito cuando cambia su vtable. Si el objeto del editor sigue intacto en
    memoria, `s_gx` queda ≥ 0 y la pantalla de rivales no tiene mouse. Que pase o no depende de si esa memoria se
    reusa, por eso es a veces.
  - El registro de esta PC no tiene el caso fallido; el de la otra PC no está.
- Arreglo (raton.c):
  - la pantalla 0x106 activa las flechas sin mirar la manito (sigue respetando la rueda del menú, 150 ms);
  - `s_opp_at` (la hora en que corrió la pantalla) apaga el puntero del editor de mazo mientras está esa pantalla.
- Para comprobar: en `registro.txt`, los clics ahí tienen que decir `[RATON] arrows click ... (screen 262)`. Si
  aparece `[RATON] click game` entre líneas `[PREMIOS] slot`, sigue fallando: mirar el `fingertip` de esa línea.

## 2026-11-14 — Revisión general + animación de clic de la manito (compilado, sin probar)
- **Animación de clic** (raton.c, `glove_make(px, pressed)` / `glove_cursor`):
  - mientras el botón izquierdo está apretado, o durante 110 ms en un clic rápido, el cursor cambia a un cuadro
    "apretado": la manito al 86 % hacia la punta del dedo (el punto del clic no se mueve) y un poco más oscura;
  - los dos cursores se arman una vez por tamaño.
- **Clic que vuelve a la ventana** (falla real del registro de las 18:26, cuadro 92342):
  - el juego estaba en pausa en segundo plano; el clic para volver se tomó como clic en el editor de mazo y abrió el
    menú de la parte 9;
  - ahora el gancho del mouse marca `s_lfocus` si al apretar la ventana del juego no estaba al frente;
  - `lbutton` no ve ese clic, y `pointer_mode` le saca la A del kit mientras está apretado y 300 ms después.
- **Revisado sin fallas:**
  - registro (los HITCH de 6–22 s son pausas en segundo plano; `Unresolved kernel ordinal` y `contentmeta.xbx` son
    de siempre, al arrancar);
  - avisos del compilador en src/ (solo `menos3` sin usar en premios.c y avisos de CRT);
  - cartas.txt: todas las cartas con nombre y descripción en los 5 idiomas.
- Lista de ideas de Master Duel: pasada al usuario en el chat (sin hacer).

## 2026-11-14 — PUNTO SEGURO: "versión muy jugable" (respaldo)
- El usuario: "tenemos una versión muy jugable y me encantó como está". Si algo se rompe, **se vuelve a esta**.
- Respaldo: `respaldos/2026-11-14_version_muy_jugable/` (1,5 GB): exe y archivos del jugador, fuentes,
  herramientas, mod de cartas y modelos, cambios del kit (diff + archivos nuevos sobre f4955d1) y ESTADO.md.
  Cómo volver: su LEEME.txt.
- Sin texturas (no cambiaron; sirven las actuales de Mods-Extras\texturas y modding\texturas).
- Todo lo de después de esto se compara contra este respaldo.

## 2026-11-14 — Ideas de Master Duel (lista del usuario, se hacen de a una, sin cartas)
Duelo: 1 invocación especial con animación propia · 2 brillo al activar mágicas/trampas · 3 animación para cartas
"estrella" · 4 opción ver animaciones solo la primera vez / saltearlas (F2 + Configuración) · 5 LP que bajan de a
poco con el número de daño · 6 carteles de cambio de fase · 7 moneda/dado para quién empieza · 8 cadena numerada.
Menús: 9 mascota (Mate) en el campo · 10 sobres de premio que se dan vuelta, las raras brillan · 11 buscador y
filtros en el editor de mazo · 12 historial de duelos · 13 fundas y tapetes elegibles · 14 estadísticas del perfil.
- **Se empieza por la 9** (pedido del usuario: usar 4 o 5 modelos 3D del juego con su animación).

## 2026-11-14 — Idea 9, mascota en el campo: investigación (sin cambios)
- Los monstruos 3D del juego solo existen en la batalla. Un "visor" dibuja un modelo con su animación de reposo en
  una textura de 480x480 con su propia cámara, y la pega en la pantalla 2D (`sub_00057D60`, el atacante en
  x 0..480, el atacado en x 160..640).
- La clase del visor: vtables 0x4299C0 y 0x4299C8 (2 entradas: 0x5A390 destructor, 0x5A400 / 0x5A460 dibujar).
  Constructores: `sub_0005B4A0` y `sub_0005B680` (0x4299C0), `sub_0005B7E0` y `sub_0005B9C0` (0x4299C8).
  - Carga del modelo: `sub_0005A670` (número de modelo en +0xD8); la batalla lo llama desde 0x76B41..0x76BC7 y
    0x7D573..0x7D5B7.
  - Cámara: `sub_00059AA0` (45°, 480x480); se llama desde 0x5B648. Cámara por modelo: tabla 0x42A718.
- Plan: un visor más, armado con el mismo constructor, con un modelo fijo, dibujado chico en un costado durante
  todo el duelo, sin tocar los de la batalla.
- Falta leer, antes de tocar nada:
  - qué recibe cada constructor;
  - quién llama al "dibujar" de la vtable en cada cuadro y quién avanza la animación;
  - cómo achicar y ubicar la textura (hoy ocupa toda la altura).
- Modelos elegidos por el usuario (todos con .igb en Game\monster):
  | Carta | ID | Modelo |
  |---|---|---|
  | Dragón Blanco de Ojos Azules | 4007 | m000 |
  | Cráneo Convocado | 4028 | m021 |
  | Mago Oscuro | 4041 | m034 |
  | Kuriboh | 4064 | m057 |
  | Jinzo | 4758 | m854 |

  El Mago Oscuro 5165 usa m960 (otro modelo).
- Cómo arma y dibuja el juego los visores de batalla (desensamblado `analysis/disasm/asm/text.asm`):
  - Escena del duelo = `[0x656F88]`. En 0x44A90..0x44B97 arma:
    - +0x7C: objeto 0xCC (`sub_0005AA50`), compartido;
    - por cada lado: +0x6C/+0x70, objeto 0xD0 (`sub_00057040`), y +0x74/+0x78, el visor de 0x138 bytes:
      `sub_0005B7E0(this, [esc+0x7C], objeto 0xD0, [esc], lado==1)`, después `sub_00058760(0)`.
  - Memoria: `sub_00185210(tamaño, sub_00192370(esc))`.
  - `sub_0005B7E0` llama a la base `sub_0005B4A0` (vtable 0x4299C0; textura 480x480 con `sub_0005AF00`,
    cámara `sub_00059AA0`) y pone la vtable 0x4299C8.
  - Cada cuadro, en 0x46100: `sub_00056FC0(visor, render)` para +0x74 y +0x78 (dibuja el modelo en su textura).
  - En pantalla, en 0x456A9..0x456E3: dos objetos de `sub_0006BA40` (+0x40/+0x44 de otra escena) reciben la
    textura de cada visor con `sub_0005A430` y se muestran con `sub_0006B540`.
- Plan de la mascota: armar un tercer visor igual (mismo constructor y argumentos, lado 0), cargarle el modelo
  con `sub_0005A670`, dibujarlo cada cuadro como en 0x46100 y mostrar su textura chica en un costado con un
  objeto como el de `sub_0006BA40`. Sin tocar los dos de la batalla.
- Pedido del usuario (2026-11-14): una mascota **por jugador**, como Master Duel. Cada rival con la suya fija y
  el jugador elige la propia. Encima del Deck extra (Fusión) de cada uno. Para la prueba, un solo modelo.
- Leído del código (cómo carga la batalla un modelo y se lo da al visor):
  - Carga: en 0x76041..0x760D1, el número n de modelo va con el formato de 0x423B58 a una ruta
    (`sub_00356B93` = sprintf, carpeta n/100*100). Después `sub_0001DB20(n, 1)` y
    `sub_0001ADF0(ruta, 1, 0, 0, 0)` → manija, guardada en [batalla+0x12904/+0x12908].
  - Listo: `sub_00018FE0()` y `sub_000199C0(manija)` ≠ 0 (`sub_00075E00` espera a los dos).
  - Nodo: `sub_00019AF0(manija)` → raíz; `sub_00013610(&ptr, raíz)` → puntero con cuenta de referencias en +8
    (`sub_00076350`).
  - Al visor: `sub_0005A670(visor; nodo, n)` (ret 8): lo cuelga de [visor+0x30] con `sub_00258AC0`; nodo en
    +0x34, n en +0xD8.
  - Cada cuadro: `sub_00056FC0(visor, render)`. El segundo visor (lado 1, el atacado) ya sale **espejado**
    (X × −1): sirve para la mascota del rival.
- Falta: el cuadro en pantalla. Hoy `sub_00057D60` lo pega a toda la altura (vtable +4 del visor) y las texturas
  pasan a objetos de `sub_0006BA40` (`sub_0005A430` → `sub_00059150`; `sub_0006B540` cambia la textura de
  [obj+0x10]). Hay que dibujarlo chico en la posición del Deck extra de cada jugador.
- Leído en la batalla (0x7D4D0..0x7D720, `sub_0007D230`):
  - Saca el modelo anterior: `sub_0005A760(visor)`.
  - Pone el nuevo: `sub_0005A670(visor; nodo, n)`. El nodo sale de `sub_0007B870` + `sub_00058B20` (caché).
  - Prende el visor: `sub_00057020(visor, 1)` (+0xCC). Solo con +0xCC prendido, `sub_00056FC0` llama a
    [[visor+0x2C]]->vf0 (el render a textura).
  - Cámara: `sub_000583A0(visor; vec3, vec3, vec3 (0,1,0))`, los vectores por valor (9 floats).
- El cuadro en pantalla (`sub_00057D60`): 8 tiras ([visor+0xC..+0x28], objetos vtable 0x429964 de
  `sub_0005B3C0`, hijas de [visor+8], la capa de `sub_0005AF00`). Cada una con 4 vértices: posición por
  vtable +0x5C (y negativa), color +0x8C (alfa 0,1 / 0,22 / 0,47 / 1: bordes que se desvanecen), coordenadas
  de textura +0xB0. Las posiciones dependen del avance [visor+0xC0]/[visor+0xC4] (la entrada deslizándose) y del
  lado (+0x44).
- **Diagnóstico compilado** (`src/mascota.c`, exe 19:38, en el juego): no cambia nada en pantalla.
  - Envuelve `sub_00057D60` (vuelca visor, capa, tiras, geometría y sus búferes en las 6 primeras llamadas),
    `sub_00057020` (prendido/apagado) y `sub_000583A0` (cámara).
  - Agregadas a PADS en rename_pads.py y a CMakeLists.
  - Con el registro de un duelo que tenga un ataque entre monstruos se ve dónde quedan los vértices y se arma
    el cuadro chico de la mascota sobre el Deck extra. Líneas `[MASCOTA]` en registro.txt.
- **Primera versión programada** (exe 19:48, sin probar), `src/mascota.c`, F2 / Configuración "Mascotas en el
  duelo" (`mascotas`, Sí por defecto, 6 idiomas):
  - `sub_000444B0` envuelta: al armarse la escena del duelo crea dos visores igual que el juego (lado 0 y 1) y
    pide Kuriboh (m057, `MODELO_PRUEBA`).
  - `sub_00056FC0` envuelta: después del segundo visor de batalla, cada mascota:
    - espera el modelo;
    - lo cuelga (`sub_0005A670`) y prende el visor (`sub_00057020`);
    - le pone la cámara B de la tabla 0x42A718;
    - arma el cuadro y lo dibuja en cada cuadro.
    Si cambia la escena, se olvidan.
  - El cuadro: `recomp_manual.c` pregunta a `ygo_mascota_lookup` en cada llamada indirecta. Mientras se arma el
    cuadro de una mascota, la llamada +0x5C (posición de vértice) pasa por `setpos_mascota`:
    - una pasada mide el cuadro;
    - otra lo achica a una caja de 120 px parada sobre el Deck extra (zona 12: 184,299 y 439,122).
    - El signo de cada eje se toma de dónde cae el cuadro entero.
  - Registro: `[MASCOTA] i: visor ... manija`, `lista en N ms`, `cuadro x .. y .. -> centro .. escala`, más el
    volcado de los visores de batalla en los primeros ataques.
  - Sin comprobar: si el fondo de la textura es transparente y si el cuadro se dibuja fuera de la batalla.
- **Prueba 1 del usuario (19:52):** no se vio nada.
  - Registro: Kuriboh cargó en 922 ms, los dos visores prendidos, cámara puesta.
  - El cuadro quedó fuera de pantalla: el juego pasa x = −columna (32 puntos x −480..100 / −740..−160,
    y −250..480; las filas bajo 0 y las columnas de más son la entrada deslizándose) y `cuadro()` tomó el signo
    al revés (+1).
  - Arreglo: la imagen en reposo (filas del propio visor +0x90..+0xBC) es columnas 0..480 (lado 0) o 160..640
    (lado 1) y filas 0..480. Su centro (−240 o −400, 240) va al centro de la caja con escala 120/480.
- **Lentitud** ("un poco más lento aunque juegue a 90 fps"):
  - Con mascotas, el registro muestra 3 clears por cuadro (antes 1) y unas 333 draws por cuadro (antes ~268): dos
    renders 480x480 más en cada cuadro.
  - Ahora cada mascota se redibuja un cuadro sí y otro no, por turnos (la textura guarda la última imagen).
  - Exe 19:54, sin probar.
- **Prueba 2 (19:58):** tampoco se ve.
  - Registro: cuadro bien ubicado (centro −184,239 / −439,62, escala 0,25); 2 clears por cuadro (renders
    alternados).
  - Causa leída del código: la capa del visor solo se dibuja si algo la cuelga en una escena. La escena 2D del
    duelo, [0x656F8C] (`sub_000451C0`), da la capa de cada visor de batalla (`sub_0005A430(visor; &ptr, 0)`,
    0x456AD) a su objeto de efectos +0x40/+0x44 (`sub_0006BA40`, 0x880 bytes, hecho con el render
    [s2+0x14] / [s2+0x18]). Ese objeto cuelga sus nodos de ese render (`sub_0004AB50(render; nodo)` = hijo de
    [render+4]).
  - `sub_0006B100` (0x43B59, cada cuadro) es la actualización de efectos con temporizador [+0x18], no el dibujo.
  - Arreglo: la capa de cada mascota se cuelga directo del mismo render (lado 0 → +0x14, lado 1 → +0x18).
    Registro: `[MASCOTA] i: capa ... colgada del render ...`. Exe 20:00, sin probar.
- **Prueba 3 (video 20-03-58):** sin mascota.
  - El video confirma el lugar: la zona de fusión (remolino) cae en (183,289) la del jugador y en (433,116) la del
    rival, que son las de la zona 12.
  - Registro: capa colgada del render [s2+0x14]/[+0x18]. Ese render solo se dibuja por el objeto de efectos
    (`sub_00068B40`, si el byte +0x87C está prendido = en batalla).
  - La causa real: el objeto 0xD0 de cada visor (`sub_00057040`) es un render de pantalla (`sub_0004C060` +
    orto `sub_001E6540(0,640,480,0,1,100)`, vtable 0x429940). El visor cuelga ahí su capa ([visor+0x40]). El
    duelo dibuja los de batalla en cada cuadro con `sub_00043800(k, r)` = [esc+0x6C+4k]->vf0(r) (0x4615A,
    0x46187); los de las mascotas no los dibujaba nadie.
  - Arreglo: `sub_00043800` envuelta (PADS); después de k 0 dibuja el render 0xD0 de cada mascota lista. Se
    sacó el enganche al render de efectos. Exe 20:1x, sin probar.
  - El usuario: por ahora sobre el Deck extra; más adelante quizá otro lugar del campo.
- **Prueba 4 (20:11):** nada visible.
  - Ahora sí se dibuja: unas 343 draws por cuadro, antes 333.
  - Causa: la opacidad. `sub_00057D60`:
    - con [v+0x45] = 0, corre la imagen 480 afuera y le da opacidad c0/c4 (0/0 del constructor);
    - con 1, la corre (c4−c0)/c4·480 con opacidad 1 (constantes 0x42947C = 480, 0x4236B4 = 1, 0x42994C = 230,
      0x429948 = 405, 0x429944 = 160).
  - Arreglo: +0x45 = 1, c0 = c4 = 1 (en reposo, opaca, como la deja la batalla). La primera pasada mide la imagen en
    reposo ("en reposo x .. y .." en el registro), su centro va al de la caja, el lado mayor a 120 px y el signo
    de x se toma del centro. Exe 20:13, sin probar.
- **Prueba 5 (20:23): se ve algo**: "una especie de pasto transparente de color morado que se mueve".
  - Registro: en reposo x −480..100 / −740..−160, y 0..480; escala 0,207; centros bien.
  - Causa: la cámara B (el primer plano del final del ataque). Para m057 queda en (6,45, 6,87, 1,09) mirando a
    (0,57, 5,65, 2,42), a unos 6 del modelo: solo se ve el pelo.
  - Arreglo: la cámara A (el comienzo del ataque, posición d[4..6], destino d[1..3], arriba d[7..9]). Para m057
    queda en (−16,77, 10,06, 17,43), a unos 25 del modelo. Exe 20:24, sin probar.
- **Prueba 6 (20:29):** con la cámara A, "se ve como un pasto todavía". La cámara A quedó aplicada
  (`sub_000583A0` → `sub_0004AC10([visor+0x2C]; pos, destino, arriba)` en el momento).
  - Kuriboh es una bola de pelo: en 120 px puede verse así.
  - Prueba: `MODELO_PRUEBA` 0 (Ojos Azules, m000) y caja de 160 px. Exe 20:30, sin probar.
  - El usuario no puede sacar capturas por ahora (ni Windows ni el juego).
- **Prueba 7 (video 20-35-24, Ojos Azules, 160 px): la mascota aparece en su lugar**, junto a cada zona de
  fusiones, pero casi transparente: solo trazos blancos y celestes.
  - Causa: `sub_00057D60` pone a cada punto un color (vtable +0x8C: this; índice, &rgba) con alfa en degradé
    (0,1..1, el borde suave de la batalla).
  - Arreglo: en la segunda pasada, la +0x8C de la mascota pasa por `setcol_mascota` (alfa 1). Exe 20:36, sin
    probar.
- Capturas de Windows del usuario: había 37 SnippingTool.exe colgados (desde las 18:20 y uno del 12/11). Se
  cerraron todos.
- **Prueba 8 (captura, alfa 1):** igual. Solo se ven los brillos blancos y celestes del Ojos Azules, en el lugar
  correcto (junto a cada zona de fusiones).
  - Causa: la luz del visor. La batalla llama en cada cuadro a la vtable +4 del visor (`sub_0005A400` /
    `sub_0005A460`), que llama a `sub_00058420`: gira la luz [v+0x38] (ángulo +0x3C, dirección sen/cos,
    altura 10, vtable +0x74).
  - Arreglo: las mascotas llaman a `sub_00058420` antes de su render en cada cuadro. Exe 20:40, sin probar.
- **Prueba 9 (captura, con luz):** igual, "todo cortado".
  - Repasado contra la batalla: la carga del modelo es la misma (`sub_0007B870` usa el cargador de +0x350 →
    `sub_00058B20`).
  - El estado de entrada también: `sub_000584E0(0)` en el constructor oculta (alfa 0, c4 = 1, c0 = 0);
    `sub_00058760` arranca 20 cuadros; `sub_0005A460` (vtable +4) = cámara `sub_000585E0` + luz `sub_00058420`
    + c0−− + `sub_00057D60` mientras c0 > 0. Con +0x45 = 1 y c0 = c4: columnas 0/230/405/480, alfa 1.
  - La mezcla de la capa: `igAttrSet` + `igBlendStateAttr` prendido, 2 etapas `igTextureAttr` /
    `igTextureBindAttr` / `igTextureStateAttr` / `igTextureFunctionAttr` (3,1 y 3,0) /
    `igTextureCoordSourceAttr`.
  - **Diagnóstico** (exe 20:54):
    - las dos mascotas se renderizan en todos los cuadros (sin alternar);
    - la del jugador queda a tamaño de batalla, sin achicar y con la opacidad del juego;
    - la del rival, achicada.
    - Una captura dice si la imagen está bien (la grande) o si la arruina el achicado (la chica).
- **Prueba 10 (captura, grande sin achicar):** también cortada: garras y bordes del Ojos Azules, sin cuerpo.
  - **Causa:** la imagen 480x480 del visor. El modelo escribe un alfa bajo en su cuerpo. En la batalla no se
    nota porque antes va un fondo opaco (el efecto por atributo); la mascota no tiene fondo.
  - **Arreglo en el kit** (`xboxrecomp-apex/src/d3d/nv2a_pb_d3d11.c`, `blend_state()`):
    - `g_nv2a_alpha_uno_480` = el juego lo pide. Cada dibujo hacia una imagen de 480x480 conserva su mezcla de
      color y escribe alfa = 1 (SrcBlendAlpha = BLEND_FACTOR con bf[3] = 1, DestBlendAlpha = ZERO).
    - Excepto si el color mismo usa el blend factor.
    - Clave de caché key[5]. Aviso único en el registro: `[d3d11] alpha 1 into the 480x480 targets`.
  - `mascota.c` lo prende cuando una mascota está lista y lo apaga al cambiar de escena.
  - Se sacó el diagnóstico grande y volvió el render alternado (dibujar en todos los cuadros no cambiaba
    nada). Exe 20:59, sin probar.
- **Prueba 11 (captura 21:08, exe 20:59): igual, trazos.** El alfa forzado no cambió nada → la "causa" de la
  prueba 10 era una suposición sin medir. El registro sí confirma: cuadro chico en su lugar, toda la imagen
  480x480 (uv 0..480) dibujada, y la imagen viene del render (2728 "render-target textures").
  - **Hipótesis nueva (sin confirmar): la profundidad de la imagen de la mascota no se borra cada cuadro.**
    Con el color borrado y la profundidad vieja solo pasan los píxeles que quedaron más cerca que el cuadro
    anterior: las partes que se movieron = trazos que se mueven (lo que describe el usuario).
  - **Medición + arreglo en el kit** (`nv2a_pb_d3d11.c`, exe 21:15, sin probar):
    - `[T480]` en el registro: para cada imagen 480x480 (hasta 6: mascotas y monstruos de batalla) anota sus
      clear, zclear y cada dibujo (profundidad, función, máscara, alpha test, mezcla, textura) en sus cuadros
      1, 2 y 30. Al final del cuadro 30 guarda `diagnostico_480\<offset>_color.png`, `_rgb.png` (sin alfa)
      y `_profundidad.png` en la carpeta del juego.
    - Arreglo: con mascotas activas, si una imagen 480x480 dibuja con profundidad sin zclear en ese cuadro,
      el port la borra (1.0) antes del primer dibujo. Línea `[T480] ... the port clears it`.
    - Si sigue igual, los PNG muestran qué hay en la imagen (mascota vs. batalla) y se compara.
- **Prueba 12 (captura 21:21, exe 21:15): igual, pero el registro `[T480]` y las fotos dieron la causa real.**
  - Medido: el juego borra color y profundidad de cada imagen 480x480 en cada cuadro (la hipótesis de la
    profundidad era falsa; se sacó ese borrado del port). `diagnostico_480\01517000_rgb.png`: **la imagen de la
    mascota es un Ojos Azules entero y opaco**, tan buena como la de batalla (el Golem de 01070000).
  - **Causa:** la capa que pega la imagen en pantalla (`sub_0005B4A0`) tiene en su etapa 1 la máscara
    `duel/mask_00.png` (cadena 0x4299AC; blanca arriba, negra abajo, borde de picos): la cortina de entrada del
    monstruo en batalla. `sub_00057D60`: con +0x45 = 1 las filas son 0, 230−s, 405−s, 480−s, 480 con
    s = (c4−c0)/c4·480; posición x = −(columna + sesgo + lado), y = fila; la coordenada de textura es la columna
    y la fila sin sesgo (0x581A0). La batalla termina con c0 = 0 (s = 480): la imagen queda en la franja blanca.
    La mascota tenía c0 = c4 (s = 0): el comienzo de la cortina → solo los picos = "trazos" / "pasto morado".
  - **Arreglo** (`mascota.c`, `cuadro()` / `setuv_mascota`): c0 = 0, c4 = 20 (la batalla en reposo); se captura
    +0xB0 (coordenada de textura, set 0) de cada punto, se recorta a 0..480 y el punto se ubica desde ella en la
    caja de la mascota (fuera de la imagen las franjas quedan en nada). Sin forzar colores (+0x8C) ni el
    pase de medición. El registro `[T480]` sigue (fotos de cada 480x480 en el cuadro 30). Exe 21:27, sin probar.
- **Prueba 13 (captura 21:33, exe 21:27): ¡LAS MASCOTAS SE VEN!** (Ojos Azules entero, animado, en los dos lados).
  Confirmado por el usuario: "bien ya salen". Queda: "se nota que están dentro de un cuadrado" (alas y cola
  cortadas por el borde de la imagen 480x480: la cámara A de batalla no encuadra el modelo entero).
  - Pedido nuevo: ubicarlas **arriba del nombre del jugador y abajo del nombre del rival**.
  - Medido en la captura: el cuadro 640x480 se estira a la ventana (sin bandas hor+) → px = columna×3,
    fila×2,25. Cartel "Dennis": col 27..153, filas 240..271; "Duel Computer": col 483..613, filas 118..149.
  - Cambios (`mascota.c`, exe 21:36, sin probar): cámara `LEJOS` 2.4 veces más lejos que la A (mismo objetivo);
    `ESC` 0.5 (imagen → cuadro); centros `k_caja` {120,178} jugador y {524,182} rival, calculados con dónde cae el
    Ojos Azules en cada imagen (fotos 21:21 escaladas): el del jugador parado sobre su cartel (fila ~236), el del
    rival colgando bajo el suyo (fila ~155). Otros modelos pueden caer distinto en su imagen: se ajusta por modelo.
- **Prueba 14 (capturas 21:45, exe 21:36):** sin cortes, pero grandes y chocando con los nombres; además la del
  jugador se dibuja **encima del panel de la carta seleccionada** (Soldado gigante de piedra).
  - Medido en pantalla (Ojos Azules volando): u 0..300, v 62..372 en la imagen del jugador; u 192..458,
    v 80..374 en la del rival.
  - Cambios (`mascota.c`, exe 22:13, sin probar): `LEJOS` 3 (más margen en la imagen), `ESC` 0.5 → 20 % más
    chicas; centros {126,183} (patas en la fila 236, sobre "Dennis") y {531,219} (arriba en la fila 155, bajo
    "Duel Computer", corrida a la derecha del borde del campo).
  - Orden de dibujo (`sub_00046064`): capas 2D 1..5 de la escena (`sub_00044360` → `sub_0001B280`), campo 3D,
    imágenes 480, visores k 1 / k 0 (las mascotas después del k 0), después capas 7, 8, 9 y `sub_00062590`.
    Si el panel está en 7..9 y aun así queda tapado, la causa sería profundidad (las franjas escriben z).
  - **Medición** `[T480C]` en el kit: dos cuadros después de la primera mascota, cada dibujo de ese cuadro (hasta
    400) con destino, profundidad (prendida, función, máscara), textura y rango de posiciones/z. Con eso se decide
    dónde dibujar las mascotas para que queden debajo de los carteles y el panel.
- **Prueba 15 (captura 22:20, exe 22:13): tamaño y lugar "perfecto"** (confirmado). Faltaba: el panel de la carta
  por encima de la mascota, que no aparezca antes de que arranque el duelo, y quedar detrás de los monstruos de
  batalla.
  - `[T480C]` (un cuadro entero, en orden): templo 3D → capas 2D (PV, diamantes, carteles que entran
    deslizándose, barra de abajo) → campo/cartas 3D → render de las imágenes 480 → franjas de batalla (k 1, k 0)
    → mascotas → capas 7..9. Las franjas tienen prueba de profundidad LEQUAL sin escritura (z 10).
  - Orden del cuadro del duelo (`sub_00045FB0` → `sub_00046064`): scene2 1, capa 0, scene2 2 y 3,
    `sub_0005C3F0(0)`, capas 2D 1..5 (`sub_00044360(r, k)` → `sub_0001B280`), [0x66DCD8] vf4, campo
    (`sub_00043830`), imágenes, k 1, k 0, capas 7..9.
  - **Arreglo** (`mascota.c`, exe 23:28, sin probar): las mascotas se dibujan dentro de `sub_00044360` cuando k = 1
    (PADS + `rename_pads.py`), antes de la capa 2D 1: sobre el templo, debajo de carteles, panel, campo y
    monstruos de batalla. La imagen se renderiza más tarde en el cuadro (muestra la del cuadro anterior).
  - **Solo durante el duelo:** `[0x657148]` es el estado del duelo (0x48805..0x48AF8): 3..5 la apertura, 6 el
    duelo hasta que `sub_0002E360` da 0xF, 7+ el resultado. Mascotas (render y dibujo) solo con estado 6, y recién
    después de su primer render (`dibujada`).
- **Prueba 16 (captura 23:00, exe 23:28): "quedó genial"** → CONFIRMADO: mascotas sobre/bajo los nombres, debajo
  del panel y de la batalla, solo durante el duelo. Pedido: agregar los modelos que faltan.
- **Los 5 modelos + encuadre automático** (exe 23:40, sin probar):
  - Modelos `k_modelo` = Ojos Azules 0, Kuriboh 57, Mago Oscuro 34, Jinzo 854, Cráneo Convocado 21.
  - **Tu mascota:** opción nueva `mascota_tuya` "Tu mascota" (F2 y Configuración, 6 idiomas; por defecto Ojos Azules).
  - **Rival:** fija por personaje (`0x66DCDC + 4`, lo que pasa `sub_001037D0` al duelo; personajes 0..27 en el orden
    de `modding/dialogos` 01..28), tabla `k_rival`: Yami Yugi/Arkana/Ishizu/Shadi/Shimon Mago Oscuro; Seto/Rex Ojos
    Azules; Joey/Trusdale/Bandit Keith/Espa Roba/Strings/Duel Computer Jinzo; Yugi/Tea/Tristan/Mai/Mako/Weevil
    Kuriboh; Bakura/Yami Bakura/Pegasus/Bonz/Rare Hunter/Lumis & Umbra/Odion/Marik/Yami Marik Cráneo. 28+ Kuriboh.
  - **Encuadre automático:** cada modelo cae distinto en su imagen 480 (y distinto según el lado). `mascota.c`
    registra el rectángulo de las franjas de cada mascota en `g_nv2a_mascota[2]` (kit, `nv2a_pb_d3d11.c`) y pide
    una medida. El kit encuentra la imagen (primer dibujo 2D con etapa 0 de 480x480 dentro del rectángulo), oculta
    las franjas mientras `ocultar`, y en los cambios de cuadro lee la imagen (alfa > 16, cada 2 px) 24 veces cada
    5 cuadros y devuelve la unión (u0, v0, u1, v1). Toca el borde → cámara ×1,35 y otra medida; chica (< 150 px)
    → ×1/1,3 (máx. 6 intentos). Ubicación como el Ojos Azules aprobado: escala máxima que entra en 120×124 px del
    cuadro, jugador con la base en la fila 236 centrado en la columna 90, rival con el tope en la fila 155 centrado
    en la 565. Se mide durante la apertura (estados 3..5, render y franjas ocultas); se muestra en el estado 6.
    Memoria por modelo y lado (`s_encuadre`) para los duelos siguientes. Registro: `[MASC]` (kit) y
    `[MASCOTA] ... medida / modelo N en u..` (juego).
  - Se sacó la medición `[T480C]`; `[T480]` (fotos de las imágenes 480 en `diagnostico_480\`) sigue.
- **Prueba 17 (captura con F2):** las mascotas andan con otros modelos (Mago Oscuro del jugador; encuadre
  automático: Mago Oscuro medido de una, Kuriboh "chico" → cámara ×3 → ×2,31 → ×1,78). La Duel Computer salió con
  Kuriboh: es el **personaje 32** del menú de rivales (duelista 30), no el 27. Los nombres 0..26 coinciden con los
  personajes (lista de default.xbe, textos 86..112); desde el 27 la tabla s8 0x423D60 va aparte (27..30, 34, 35 →
  27; 31 → 29; 32 → 30; 33 → 31). `k_rival` ahora llega a 35 (32 Jinzo; 28..31, 33..35 Kuriboh); el registro anota
  `[MASCOTA] rival: personaje N`.
- **F2 limpio** (pedido del usuario): ocultas "Modo del ratón", "Onda al hacer clic", "Cartel de premios del rival",
  "Premios sin cuarta copia", "Aviso de carta nueva", "Modo experimental", "Volcar texturas a carpeta" e
  "Imagen 2D" (y los títulos Imagen y Rendimiento, que quedaban vacíos). **Ocultas, no borradas:** nuevo
  `xbox_MenuAddHidden` / `xbox_MenuHide(key)` en el kit (`host_menu.c`): se cargan y guardan en ygo_menu.ini igual
  (si no, al sacarlas volvían a su valor por defecto, p. ej. el puntero estilo PC) y siguen en Configuración.exe.
- **Más mascotas, cambio en vivo, Configuración limpia** (pedido del usuario; exe 23:57, sin probar):
  - **15 modelos** (`k_modelo`, mismo orden que `k_mascota_tuya` en main.c y la lista de Configuración): Ojos Azules 0,
    Kuriboh 57, Mago Oscuro 34, Jinzo 854, Cráneo Convocado 21, Ojos Rojos 81 (4088), Chica Maga Oscura 774 (4766),
    Gaia el Caballero Feroz 37 (4044), Guardián Celta 40 (4047), Dama Arpía 61 (4068), Mago del Tiempo 15 (4022),
    Dragón Bebé 3 (4010), Espadachín de la Llama 14 (4021), Soldado del Brillo Negro 363 (4370), Buster Blader 860
    (4983). Número de modelo de la tabla del juego carta → modelo (0x44FBB0, `tools/cartas_mod.py modelo_de`); cada
    .igb comprobado en Game\monster. (Ojo: el modelo NO es siempre ID − 4007: Jinzo 4758 → m854.)
  - Rivales con los nuevos: Joey y Rex Ojos Rojos, Tea Chica Maga Oscura, Tristan Espadachín, Trusdale Mago del Tiempo,
    Mai Dama Arpía, Ishizu Soldado del Brillo Negro, Odion Guardián Celta, Shadi Gaia, Shimon Buster Blader.
  - **Cambio de "Tu mascota" durante el duelo:** antes se elegía al armar el duelo ("tuve que reiniciar"). Ahora
    `tick` ve el cambio, carga el modelo nuevo en el mismo visor (`cargar`, `sub_0005A670` lo reemplaza como en cada
    batalla) y lo mide/encuadra oculto (o usa la medida guardada).
  - **Configuración.exe:** sacadas las mismas 8 opciones que en F2 (imagen_2d, raton_modo, onda_clic, tex_dump,
    cartel_premios, premios_sin_cuarta, aviso_nueva, modo_rapido). Su `Save()` reescribe solo sus claves y deja
    las demás líneas: los valores ya guardados siguen. Sin archivo, el juego usa sus valores por defecto (iguales).
- **Prueba 18 (2026-11-15 00:02):** "la mayoría se vieron bien", pero tardan (~10 s algunos), Kuriboh "no se ve" y,
  pasando toda la lista hasta Jinzo, **crasheo** ("disco dañado").
  - **Crasheo = memoria llena:** `xbox_HeapAlloc: out of memory (requested 1708032, used 49446912/50528256)` al cargar
    m854. Cada cambio de mascota cargaba un modelo y nunca soltaba el anterior; y cada duelo dejaba sus dos modelos
    cargados. **Arreglo:** `soltar(h)` = `sub_00018FE0()` + `sub_00019700(h)` (cdecl), como la batalla suelta los
    suyos al terminar (`sub_00075DB0`): el anterior se suelta cuando el nuevo ya está puesto (`lista`, `viejo`), uno
    que todavía carga se suelta enseguida, y `soltar_todo()` al armar el duelo siguiente o cambiar de escena.
  - **Kuriboh / demora:** Kuriboh salía "chico" → la cámara se acercaba y se medía otra vez (3 medidas, ~8 s oculto);
    el usuario pasó al siguiente antes. Se sacó el acercamiento (la imagen interna es 4× lo que se muestra: agrandar
    no pierde detalle); solo se aleja si toca el borde (máx. 4). Medida del kit: 16 muestras cada 4 cuadros (~1 s).
    **Medidas de fábrica** (`s_encuadre`, cámara ×3) de los 14 modelos del lado del jugador + Jinzo del rival,
    sacadas de ese registro: aparecen al instante. Al cambiar, la mascota no se dibuja hasta que su imagen tiene el
    modelo nuevo (`dibujada = 0`). Exe 00:05, sin probar.
- **25 mascotas y la de cada personaje** (pedido: "faltaría Exodia" y "su mascota ideal a cada personaje"; exe 00:09,
  sin probar): + Exodia 20 (4027), Dragón Barril 739 (4749), Rey Rex de Dos Cabezas 31 (4038), Gran Polilla 66
  (Polilla Perfectamente Definitiva 4073), Pescador Legendario 921 (5069), Restricción de Mil Ojos 903 (4740),
  Necrofear Oscuro 930 (5222), Bestia Enmascarada 902 (5094), Comandante Cibernético 420 (4427), Dragón Zombi 96
  (4103); también en "Tu mascota" (F2 y Configuración, 6 idiomas).
  - Rivales: Yami Yugi Mago Oscuro · Seto Ojos Azules · Joey Ojos Rojos · Yugi Kuriboh ·
    Tea Chica Maga Oscura · Tristan Comandante Cibernético · Abuelo (Trusdale) y Cazador de Cartas Raras Exodia ·
    Yami Bakura Necrofear Oscuro · Pegasus Restricción de Mil Ojos · Mai Dama Arpía · Bandit Keith Dragón Barril ·
    Mako Pescador Legendario · Rex Rey Rex de Dos Cabezas · Weevil Gran Polilla · Bonz Dragón Zombi · Espa Roba Jinzo ·
    Arkana Mago Oscuro · Lumis y Umbra Bestia Enmascarada · Duel Computer Jinzo. Sin modelo para su carta (quedan como
    estaban): Strings (Slifer) Jinzo, Ishizu Soldado del Brillo Negro, Odion Guardián Celta, Bakura y Marik Cráneo
    Convocado, Shadi Gaia, Shimon Buster Blader.
  - Los 10 nuevos no tienen medida de fábrica: la primera vez se miden (~1 s, ocultos) y quedan guardados.
- **Prueba 19 (2026-11-15 00:10, capturas de 5 mascotas):** "se ven geniales". Pedidos: Kuriboh más chico; Dragón
  Zombi, Bestia Enmascarada, Pescador y Rey Rex más a la izquierda (chocaban con el PV / diamante); Pescador más
  arriba. Problemas: al cambiar rápido 2 veces dejó de mostrar mascotas; la primera vez de cada una tarda.
  - **La pérdida de memoria real:** `lista()` sumaba +1 al nodo para el argumento de `sub_0005A670`, pero esa función
    toma su propia referencia (inc 0x5A6F0) y suelta la del argumento (dec 0x5A72B): la referencia de nuestra copia
    (`sub_00013610`) nunca se soltaba → ningún modelo de mascota se borraba (el heap seguía en 49,3/50,5 MB aunque se
    soltaran las manijas; el modelo siguiente no cargaba: "el modelo no cargo en 20 s"). Ahora se suelta (como la
    batalla con su temporal, 0x76413).
  - **Tope a la derecha** (jugador): el modelo termina a la izquierda de la columna 132 (PV desde 147, diamante ~135);
    los anchos se corren a la izquierda (no más allá de la 8). **Retoques** `k_retoque`: Kuriboh ×0,7; Pescador 12
    filas más arriba.
  - **Medidas:** 11 más de fábrica (Jinzo, Exodia a ×4,05, Dragón Barril, Rey Rex, Gran Polilla, Pescador, Mil Ojos,
    Necrofear, Bestia Enmascarada, Comandante, Dragón Zombi) y las nuevas se guardan en
    `Ajustes\mascotas_encuadre.txt` (se leen al primer duelo): cada modelo se mide una sola vez.
- **Prueba 20 (00:19 exe):** bien, pero cada modelo sale girado y ubicado distinto. Pedido: "que todos salgan del mismo
  lado, como la Maga Oscura" → el usuario eligió las 3 cosas: **misma altura** (flotando), **mismo lugar** (desde su
  borde izquierdo) y **mirando hacia el mismo lado**.
  - Cámaras A de la tabla (default.xbe 0x42A718): azimut −19..−59°, elevación −16..+22° según el modelo. Ahora todas
    con el ángulo de la Chica Maga Oscura (az −35,4°, el −2,1°), con su objetivo y distancia propios; el rival
    espejado (az +35,4°, mirando al campo desde el otro lado). `camara()` en `mascota.c`.
  - Jugador: borde izquierdo en la columna 60 (el de ella) y crece a la derecha; si pasa la 132 se corre (no más
    allá de la 8); la base en la fila 226 (flotando como ella, antes 236). Rival: borde derecho en la 620, el
    izquierdo no antes de la 508.
  - Las medidas del ángulo viejo no sirven: queda solo la de ella; las demás se miden de nuevo una vez y se guardan
    en `Ajustes\mascotas_encuadre2.txt`. Si algún modelo del juego está armado mirando para otro lado, puede salir
    distinto aunque la cámara sea la misma. Exe 00:32, sin probar.
- **Prueba 21 (exe 00:32):** "para donde mira está bien", flotando bien. Falta: siguen saliendo de lados distintos
  (el Cráneo más a la derecha, no centrado como la Maga) y el Jinzo del rival mira hacia afuera.
  - El lado del rival ya sale espejado por el visor: con +35,4° miraba hacia afuera (con su cámara A, −46°, miraba al
    campo). Ahora el mismo −35,4° en los dos lados.
  - Jugador **centrado en la columna 82** (el centro de la Maga), no crecido desde su borde; si pasa la 132 se corre a
    la izquierda (no antes de la 8). Rival centrado en la 560 (entre la 508 y la 632).
  - Medidas en `Ajustes\mascotas_encuadre3.txt` (las del rival cambiaron de ángulo). Exe 00:38, sin probar.
- **Exodia entero, Maha Vailo y flechas del triple** (exe 01:06, Configuración 01:06, sin probar):
  - Fondos de diálogo de cada personaje = `cf_anim/bg/cfbgNN.png` (NN = personaje). Bakura (07) = Maha Vailo
    (4499, modelo 492): mascota nueva (26). Shadi, Shimon, Marik y Yami Marik tienen mágicas/trampas (Pote de la
    codicia, Tifón espacial místico, Agujero oscuro): sin modelo, siguen con la suya.
  - Exodia con brazos y piernas = **modelo 737** del juego, guardado aparte en `effect/special_igb/se002.igb` (la
    animación de Exodia: `sub_000662A0`, cargado por `sub_000625C0(9)` = `sub_0001ADF0(ruta,1,0,1,0)` sin
    `sub_0001DB20`; nodo "737", la tabla de cámaras tiene la entrada 737). Reemplaza a la cabeza (20) en "Exodia".
    `se000` son las 5 cartas volando, `se001` el círculo. ~280×215 unidades (la cabeza ~39×32): arranca con la
    cámara a 20× (`LEJOS_EXODIA`) y los reintentos por borde la alejan.
  - Duelo triple: `sub_001057D0` (estado +0x3D8: 0 nivel con arriba/abajo, derecha → orden; 1 orden: +0x3F4 flecha
    0/1, A intercambia). `raton.c tri_pointer`: click en las flechas (x 343 / 453, y 213) camina con las teclas del
    juego y pulsa A; click en los triángulos del nivel (x 83 / 190, y 173) arriba/abajo; otro lado A; derecho B.
  - Shadi (cfbg25, el usuario): **Golem del Milenio** (4459, modelo 452), mascota 27. Maha Vailo y el Golem también
    en "Tu mascota" (F2 y Configuración, 6 idiomas). Exe y Configuración posteriores a 01:06, sin probar.
  - 01:10 probado: Bakura y Shadi bien; Exodia entero sale pero pixelado (medido 13×23 px a cámara ×20). Exe 01:15:
    `LEJOS_EXODIA` 2,3 y se descarta la medida ×20 guardada. Sin probar. Menús (F2/Configuración) sin probar.
  - 01:15 probado: Exodia nítido, pero se notaba el bucle de 19 s y se pisaba con los PV. Exe 01:25: la mascota carga
    `effect/special_igb/se002_mascota.igb` (`tools/modelos/exodia_mascota.py`: cada pista muestreada en su propia
    animación en 7,6 s ±0,12 s, 7 vaivenes por bucle; la pose de la captura del usuario). `copiar_al_juego.sh` lo copia
    a `Game\effect\special_igb\`. Medidas de 737 con clave aparte (`CLAVE`, +100000): se mide de nuevo. Sin probar.
  - Ataque: los modelos traen una sola animación (reposo). El de Exodia entero tiene el golpe (17–18,5 s): se podría
    armar otro .igb con ese tramo para cuando ataca o gana. Pendiente, a pedido del usuario.
- **Exodia al ganar, reposo más lento, triple y menú principal** (exe tras 01:25, sin probar):
  - El usuario: el golpe "solo para cuando ganas con exodia, no en cada ataque". `se002_ataque.igb`
    (`exodia_mascota.py`: de la pose quieta a la preparación 16,3 s → golpe 17,4 s → vuelta, 10 por bucle). La escena de
    victoria de Exodia (`sub_000662A0`, envuelta) prende `s_exodia_gana`; la mascota que sea Exodia entero, ya
    ubicada, carga la de ataque (misma medida) hasta el fin del duelo. Se apaga en `crear`.
  - Reposo de Exodia más lento ("mirá la de Jinzo o Buster Blader"): 4 vaivenes por bucle (~4,75 s), ±0,10 s.
  - Triple: click en el "9" (x 110..165) con una flecha marcada vuelve al nivel (izquierda, la salida del juego del
    estado 1); click derecho ahí lo mismo; B solo si el click derecho empezó en el nivel.
  - Menú principal (rueda de modos): `mouse_a_off` también ahí; un click fuera de los íconos ya no entra.
  - `copiar_al_juego.sh` copia todos los `modding/modelos/exodia_mascota/*.igb`.
- Diagnóstico `[T480]` apagado (`t480_slot` devuelve -1; escribía ~22 MB de PNG en `diagnostico_480\` y leía la GPU
  unos cuadros por partida). Costo de las mascotas: 2 dibujos 480×480 en cuadros alternados; lectura de la GPU solo
  mientras miden (~1 s por modelo nuevo). Registro del usuario: F8 90–120 fps. Exe 01:50.
- "Soldado cinético": no existe en este juego (ni en los nombres en español ni en inglés del default.xbe).
- **14 mascotas más** (el usuario: "sí, sumalos"; solo para "Tu mascota"): Dragón Definitivo de Ojos Azules 379,
  Dragón Negro de Cráneo 216, Mago del Caos Negro 721, Dragón Meteoro Negro 712, Valkyrion 863, Dragón Toon de Ojos
  Azules 900, Gaia el Campeón Dragón 36, Sabio Oscuro 931, Dragón del Trueno de Dos Cabezas 612, Dragón de Tres
  Cuernos 704, Reina del Cosmos 707, Zera el Mant 359, Sengenjin 702, Caballero de Cráneo 706 (NMODELOS 41). F2 y
  Configuración en 6 idiomas. Cada uno se mide la primera vez (~1 s). Sin probar.
- Capturas 01:55: Dragón Definitivo (379), Dragón Negro de Cráneo (216) y Sengenjin (702) chicos (alas/lanza ~2,5× más
  anchas que altas: ajuste por ancho 0,36–0,46) → `k_retoque` ×1,45 y pueden salir por el borde izquierdo (hasta −45).
  Kuriboh seguía grande (0,7 × ajuste 1,446) → ×0,5. Exe 02:01, sin probar.
- Dragón Negro de Cráneo +25 columnas a la derecha (`k_retoque.dx`, después de los límites). Exe 02:09, sin probar.
- Demoras medidas (registro 02:01): al empezar el duelo las dos mascotas ~0,77 s (cola de carga del duelo); cambio con
  medida guardada 0,05–0,2 s; modelo nuevo + ~1 s de medida; texturas HD del pack llegan después en segundo plano
  ("cambios tardíos"). Sin más datos, no se tocó.
- **Demora de las mascotas nuevas:** la tabla de medidas era de 40 (`NENCUADRE`) y el archivo ya tenía 40: desde la 41
  no se guardaba ni se recordaba la medida, así que cada vez se medía de nuevo (~1 s). Ahora 200. Exe tras 02:09.
- **Confirmado por el usuario (02:16):** tamaños de las mascotas bien, demora solucionada, Exodia bien (quieto y golpe
  al ganar), menú principal (click fuera de los íconos no hace nada) bien.
- Triple: el click en el 9 y el click derecho se detectaban ("back to the level") pero no volvían: el paso que aprieta
  izquierda estaba dentro de `if (want >= 0)` y "volver al nivel" es −2. Arreglado. Exe 04:51, sin probar.
- **Video del usuario (04:55): la entrada al duelo.** Orden actual: moneda del milenio (Primero/Segundo) → puertas con
  el ojo de Horus que se cierran y se abren sobre el campo → cartel violeta "Comenzar duelo" → el campo, y en ~1,3 s
  entran carteles de nombre, dados de fase y el retrato del rival → recién entonces las mascotas aparecen de golpe
  (estado 6) → se reparten las cartas. Pedido: que estén cuando llega el duelo, con una entrada estilo Master Duel pero
  egipcia.
  - Hecho (exe 05:03, sin probar): **entrada de la mascota**, 0,7 s al aparecer en el duelo (o al cambiarla): crece
    del 15 % a su tamaño con un pequeño rebote y sube desde abajo (la del rival baja); las tiras se rearman cada cuadro
    (`cuadro`, sin log) y al final una vez exacta. No se dibuja antes de que empiece (sin parpadeo).
  - Registro nuevo: `[MASCOTA] estado del duelo N (ms desde la escena)` y `entrada` / `entrada hecha`, para ver en
    qué estado se abren las puertas y adelantar las mascotas a ese momento.
- Estados del duelo medidos (registro 05:10, desde que se arma la escena): 3 a 0,77 s (puertas abiertas sobre el
  campo), 4 a 1,39 s ("Comenzar duelo"), 5 a 2,06 s (carteles, PV y retrato entran), 6 a 3,03 s. Las mascotas ya
  estaban ubicadas a 0,77 s: ahora entran en el estado 5, con los carteles (`en_llegada`). Exe 05:10.
- Cámara del templo: `sub_0005C3F0` (el templo y su luz) copia la matriz 4×4 de [this+0xC] a su cámara
  ([this+8]+0x118, `sub_001E5EE0`) antes de dibujar. Envuelta para registrarla (`[TEMPLO]`, cada estado y cada 0,25 s
  en 3..5). Exe 05:13: falta un duelo del usuario para leerla y armar el viaje de cámara.
- **Medido (registro 05:16):** la entrada de las mascotas con los carteles funciona en tiempo: estado 5 a 2,05 s →
  entrada hasta 2,75 s → estado 6 a 3,0 s. La matriz que `sub_0005C3F0` copia a la cámara del templo ([this+0xC]) es
  la **proyección** (2,414 0 0 0 / 0 3,219 0 0 / 0 0 −1 −1 / 0 0 −2 0, igual en los estados 3..6), no la vista.
  Siguiente paso del viaje de cámara (sin hacer): (1) ver si el tablero (`sub_00043830`) y las capas usan esa misma
  proyección o la suya; (2) si el templo solo: un acercamiento de su lente (escala x/y de 0,6 a 1 entre los estados 3
  y 5) o buscar su matriz de vista ([this+8] cámara, otros campos) para moverla de verdad; (3) opción "Entrada del
  duelo: Completa / Corta / Original" en F2 y Configuración (6 idiomas); (4) destello dorado con el resplandor de la
  moneda. El registro `[TEMPLO]` sigue prendido (pocas líneas por duelo).
- **Entrada del duelo (sin pedir más duelos de medida):** leyendo el código, el mismo objeto del templo tiene en
  +0x4C su ubicación (`sub_0020C210` la pasa a la cámara, [cam]+0x6C), pero su convención (mundo o vista) no está
  comprobada; el lente (+0xC) sí está medido. Hecho con el lente: en el estado 3 (se abren las puertas) el templo
  arranca con el lente abierto ×0,55 y se cierra al suyo en 1,3 s (ease out): parece que el templo viene hacia vos;
  el tablero y los carteles no cambian. Opción **"Entrada del duelo"** (F2 y Configuración, 6 idiomas): Completa
  (viaje + mascotas con los carteles) / Solo las mascotas / Original (mascotas en el estado 6, sin animación).
  `[TEMPLO] viaje de camara` en el registro. Destello dorado: no hecho. Exe y Configuración posteriores a 05:13.
- 05:23 probado: "no pasó nada". El registro muestra que el viaje corrió (lente del templo cambiado 0,72–2,02 s): lo
  que se ve detrás del tablero no se dibuja con esa cámara (la nota vieja "sub_0005C3F0 = el templo" no estaba
  comprobada). Cambio: el kit acerca la **imagen final** (`g_nv2a_zoom` en `present_blit`: uv alrededor del centro);
  en "Completa" arranca ×1,6 al abrirse las puertas (estado 3) y vuelve a ×1 en 1,3 s (ease out). Se pone en 1 en
  `crear`, al salir de la escena y fuera de los estados 3..5. No pasa por el filtro `present.shader` (no se usa).
  Exe 05:29, sin probar.
- **Centradas y destello** (el usuario: "cada mascota sale en un lado diferente ... centradas arriba del nombre"; "el
  destello de las mascotas"). Exe 05:4x, sin probar:
  - `ubicar`: la del jugador centrada en la columna 90 (cartel 27..153), la del rival en 548 (483..613), solo sin salir
    de la ventana. Se quitaron el corrimiento a la izquierda por los PV (TOPE_DER) y los `dx`/`izq` por modelo (el
    tamaño ×1,45 / ×0,5 sigue).
  - Destello: el kit suma sobre la imagen final (`destello()` tras `present_blit`, mezcla aditiva, shader propio) un
    resplandor dorado en el medio del modelo (sube en 80 ms, se va a los 650 ms) y un anillo que crece desde él;
    `g_nv2a_destello[2][6]` lo maneja `entrada()` en mascota.c. Solo en "Completa" y "Solo las mascotas".
- **05:49 — mascotas anchas achicadas** (pedido: "achicalas, en el futuro veremos 1 a 1 si vale la pena moverla y hacerla más grande"):
  centradas sobre su cartel, y si su ancho pasa del espacio libre se achican hasta caber: la del jugador no pasa de la
  columna 132 (`TOPE_DER`, diamante en 135), la del rival no baja de 508 (`IZQ_RIVAL`, diamante hasta 503) → ancho
  máximo 84 / 80. Log `[MASCOTA] i: modelo n ancho X, achicado a Y`. Mover los carteles de nombre se descartó por ahora:
  ya están a 27 px del borde y van en capas 2D junto con PV/diamantes. Pendiente futuro: revisar 1 a 1 cuáles conviene
  agrandar/mover. Exe 05:49, sin probar.
- **15:39 — ajustes 1 a 1 con capturas** (Jinzo "perfectamente centrado" = referencia). `k_retoque` ahora es
  {n, esc, arriba, dx, ancho, pies}: Mago del Tiempo 8 col a la izquierda; Bestia Enmascarada ancho 114 (como antes:
  borde derecho en 132); Comandante Cibernético ×0,8; dragones (379, 216, 0, 81, 712) ×1,45 y ancho hasta 124
  (todo el espacio 8..132, corridos lejos de los PV); Cráneo Convocado y Guardián Celta centrados **por los pies**.
  Kit: `masc_bbox` mide también la columna media de los píxeles del décimo de abajo (`pies`, promedio de las
  muestras); el archivo de medidas guarda un 7.º número; las líneas viejas sin pies solo se re-miden para los
  modelos "por pies". **Sengenjin borrado** de la lista (40 modelos; F2 y Configuración). Exe 15:39, sin probar.
- **Ajustes 2** (capturas: "agrandar estos ... centra cualquiera que puedas"): Cráneo Convocado (sigue por los pies),
  Exodia entero, Dragón Barril, Restricción de Mil Ojos y Maha Vailo ×1,3 con ancho hasta 110 (`MEDIANO`: lo más
  cerca del centro que deja el diamante, columnas 22..132); Ojos Azules 10 filas más arriba. Sin probar.
- Maha Vailo: vuelve a su tamaño (84) y se centra por la base de la túnica (`pies`), no por la capa. Sin probar.
- **Ajustes 3** (exe 15:58, sin probar; "tomá sus pies como referencia" para los descentrados): **todas** se centran
  por los pies salvo los dragones voladores, el Dragón Barril y el Mago del Tiempo (`pies` -1). Los pies ahora son
  las filas más bajas (1/25 + 4), no el décimo (tomaba la capa del Cráneo Convocado). Medidas nuevas en
  `mascotas_encuadre4.txt` (cada modelo se re-mide una vez). Dragón Meteoro ancho 124 → 100; Zera ×1,3 MEDIANO;
  Dragón Negro de Cráneo ancho 170 (las alas salen por el borde izquierdo).
- **Ajustes 4** (exe 16:04, sin probar): Jinzo de vuelta por su contorno (`pies` -1: "ya estaba centrado");
  Gaia el Campeón Dragón ×1,45 ancho 124; Dragón Negro de Cráneo centrado en el cartel (6..176) con campo nuevo
  `der` 176 en `k_retoque` (límite derecho propio: su ala derecha va por la fila ~144, arriba de los PV 186..220;
  las garras quedan en la columna ~104, antes del diamante).
- **Ajustes 5** (exe 16:1x, sin probar): Exodia vuelve a ser la cabeza (modelo 20; el entero 737 sale de la lista, el
  puñetazo al ganar ya no aplica); Golem del Milenio ×0,75; Maha Vailo por su contorno y 10 columnas a la izquierda
  (38..122); Restricción de Mil Ojos por su contorno, centrada (35..145, `der` 146: sus garras van altas).
- Golem del Milenio 6 columnas a la izquierda ("centrarlo un poco más"). Los retoques valen para los dos lados (el del rival en espejo, bajo su cartel, columna 548). Sin probar.
- **16:25 — rendimiento de las mascotas** (pedido: "se siente más lento ... 60, 90, 120 fps"). Medido en el
  registro del duelo de las 16:23: la apertura tardó estado 3 → 6 en 3,97 s (a las 05:10: 2,23 s); al crear la
  escena hubo 2 tirones de 292 y 191 ms (carga de los modelos). El destello y el zoom no cuestan nada en reposo
  (se saltean). Lo que sí escalaba con los fps: cada mascota se renderizaba cada 2 cuadros → a 120 fps, 120
  renders/s (el doble que a 60). Ahora va por reloj (QPC): una imagen cada 16 ms por turnos (~30/s cada una,
  igual que a 60 fps antes) a cualquier fps. Registro nuevo cada 10 s del duelo: `[MASCOTA] costo: N cuadros/s
  (el más largo X ms), dibujos, ms cada uno, % del tiempo`. Se sacó el registro `[TEMPLO]` de la matriz.
  Exe 16:25, sin probar.
- **16:30 — repaso de rendimiento de todo lo agregado** (pedido del usuario). Revisado:
  - kit: `t480_slot` (fotos PNG) apagado; `[T480C]` no está; `masc_draw` solo mira dibujos 2D con textura 480 y
    mascota activa; `masc_flip` lee la GPU solo mientras mide (una vez por modelo, guardado en el archivo);
    destello y zoom se saltean en reposo.
  - mascota.c: 1 imagen cada 16 ms por turnos (por reloj); `cuadro()` cada cuadro solo los 0,7 s de la entrada.
  - hilos de ventanas en capas: onda.c solo dibuja durante el clic; premios.c bien. **cartel.c**: mientras un cartel
    se veía, mandaba la imagen entera a Windows cada 30 ms (UpdateLayeredWindow con el bitmap) aunque no cambiara →
    ahora solo al repintar, o solo la posición si se movió.
  - Queda, sin tocar: las imágenes 480 de las mascotas se renderizan a la escala interna del juego (la misma que las
    de batalla); bajarlas solo para las mascotas pide un cambio en el kit. Exe 16:30, sin probar.
- **16:40 — mascotas con pausa** (pedido: "que la hagan, luego se queden quietos un tiempo, luego vuelvan a hacerla").
  Medido en los .igb: cada modelo trae una animación en bucle, `igTransformSequence1_5` campo 19 = duración (ns),
  última clave en la duración e igual a la primera (m057 5,03 s, m903 4 s, m854 3 s, m000 2 s).
  `tools/modelos/mascotas_pausa.py` escribe `mNNN_p.igb` al lado de cada original (40 modelos, 66 MB): misma
  cantidad de claves, la animación remuestreada en N-1 claves de 0 a L y la última (pose inicial) en L+4 s;
  duración L+4 s → 4 s quieto en la pose del principio entre vueltas. Los originales no se tocan (batalla).
  `copiar_al_juego.sh` los copia a Game/monster/NNN/. Opción **"Animación de las mascotas"** (`mascota_pausa`):
  Con pausas (por defecto) / Seguida, en F2 y Configuración (6 idiomas); vale desde el próximo modelo cargado.
  Exe 16:40, sin probar.
- **16:5x — pausa de las mascotas DESCARTADA** (el usuario: "quedó horrible, se nota que se quedan quietos"). Sacados
  la opción (F2, Configuración, traducciones), el camino `_p` en `cargar`, el script y los 40 `mNNN_p.igb` (juego,
  TDOD y modding). Las mascotas vuelven a su animación en bucle. No volver a proponer congelarlas.

## 2026-11-15 16:55 — Idea 5: número de daño (compilado, SIN PROBAR)
- Pedido: "vamos por esa, tiene que parecer parte del juego".
- **Los números del juego:** el atlas del duelo (82D3B7DD2E4DECF3, filas 249..297, col 0..455; volcado dado vuelta)
  trae "o123456789-+" blanco con borde negro, pegado a la barra de PV: es la tipografía de los PV ("8ooo").
  `tools/danio_digitos.py` lo toma del escalado 4x, lo baja a 2x (912x104, luminancia+alfa) → `src/danio_digitos.h`
  con la caja de cada símbolo. El mismo atlas trae también los carteles de fase ("Principal Fase 1", "Batalla Fase",
  "Final Fase", "Cambiar Turno", "Cadena"): sirven para la idea 6.
- **`src/danio.c`:** cada cuadro de la escena del duelo (desde `sub_00056FC0` de mascota.c) mira los PV
  (0x65D0A4 + p*0x13F8, p 0 = vos) en estados 6/7; al cambiar: "-N" rojo anaranjado o "+N" verde, encima de los
  números de su barra (frame 217,174 / 423,162). Entra grande (×1,6 → 1 en 0,16 s), queda, sube 4 filas, y en
  los últimos 0,5 s sube 12 más y se desvanece (1,6 s). Dos cambios del mismo signo en 0,25 s se suman. Log `[DANIO]`.
- **Kit:** `dano()` después del destello en `present_blit`: textura R8G8 inmutable, PS a pantalla completa que
  arma el signo + dígitos con las cajas (solape 4 px de borde), alfa premultiplicado. Sin nada que mostrar no dibuja.
  Shader verificado con fxc. Vista previa sobre la captura del usuario: se ve como los PV.
- Opción **"Número de daño"** (`numero_danio`, Sí/No) en F2 y Configuración, 6 idiomas. Exe y Configuración 16:55.
- **17:1x — número de daño al estilo Master Duel en la batalla** (video del usuario 17-04-18 visto cuadro a cuadro:
  el daño sale grande en rojo donde pega, vuela achicándose hasta los PV y recién ahí el contador baja).
  - Arreglo: el registro de las 17:02 mostró "+8000" en los dos al empezar (en el estado 6 los PV todavía son 0 y
    después pasan a 8000): con PV anteriores en 0 no hay número.
  - Batalla: el envoltorio de `sub_00056FC0` (visores de batalla del juego, [v+0x2C] y [v+0xCC] prendidos; las
    mascotas no pasan por ahí) avisa `ygo_danio_batalla()`. Si los PV bajan hasta 3 s después de un cuadro de
    batalla: el número sale ×2 en el centro (320,215), entra (0,2 s), queda (0,55 s), vuela en arco a los PV de
    quien perdió achicándose a ×0,9 (0,4 s) y entra en el contador (0,3 s). Si no, como antes (al lado de los PV).
    Log `[DANIO] batalla en pantalla` y `..., de batalla`.
  - Pendiente posible: que el contador del juego espere a que llegue el número (hoy baja apenas cambian los PV).
  Exe 17:1x, sin probar.
- **17:5x** (video 17-54-44 del usuario): el vuelo desde la batalla se apagó ("se ven 2 veces": la pantalla de batalla y después el campo); el daño de batalla sale arriba de los PV como una curación. El código del vuelo queda (`vuela` = 0). Video: en Master Duel la carta atacante se levanta y vuela hacia el blanco con un rayo dorado; impacto con el número en un estallido dorado.

## 2026-11-15 18:0x — Rayo al atacar (compilado, SIN PROBAR)
- Pedido (video 17-54-44: en Master Duel la carta atacante se levanta y sale un rayo dorado hacia el blanco):
  "probemos rayo antes de la batalla; luego veremos lo otro" (los monstruos 3D peleando sobre el campo).
- `src/rayo.c`: en la declaración del ataque (`sub_0009A290`, cartas.c; 0x6666E0 bit 0 directo, bits 10-12
  atacante, 13-15 atacada) la casilla del atacante se ilumina (0,2 s), un rayo dorado viaja acelerando hasta el
  blanco (0,36 s, cola 0,45 detrás) y estalla con un anillo (0,24 s): 0,8 s. Directo: al retrato del rival
  (575,60) o a tu nombre (90,256). Centros de casillas: los de raton.c (tuyos fila 252, del rival 142).
- **La batalla espera:** el combate es una máquina de estados (0x65F8DE, tabla 0x6010F8, `sub_0009E5C0` pasa al
  siguiente cuando el paso devuelve ≠ 0). El paso 1 `sub_0009A540` (nuevo en PADS) devuelve 0 sin hacer nada
  mientras dura el rayo (tope 1 s).
- Kit: `rayo()` aditivo (blend propio) antes del número de daño; shader verificado con fxc; vista previa sobre la
  captura del usuario: el rayo cae en las casillas correctas.
- Opción **"Rayo al atacar"** (`rayo_ataque`, Sí/No) en F2 y Configuración, 6 idiomas. Log `[RAYO]`.
- **18:13 — arreglo del rayo** ("no se vio nada"; registro 18:11: hubo un ataque, `[CARTAS] batalla: ataque 210C` j0 casilla 0 → j1 casilla 1, pero ninguna línea `[RAYO]`: desde `sub_0009A290` no llegaba en un ataque elegido con el menú). Ahora el rayo arranca en la primera llamada del paso 1 del combate (`sub_0009A540`, estado 0x65F8DE = 1 y subpaso 0x65F8DF = 0; el argumento es el jugador que ataca; 0x6666E0 ya tiene las casillas) y se rearma al salir del paso 1. Log `[RAYO] combate: ...` siempre. Sin probar.
- **18:2x — rayo DESCARTADO** (el usuario: "quedó horrible, sucede antes y a veces después, recontra a destiempo"). Sacados el dibujo del kit, la retención del combate, la opción (F2, Configuración, idiomas) y la llamada en mascota.c; `sub_0009A540` queda como envoltorio que solo llama al original. No volver a proponer un efecto superpuesto que no esté atado a la animación del propio juego.
- **18:26 — el número va con el juego** (el usuario: "en 120 fps baja rapidísimo donde tiene la vida y nuestro
  número se queda mucho tiempo ... que vaya con el juego"). danio.c reescrito:
  - todo en **cuadros del juego** (el tick corre una vez por cuadro del duelo; el juego avanza por cuadro, a 120 fps
    su contador va al doble): entra 10, sale 18, sube 0,06 fila por cuadro;
  - **sigue al contador del juego:** al primer cambio de PV de cada duelo busca en los 64 MB del juego todos los u32
    / float iguales a los PV viejos (hasta 4096; sin las dos direcciones de PV) y cada cuadro descarta los que salen
    del rango o saltan derecho al valor nuevo; el que pasa por valores intermedios y llega es el contador. Hallado,
    el número queda mientras el contador no llegó a los PV y sale apenas llega. Sin hallar: 50 cuadros.
    Log `[DANIO] buscando el contador ... candidatos en X ms` y `[DANIO] contador del jugador N: DIR (u32/float),
    llegó en K cuadros` (o "no hallado").
  - Sacados el vuelo y la detección de batalla (`ygo_danio_batalla` queda vacía). Exe 18:26, sin probar.
- **18:33 — CONFIRMADO por el usuario: el número de daño "quedó bien ahora"** (cuadros + contador del juego).
  - Golpe final (500 de vida, ataque de 1600: salía -500): `sub_000C8BC0(p, n)` (perder n PV, encola la orden 0x27;
    nuevo en PADS) envuelto en danio.c guarda n; si los PV quedan en 0 y lo pedido era más, el número muestra el golpe
    entero (-1600), como Master Duel. Log `[DANIO] ... le quedaban X, el golpe era de N`. Si la batalla no pasa por
    esa función, sigue mostrando lo que bajó (ver el log). Exe 18:33, sin probar.
  - Pedido nuevo: usar las letras del juego en los carteles nuevos (cartas nuevas, sobres, "Te la pueden dar"...).
    La tira de números solo trae 0-9 - +; la fuente de textos del juego son 3 hojas 512x512 (mejoradas/letras:
    9BF7092A495E4E38, F867714BEE53B873, FED7C5881DA64D4A; ASCII y acentos, blanco, dadas vuelta en el volcado).

## 2026-11-15 18:5x — La letra del juego en nuestros carteles (compilado, SIN PROBAR)
- Pedido: "estas letras y fuentes que usaste podés usarlas para todo lo que hicimos de cartas nuevas y los sobres".
- La página de letras del juego (9BF7092A495E4E38, 512x512, volcado dado vuelta): grilla de celdas de 26 px, 15 por
  fila, **0x21..0x7E y 0x80..0xFF de Windows-1252** (salta 0x7F: sin eso los acentos salían corridos uno: á→â),
  cada letra pegada arriba de su celda (la altura en la línea la pone el juego aparte). `tools/fuente_juego.py`:
  la versión mejorada 4x reducida a 2x (780x780 alfa) + ancho y altura de cada letra (base fila 14; g j p q y , ;
  ç ( ) [ ] { } | $ @ bajan hasta la 18; comillas, ^ ~ * ° arriba; - + = · × ÷ < > al medio) → `src/fuente_juego.h`.
  Vista previa revisada ("Te la pueden dar:", "Kaiba, Joey, Pegasus", "¡Ñandú! ¿Qué? áéíóú 0123": bien).
- `src/fuente.c` / `fuente.h`: `fj_linea`, `fj_ancho`, `fj_medir`, `fj_dibujar` (bilineal, escala h/20: mayúsculas
  0,7 h como Segoe UI; centrado, centrado vertical, "..."); escriben la cobertura en los canales pedidos del bitmap,
  como DrawTextW en rojo/verde/blanco puros, así el resto de cada cartel (panel, borde dorado, colores) no cambia.
- Reemplazado todo GDI de texto: `cartel.c` (carteles: "Te la pueden dar", aviso de carta nueva, nombres bajo las
  fotos; y las marcas rojas sobre las cartas) y `premios.c` (cartel de premios del rival). Exe 18:5x.

- 2026-11-15 18:52: cartel "Te la pueden dar": cada retrato se corre para que lo visible (medido por alfa, los personajes estan al 60-69% del ancho del png) quede centrado en su celda, asi el nombre queda centrado bajo la foto; con un solo retrato la fila va al centro del cartel, con 2+ desde la izquierda. Sin probar.

- 2026-11-15 19:1x: 25 mascotas mas (el usuario: "si todas") -> 65: Dragon Mascota de Harpie 385, Guardian de la Puerta 373, Renunciado 901, Craneo Convocado Toon 728, Maldicion del Dragon 38, Mil Dragones 68, Alfa/Beta/Gamma 855-857, Dragon Meteoro 711, Elfa Mistica 1, Bruja del Bosque Negro 573, Tomate Mistico 918, Sangan 47, Gardna del Gran Escudo 908, Masaki 292, Fantasma Ingenioso 135, Mago de la Fe 427, Rey Maquina 406, Princesa de Fuego 926, Hermanas Dama Arpia 62, Sanga 370, Kazejin 371, Suijin 372, Mago Oscuro II 960 (segundo modelo del Mago Oscuro). Modelo->carta por la tabla 0x44FBB0. F2 y Configuracion en 6 idiomas. Sin retoques de tamano todavia; sin probar.

- 2026-11-15 19:2x (capturas): Mago Oscuro II pasa al lado del Mago Oscuro (indice 3; k_rival corrido +1 desde 3; F2/Configuracion guardan por nombre). Retoques: Tomate arriba 10; Hermanas Arpia dx -8 (38..122 -> derecha); Dragon Mascota de Harpie 385 y Maldicion del Dragon 38 x1,45 DRAGON por extension; Kazejin 371 por extension (24..108 -> 48..132, antes del rombo). Sin probar.

- 2026-11-15 19:3x: Princesa de Fuego 926 arriba 6; Hermanas Arpia 62 con su propio angulo de camara (az -8 en vez de -35,4: de costado se tapaban, "son 3 pero se ven 2") y su propia clave de medidas (+200000, se mide de nuevo). Sin probar.

- 2026-11-15 19:4x: Hermanas Arpia x1,3 ancho MEDIANO (110, columnas ~22..132; eran 84). Sin probar.

## 2026-11-15 19:34 — Ideas 2 (brillo al activar) y 8 (cadena numerada): paso 1, registro
- Leído en el código: tabla de efectos 0x601318 (24 bytes: +0 ID, +4 resolver, +0x10 costo, +0x14 ?).
  Máquina de efectos `sub_000A1BF0`: costo `sub_000A1990` (+0x10) → `sub_000A1A10` (+0x14) → aviso de Cadena
  `sub_000C07F0` (texto 0xC3E/0xC3F con el nombre de la carta de u16 0x66C0B8) → resolver `sub_000A1AA0` (+4).
  Efecto en curso: 0x66C71C marcas, 0x66C720 paso, 0x66C722 k, 0x66C724..2C; marcas generales 0x66C494.
  Mensajes del duelo 0xF0xx: `sub_00085720(cód, a, b, c)` / `sub_00085780(cód, datos, largo)` → `sub_00030AD0`;
  los atiende el switch de `sub_00085D90` (copia 0x66C18A → 0x66BA7C, etc.).
  Órdenes: tabla del despachador `sub_000811B0` en 0x8150C (cód-1, 174), p. ej. 0x27 → CFB70, 0x54 → CA130.
  Casillas: 0x65D0C8 + p*0x13F8 + z*0x9C, mágicas/trampas 5..10, +0xA bit 1 boca arriba, +0x96 bit 5 negada.
  El atlas del duelo trae el cartel "Cadena" (turquesa) para el número.
- `src/cadena.c` (registro "[CADENA]", nada visible): envuelve A1990/A1A10/A1AA0/C07F0, los 2 emisores de mensajes y
  sub_00081150; cada cuadro del duelo (`ygo_cadena_tick` desde mascota.c) anota orden en curso 0x65F948, cambios del
  efecto y de las casillas 5..10. Falta: un duelo del usuario (activar una mágica, y una trampa en respuesta a algo)
  para medir en qué cuadro pasa cada cosa y dónde guarda el juego la cadena.
- 19:5x registro del usuario (2 mágicas de la mano, trampas del rival; sin cadena): la máquina A1990/A1A10/C07F0/A1AA0
  NO corrió (no es el camino de mágicas/trampas). Las órdenes sí:
  - mágica de la mano: 0x91 (carta, ?) 1419..1512 (la lleva a la casilla; la casilla 0.5 queda boca arriba en 1501),
    0x4D (carta, 1) 1514..1692 = la activación (178 cuadros), luego su efecto (0x28 = ganar LP 1000), 0x53 (casilla)
    la saca, 0x59 Cementerio. Otra: 0x91 7500, 0x4D 7595..7782, 0x53.
  - trampa colocada del rival: 0x805C (casilla) la da vuelta (boca arriba a los 4 cuadros), 0x804D (carta, 1)
    4224..4286 (62 cuadros), 0x8053, efecto (0x801F). 0x8000 = jugador 1.
  - 0x4D también sale con monstruos de efecto (cartas en casillas 0..4): el brillo solo si la carta está en 5..10.
  - 0x90 invocar, 0x4C anuncio de la invocación, 0x54 voltear monstruo, 0x2F/0x39/0x3A/0x3B/0x2E/0x3E fases.
- Mascotas a x2 en el turno del rival (el usuario): "Turno del rival rápido" usa xbox_SpeedSet(2/4) (raton.c 4775),
  que acelera todos los relojes del juego; la animación de los modelos (Alchemy) corre con ese reloj. Sin arreglar:
  falta hallar de dónde toma el tiempo la animación del visor.
- 20:0x video del usuario (19-44-11.mkv) + registro: el juego YA tiene las dos ideas a su manera:
  - activación (orden 0x4D): la carta grande en el centro con un círculo mágico y el Ojo del Milenio (celeste las
    mágicas, magenta las trampas), y se desvanece;
  - cadena: panel "Chain List" (sin traducir, con "Card Status") con las cartas de la cadena en orden y flechas.
  - Cadena medida: Duster 0x287 (0x4D en 2766) → el rival da vuelta 0x3E1 (0x805C 2952, 0x804D 3091) → resuelve el
    eslabón 2 (0x8053 3367) → el 1 (0x53 3431). Regla: eslabón nuevo = 0x4D de una carta en 5..10 mientras la
    anterior sigue en su casilla sin resolver; termina con su 0x53 (o la casilla vacía).
  - Propuesta al usuario: no sumar otro brillo de activación (duplicaría el del juego); números de cadena sobre las
    cartas del campo mientras la cadena está armada + traducir "Chain List"/"Card Status".
- 19:55 HECHO (compilado, copiado, SIN PROBAR), `src/cadena.c`:
  - Cadena del juego hallada: 0x66BD18, eslabones de 0x14 bytes (+0 u16 carta, +2 bit 0 jugador), cantidad u16
    0x66BF98 (0x66BF9A otro contador); es lo que sub_00082890/sub_00082F80 pasan al panel con sub_000D8790.
    El panel "Chain List" es sub_000D8600 (estado (u8 0x66C17C >> 1): 0..3, corre cada cuadro mientras está).
    Ejecutor de órdenes sub_000817D0: u8 0x65F90C 0 libre, 1 corriendo, 2 terminando.
  - Números de cadena: con 2+ eslabones, el número de cada uno (1, 2, ...) sobre su carta del campo (casilla
    buscada por carta: 5..10 y después 0..4; lugares de raton.c), dorado, con los números de los PV: el pase
    dano() del kit ahora tiene 8 lugares (0, 1 daño; 2..7 cadena; tamaño negativo = sin signo). No se dibuja con
    la orden 0x4D corriendo (la carta grande) ni con el panel abierto (sub_000D8600 envuelta). F2 / Configuración
    "Números de cadena" (Sí/No, 6 idiomas). Registro "[CADENA] la cadena tiene N eslabones (66BF9A ..): carta/jug/+3".
    Duda: si la cantidad no vuelve a 0 al resolverse, con una continua seguiría el número (ver registro).
  - Títulos de listas traducidos (estaban en inglés en todos los idiomas): sub_00047CD0 envuelta cambia el puntero
    (Chain/Fusion/Grave/Deck/Exclude/Remove/Target List → Cadena, Fusión, Cementerio, Baraja, Fuera de juego,
    Objetivos; y en pt/fr/de/it). "Card Status" (0x42989C, 11 letras) se reescribe en su lugar: "Ver carta".
  - Los envoltorios de diagnóstico (A1990, A1A10, A1AA0, C07F0, 85720, 85780, 81150) quedan de paso, sin registro.
- 20:1x (el usuario: "no se ven los numeros de la cadena ... no agregues todo al f2 ... quien va a desactivar esto?
  nadie"): opción "Números de cadena" QUITADA de F2 y Configuración (siempre encendido). Causa de no verse: los
  ocultaba con el panel "Chain List" abierto, y el juego lo deja abierto toda la cadena (registro 20:00: 2 eslabones
  00FF/4 = el ataque del monstruo, 03E1/83 = la trampa del rival; +2 bit 0 jugador). Ahora, con el panel quieto
  (su paso 1, 12 cuadros después), el número va en la esquina de arriba a la izquierda de cada carta del panel
  (video 1080p: cartas 95 px de ancho cada 122,7 desde la columna 150, arriba en la fila 91); sin panel, en el campo.
- Mascotas a x2: kit `xbox_TscX1(on)` (kernel_hal.c, por hilo): mientras está, xbox_ReadTimeStampCounter da un reloj
  a velocidad 1 (nunca adelante del del juego; a x1 lo alcanza +10%). mascota.c lo prende solo alrededor del dibujo
  de cada mascota (sub_00058420 + sub_00056FC0_pad). Alchemy toma el tiempo de ahí (sub_00117xxx, rdtsc). Sin probar.
- 20:15 capturas del usuario: números vistos pero tapaban las cartas → en el panel van sobre la gema de color del juego
  debajo de cada carta (fila 250, tamaño 0,62); en el campo, en la esquina de arriba a la derecha, por fuera, chicos
  (0,52 / 0,45 el rival). "Card Status" seguía en inglés: el texto se copia al armar la escena, antes del primer
  tick del duelo; ahora se reescribe desde el hilo de premios_extra.c (10 por segundo desde el arranque).
- Límite de copias que no se respetaba (2 Golem Centinela en el mazo): el límite de cada carta vive en la PARTIDA
  GUARDADA (u8 0x665896 + k, bloque 0x665090 de 0x14B0 bytes con checksum sub_0007F630), llenado con sub_0007EF00
  al crear la partida (sub_0007F2C0): una partida anterior al mod guarda 3. `ygo_cartas_limites()` (cartas.c, desde el
  hilo de premios_extra.c) lo baja al del mod (nunca lo sube). Las copias que ya están en el mazo no se sacan solas.
  Sin probar.
- 20:2x el usuario: el editor igual dejó poner 2 Golem (el registro no mostró "limite guardado": ese byte no era 3).
  Leído: el editor nunca mira un límite; sub_000DEE80 (¿puede ir?) y sub_000DEF20 (ponerla) comparan las copias en
  los mazos con las del cofre (u8 0x651E70 + 0xCC1 + k; carta del cursor: u16 0x66C880 + 8 + 2*(u16 +0x12F6 +
  u16 +0x12F8)). sub_0007F710 (agregar al mazo guardado, sí mira 0x665896 + k) no tiene llamadas directas.
  Arreglo (cartas.c `con_limite`): mientras corren DEE80/DEF20, una carta del mod con límite muestra en el cofre como
  mucho ese número; lo que ellas cambien se conserva. Registro "[CARTAS] mazo: k .. limitada a ..". Sin probar.

- 2026-11-15: cartas del mod con límite: si el cofre tiene más copias que el límite, se bajan al límite (nunca por debajo de las del mazo); queda al guardar (cartas.c, ygo_cartas_limites). Pedido: sobraba 1 Golem Centinela.

- 2026-11-15: cadena estilo Master Duel (cadena.c, escalera()): el panel es el widget de listas del juego ([[0x656F88]+0x5C], sub_000560D0 lo arma; 9 lugares: carta +0x678, flecha +0x69C, gema +0x6C0; titulo +0x658 en 140,60; items +0x5B0, cursor +0x5B4, el lugar 4 es el medio). sub_00055530 (envuelto) los ubica cada cuadro con el metodo +0x2C (x,y,0) en px del cuadro 640x480. Con la Cadena: cartas en escalera al centro (50 px a la derecha y 12 arriba por eslabon), la nueva entra desde la derecha en 10 cuadros, gema en la esquina sup. izq., sin flechas ni cartel (titulo vacio). Sin probar.
- 23:4x (captura 23:36): la escalera salio en el lugar medido (unidades = px del cuadro, ancla = centro); la gema quedaba tapada por su carta y los numeros no salian (sub_00055530 no corre todos los cuadros). Ahora: gema arriba del borde de cada carta (x-25, y-84), medidas guardadas al correr sub_00055530 y la escalera se mantiene tambien desde ygo_cadena_tick.
- 23:5x: cadena segun el video de Master Duel del usuario (Downloads/Video/ssstik.io_1794796920385.mp4): cartas 1.4x (sub_000F69A0 tamano), repartidas por k_lugar (1 arriba der., 2 izq., 3 abajo der.), la nueva entra desde su costado y un poco mas grande; numero grande celeste en la esquina inf. der. (kit); gemas y flechas fuera; tamano del juego (96x139.5) devuelto en las otras listas. Faltan: cadenas entre cartas, texto CHAIN/RESOLVE, giro de las cartas. Sin probar.
- 23:5x: el panel de la cadena salia 2 veces: sub_000D8790 lo abre desde sub_00082890 (flag 0, armando) y sub_00082F80 (flag 1, por resolver). Envuelto: el de flag 1 no se muestra si esa cadena ya se vio (sub_000D8600 termina como su ultimo paso: sub_00088340 y devuelve 1). Log [CADENA] panel. Orden: 1 izquierda, 2 derecha, 3 abajo al medio; una sola carta al centro. La cadena dorada de "Cambiar Turno" no esta en el atlas 2D del duelo ni en duel_m/special_igb por nombre: pendiente.
- 00:0x (16/11): cadena dorada entre las cartas de la cadena, dibujada en el kit (nv2a_pb_d3d11.c: g_nv2a_cadena[8][8] = x0 y0 x1 y1, alfa, cuanto, largo del eslabon; en el pase de los numeros, debajo de ellos; eslabones planos y de canto alternados, oro con borde marron). La del juego ("Cambiar Turno") es malla 3D de duel_m_01.igb con un degrade dorado: no hay imagen. cadena.c: de borde a borde de cada par de cartas (85%), crece con la entrada de la carta nueva; con el panel quieto. Shader verificado con fxc. Sin probar.
- 00:1x (16/11): la cadena dibujada en oro quedo horrible (el usuario); ahora es una imagen propia: tools/cadena_render.py renderiza 2 eslabones de acero (toros alargados por sphere tracing, girados 40/-50 grados) que se repiten -> modding/texturas/cadena_eslabones.png y src/cadena_eslabones.h (256x80 RGBA). El kit la tiende a lo largo de cada tramo (textura t1, periodo = grosor * 3.2), grosor 26 px del cuadro. Cartas en zigzag bajando: 1 (190,100) arriba izq., 2 (450,150) der. mas abajo, 3 (200,185)... Un panel por cadena: confirmado por el usuario. NO COPIADO: el usuario vio la vista previa y dijo "se ve horrible"; el juego sigue con el exe de las 00:00 (cadena dorada dibujada).
- 00:3x (16/11): cadena = la imagen que eligio el usuario (modding/texturas/cadena/cadena_usuario.webp; tools/cadena_tira.py endereza el tramo recto -> cadena_tira.png y src/cadena_eslabones.h 520x97). Kit: una tira centrada entre los centros de 2 cartas (sin repetir), grosor 30 px del cuadro, tapada por las cajas de las cartas (g_nv2a_cadena_cartas: la cadena pasa por detras). Muestra mandada al usuario; COMPILADO, NO COPIADO hasta su OK.
- 00:5x (16/11): estilo de la captura de Master Duel ("mas fina, no derecha, con movimiento, en diagonal"): baldosa repetible de la cadena del usuario (cadena_tira.py: periodo 356 px desde x 60, union fundida 60 px); 2 cadenas finas (13 px del cuadro) por par de cartas, cruzadas (arriba->abajo y abajo->arriba), curvas (sag 16 / -10) y con balanceo por cuadros (fase s_cuadro*0.05); kit: g_nv2a_cadena[8][12], cad[24]. Muestra 3 mandada; COMPILADO, NO COPIADO hasta el OK.
- 01:0x (16/11): el usuario eligio la muestra 3 (las 2 cadenas finas cruzadas que se balancean): copiada al juego.
- 01:1x (16/11): una sola cadena por par (el usuario: "con 3 se ven mal"), centro a centro, curva (14) y con balanceo; con 3 eslabones escalera 1 (150,100) 2 (320,148) 3 (490,196) y cartas a 1.2 (4 o mas: 1.0). Muestra 4 mandada; COMPILADO, NO COPIADO hasta el OK.
- 01:2x (16/11): zigzag hacia abajo (el usuario: "la tercera abajo de la primera y asi"): impares x 190, pares x 450, cada eslabon media carta mas abajo; tamano = 284/(n+1) medias alturas (max 1.4): 2 cartas 1.36, 3 1.02, 4 0.81. Muestra 5 mandada; COMPILADO, NO COPIADO hasta el OK.
- 01:4x (16/11): cartas inclinadas (nodo [sprite+8]: +0xC giro en radianes, +0x4C rehacer matriz; sub_00011FE0 arma la matriz, sub_00012B80 las 4 esquinas: gira en el centro). Izquierda -0.12, derecha +0.12, la nueva entra mas girada. Columnas = 320 -/+ (media carta + 40): la cadena mide parecido con 2, 3 o 4. Mascara del kit girada (rc[18]); numero en la esquina girada. Filas 10..280. Muestra 6 mandada; COMPILADO, NO COPIADO hasta el OK.
- 01:5x (16/11): el usuario aprobo la muestra 6 ("este quedo bueno"): copiada al juego.

## 2026-11-16 — Ideas 6 y 13 (muestras, nada en el juego)
- Carteles de fase: el usuario eligio la A (banda oscura con lineas doradas que cruza el campo, el cartel del juego "Principal Fase 1"/"Batalla Fase"/... del atlas 82D3B7DD2E4DECF3 en el medio). Pendiente de hacer.
- Fundas/tapetes: los recoloreos NO gustaron. Master Duel: Protectores con arte de monstruo (o de atributo / tipo de invocacion) y Campos de Duelo tematicos. Muestras nuevas con el arte del juego (texturas de carta 4x en mejoradas/upscayl..., guardadas dadas vuelta; arte en x 58..424, y 196..548): fundas Ojos Azules, Mago Oscuro, Ojos Rojos, Zera; campos con el monstruo bajo las zonas del tablero (textura del tablero: duel_m_00.igb d_f00.png). Esperando eleccion.
- Fundas: el usuario quiere la de Exodia de Master Duel tal cual (su imagen: modding/texturas/fundas/exodia_usuario.jpg; enderezada con QUAD en las 4 esquinas -> fundas/exodia.png 520x760). Las nuestras con arte del juego se ven borrosas (cartas de 128 px). Para mas fundas: imagenes en alta que pase el usuario. Muestra mandada.
- Fundas en alta: nueva Exodia del usuario (fundas/exodia_usuario2.webp -> exodia.png 1038x1516, sin logo). Bajadas con su OK 8 ilustraciones 624x624 de images.ygoprodeck.com/images/cards_cropped/<passcode>.jpg (ojos azules 89631139, mago oscuro 46986414, ojos rojos 74677422, chica maga 38033121, kuriboh 40640057, craneo 70781052, soldado del brillo 5405694, definitivo 23995346) en fundas/arte/. tools/fundas.py las pone en el marco de la Exodia (ventana 86,86-968,1432) -> fundas/<nombre>.png y todas.png. Muestra mandada; nada en el juego todavia.
- Fundas: arte y marco se eligen por separado (tools/fundas.py). Mago Oscuro y Ojos Rojos "entera" (la ilustracion completa a lo ancho sobre su copia desenfocada) porque el corte vertical los cortaba (el usuario). 5 marcos: dorado (el de la Exodia), plata, obsidiana, milenio (violeta con ojos del atlas), neon. Salida fundas/<arte>__<marco>.png (9 x 5). Muestra mandada; nada en el juego.
- 02:xx (16/11): FUNDAS EN EL JUEGO. Kit: nv2a_tex_pack_override(hash, w, h, texels) (nv2a_tex_pack.c: una entrada en memoria que find() contesta primero, sube nv2a_tex_pack_gen() para que se vea al instante; necesita un pack cargado). fundas.c arma el reverso 4x (512x1024, la carta en las 740 filas de abajo, dada vuelta) con Mods-Extrasundasrte\<x>.png + marco\<y>.png (512x740, WIC) y lo pone sobre la huella E86E1EE8D605D332 (card_ura.tga). Opciones F2 + Configuracion (6 idiomas): "Funda" (Clasica, Exodia, Ojos Azules, Mago Oscuro, Ojos Rojos, Chica Maga Oscura, Kuriboh, Craneo Convocado, Soldado del Brillo Negro, Dragon Definitivo) y "Marco de la funda" (Dorado, Plata, Obsidiana, Milenio, Neon), en vivo. tools/fundas.py genera fundas/juego/ y copiar_al_juego.sh lo copia. Vale para todas las cartas boca abajo (tuyas y del rival). Log [FUNDAS] / [TEXPACK] override. Sin probar.
- 01:2x: primera prueba "no se vio nada": RECOMP_MODS_DIR es una lista (mods_puntero;Mods-Extras\cartasrchivos), no Mods-Extras. Ahora la ruta sale de la carpeta del exe (GetModuleFileName) + Mods-Extrasundas.
- 01:4x: investigacion para separar fundas: sub_00013D10(padre, nombre) arma un sprite 2D (vtable 0x4236E4) con textura por nombre (sub_00017FD0); el reverso es 0x428834. Envuelto en fundas.c solo para registrar quien lo pide ([FUNDAS] reverso: sprite/padre/desde). Falta un duelo del usuario para ver si las cartas del rival salen de otras funciones.
- 02:0x (16/11): funda del rival fija (Ojos Rojos + Obsidiana). fundas.c: guarda los sprites de reverso (sub_00013D10 con 0x428834, region +0x50); en cada cuadro del duelo (ygo_fundas_tick desde mascota.c) los de y < 200 (matriz de sub_00011FE0: +0x14) usan la region de card/normal/card_ura_rival.tga (sub_00017FD0, huella 092BA2CA9E17D332, kit override con la funda del rival); al salir del estado 6/7 se devuelven las regiones originales antes de que la escena las libere. card_ura_rival.tga = card_ura con un texel cambiado (fundas/), copiado a Game/card/normal. Sin probar: riesgo si la escena se libera sin pasar por otro estado.
- 02:3x (16/11): NO funcionó (captura del usuario: las dos fundas iguales; el log cambió 10 regiones). Causa medida en el código: sub_00014F90 arma los lotes de dibujo una sola vez, con la textura metida en el material (0x15082, etapa +0x68), y agrupa los sprites hermanos con la misma textura (0x151F1). Cambiar +0x50 no cambia el dibujo. Además el override del pack tiene un solo lugar: el del rival pisaba la funda propia. Nuevo: el kit (nv2a_pb_d3d11.c, rev_draw) dibuja cada lote con la textura de card_ura (g_nv2a_reverso_hash) dos veces: con el scissor por encima de la fila 200 usando el reverso del rival (nv2a_reverso_rival, textura propia del kit) y por debajo como siempre. fundas.c ya no toca sprites: ygo_fundas_tick pone la fila y un sello de tiempo (el kit corta 1.5 s después de que el tick deja de llamar). Log: [REVERSO]. card_ura_rival.tga ya no se usa. Sin probar.
- 01:50 (16/11): CONFIRMADA la funda del rival ("ahora si"). Fundas por rival como las mascotas: 15 ilustraciones mas bajadas con OK de images.ygoprodeck.com/images/cards_cropped (jinzo 77585513, arpia 76812113, dragon_barril 81480460, pescador 3643300, rey_rex 94119974 [corte 0.68], gran_polilla 14141448, mil_ojos 63519819, necrofear 31829185, maha_vailo 93013676, bestia_enmascarada 49064413 [0.5, aprobada], guardian_celta 91152256, comandante_cyber 6400512, dragon_zombi 66672569, golem_milenio 47986555, espadachin/Buster Blader 78193831). fundas.c k_por_mascota: indice de mascota del rival (mascota.c ygo_mascota_rival = k_rival[personaje]) -> arte; marco Obsidiana fijo; se recompone al empezar cada duelo si cambia. Las 15 tambien elegibles en F2 y Configuracion (24 artes). CONFIRMADO por el usuario ("se ve genial").
- 02:10 (16/11): ruedita en el nivel del duelo triple (tri_pointer cortaba antes del paso generico: ahora la rueda sube/baja ahi mismo). Mascotas segun el mazo de cada rival (listas de premios del xbe 0x453830/0x453630; Tea no tiene la Chica Maga, si la Princesa de Fuego): Yugi Elfa Mistica (51), Tea Princesa de Fuego (60), Tristan Dokurorider (m718, nuevo 65), Ishizu Dunames (m859, 66), Odion Guerrero de Pizarra (m845, 67), Strings Gusano Dragon Humanoide (m905, 68), Shimon Tomate Mistico (53); + Asaltante Vorse (m945, 69) para llegar a 70 (pedido del usuario). Weevil sigue Gran Polilla (tiene la Petit), Yami Bakura sigue Necrofear. 7 fundas nuevas (ygoprodeck con OK: princesa_fuego 64752646, elfa_mistica 15025844, dokurorider 99721536, dunames 12493482, guerrero_pizarra 78636495, gusano_humanoide 5600127, tomate 83011278); k_por_mascota[70]. F2/Configuracion 70 mascotas y 32 fundas, 6 idiomas. Fundas aprobadas ("hermosa las fundas"); copiado al juego 02:1x (exe 02:10). Sin probar en el juego.
- 02:2x (16/11): mascota del rival tocaba su nombre (Princesa de Fuego: k_retoque "arriba" 6 tambien la subia a ella). ubicar(): el rival ignora "arriba" y su fila de arriba pasa de 155 a 162 (TOPE_RIVAL). Las cartas boca abajo miden lo mismo que antes (65x75 px en las dos capturas): el marco oscuro las hace ver mas chicas. Sin probar.
- 02:3x (16/11): la funda del rival tardaba ~1 s (log: compuesta en el estado 6; el mazo ya se ve desde el 3). ygo_fundas_tick la compone en el primer tick de la escena (sin tick 1 s = escena nueva) y parte los lotes en los estados 0..7. Retoques: Gusano Dragon Humanoide 905 x1,45 DRAGON por su extension; Asaltante Vorse 945 y Guerrero de Pizarra 845 dx 10 (a la izquierda). Sin probar.
- 02:5x (16/11): **Idea 6, carteles de fase (opcion A)**. Fase = u8 0x662B9C (tabla de pasos 0x6001D4, sub_00083F50): 2 robo 9FAB0, 3 espera 9E640, 4 principal A0540, 5 batalla 9FCB0 (usa 0x65F8DE), 6 final 9EC80, 7 cambio de turno A12C0; 0x66686C es un subpaso; turno u16 0x65F8D8. fases.c (tick desde mascota.c, cuadros del juego): Principal 1/2 (2 si ya hubo batalla en el turno), Batalla, Final, Cambiar Turno, cada uno una vez por turno; log [FASE]. Kit fase() antes de dano(): franja oscura con lineas doradas (fila 196, media altura 50) que entra desde la izquierda y el cartel del atlas del duelo (busca en la cache la textura 82D3B7DD2E4DECF3: el del pack del idioma) con sus uv medidas en el atlas 4096x2048 (dado vuelta). Shader verificado con fxc. Aprobado por el usuario ("esta bien") con dos cambios: cartel siempre centrado (sin deslizarse) y mostrar la Principal 2 (ya estaba; se muestra solo si hubo batalla). Copiado al juego. Sin probar en partida.
- Opcion "Carteles de fase" (clave fases, Si por defecto / No = como antes) en F2 (despues de las fundas) y Configuracion, 6 idiomas (pedido del usuario).
- 03:0x (16/11): el turno del rival no mostraba carteles (log: 0x662B9C queda en 8 = sub_000A1D80, el turno de la maquina; sus fases son u8 0x66686C por la tabla 0x6001B0 via sub_000810D0: 0 robo, 1 espera, 2 principal, 3/5 piensa (8FE10/8FE70), 4 batalla 8B4A0, 6 final, 7 cambio). fases.c traduce esas a las del jugador. Log [FASE] ahora con subfase. El usuario aclaro: el juego ya muestra su propio cartel de fase y salia el viejo y despues el nuevo. Kit fase_oculta(): con la opcion en Si, durante el duelo se saltea cada lote 2D (hasta 64 vertices) que usa el atlas 82D3B7DD2E4DECF3 con sus uv dentro de las cajas de los 5 carteles (g_nv2a_fase_ocultar, desde fases.c). Log "[FASE] cartel del juego tapado". Sin probar.
- 03:1x (16/11): probado: nuestros carteles salen en los dos turnos (log), pero "se siguen viendo los antiguos" y no hubo ningun "tapado": el cartel del juego no sale de esas cajas del atlas. Diagnostico: fases.c pone g_nv2a_fase_diag = ahora + 3 s en cada cambio de fase; el kit (fase_diag) anota cada lote nuevo en ese tiempo ("[FASE] lote: tex ... pos ... uv ...", 150 por cambio, 1500 en total). Pedida una partida corta y captura del cartel viejo.
- 03:3x (16/11): capturas del usuario: el juego tiene su propio cartel grande de fase en el centro (rosa el jugador, celeste el rival; tambien "Sorteo Fase") y la animacion "Cambiar Turno" con cadenas. Log 03:1x: el cartel sale del atlas 82D3 en el MISMO lote que las barras de PV (11 vertices, x 148..492, y 170..220). Ademas las fases por variable fallaban (el usuario: su Batalla y Principal 2 no salian; en el rival la Principal 2 dio "Final Fase"). Nuevo: el kit marca ese lote (s_lab) y lo dibuja por tramos sin los triangulos cuyas uv caen en las cajas de los 4 carteles (lab_draw), y avisa cual vio (g_nv2a_fase_juego[i]); fases.c dispara el nuestro cuando el juego trae el suyo (no visto en los 0,6 s antes). "Cambiar Turno" queda el del juego. Otras piezas del atlas en el medio se anotan una vez ("[FASE] en el medio, del atlas") para encontrar "Sorteo Fase". Sin probar.
- 03:4x (16/11): "no sale nada, sigue saliendo Sorteo de fase y uno parecido al final". Log 03:2x: los triangulos del cartel leian una esquina con uv 0 (la caja entera nunca entraba) y un "Final Fase" falso al empezar (otra pieza entraba en la caja). Ahora se reconoce por el centro de sus uv y solo en el medio del campo (x 200..440, y 130..260). Sorteo Fase no esta en el atlas 82D3 ni en 46029E56: el kit anota cualquier pieza 2D (lotes de hasta 64 vertices) en el medio, de cualquier textura ("[FASE] en el medio: tex ..."). El lote partido de las fundas no pasa por ese camino. Sin probar.
- 03:5x (16/11): log 03:4x MEDIDO: "Sorteo Fase" y "Espera Fase" son de la textura CAE5437E911B62D9 (la de los dados; uv del juego Espera 0.8027..0.8926, Sorteo 0.8945..0.9844, v 0.75..1; en el centro x 256..380, y 155..241). Causa de que nunca se taparan los 4 del atlas: cada quad del cartel son 2 triangulos y los dos tienen una esquina leida uv 0,0 (el registro deduplicaba por la esquina minima y mostraba uno solo); ni la caja entera ni el promedio entraban. El "Principal Fase 1" falso al final del turno era un triangulo de "Cambiar Turno" cuyo promedio caia en la caja de Principal 1. Nuevo criterio (lab_box): un triangulo es del cartel si >= 2 de sus 3 esquinas caen en la caja (margen u 0.003, v 0.01) de la textura de esa caja, con esas esquinas en el medio; gana la caja con mas esquinas y, empatadas, la de centro mas cerca (Principal 2/Final a 0.004 y Espera/Sorteo a 0.002 en u). Simulado con las uv del log: 10/10 bien; Cambiar Turno toca a lo sumo 1 esquina de Principal 1. Lotes 2D de hasta 512 vertices. Nuestro cartel toma el rotulo de su textura (g_nv2a_fase_rotulo_tex). Sin probar.
- 04:0x (16/11): log 03:5x: ya se detectan y tapan los 6 (Sorteo, Espera, Principal 1/2, Batalla, Final) en los dos turnos. Faltaba: el Sorteo del primer turno sale antes del estado 6 (tapar y detectar ahora en estados 3..7), y "pasan muy rapido": en el turno de la maquina el juego muestra 3 seguidos y cada uno cortaba al otro. Ahora cada cartel dura 1,5 s (QUEDA 90, SALE 104) y los que llegan antes de 0,8 s (MINIMO 48 cuadros) esperan en una cola de 4. Sin probar.
- 04:1x (16/11): captura del usuario: la carta boca abajo del medio ("Descarta de tu mano") salia mitad Kuriboh mitad Exodia (el scissor la cortaba en la fila 200). Ahora el lote del reverso se parte por triangulo: rev_posiciones() corre el programa de vertices en la CPU (nv2a_vsh_run, como el trace) para la fila de cada vertice (las entradas de posicion venian en 0), y rev_draw dibuja tramos de cada lado segun el medio de las filas del triangulo (fila 200 en pixeles de superficie); sin programa o si falla, el scissor de antes. Carteles: "pasan volando, sobre todo los del rival" aunque no este en x2: se contaban en cuadros del juego (va a mas de 60). Ahora en ms: entra 220, queda 1800, sale 2150, minimo antes del siguiente 1100. Sin probar.
- 04:2x (16/11): el usuario: "eliminemos todo esto y dejemos 100% el original, ya que quedo todo mal". QUITADOS los carteles de fase (idea 6): fases.c fuera del build (copia en el scratchpad de la sesion como fases.c.quitado), el tick en mascota.c, la fila F2 y del launcher ("Carteles de fase") y su traduccion; en el kit: ocultado de los carteles del juego (s_lab/lab_box/lab_draw/fase_diag, g_nv2a_fase_*) y la franja fase(). Los carteles del juego salen como el original. Se queda el reparto por carta del reverso del rival (rev_posiciones), que es de las fundas. Compilado y copiado (exe 03:54:07, Configuracion.exe con cmp).

## 2026-11-16 — Perfil en el menú principal (compilado, sin probar)
- Pedido: un "Perfil" dentro del menú principal, como si fuera del juego. Boceto 3 aprobado: un solo panel con foto + nombre, mascota de a una (modelo 3D animado) y funda de a una (mazo).
- `src/perfil.c` (nuevo, en CMakeLists): todo en ventanas en capas propias, transparentes al clic, encima del juego (como cartel.c); no reemplaza nada del juego.
  - Botón: un medallón real de la rueda con un busto de duelista, en el centro de la rueda (raton.c `sub_0010D6C0` llama `ygo_perfil_rueda` con el punto medio de los íconos 0 y n-1 cada cuadro que la rueda espera teclas). Se ilumina con el puntero.
  - Panel: oscurece el juego; fondo.png (el cartel del menú sin texto, placas, flechas redondas, jeroglíficos de las paredes); textos con la letra del juego (fuente.h); foto = retratos del disco cf_anim/small/png/cfsNN.png (clic = el siguiente, 28); nombre (clic = escribir, Enter guarda, Esc deja el anterior, máx 16); cartas distintas y copias (0x651E70+0xCC1+k); mascota con flechas (70, mismo setter que F2 "Tu mascota"); funda con flechas (32) y 5 puntos de marco (setters de F2); `xbox_MenuSaveSoon`. Nombre y foto en YGO_AJUSTES\perfil.ini.
  - Mascota animada: ventana propia sobre el pozo del panel, 32 cuadros a ~11/s.
  - Mouse (raton.c `wheel_pointer`, antes que la rueda): sobre el botón la rueda no hace nada y el clic abre; con el panel abierto el estado del mando se pone en cero (ni botones ni teclas llegan al juego); clic derecho o Esc cierra y se espera a soltar los botones.
- Imágenes: `tools/perfil_recursos.py` (desde `modding/texturas/perfil/menu_principal.webp`, la captura del usuario) → `modding/texturas/perfil/juego/` (boton, boton_luz, fondo, flechas iluminadas, brillo de la foto, fundas/AA_M.jpg 240x350 + 00_0.jpg el reverso clásico del dump).
- Mascotas: `tools/visor_modelos/grabar.html` (ruta /grabar del visor, POST /api/cuadro) graba cada modelo original con su animación de reposo, desde la dirección de la cámara A de la batalla, encuadrado a su tamaño, vaivén ±22°, 32 cuadros 768 px → `modding/texturas/perfil/crudo/`; `tools/perfil.py` recorta todas al mismo cuadro, 320 px, tira vertical de 256 colores → `juego/mascotas/II.png` (II = orden de k_modelo). 70 de 70, 28 MB.
- copiar_al_juego.sh copia `perfil/juego` a `Mods-Extras\perfil`. Traducciones nuevas en f2_idiomas.c (`f2_traducir`), nombres desde main.c (`ygo_perfil_mascota_nombre/funda_nombre`).
- Solo con el mouse en modo Puntero; con mando no se abre todavía. Exe 06:32:25 copiado. Sin probar.
- 12:5x (16/11): prueba del usuario: el Perfil se ve y funciona. Pedidos: "se siente muy lento" (eran 32 cuadros cada 90 ms, ~11/s) -> regrabadas las 70 con 48 cuadros mostrados cada 50 ms (2,4 s por vuelta) y la animación del modelo a ~1,5x (T = duración x round(3,6 / duración)), vaivén ±17°; 42 MB. "Perfil se sale de su recuadro": título 40 (era 50), centrado en la placa. "Los nombres se salen": los textos se achican hasta entrar (texto_w maxw: nombre 306, mascota 300, funda 220). Sacadas las líneas "Clic en la foto o el nombre / para cambiarlos" y "Copias"; ahora "Cartas: tenidas / total" (total = entradas de la lista 0x5FE692 con ID). Ojo: modding/texturas/perfil/crudo tiene restos m*_48..95 de una grabación a 96 cuadros cancelada (perfil.py solo usa 00..47). Exe copiado. Sin probar.
- 13:xx (16/11): el usuario: "a 120 fps se ve un poco lento aun", "hacela como se ve en el juego". Regrabadas las 70 a la velocidad propia de su modelo (como en el duelo), sin el vaivén, 60 cuadros por segundo: cada una T = duración x round(2 / duración) (>= ~2 s, sin salto), cuadros = T x 60 (máximo 240: las más largas a ~48/s). grabar.html manda los cuadros mNNN_FFF y POST /api/info -> crudo/mNNN.json; perfil.py arma la tira y escribe mascotas/II.txt con los ms de cada cuadro. perfil.c: la tira se guarda en memoria con sus 256 colores (índices + paleta, leer_tira; 320x320 x 240 = 24 MB como mucho, no 100 en BGRA), el cuadro sale del reloj (ms del .txt), el hilo duerme 4 ms con el panel abierto. 80 a 240 cuadros, 134 MB. Exe copiado. Sin probar.
- 13:2x (16/11): el usuario: "quedó bien ahora" (mascotas a su velocidad). Borradas las 4899 imágenes viejas de crudo (mNNN_NN.png de las grabaciones de 32/48/96 cuadros; quedan las actuales mNNN_NNN, 1,8 GB, solo para rearmar). Título "Perfil" bajado (arriba 82, era 75).
- Dónde guarda la partida el nombre: sub_0002D990 (partida nueva, llamada por la pantalla del nombre en 0x111867 vía sub_0002DB90) hace strncpy(0x6518A9, nombre, 13); sin nombre copia "NO NAME" (0x4238E8). El bloque guardado empieza en 0x6518A8 (sub_0002DBA0 lo limpia: 0x1072 dwords) e incluye la colección (0x651E70+0xCC1); en disco Partidas\UDATA\4b4e001e\0061006D1822\savedata.dat, el bloque desde el byte 0x1C ("Dennis" en 0x1D). Letras: Windows-1252 (tabla 0x456460), 12 como mucho.
- El Perfil ahora muestra ese nombre (si está vacío, el de perfil.ini) y al cambiarlo lo escribe en 0x6518A9 (solo letras de 1252, máx 12; el juego lo guarda con su próximo guardado). Exe copiado. Sin probar.
- 13:4x (16/11): capturas del usuario: algunas mascotas cortadas o corridas (Ojos Azules, Golem, Arpías, Vorse, Exodia), y "raras": el dragón con manchas blancas (m711), el Tomate sin ojos (m918), la cara del Sabio (m931). Encuadre: grabar.html con la cámara a 1,5 x el radio (era 0,92); medido: ningún cuadro toca el borde (antes se cortaban y el recorte centraba lo cortado). Regrabadas las 70 (133 MB).
- Las "raras" son del conversor tools/igb_gltf.py, no del juego: m918 y m931 tienen igUvAnimeShader/igUvAnimeKey (animación de textura: 918 tiene 11 imágenes y el glTF lleva 4) y m931 además igBlendFunctionAttr/igDepthWriteStateAttr; el conversor no exporta animación de textura ni mezclas. De las 70: 13 con igUvAnimeShader (34, 960, 774, 40, 61, 903, 900, 931, 918, 427, 926, 62, 859), 8 con mezcla propia (774, 863, 931, 855, 857, 406, 926, 845). grabar.html?m=N&ver=1 deja un modelo en pantalla para mirarlo (window.ver). Sin arreglar.
- Barrita de la colección debajo de "Cartas: x / y" (perfil.c barra(): borde dorado, relleno dorado). Exe copiado. Sin probar.
- 13:5x (16/11): "Sí, intentalo" (las mascotas raras). Causas medidas:
  - Tomate (m918) y Sabio (m931): tools/igb_gltf.py salteaba igUvAnimeShader, que es un grupo (hijos en el campo 7): los ojos del Tomate (malla "body" con m918_05.png, 4 cuadros de ojos 2x2) y el brillo del bastón del Sabio ("blendmode_add", m931_10) no se exportaban. igUvAnimeShader: campo 28 la unidad, 13 la vuelta (s), 20..23 igUvAnimeKeyList de corrimiento u, v y escala u, v; igUvAnimeKey: 3 interpolación (0 salto, 1 lineal, 2 fija, 3 fin), 4 tiempo (s), 5 valor. Ahora se exportan y el material lleva extras "uvanim" (unidad 0) y "blend": "add" (igBlendStateAttr encendido + igBlendFunctionAttr destino ONE); grabar.html mueve map.offset/repeat por cuadro y dibuja lo aditivo con AdditiveBlending sin escribir profundidad. visor.py rehace el glTF también si cambió igb_gltf.py.
  - Dragón Meteoro (m711) con la bola blanca: solo en la grabación de las 70 seguidas (grabado solo sale bien, con lava): grabar.html nunca liberaba geometría, materiales ni texturas; ahora los libera tras cada modelo.
  - Las texturas "S" (unidad 1, gris: brillo) no se usan todavía.
- 14:1x (16/11): primera regrabación con los arreglos: m021 (Cráneo Convocado) falló a la mitad ("Failed to fetch") y quedó en la escena: apareció encima de todas las siguientes. grabar.html ahora saca de la escena lo que haya quedado tras cada modelo y reintenta una vez. Regrabadas las 70 (70 de 70, 134 MB): Dragón Meteoro con su bola de lava, Tomate con ojos (parpadea), Sabio con el brillo del bastón, Princesa de Fuego con sus llamas (926: animación + aditivo), Restricción de Mil Ojos con su textura animada; Renunciado ahora con las alas blancas (la mitad clara de m901_2, como su carta). Copiado. Sin probar en el juego.
- 14:4x (16/11): el usuario: "centra todas", "cambia la camara para algunos como el golem ... de frente", barrita verde, sacar "Clic: elegir / Clic derecho: volver"; y luego "algunos estan bien, solo graba los que no estan de frente o se ven mal, revisalos 1 por 1". grabar.html?vista=frente graba aparte (crudo_frente; cámara de frente: 15° de giro, 12° desde arriba; los modelos miran a +Z); comparadas las 70 de a una (cámara A de la batalla vs de frente, recortadas como en el panel): 39 de frente, listadas con su nombre en modding/texturas/perfil/camaras.txt (perfil.py las toma de crudo_frente). Centrado (perfil.py): en la caja, salvo que el centro del cuerpo (donde pesan los píxeles de todos los cuadros) se aparte más del 4 % del lado: entonces de costado sobre el cuerpo. 138 MB. perfil.c: barrita verde, sin la línea de los clics. Exe y mascotas copiados. Sin probar.
- 15:xx (16/11): el usuario: las mascotas "quedaron bien"; el botón del Perfil "tarda un poco en aparecer" y se vio encima del cartel "La partida que no se guarda se perderá. ¿Seguro?". Leído: la rueda (sub_0010D6C0, tabla de estados 0x10DF00) está en 1 mientras entra el menú (0x10D969 espera el fundido y pasa a 0) y en 5 con ese cartel (B en 0x10D8C5 -> sub_000EEFE0..., estado 5 en 0x10D9F1 maneja Aceptar/Cancelar, 6 lo cierra). El botón solo se avisaba en el estado 0: ahora también en el 1 (midiendo los íconos 0 y n-1 con wheel_place, si [rueda] tiene su cámara). Por código el botón ya debía ocultarse en el 5 (200 ms sin aviso); el registro de 14:17 muestra que justo antes del cartel el Perfil se abrió y cerró en el mismo instante y una B llegó a la rueda (sonido desde 0x10D8D4): al cerrar el panel ahora se sigue tapando el mando 250 ms más después de soltar los botones. Nuevas líneas en registro.txt: "[RATON] wheel state a -> b" y "[PERFIL] boton visible / oculto (motivo)", para medir si vuelve a verse sobre el cartel. Exe copiado. Sin probar.
- 14:5x (16/11): video del usuario: el botón del Perfil aparecía sobre las puertas cerradas (pared del ojo) antes que los demás íconos. Leído: las puertas son la transición en [0x66DCD8] (sub_000F49D0), +0x10 = 1 cerrando (0,67 s), 2 cerradas, 3 abriendo (1,33 s, después 0); sub_00035110 (lo que espera el estado 1 de la rueda) da ocupado salvo 0/2. En el video las puertas quedan cerradas ~0,33 s del 3 y se deslizan ~0,27 s. Ahora en el estado 1 de la rueda el botón solo se avisa con la transición en 0 o 330 ms dentro del 3, y entra con un fundido de 220 ms (mostrar() recibe alfa). Registro: "[RATON] doors state a -> b (wheel n)". Exe copiado. Sin probar.
- Efecto del ícono elegido (sol dorado con 16 rayos): no es una textura suelta del juego (no está en el dump; parece del modelo 3D de la rueda). Maqueta dibujada (modding/texturas/perfil/maqueta_sol.png, script en el scratchpad sol_maqueta.py): el medallón pasado a dorado + rayos con borde oscuro, al pasar el mouse. Esperando el OK del usuario.
- 15:0x (16/11): el usuario aprobó la maqueta del sol ("Sí, hacelo"). tools/perfil_recursos.py: boton(True) ahora es el medallón dorado (dorar: luminancia -> oscuro/naranja/amarillo, busto teal igual) + 16 rayos con borde oscuro (sol), en el mismo lienzo 242x199; reemplaza el brillo blanco viejo de boton_luz.png. perfil.c no cambió (ya usa boton_luz.png con el mouse encima). Copiado a Mods-Extras/perfil. Sin probar.
- 15:0x (16/11): el usuario: "que gire asi como los otros? ... cuando pones el mouse encima tienen como una onda expansiva". Medido en el video (30 fps): el medallón y sus estrellas quietos, los rayos giran ~110°/s contra el reloj; al elegirse sale una elipse dorada translúcida que crece (~1,3 a 2,5 veces el medallón) y se apaga en ~4 cuadros. Hecho: perfil_recursos.py arma boton_sol.png (12 cuadros = un paso de rayo, 22,5°); perfil.c con el mouse encima recorre la tira por el reloj (repinta cada ~8 ms) y al entrar el mouse dibuja la onda 200 ms (elipse 1,25->2,5, alfa 0,45->0, borde suave). La ventana del botón es 2,2 veces el botón (lugar para la onda); el hit-test no cambió. Exe 15:03:06 copiado. Sin probar.
- 15:1x (16/11): video del usuario (15-06-50): "imitar cuando entra en cualquier menu en nuestro perfil (fondo completo que tape todo)". Maqueta aprobada ("Sí, hacelo"). Hecho: perfil_recursos.py saca las puertas del menú de modding/texturas/perfil/puertas_menu.png (cuadro del video, puertas cerradas, unión en la columna 961) -> juego/puerta_izq.jpg / puerta_der.jpg, y el fondo de menú del juego (dump FDE76864287C3300, 640x480 desde la fila 32) -> fondo_completo.jpg. perfil.c: al abrir, las puertas se cierran sobre el menú (200 ms), quedan 330 ms (el panel aparece debajo, con el fondo completo en vez del juego oscurecido) y se abren (270 ms); al cerrar (clic derecho / Esc) lo mismo al revés y recién al final s_abierto = 0 (s_cerrar_t). Mientras se mueven, el panel no responde y el mando sigue tapado. Ventana propia (vd) encima de todo; cada mitad escalada una vez al tamaño de la ventana y corrida por filas. Exe 15:14:05 copiado. Sin probar.
- Pendiente (pregunta al usuario sin respuesta): el salto al entrar al duelo es nuestro zoom ("Entrada del duelo: Completa", g_nv2a_zoom ×1,6 desde el estado 3), que arranca con las puertas todavía cerradas ~0,3 s (en el video la pared y la barra de abajo se agrandan y se achican). Opciones: sacarlo, o que arranque cuando las puertas empiezan a deslizarse.
- 15:2x (16/11): el usuario: "El Perfil aparece antes" (de que se abran las puertas). Leído: mostrar() ponía cada ventana HWND_TOPMOST al mostrarse por primera vez, así que el panel y la mascota subían por encima de las puertas cerradas hasta que la ventana de las puertas volvía a subir. Ahora las puertas se dibujan antes que el panel en cada vuelta, y el panel y la mascota se ponen justo debajo de ellas (s_arriba = la ventana de las puertas mientras se ven). Exe 15:19:48 copiado. Sin probar.
- 15:2x (16/11): el usuario: el viaje de entrada "a simple vista ni se nota" / "es como hacer algo en vano". Medido en el video 15-06-50 a 60/s: el zoom arrancaba a ×1,6 de golpe con el estado 3 (17,017 s) sobre las puertas todavía cerradas; la rendija aparece +167 ms, abiertas del todo +320 ms; con la caída cúbica de 1,3 s, al abrirse ya quedaba ~×1,1 (el error de antes: anoté que en el estado 3 las puertas ya estaban abiertas). Ahora (sub_0005C3F0 en mascota.c): 0..160 ms del estado 3 la imagen se acerca de ×1 a ×1,6 sobre las puertas cerradas (sin salto, suave), y desde ahí vuelve a ×1 en 1,3 s con smootherstep (lento al principio y al final, sin rebote): cuando se abren las puertas el campo se ve grande y se acomoda a la vista. Exe 15:28:29 copiado. Sin probar.
- 15:3x (16/11): el usuario: "no se ve nada saca el zoom y deja el efecto de las mascotas". Zoom de la entrada sacado (sub_0005C3F0 deja g_nv2a_zoom en 1); la entrada de las mascotas (suben con los carteles + destello) sigue. "Completa" y "Solo las mascotas" ahora hacen lo mismo (la opción quedó en F2/Configuración). Exe 15:32:53 copiado. Sin probar.
- 15:4x (16/11): el usuario: "cuando haces el primer ataque que carga los 2 monstruos se tranca un poco ... se pega un lagaso". Registro 15:36: [HITCH] frame 5895 265 ms (sin shaders ni disco). Causa medida: el kit (xboxrecomp-apex nv2a_pb_d3d11.c) tenía DT_MAX 4 depth buffers, y la batalla usa 5 a la vez (640x480 del cuadro + 2 mascotas 0x014A6000/0x015F8000 + 2 visores de batalla 0x01151000/0x012A3000, 480x480 = 1920x1920 en la PC, ~29 MB cada uno): cada cuadro de la batalla tiraba el menos usado y lo volvía a crear y limpiar (17 creaciones "zeta ... depth buffer" en esos cuadros, 5 en el del tirón). DT_MAX 8. Exe 15:39:44 copiado. Sin probar.
- 17:4x (16/11): **Tapetes** (el usuario: "capaz podriamos poner Tapetes ... como master duel"; maqueta aprobada). tools/tapetes.py: 31 tapetes (las ilustraciones de las fundas, orden de fundas.c k_arte) sobre el tablero mejorado (BFBD8339E5E8D7E0, 4x = 2048x512): marcos dorados iguales, el marrón pasa a ser la ilustración (nítida al medio, estirada/borrosa/oscura a los costados, más oscura en las casillas), dada vuelta arriba-abajo (tu mitad muestra la textura así) -> modding/texturas/tapetes/juego/<nombre>.png y <nombre>_vista.jpg (660x480 para el Perfil; clasico_vista.jpg = el del juego), 36 MB, copiados a Mods-Extras\tapetes. Kit: segundo dibujo partido como la funda del rival (g_nv2a_tapete_hash, nv2a_tapete(texels) BGRA recto; arriba de la fila 200 la textura propia, abajo el tapete; rev_draw usa s_rev_arriba/abajo). fundas.c: ygo_tapete_get/set (lee el PNG en un hilo aparte). F2 y Configuración: fila "Tapete" (Clásico + 31), 6 idiomas. Perfil: el recuadro de la foto muestra tu tapete con su nombre abajo y flechas chicas a los costados (clic en el recuadro = siguiente). La foto quedó sin uso. Exe 17:41:52 y Configuración 17:42 copiados. Sin probar.

- 2026-11-16 19:00 Tapetes con la transparencia del tablero original: tools/tapetes.py usa el alfa de la textura (fondo ~214, antes forzado a 245). Regenerados y copiados. Sin probar.

- 2026-11-16 19:1x Perfil: el título sobre el tapete dice "Tapetes" (traducido a los 6 idiomas). El botón del Perfil se oculta en el mismo cuadro en que la rueda sale de 0/1 (raton.c llama a ygo_perfil_fuera), antes tardaba hasta 200 ms y se veía sobre el cuadro "¿Seguro?". El registro ahora tiene los ms en "wheel state" y "boton oculto". Sin probar.

- 2026-11-16 (usuario): confirmado que anda bien: tapetes con transparencia, título "Tapetes", botón del Perfil sin aparecer sobre "¿Seguro?", puertas del Perfil, sol con onda, fondo completo, entrada al duelo sin zoom.
- 2026-11-16: el registro de esa prueba no tiene duelos (solo 2 HITCH de menús, tras las puertas): el tirón del primer ataque se revisa con el registro del próximo duelo ([HITCH] dice la causa). Propuesta de Estadísticas: 2ª página del Perfil con las flechas del cartel (récord, % ganadas, rachas, victorias con Exodia, rival más difícil, últimos 5 duelos con los retratos cf_anim/small/cfsNN). Maqueta enviada, esperando OK. Falta hallar en el código dónde se pierde un duelo (ganar: sub_0008C7E0).

## 2026-11-16 20:1x — Estadísticas en el Perfil (maqueta aprobada: "Sí, hacelo")
- Segunda página del Perfil: las dos flechas del cartel de arriba (las de fondo.png, ref 704/1218, 116) pasan Perfil ↔ Estadísticas; al abrir el Perfil arranca en la 1. En la 2 no hay mascota ni nada más para clickear.
- Contenido: Duelos / Victorias / Derrotas, "Ganadas: N %" con la barra verde, racha actual, mejor racha, victorias con Exodia, rival más difícil (más derrotas; empate: menos victorias) con su retrato y "N ganados / N perdidos", y los últimos 5 duelos (retrato cfsNN del disco, nombre = texto 86+personaje del juego en su idioma, fecha dd/mm, Victoria/Derrota/Empate).
- Fondo de la página 2: fondo.png con el interior cubierto por su propia piedra (hacer_fondo2, en memoria; sin archivo nuevo).
- Resultado, leído en el código: el chequeo de victoria `sub_00083AD0`/`sub_00083F50` termina el duelo con PV 0 o la marca "perdió" (u8 0x65D0AD + p*0x13F8, bit 0) y deja en u16 0x65F8DA: 0x40 ganó el jugador 0 (vos), 0x80 el rival, las dos = empate; u8 0x65F894 |= 0x10 = terminado. mascota.c (tick) llama `ygo_estadisticas_fin` cuando la escena pasa de 6 a 7. Sin 0x10 o sin bits: no se cuenta (registro "[ESTADISTICAS] no terminado").
- Rival: personaje [0x66DCDC+4]; >27 se muestra como el 27.
- Guardado: YGO_AJUSTES\duelos.txt, una línea por duelo ("AAAA-MM-DD HH:MM personaje V|D|E exodia"). Arranca en cero desde hoy.
- `estadisticas.c` / `.h` nuevos; textos en 6 idiomas (f2_idiomas.c). Registro: "[ESTADISTICAS] fin del duelo: resultado 0x.. (terminado 0x..), PV a / b, personaje n, exodia e -> V/D/E". Sin probar.
- Riesgo: el bit 0x40/0x80 está leído del código pero no medido en una partida; si el registro muestra otra cosa al terminar un duelo, se corrige con ese dato.
- 20:4x (el usuario eligió): **cerrar a mitad del duelo no cuenta; los duelos van con el guardado de la partida.** Un duelo terminado queda pendiente en memoria (se ve en las Estadísticas) y se escribe en duelos.txt recién cuando savedata.dat (\Device\Harddisk0\partition1\UDATA\4b4e001e\0061006D1822\savedata.dat vía xbox_translate_path) tiene fecha más nueva que al terminar el duelo (revisado cada 0,5 s desde el hilo del Perfil). Cerrar sin guardar lo descarta, igual que las cartas. Registro: "[ESTADISTICAS] pendiente ..." / "partida guardada: N duelo(s) anotados".
- Leído en el código: rendirse = una orden de `sub_000811B0` que va a `sub_000CFAE0`, que pone en 0 los PV de quien la da → el chequeo lo da por perdido (0x80). Sin cartas para robar: `sub_000CBD40` marca +9 bit 0 → perdido. Exodia u otros: `sub_00083AD0` pone 0x40/0x80 directo. Sin probar.
- 21:0x (maqueta aprobada): **flechas del cartel hacia afuera y letra de títulos en todo el Perfil.** `flechas_afuera()` espeja en memoria los dos triángulos y los iconos de los dos medallones de fondo.png (izquierda ◀ anterior, derecha ▶ siguiente); el clic toma triángulo o medallón (ref 704/1218,116 y 614/1304,136, radio 40); al pasar el mouse se prende la flecha dorada del lado. Toda la letra del Perfil (las dos páginas) es ahora la de los títulos del juego (página FED7C5881DA64D4A, la de "Duelo (Individual)"): `tools/fuente_titulo.py` → `fuente_titulo.h` (celdas 30x26, cada letra medida y puesta en la base, fila 18), `fuente_titulo.c` (ft_ancho / ft_linea / ft_dibujar). Probado fuera del juego con un render de prueba (acentos y colas bien). Sin probar en el juego.
- 21:3x **Arreglo:** un duelo ganado (Duel Computer) no se contó: el registro muestra la escena pasando de 6 directo a 9 (79,7 s), y se esperaba 6 -> 7. Ahora cuenta cualquier salida de 6, y el resultado se toma mientras el duelo sigue en 6 (`ygo_estadisticas_mirar` cada cuadro: con 0x65F894 bit 0x10 y 0x65F8DA, registro "[ESTADISTICAS] el duelo terminó: resultado 0x.."). Ese duelo (guardado 20:33) se agregó a mano a Ajustes\duelos.txt. Sin probar.
- 21:5x (el usuario: "dejame los verdes nomas y hacele una animacion cuando el mouse pasa por arriba y cuando le hace click"): sin la flecha dorada. Los dos triángulos verdes salen de fondo.png (`sacar_triangulos`: transparencia = cuánto difieren de la placa, sin los biseles naranjas; la placa se rellena fila por fila) y van en dos ventanitas propias (vf[2], 110 px ref): quietos; con el mouse encima más claros, un 8% más grandes y con un vaivén hacia afuera de 5 px cada 0,7 s; al hacer clic, un empujón hacia afuera de 12 px y +25% en 0,22 s. No repinta el panel. Revisado fuera del juego (placa limpia). Sin probar en el juego.
- 22:0x Arreglo: las flechas verdes no se veían: a las ventanas vf[2] les faltaba su DC de memoria (CreateCompatibleDC), UpdateLayeredWindow no tenía de dónde copiar. Agregado. Quitada la línea "Victorias con Exodia" (pedido del usuario); las rachas bajan un poco para repartir el lugar. Sin probar.
- 2026-11-16 (usuario: "quedo bien"): CONFIRMADO el Perfil con Estadísticas: flechas verdes animadas, letra de títulos del juego, sin "Victorias con Exodia".
- 2026-11-16 21:0x registro de 3 duelos seguidos: **las estadísticas cuentan bien** (dos victorias, resultado 0x40, personajes 2 y 1, pendientes hasta guardar). Tirones: los de 4,9 / 6,2 / 9,4 s son la ventana en segundo plano ("paused"), no cuentan. **Primer ataque:** solo en el primer duelo de la sesión, un cuadro de 235 ms (0 disco, 0 shaders, 0 texturas, 880 dibujos) 18 cuadros después de crear los dos visores de batalla 480x480 (1920x1920 en la PC, color + profundidad). En el duelo 2 se reusan (sin tirón) y en el 3 se recrean sin tirón: el costo es la primera vez en la sesión. Posible arreglo: preparar esas superficies al cargar el juego (sin hacer).
- 21:2x **Guerrero de la Fortaleza (4015) limitado a 1** (`copias = 1` en cartas.txt, pedido del usuario). Como con Golem: el cofre baja a 1 al guardar, pero las copias que ya estén en el mazo no se sacan solas. Sin probar.
- 21:2x **Tirón del primer ataque:** el kit (nv2a_pb_d3d11.c, al cuadro 30) crea, limpia y suelta 4 superficies 480x480 con su profundidad (tamaño de la PC, como las de mascotas + batalla), para que el driver tenga esa memoria antes de la primera batalla. Registro: "visores precalentados: N de 8 superficies WxH en X ms". Es una hipótesis medida a medias (el costo es solo la primera vez de la sesión); el próximo registro dice si el HITCH de la primera batalla desapareció. Sin probar.
- 21:3x registro nuevo: **el precalentado no sirvió** (corrió: 8 superficies 1920x1920 en 1,1 ms; igual hubo HITCH de 241 ms en la primera batalla, 876 dibujos, 0 disco/shaders/texturas). Quitado del kit. En las dos sesiones el tirón viene justo después del primer sonido de batalla (llamador 0005F255; 1000001C y 10000040; el registro anota cada sonido la primera vez) y en los duelos siguientes no hay tirón: sospecha = primera vez que suena. Medición agregada (sin cambiar nada más): `[SONIDO] cmd .. id .. tardo N ms` si un pedido de sonido tarda más de 15 ms (raton.c, sub_001197F0) y `[MASCOTA] lento: danio/cadena/fundas/mascotas ms` si lo nuestro del duelo pasa 15 ms en un cuadro. Estadísticas: el duelo con Yami Yugi se contó y se anotó al guardar. Guerrero de la Fortaleza: "hasta 1 copias" en el registro.

## 2026-11-16 21:3x — Tocar tu mascota (maqueta de 4 reacciones aprobada)
- Clic sobre tu mascota en el duelo (estado 6, entrada hecha): reacciona, y cada clic hace la siguiente de cuatro, sobre su animación de reposo (los modelos del juego no traen otra; el "ataque" de la batalla es cámara + efectos):
  0 Salto (se aplasta, salta 1/5 de su alto, brillo y anillo dorados), 1 Giro (vuelta completa, chispas azules que suben), 2 Rugido (se agacha, se infla y tiembla, brillo y anillo rojos), 3 Destello (flota, se ilumina, chispas doradas hacia afuera). 0,7 s cada una; un clic mientras reacciona se ignora.
- mascota.c: `ygo_mascota_clic(gx, gy)` (desde raton.c, antes de los botones del duelo; si cae en la caja vx±ancho/2, vy±alto/2 se queda con el clic) y `reaccion()` cada cuadro: como la entrada, rearma las tiras (`cuadro`) con escala horizontal / vertical propias (`s_kx`/`s_ky`; negativa = dado vuelta), anclada a los pies y al medio. Nuevo `vw` (ancho del modelo en el cuadro).
- Kit: el destello de las mascotas ahora tiene tinte y chispas (`g_nv2a_destello_extra[2][2]`: tinte 0 oro, 1 azul, 2 rojo, 3 oro; chispas 0..1, azules suben, doradas salen). Shader verificado con fxc.
- Sonido: uno del juego por reacción con `sub_0011A400` (ids 1000008D, 10000089, 10000017, 10000093: los de la escena del duelo y de la batalla; elegidos sin oírlos, se cambian en `k_reac_sonido`). Registro: "[MASCOTA] tocada en x,y: reaccion n (sonido ...)". Sin probar.

## 2026-11-16 22:xx — Mascotas al ganar / perder (aprobado tras maquetas en Blender del modelo real)
- Vistas previas hechas fuera del juego: `tools/igb_gltf.py` (m931 Sabio Oscuro → glTF) + Blender (Steam, `blender_mascota.py` en el scratchpad). Rechazados: giro 3D, caída de costado, saltos de festejo ("los saltos no quedan"), aura muy llamativa. Aprobado: el par tranquilo.
- En el juego, apenas el chequeo de victoria decide (0x65F894 bit 0x10; 0x65F8DA 0x40 gana el jugador 0, 0x80 el 1; empate: nada), en 1 s:
  - **gana:** su mascota un poco más alta (x1,03 / x1,08 desde los pies), 15 % más luminosa con sus colores, halo dorado suave detrás y motitas que suben;
  - **pierde:** se encoge (x1,04 / x0,90), se apaga (-35 %) y queda gris (85 %), aura violeta suave detrás y humo que sube.
- mascota.c `fin_duelo()` (cada cuadro, después de `reaccion`), `fin_limpiar()`; los clics de reacción no responden después del final. Kit: `g_nv2a_mascota_tono[2][2]` (brillo, saturación: cada cuadro una copia entonada de la imagen de la mascota, `masc_tono`, y sus tiras la muestrean, `masc_tono_srv` en setup_pixel) y `g_nv2a_aura[2][12]` (pase `aura()` sobre la imagen final, detrás de la silueta de la mascota). Shaders verificados con fxc.
- Solo mientras la escena está en 6 (después las mascotas no se dibujan). Riesgo: cuánto dura el 6 después de decidirse; el registro lo dice: "[MASCOTA] fin del duelo: gana la mascota N (X ms desde la escena)" contra "[MASCOTA] estado del duelo 9 (Y ms...)". Sin probar.
- Medición del tirón del primer ataque: el [HITCH] ahora dice "kit drawing N ms, last Present N ms".

## 2026-11-16 — "Nueva" marks no longer over the card's detail
The red "Nueva" badges stayed 250 ms (their hold time) after the Biblioteca
opened a card's detail, over the popup. premios_extra.c marcas_tick now calls
ygo_cartel_marcas_quitar() (cartel.c, new) whenever the mode [0x66E684] is
not the grid (0): they go at once. Built and copied, untested.
Proposed next (mock-up sent, waiting OK): a Master Duel style search for the
Biblioteca (our own panel, F3: name/text, attribute, type, level, ATK, "only
mine", "only new"; non-matching grid cells dimmed with the badge overlay
method, a result click jumps the game's cursor there). The game's grid itself
is not filtered.

## 2026-11-16 — Biblioteca: buscador estilo Master Duel (F3) — sin probar
- Usuario: "hacela asi pero usa la fuente de texto del juego y que todo se parezca lo mas parecido al juego".
- `src/buscador.c` (nuevo; hilo propio, ventanas en capas como perfil.c/cartel.c):
  - botón "Buscar (F3)" (placa dorada) a la derecha de la Biblioteca, game px 478..600 x 80..100;
  - panel (F3 o el botón; F3, Esc o clic derecho sobre él cierran): marco dorado y piedra del Perfil,
    campo de texto en la placa dorada (escribir enseguida; Enter/Esc salen del campo), iconos de atributo
    (los de las cartas), fichas Normal/Efecto/Fusión/Ritual/Mágica/Trampa, Nv 1-4/5-6/7+, barra de ATK
    mínimo (0..4000, de a 100), "Solo nuevas", "Borrar", lista de resultados con la miniatura del juego,
    nombre y Nv/ATK/DEF (rueda: desplaza). Letra del juego (fj_*) y título con la letra de títulos (ft_*).
    Textos en los 6 idiomas (el idioma del juego). Panel en game px x 462..634, y 62..404.
  - sobre la grilla: las que no coinciden oscurecidas, las que sí con borde dorado.
  - busca nombre (texto 849+k) y si no descripción (1988+k), sin mayúsculas ni tildes; solo cartas que
    tenés (las otras están boca abajo sin nombre). La grilla del juego no se filtra.
  - clic en un resultado: `ygo_biblioteca_ir(lugar)` en raton.c (fila de arriba por s_lib_drag, cursor por
    s_lib_want + s_lib_force).
- raton.c: `ygo_buscador_raton` antes que la grilla (1: el puntero es nuestro; 2: además se borra el pad,
  al escribir). premios_extra.c: `ygo_carta_nueva(k)`. main.c arranca el hilo.
- Recursos: `tools/buscador_recursos.py` (fondo.png del Perfil, iconos de texto_cartas.py, miniaturas de
  Game/card/mini/texdb) -> modding/texturas/buscador/juego -> Mods-Extras/buscador (copiar_al_juego.sh).
- Riesgo: la posición del botón se eligió sin captura de la grilla (puede tapar algo de la derecha).
- 23:40 (usuario: "aparece con la carta vieja", "que se note mas el boton de buscar"): las miniaturas del
  buscador ahora salen de las hojas del mod de cartas (buscador_recursos.py toma la hoja del mod si existe;
  copiar_al_juego.sh lo corre después de cartas_mod.py, así siguen al mod). Caius verificado en minis.png.
  Botón más grande (game px 470..608 x 72..104), lupa, "Buscar" con la letra de títulos, "F3" a la derecha y
  un brillo dorado que respira (2,4 s), entero con el mouse encima. Sin probar.
- 23:50 (capturas del usuario: el botón tapaba el encabezado y era más grande que el lugar; el panel tapaba
  el encabezado y el borde de la grilla; el borde dorado tapaba los "Nueva"):
  - botón con la forma de las cajas del juego de la derecha (marco naranja, piedra oscura, letras amarillas,
    lupa amarilla), del ancho de esas cajas, en el hueco entre el encabezado y la primera: game px
    483..590 x 91..107; el marco respira un poco.
  - panel bajo el encabezado y a la derecha del marco de la grilla: x 478..636, y 92..404 (312 alto);
    filas de 20, sin el pie "F3 / Esc: cerrar" (F3 chico junto al título); la barrita de la lista dentro.
  - grilla: borde dorado justo en el borde de la carta, y la ventana debajo de las de cartel.c (los "Nueva"
    quedan encima).
  - datos: los IDs de imagen llegan a 5695 (minis.png ahora 40 x 43); verificado que las 1138 cartas y las
    16 del mod tienen miniatura; Maryokutai (5260) no tiene en el juego: usa card/mini/0000.png como el juego.

## 2026-11-17 — Buscador: lupa, miniaturas, líneas, "Nueva" (sin probar)
- Soldado del Brillo Negro (4409, del mod) salía negro: texdb009 es una textura de 512 x 256 dentro de un PNG de
  512 x 512 (su v va sobre 256 filas). buscador_recursos.py lo detecta en la hoja del juego (nada debajo de la
  fila 256). Verificadas las 1189 miniaturas: ninguna vacía ni negra.
- "Se ve todo cortado por líneas": pegar9 dejaba los bordes de cada parte a medio pixel y se veía el fondo
  (fichas, campo, panel). Ahora corta en pixeles enteros. El panel ya no lleva la línea fina interior (cruzaba
  el título): solo la banda dorada de afuera; la piedra en espejo (sin costuras).
- "Nueva" no se iba al ver la carta (el registro no tenía ninguna línea "vista"): marcas_tick lee la carta de
  la lista de la grilla (0x66DD20, como raton.c) y la borra en cualquier modo de carta abierta (1, 2, 3); las
  marcas se ubican también por esa lista. Registra "[PREMIOS] Biblioteca modo ..." al cambiar de modo.
- Botón: un medallón como el redondo del juego entre las cajas de números (aro naranja, piedra, lupa con aro
  dorado, vidrio celeste y mango de madera; lupa.png / lupa_luz.png), a su izquierda: centro game px 504,155.5,
  r 14.6. Con el panel abierto la misma lupa está junto al título: un segundo clic lo cierra.
- 00:15 revisión de idiomas (usuario: "que funcione en los 5 idiomas"): el panel toma el idioma del juego
  (0x5FC2C0: 1 en, 2 de, 3 fr, 4 it, 5 es; jp -> en), el mismo de los nombres de las cartas (F2 solo lo cambia
  al reiniciar: antes podían no coincidir). Las fichas achican la letra hasta que el texto entra
  (Armadilha, Trappola, Nouvelles; en 4:3 son más angostas). La letra del juego cubre todo Windows-1252
  (ä ö ü ß é è ç ñ...). Las 16 cartas del mod tienen nombre en los 5 idiomas. La búsqueda ignora
  mayúsculas y tildes en todos.

## 2026-11-17 00:45 — Revisión de los clics del mouse (compilado, sin probar)
Pedido: "revisa los clicks del mouse que todos respondan bien... la hitbox de los botones, cuanto tarda".
Leído raton.c entero, el mouse del kit (xboxrecomp-apex/src/input/mouse.c) y los registros.
- Causa general: con el modo puntero (captura NEVER) el kit convierte cada clic izquierdo en A del mando por
  su cuenta (main.c lo enlaza: izquierdo A, derecho B, medio Y). Varias pantallas no lo sacaban, y ese A
  llegaba al juego aunque raton.c decidiera "nada":
  - campo: un clic entre casillas hacía A sobre la última carta señalada; un blanco no ofrecido se
    confirmaba igual. Ahora el A del kit queda afuera y un clic fuera de las casillas no hace nada.
  - botones de comando (Llamar, Colocar...): un clic fuera de ellos, que debía cerrarlos (B), corría el
    comando marcado con el A del kit.
  - cuadros de opciones (fases, sí/no, posición): un clic fuera de las líneas respondía la marcada.
  - Biblioteca: un clic entre cartas o a la derecha abría la carta marcada; uno fuera del cuadro de salida
    respondía la línea marcada (Aceptar = salir). La carta abre igual desde el envoltorio (0 ms, registro).
  - pantalla del nombre: un clic fuera de los botones escribía la letra marcada del teclado en pantalla.
  - info de la carta: solo vuelve atrás.
- mouse_a_off no veía un clic muy rápido (abajo y arriba entre dos lecturas): ahora cuenta también la hora
  del gancho (s_lpress_at / s_rpress_at). Con B también en el Editor de Baraja (mouse_kit_off).
- Cada pantalla tenía su propio "estaba apretado" (campo, botones, menú principal + Perfil, flechas, info):
  quedaba viejo de la vez anterior, y el clic que abría los botones, todavía apretado, podía contar como uno
  nuevo. Ahora todas usan el borde único de pointer_mode (ledge) y uno igual para el derecho (s_redge).
  Antes: un clic derecho que cerraba los botones podía ser otro B en el campo al volver (miraba 150 ms atrás).
- Cuadros de opciones: margen de 8 px arriba de la primera línea, abajo de la última y a los lados (el
  registro del 12/11 tenía dos clics 2 y 4 px arriba de la primera línea: "line -1").
- Pantallas con guante del juego sin manejo propio: un clic con el guante todavía en camino era el A del kit
  en la opción por la que pasaba y después el nuestro en la correcta (dos A). Ahora ese clic espera a que
  llegue (máx. 1,2 s) y entra un solo A. Con el guante ya ahí, igual que antes (al instante).
- Medido, sin cambios: campo y mano coinciden con fotos_cursor (mano y 320..410, carta alzada 307); el cursor
  del campo va a la casilla en el cuadro siguiente; la Biblioteca abre la carta 0 ms después del clic.
- Registro: "field click ... off the squares, nothing", "click game: A N ms after the click (the glove ...)".
- 01:1x (captura del usuario: "no funciona hacer click en OK", cartel "Selecciona a uno de los monstruos de tu
  adversario para atacarlo."): lo rompió la espera del guante de arriba. Registro: "click game 311,221;
  fingertip 545,218" y "A 1203 ms after the click": los carteles del duelo no tienen guante, y uno viejo del
  campo, quieto, contaba como "en camino". Ahora espera solo si el guante se movió en los últimos 300 ms, y
  deja de esperar si se queda quieto 400 ms. En los registros, 2 de cada 3 clics de ese camino tenían un
  guante viejo quieto (545,218 x36...): todos esos carteles vuelven a responder al instante.
- 01:3x (el usuario, a 120 fps: "algun click fantasma o que me hace click muy rapido"). Leído cómo arma el
  duelo sus teclas (sub_0007D880: [0x6589E6] = solo lo recién apretado, sin repetición): un A sostenido es un
  solo clic a cualquier fps; el fantasma es un segundo A. Dos fuentes:
  1. El A del kit sacado en una pantalla volvía en la siguiente con el mismo clic todavía apretado (p. ej.
     "Atacar": los botones se cierran y el cartel que sigue, sin manejo propio, recibía el A y se cerraba
     solo). A 120 fps la pantalla siguiente llega antes, dentro del clic. Ahora ese clic queda sin A del kit
     hasta soltarlo (+300 ms) en cualquier pantalla (s_a_press).
  2. Camino genérico sin guante caminando: el A del kit entraba al instante y raton.c mandaba otro hasta
     700 ms después (mientras probaba el d-pad buscando guante): caía en lo que venía. Ahora ahí el A del kit
     es el único; el propio solo cuando el guante camina.
- 01:5x (el usuario: "revisa en todos los modos de 60, 90, 120 y elimina todos los click fantasma"). Revisados
  todos los lugares que mandan A (pad o inyectado en el manejador): cada pantalla con A propio saca el del kit.
  Lo que quedaba, en ms (no en cuadros), así que vale igual a 60, 90 y 120:
  - Regla general: un clic actúa solo en la pantalla donde empezó (screen_now, el mismo orden que
    pointer_mode). Si la pantalla cambia con el botón todavía apretado, el A / B del kit de ese clic queda
    afuera hasta soltarlo (+300 ms). Cada pantalla del juego tiene su propio "estaba apretado", así que un
    botón sostenido era un clic nuevo en la siguiente; cuanto más fps, antes llega. Registro: "left press
    began on screen X, now Y: its A stays out".
  - Clics pendientes que quedaban guardados: el del campo y el del menú principal se disparaban al volver a
    esa pantalla (su espera ya vencida); el genérico seguía pendiente bajo los botones, el menú, las flechas y
    la info. Ahora se borran al salir. La elección de botón de una lista (cementerio) se borra al abrir otra.
- 02:0x (el usuario: el OK "lee menos rápido el click", "si presionas muy rápido no te lo toma"; pasar el
  mouse "que sea instantáneo"; buscador: "si presiono segundo click me quiere sacar del menu").
  - Clic rápido: el A del kit es el botón apretado en el momento en que el juego lee el mando (mouse.c:
    s_btn); uno soltado antes de esa lectura no llegaba. En el camino genérico (carteles OK) raton.c manda
    también su A al instante con el mismo clic (un solo flanco: empiezan juntos); visto por el gancho, no se
    pierde.
  - Pasar el mouse: revisado; campo, mano, Biblioteca, Editor de Baraja, botones de carta, cuadros y opciones
    ponen el cursor del juego directo en el cuadro siguiente. Solo las pantallas sin manejo propio caminan con
    el d-pad (un paso cada ~0,1-0,15 s). Sin cambio de tiempos.
  - Buscador: el clic derecho en cualquier lado con el panel abierto lo cierra (como F3), y ese clic no
    llega al juego hasta soltarlo (+300 ms): antes, cerrado el panel, el mismo clic todavía apretado (o la B
    del kit que sigue al botón) era B y abría el cuadro de salida. Con el panel cerrado, el clic derecho
    es del juego (salir).
- 02:1x (el usuario: la voz sigue aunque haga clic rápido; antes un clic llenaba el globo y dos avanzaban y
  cortaban la voz suave). Registro 01:40: texto 413, dos clics genéricos y "left press began on screen 0,
  now 14": la pantalla de piedra-papel-tijera corre debajo del globo, y ahí un clic fuera del medio era un
  paso de mano (sin A); además la regla de una pantalla por clic sacaba el A de un clic empezado en la
  genérica. Ahora: (1) con el globo de una línea con voz arriba (voces.c ygo_voz_globo_arriba: escribiendo o
  completo, y su voz sonando) un clic izquierdo en cualquier lado es A para el globo y ninguna pantalla lo
  toma (aplicado al final de ygo_mouse_apply); voces.c cuenta esos A como antes. (2) Un clic empezado en la
  pantalla genérica conserva el A / B del kit aunque la pantalla cambie. Registro: "click on a voice line's
  balloon -> A".

## 2026-11-17 02:5x — Mazo del rival en Estadísticas (maqueta aprobada: "Sí, hacelo así") — sin probar
- Pedido: "en estadisticas donde estan tus rivales ... si presionas en ellos te muestre el Deck que usa".
- Lo que hace el juego (leído en el código): un rival NO tiene mazo fijo. Al empezar el duelo
  (sub_0008C760) sub_0008C6D0 pone TODAS las cartas de su lista B (u32 lista 0x453830 + 16d + 4r, u16 cuenta
  0x453838 + 16d + 2r; entradas u16 ID | peso) y sub_0008C5E0 completa hasta 40 al azar con la lista A
  (0x453630 / 0x453638). d = s8 0x423D60[personaje], r = variante de la historia (ygo_premios_variante).
  Las mismas listas que usan los premios. Los prdeck*.mhr NO son mazos (el juego elige uno al azar).
- perfil.c: clic en un rival de "Últimos duelos" o en "Rival más difícil" (el borde se ilumina al pasar) abre
  un panel sobre la página: retrato, "Mazo de X", récord contra él, "Siempre en su mazo (N cartas)" y
  "Puede sumar al azar", cada carta una vez con xN, tilde verde si la tenés; abajo, la carta señalada (nombre
  del juego, ATK/DEF o mágica/trampa, copias, si la tenés y cuántas). Miniaturas: Mods-Extras\buscador\
  minis.png. Tamaño automático (simulado con los tamaños reales: el mayor es 25 fijas + 57 al azar).
  Se cierra con la X, clic fuera, clic derecho o Esc (solo el panel, no el Perfil). 6 idiomas (f2_idiomas.c).
- estadisticas.c: ygo_estadisticas_rival (récord contra uno) y ygo_texto_juego. Registro: "[PERFIL] mazo del
  rival: personaje ..".
- 03:1x (captura del usuario: "no se ve bien las cartas aunque el diseño de todo muy bien"): las miniaturas de
  35 x 51 agrandadas salían borrosas. El panel ahora dibuja la textura de la carta del juego,
  euro\<idioma del juego>\card\normal\<NN00>\<ID>.tga (todas tipo 1, paleta de 256 colores de 24 bits,
  128 x 256, fila de abajo primero; la carta son las 185 filas de arriba), primero la del mod de cartas
  (Mods-Extras\cartas\archivos, mismo árbol). Lector propio (leer_tga), caché de 160; la miniatura queda de
  respaldo. Comparación hecha a 83 x 121 (el tamaño en la pantalla del usuario): nombre y estrellas legibles.
- 03:2x (el usuario: "hay imagenes mejores como las de la Biblioteca"): la Biblioteca muestra la x4 del pack
  de texturas (512 x 1024, buscada por la huella). tools/perfil_cartas.py arma una por carta e idioma desde
  modding/texturas/idiomas/<xx>/HUELLA_128x256.png (+ mejoradas/texto_cartas, + modding/cartas/salida/hd
  para las del mod), dada vuelta y recortada a la carta, 144 x 208 JPG q85 -> Mods-Extras\perfil\cartas\
  <sp|en|gr|fr|it>\<ID>.jpg (5737, 79 MB; 208 sin x4 caen al .tga). copiar_al_juego.sh lo corre (salta las
  que no cambiaron: 2 s). perfil.c carta_img: primero esa, después el .tga del mod, después el del juego.

## 2026-11-17 03:5x — Limpieza y clics (el usuario: "no se sienten cómodos ahora")
- Borrado con OK ("borra lo seguro", ~2,1 GB): respaldos del 8 al 12/11 (queda 2026-11-14_version_muy_jugable),
  dist (versión del 10/11), ygo-apex/fotos_raton, sobrantes_carpeta_juego_2026-11-13, ygo-apex/runs/*,
  modding/texturas/descartadas, diagnostico_480 del juego. Los packs de texturas de modding y del juego son
  el mismo archivo (enlace duro): borrar uno no libera nada.
- Registro 03:49, tres fallas mías de las tandas anteriores:
  1. "right press began on screen 17, now 6 / 6, now 17" una y otra vez: con un cuadro / botones / lista
     arriba, el campo sigue corriendo debajo y la pantalla salta entre ellos cuadro a cuadro: cada clic
     derecho era "de otra pantalla" y su B quedaba afuera. Las pantallas del duelo (6, 7, 10, 11, 15, 16,
     17) leen la misma palabra de teclas: ahora son una familia para la regla (screen_familia).
  2. El OK después de "Atacar" tomaba su A 171-250 ms tarde ("the glove still on its way"): el guante que
     se movía era el del juego yendo solo al blanco. Ahora es caminata solo si raton.c dio un paso en los
     últimos 400 ms.
  3. "Si movés y apretás el segundo clic no te lo hace": en las pantallas genéricas mover el mouse camina el
     guante con el d-pad y los menús toman una tecla por cuadro: la dirección le ganaba al clic. Un clic
     (izq. o der.) suelta el paso del d-pad y frena la caminata 250 ms; la B dura B_HOLD_MS como mínimo.

## 2026-11-17 04:1x — Fundas al final del duelo y botón de la derecha (sin probar)
- "Al final de cada duelo las fundas de sus cartas se transforman en las mías": la escena del duelo pasa
  6 -> 9 -> 10 al terminar (registro 03:49; el resultado con las cartas todavía en la mesa) y fundas.c
  partía las fundas (kit: g_nv2a_reverso_fila = 200) solo con el estado <= 7. Ahora en toda la escena.
- "Que por default se ponga en el botón de la derecha cuando vas a invocar": al abrirse los botones de una
  carta de tu mano (cursor p0 zona 11), raton.c marca el de más a la derecha (Llamar / Activar; el juego
  marcaba el primero, la información) con el mismo camino que el mouse sobre un botón (s_cmd_want), sin el
  tic (s_cmd_mudo). Si el mouse está sobre otro botón, gana ese. Solo en el modo puntero (el de siempre).
  Registro: "buttons open on a hand card: button N of M marked".
- 04:3x (el usuario: "aparece un segundo aún en el botón izquierdo"; "en los menú de entrar en batalla,
  main phase 2 ... por default esté el de arriba del todo"):
  - El botón se marcaba cuando la fila ya esperaba teclas y la luz se deslizaba desde el primero (10
    cuadros de sub_000ED100). Ahora se envuelve la apertura de la fila, sub_00048010 (rename_pads.py; solo
    llamada desde el paso 1 de los botones, retorno 0x80E2A): para una carta de tu mano pone [fila+8] (el
    botón encendido, que la apertura lee con sub_000ECA80 y donde manda el guante) y el comando de 0x65F8F8
    en el de la derecha antes de abrir. Registro: "lit from the start". El marcado al esperar queda de
    respaldo (si ya está ahí no hace nada).
  - El cuadro de fases (3 líneas, B en el campo) abre con la línea de arriba marcada ([0x660B8A] = 0).
    Registro: "phases box: top line marked".
- 04:5x (captura del usuario: al abrir, "Llamar" y el guante en el de la derecha pero la lupa también
  encendida): agregar el primer botón lo enciende; cambiar solo [fila+8] lo dejaba prendido. Ahora el
  envoltorio de la apertura llama a sub_000ED100_pad(botón, animar 0) antes de abrir: el cambio del propio
  juego (apaga el viejo, prende el nuevo) al instante. Si se negara (fila ocupada), escribe [fila+8] como antes.
- 05:0x (el usuario: en la lista para traer una carta, los botones "que sea con la derecha principal"):
  la Target List (sub_000920B0) al A del paso 2 muestra sus dos botones y marca el 0, la lupa:
  sub_00047C30(0, 1) -> sub_00054E60 -> sub_000ED100(botón, animar) sobre la fila [[[0x656F88]+0x5C]+0x6F8].
  Ahora, mientras corre el paso 2 (fila oculta), la fila queda en el botón 1 (el engranaje: tomar la carta)
  al instante, y el "botón 0" del juego se cambia por 1 en el envoltorio de sub_000ED100: abre en el
  engranaje sin deslizarse. Registro: "target list: buttons open on the gear". Sin probar.
- 05:1x el usuario: "quedó bien parece" (botones por defecto a la derecha: cartas de la mano y Target List).

## 2026-11-17 05:3x — Traba del primer choque (confirmado: "quedo bien")
- El usuario: "a veces sigue pasando que se tranca en la primer animación de choques".
- Registro 04:40: "[HITCH] frame 3948 took 302 ms ... kit drawing 285 ms" (0 shaders, 0 subidas, 0 disco),
  justo después de 153 líneas "[SPR] flip 3947": el rastreo de sprites del kit (spr_log,
  nv2a_pb_d3d11.c, del 8/11 por el parpadeo de Kaiba) seguía prendido; la imagen 480x480 de la batalla se
  dibuja en 256 mosaicos, nuevos para él, y cada línea se vaciaba al disco (~2 ms cada una; el cuadro 1655:
  31 líneas, 76 ms). Ahora apagado salvo RECOMP_SPR_LOG=1.

## 2026-11-17 05:5x — Botón Perfil hasta que cierran las puertas (confirmado: "solucionado")
- El usuario: el botón Perfil "desaparece bastante antes de que la puerta se cierra".
- Registro: rueda 0 -> 2 y puertas 0 -> 1 en el mismo cuadro; el botón se ocultaba ahí y las puertas cerraban ~0,67 s después.
- `raton.c` `sub_0010D6C0`: al elegir un modo, el botón queda en su lugar mientras las puertas cierran (estado 1) y se va cuando están cerradas (2). El "¿Seguro?" (5, 6) lo sigue ocultando al instante. Registro: "[RATON] doors shut: the Perfil button goes".
- Compilado y copiado.
- 05:0x: la captura del usuario era NUESTRA puerta (la del panel Perfil, ojo de Horus), no la del juego. Registro: "boton oculto (panel)" apenas se abre el panel. `perfil.c`: el botón se dibuja mientras el panel no se ve (`!abierto || !panel_vis`): sigue bajo las puertas mientras cierran (la ventana de las puertas va TOPMOST cada cuadro) y vuelve mientras abren al cerrar el panel. Compilado y copiado (sin probar).

## 2026-11-17 06:0x — Mouse lento en Nueva/Cargar partida (midiendo)
- El usuario: en el menú de título los botones responden "un poco lento" (en otros lados va más rápido).
- Leído: el clic usa el mismo camino que el resto (ledge, A propia 60 ms, hover aplicado antes de que el juego lea las teclas); el registro no tenía horas para medir.
- `raton.c`: horas en "title: click/hover/state" (y cuánto antes se apretó), y "title menu: N updates in X ms (slowest gap)" cada segundo en el estado 11. Solo registro, sin cambio de comportamiento. Compilado y copiado.
- 06:1x, medido (registro del usuario): el menú corre a 120 actualizaciones/s (pausa máx. 16 ms), el clic se aplica en 0-16 ms y el hover al instante. Lo que fallaba: 2 de 5 clics cayeron fuera de las cajas — (296,374) entre las opciones y (482,412) justo debajo de Cargar — y no hacían nada. Cajas agrandadas: toda la franja de la fila, y 356..414, x 70..560, partida al medio (x < 305 Nueva, si no Cargar). Compilado y copiado (sin probar). Los logs con hora quedan.

## 2026-11-17 06:3x — Estrellas de nivel nítidas (confirmado: "quedo bien")
- El usuario: las estrellas de las cartas escaladas se veían como manchas; pasó capturas de cartas reales ("así se tienen que ver") y pidió que se integren como el ATK y el atributo, y que el lado derecho no quede "superpuesto".
- `tools/texto_cartas.py`: `estrellas(img, nivel, k)` al final de `escribir` (solo monstruos). Nivel = bits 25-28 de la tabla 0x5EF598 (`textos_bytes` ahora da `nivel`; cartas_mod ya lo pasaba). Medido en el .tga: centros cada 9 px desde x 107.5 hacia la izquierda, y 30.6, radio 4.15.
  - Dibujo: disco rojo-naranja, aro marrón oscuro, estrella amarilla de 5 puntas; el mismo rojo en todas (con el color de la carta, las de ritual/fusión quedaban verdosas). Toma la luz del papel de la carta (×0.88..1.08), sombra suave abajo a la derecha, grano del papel encima.
  - Papel: se borra solo una cápsula de la primera a la última estrella (más ancha a la derecha, donde caía la sombra de la IA), fundida en 1 px de la carta; tono por renglón del papel justo antes y justo después, textura del papel de la izquierda. El rectángulo entero dejaba otro tono a la derecha.
- Muestras aprobadas en el scratchpad (estrellas/muestra.py). Rearmando `idiomas_pack.py` (5 idiomas).
- Niveles 11-12: la primera estrella llega al marco: tono solo de la derecha y sin textura (la tira de la izquierda caía bajo las manchas viejas). Rearmado de los 5 idiomas otra vez.

## 2026-11-17 16:3x — Confirmados por el usuario ("todo lo demás anotalo como hecho")
- Hechos: revisión de clics (OK de carteles, clics fantasma, clic derecho al mover), corte de la voz con clic en el globo, fundas del rival al final del duelo, zona de clic de Nueva/Cargar partida, mazo del rival en Estadísticas.
- Sin probar todavía: la caja de fases abre en la línea de arriba, y la Target List (la carta que trae un guerrero) abre en el engranaje. El usuario no está seguro de que la del guerrero salga a la derecha; el registro actual no tiene "target list: buttons open on the gear" (no se usó en esta partida).
- Respaldo nuevo: respaldos/2026-11-17_version_jugable (2 GB, mismo esquema que el del 14/11, con LEEME). El del 14/11 borrado a pedido del usuario.

## 2026-11-17 17:2x — Botón Perfil sobre las puertas ya cerradas
- El usuario (captura): al elegir otro modo del menú principal, el botón queda sobre las puertas ya cerradas.
- Registro: se iba con el estado 2 de las puertas (672 ms). En `sub_000F49D0` las puertas suenan al juntarse a 500000 µs del estado 1 (sonido 0x1000000A), y el 2 llega a 666667.
- `raton.c`: el botón se va a los 500 ms del estado 1 (cuando se juntan). Compilado y copiado (sin probar).
- 17:3x: el usuario: "quedo exactamente igual". Registro: el botón ya se iba a los 500 ms del cierre; lo que se ve es al VOLVER del menú: el primer cuadro del estado 3 de las puertas carga la rueda (HITCH 340-351 ms justo después de "doors 2 -> 3") y los 330 ms se contaban con el reloj, así que el botón aparecía con las puertas todavía cerradas. Ahora los 330 ms del 3 cuentan cuadros (cada uno a lo sumo 34 ms). Compilado y copiado (sin probar).

## 2026-11-17 17:4x — Caballero Real (reemplaza a Pico de excavación 4165)
- No existía en el juego (buscado por nombre). Luz / Hada / efecto, nivel 3, 1300/800. Ilustración `modding/cartas/Foto_caballero_real.webp`; textos oficiales en 5 idiomas (Royal Knight). Marco elegido solo: Sombrero mágico blanco (4120). Imagen revisada: 3 estrellas, LUZ y ATK/DEF como las demás.
- Efecto `caballero_real` (cartas.c): ninguna carta del juego gana LP al destruir en batalla; se usan dos caminos que ya funcionan. En el resultado de la batalla (`sub_0007F8D0`, como Isaac) se anota la carta destruida por el Caballero; cuando esa carta va al Cementerio por batalla (`sub_000C7C50`, donde Isaac está confirmado), su dueño gana la DEF de la carta (u32 0x5FEF78 bits 0-8 x10) con `sub_000C8DF0` (Escudo drenador). Se borra en el paso 0 del turno. Registro: "[CARTAS] Caballero Real".
- Modelo del usuario (`Caballero_real.glb`, 3243 triángulos) en `modding/modelos/caballero_real`, m158 (original medido 0,2–13,5 × 21,5): riggear_rigido → volcar (textura 8192 reducida a 2048) → `--cuerpo --alto=14 --suelo=3 --derecha=1.5` (3–17).
- premio_peso 35. Exe y mod copiados (sin probar).
- 17:5x: el usuario: el botón se sigue viendo al cerrarse un menú. Video 15-06-50 a 60/s: al volver de la Biblioteca las puertas quedan cerradas casi todo el estado 3 (incluida la traba) y se abren en ~0,15 s al final. Ahora en el estado 1 de la rueda el botón solo vuelve con las puertas en 0 (terminó la apertura), con su fundido de 220 ms. Compilado.
- 18:0x: Caballero Real CONFIRMADO por el usuario ("el efecto quedo bien"); registro: ganó 1000 y 800 LP (DEF de las cartas k 215 y k 220). El usuario ajustó el modelo en el visor (alto 16, suelo 0,5, derecha 1,5) y lo está copiando con el visor (copiar_al_juego.sh también lleva el exe con el arreglo del botón).
- 18:1x: el usuario: "hiciste al revés": con el cambio de 17:5x el botón desaparecía al volver al menú; lo que se ve es al ENTRAR a la Biblioteca / Guardar (sobre las puertas cerrándose). Deshecho el de 17:5x (vuelve: 330 ms de cuadros dentro del 3). Video 15-06-50 a 60/s con audio: del clic del menú a que la rendija de las puertas es más angosta que el botón ~130 ms; se juntan ~240 ms (el golpe suena a ~500 ms). Ahora al entrar el botón se va a los 150 ms del estado 1 (antes 500). Exe 17:58:17 copiado. Sin probar.
- 18:2x: el usuario: "bien se va antes pero ... que quede atrás de la puerta" (como en nuestro panel). Medida en el video 15-06-50 a 60/s la rendija de las puertas del juego al cerrarse (mitad de la rendija / media ventana, desde el inicio del estado 1 tomado del golpe a 500 ms): 1 hasta 20 ms, .96 a 30, luego cada 16,7 ms .83 .69 .56 .44 .32 .19 .115 .0875 .067 .046 .033 .021 .0125 .006, cerradas a 280 ms. `raton.c` pasa la rendija cada cuadro (`ygo_perfil_rendija`) y el botón se va cuando llega a 0; `perfil.c` borra las columnas del botón fuera de la rendija (centrada en la ventana). Exe 18:05:11 copiado. Sin probar.
- 18:3x: video del usuario 18-09-26 a 60/s, el cierre de las puertas del juego al ir a la Biblioteca. El recorte iba adelantado: el botón quedaba mucho más angosto que la rendija real (rendija 0,25 con el botón en 0,04; 0,125 con 0,03; 0,065 con 0,009), unos 70-95 ms antes que las puertas. El cuadro del juego llega a la pantalla después que nuestra ventana. Con nuestro panel el botón queda entero bajo las puertas. Ahora la tabla corre 80 ms más tarde (cerradas a 360 ms de nuestro reloj). Exe 18:11:46 copiado. Sin probar.
- 18:2x: confirmado por el usuario ("quedo bien eso"): el botón del Perfil queda detrás de las puertas del juego.
- 18:2x: revisión de fluidez a 120 fps (registro de 18:16, solo lectura). vblank: 600 en 5 s, separación 7,1-9,5 ms; mascota: 117-120 cuadros/s, 0,8 ms por dibujo (4,8 %). Tirones: solo en transiciones con puertas (195-463 ms, carga de menú o duelo, tapados por las puertas), 168 ms al armar el duelo, 92 ms al empezar el duelo y 44 ms al empezar una batalla (modelos 77 y 158 nuevos: 2 render targets 480x480 y texturas). Los 36 s fueron la ventana en segundo plano (pausa).
- 18:2x: el usuario: "Sí, buscalo" (el tirón de las batallas). En el registro de 18:16, cada batalla con un monstruo nuevo tiene un cuadro de 28 a 47 ms, no solo la primera (8732: 44 ms; 9083: 39 ms; 9686: 47 ms). En esos cuadros el juego lee del disco el modelo (`Z:\monster\...\mNNN.igb`), el efecto (`effect\battle_igb\beNNN.igb`) y sube sus texturas, varias reemplazadas por el pack. Solo medición (xboxrecomp-apex nv2a_pb_d3d11.c): `tex_upload` cronometrada (`s_texup_ms`), y el [HITCH] sale desde 25 ms (antes 150) con "N texture uploads (X ms)". Con el disco (ya medido) y las texturas, lo que sobra es el código del juego (leer el igb). Exe 18:21:20 copiado. Falta: el registro de una partida con batallas.

## 2026-11-17 18:5x — Abare Ushioni (reemplaza a Payaso enmascarado 4188)
- No existía en el juego (buscado por nombre en inglés y español). Tierra / Guerrero Bestia / efecto, nivel 4, 1200/1200 (datos de la carta real que mandó el usuario). Ilustración `modding/cartas/Foto_abare_ushioni.webp`; texto de la carta (DR3) en 5 idiomas. Marco elegido solo: Jirai Gumo (4373). Imagen revisada: 4 estrellas, TIERRA, ATK/DEF como las demás.
- Efecto `abare_ushioni` (cartas.c): pasa por Mago de tiempo (4022, alias en el efecto y en el repaso del turno). Se envuelven su condición `sub_000A5010` (la de Mago pide la bandera 0x2000 y monstruos del adversario; la de Abare: boca arriba y sin usar este turno, `s_abare_usado[jugador][casilla]`, se limpia en el paso 0) y su resolver `sub_000B2D10`: 0x80 pide la moneda (`sub_00084D00(p, 0x13)`, el pedido común de Comediante y Gamble), 0x7F el resultado (u16 0x65F930 != 0 = acertó). Abare en 0x7F: `sub_000C8BC0(adversario o él, 1000)` y devuelve 0xA (fin, orden 0x6F, como Mago). rename_pads + cartas_mod.py (efecto conocido). Registro: "[CARTAS] Abare Ushioni".
- Modelo del usuario (`Abare_ushioni.glb`, 3385 triángulos) en `modding/modelos/abare`, m181 (original medido 9,9–27,6 × 11,5, agregado a medidas_originales.txt): riggear_rigido → volcar (rutas absolutas: con relativas Blender no puede escribir) → `--cuerpo --alto=15 --suelo=3 --derecha=1.5`. Mira en 3/4 a la cámara.
- premio_peso 35. Exe 18:48:56 y mod copiados. Sin probar.
- 19:0x: Abare Ushioni FUNCIONA (usuario: "si funciona"), pero la moneda no se podía elegir con el mouse, solo con teclado. El pedido de moneda (modo 0x13) es `sub_00090D20` (despachador sub_00085350, tabla 0x85668 por modo-1): paso u8 0x65F92E 1 espera teclas, 0x30 da vuelta la cara (u16 0x660B8C, sub_00047600 la muestra en la moneda), A la elige. raton.c lo envuelve (rename_pads): clic en una flecha (~225,224 / ~414,224 px del juego) o la rueda da vuelta; clic en la moneda (centro ~322,213) elige; se pasa como 0x10 / A en una llamada hasta que el juego lo toma. Posiciones medidas en la captura del usuario. Registro "[RATON] coin toss". Exe 19:02:18 copiado. Sin probar.
- 19:1x: pedido del usuario: girar el modelo en el visor. `armar_igb.py --girar=G` (grados sobre el eje vertical, Blender Z = juego Y; por el centro del cuerpo con --cuerpo, si no por el centro de la caja): gira posiciones, normales y las matrices de los huesos (reposo y animación). Visor: fila "Girar" (-180..180, de a 5) que se ve al instante (rotación relativa a lo armado, sobre el eje en x = derecha), "Armar" la pasa y la guarda en ajuste.json ("girar") y en la tabla. Probado: Abare armado a 45° en un archivo aparte, exportado y renderizado: girado, malla entera. El m181 del juego no se tocó. Hay que reiniciar el visor (visor.py) para que tome el cambio.

## 2026-11-17 19:3x — Acantilado Naturia (reemplaza a Guerrero de montaña 4035)
- No existía (ninguna Naturia en el juego). Tierra / Roca / efecto, nivel 4, 1500/1000 (carta del usuario). Ilustración `modding/cartas/Foto_acantilado_naturia.webp`; textos en 5 idiomas. Marco: Jirai Gumo (4373). Imagen revisada.
- Efecto `acantilado_naturia` (cartas.c). Leído en el código: `sub_000C7C50` (solo la llama el final de la batalla, sub_0009D350) registra los disparos al Cementerio por batalla (Sangan, Rata, Tomate, Momonga…); `sub_000C5DA0` (la llaman los demás envíos del Campo al Cementerio) registra los de Sangan/Bruja, no los de Rata. Acantilado pasa por Sangan (4054) mientras corren las dos (disparo por cualquier causa). Efecto: alias de Rata gigante (4915); su resolver `sub_000B0FF0` (pregunta, lista, elegir, Invocación Especial boca arriba en ataque) envuelto: mientras corre, `sub_000C1B80` devuelve una lista propia: Deck (0x65D9FC + p*0x13F8, cuenta u8 0x65D0A9, entradas 8 bytes, origen 2) con monstruos de Nivel 1-4 y "Naturia" en el nombre (cartas.txt; se marcan al cargar el mod: toda Naturia nueva entra sola, ya entra Cerezas Naturia). rename_pads: sub_000C5DA0, sub_000B0FF0. Registro "[CARTAS] Acantilado Naturia".
- Modelo del usuario (3312 triángulos) en `modding/modelos/acantilado`, m028 (original 0–24,1 × 13,2, agregado a medidas): riggear_rigido → volcar → `--cuerpo --alto=17 --suelo=3 --derecha=1.5`. Render de frente revisado.
- Un comando mío abrió Blender con ventana por error (faltaba -b); cerrado.
- premio_peso 35. Exe 19:26:11 y mod copiados. Sin probar.
- 19:4x: el usuario: "parece que está bien" (Acantilado Naturia).

## 2026-11-17 19:5x — Berenjena Naturia (reemplaza a Segadora de fuego 4160)
- No existía. Tierra / Planta / efecto, nivel 2, 1000/700 (carta del usuario). Ilustración `modding/cartas/Foto_berenjena_naturia.webp`; textos en 5 idiomas. Marco: Flor de arco iris (4494). Imagen revisada.
- Efecto `berenjena_naturia` (cartas.c): disparo de Sangan (como el Acantilado; `como_sangan()` en los envoltorios de sub_000C7C50 y sub_000C5DA0 cubre las dos cartas). Efecto: alias de Mago de la fe (4434): elección del objetivo `sub_000BABA0` (lista con sub_000C1B80; la CPU elige con sub_00089CB0) y resolver `sub_000A9570` (sub_000D5050 + orden 0x9F, del Cementerio a la mano), envueltos con la carta pasando por Mago de la fe; la lista es propia: Cementerio (0x65DC7C, cuenta 0x65D0AA, origen 4), monstruos "Naturia" salvo la Berenjena. Mago de la fe no pregunta Sí/No: el texto dice "selecciona" (no "puedes"). rename_pads: sub_000BABA0, sub_000A9570. Registro "[CARTAS] Berenjena Naturia".
- Modelo del usuario (3376 triángulos) en `modding/modelos/berenjena`, m153 (original 0,2–19,5 × 11, agregado a medidas): `--cuerpo --alto=13 --suelo=3 --derecha=1.5 --girar=-60` (de frente miraba de costado; con -60 mira a la cámara como en la ilustración; renders revisados).
- premio_peso 35. Exe 19:48:30 y mod copiados. Sin probar.

## 2026-11-17 20:0x — Naturia Vanidosa (reemplaza a Doron 4201)
- No existía. Tierra / Planta (el juego no tiene Cantante) / efecto, nivel 1, 200/300. Ilustración `modding/cartas/Foto_naturia_vanidosa.webp`; textos en 5 idiomas. Marco: Escorpión de acero (4400). Imagen revisada. El usuario aclaró "revisa los efectos juntos" = él los prueba todos juntos en duelo.
- Efecto `naturia_vanidosa` (cartas.c), leído en el código: es Maryokutai (5260), el monstruo boca arriba del juego que responde a una Mágica del adversario sacrificándose. `sub_00082140(activación, p, zona, z)` decide quién puede responder (0x41; en casillas de monstruo solo Maryokutai, imagen 0x148C, boca arriba, tipo 0x16): envuelta, Vanidosa boca arriba responde a Mágicas (0x16) y Trampas (0x15) si hay otro monstruo Naturia boca arriba en su Campo. Alias de Maryokutai para la entrada; su condición `sub_000A8500` y su resolver `sub_000B71A0` envueltos: sacrifica al otro Naturia (el de menos ATK, elegido solo) y a Vanidosa con `sub_000C7980(p, z)` (el sacrificio de Maryokutai) y encola la orden 0x88 (negar y destruir; la misma que usan Magic Jammer y Siete herramientas para Trampas). rename_pads: sub_00082140, sub_000A8500, sub_000B71A0. Registro "[CARTAS] Naturia Vanidosa". Sin verificar: la IA (sub_000A4EC0 mira 0x148C) probablemente no la use; si Maryokutai solo responde en el turno del adversario por otro lado, Vanidosa también.
- Modelo del usuario (3378 triángulos) en `modding/modelos/vanidosa`, m194 (original 0,1–15,4 × 9,2): `--cuerpo --alto=11 --suelo=3 --derecha=1.5`. Render de frente revisado.
- premio_peso 25. Exe 20:01:47 y mod copiados. Sin probar.

## 2026-11-17 20:1x — Chinche Hedionda Naturia (reemplaza a Adicto de medianoche 4228); Cosmoremolacha pendiente
- Chinche: no existía. Tierra / Insecto (sin Cantante) / efecto, nivel 3, 200/500. Textos en 5 idiomas. Marco: La bestia de gusano perversa (4029). Imagen revisada.
- Efecto `chinche_naturia` (cartas.c), como Krebons (siempre, sin pregunta; el texto no dice "puedes"): en la declaración del ataque (envoltorio de `sub_0009A290`) si el atacado (0x6666E0 bits 13-15) es un monstruo Naturia boca arriba y su dueño tiene la Chinche boca arriba: la Chinche al Cementerio con `sub_000C7980` y orden 0x1F | dueño<<15 (la de Anule el ataque 4804, resolver sub_000AD1F0: niega los ataques y termina la Battle Phase). Registro "[CARTAS] Chinche Hedionda Naturia". Investigado y descartado: que el jugador elija responder con el monstruo (sub_00082280 elige quién responde; no se sabe qué recibe para un ataque).
- Modelo del usuario (3406 triángulos, el insecto con hojas) en `modding/modelos/chinche`, m221 (original 1–19,9 × 38,1): `--cuerpo --alto=14 --suelo=3 --derecha=1.5`. Render revisado.
- Exe 20:13:44 y mod copiados. Sin probar.
- Cosmoremolacha Naturia (reemplazaría a Alma de Hinotama 4216, m209): NO hecha. "Cuando tu adversario Invoca de Modo Normal/Coloca un monstruo: puedes Invocar esta carta de Modo Especial desde tu mano." Ninguna carta del juego se activa desde la mano así. Leído: la orden que saca del Deck es 0x44 (sub_000CC350, Rata gigante), del Cementerio 0xA0 (sub_000CCFE0, Llamada del encantado); la de la mano no está identificada (candidatas 0x42 sub_000CC190 y 0xA1 sub_000CD3B0, tocan la mano 0x65D0A8 y llaman sub_000D5250). Archivos copiados: Foto y glb en el Escritorio.

## 2026-11-17 21:3x — Cosmoremolacha Naturia (reemplaza a Alma de Hinotama 4216)
- No existía. Tierra / Planta / efecto, nivel 2, 1000/700. Textos en 5 idiomas. Marco: Flor de arco iris (4494). Imagen revisada.
- Camino (pista del usuario: cartas que salen de la mano): la Llamada de la Momia (5435, resolver `sub_000B8AB0`) saca un monstruo de la mano con la orden 0x8E (cmd|p<<15, u16 entrada, u16 entrada+2, 0) y `sub_000963C0(p, &entrada_mano, 1, 0)` (arma 0x66BA7C.. y sigue en la máquina de la invocación `sub_00095D80`). Los Espíritus (5251-5255) se invocan desde la mano por el menú (`sub_00093A30`/`sub_000968D0`), no por efecto. Las respuestas desde la mano (`sub_00082140` zona 0xB → `sub_000A4DE0`) son solo Mágicas Rápidas.
- Efecto `cosmo_naturia` (cartas.c), sin pregunta (el texto no dice "puedes"): después de cada paso de la Llamada Normal (`sub_00093FA0`), en el estado 2 (una vez por llamada; ahí el juego registra los disparos al ser llamada) si el otro jugador tiene la carta en la mano y una casilla libre se registra el disparo como el del Goblin: `sub_00081EF0(((casilla_libre|0x840|dueño<<15)<<16)|k)`, alias de Garuda (5255). El resolver (envoltorio de `sub_000B6DA0`, rama propia antes del Goblin): 0x80 busca la carta en la mano, orden 0x8E + `sub_000963C0` → 0x7E → fin. Registro "[CARTAS] Cosmoremolacha Naturia".
- Dudas para la prueba: si el disparo se resuelve con la carta en la mano (el juego podría validar la casilla); si Colocar boca abajo también pasa por el estado 2; la posición la elige la máquina de invocación (como la Momia).
- Modelo del usuario (3360 triángulos) en `modding/modelos/cosmo`, m209 (original 3,9–17,0 × 13,7): `--cuerpo --alto=14 --suelo=3 --derecha=1.5`. Render revisado.
- Exe 21:32:48 y mod copiados. Sin probar.

## 2026-11-17 22:2x — Prueba del usuario de los Naturia: Chinche, Vanidosa y Acantilado "están mal"
- Registro (registro.txt 22:14): la CPU atacó a la Chinche boca arriba (k 218) y después a la Vanidosa: ninguna línea de la Chinche → la declaración `sub_0009A290` no corre (o no con 0x65F8DE==1) cuando ataca la CPU. Krebons usa el mismo enganche y nunca se probó: probablemente tiene el mismo problema.
- Arreglo Chinche: el chequeo pasa al paso 1 del combate (`sub_0009A540`, rayo.c → `ygo_cartas_paso_ataque` en cartas.c), una vez por ataque, atacante = jugador del turno ((0x65F8DA>>1)&1). Registra cada ataque a un Naturia y si la Chinche actúa o no.
- Acantilado sacrificado sin efecto: el sacrificio `sub_000C7980` (Invocación por Sacrificio, Maryokutai, etc.) compara la imagen con Sangan (0xFD6) y registra el disparo por su cuenta; solo estaban cubiertas la batalla (C7C50) y la destrucción (C5DA0). Envuelto `sub_000C7980` (rename_pads): Acantilado/Berenjena pasan por Sangan mientras corre. Registro "sacrificada ... pasa por Sangan".
- Vanidosa: en ese duelo la CPU no activó ninguna Mágica/Trampa (las 3 cadenas del registro son del usuario: Carga del Poderoso, Refuerzo del Ejército, Decreto real) y no hay ninguna línea de la Vanidosa: nunca tuvo a qué responder. No limita turnos. Falta probarla con una activación de la CPU.
- Exe 22:20:55 copiado. Sin probar.
- 22:2x Vanidosa (el usuario: el rival le negó una Mágica en su turno con una carta de cancelar Mágicas y la Vanidosa no pudo encadenar): `sub_000A4EC0(activación, p)` decide si p puede encadenar; revisa las Mágicas/Trampas propias, en su turno las Rápidas de la mano, y SOLO fuera de su turno y contra una Mágica (0x16) los monstruos (Maryokutai 0x148C boca arriba). Envuelto (rename_pads): con la Vanidosa boca arriba, otro Naturia boca arriba y una Mágica o Trampa del ADVERSARIO (entrada de cadena +2 bit 0 = dueño) devuelve 1 en cualquier turno. El chequeo por casilla (`sub_00082140`) ahora también exige que la activación sea del adversario. Registro "puede encadenar" / "falta otro Naturia". Exe 22:27:05 copiado. Sin probar.
- 22:4x Copias de más en el cofre (el usuario: tiene 1 en el mazo; después de que edito la carta y vuelve a jugar, aparece otra afuera del mazo → 2; nunca le salieron como premio). Mirado: mi código solo baja el cofre (límites); premios_registro.txt muestra sorteos de cartas del mod pero el usuario dice que nunca se las dieron. Las únicas funciones del juego que SUMAN al cofre: `sub_000D9420` (repasa las 0x473 cartas, pone el límite y suma 1 por cada carta del mazo 0x66609A: parece rearmar el cofre desde el mazo) y `sub_000D9720` (llamada desde 0x101A2F). Diagnóstico: las dos envueltas con registro "[COFRE] sub_..." y `ygo_cofre_vigilar` (hilo de premios_extra, 10/s) anota cada cambio del cofre con la hora. Copias actuales en la partida: Acantilado 2, Berenjena 2, Vanidosa 2, Caballero Real 2, Chinche 1, Cosmoremolacha 1. Exe 22:43:03 copiado. Falta: que pase de nuevo y mirar el registro.
- 22:5x Cosmoremolacha (prueba del usuario: sale la carta en pantalla pero sigue en la mano). Registro: disparo registrado, cadena de 1 eslabón 00CE (k 206), pero ninguna línea del resolver. Causa: la máquina de la cadena (`sub_00082F80`) copia la casilla del Campo del eslabón (+2: z bits 4-9, dueño bit 0) a 0x66BFB4 con `sub_000D1E70` y esa copia es el ctx del resolver; para una carta en la mano la casilla está vacía → `es_k` fallaba y corría el resolver de Garuda. Arreglo: se la reconoce por el eslabón que se resuelve (u16 0x66BF98; entradas 0x14 en 0x66BD04: +0 k, +2 bit 0 dueño). Registro "resolver, estado NN". Exe 22:50:47 copiado. Sin probar.
- El registro del cofre ya anda: al cargar la partida anota ~240 cambios (el cofre cargándose); falta que aparezca la copia de más.
- 22:5x Cosmoremolacha sigue en la mano (registro 22:56: disparo y cadena sí, resolver nunca). Causa real: la tabla de alias (`NALIAS 8`) estaba llena y `alias()` descartaba en silencio: faltaban las líneas "(como ...)" de la Vanidosa (Maryokutai 5260) y la Cosmoremolacha (Garuda 5255). Sin alias, la búsqueda del efecto no les daba entrada → no hay resolver. La Vanidosa tampoco podía resolver por eso. NALIAS → 32 y si se llena se registra "SIN alias". Exe 22:57:13 copiado. Sin probar.
- 23:0x Cosmoremolacha: el alias ya anda ("(como 5255)" en el registro), pero el rival COLOCÓ boca abajo y no hubo disparo. Las máquinas de Llamada (despacho en 0x95A30, tabla 0x95D68): sub_00093FA0 y sub_00094610 (orden 0x90, boca arriba), sub_000951D0 y sub_00095450 (orden 0x52: Colocar), sub_000957D0 (0x90). Ahora la Cosmoremolacha registra también en sub_00094610 (estado 4, como Senju) y al terminar las dos de Colocar (devuelven ≠ 0); envueltas 951D0/95450 (rename_pads). Registro "... de la Llamada Normal/Colocar del adversario; disparo registrado". Exe 23:06:54. Sin probar.
- 23:1x CORRECCIÓN: 951D0/95450 NO son Colocar, son Llamada Especial (verificado por quién arma cada tipo). Mapa definitivo de las máquinas de Llamada (despacho sub_00095A30, tabla 0x95D68; tipo = (u16 0x66BA8E>>2)&7, estado = (>>5)&0x7F; jugador u32 0x66BA7C bit 0, boca arriba bit 14 (0x4000); carta u16 0x66BA80):
  1. sub_00093FA0 — Llamada Normal Y Colocar. Estado 0 sacrificios + orden 0x90; estado 1: boca arriba → orden 0x4C y estado 2 (registra disparos al ser llamada: Senju, Sincronizador, Caius, Cosmoremolacha); boca abajo → termina (estado 0xA) sin disparos.
  2. sub_00094610 — Llamada Normal boca arriba (fuerza 0x4000); disparos en el estado 4.
  3. sub_00094AF0 — Llamada de Volteo (la arma sub_00096160).
  4. sub_000951D0 — Llamada Especial desde Deck/Cementerio (sub_000961F0 / sub_000962E0).
  5. sub_00095450 — Llamada Especial desde la mano (sub_000963C0, Llamada de la Momia).
  6. sub_000957D0 — la arma sub_000964A0 (no identificada).
  Cosmoremolacha: 93FA0 estado 2, 94610 estado 4 y Colocar (93FA0 estado 1 boca abajo). 951D0/95450 envueltas sin cambios. Revisadas las demás: Sincronizador ("Invocada de Modo Normal") y Caius ("por Sacrificio") usan 93FA0 est. 2 / 94610 est. 4 = solo boca arriba, correcto; las de Volteo van por 94AF0 y el volteo del juego. Exe 23:12:44.

- 2026-11-17 23:3x — Prueba del usuario: Acantilado Naturia, Berenjena Naturia y Abare Ushioni FUNCIONAN. Faltan probar: Naturia Vanidosa, Chinche Hedionda Naturia, Cosmoremolacha Naturia. Total de cartas del mod: 23 (cartas.txt), todas con nombre y texto en es/en/fr/de/it.
- 2026-11-17 23:4x — Prueba del usuario: Berenjena se disparaba al ser VOLTEADA (registro: eslabón 0098/52/28 sin "va al Cementerio"): el alias de Mago de la fe es de Volteo y la Llamada de Volteo (94AF0, con alias) registraba su disparo. Envuelto `sub_00081EF0` (registro de TODO disparo, rename_pads): el de la Berenjena solo pasa si su imagen es Sangan (los 3 caminos al Cementerio: C7C50, C5DA0, C7980). Registro "disparo de Volteo descartado".
- Chinche: "me atacaron y no pude hacer nada"; el registro no tenía NINGUNA línea "ataque del jugador". Causa: `ygo_cartas_paso_ataque` se reiniciaba con estado != 1, pero el paso 1 (9A540, tabla 0x6010F8[1]) solo corre en estado 1 → tras el primer ataque de la partida no revisaba más. Ahora `ygo_cartas_paso_ataque_fin()` en rayo.c, después del pad: reinicia si el paso devolvió != 0 (el despacho sub_0009E5C0 pasa al 2) o cambió el estado. Exe 23:46:09 copiado. Sin probar.
- 2026-11-18 00:0x — Usuario: "vanidosa no me ofreció nada". Registro 23:41: "puede encadenar a la carta k 771" → el juego SÍ ofreció: `sub_00082890` llama a `sub_00082280(act, p)`, que para el humano arma el texto 0xC3E/0xC3F (nombre de la carta del rival) y muestra la caja Sí/No (`sub_000880B0`; RATON "box of 2 lines" justo después). Se cerró con clic derecho ("right press began on screen 17" = B en la caja = No) y la cadena terminó sin pasar por sub_00082140. Con Sí sigue la elección en el Campo (`sub_00092E20`, máscara 0x0F en el turno del jugador 0 / 0xEE en el del 1) y luego sub_00082140 con la casilla elegida. Sin cambios de código.
- 2026-11-18 00:0x — Chinche CONFIRMADA por el usuario ("si funcionó la chinche"). Vanidosa: el rival activó La Máquina que Absorbe Vida en el turno del usuario; registro: "puede encadenar", Sí en la caja, y en la elección del Campo la casilla de la Vanidosa "not a choice here (mask 0F)". La máscara de `sub_00092E20` la pone sub_00082280 (0x82490): 0x0F en el turno del jugador 0, 0xEE en el del 1. `sub_00092620`: bits por jugador << (p*16); monstruos (casilla 0, caso 0x926D0) = 0x20 boca arriba / 0x10 boca abajo / 0x40-0x80 según +0xA bit 0; 0x0F no tiene monstruos. Ahora raton.c (envoltorio de sub_00092E20, vuelta 0x824A9) llama a `ygo_cartas_mascara_cadena` (cartas.c): con la Vanidosa boca arriba, pareja Naturia y Mágica/Trampa del rival suma 0xE0 (lo que el juego da fuera de turno). Registro "elección de la cadena, máscara 0F -> EF". Exe 00:02:42 copiado. Sin probar.
- 2026-11-18 02:5x — Usuario: contra Yugi, al atacar/invocar, "el disco está rayado o sucio". Registro (02:25-02:52, ~26 min, varios duelos): `xbox_HeapAlloc: out of memory (requested 16777216, used 43507712/50528256)` y luego 20 MB con 49,3/50,5 MB. [HEAP] exhausted: 264 bloques vivos = 18 MB + **17 MB de sobra en bloques reusados**, libre fragmentado (el mayor 7,4 MB); "reclaim off". Sin RECOMP_HEAP_RECLAIM el heap le da un bloque liberado ENTERO al siguiente pedido aunque sea chico: con el tiempo se agota. Ahora `RECOMP_HEAP_RECLAIM=1` en portable_defaults (main.c) y env.bat (el kit lo trae: parte y une bloques liberados). Exe 02:55:00 copiado. Sin probar: jugar varios duelos seguidos (~30 min) y ver si vuelve; en el registro buscar "out of memory".
- 2026-11-18 22:2x — Pedido del usuario (Desktop\YU-GI-OH DAWN OF DESTINY\Dawn of Destiny.txt, 4 primeras): cuelgue, "disco sucio" (~30 min), límite de copias en premios, mostrar el límite en la carta.
  - **Cuelgue 20:02 (registro del usuario):** con RECOMP_HEAP_RECLAIM el heap ya no falló; ahora se llenó la **memoria contigua (texturas)**: `[CONTIG] arena exhausted (131072 requested ... 64300 of 65408 KB in use)` a los 29 min. Sube ~6 MB por duelo y no baja (ninguna liberación ignorada por el kit: el juego la retiene). Cada carga de mascotas suma ~2,8 MB y al soltarlas no baja nada.
  - **Causa (mascotas):** el visor de cada mascota guarda el nodo del modelo ([v+0x34], en su grupo [v+0x30]); soltar la manija (sub_00019700) no alcanza, el nodo retiene modelo y texturas. **Arreglo (mascota.c):** envuelto `sub_00045750` (cierre de la escena del duelo: sub_00044E60 + [0x656F88]=0; rename_pads): antes, con la escena viva, `soltar_visores()` hace lo de sub_0005A670 al cambiar de monstruo (0x5A6AE..0x5A6E9): sub_002345A0(grupo; nodo), una referencia menos, sub_0019F5E0 en 0, [v+0x34]=0; luego soltar_todo(). Registro "[MASCOTA] n: el visor suelta su modelo".
  - **Diagnóstico en el kit** (xboxrecomp-apex: kernel_bridge.c + xbox_memory_layout.c): cada bloque contiguo guarda las 4 primeras direcciones de código del juego en la pila al pedirlo; desde 24 MB en uso y cada 8 MB más, "[CONTIG] vivo (...)": los 16 grupos que más retienen. Para ver si queda otra fuga (dentro del duelo sube ~3,7 MB más entre cargas de mascotas).
  - **Premios y límite de copias (premios_extra.c):** el re-sorteo ("sin cuarta copia") ahora usa el límite de cada carta (`tope_k`: u8 0x665896 + k del guardado, el del mod ya bajado por ygo_cartas_limites; 0 = prohibida) en vez de 3 fijo.
  - Exe 22:26:35 copiado. Sin probar.
- 2026-11-18 22:5x — **Límite de copias en la carta** (el usuario eligió la muestra A, círculo con número): `tools/cartas_mod.py` dibuja un círculo arriba a la izquierda (rojo "1", naranja "2", borde dorado; `insignia()`) en la carta grande (.tga 128x256), la HD (cartas.pack) y las hojas de miniaturas (normal y HD por idioma), los 5 idiomas: en las cartas del mod con `copias` 1/2 (o limitadas por el juego) y en las 61 limitadas del juego (`limitadas_juego()`: 61 u16 en 0x450BC0, id bits 0-13, copias 14-15; 52 a 1, 9 a 2), que pasan a ir en Mods-Extras\cartas\archivos como reemplazo. Prohibidas (0) y los límites "Personalizado" no se marcan. Revisado en imágenes (Sangan, Morphing Jar #2 en, Golem, Guerrier Forteresse fr). Copiado. Sin probar en el juego.
- 2026-11-18 23:0x — Límites pedidos por el usuario (cartas.txt `copias`): Cosmoremolacha 1, Acantilado 2, Abare Ushioni 2, Caballero Real 2, Soldado del Brillo Negro 1, Escudo drenador 2. Mod rearmado (círculos) y copiado; lámina actualizada en Desktop\YU-GI-OH DAWN OF DESTINY\cartas_agregadas.png. Al bajar el límite, ygo_cartas_limites deja en el cofre como máximo max(límite, copias en el mazo).
- **PENDIENTE: Prisión dimensional (4990, reemplaza a "El Lawbook de Solomon") SIN PROBAR**: el usuario nunca la consiguió (premio_peso 35). Probarla cuando salga (trampa: al declarar ataque el rival, destierra al atacante; condición de Cilindro mágico sub_000A6810).
- 2026-11-18 23:2x — **Textos en español de España corregidos** (el usuario): 13 cartas con "vosotros" (descartáis, robáis, cogéis, vuestras...) reescritas con "ustedes"/"tomar" en cartas.txt con `solo_espanol = si` (4597, 4617, 4785, 4821, 4896, 4908, 4913, 4923, 4928, 4955, 4969, 5127, 5218). cartas.c: con ese campo el nombre/texto va solo a la tabla 5 (español); cartas_mod.py: solo la carta "sp", sin ilustración conserva el dibujo; si la versión mejorada propia está sucia (Cyber pote: tomada por Mágica, etiqueta sobre las estrellas y restos de texto) usa el marco de otra limpia y le pega su propio dibujo. "Baraja" (75 cartas) sin tocar: preguntar si pasa a "Deck". Revisado en imágenes (Desktop\...\textos_corregidos.png).
- **AUTO en el duelo** (muestra aprobada): `src/auto.c` (nuevo, CMakeLists): placa bajo el nombre del jugador (game px 28,275 74.7x24.9), ventana en capas como buscador.c, solo con escena de duelo y estado 0x657148 = 6; imágenes Mods-Extras\auto\auto_on/off.png (`tools/auto_recursos.py` desde modding/texturas/auto/placa_nombre.png + letra del juego; copiar_al_juego.sh las copia). Encendido: `sub_000A4EC0` devuelve 0 para el jugador 0 (cartas.c) → el juego no muestra la caja "¿encadenar?". Clic en la placa (raton.c, antes del clic del campo) cambia y guarda (xbox_MenuSaveSoon). F2 y Configuración "auto_cadena" No/Sí, 6 idiomas (f2_idiomas.c, Configuracion.cs; Configuración.exe recompilado y copiado). Registro "[AUTO]".
- **Guardar al ganar (opción 1 elegida)**: la lectura de la partida es sub_0002C900 ("u:\0061006D1822\savedata.dat", "rb"); la escritura no aparece directa (sub_000E0250 es T:\deck.mhr). Archivo: 20 bytes de firma + "YX11" + bloque 0x6518A8. Diagnóstico en el kit (kernel_bridge.c NtCreateFile): al abrir savedata.dat para escribir, "[FILE] escritura de la partida, pila: ..." (direcciones de código del juego). FALTA: que el usuario guarde una vez desde el menú y mande registro.txt; con eso se llama a esa función tras ganar + respaldos en Ajustes\respaldos (últimos 5) + opción "Guardar al ganar".
- Exe 23:15:55 copiado. Sin probar.
- 2026-11-18 23:3x — **Guardar al ganar** (registro del usuario tras guardar desde el menú: "[FILE] escritura de la partida, pila: ..." → la escribe el hilo de archivos, sub_0001B430, modo de escritura [0x631A44] = 5). El guardado del juego, leído: **`sub_0002D830(ranura)`** (la página de guardar la llama por sub_0010E050 con [page+0x48]; ranura < 0 = la actual u32 0x65189C): lee savedata.dat en un búfer de 0x14A00 (sub_0002D630), copia el bloque 0x6518A8 (0x1072 dwords) en ranura*0x41C8+0x1C y lo escribe (sub_0002D670 → sub_0002D230, espera con sub_0002D040 + sub_0011EDB0/sub_0001BF20); al = 1 bien. Lectura: sub_0002D750(ranura). Los premios entran al cofre con sub_000D9720 antes de volver al menú.
  - `src/guardado.c` (nuevo, CMakeLists): estadisticas.c avisa un duelo terminado y ganado (`ygo_guardado_ganado`); en el menú principal (sub_0010D6C0, raton.c → `ygo_guardado_menu(estado)`), con la rueda en estado 0 hace 1 s y sin escena de duelo: copia savedata.dat a YGO_AJUSTES\respaldos\savedata_<fecha>.dat (quedan 5) y llama a sub_0002D830(-1). En torneo espera a volver al menú. F2/Configuración "guardar_ganar" Sí/No (Sí por defecto), 6 idiomas. Registro "[GUARDADO]".
  - Exe 23:35:17 + Configuración.exe copiados. Sin probar.
- 2026-11-18 23:5x — **Círculo de limitadas estilo Master Duel, solo fuera del duelo** (usuario: "como en master Duel ... solo en la biblioteca y cuando te armas el Deck", "que tape el inicio del nombre", "en el duelo nada"; muestra aprobada: aro rojo, centro negro, número amarillo, Arial Black).
  - Kit (nv2a_tex_pack.c/.h): `nv2a_tex_pack_set_variant(on)`; con la variante encendida, `nv2a_tex_pack_find` busca primero `huella ^ NV2A_TEX_PACK_VARIANT_SALT` (0x4C494D4954414441); al cambiar sube la generación del pack (se re-suben las texturas). Registro "[TEXPACK] variant on/off".
  - ygo-apex/src/auto.c (su hilo, cada 33–150 ms): variante = sin escena de duelo ([0x656F88] == 0).
  - tools/cartas_mod.py: los .tga y las hojas de miniaturas del juego quedan SIN círculo (las limitadas del juego ya no se reescriben); la carta HD con círculo va al pack con la huella ^ VARIANTE (cartas.pack: mod + 61 limitadas × 5 idiomas), y las hojas de miniaturas con círculos en cartas_<xx>.pack (las 10 hojas). cartas.pack quedó en 410 MB (el kit acepta hasta 512 MB).
  - Exe 23:54:16 + mod copiados. Sin probar. Fuera del duelo también sale en premios/sobres (todo lo que no es duelo). Con F6 (pack apagado) no se ve círculo en ningún lado.
- 2026-11-19 00:1x — Miniaturas (vista chica de la biblioteca/deck): el círculo salía cortado por el borde de la casilla. Ahora cartas_mod.py dibuja la casilla sin girar, su borde y después el círculo adentro (radio 12 % del ancho, metido tras el borde), y recién ahí la gira. La carta grande no cambia. Mod copiado; sin probar.
- 2026-11-19 01:5x — Pedido del usuario (varios):
  - **Crash al terminar un duelo** (registro: [CRASH] en sub_00013E70 desde sub_00045750 → sub_00044E60 → sub_0004F140 → sub_0004CD10, tras "[MASCOTA] el visor suelta su modelo"): bajar la referencia del nodo de la mascota ANTES de desarmar la escena dejaba objetos de la escena (clase vtable 0x4236E4) leyendo lo liberado. mascota.c: antes de la escena solo se saca el nodo del grupo y se anota; la referencia se baja DESPUÉS de sub_00045750_pad ("[MASCOTA] n: el visor suelta su modelo tras la escena").
  - **Buscador**: botones "Limitadas" (límite del guardado u8 0x665896+k = 1 o 2) y "Agregadas" (cartas del mod, sin las de solo_espanol; solo si el mod cargó alguna: cartas.c ygo_carta_agregada / ygo_cartas_hay_agregadas). Filas: Solo nuevas | Limitadas, Agregadas | Borrar (sin mod: Borrar solo). La lista muestra 4 filas (antes 5) para que entre.
  - **AUTO como la placa del nombre**: las placas son los 2 widgets HUD de la escena (+0x34/+0x38, vtable 0x4294A8); sub_0004F440(mostrar, animar) las entra (x 92) o saca (x -158). auto.c lo envuelve (rename_pads) y oculta AUTO cuando la del jugador ([hud+0x48]==0) sale. Registro "[AUTO] la placa del nombre entra/sale".
  - **Perfil**: el nombre ya no se puede editar (no es zona de clic).
  - Exe 01:47:37 copiado. Sin probar.
  - **Decks de los Oponentes**: maqueta mostrada (28 rivales con retrato cf_anim\small\png\cfsNN.png y nombre del juego; clic → panel del mazo que ya existe en Estadísticas, listas B/A 0x453830/0x453630; clic en carta → carta grande). Esperando OK; nada programado todavía.
- 2026-11-19 04:5x — **Decks de los Oponentes programado** (maqueta aprobada: "me encantooo hacelo al menu"). perfil.c:
  - Ahora son 3 páginas (`NPAGINAS`): 0 Perfil, 1 `PAG_RIVALES`, 2 `PAG_ESTAD`. Las flechas del cartel avanzan/retroceden en ciclo. La mascota solo se ve en la página 0.
  - `pintar_rivales()`: fondo2, título "Decks de los Oponentes", grilla 7×4 de los 28 personajes (`retrato()` + `ygo_estadisticas_nombre`, RG_* en ref px). Clic → `mazo_abrir(ch)` (el mismo panel de Estadísticas).
  - Panel del mazo:
    - `mazo_t.grande` (k de la carta). Clic en una miniatura → carta grande (`carta_img`, 760 de alto, centrada, página más oscura, "Clic para cerrar"). Cualquier clic, clic derecho o Esc la cierra; después Esc/clic derecho cierra el panel.
    - Con el mouse fuera de las cartas: "Hacé clic en una carta para verla en grande".
    - Funciona también desde Estadísticas.
  - Círculo de limitada en el panel: `insignia()` dibujada en C (anillo rojo, centro negro, número amarillo, igual que cartas_mod.py) según `MEM8(0x665896+k)` 1..2. En las miniaturas va arriba a la izquierda; el tilde de "la tenés" pasó abajo a la izquierda. En la carta grande va a (9,9) r 9 sobre 128.
  - Textos nuevos en f2_idiomas.c (6 idiomas).
  - Compilado (exe 04:51). Sin probar.
  - copiar_al_juego.sh ya tarda más de 300 s (cartas_mod con las hojas variante): se corrió sin `timeout`.
- 2026-11-19 05:0x — Capturas del usuario sobre Decks de los Oponentes:
  - **Flechas del cartel encima de la carta grande:** ahora se ocultan mientras hay carta grande (las ventanas `vf[]`).
  - **Carta grande borrosa** (era la jpg de 144×208 agrandada):
    - perfil_cartas.py arma además `<idioma>\g\<ID>.jpg` de 512×740 (la x4 tal cual), solo para las 624 cartas de las listas A/B de los rivales;
    - salida 367 MB en total (antes 80);
    - perfil.c `carta_grande_img()` la usa, una sola en memoria; si falta, usa la chica.
  - **"Duel Computer" sin deck:**
    - en la tabla 0x423D60, el personaje 27 es el duelista 27, sin listas, y 28..30 → 27;
    - los Duel Computer reales son los personajes 31, 32 y 33 (duelistas 29, 30 y 31), y el usuario jugó contra el 31.
    - La página ahora muestra `k_rivales` = 0..26 + 31, 32, 33 (30 casillas, grilla 8×4, retratos de 150×100) con el nombre "Duel Computer 1/2/3" (`nombre_rival`, también en el título del panel).
  - Exe 05:09. Sin probar.
- 2026-11-19 05:3x — **Dibujo de las cartas con remacri-4x** (el usuario: "hay cartas que perdieron muchos detalles al agrandarlas con IA").
  - **Conclusión: remacri-4x funciona bien para ilustraciones.** digital-art-4x (el de todo el pack) deja los bordes limpios pero borra la textura fina: la piedra del Mago oscuro, la piel del Ídolo mil ojos, el musgo de Ryu-Kishin. Remacri la conserva. Comparados 6 modelos en 7 cartas; el usuario eligió remacri.
  - Para futuras ilustraciones o texturas pintadas, probar remacri primero. En fondos con mucho punteado (cielos) puede quedar como papel arrugado: revisarlos.
  - **Cómo:**
    - `tools/dibujos_remacri.py` guarda solo el recuadro del dibujo escalado (x 18..109, y 40..131 del .tga, x4, orientación del volcado) en `modding/texturas/dibujos_remacri/HASH.png` (1189, 257 MB, solo en la carpeta de trabajo).
    - `texto_cartas.abrir_escalada()` lo pega sobre la escalada digital-art con un fundido de 3 px. El marco, el nombre y el texto no cambian.
    - Lo usan texto_cartas.py (español) e idiomas_pack.py (en/fr/de/it), así que los packs tienen las mismas entradas que antes.
  - **Excepción:** `DIBUJO_OTRO` = Sengenjin (4709, huella ED0B4851B3117253) con ultramix-balanced-4x (`dibujos_ultramix/`), a pedido del usuario. Swordsman de Landstar y Hada de la fuente quedan con remacri, por decisión del usuario.
  - Detección de cartas dudosas: ranking del grano que remacri agrega en las zonas lisas del original. Revisadas las 40 primeras.
  - Rearmando `idiomas_pack.py` (5 idiomas); después, copiar_al_juego.sh (mod de cartas y Perfil).
  - 06:5x Terminado y copiado:
    - **Packs:** los 5 de idioma rearmados con el dibujo remacri. Tamaños idénticos a los anteriores (es 979 MB, en/de 949, fr/it 938); enlazados con el juego, así que ya están ahí. El pack grande no cambia (no tiene cartas).
    - **Mod de cartas y Perfil:** también rehechos, incluidas las imágenes de la carta en grande.
    - **copiar_al_juego.sh:** ahora borra del juego las imágenes del Perfil que perfil_cartas.py ya no genera (16: 5030, 5031, 5162, 5318, 5406 en en/fr/gr/it; imágenes que no están en la lista de cartas). Antes, el diff frenaba la copia.
    - **Intermedios borrados del scratchpad:** escaladas completas remacri/ultramix y pruebas.
    - **Quedan en la carpeta de trabajo:** dibujos_remacri/ (257 MB) y dibujos_ultramix/ (1 carta).
    - Sin probar en el juego.
- 2026-11-19 06:5x — **El crash al terminar el duelo siguió** (registro 06:53: el mismo [CRASH] en sub_00013E70 desde sub_00045750 → 44E91 → 4F155 → 4CD54; nodo 0x89CE8BC0 inválido). Lo único que pasaba antes del desarme eran los dos "modelo soltado" (sub_00019700, los modelos de las mascotas).
  - Antes del arreglo de memoria los modelos se soltaban al crear la escena siguiente (crear → soltar_todo) y nunca hubo crash. Soltados antes de sub_00045750, la escena todavía los usaba.
  - **Arreglo (mascota.c):** antes de la escena solo `soltar_visores()` y apagar el dibujo; después de `sub_00045750_pad`, `soltar_nodos()` + `soltar_todo()`. La memoria se sigue liberando al terminar cada duelo.
  - Exe 06:55. Sin probar.
- 2026-11-19 07:0x — **Crash al terminar el duelo, otra vez** (registro 07:02, PRIMER duelo de la sesión, contra Duel Computer 31): [CRASH] en sub_00013E70 desde 4CD54 ← 4F155 ← 44E91 ← 45755, con `this` = 0x186CA0 (dirección de código) leído de un arreglo de la escena (sub_0004F140 sobre [escena+0x30]) que tenía basura. Al ser el primer duelo, nada de lo que corre DESPUÉS del desarme pudo causarlo: lo único de la mascota que corría ANTES era `soltar_visores()` (sacar el nodo del grupo del visor con sub_002345A0 y [v+0x34]=0, escrituras en memoria de la escena). Antes del arreglo de memoria (11-18 22:26) no se tocaba nada ahí y nunca hubo crash (el registro 20:02, 29 min de duelos con RECOMP_HEAP_RECLAIM, sin crash).
  - **Arreglo (mascota.c):** antes del desarme la mascota SOLO LEE (`anotar_nodos`: nodo, vtable y referencias) y no escribe nada en la escena. Después de `sub_00045750_pad`, `soltar_nodos` comprueba que el nodo siga igual (misma vtable y mismas referencias, ≥ 2) y le baja las dos referencias de la mascota (grupo + visor; visor y grupo quedan abandonados con la escena), destruyéndolo en 0 como sub_0005A670; si algo cambió no lo toca ("[MASCOTA] n: nodo ... cambio tras la escena: no se toca"). Luego `soltar_todo()` (manijas).
  - Exe 07:06:59 copiado (solo el exe; cmp idéntico). Sin probar. En el registro debe verse "el visor suelta su modelo tras la escena (... 2 -> 0 referencias)" o "no se toca", y ningún [CRASH].
- 2026-11-19 07:1x — **Prueba del usuario: el crash del fin del duelo ARREGLADO** (registro 07:14: tras la escena "el visor suelta su modelo ... 3 -> 1 referencias" en las dos y "modelo soltado" x2, sin [CRASH]).
  - **Guardar al ganar no guardó:** la rueda del menú estuvo en estado 0 solo 563 ms (esperaba 1 s), el usuario fue al título y el juego recargó la partida (229 cambios del cofre): se perdió esa victoria. guardado.c: espera 150 ms. Exe 07:15:18 copiado. Sin probar.
- 2026-11-19 07:1x — **Guardar al terminar cualquier duelo** (el usuario: "que se guarde cuando gane el duelo o pierda? o empate? ... a veces te jugas 2 duelos seguidos sin pasar por el menú"). guardado.c: estadisticas.c avisa todo duelo terminado (`ygo_guardado_terminado(gano)`); se guarda (sub_0002D830(-1), respaldo una vez por duelo) **al cerrar la escena del duelo** (mascota.c, después de sub_00045750_pad), **con cada premio** si se ganó (cartas.c, después de sub_000D9720: los premios entran al cofre en la pantalla de premios, después de la escena) y en el menú si quedó algo pendiente (150 ms en estado 0). Registro "[GUARDADO] partida guardada al cerrar el duelo / con el premio / en el menu". Exe 07:18:04 copiado. Sin probar.
  - Visto en la captura y el registro 07:14: con AUTO encendido igual sale la caja "Tu adversario Colocó un monstruo. ¿Deseas activar...?" (15 cajas de 2 líneas después de encenderlo): AUTO solo apaga sub_000A4EC0, esta pregunta va por otro camino. Pendiente.
- 2026-11-19 07:2x — **AUTO** (el usuario, captura: AUTO encendido y aun así "Tu adversario Colocó un monstruo. ¿Deseas activar una Carta Mágica de Juego Rápido o de Trampa?"; y la placa AUTO "cuando un monstruo ataca o lo ves de cerca sigue apareciendo").
  - La caja es `sub_00082280(act, p)` (texto 0xC3E/0xC3F + sub_000880B0; máquina propia en u8 0x66C078, que quien llama pone en 0). Envuelta (rename_pads, auto.c): con AUTO, p = 0 y la máquina en 0 → devuelve ax 1 y bit 7 de 0x66C079 limpio (la salida "nada que encadenar" del juego, 0x8275F), sin caja. Registro "[AUTO] pregunta de cadena salteada (desde ...)".
  - Placa: el registro 07:14 no tiene ningún "la placa del nombre sale" (en los ataques el juego no saca las placas con sub_0004F440). mascota.c anota la hora de cada cuadro con un visor de batalla en pantalla (sub_00056FC0, la misma condición que danio.c) en `g_ygo_batalla_t`; auto.c oculta AUTO hasta 250 ms después.
  - Exe 07:24:20 copiado. Sin probar.
- 2026-11-19 07:3x — Prueba del usuario: **guardado al terminar el duelo FUNCIONA** (registro 07:27: "partida guardada al cerrar el duelo: bien" + 3 "con el premio: bien", respaldo en Ajustes\respaldos; sin crash). AUTO seguía visible (y brillante) sobre el panel "Información de la carta" con el campo oscurecido: raton.c (envoltorio de sub_00080D40, botones redondos) anota la hora mientras el paso [0x65F8FC] está en 1..0xA (botones o información) en `g_ygo_botones_t`; auto.c oculta AUTO hasta 150 ms después. Exe 07:31:36 copiado. Sin probar (tampoco el salto de la pregunta de cadena: en ese duelo no hubo ninguna).
- 2026-11-19 07:3x — **Revisión para sesiones largas** (el usuario: "analizar todo para detectar algún error al jugar 30 min"). No hay registro largo para medir (cada arranque pisa registro.txt). Revisado a mano lo de hoy:
  - Fin del duelo (mascota.c): antes del desarme solo lee; después baja 2 referencias solo si el nodo sigue igual; probado (3 -> 1, sin crash).
  - Guardado (guardado.c): al cerrar la escena y con cada premio, en el hilo del juego; probado ("bien" x4). Perder/empatar: mismo camino, sin probar. Salir a mitad de duelo no marca pendiente.
  - AUTO (sub_00082280): devuelve lo mismo que la salida "nada que encadenar" del juego; los dos llamadores siguen su camino normal de "No" (0x82E8A / 0x82CF1). Sin probar.
  - Nuevo: el kit expone `xbox_ContiguousUsageKB` (xbox_memory_layout.c) y al final de cada duelo el registro anota "[MEMORIA] fin del duelo N: texturas en uso X KB (pico Y KB, el máximo es 65408)" para ver si la memoria de texturas sube duelo a duelo. main.c copia registro.txt a **registro_anterior.txt** al arrancar (el registro de un cierre ya no se pierde al volver a abrir).
  - Exe 07:35:38 copiado. Pendiente: una sesión de ~30 min y mirar las líneas [MEMORIA].

- 2026-11-19 14:2x — Usuario (captura, registro 14:15): AUTO solo se podía apretar en su turno; desapareció con la mano abierta; con AUTO y una Trampa colocada igual preguntaba "¿activar?" ~4 veces por turno; en los ataques AUTO desaparece en vez de oscurecerse como el nombre; premio Sangan (limitada a 1) estando ya en el cofre. Registro: ni una línea "[AUTO] pregunta de cadena salteada" en todo el duelo → la caja no era la de sub_00082280.
  - **Las cajas (textos leídos del xbe, tabla de tools/dialogos.py):** "Tu adversario Colocó un monstruo." + 0xD01 "¿Deseas activar una Carta Mágica de Juego Rápido o de Trampa?" la arma sub_000BFAD0 y la muestran dos máquinas: `sub_000BFE50` (desde sub_000C0770; su paso 0 pregunta `sub_000BF9C0(p)` si p tiene con qué responder: Mágicas/Trampas y mano) y `sub_000C07F0` (desde sub_000A1BF0, bandera 8 de 0x66C494; paso = u8 0x66C07A>>1; con bit 0 de 0x66C07B muestra 0xC3E/0xC3F "El efecto %s está activado...").
  - **Arreglo (auto.c):** `sub_000BF9C0` envuelta (rename_pads: agregada): con AUTO y p 0 devuelve 0 = la salida "no tenés nada" del juego (0xC039E, máquina 0xF0, sin caja). `sub_000C07F0` (antes paso libre en cadena.c, ahora en auto.c): con AUTO en su paso 0 toma la salida "No" del juego (0xC093F: sub_00085720(0xF055,0,0,0), ax 1), salvo su propia espera (0x65F8F8 bit 2 y paso de llamada 0x1E/0x1F/0x28/0x29/0x2A). Registro "[AUTO] pregunta de Juego Rapido o Trampa salteada" / "pregunta de activar salteada (...)". Siguen los de sub_00082280 y sub_000A4EC0.
  - **Clic en AUTO en cualquier turno (raton.c):** antes lo leía field_pointer, que solo corre con el campo en manos del jugador; ahora se lee antes de ask_pointer (todas las pantallas del duelo), quitando la A del kit.
  - **Visible:** se oculta solo con el panel de información de la carta (paso 0xA de [0x65F8FC]); antes también con los botones redondos (1..9) → desaparecía con la mano abierta. En batalla (visor 3D en pantalla, g_ygo_batalla_t) ya no se oculta: se dibuja oscurecido (color x0,25, misma forma).
  - **Premios (premios_extra.c tope_k):** el tope es el menor entre el byte de la partida (u8 0x665896+k) y el límite de la carta (`ygo_limite_carta`, cartas.c: el del mod y si no la lista del juego, 61 u16 en 0x450BC0; Sangan 4054 = 1). En el registro: Sangan salió en la ranura 0 con 1 en el cofre y otra vez en la 2 → su byte en la partida no era 1.
  - Compilado y copiado (exe idéntico 14:27:16). Sin probar.

- 2026-11-19 16:1x — Usuario (registro 16:13): AUTO funcionó (40+ "[AUTO] pregunta de Juego Rapido o Trampa salteada", todas desde 0xBFF3F). Pero: "ME DIO AL SANGANO DE NUEVO"; y con Ganga de arrancada (k 792, imagen 4898) le robó Guerrero de pizarra (k 906) y la carta siguió dibujada boca arriba en la casilla del rival; un monstruo que el rival colocó ahí después no se vio.
  - **Sangan:** el tope ya era 1, pero los sorteos del rival 28 dieron 600 veces cartas que ya tenía hasta el límite y quedaba el último (Sangan, ranuras 0 y 2). Ahora (premios_extra.c `otra_carta`): si tras todos los sorteos la carta sigue en su límite, se da otra de las listas de premios del rival (las dos, la variante actual, como da_carta) por debajo de su límite, por los pesos de la lista (bits 0-12); si no hay ninguna, queda la sorteada. Registro "[PREMIOS] ranura N: tras N sorteos seguía la ... se da otra de la lista del rival, la ...". El texto del registro ahora dice "ya tenías N (límite M)".
  - **Ganga de arrancada:** sin causa encontrada en el código (no es carta del mod; ningún envoltorio nuestro toca el cambio de control). Diagnóstico (cartas.c, en ygo_cofre_vigilar, 10 por segundo): "[CAMPO] jP casilla Z: k .. (imagen ..): 16 bytes" en cada cambio de las 10 casillas de monstruos durante el duelo, para ver si el juego deja la carta en la casilla del rival o es solo el dibujo.
  - Compilado y copiado (16:19:34). Sin probar.

- 2026-11-19 16:2x — Usuario: "¿esto no ralentiza nada?" y "el cartel de Auto se apaga pero se sigue viendo por arriba cuando atacas". Su captura (elegir ataque): AUTO oscuro con "Dennis" brillante → la señal del visor de batalla (g_ygo_batalla_t) se prendía fuera del momento en que el juego oscurece.
  - **Arreglo:** AUTO copia el brillo de la placa del nombre leído del propio cuadro del juego. Kit (xboxrecomp-apex nv2a_pb_d3d11.c `muestra_luz`, antes del blit del Present): `g_nv2a_muestra_caja` (caja en px del juego) → `g_nv2a_muestra_luz` (brillo medio 0..1), con 2 texturas staging que se turnan y Map sin esperar (sin trabar la GPU); solo cuadros RGBA/BGRA de 8 bits sin MSAA (si no, "[MUESTRA] frame format ..." una vez). auto.c `brillo_placa`: caja 45..135 x 246..264 (adentro de la placa, medida en las capturas 1919x1079), referencia = lo más brillante del duelo (saltos > 1,25x no cuentan: un monstruo encima), AUTO = color x (luz/ref) en 20 pasos. Solo se mide con AUTO en pantalla.
  - Límite: AUTO es una ventana encima del juego; si un modelo 3D pasa por encima de su lugar, AUTO queda delante (para quedar detrás habría que dibujarlo dentro del cuadro del juego).
  - Costo: la medición copia unos 270x54 px por cuadro; el vigilante de casillas lee 160 bytes 10 veces por segundo; otra_carta solo al dar premios. Despreciable.
  - Compilado y copiado (16:24:19). Sin probar.

- 2026-11-19 16:3x — Usuario (con ironía): "me tocaron 3 sanganos ... que está limitado 1". Registro 16:30: "límite 0" en TODAS las cartas (también en las que nunca tuvo): en la pantalla de premios el byte de la partida (u8 0x665896+k) vale 0 para todas → cada sorteo fallaba y quedaba el último (Sangan: llegó a 4 copias). Pasaba desde el cambio del 11-18 (registro 14:15: una carta con 0 copias recién tras 600 sorteos). Arreglo: `tope_k` ya no usa ese byte; solo el límite de la carta (`ygo_limite_carta`: mod, lista del juego, 3). Compilado y copiado (16:32:13). Sin probar. Las copias de más de Sangan siguen en el cofre (no se tocaron).
- 16:3x — Usuario: "Sí, dejá 1". cartas.c ygo_cartas_limites: Sangan (k 48) baja en el cofre a 1 (o a las copias del mazo, si son más), registro "[CARTAS] Sangan: N copias en el cofre ..."; queda al guardar. Compilado y copiado (16:33:20). Sin probar.
- 16:3x — Comprobado sin jugar (script con default.xbe y savedata.dat 16:30): el cofre tiene 1 Sangan. El propio juego (sub_000D9720) solo suma una copia si el cofre está por debajo de sub_0007EF00(id) → los Sangan "de más" se mostraron como premio pero nunca entraron (premios perdidos; el registro 16:30 no tiene ningún cambio del cofre tras los premios). Quitado el recorte de Sangan en ygo_cartas_limites (innecesario). Con el tope real, de las 50/58 entradas de las listas del rival 28 (variante 0/1), 31/42 están por debajo del límite → el filtro las encuentra. El tope vale para TODAS las cartas (no solo Sangan): límite 1 o 2 de la lista del juego o del mod, 3 si no tiene. Copiado (16:35:11). Sin probar.
- 16:42 — Probado por el usuario: premios con límites reales (3; 1 para la 4342), 2 a 4 sorteos de más en vez de 600; el cofre recibió 3 copias de 4037 (0 -> 3, dentro del límite). Guardado al cerrar el duelo y con cada premio: bien. Memoria al fin del duelo 15884 KB (pico 24700). La Ganga no se jugó: [CAMPO] sigue sin dato.
- 16:4x — Usuario: "me tocó una nueva pero a 3 copias ... la cosa es que te ganes cartas variadas". premios_extra.c `mala(k, rep)`: una carta se rechaza si está en su límite O si ya salió en este mismo premio; otra_carta primero busca no repetidas y solo si no hay ninguna acepta repetir. Copiado. Sin probar.

- 2026-11-19 16:5x — Usuario (capturas): AUTO encima del monstruo en el ataque de cerca, y más oscuro que "Dennis" fuera del ataque (la medición de brillo se confundía con el panel ATK/DEF de arriba). "más te vale que sea la definitiva".
  - **AUTO dibujado dentro del cuadro del juego (kit nv2a_pb_d3d11.c `auto_placa`, llamado tras cada dibujo).** Ctrl+F9 del usuario (flip 6322): la placa del nombre del jugador es un lote 2D de 5 vértices, tex0 hash 82D3B7DD2E4DECF3 (atlas del HUD 1024x512), pos 24.5..158.5 x 239..276, uv 0..0.2930 x 0..0.2559. Justo después de dibujarla, el kit repite el MISMO lote (vértices, shaders, mezcla, tinte del juego) con la textura 0 cambiada por la imagen de AUTO (puesta en esa esquina uv de una textura 1024x512) y el viewport movido/escalado para que la placa caiga en el lugar de AUTO (x+3.5, y+36, 74.7x24.9 px del juego; mapeo con nv2a_aspect_box). Así se oscurece con el nombre, se desliza con él y lo tapa lo que el juego dibuja después (los monstruos). Filtro: hash, tamaño 120..150 x 30..45, mitad izquierda, esquina uv. Registro "[AUTO-KIT] ...".
  - auto.c: sin ventana propia ni medición de brillo; entrega las dos imágenes (RGBA recta), on/off y "duelo en juego" (`g_nv2a_auto`); el clic usa el rectángulo que el kit dibujó (últimos 300 ms). `muestra_luz` del kit queda sin uso (caja en 0).
  - Compilado y copiado (17:00:21). Sin probar.
- 2026-11-19 18:1x — Usuario (captura): debajo de "Dennis" sale una barra naranja sin el texto AUTO. Registro: el kit SÍ dibujó AUTO ("[AUTO-KIT] first AUTO drawn ... AUTO host 112,1100-412,1200", clics "[AUTO] encendido/apagado (clic en la placa)" funcionan).
  - Causa probable: el atlas del HUD se guarda al revés (el volcado 82D3B7DD2E4DECF3 hay que darlo vuelta para verlo derecho), así que la imagen de AUTO puesta derecha en la esquina uv salía invertida/estirada.
  - **Arreglo (kit `auto_placa`):** la orientación se toma de los vértices de la placa (u del vértice de más a la izquierda, v del de más arriba); si traen u o v altos, la imagen se da vuelta en ese eje. Registro una vez: "[AUTO-KIT] plate vertices (flip u N, flip v N): (x,y uv u,v)...", y "picture N made (..., flip N)".
  - Evidencia extra: "[AUTO-KIT] drawn over AUTO later in flip ..." (hasta 12 líneas, primeros ~10 min): lotes 2D que el juego dibuja después encima del lugar de AUTO en el mismo cuadro.
  - Compilado y copiado (exe 18:16:06, cmp idéntico). Sin probar.
- 2026-11-19 18:3x — Usuario (captura): sigue la barra naranja sin texto ("¿el arreglo me lo imagino?"). Registro: "plate vertices (24.5,239 uv 0.1621,0.2559) (24.5,276 uv 0.1621,0.1836) (158.5,239 uv 0.2930,0.2559) (158.5,276 uv 0.2930,0.1836) (158.5,276 uv 0,0)". La placa NO empieza en uv 0: ocupa u 0.1621..0.2930, v 0.1836..0.2559 (v al revés), y el 5º vértice repite una esquina con uv 0. Yo llenaba la imagen desde uv 0 y medía la orientación con la mitad del máximo → solo se veía una tira del borde estirada.
  - **Arreglo (kit `auto_placa`):** uv de la esquina arriba-izquierda y abajo-derecha (primer vértice en cada esquina); la imagen de AUTO llena exactamente ese rectángulo (texels 166..300 x 94..131), dada vuelta según el orden de esas uv, con 4 texels de borde repetido. Registro "picture N made (... uv u0..u1 x v0..v1 = texels ..., flip N)". Exe copiado (cmp idéntico). Sin probar.
- 2026-11-19 18:4x — **"Nueva" sobre cada carta del premio** (el usuario: con 2 cartas nuevas el cartel grande mostraba una y después la otra, no se entendía, y seguía un rato al salir; "que aparezca arriba de cada carta"). Ctrl+F9 del usuario (registro 18:34, flip 56498): la pantalla del premio dibuja cada carta como lote 2D de 5 vértices con su cara 128x256; con 3 cartas: x 85.5/255.5/425.5, 128 px de ancho, y 75..260. Sus huellas (81A7698D3BD06ADF, 03576E130DB679F5, D5B2CD946F5FBB23) = cv.huella de 4008/4047/4013.tga (comprobado).
  - `tools/cartas_mod.py`: escribe `salida/cartas/huellas.txt` ("k HUELLA" por carta; la del mod si la reemplaza) → Mods-Extras\cartas\huellas.txt (copiar_al_juego.sh ya copia esa carpeta).
  - Kit (nv2a_pb_d3d11.c): `g_nv2a_marca` (hasta 16 huellas) + `marca_vista()` tras cada dibujo: lote 2D de 3..6 vértices con esa textura → su rectángulo (px del juego) y la hora.
  - `premios_extra.c`: `huellas_cargar()`; en `sub_0008CBB0` cada carta que no tenías va a la lista (un premio nuevo la vacía); `marcas_premio_tick()` cada 50 ms: las vistas en los últimos 200 ms llevan la pastilla "Nueva" (6 idiomas, `ygo_cartel_marcas`, la de la Biblioteca) en su borde de arriba; sin ninguna a la vista se van en 250 ms y a los 3 s la lista se vacía. **Se sacó el cartel grande "¡Carta nueva!"** (queda la línea del registro y la marca de la Biblioteca). Registro "[PREMIOS] huellas de N cartas leídas", "[PREMIOS] marca Nueva sobre N carta(s) del premio".
- 2026-11-19 18:4x — Fundas y tapetes de las 38 mascotas sin arte propio (muestras, NO puestas en el juego): números por la API de ygoprodeck, artes 624x624 de images.ygoprodeck.com/images/cards_cropped/ (OK del usuario: "descargarla como hiciste antes"), guardadas en modding/texturas/fundas/arte_mascotas/mNN.jpg (NN = índice de mascota; lista.txt = índice y número de carta). Tapetes más claros (fondo 0.85 en vez de 0.6, piso de casillas 0.8, x1.2). Esperando OK del usuario.
- 2026-11-19 18:5x — **REGLA (pedido del usuario): cada mascota nueva lleva su combo completo: funda y tapete con el arte de su carta.** Si se agrega una mascota (mascota.c k_modelo + main.c k_mascota_tuya), también su arte (images.ygoprodeck.com/images/cards_cropped/<número>.jpg, 624x624, con OK del usuario para bajar) en fundas.py COMO / tapetes.py ARTES / fundas.c k_arte (+ k_por_mascota) y la fila de F2/Configuración.
  - Encuadres de funda revisados (el usuario: "centrar o correr un poco al personaje"): B. Skull Dragon (m29) corte 0.35, Dragón Meteoro (m50) 0.25, Dragón Toon de Ojos Azules (m33) 0.30, Mil Dragones (m46) 0.35. Tapetes aprobados ("los tapetes están bien", la versión más clara). Falta el OK de las 4 fundas para meter las 38 en el juego.
  - Copiado con copiar_al_juego.sh (exe 18:39:52 idéntico; Mods-Extras\cartas\huellas.txt con 1137 cartas, k 2/7/41 = las huellas del volcado). Sin probar.
  - 19:0x (usuario): B. Skull Dragon más a la derecha → corte 0.62; Meteor B. Dragon (m31, fusión) → corte 0.30. Los otros 3 aprobados.
- 2026-11-19 18:5x — **Las 38 fundas y tapetes nuevos al juego** (el usuario: "Sí, metelas"). Artes en modding/texturas/fundas/arte/<nombre>.jpg (copias de arte_mascotas/mNN.jpg). `tools/fundas.py` COMO (+38; encuadres: dragon_craneo_negro 0.62, dragon_meteoro_negro 0.30, dragon_toon 0.30, mil_dragones 0.35, dragon_meteoro 0.25, el resto 0.5). `tools/tapetes.py`: NUEVAS (38) más claras (fondo 0.85, piso 0.8, x1.2 = LUZ_NUEVAS); los 31 de antes iguales. `fundas.c` k_arte 32..69 y k_por_mascota (cada mascota su arte: el reverso del rival también); `ygo_arte_archivo/ygo_arte_n` para perfil.c (su lista fija de 32 tapetes se sacó). main.c k_funda/k_tapete y Configuracion.cs (funda, tapete) +38 nombres (los de las mascotas, ya traducidos en los 6 idiomas). Configuración.exe 18:50:10.
  - Copiado (copiar_al_juego.sh: exe 18:50:01 idéntico, 69 fundas x 5 marcos y 69 tapetes en Mods-Extras; Configuración.exe con cmp). Sin probar.
- 2026-11-19 19:0x — **Mago Oscuro II con funda y tapete propios** (el usuario: "me gustó la versión del medio"). Su carta del juego (5165) = ygoprodeck 36996508 (comparado con las 9 ilustraciones de Mago Oscuro); arte mago_oscuro_2.jpg, corte 0.45, tapete claro; fundas.c k_arte[70], k_por_mascota[3] = 70; main.c y Configuracion.cs "Mago Oscuro II". Las otras 7 ilustraciones alternativas guardadas para el futuro en modding/texturas/fundas/arte_alternativas/. Exe 19:05:44 y Configuración copiados (cmp). Sin probar.
- 2026-11-19 19:1x — **Mascotas a 85** (el usuario elige): + Fuerza de Ataque Goblin (5145, m924), Elfas Gemelas (4538, m531), Santa Juana (5208, m906), Zoa (4397, m390), Ryu-Ran (4732, m725), Bickuribox (4391, m384), Elfo Oscuro (4557, m550), Tortuga Cangrejo (4716, m709), Espada de Garma (4722, m715), Escarabajo Jabalina (4723, m716), Tanque Laberinto (4376, m369), Ballena Fortaleza (4724, m717), Dragón Nocturno Serpiente (4712, m705), Chakra (4715, m708), Fiend Megacyber (4761, m907). Todas con modelo en Game\monster. Artes bajadas (ygoprodeck) en arte_mascotas/<nombre>.jpg (lista.txt). Muestras de funda y tapete mostradas; esperando OK para meterlas. "El último guerrero de otro planeta" (4785) está en el juego pero sin modelo 3D: no puede ser mascota salvo que se importe un modelo.
  - 19:2x: tapetes de las 15 aprobados; encuadres de funda: zoa 0.30, dragon_nocturno 0.25 (el resto 0.5). Modelos conocidos libres (mismo ATK/DEF, con .igb): Man-Eater Bug 4507 m500, Cyber Jar 4913 m911, Airknight Parshath 5023 m762, Neo 4881 m732, Penguin Soldier 4608 m601, Morphing Jar 4597 m590, Muka Muka 4522 m515, Jinzo #7 4428 m421, Giant Soldier of Stone 4080 m73, Labyrinth Wall 4372 m365, Armed Ninja 4475 m468, Petit Dragon 4206 m199, Witch's Apprentice 4634 m627, Gilasaurus 5256 m933, Gradius 5140 m954, Leogun 4409 m402, Lava Battleguard 4560 m553, Swordsman of Landstar 4759 m941; y Sengenjin m702, Kaiser Dragon m426, Seiyaryu m357, Cyber-Tech Alligator m943.
- 2026-11-19 19:3x — **Mascotas a 93** (el usuario: de los conocidos, todos menos Neo y Jinzo #7): + Bicho Devorador de Hombres (4507, m500), Jarra Cibernética (4913, m911), Parshath Caballero Aéreo (5023, m762), Soldado Pingüino (4608, m601), Jarra Cambiante (4597, m590), Muka Muka (4522, m515), Soldado Gigante de Piedra (4080, m73), Muro Laberinto (4372, m365). Artes bajadas a arte_mascotas/<nombre>.jpg; muestras mostradas, esperando OK.
  - Jarra Cibernética: la funda cortada no la mostraba completa (el usuario) → modo "entera" (la imagen cuadrada entera sobre su copia desenfocada).
- 2026-11-19 19:4x — Jarra Cibernética: corte 0.4 (el usuario, no "entera"). **Mascotas a 97**: + Ninja Armado (4475, m468), Aprendiz de Bruja (4634, m627), Gilasaurus (5256, m933), Battleguard de Lava (4560, m553). Artes en arte_mascotas; muestras mostradas. Faltan 3 para 100 (propuestas: Petit Dragón m199, Gradius m954, Seiyaryu m357; repuesto Leogun m402, Espadachín de Landstar m941).
- 2026-11-19 19:5x — **99**: + Seiyaryu (4364, m357, corte 0.3) y Guerrero Pantera (4751, m746, corte 0.6). Artes en arte_mascotas; muestras mostradas. Falta 1 para 100 (propuestos: Hombre Juez 4039 m32, Araña Lanzadora 4396 m389, Kaiser Rudo 4388 m381, Máquina Tragamonedas 4729 m722, Espada de Cocodrilo 4767 m942, Cofre Devorahombres 4883 m734, Caimán Toon 4387 m380).
- 2026-11-19 20:0x — **100**: + Jirai Gumo (4373, m366, ygoprodeck 94773007, corte 0.5). Muestra mostrada; esperando OK para meter las 30 nuevas (70 → 100).
- 2026-11-19 20:xx — **100 mascotas, cada una con su combo (mascota + funda + tapete + animación del Perfil)**. Las 30 nuevas (orden 70..99 en mascota.c k_modelo): Goblin 924, Elfas Gemelas 531, Santa Juana 906, Zoa 390, Ryu-Ran 725, Bickuribox 384, Elfa Oscura 550, Tortuga Cangrejo 709, Espada de Garma 715, Escarabajo Jabalina 716, Tanque Laberinto 369, Ballena Fortaleza 717, Dragón Nocturno Serpiente 705, Chakra 708, Fiend Megacyber 907, Bicho Devorador 500, Jarra Cibernética 911, Parshath 762, Soldado Pingüino 601, Jarra Cambiante 590, Muka Muka 515, Soldado Gigante de Piedra 73, Muro Laberinto 365, Ninja Armado 468, Aprendiz de Bruja 627, Gilasaurus 933, Battleguard de Lava 553, Seiyaryu 357, Guerrero Pantera 746, Jirai Gumo 366.
  - Código: mascota.c NMODELOS 100; perfil.c NMASCOTAS 100 y NFUNDAS = ygo_arte_n() (antes 32 fijo: las flechas de funda del Perfil no llegaban a las nuevas); fundas.c k_arte 71..100, k_por_mascota[100] (70..99 → 71..100); main.c k_mascota_tuya/k_funda/k_tapete; f2_idiomas.c y Configuracion.cs: 30 nombres en 6 idiomas (script scratchpad agregar30.py).
  - Fundas/tapetes: fundas.py COMO (+30; zoa 0.3, dragon_nocturno 0.25, jarra_cibernetica 0.4, seiyaryu 0.3, guerrero_pantera 0.6), tapetes.py NUEVAS (+30, claros). 100 artes x 5 marcos, 100 tapetes.
  - Perfil: grabadas con el visor (visor.py --sin-navegador, /grabar?m=... y ?vista=frente, en el navegador de la app; 30 de 30 cada una); camaras.txt: 25 de frente (cámara A: Tanque Laberinto, Jarra Cibernética, Jarra Cambiante, Muro Laberinto, Seiyaryu); perfil.py armó las 100 tiras (193 MB).
  - Copiado (copiar_al_juego.sh: exe 19:34:11 idéntico; 100 mascotas del Perfil, 100 fundas, 100 tapetes en Mods-Extras; Configuración.exe con cmp). Sin probar en el juego.
- 2026-11-19 20:3x — Perfil sin fundas nuevas (captura del usuario: Gaia sin imagen): tools/perfil_recursos.py fundas() tenía su propia lista fija de 31 artes; ahora lee k_arte de fundas.c. Generadas perfil/juego/fundas (00_0 + 100 x 5 = 501 jpg); falta copiar. Rediseño pedido (colores del marco entre funda y nombre, sin "Marco"; número "n / 101" abajo como la mascota): maqueta mostrada, esperando OK.
- 2026-11-19 20:4x — Perfil, funda (el usuario aprobó la maqueta: "Clásica como la 0", colores "un poquito más chicos"): perfil.c PUNTO_Y 721 (entre la funda y su placa; era 840), PUNTO_R 12 (era 16; +2 con el mouse), sin la palabra "Marco"; en y 820 "fu / (NFUNDAS-1)" como el número de la mascota (Clásica = 0, de 0 a 100).
  - Copiado (exe 19:49:39 idéntico; Mods-Extras\perfil\fundas 501 imágenes). Sin probar.
- 2026-11-19 20:5x — Pedido (captura): mismo número para mascota, funda y tapete; Perfil: tapete como mascota/funda (placa con nombre, flechas, número), nombre del jugador arriba de todo, "Cartas" y barra abajo dentro del recuadro. Comprobado: cada una de las 100 mascotas tiene un arte propio y distinto (Dragón Definitivo, la 28, sin unir en k_por_mascota: su arte es "definitivo"), así que k_arte puede ir en el orden de las mascotas (funda/tapete N = mascota N, Clásica 0). Maqueta mostrada, esperando OK.
- 2026-11-19 21:0x — **Perfil rediseñado + mismo número para mascota, funda y tapete + flechas D animadas** (el usuario aprobó la maqueta y eligió la opción D de flechas; la C quedó guardada en modding/texturas/perfil/ideas/ para otro menú).
  - Orden: fundas.c k_arte ahora en el orden de las mascotas (k_arte[i+1] = arte de la mascota i; scratchpad reordenar.py); k_por_mascota = i+1; RIVAL_ARTE 4 → 7 (ojos_rojos). main.c k_funda/k_tapete y Configuracion.cs = "Clásica"/"Clásico" + los nombres de las mascotas. Las fundas/tapetes elegidos antes pueden haber cambiado de dibujo (el índice guardado es el mismo).
  - perfil_recursos.py: FOTO (305, 420, 330, 240), placa del nombre arriba sobre el borde (960, 220), placa del tapete (470, 750, 240, 56), sin el distintivo de la esquina ni flechas en el fondo; flecha_d(): medallón (aro dorado con 8 muescas, centro verde azulado, flecha dorada), tiras de 12 cuadros (el aro gira una muesca), normal y _luz → flecha_d_izq/der(_luz).png.
  - perfil.c: k_flecha junto a cada placa (mascota 766/1154, funda 1296/1604, tapete 316/624; y 778), clic radio 32; cada flecha en su ventana (vfd[6], como las de página): quieta el cuadro 0; con el mouse encima iluminada, el aro girando (40 ms por cuadro), vaivén hacia afuera y 10 % más grande; al hacer clic empuje 220 ms. Tapete: nombre en su placa y "n / 100"; "Cartas" y barra abajo al centro (868 / 922, barra 440). perfil_recursos fundas() regenerado con el orden nuevo.
  - Copiado (exe 20:09:43 idéntico, Configuración con cmp, recursos del Perfil y fundas en el orden nuevo). Sin probar.
- 2026-11-19 20:2x — Usuario (captura): las flechas D no se veían. Causa: las 6 ventanas vfd no tenían su DC (CreateCompatibleDC), como pasó antes con las de página: no dibujaban nada. Arreglado en perfil_thread. Pedido: clic sostenido sobre una flecha (mascota/funda/tapete) sigue avanzando: ygo_perfil_raton repite el clic con el botón izquierdo apretado (GetAsyncKeyState) sobre la misma flecha: a los 400 ms, cada 110 ms, desde 1,5 s cada 60 ms. Exe 20:19:23 copiado (cmp). Sin probar.
- 2026-11-19 20:3x — Perfil, encuadre de 6 mascotas (capturas del usuario): tools/perfil.py AJUSTE por número de modelo (tamaño, derecha, abajo): Soldado Pingüino 601 y Jarra Cambiante 590 al 75 %, Muro Laberinto 365 subido 8 %, Seiyaryu 357 al 120 % y subido 12 % ("como que vuela"), Zoa 390 al 120 %, Fiend Megacyber 907 corrido 6 % a la izquierda. Tiras regeneradas (mascotas 88, 89, 92, 97, 73, 84) y copiadas a Mods-Extras\perfil\mascotas. Sin probar.
  - CORRECCIÓN 20:3x: las capturas eran de la mascota EN EL DUELO (junto a la placa del nombre/AUTO), no del Perfil. perfil.py AJUSTE quedó vacío y las tiras del Perfil regeneradas como antes. El ajuste va en mascota.c k_retoque: Soldado Pingüino 601 esc 0.75; Jarra Cambiante 590 esc 0.75 arriba 12; Muro Laberinto 365 arriba 16 (por su extensión); Seiyaryu 357 esc 1.45 DRAGON arriba 16 (por su extensión, "como que vuela"); Zoa 390 esc 1.3 MEDIANO; Fiend Megacyber 907 dx 10 (a la izquierda).
  - Perfil: cambiar funda/marco/tapete desde F2 no se veía en el Perfil abierto (el panel solo se repinta al cambiar s_version; la mascota es otra ventana y se relee sola). perfil_thread suma funda, marco y tapete a `ver`. Exe 20:36 copiado (cmp). Sin probar.
- 2026-11-19 20:4x — Mascotas del duelo (capturas): k_retoque Jarra Cibernética 911 dx 18 por su extensión (medida quieta en 44..128; su animación rueda hacia la derecha hasta el rombo), Bicho Devorador de Hombres 500 esc 1.45 DRAGON (antes recortado a 84 de ancho; ahora 124). Exe copiado (cmp). Sin probar.
- 2026-11-19 20:5x — Perfil (capturas de Estadísticas y Decks de los Oponentes): la placa vacía del nombre se veía en las otras páginas (estaba pegada en fondo.png, y fondo2 sale de él). perfil_recursos.py ya no la pega: guarda placa_nombre.png (338x68, en ref (NOMBRE_CX - 169, NOMBRE_Y - 4)); perfil.c la dibuja solo en la página Perfil. Exe y fondo.png + placa_nombre.png copiados (cmp). Sin probar.
- 2026-11-19 20:5x — Perfil y Buscar (capturas del usuario):
  - Perfil: clic sobre la foto del tapete ya no lo cambia (solo las flechas; la foto sigue brillando con el mouse). Mascota y funda brillan con el mouse como el tapete: perfil.c MASC_FOTO / FUNDA_FOTO (30/31), masc_luz.png y funda_luz.png (perfil_recursos.py foto_luz(w, h)); la de la funda va debajo del mazo.
  - Buscar, botones cortados: placa.png se recortaba de un lugar fijo de perfil/fondo.png (la vieja placa del nombre); con el rediseño ese lugar era el marco de la foto (violeta) y piedra. buscador_recursos.py ahora copia perfil/placa_nombre.png. Regenerada y copiada.
  - Buscar "Limitadas" vacío: leía el byte de la partida u8 0x665896+k, que vale 0 para todas fuera de la pantalla del mazo (lo mismo que pasó con los premios). Ahora ygo_limite_carta (mod, lista del juego, 3) = 1 o 2. Lo mismo en el círculo de las cartas del mazo del rival (perfil.c limite_de).
  - Exe 20:57:42 copiado (cmp), masc_luz/funda_luz/foto_luz y buscador/placa.png copiados. Sin probar.
- 2026-11-19 21:1x — Capturas del usuario:
  - Perfil, brillo de la funda: era un rectángulo con hueco cuadrado. perfil_recursos.py funda_luz(): el halo sigue la silueta del mazo (5 cartas 200x292 escalonadas) y la recorta. Copiado.
  - Buscar, marco amarillo corrido: medido en la captura (3,0 x 2,25 px por px del juego): la carta se ve 34 x 49,5 desde 0,5 a la derecha de la celda; buscador.c pintar_rejilla usa eso (antes 35 x 51, sobraba abajo y a la derecha).
  - Armar el mazo, barra "Ordenar" (8 botones abajo de la lista, el cofre): es la parte 3 de sub_000D9A10: derecha/izquierda [0x66D1F9] 0..7 (0xD9C81, |= 0x280, [0x66D210] = 0), arriba/abajo el orden [0x66D1FD + n] (1/0), A ordena (0xDA2A1). raton.c: deck_menu(3) = 0x66D1F9, 8 botones; el mouse encima lo marca, clic = A; rueda = arriba/abajo; clic afuera = B (como los otros menús). Posición medida en la captura contra las filas: 64 de ancho, cada 70,7 desde x 75,6, y 407..440 (si no coincide, el registro dice "[RATON] deck click game x,y").
  - Exe 21:1x copiado (cmp). Sin probar.
- 2026-11-19 21:2x — Usuario: buscador y Ordenar andan (registro: clics de la barra en y 421..423, botones 0 y 1 bien). Arreglos:
  - Buscar: las cartas que no coinciden mostraban una "L" amarilla a la derecha y abajo (el oscurecido de 34 x 49,5 no tapaba el borde dorado de la carta). El oscurecido vuelve a la celda entera, 36 x 52 desde medio px antes; el marco amarillo sigue con la medida nueva.
  - Armar el mazo, el menú redondo (parte 10) botón 2 ("Adjunta cartas"?) lleva a la parte 0 del cofre: la fila de pestañas sola (Mágicas, Ritual...), donde el mouse no hacía nada (registro: "deck click game 12,287 -> nothing (part 0)"). raton.c deck_menu(0, cofre) = [0x66DB74] (7 pestañas, 0xD9B9F |= 0x280; A = 0xDA1D7 la muestra): pestaña bajo el mouse marcada, clic = A, clic afuera = B y se repite el clic.
  - Exe copiado (cmp). Sin probar.
- 2026-11-19 21:3x — Usuario: la fila de pestañas con el mouse "ahora si funciona"; pidió que el clic derecho vuelva atrás. El clic derecho (B) excluía la parte 0 del cofre (por el paso de un clic en una pestaña de la lista); ahora vale ahí salvo durante ese paso (s_deck_tab_back): B vuelve al menú redondo (0xDA07E) y otro B a la lista. Exe copiado (cmp). Sin probar.
- 2026-11-19 22:1x — **Carta As (idea 3 de Master Duel), primera versión: estilo "Carga de guerra" (los Goblins)**. El usuario eligió esa muestra (y del Ojos Azules la del rayo por la boca; Descenso celestial y Ritual del Milenio guardadas para otras cartas). Muestras: scratchpad as_cine.py / as_goblin.py (con la carta real del juego x4: Mods-Extras\perfil\cartas\<idioma>\g\<ID>.jpg, no la funda).
  - ygo-apex/src/carta_as.c (nuevo): envuelve el despacho de llamadas sub_00095A30 (tools/rename_pads.py). En el estado de "al ser llamada" con la carta ya en su casilla (tipo 1 estado 2, tipo 2 estado 4, boca arriba 0x4000), si la carta (T_IMAGEN) es del modelo de la mascota de ese lado (tabla 0x44FBB0; mascota.c ygo_mascota_modelo_lado) arranca la animación y el despacho devuelve 1 ("llamada en curso", lo que lee 0x8404D) sin avanzar hasta 3,45 s: el duelo espera. Tipos 3..6 (Volteo, Especiales): solo registro "[AS] llamada tipo N ... todavía sin animación".
  - Dibujo: un hilo arma cada cuadro en la CPU (tiempo en ms reales: igual a 60 o 120 FPS; partículas a pasos fijos de 1/120 s), lienzo del tamaño de la vista (máx. 960 de ancho), RGBA premultiplicado: oscurecido con viñeta, carta 3D con perspectiva (homografía, bilineal), luz detrás, polvo y piedras (tapan), chispas/brasas/anillos/grietas/ojos (luz que se suma), bloom a 1/4, nombre con la letra de títulos del juego (fuente_titulo.c). Probado fuera del juego (scratchpad as_prueba/prueba.exe): 8-15 ms por cuadro, igual a la muestra.
  - Kit (nv2a_pb_d3d11.c): carta_as(&r) después de masc_tono: textura dinámica con la imagen (g_nv2a_as_px/w/h/ver), ONE/INV_SRC_ALPHA; g_nv2a_as_mueve mueve el cuadro (sacudida); g_nv2a_vista_w/h = tamaño de la vista.
  - Ojos por carta: k_ojos (5145 Duende travieso fuerza de ataque, 10 ojos medidos en la x4). La carta cae al centro, 4 golpes (0,62 / 1,25 / 1,55 / 1,85 s), ojos 1,9 s, nombre 1,95 s, baja a su casilla (raton.c near_mon/far_mon, y 252/142) 2,9 s, anillo 3,2 s.
  - F2 / Configuración "Carta As": Siempre / Primera vez por duelo / No (6 idiomas); "Primera vez" se reinicia en mascota.c crear().
  - Falta: la carta grande existe solo para las cartas de los rivales (perfil_cartas.py g/): sumar las de las 100 mascotas; los otros estilos; Volteo/Especial. Exe 22:15 y Configuración copiados (cmp). Sin probar.
- 2026-11-19 22:4x — Carta As, después de la prueba del usuario ("si se vio pero primero toco donde va y luego aparecio la animacion"; "de mala calidad la imagen de la carta de cerca"; "en f2 no ahi ninguna opcion"):
  - Momento: ahora antes de que la carta se coloque. La máquina la coloca en Llamada Normal estado 1 y por Sacrificio estado 3: sub_00086F50(p, 0, casilla) mueve el cursor a su casilla y la orden 0x4C hace la animación del juego; después pasa al 2 / 4 (donde arrancaba antes). La casilla sale de (u32 0x66BA7C >> 1) & 0x1F.
  - Calidad: la carta la dibuja el kit con su propia textura (BGRA premultiplicada, mipmaps con GenerateMips y un muestreador trilineal propio: el de la imagen tiene MaxLOD 0) a la resolución de la pantalla, por la homografía que manda carta_as.c (g_nv2a_as_carta: 3x3, alfa, ancho/alto del lienzo); bordes suaves con fwidth. El lienzo de la CPU se partió en dos: detrás (oscurecido, luz de atrás, anillos y grietas del piso) y delante (polvo, chispas, ojos, nombre). Kit: detrás → carta → delante.
  - F2: faltaba xbox_MenuAdd(&k_mascota_rows[3]) en main.c (la fila estaba en la tabla pero no se registraba).
  - Probado fuera del juego (capas bien). Exe copiado (cmp). Sin probar.
- 2026-11-19 22:37 — Carta As, segunda prueba del usuario ("se vio luego de apoyarse la carta, la letra que dice el nombre se ve borrosa (usa la letra del juego)"):
  - Momento: el registro mostró [CAMPO] con la carta ya en su casilla al arrancar. La casilla la escribe la orden 0x90 (sub_00081150) un paso antes: Llamada Normal estado 0 (también saca los sacrificios con sub_000C7980) y por Sacrificio estado 2 (después de elegir los sacrificios en el 1). El disparo pasó a tipo 1 estado 0 / tipo 2 estado 2.
  - Nombre: sigue con la letra de los títulos del juego (fuente_titulo.c), pero las letras ahora las dibuja el kit como la carta: titulo_gpu() arma la misma línea a la resolución de la pantalla (Q = alto de la vista / alto del lienzo x 1,5 por la entrada a 1,5x; hasta 128 px, más no da la página 2x), dorado con borde oscuro, BGRA premultiplicado; kit: g_nv2a_as_tit_px/w/h/ver + g_nv2a_as_tit[12] (homografía afín del recuadro), textura con mipmaps (as_textura) dibujada después de la capa de delante. El lienzo conserva solo el brillo del nombre. Kit: as_textura() y as_constantes() comunes a carta y nombre.
  - Probado fuera del juego (textura del nombre 2059x234 a 1080p, nítida). Exe 22:37:18 copiado (cmp). Sin probar en el juego.
- 2026-11-19 22:5x — Carta As, tercera prueba ("se ve muy bien ahora solo que parece que se pone 2 veces en el campo"): después de nuestra animación la orden 0x90 del juego (tabla del despacho de órdenes 0x8150C -> sub_000CDE60) hacía lo suyo: paso 0 sub_00086EB0(p, 0xB), paso 1 sub_000D1E70 (datos de la carta en 0x66015C) + sub_00046B30 (deslizamiento desde la mano), paso 2 espera (sub_00046A10) y coloca (sub_000D2A00). carta_as.c envuelve sub_000CDE60 y sub_00046B30 (rename_pads): una vez después de la animación (s_sin_desliz) saltea el paso 0 y en el paso 1 no arranca el deslizamiento (los datos sí), así el paso 2 coloca enseguida. Registro "[AS] el deslizamiento del juego desde la mano, salteado". La orden 0x4C (sub_000D1280: sub_00047330, efecto de la llamada) queda. Exe copiado (cmp). Sin probar.
- 2026-11-19 23:0x — Carta As, cuarta prueba (captura: después de la animación quedó la carta grande del juego trabada arriba al centro y la casilla con otros datos: bytes 4..9 00 00 00 00 0D 00 en vez de C3 18 08 00 1A 00). Saltear sub_00046B30 dejaba a medias la colocación del juego: DESHECHO. Ahora es un traspaso: en 2,9..3,25 s nuestra carta va, derecha, al lugar donde empieza la colocación del juego (centro arriba, centro a 0,292 del alto, 0,347 del alto de altura; medido de la captura: la del juego ocupa y 126..503 de 1080, la nuestra 130..500; la del juego es más ancha, es su diseño de ficha) y se queda hasta que el juego llama a sub_00046B30 (envoltura: s_entregada, "[AS] la carta, al juego (su colocación)"); la baja a la casilla la hace el juego, una sola vez. SUELTA 3,25 s. Sin anillo de llegada. sub_000CDE60 queda envuelta sin cambios. Exe copiado (cmp). Sin probar.
- 2026-11-19 23:1x — Carta As ("quedo bien ahora me parece pero el texto no se ve nitido el nombre de la carta"): (1) la letra de títulos se usaba de la página 2x y el nombre la estiraba ~1,5..2,3x: tools/fuente_titulo.py escribe además ygo-apex/src/fuente_titulo4.h (la página mejorada 4x tal cual, 1800x1560) y fuente_titulo.c usa la 4x cuando hay más de 2 px por px 1x (e > 2; la elección es local, Perfil y Buscar siguen con la 2x en sus tamaños); titulo_gpu hasta 192 px. (2) el borde oscuro eran 8 copias corridas de la letra (se veía doble); ahora es un contorno sólido (máximo en un cuadrado, dos pasadas, 2,4 px de lienzo). Probado fuera del juego (comparación antes/después). Exe copiado (cmp). Sin probar.
- 2026-11-19 23:1x — Carta As para las demás mascotas, de a 10 (el usuario: "podes seguir con las demas? empieza de a poco ve de 10 en 10"). carta_as.c: tabla k_estilos {id, estilo, color, boca u/v en la carta, dirección del rayo}; estilo_de(id) en empezar(); sin fila = CARGA tierra (como antes). Estilo nuevo RAYO ("Rayo de destrucción", la muestra del Ojos Azules que le gustó): entra rápido desde la derecha girada, anillo de choque + líneas de velocidad + chispas, electricidad en los bordes (rayo_puntos/rayo_dibujar, forma nueva 24 veces por segundo); con boca: carga y dispara el haz (núcleo blanco, halo del color, estelas, destello anamórfico); sin boca: rayos del cielo al borde de arriba de la carta. Nombre a 2,25 s en RAYO. luz_todo (destellos suaves), luz_linea saltea fuera de 3 w. Primeras 10 mascotas (k_modelo 0..9): Ojos Azules 4007 RAYO azul boca (0,27, 0,43) a 172°; Ojos Rojos 4088 RAYO rojo-naranja boca (0,44, 0,315) a 200°; Cráneo Convocado 4028 RAYO violeta, cielo; Jinzo 4758 RAYO rojo, cielo; Gaia 4044 CARGA dorado; Guardián Celta 4047 CARGA verde. Faltan del grupo: Kuriboh 4064 (sin carta HD), Mago Oscuro 4041, Mago Oscuro 5165, Maga Oscura 4766 -> estilos Descenso celestial / Ritual del Milenio (próxima tanda). Muestra (motor real, banco scratchpad as_prueba/video.c): modding/muestras/carta_as_mascotas_1a10.mp4. Peor cuadro ~21 ms en la CPU (con la carta dibujada por CPU en el banco). Exe copiado (cmp). Sin probar.
- 2026-11-19 23:2x — Carta As para las 100 mascotas (el usuario: "segui entonces hasta terminar todas"):
  - Cartas grandes: tools/perfil_cartas.py suma cartas_mascotas() (mascota.c k_modelo -> tabla de pares 0x44FBB0): las 33 que faltaban (Kuriboh incluido) hechas en los 5 idiomas (657 por idioma) y copiadas al juego (diff ok). copiar_al_juego.sh ya corre ese paso.
  - Estilos nuevos en carta_as.c: DESCENSO (rayos de luz desde arriba, rayos_dios a 1/4 con tabla por ángulo; neblina; chispas que caen; la carta baja balanceándose — sin el giro completo de la muestra: la carta es una sola textura, sin dorso —; aterriza a 1,35 s con anillo, destello anamórfico; brillo holográfico holo() sobre la carta) y RITUAL (arena que entra en espiral, anillo de jeroglíficos en perspectiva, el Ojo del Milenio detrás con rayos dorados, la carta armada en 54 piezas dibujadas en el lienzo 0,8..1,7 s — tejas() — y después la del kit; pulso dorado, holo). La pose de cada estilo hasta 2,9 s y después el traspaso común.
  - Asignación: tabla k_estilos (Goblin; bocas medidas: Ojos Azules, Ojos Rojos, Definitivo, B. Skull, Meteoro Negro, Dragón mascota de la Arpía, Dragón milenario, Dragón de tres cuernos, Ryu-Ran, Dragón zombi; Cráneo y Jinzo rayo del cielo; Kuriboh DESCENSO; Gaia dorado, Guardián Celta verde). El resto por los datos del juego (T_DATOS: tipo bits 20-24, atributo 29-31; tipos medidos: 1 dragón, 2 zombi, 3 demonio, 4 piro, 6 roca, 7 máquina, 8 pez, 9 dinosaurio, 10 insecto, 12 guerrero-bestia, 13 planta, 14 aqua, 15 guerrero, 16 bestia alada, 17 hada, 18 lanzador, 19 trueno; atributos 1 luz, 2 oscuridad, 3 agua, 4 fuego, 5 tierra, 6 viento): dragón/máquina/trueno RAYO cielo, lanzador/demonio/zombi RITUAL dorado (oscuros más violáceo), hada/bestia alada DESCENSO, el resto CARGA; color por atributo. El registro dice el estilo ("[AS] carta ... estilo ritual").
  - Rendimiento: luz_anillo y luz_linea saltean fuera de 3 w; rayos_dios solo debajo del origen. Peor cuadro en el banco (con la carta por CPU) ~19 ms.
  - Muestras (motor real): modding/muestras/carta_as_descenso_ritual.mp4 (Mago Oscuro x2, Maga Oscura, Kuriboh, Arpía, Definitivo, Exodia, Dragón zombi). Exe 23:18:44 copiado (cmp). Sin probar en el juego.
- 2026-11-19 23:3x — Carta As, pedido del usuario ("vamos a eliminar el Nombre de abajo solo va a aparecer la carta. Definitivo cambiale ... queda mal, lo mismo para dragon zombi. los otros que disparan un rayo que no sean ojos azules tienen que ir de un color que combine"): sin nombre (no se llama a titulo(); la textura se arma igual pero con alfa 0); Dragón Definitivo 4386 -> DESCENSO; Dragón Zombi 4103 -> RITUAL violeta; el haz de los demás (no 4007) con su color saturado (núcleo 0,35 + 0,65 del color, sin blanco): Dragón de la Arpía verde, Meteoro Negro y Ojos Rojos rojo-naranja; Ojos Azules sigue blanco. Muestra: modding/muestras/carta_as_ajustes.mp4. Exe copiado (cmp). Sin probar.
- 2026-11-19 23:31 — Botón ON/OFF de la Carta As al lado de AUTO (el usuario: "al lado de auto podemos hacer un pequeño boton que diga Off y si lo presionas On o viceversa esto para activar o desactivar las animaciones ... el Rival tambien solo tiene que tener la de su mascota"). tools/auto_recursos.py boton(): as_on.png / as_off.png (196x70, la misma placa del nombre acortada, "ON" claro / "OFF" apagado con la letra del juego) -> Mods-Extras\auto. Kit (nv2a_pb_d3d11.c auto_placa): struct nv2a_auto_kit + px2/w2/h2/on2/x2..alto2; el bloque textura+dibujo es un bucle de 2 imágenes (srv/hecho_* de 4): la segunda a la derecha de AUTO (ax + 74,7 + 2, ancho 74,7 x w2/280, mismo alto), con la misma geometría de la placa del juego. auto.c: lee as_off/as_on, on2 = ygo_as_get() != 2 en el hilo; el clic en su rectángulo alterna ygo_as_set(2 / 0) (Siempre), guarda (xbox_MenuSaveSoon) y registra "[AS] animaciones apagadas/encendidas (clic en el botón)". Afecta a las dos (la tuya y la del rival); el rival ya usa solo la carta de su mascota (carta_as.c: modelo == ygo_mascota_modelo_lado(lado)). Exe 23:31:20 y PNG copiados (cmp / diff). Sin probar.
- 2026-11-19 23:5x — Perfil: botón "Ver Invocación" (el usuario eligió la propuesta A de modding/muestras/perfil_carta_as; "pone al boton 'Ver Invocacion'"; la D, una galería de las 100, la está pensando: otro menú o un botón dentro de Perfil). perfil.c: VER_INV debajo del número de la mascota (INV_Y 850, 300x50, el botón dorado de fondo.png px 560..886 x 710..773 achicado, marco al pasar el mouse); "N / 100" a 812, "Cartas" a 908 (30) y su barra a 946 (14). Clic -> carta_as.c ygo_as_ver(mascota): busca la carta del modelo (tabla de pares) y su k (T_IMAGEN), modo vista (s_vista: nada espera, sin traspaso: al final la carta crece un poco y se desvanece, alfa s_carta_a al kit). Mientras corre (ygo_as_activa) mostrar() oculta todas las ventanas del Perfil (la animación es del kit, en el cuadro del juego, abajo) y los clics del Perfil se ignoran; al terminar vuelven solas. mascota.c ygo_mascota_modelo_de(i). f2_idiomas.c "Ver Invocación" en 6 idiomas. Muestra: modding/muestras/perfil_carta_as/A_ver_invocacion.png. Exe copiado (cmp). Sin probar.
- 2026-11-19 23:5x — Perfil, prueba del usuario (captura: durante la animación se veían las 6 flechas, el modelo 3D de la mascota y el menú de fondo): "al hacerle click a la mascota que brilla salga la animacion. eliminemos el Boton ... de ver invocacion. arregla lo que se ve el fondo y los botones aparte del modelo 3D". Causa: mostrar() ocultaba cada ventana pero el hilo del Perfil las volvía a mostrar con su SetWindowPos(SWP_SHOWWINDOW) de "primera vez" (vm, flechas). Ahora el hilo trata la animación como panel cerrado (if (... && !ygo_as_activa())): todo oculto, nada liberado; al terminar se repintan solas. Sacado el botón "Ver Invocación" (posiciones de "N / 100", "Cartas" y barra como antes); el clic en la mascota (MASC_FOTO, la que brilla) llama ygo_as_ver. Fondo de la vista previa: oscurecido 0,93..1 (antes 0,72..0,97, se veía el menú). Exe copiado (cmp). Sin probar.
- 2026-11-19 23:5x — Carta As, prueba del usuario ("cuando hace las ondas se ve como que esta dentro de un cuadrado"; "se ve de fondo el menu principal quiero que se vea perfil"; Ojos Rojos "el disparo del rayo sale de cualquier lado no de la boca"; "los Magos Oscuros ... su propia animacion ... violeta, muy genericas las de ahora"):
  - luz_anillo: distancia real a la elipse (valor del nivel sobre su gradiente): el anillo plano tenía el brillo de las puntas más ancho que su caja (cortado en recto); ahora parejo y se apaga dentro de la caja.
  - Fondo de la vista previa: el clic en la mascota pide (s_pedir_as) al hilo del Perfil una foto del panel (vp + el modelo vm + las 6 flechas vfd), ygo_as_ver_con() la escala al lienzo (s_vfondo, promedio por caja) y la animación la usa de fondo bajo el oscurecido (0,55..0,9); el Perfil sigue oculto mientras corre y vuelve igual.
  - Ojos Rojos 4088: RAYO del cielo rojo (sin boca).
  - Estilo nuevo MAGO ("magia oscura"): círculo mágico en el piso (anillos, hexagrama que gira, runas), la carta sale de él levantándose (0,35..1,2 s), chispas violetas en espiral, rayos en los bordes, bola oscura en la punta del báculo (medida) 1,5..2,0 y su estallido (3 anillos, destello, rayos, sacudida), brillo holográfico. Mago Oscuro 4041 y 5165 violeta, Maga Oscura 4766 rosa, Mago del Caos Negro 4728 carmesí-violeta.
  - Muestra: modding/muestras/carta_as_magia_oscura.mp4. Exe copiado (cmp). Sin probar.
- 2026-11-20 00:0x — Perfil, "se ve el fondo del menu principal medio segundo ... queda feo": los huecos entre las ventanas del Perfil y la animación. Inicio: el Perfil se ocultaba apenas empezaba (s_activo), antes de que el kit dibujara la primera imagen (la primera vez compila sus sombreadores). Ahora se oculta con ygo_as_tapa(): el kit dice qué versión dibujó (g_nv2a_as_dibujado, nv2a_pb_d3d11.c carta_as) y el Perfil espera a la primera. Final: la animación se borraba antes de que el Perfil volviera a pintar sus ventanas. Ahora el hilo deja la última imagen (la foto del Perfil sola, cuadro(DUR + 0,5): sin oscurecido ni carta), marca s_fin (el Perfil vuelve a mostrarse) y espera ygo_as_perfil_vuelto() (panel y modelo mostrados) o 1,5 s, más 40 ms con las dos. Exe copiado (cmp). Sin probar.
- 2026-11-20 00:1x — Perfil: el tapete y el mazo de la funda se mueven apenas (el usuario: "un leve movimiento la baraja y el tapete"; muestra modding/muestras/perfil_flotar.mp4). perfil.c: ya no se pintan en el panel (el hueco del tapete queda oscuro debajo); dos ventanas propias vt (tapete) y vk (mazo, pintar_baraja: 5 cartas con su sombra, caja (CARTA_W+24) x (CARTA_H+28) desde CARTA_X-2, CARTA_Y-26), sobre el panel y bajo las puertas como el modelo; suben y bajan 3 px de referencia (sen(t·1,1) y sen(t·1,3+1)), solo se mueven (UpdateLayeredWindow sin repintar) y se repintan al cambiar tapete/funda/marco/escala. Se ocultan con el resto (otras páginas, panel cerrado, la animación de la Carta As) y entran en la foto del Perfil de la vista previa. Exe copiado (cmp). Sin probar.
- 2026-11-20 00:2x — El flotado del tapete y el mazo en el Perfil, DESHECHO (el usuario: "se ven horribles"; perfil.c vuelto al de antes de ese cambio). Lo que quería: movimiento real en el campo (el tapete y la baraja en el duelo) y en el Perfil verlo al tocar el tapete o el mazo. Propuestas en video (modding/muestras/tapete_baraja_propuestas.mp4, sobre el tapete y la funda reales): tapete A Destello (franja de luz que cruza), B Energía en las líneas (pulsos del color de la mascota por los marcos), C Tapete vivo (el dibujo se mueve despacio y suben partículas); baraja D brillo holográfico en la de arriba, E barajar (en el Perfil al tocar el mazo). En el juego el tapete es la textura del tablero (nv2a_pb_d3d11.c s_tap_srv, fundas.c TABLERO): animarlo es cambiarla por una que el kit actualice (queda debajo de las cartas). Esperando que elija. Exe 00:2x (sin el flotado) copiado (cmp).
- 2026-11-20 00:2x — Tapete con luz y baraja con brillo (el usuario, tras las muestras: "la C de tapete me gustaron los brillos pero no que se mueva. quiero las 3 y que la gente elija como con los marcos ... pone abajo de tapete (nombres simplificados). en la baraja quiero que se vea el Brillo ... al pasar el mouse por el Deck, lo mismo en Batalla. lo de barajar quedo mal hazme otra mas trabajada"):
  - src/tapete_fx.c (nuevo): "Efecto del tapete" No / Luz (franja que cruza cada 4 s) / Pulso (luz que recorre los marcos dorados, del color del tapete, respirando) / Brillo (partículas de luz que suben; el dibujo quieto). Máscaras por tapete (líneas doradas por color, su halo, el color de su arte). fundas.c le pasa cada tapete leído (ygo_tapete_fx_base); un hilo en el duelo (escena 0x656F88, estado 0x657148 = 6) arma ~30 cuadros/s en dos búferes y se los da al kit; fuera del duelo o en No vuelve el tapete tal cual. Medido fuera del juego en el tapete real 2048x512: 2..6 ms por cuadro.
  - Kit (nv2a_pb_d3d11.c): nv2a_tapete_cuadro() (sin copia, búfer del llamador) y tap_srv con textura DEFAULT que se actualiza en el lugar (UpdateSubresource) si el tamaño no cambia.
  - F2 (main.c k_funda_rows[3] "tapete_efecto") y Configuración (Opt + textos + tip), 6 idiomas (f2_idiomas.c: Efecto del tapete, Luz, Pulso, Brillo). Configuración.exe recompilado y copiado (cmp).
  - Perfil: cuatro botoncitos bajo el cuadro del tapete (No / Luz / Pulso / Brillo; el elegido dorado; solo con un tapete que no sea el Clásico); el cuadro muestra el efecto elegido en vivo (ventana vt sobre la foto, ygo_tapete_fx_pintar). El mazo brilla (arcoíris que cruza la de arriba cada 2 s) mientras el puntero está encima (ventana vk, pintar_baraja). Ambas entran en la foto del Perfil de la Carta As.
  - Barajar: muestra nueva modding/muestras/barajar_v2.mp4 (riffle en 3D, puente, golpecito, brillo). Esperando aprobación.
  - Falta: el brillo del mazo en el duelo al pasar el mouse (el mazo del jugador es la zona 13, raton.c slot_place: 458,285 px del juego).
  - Exe 00:21:57 y Configuración copiados (cmp). Sin probar.
- 2026-11-20 00:3x — "no se vio nada" (el tapete con luz en el Perfil y el brillo del mazo): las ventanas nuevas vt y vk no tenían su DC de memoria (perfil_thread crea uno por ventana con CreateCompatibleDC; sin él lienzo_de no puede seleccionar el bitmap y UpdateLayeredWindow no dibuja nada). Agregados vt.mem y vk.mem. Exe copiado (cmp). Sin probar.
- 2026-11-20 00:4x — Perfil (el usuario: "eliminar los Botones y el Contorno de los mazos (los botones) ahora sera haciendo click en el tapete y se cambia entre las 4 opciones y el deck lo mismo. las lineas estoy a 120 fps y se siente muy lentas"): sin los cuatro botoncitos bajo el tapete ni los cinco puntos de marco bajo el mazo; clic en el tapete (FOTO) = siguiente efecto (No, Luz, Pulso, Brillo; solo con un tapete que no sea el Clásico), clic en el mazo (FUNDA_FOTO) = siguiente marco (solo con funda). tapete_fx.c más rápido: Luz cada 3 s y cruza en 0,9 s (era 4 y 1,6), Pulso al doble (1,2), Brillo sube más rápido; en el duelo el hilo arma un cuadro cada ~6 ms (~100 por segundo; antes 30). Exe copiado (cmp). Sin probar.
- 2026-11-20 00:3x — Duelo (el usuario: "ese boton ... no choque con el campo"): el botón On/Off de Carta As terminaba en x≈157 px del juego (placa en 24.5,239 134x37; AUTO 28..103, botón 105..157, y 275..300), pegado a la columna cercana del campo (x 184 ±22). Debajo está la mano (arriba y≈311), así que no puede bajar. AUTO y el botón quedan al 0,85 de su tamaño, uno al lado del otro: terminan en x≈138 (nv2a_pb_d3d11.c auto_placa). Los clics usan la posición que deja el kit. Exe 00:36 copiado (cmp). Sin probar.
- 2026-11-20 00:4x — FPS guardados, AUTO, resoluciones (el usuario: "guardar los Fps ... siempre inicia en 60", "el boton auto tambien se tiene que guardar", "cubrimos todas las resoluciones?"):
  - Fila nueva "Cuadros por segundo (F8)" (fps_juego = 60/90/120; main.c k_fps_rows, el vblank del kit) en F2 y en Configuración, 6 idiomas. F8 llama a xbox_MenuSaveSoon (raton.c fps_key): queda guardado para el próximo inicio.
  - AUTO ya se guardaba (auto_cadena=Sí en Ajustes/ygo_menu.ini; en los registros AUTO salteó preguntas sin clic). Agregado en el kit: al cerrar la ventana se escribe un guardado pendiente (xbox_MenuSaveFlush en WM_CLOSE), por si se cierra a menos de 0,4 s de un cambio.
  - Los FPS son reales: a 120 el kit da 120 vblank/s (registro 18:16: 600 en 5 s) y el juego dibuja 117-120 cuadros/s.
  - Tamaño de ventana: agregados 1280x800, 1280x1024, 1440x900, 1680x1050, 1920x1200, 2560x1600 (16:10 y 5:4), en el kit y en Configuración. La pantalla completa usa la resolución del monitor.
  - Exe y Configuración 00:42 copiados (cmp). Sin probar.
- 2026-11-20 00:5x — F2 (el usuario: "oculta todas estas opciones", "guardar despues de ganar pone que si siempre", "auto ... por defecto viene que no y si el jugador lo cambia se guarda asi"): ocultas en F2 (xbox_MenuAddHidden, se siguen cargando y guardando) Tu mascota, Entrada del duelo, Carta As, Funda, Marco de la funda, Tapete, Efecto del tapete, AUTO y Guardar al ganar. Guardar al ganar siempre Sí (main.c guardado_siempre ignora un "No" guardado) y fuera de Configuración. AUTO arranca en No (s_on = 0) y lo que elija el jugador en la placa queda guardado. Exe y Configuración copiados (cmp). Sin probar.
- 2026-11-20 01:0x — Perfil (el usuario: "a veces al cambiar de tapete no cambia queda el otro"): la ventana del tapete con efecto (vt) rearmaba su imagen cuando cambiaba la elección (ygo_tapete_get, cambia en el clic), pero la imagen nueva (s_tapete_img) se lee recién en el próximo pintado del panel; si el efecto pintaba en el medio copiaba la vieja y no volvía a leer. Ahora sigue a s_tapete_de (el tapete que tiene la imagen). Exe copiado (cmp). Sin probar.
- 2026-11-20 01:1x — Premio (el usuario: el cartel "Nueva" se vio bien sobre las cartas, "pero al cerrar la puerta se vieron por un segundo mas"): el juego sigue dibujando las cartas debajo de sus puertas al cerrarse, así que el kit las seguía viendo y los carteles (ventana encima) quedaban sobre las puertas. premios_extra.c marcas_premio_tick: lee la transición [0x66DCD8]+0x10 (1 cerrando, 2 cerradas, como raton.c) y quita los carteles a los 120 ms del 1 y mientras están cerradas. Exe copiado (cmp). Sin probar.
- 2026-11-20 01:2x — Tapetes (el usuario: "que el Rival Tenga su Tapete de su mascota", "el tapete numero 0 tiene que ser el comun del juego. aplicale el brillo y las lineas a ese tambien"):
  - Rival: fundas.c, con la funda del rival (k_por_mascota, ygo_fundas_tick) pide su tapete; un hilo (tapete_rival_hilo) lee Mods-Extras\tapetes\<arte>.png y lo pasa al kit (nv2a_tapete_rival, nuevo). El kit dibuja la mitad de arriba del tablero con él (s_rev_arriba = s_tap2_srv), la de abajo con el tuyo; si uno falta, la del juego. El rival lo ve al derecho desde su lado (en pantalla, dado vuelta, como Master Duel). Sin efectos de luz.
  - Tapete 0: Mods-Extras\tapetes\clasico.png = el tablero del juego (su 4x de modding/texturas/mejoradas, también copiado a modding/texturas/tapetes/juego). fundas.c lo pasa como a los demás, así tapete_fx.c le pone Luz, Pulso o Brillo; si falta el archivo, el del juego. Perfil: el clic en el tapete cambia el efecto también con el Clásico y su recuadro muestra el efecto en vivo (clasico_vista.jpg).
  - Lector de png compartido (tapete_leer). Exe copiado (cmp). Sin probar.
- 2026-11-20 02:0x — FPS y lentitud (el usuario: "lo siento un poco lento ... agrega 144 fps, 165 fps y 200 fps aparte de eso vsync"):
  - F8 y la fila fps_juego: 60, 90, 120, 144, 165, 200 y "Monitor" (la frecuencia de la pantalla, EnumDisplaySettings; guardada como "Monitor"). F2 y Configuración.
  - Registro leído: a 120 el kit da 600 vblank cada 5 s parejos (7-9,7 ms); HITCH de 25-29 ms cada tanto y [MASCOTA] lento 12-18 ms solo al terminar duelos y en cargas (30 en la sesión); el de 922 ms fue la ventana en segundo plano. Nada frena el juego todo el tiempo; lo probable es que 120 no coincida con la frecuencia del monitor (tirones de ritmo): elegir Monitor con VSync.
  - Botón Salir a Windows: muestra modding/muestras/salir_muestra.png (tools/salir_recursos.py: el medallón del Perfil con puerta y flecha, salir*.png en modding/texturas/perfil/juego), debajo del último ícono de la rueda. Esperando aprobación; no está en el juego.
  - Exe y Configuración 02:01 copiados (cmp). Sin probar.
- 2026-11-20 02:1x — Botón Salir a Windows en el menú principal (el usuario aprobó la muestra: "Sí, ponelo"): perfil.c, ventana propia (vs) con salir.png / salir_luz.png / salir_sol.png (Mods-Extras\perfil, tools/salir_recursos.py), debajo del último ícono de la rueda: SALIR_DX 166, SALIR_DY 64,4 px del juego desde el centro de la rueda, al 0,73 del tamaño del botón del Perfil. Igual que ese: se muestra con él, se recorta con la rendija de las puertas del juego al cerrarse, entra con fundido, con el mouse encima se dora con el sol que gira y la onda. Clic: WM_CLOSE a la ventana del juego (como Salir a Windows del F2; guarda lo pendiente). Log [PERFIL] salir a Windows. Exe e imágenes copiados (cmp). Sin probar.
- 2026-11-20 02:2x — Menú principal (el usuario: "pasar el Botón de duelo vinculado al de Opciones? y usar el Boton de duelo vinculado para el de salir a windows asi nos queda todo simetrico"; Perfil y Salir "como botones reales": una sola luz a la vez y el nombre en el cartel de abajo):
  - El último ícono de la rueda (Duelo vinculado, índice n-1 = 6 de 7) es Salir a Windows. El port no tiene red (no hay XNet/sockets en el kit), así que Duelo vinculado no podía andar: se reemplaza, no se mueve a Opciones.
  - raton.c: el texto 24 de cada tabla (0x5FC2DC/0x5FC2C4: 1 inglés, 2 alemán, 3 francés, 4 italiano, 5 español; 0 inglés) apunta a "Salir a Windows" / "Exit to Windows" / "Zu Windows beenden" / "Quitter vers Windows" / "Esci a Windows" (memoria del juego, una vez). El A con la rueda quieta en ese ícono (mando, teclado o clic) se saca y cierra el juego (WM_CLOSE como el F2). Pasa el lugar del ícono (wheel_place) y si está elegido (g_ygo_sal_*) en los estados 0, 1 y mientras cierran las puertas.
  - perfil.c: el medallón de salir (salir.png / salir_sol.png) se dibuja encima del ícono del juego, a su tamaño (1919x1079 de la captura -> px del juego, /1,3), apagado o dorado con el sol que gira cuando la rueda lo elige; sin fundido (para no mostrar el ícono del juego); recortado por la rendija de las puertas. El botón aparte de 02:1x y su clic propio sacados.
  - Pendiente: que el Perfil apague la luz del ícono elegido y ponga su nombre en el cartel (el sol es parte del modelo de la rueda; hace falta un volcado Ctrl+F9 del menú para saber qué dibujo es).
  - Respaldos antes del cambio en el scratchpad (perfil_antes_rueda.c, raton_antes_rueda.c). Exe 02:14 copiado (cmp). Sin probar.
- 2026-11-20 02:4x — Rueda del menú (capturas del usuario: el sol del juego girando debajo de nuestro medallón de salir; Perfil y Salir "como botones reales"). Volcado Ctrl+F9 02:15 (flip 4335): los íconos son 2D; la fila de íconos apagados es un dibujo (3F765E756A9AD540, 41 vértices, incluye el elegido); el elegido suma su sol (603BA796E198985F, 71 vértices, 69 px) y su medallón dorado (3F765E756A9AD540, 5 vértices, 60 px).
  - Kit (oculta_2d): no dibuja ese sol ni ese medallón dorado si su centro cae en una caja pedida (g_nv2a_oculta, 2 cajas, px del juego, vencen solas). perfil.c pide la del último ícono siempre (debajo va nuestro medallón de salir) y todas mientras el puntero está en el botón del Perfil: así no quedan dos iluminados.
  - Cartel: con el puntero en el Perfil, una ventana tapa el medio del cartel de abajo con su fondo sin texto (cartel.png de la captura del usuario, tools/cartel_recursos.py; 216,9..426,8 x 346,5..378 px del juego) y "Perfil" (traducido) con la letra del juego.
  - El medallón de salir se apaga también mientras el puntero está en el Perfil. Exe y cartel.png copiados (cmp). Sin probar.
- 2026-11-20 02:5x — Rueda, segunda vuelta (capturas del usuario: el ícono del juego se asomaba alrededor de nuestro medallón de salir; con el puntero en el Perfil otro ícono seguía encendido). Leído: el elegido se dibuja brillante dentro de la fila misma (el dibujo de 5 vértices era otra cosa), por eso sacar el sol no alcanzaba.
  - Kit: 3 cajas; la fila de íconos (3F765E756A9AD540, >6 vértices) se mide en cada caja chica: el rectángulo de las esquinas a menos de 45 px de su centro (bx0..by1, medido).
  - Salir: el medallón se pone sobre ese rectángulo medido (centro exacto; lo visible al 0,8 del cuadro, como el del juego); sin medida, como antes pero 12 % más grande.
  - Perfil con el puntero encima: además de ocultar soles, el ícono elegido (raton.c g_ygo_sel_k / x / y) se mide y se tapa con su versión apagada (rueda_0..5.png, recortes del cuadro de 60x60 de la captura del usuario donde estaban apagados, bordes difuminados; tools/cartel_recursos.py). Si el elegido es Salir, nuestro medallón se apaga.
  - Exe y rueda_*.png copiados (cmp). Sin probar.
- 2026-11-20 03:0x — Rueda (el usuario: "aun se ve un poco el boton"; la onda del Perfil "con menos intensidad" que la de los botones del juego): el medallón de salir pasa al 0,96 del cuadro medido (era 0,875); la onda del Perfil y la de Salir con alfa 0,85 (era 0,45). Exe 02:27 copiado (cmp). Sin probar.
- 2026-11-20 03:2x — Cartel de Salir y bocas de Lumis y Umbra (el usuario: "un cartel cuando se apriete el boton de Salir que diga Salir a Windows? y tenga si y no todo con tematica y fuente de letra del juego"; "2 personajes que son uno solo ... cuando hablan los 2 solo uno mueve la boca"):
  - Cartel: el del propio juego, el de B en la rueda (estado 0 en 0x10D8C5: mensaje texto 27, botones textos 9 y 10; estado 5 en 0x10D9F1: cursor [caja +0xBD8], 0 = el izquierdo; A ahí = volver al título). raton.c: el A en el ícono de salir apunta los textos 27/9/10 de las 6 tablas a "¿Salir a Windows?" (traducido) y a Sí / No del juego (textos 7 y 8), manda B 120 ms y, al llegar al estado 5, devuelve los textos. En el estado 5 con nuestro cartel: A sobre Sí cierra el juego (WM_CLOSE; ese A no llega al juego, que iría al título); No o B lo cierran como siempre. Log "[RATON] Salir a Windows: el cartel / Sí".
  - Lumis y Umbra (personaje 19; retratos 19, 46 y 73 de la tabla 0x60F518): su guion tiene una segunda boca, la pista de control bit 2 (slot 2, bucle reposo/media/abierta como la primera). voces.c movía solo la boca del slot 1; ahora con la voz mueve las dos (y toma el retrato que habla con el bit 1 o el 2).
  - Exe copiado (cmp). Sin probar.
- 2026-11-20 02:42 — Mouse en el cartel de la rueda (el usuario, con captura del cartel Sí/NO: "revisa que ande bien el click y lea bien ese cartel de si y no"):
  - Leído: el cartel es una lista vertical (Sí arriba = cursor 0, NO = 1), el mismo widget de los carteles del duelo (sub_000F0040: +0xBC4 renglones, renglón k objeto en +0x1B0+4k con x/y en +0xC/+0x10; marcado [+0xBD8], lo pone sub_000EEFE0(k) con el tic 0x10000001 como arriba/abajo del juego en 0x10DA1E/0x10DA57). El puntero no lo veía: en el estado 5 la rueda deja de refrescarse y un click en cualquier lado era A sobre el renglón marcado.
  - raton.c: caja_raton (en el estado 5, dentro de sub_0010D6C0) mide los renglones y marca el pedido a la manera del juego; caja_pointer (antes de wheel_pointer) marca el renglón bajo el puntero, el click responde ese renglón (A cuando ya está marcado), fuera de los renglones no hace nada, clic derecho = B. Vale para nuestro cartel y para el "¿Seguro?" de B. Log "[RATON] wheel box: ...".
  - Exe copiado (cmp). Sin probar.
- 2026-11-20 02:48 — Configuración.exe (el usuario: ocultar filas y la línea de Voces si el idioma no es español):
  - Configuracion.cs: fuera de la lista las filas Entrada del duelo, Carta As, Número de daño, AUTO en el duelo, Tu mascota, Funda, Marco de la funda, Tapete, Efecto del tapete y Cartas nuevas. Save() reescribe solo las claves que muestra, así que el ini conserva esos valores (los ponen el juego, F2 y el Perfil). En F2 siguen como estaban.
  - Estado: la línea "Voces en español latino" se ve solo con Idioma del juego = Español; si no, se oculta y "Ajustes en" sube un renglón. Se actualiza al cambiar el idioma y cuando F2 cambia el ini.
  - Compilado con compilar.ps1, copiado (cmp). Sin probar.
  - 02:50 (el usuario: "si ocultalas en f2"): main.c, Número de daño y Cartas nuevas con xbox_MenuAddHidden (siguen leídas y guardadas en el ini). Exe copiado (cmp). Sin probar.
- 2026-11-20 02:57 — Revisión final (el usuario: "es la version final ... que todo este traducido"):
  - Configuración: script que cruza cada fila, su .tip y cada valor contra los diccionarios T/V: todo en 6 idiomas. Faltaba "Monitor" (agregado; Moniteur en francés). La ayuda de Voces decía "90 o 120 fps": ahora "más de 60 fps" en los 6.
  - F2 / Perfil: script que cruza filas, valores y cada f2_traducir("...") contra k_texts de f2_idiomas.c: solo faltaba "Monitor" (agregado); Jinzo y Buster Blader son nombres propios. Los textos con tabla propia (buscador, premios, cadena, cartas, Nueva, Salir a Windows, ¿Salir a Windows?) tienen los 6 idiomas.
  - Build limpio de los archivos de la sesión: sin errores; solo avisos de código sin usar. Recursos nuevos presentes en la carpeta del jugador. registro.txt de las 02:54: sin errores (los texdb "missing" son opcionales del juego, de siempre; tirones solo en cargas).
  - Yu-Gi-Oh.exe 02:57 y Configuración.exe copiados (cmp los dos).
  - Probado por el usuario: el cartel de Salir a Windows (Sí/No con el mouse) funciona bien. Falta ver las dos bocas de Lumis y Umbra.
- 2026-11-20 03:02 — Lumis y Umbra: la "segunda boca" no existe (el usuario: sigue moviéndose una sola boca, hable quien hable).
  - Leído del xbe: recortes de los retratos 19/46/73 (8 sprites): 1-2 ojos de la cara 1 (40x56), 3-4 boca (48x40 / 72x64), 6-7 = 32x56. La 4.ª pista usa los sprites 6/7 con duraciones 1-3-6-1 como el parpadeo, y el retrato 73 la corre con el bit 0 del parpadeo: son los ojos de la segunda cara. El juego original tiene boca animada solo para una cara.
  - voces.c: deshecho el cambio de 03:2x (la voz movía los ojos de la segunda cara); de nuevo solo bit 1 / slot 1. Exe 03:02 copiado (cmp).
  - Una boca para la otra cara pediría dibujarla (sprites nuevos en la textura del retrato y comandos nuevos): no hecho; queda a decisión del usuario.
- 2026-11-20 03:16 — Boca nueva para Umbra (el usuario: "le sumamos una boca usando como base las que ya estan en el juego"; aprobó la muestra):
  - Texturas: 98 = E15FAC8B3DB60129 (guion 19), 28 = 3C411B615CD07145 y 101 = E9F0B1A8562D59C2 (guiones 46/73, mismas posiciones). Marco: D (volcado) rotado 270 = marco de los rects; traspuesto = T derecho (cara T (0,0)-(512,408); pieza en T rect (y0,x0,y1,x1), dibujada en (@4,@6) del comando).
  - tools/boca_umbra.py: con sus propios dientes, la mandíbula baja (entreabierta / abierta) hacia el mentón sin cortes; hueco con borde negro, rojo oscuro y lengua como la boca abierta de Lumis; la franja de dientes del demonio abre al 60 %. Piezas en la esquina libre de T (300,410) y (386,410), 82x58 (19: 72x58→72x54). Escribe la mejorada (caras/) y el volcado (dump/, de donde el pack toma el alfa). Originales en respaldos/2026-11-20_boca_umbra. Muestras en modding/muestras/boca_umbra_*.png.
  - tools/parchar_pack.py (nuevo): arma un pack chico con el mismo camino (texturas_pack.py + bc3pack.exe) y copia esas entradas en su lugar dentro del pack grande (mismo tamaño, niveles y flags; FLAG_BC3 = 0x10); bytes viejos en respaldos/pack_textures_bc3/*.bin (--deshacer los devuelve). Aplicado a modding/texturas/textures_bc3.pack, enlace duro con Mods-Extras\texturas del jugador (sigue enlazado). Los packs de idioma no tienen estas texturas.
  - voces.c boca_umbra() (en voces_init, antes de que cargue un retrato): a los guiones 19/46/73 les copia rects (8→10) y comandos al heap del juego y agrega una 5.ª pista: control bit 3 (slot 3), bucle reposo (sprite 5, vacío) / media (8) / abierta (9) / media con los pasos de la boca de Lumis (1,1,4,3); la cuenta de pistas 4→5. El cargador (sub_00018290) crea los objetos de los sprites que alcanza cada bucle, así 8 y 9 existen. sub_00018790: el bit 3 sigue al bit 1 (el "hablando" del juego): las dos bocas se mueven juntas; con voz, la de Umbra toma la misma forma que la de Lumis. Log "[VOZ] boca de Umbra en el guion N".
  - Sin el pack (F6) las piezas son transparentes en la textura original: Umbra queda con la boca cerrada como siempre.
  - Exe 03:16 copiado (cmp). Sin probar.
- 2026-11-20 03:22 — Cada boca con su voz, y las máscaras quietas (el usuario: "separamos los dialogos por personaje incluyendo ellos y hicimos las voces por separado (menos las que estan juntas) y la mascara que tienen que es media cara y media boca no se mueve nunca"):
  - modding/dialogos/guion.json: las 19 frases de Lumis & Umbra (678-696) van por partes {Lumis} / {Umbra} / {Ambos}; doblaje_elevenlabs.py las juntó con ffmpeg concat sin guardar dónde empieza cada parte.
  - tools/voces_quien.py (nuevo): busca los cortes en N.env (las pausas de 120 ms o más; elige las K-1 que mejor juntan pausa larga + posición según el largo del texto de cada parte) y escribe N.quien (1 byte cada 20 ms: 1 Lumis, 2 Umbra, 3 los dos) en voces_latino, x1.5 y x2 (mismos cortes escalados). Ej.: 678 Umbra 0 s / Lumis 1,84 s / Ambos 6,58 s. Copiados a Mods-Extras\voces (57 archivos); armar_paquete.ps1 copia la carpeta entera.
  - voces.c: env_load lee también N.quien; voz_quien(). Con voz: en la parte de Umbra la boca de Lumis queda cerrada, en la de Lumis la de Umbra; {Ambos} o sin archivo: las dos. Sin voz siguen juntas (el texto no dice quién habla).
  - tools/boca_umbra.py: sacada la apertura de la franja del demonio; la mandíbula termina en el borde de la máscara (x 340 / 336). Texturas regeneradas desde los originales de respaldos y pack re-parchado (el .bin de respaldo sigue siendo el original).
  - Exe 03:22 copiado (cmp). Sin probar.
- 2026-11-20 03:4x — Usuario: con Ganga de arrancada (k 792, Snatch Steal) "se buguean las texturas" (el juego no se frena). Leído registro 03:31 + 6 capturas (Pictures\Screenshots 03:23-03:29):
  - Registro sin errores. [CADENA] 15918: 0318/144 (la Ganga); [CAMPO]: el monstruo robado (k 925, imagen 5093) pasa de j1 casilla 0 a j0 casilla 1 y la casilla del rival queda vacía; después el rival coloca k 100 ahí; +1000 al rival en su Fase de Espera. Los datos del juego están bien: es el dibujo.
  - Capturas: 03:26 íconos ⊕ (equipo) del tamaño de una carta sobre la casilla 2 de monstruos y la 1 de mágicas; 03:29 una carta en blanco con ▲ en la casilla de mágicas donde estaba la Ganga. La carta azul del rival es Bestia Enmascarada (ritual, misma ilustración que su funda): normal.
  - Descartado: masc_draw (560 lotes salteados, fijo desde antes del duelo); la funda del rival (va por fila de pantalla, no toca caras).
  - Sin causa en el código todavía. Pedido al usuario: Ctrl+F9 con la carta rara en pantalla ([DUMP] tex0 hash de cada lote) para saber qué textura dibuja ese lugar. Sin cambios en el exe.
  - Usuario: no puede repetirlo a propósito (1 carta en 40). Releído: las 3 capturas de 03:26 caen en las 3 pausas "[d3d11] paused" de justo después de [CAMPO] j0 casilla 1 = k 925 (el robado ya está en tu casilla): el ⊕ se dibuja donde debería verse la carta robada. Es del dibujo. El ▲ de 03:29 es el emblema normal de casilla de mágica vacía (el rival tiene ▼).
  - Volcado automático (sin causa todavía): kit `nv2a_pb_dump_next(why)` (nv2a_pb_d3d11.c, lo mismo que Ctrl+F9). cartas.c, en el vigilante [CAMPO]: si un monstruo queda del lado del otro jugador (bit 0x8000 de su k contra el lado: j0 con 0x80 en el byte 1, o j1 sin él), pide el volcado al momento y a +0,5, +2 y +5 s; 2 veces por sesión. Registro: "[d3d11] [DUMP] next frame's batches (flip N): carta k .. del otro jugador en jP casilla Z, +N ms" y una línea por lote con tex0 hash.
  - Exe 03:46 copiado (cmp). Sin probar.
  - Revisado sin jugar: el ⊕ no está en el tablero (duel_m_00..03.igb: d_f00 tiene ▼ en las casillas de mágica = la ▲ de tu lado), ni en las miniaturas (card/mini, solo cartas; 0000.png es un reverso), ni suelto en dump/ (búsqueda por parecido). Es el ícono de "mágica de equipo" del juego; falta saber qué lote lo pone en la casilla: lo dirá el volcado automático.

### 2026-11-20 04:00: el ⊕ de Ganga de arrancada, encontrado y sacado
- El volcado automático saltó en la partida de las 03:52, cuando k 343 pasó a j0 casilla 2 (cadena 0318). El ⊕ NO es una carta. Es el ícono de "equipo" del atlas de íconos 881EBC12D215BFF3 (1024x128, uv 0,375..0,402 x 0,516..0,734).
- Es el único uso en 3D de ese atlas: un grupo de unos 30 cuadrados, con los ocultos en tamaño 0. El de la carta equipada mide ±1,203 a y 0 en el marco de la propia carta, entre su frente (+0,02) y su dorso (-0,02). Acá se veía encima de la carta boca abajo (captura de las 03:52).
- Arreglo en el kit (nv2a_pb_d3d11.c, antes de masc_draw): en el duelo (con la división de fundas activa) no se dibujan los lotes 3D con ese hash. Una vez por sesión escribe "[EQUIPO] marca de equipo fuera (depth/func/mask/blend)", para saber por qué se veía.
- Exe de las 03:59 copiado a Yu-Gi-Oh.exe (cmp iguales). Sin probar. El volcado automático de cartas.c queda puesto (2 por sesión).
- 2026-11-20: el usuario lo probó y el ⊕ ya no aparece, pero el volcado automático congeló el juego. Lo apagué en cartas.c (`if (0 && ...)`, el código queda). Exe nuevo copiado (cmp iguales).

### 2026-11-20 04:04: limpieza de cosas de desarrollador (el usuario: "el juego está casi terminado")
- cartas.c: se borró el volcado automático de texturas. El registro [CAMPO] de casillas queda: son pocas líneas.
- Kit (nv2a_pb_d3d11.c): F10 (registro de métodos), F7 (congelar) y Ctrl+F9 (volcado) solo funcionan con RECOMP_DEV. Siguen activos F6 (texturas mejoradas/originales), F11, Alt+Enter y Pausa.
- Revisado sin cambios: "Volcar texturas" ya estaba oculta en F2 (xbox_MenuHide) y no está en Configuración. buscar_cursor.c solo corre con YGO_SCAN. La velocidad Ctrl+F8 solo con RDR_VELOCIDAD. El registro.txt normal es liviano (lo que pesaba eran los volcados) y queda para soporte.
- Exe de las 04:04 copiado (cmp iguales). Sin probar.

### 2026-11-20 04:5x: Ganga de arrancada, la carta robada sigue dibujada del lado del rival
- Usuario: "la carta se sigue viendo en frente cuando ya pasó para mi lado del campo y eso genera más errores". El ⊕ era un síntoma: se veía en tu casilla porque ahí la carta no se dibuja (el ícono queda entre sus caras).
- Registro 04:37: [CAMPO] k 426 pasa de j1 casilla 0 a j0 casilla 1 (AA 81 = dueño el rival) y la casilla del rival queda vacía. Los datos del juego están bien; el objeto que dibuja la carta no se mueve. Juego a 165 FPS ([FPS] los del monitor), AUTO activo, objetivo elegido con el ratón. Ninguna de nuestras rutinas toca el cambio de control (revisados auto.c, carta_as.c, los alias de cartas.c). Sin causa todavía.
- Diagnóstico (cartas.c, campo_escanear/campo_buscar en el vigilante [CAMPO]): cuando un monstruo queda en la casilla del otro jugador, busca en la RAM del juego cada u16 igual a su imagen y escribe dirección + 48 bytes, al momento, a +2 s y a +6 s (2 veces por sesión, máximo 80 lugares). Solo lee memoria, sin volcado de texturas. Registro: "[CAMPO] buscar imagen N (...)".
- Exe copiado a Yu-Gi-Oh.exe (cmp iguales). Sin probar.
- 04:5x — Pasó de nuevo (captura: la carta robada boca arriba en la casilla del rival; tu casilla 3 vacía). Registro 04:54 (guardado en ygo-apex/runs/registro_ganga_0454.txt): cadena 0318 → AUTO saltea la pregunta → [CAMPO] k 351 pasa de j1 casilla 3 a j0 casilla 3 (bytes 8-9 "2A 00" iguales: parece el número de objeto). La búsqueda por imagen dio solo ruido (0x1110 aparece en el código).
  - Descartado leyendo el código: AUTO (sub_000BF9C0 solo consulta, devuelve 0 como el original sin cartas); cadena.c (pasos de largo; el salto del panel no toca el campo); los alias (orden 01).
  - Órdenes del juego: sub_000811B0 las reparte por MEM32(0x65F948)&0xFFF con la tabla 0x8150C (174). Nuestras: 01 (alias), 54 (Isaac), 90 (Carta As). Tabla armada con un script (scratch /tmp/ord.py).
  - Diagnóstico nuevo: cadena.c `ordenes()` escribe "[ORDEN] cuadro: NN jP: 16 bytes" en cada cambio de 0x65F948 durante el duelo (tope 6000). La búsqueda [CAMPO] ahora busca la palabra de la casilla (k con su dueño) desde 0x650000 (sin el código).
  - Exe 04:59 copiado (cmp iguales). Sin probar.
- 05:0x — **REGLA DEL USUARIO: no pedir que juegue muchos duelos (10) hasta que salga una carta.** "Juego 10 duelos mientras no hacés nada"; en cada pasada se puede forzar que la carta salga. Para probar una carta: hacer que salga sí o sí en el primer duelo.
  - Prueba TEMPORAL (cartas.c `prueba_ganga`, en el vigilante [CAMPO]): Ganga de arrancada (k 792) va a ser tu primera carta robada después del reparto. Antes del reparto saca una foto de tu Deck (0x65D9FC, cuenta u8 0x65D0A9). Después compara qué punta falta (eso dice de qué lado roba el juego) y cambia la Ganga a esa punta. Solo cambia el orden del Deck. Registro: "[PRUEBA] ...". **Sacarla cuando quede arreglado.**
  - Exe 05:02 copiado (cmp iguales). Sin probar.
- 05:1x — Registro 05:05 (guardado en ygo-apex/runs/registro_ganga_ordenes.txt). La prueba funcionó ("[PRUEBA] ... roba del lugar 0": el juego roba del lugar 0 del Deck). Órdenes: 91 → 4D → 08 → 60 (equipo) → **5F j1** (`5F 80 01 00 00 03`: de j1 casilla 0 a j0 casilla 3). A 60 FPS: los FPS no son la causa.
  - **Camino leído (desensamblado del xbe):** orden 0x5F = sub_000CB340.
    - Paso 0: cursor.
    - Paso 1: arma los descriptores (jugador | fila*10 | columna<<6 | posición<<14) y llama a sub_00046B30(&desde, &hasta). Esa llama a sub_0006C430, que carga el controlador MoveCard [0x658514] en el estado 9.
    - Paso 2: espera a que el controlador termine (sub_00046A10 / sub_0006BC80), copia la casilla (sub_0007E2D0), borra la vieja y llama a sub_00088970.
  - El controlador (sub_0006D330, switch de 0x3F estados) llama en el estado 9 a **sub_00074BE0** sobre [[0x656F8C]+0x2C] (tabla de 0x60 dibujos en +0xC; dibujo +0x18 jugador, +0x1C tipo, 1 = casilla de monstruo, +0x20 columna).
  - **sub_00074BE0 rechaza el movimiento (al 0, no mueve nada)** si no hay dibujo en el origen o si el destino ya está ocupado. La orden 0x5F no mira el resultado y copia los datos igual: los datos se mueven y el dibujo queda. Así se explican la carta en la casilla del rival y que otra carta puesta ahí después no se vea (el destino está ocupado).
  - Textos de depuración del juego en 0x42A3C3..0x42A578 (MoveCard From/To, SwapCard, RemoveCard...); los escribe sub_0006BEB0 en [0x658514].
  - Diagnóstico: envoltorio de sub_00074BE0 (rename_pads + cartas.c). Escribe "[MOVER] jP tipo K col C -> ...: origen nodo N / SIN NODO, destino libre / OCUPADO -> movida / RECHAZADA" y, si la rechaza, la tabla entera de dibujos. Tope 300 líneas.
  - Falta: un duelo con la Ganga, que ya sale sola (prueba_ganga), para saber cuál de las dos condiciones falla y arreglar esa.
  - Exe 05:14 copiado (cmp iguales). Sin probar.

### 2026-11-20 05:20: Ganga de arrancada, arreglo
- Probado por el usuario (05:18, captura): el robo funcionó y la carta pasó a su lado. Ese monstruo estaba boca arriba en ATAQUE: "[MOVER] j1 tipo 1 col 0 -> j0 tipo 1 col 0: origen nodo 6, destino libre -> movida". Las dos veces que falló (04:54 y 05:05) el monstruo estaba boca arriba en DEFENSA (byte 10 de la casilla = 03).
- La orden 0x5F arma el descriptor de origen con el bit de defensa borrado (bit 14), así que no busca al monstruo en defensa donde está. sub_00074BE0 lo rechaza y no mueve nada, y la orden copia los datos igual.
- **Arreglo** (cartas.c, envoltorio de sub_00074BE0, rename_pads):
  - Mientras corre la orden 0x5F, origen y destino salen de la orden misma: u8 0x65F94A/B (jugador y casilla de origen) y 0x65F94C/D (de destino). Registro: "[MOVER] cambio de control: ... pasa a ...".
  - Si el juego lo rechaza igual y hay un dibujo viejo en la casilla de destino, que en los datos está vacía, ese dibujo se saca de las búsquedas (tipo 0x7F) y el movimiento se hace de nuevo. Registro: "[MOVER] ... dibujo viejo ..." y la tabla de dibujos.
- Marca de equipo ⊕ (pedido del usuario: "no se ve la marca"): vuelve a dibujarse; se quitó el salteo del kit (nv2a_pb_d3d11.c). Solo se veía mal porque la carta no estaba.
- Sacado: la Ganga forzada (prueba_ganga), el registro [ORDEN] (cadena.c), la búsqueda en memoria y el registro [CAMPO] de casillas (cartas.c).
- Exe 05:20 copiado (cmp iguales). Probado: robo en ataque (05:18). Sin probar: robo en defensa y la marca devuelta.
- 05:2x — Usuario: "pasó de nuevo". Registro 05:25 (guardado en ygo-apex/runs/registro_ganga_falla2.txt): Ganga contra j1 casilla 4, sin ninguna línea [MOVER]. O sea, sub_00074BE0 no rechazó durante la 0x5F con los argumentos de la orden, o no la llamaron. El arreglo de las 05:20 no alcanza.
  - Diagnóstico completo (DIAG en cartas.c; rename_pads suma sub_000CB340, sub_0006C430 y sub_0006D330). Registra:
    - cada paso de la orden 0x5F, con las dos casillas (16 bytes);
    - los argumentos y el resultado de sub_0006C430 y el estado del controlador;
    - cada cambio de estado del controlador (sub_0006D330);
    - cada sub_00074BE0 durante la 0x5F, con la tabla de dibujos (incluye +0xC0/+0xC1).
  - Vuelve la Ganga forzada (prueba_ganga) para que alcance con un duelo.
  - Exe 05:28 copiado (cmp iguales). Sin probar.
- 05:3x — **REGLAS DEL USUARIO (hoy):**
  1. No pedir muchos duelos: forzar la carta.
  2. No decir "arreglado" sin pruebas (el 05:20 lo dije y falló).
  3. "Último duelo" quiere decir que no hay más pruebas: sacar todo de ese registro y arreglar sin pedir otra.
  4. No sacar los diagnósticos antes de tener la prueba.
- **MIS ERRORES DE HOY (Ganga de arrancada), para no repetirlos:**
  1. 04:00: tapé el ⊕ creyendo que era el error; era la marca de equipo del juego, visible porque la carta no se había movido. Ataqué el síntoma.
  2. Dejé encendido el volcado automático de texturas y le congeló el juego.
  3. Le pedí que jugara muchos duelos hasta que saliera la carta, en vez de forzarla.
  4. 04:4x: la búsqueda en memoria por imagen solo dio ruido (el valor aparecía en el código); la tendría que haber pensado antes.
  5. 05:20: di por arreglado algo sin pruebas (solo había visto el caso de ataque).
  6. 05:20: saqué la Ganga forzada y los registros dando el arreglo por hecho. Falló, y tuvo que jugar esperando a que le tocara la carta, sin registros útiles.
  7. 05:3x: leí el registro viejo como si fuera de un duelo nuevo que todavía no había jugado.

### 2026-11-20 05:4x: Ganga de arrancada, CAUSA REAL encontrada (error del recompilador)
- Registro 05:33 (guardado en ygo-apex/runs/registro_ganga_final.txt): la orden 5F va de j1 casilla 1 a j0 casilla 2, pero "[MOVER] 6C430 desde (1 1 1 2 433) hasta (0 1 **0** 2 906) -> -1". El destino llegó con la **columna 0** en vez de 2. Esa casilla tenía una carta (906), así que el juego rechazó el movimiento (-1), el controlador no arrancó y los datos se copiaron igual.
- Por qué: sub_000CB340 calcula la columna de destino (z % 5) en EDX y la guarda pasando por un `imul cl` de 8 bits (AX = AL * CL, no toca EDX). El recompilador (xboxrecomp-apex tools/recomp/lifter.py `_lift_imul`) lo traducía como el imul de 32 bits de un operando (EDX:EAX = EAX * ECX), que ponía EDX en 0. Todo monstruo robado iba a la columna 0. Solo funcionó a las 05:18 porque el destino era la casilla 0.
- La teoría de las 05:20 (bit de defensa) era FALSA. Se sacaron ese "arreglo" y la reparación de dibujos viejos.
- Arreglo de raíz:
  - lifter.py `_narrow_muldiv`: mul/imul/div/idiv de un operando de 8 y 16 bits, solo sobre AX o DX:AX, como el x86. Probado aparte contra la semántica x86: 36.605 casos, 0 errores (clang, scratch).
  - En gen/ se reescribieron los 54 `imul r/m8` ya generados (recomp_0014, 0018, 0092, 0094, 0097, 0098, 0139), con el comentario "imul r/m8: AX = AL * src". Los mul/div de 8 y 16 bits ya generados: no había ninguno.
  - Esto corrige también las otras 53 multiplicaciones de 8 bits del juego.
- Quedan hasta verlo funcionar en su partida: la Ganga forzada (prueba_ganga) y los registros [MOVER] (DIAG en cartas.c; rename_pads CB340/6C430/6D330/74BE0).
- Exe 05:38 copiado (cmp iguales). Falta verlo en una partida.
- 05:4x — **CONFIRMADO por el usuario** ("ahora parece que sí") y en su registro de las 05:43: "6C430 desde (1 1 0 ..) hasta (0 1 1 ..) -> 1, estado 9", "74BE0 j1 tipo 1 col 0 -> j0 tipo 1 col 1: movida", y el controlador pasa por 10 → 59 → 60 → 0. Con la columna correcta, el dibujo se mueve.
  - Sacado: la Ganga forzada (prueba_ganga) y los envoltorios de diagnóstico de sub_000CB340, sub_0006C430, sub_0006D330 y sub_00074BE0 (fuera de rename_pads; en gen/ vuelven a sus nombres).
  - Queda: la corrección del traductor (lifter.py) y las 54 imul de 8 bits corregidas en gen/.
  - Exe 05:45 copiado (cmp iguales; el exe ya no tiene los textos [MOVER] ni [PRUEBA]).

### 2026-11-20 06:0x: error de traducción (resumen) y todas las cartas que roban monstruos
- **ERROR MÍO AL TRADUCIR EL JUEGO** (está en la guía, GUIA_PORT_XBOX.md 11.1): el recompilador traducía las `mul`/`imul`/`div`/`idiv` de un operando de 8 y 16 bits como las de 32 bits. Pisaban EDX y daban otro resultado. Arreglado en lifter.py (`_narrow_muldiv`) y en las 54 imul de 8 bits de gen/. Las otras 53 también estaban mal; quedan corregidas.
- **Revisado en el código: todas las mágicas y trampas que roban un monstruo (cambio de control).**
  - 13 resolvedores de efectos llaman al cambio de control sub_000C6A80, que encola la **orden 0x5F**: sub_0009B5B0, 0009BA10, 0009DD70, 0009E4B0, 0009E920, 000AC3B0, 000AD8C0, 000AF020, 000AFD50, 000B3730, 000B48F0, 000B4A70, 000B6BE0.
  - La devolución del control en el turno (orden 02, sub_000D0270) también encola la **0x5F**.
  - La 0x5F era la del `imul cl` mal traducido; arreglada y **confirmada en partida** (05:43, columna correcta, "movida").
  - Los intercambios de monstruos (SwapCard, sub_0006CDB0 por sub_00047190) usan las órdenes 0x61 (sub_000CB560) y 0x93 (sub_000CE5A0). Esas calculan con `imul` de 32 bits de 3 operandos, que se traducían bien: no tenían el error.
  - Otras órdenes que mueven cartas y tenían el mismo `imul` de 8 bits, ahora corregidas: 0x4D (activación), 0x52, 0x57 (destierro), 0x58, 0x5C, 0x5D, 0x5E, 0x45, 0x81, 0x82, más el cursor del campo (sub_00086F50) y sub_00088B80.

## 2026-11-20 16:0x — "Nueva" que no salía en el premio
- Usuario: ganó cartas que no tenía y no salió la marca "Nueva" en el premio (en la Biblioteca sí). "Bajó el número de cartas que me puede dar": es el "faltan N" del cartel de premios del rival (40 → 38 → 35 → 33), bajó porque esas cartas ya las tiene. Está bien.
- Registro 15:52 (7 premios): la marca salió en 4 y faltó en 3 (15:33 k 500, 15:48 k 383/992/254, 15:52 k 596/584). Las huellas estaban (1137 leídas).
- Causa (del registro): el sorteo (sub_0008CBB0) pasa antes de que el rival hable; las cartas se dibujan recién al cerrar su globo. La lista de huellas se vaciaba a los 3 s **contados desde el sorteo** si no se había visto ninguna carta. En los 4 que anduvieron, las cartas aparecieron ~333–400 cuadros después del sorteo; en los 3 que fallaron, 637, 659 y 1191 (el usuario leyó el globo más tiempo).
- Arreglo (premios_extra.c, marcas_premio_tick): los 3 s cuentan solo después de haber visto las cartas (s_marca_alguna); si nunca se vieron, la lista espera hasta 5 min (un premio nuevo la vacía igual). Registro nuevo: "[PREMIOS] lista de marcas vaciada (...)".
- Compilado rc=0 (15:56), copiado a Yu-Gi-Oh.exe (cmp IGUALES). **Sin probar:** se ve en el próximo premio con una carta nueva, sobre todo si se tarda en cerrar el globo del rival.

## 2026-11-20 16:3x — Cuelgue a los 25 min (registro 16:25): memoria contigua llena otra vez
- Registro: tras el duelo 9, al cargar m924.igb de una batalla, `[CONTIG] arena exhausted (131072 requested; 64324 of 65408 KB in use)`, luego `[ICALL] Failed to resolve VA 0x00000000` → colgado; el usuario cerró la ventana (why=window).
- "[MEMORIA] fin del duelo N" sube ~4,7 MB por duelo sin bajar (16064 → 53688 KB en 9 duelos). "[CONTIG] vivo": crecen siempre de a 2 por duelo los bloques de 900 KB (0xE1000) y 452 KB (0x71000, pedido 0x70800), más 1 por duelo de 7 tamaños chicos (0x63000, 0x2A000, 0x24000, 0x1B000, 0x19000, 0x16000, 0x14000).
- Los de 452 KB se piden justo al crear los visores de las mascotas (mascota.c crear(): sub_0005B7E0, t=28.4 s, líneas junto a "[MASCOTA] 57D60 visor ..."). Los visores propios de cada duelo nunca se destruyen; el arreglo del 11-18 solo suelta el modelo. Falta: destruirlos como el cierre de la escena destruye los de batalla (+0x74/+0x78) y confirmar con [MEMORIA] que deja de subir. Sin cambios de código todavía.
- En este registro el cartel "Nueva" se puso en los 3 premios ("marca Nueva sobre 6 carta(s)" y "vaciada (cartas ya vistas)").
- **Arreglo 16:34 (mascota.c + rename_pads.py):** envuelto `sub_0005B9C0` (destructor del visor de batalla). El juego destruye sus visores dentro del desarme (sub_00044E60, 0x45025..0x4506E): sub_000570C0(d0)+sub_00192320(d0), sub_0005B9C0(visor)+sub_00192320(visor). sub_0005B9C0 suelta [v+0xE0]/[v+0xE4] (las imágenes 480x480) y su base sub_0005B680 las 8 capas, [v+0x30] y [v+0x34] (grupo y nodo del modelo). Justo después de que el juego destruye su visor de +0x78, se destruyen igual los de las dos mascotas (mismo momento del desarme). soltar_nodos queda sin efecto ese caso; si el juego no tenía visor en +0x78 sigue el camino viejo (registro "camino viejo"). Registro: "[MASCOTA] visores de las mascotas destruidos con los del juego: texturas X -> Y KB". Exe 16:34:02 copiado (cmp iguales). Sin probar: mirar que "[MEMORIA] fin del duelo N" ya no suba ~4,7 MB por duelo y que no haya crash al terminar el duelo.
- Por qué 64 MB en PC: el juego es código de Xbox y direcciona la memoria de texturas como la Xbox (ventana 0x80000000 de 64 MB, kit: XBOX_CONTIG_SIZE y NV2A_RT_CONTIG_SIZE, compartido con RDR). Agrandarla es tocar el mapa de memoria del kit y la GPU; el juego original nunca la llena (en Xbox real es la misma): la llenaban las mascotas.
- **Verificado sin jugar (16:3x):** en el registro 16:25 el bloque de 900 KB se pide entre la carga del visor de la mascota 0 y la de la 1 (t=402,3 / 532,1 / 612,0 / 833,5), y el de 452 KB justo antes de "[MASCOTA] 0: monster..." → salen del armado de los visores de las mascotas. sub_00045750 = sub_00044E60 y recién después [0x656F88]=0 → el envoltorio ve la escena registrada. Las otras llamadas a sub_0005B9C0 (0x435D3, 0x43926) son de otras escenas y no cumplen la condición. Los bloques chicos (0x63000 etc., 1 por duelo) aparecen mientras cargan juntos el modelo de la mascota y los fe0xx.igb del campo: NO se sabe de quién son. El nodo del modelo quedaba con 1 referencia tras soltarlo ("3 -> 1").
- **Seguro (exe 16:37:41, cmp iguales):** crear() no arma las mascotas si al empezar el duelo la memoria contigua ya pasa 40 MB (registro "[MASCOTA] memoria de texturas alta ...: este duelo sin mascotas"). Sin probar.
- **Análisis de los bloques chicos (16:4x):** los 7 tamaños (401880, 171768, 144720, 106692, 100164, 90000, 78132; todos múltiplos de 12) no aparecen como tamaño ni como cantidad de vértices (pasos 4..64) en m716/m081/m000, duel_m_0x ni fe0xx.igb. Pista: la mascota 1 cambió m081→m000 en el duelo 6 y los 7 siguieron igual; lo único igual en los 9 duelos fue la mascota 0 (m716) y lo que carga el juego. En contra: sub_0001ADF0 busca el modelo por ruta (sub_00019D20) y si sigue cargado devuelve la MISMA manija con uso+1 ([res+0xA8], sub_0001EF50); cada duelo dio manija nueva → el anterior se soltó entero (sub_00019700 baja el uso y en 0 suelta raíz y recursos, sub_0001F940). No está demostrado de quién son.
- Diagnóstico nuevo: "[MASCOTA] soltar los modelos: texturas X -> Y KB" al final de cada duelo. Exe 16:43:57 copiado (cmp iguales). Con el seguro de 40 MB, si fueran de las mascotas también dejan de crecer.
- **Última protección (exe 16:46:50, cmp iguales):** al terminar un duelo (después del guardado), si la memoria contigua pasa 50 MB, un aviso de Windows (una vez por sesión) pide guardar, cerrar y reabrir antes del próximo duelo; registro "[MEMORIA] aviso al usuario". Margen: dentro de un duelo sube hasta ~9 MB sobre el valor del final. El usuario: "si el juego se llega a colgar otra vez fue un fracaso ... único aviso". Sin probar.
- **¿Subir el límite de 64 MB? (16:5x, leído):** el juego no lo impide (solo quita el bit alto con 0x7FFFFFFF; el único 0x4000000 de memoria es sub_00020980: si la máquina tiene más de 64 MB ajusta su cuenta, como en los kits de desarrollo). El kit lo asume en ~15 lugares (kernel.h XBOX_CONTIG_SIZE, nv2a_backend_common.h NV2A_RT_CONTIG_SIZE, kernel_bridge.c 8345 "0x84000000", fb_present.c, apertura teselada, memoria física compartida con imagen/heap en xbox_memory_layout.c). No se hizo: una fuga con 128 MB solo tarda el doble en colgar; en la Xbox real imagen+heap+texturas entraban en 64 MB, así que el juego original tiene memoria de sobra y el problema es la fuga de los mods.
- **Kit (kernel_bridge.c, MmAllocateContiguousMemoryEx, diagnóstico "quién lo pidió"):** solo guarda direcciones de retorno reales (el byte anterior es una llamada: E8, FF 15/9x, FF 5x, FF Dx); antes entraban tamaños como 0x63000 y no se veía quién pedía los bloques chicos. Exe 16:50:11 copiado (cmp iguales). RDR no recompilado.

## 2026-11-20 19:3x — Registro de 10 duelos (19:30, exe 16:50, sin cuelgue)
- Visores de las mascotas: destruidos en los 9 finales de duelo, sin crash ("texturas X -> X-2704 KB" cada vez). Soltar los modelos libera ~1,7 MB más. **El arreglo de los visores funciona** (probado).
- Pero la memoria contigua sigue subiendo ~1,9 MB por duelo (fin de duelo: 13140, 15120, 17012, 18852, 20768, 22664, 24580, 26484, 28344 KB). Antes 4,7. Con el seguro de 40 MB y el aviso de 50 no se cuelga, pero sigue habiendo fuga. "[CONTIG] vivo" con retornos reales: solo nombra los cargadores genéricos (0x21749 ← 0x219DD ← 0x20C28 ← 0x11EDAD / 0x3824F2), 4 niveles no alcanzan.
- Lo demás, limpio: 0 FATAL/STRAY/ICALL/out of memory/refused; el asignador del motor no crece después del arranque; guardados 9 al cerrar + 24 con premio, todos "bien"; marca "Nueva" en los 8 premios; shaders 131/131; las paradas de 0,9 y 1,1 s fueron la ventana en segundo plano; las de ~0,55 s, cargas de 5 MB entre duelos. Archivos faltantes: los texdb opcionales de siempre.
- **Diagnóstico nuevo (kit xbox_memory_layout.c + kernel_bridge.c, mascota.c):** cada bloque contiguo lleva su número de orden (g_contig_seq) y 8 retornos del juego; al final de cada duelo (desde el 3º) "[CONTIG] siguen vivos (tomados en el duelo anterior ...)": lo que se tomó en el duelo anterior y sigue vivo un duelo entero después, agrupado por quién lo pidió. Exe 19:34:39 copiado (cmp iguales). RDR no recompilado.
- **Medición por tramos (registro 19:30):** texturas justo antes de crear las mascotas, por duelo: 8848, 10768, 12664, 14552, 16456, 18364, 20260, 22140, 24028 KB (+1,9 MB cada vez). Dentro del duelo, desde ahí hasta el final (ya soltadas las mascotas): +4,3 MB; del final al próximo duelo el juego libera 2,4 MB. Las mascotas devuelven al final lo mismo que toman (visores 2704 + modelos 1708 KB). Lo que queda se toma durante el duelo; no se sabe si es del juego o efecto de un mod. El kit no ignora ninguna liberación (nunca aparece "not the start of a block, ignored"). Falta el reporte "[CONTIG] siguen vivos" (exe 19:34) para nombrar al dueño. El usuario: tiene que poder jugar 5 horas sin perder mascotas ni reiniciar (los seguros de 40/50 MB no son la solución).
- **Registro completo para la próxima partida (exe 19:43:31, cmp iguales):** el usuario pidió anotar todo ("hasta la dirección del presidente") para no volver a pedir duelos.
  - Kit (xbox_memory_layout.c): con XBOX_CONTIG_LOG_ALL=1 (main.c lo pone por defecto) una línea por cada bloque de texturas tomado "[CONTIG+] #número t= dirección tamaño, en uso, pide: 8 retornos del juego" y devuelto "[CONTIG-] #número ...". Así cada bloque se sigue de punta a punta.
  - Al final de cada duelo (mascota.c): "[CONTIG] siguen vivos" (desde el 3º), "[MEMORIA] ... heap vivo/devuelto/alcance" (kit xbox_HeapUsageKB) y "[RECURSO] duelo N tabla T #i [NUEVO] ocupada tipo usos obj: ruta" = cada archivo que el juego tiene cargado (tablas en 0x631A1C/20/24/48/60/8C, entradas de 0xB8: ruta +4, tipo +0x84, objetos +0x88/8C/90, ocupada +0xA5, usos +0xA8), marcando los que no estaban al final del duelo anterior. Solo lectura.
  - Sin probar. RDR no recompilado (el kit cambió: MmAllocateContiguousMemoryEx guarda 8 retornos).

## 2026-11-20 20:0x — Registro 19:54 (exe 19:43, 2 duelos terminados): de dónde sale la fuga
- Bloque por bloque ([CONTIG+]/[CONTIG-]): lo tomado en el duelo 2 que sigue vivo = 1928 KB en 21 bloques (la fuga de ~1,9 MB/duelo), todos a t=431 al arrancar el duelo, mientras se cargan **effect\field_igb\fe007..fe012**: 13 por 0x161C85←0x141AE6←0x377BB2←0x2F235C←0x11EDAD (texturas) y 7 por 0x280470←0x167A88←0x3ECA3F←0x382520←0x3824F2 (vértices).
- [RECURSO]: al final de cada duelo están cargados 18 archivos (duel_m_00..03, fe000..fe016), siempre los mismos, con objetos nuevos: el juego suelta los del duelo anterior. Lo que queda no es el archivo sino partes de fe007..fe012 que algo sigue sosteniendo.
- El objeto de efectos del campo: vtable 0x429B5C, se arma en sub_0005CB60 (desde sub_000451C0), carga la tabla de 21 nombres en 0x5FDFD0 (sub_0005BB20, sub_0001ADF0), toma los nodos en [this+0xD8+i*4] (sub_0005CBF0, +1 referencia), arma [+0x2C4] (sub_0005C9D0) y se destruye en sub_0005C170 (instancias +0x12C x100, archivos por ruta sub_00048D00, [+0x2C4], los 21 nodos). Ningún mod usa estos archivos ni este objeto.
- **Diagnóstico (exe 20:00:36, cmp iguales):** envuelto sub_0005C170 (rename_pads): antes de destruir, "[EFECTOS] se destruye ...: feNNN=nodo/referencias" de los 21, y para fe007..fe012 "[EFECTOS] feNNN nodo X lo guarda Y (objeto Z+off, vtable V)" buscando el puntero en toda la memoria del juego (o "nadie más lo guarda"). Solo lee.
- **Registro 20:09 (3 duelos):** cada nodo fe007..fe012 tiene 2 referencias al destruirse los efectos: el objeto de efectos (+0xF4..+0x108) y la entrada de la tabla de recursos 4 (0x631A60, vtable 0x423CD4, +0x8C); las dos se sueltan. De los 21 archivos fe solo esos 6 piden memoria de texturas al cargar, y sus bloques no vuelven nunca (las mismas cadenas del cargador sí devuelven todo lo de los demás archivos: 1172 de 1277 y 571 de 613). Fuga confirmada: 1916 KB / 21 bloques en el duelo 2 y 1896 KB / 19 en el 1. Heap vivo plano entre los duelos 2 y 3 (18119 KB): no pierde.
- **Arreglo (exe 20:13:40, cmp iguales, sin probar):** en sub_0005C170, fe_conservar(): la entrada de la tabla 4 de cada fe007..fe012 se queda con un uso más (+0xA8, u16, lo que sub_00019600 mira antes de soltar), una sola vez. El duelo siguiente el cargador (sub_0001ADF0 con buscar=1, como lo llama sub_0005BB20) la encuentra por ruta y devuelve el mismo nodo sin leer el archivo: esos ~1,9 MB quedan una vez y no se suman. Registro: "[EFECTOS] feNNN ... se conserva para los duelos siguientes" (duelo 1) y "reaprovechado" (los demás); en [RECURSO] el nodo de fe007..fe012 debe ser el mismo en todos los duelos y "[CONTIG] siguen vivos" debe quedar en ~0 desde el duelo 2. Diagnóstico de [EFECTOS] sigue puesto hasta probarlo.
- **Exe 20:19:29 (cmp iguales, sin probar):** (1) el usuario: "las puertas demoraron más en abrirse" al terminar el duelo. Registro 20:09: ese cuadro tardó 2,5-3 s (antes 0,7 s); era quien_apunta (recorre toda la memoria del juego). Ya respondió, se apaga ((void)quien_apunta en sub_0005C170). (2) el usuario: 2 cartas nuevas (duelo 1: 4744 k696 y 4763 k705, las dos en el cofre 0→1) y "Nueva" sobre una sola. Todos los registros dicen "primera en 0,2" y llegaron a contar 4-5 marcas con 3 cartas: la misma cara se dibuja también en otro lado (caja alrededor de 0) y el último dibujo movía la marca de esa carta a la esquina. Kit nv2a_pb_d3d11.c marca_vista: solo cuenta una caja en pantalla del tamaño de una carta (x0,y0 ≥ 0, ancho ≥ 40, alto ≥ 60). premios_extra.c: "[PREMIOS] Nueva sobre la carta HASH en x0,y0 - x1,y1" una vez por carta, para probar que cada nueva tiene la suya.
- Heap: fin duelo 1 vivo 9928 KB / alcance 15516; fin duelo 2 vivo 18120 / alcance 23844. Mirar si sigue subiendo.

## 2026-11-20 20:28 — "Perfil" con la letra del juego y la funda del rival en la vista de la izquierda
- El usuario (captura): el cartel "Perfil" se veía pegado (franja plana, otra letra), "ponelo como los demás" (Salir a Windows sí lo escribe el juego). Causa: Salir es el texto 24 de las tablas; Perfil era cartel.png + nuestra letra encima. El juego arma el cartel al cambiar de ícono (0x10D50F): sub_0010C6B0 sobre [rueda+0x54] con el texto del ícono (0x10..0x13, 0x15..0x18 según [rueda+0x74+4·sel]), y arma las letras en ese momento. raton.c cartel_perfil (hilo del juego, estado 0): con el puntero en el botón del Perfil (perfil.c g_ygo_cartel_perfil_hasta) apunta ese texto a "Perfil" (6 idiomas) solo para esa llamada y la hace; al salir, la llamada con el texto del ícono. perfil.c: cartel.png queda solo de respaldo si el cartel del juego no se armó en 400 ms; el apagado del ícono elegido sigue igual. Log "[RATON] el cartel dice Perfil (con la letra del juego ...)".
- El usuario: la carta colocada del rival vista en grande a la izquierda (ahí solo salen las del rival; las mías a la derecha) tenía mi funda. El reparto de fundas era solo por fila (arriba de 200 = rival). Kit nv2a_pb_d3d11.c (rev_posiciones/rev_draw): también la columna de cada triángulo; a la izquierda de g_nv2a_reverso_col (px del juego) es del rival en cualquier fila. fundas.c la pone en 150 (la vista derecha está en x 514..587 de 640 según la captura, la izquierda ~53..126; la primera carta de mi campo empieza ~173).
- Exe 20:28:06 copiado (cmp iguales). Sin probar. Trae también lo de 20:13/20:19 (efectos del campo, marcas Nueva, sin el rastreo lento).

## 2026-11-20 20:31 — registro 20:26 (exe 20:19, 2 duelos): la fuga sigue, con otra causa a la vista
- Lo que anduvo: fe007..fe012 se conservaron en el duelo 1 y se reaprovecharon en el 2 (mismos nodos, sus archivos ya no se leen). Heap plano (9927 KB en los dos finales). Las puertas: 616 y 628 ms (antes del rastreo ~0,7 s). "Nueva": una carta nueva, su marca en 86,99 - 214,284 (sobre la carta, ya no en 0,2).
- Lo que no: lo tomado en el duelo 2 y vivo al final = 1924 KB en 21 bloques, mismas dos cadenas (texturas 0x161C85.., vértices 0x280470..) y los MISMOS tamaños que antes (100, 28, 168, 396, 144, 48, 8, 108, 80, 88, 60, 12 KB; 128, 32, 16, 256, 64, 128, 32 KB), a t=80,2 justo donde antes se leía fe007. Sin leer el archivo se piden igual: no los crea la carga sino algo que arma texturas y vértices a partir de esos efectos en cada duelo. El arreglo de 20:13 (conservarlos) queda, pero no alcanzó: falta saber quién los arma.
- Por el código no se puede subir más: las 8 direcciones terminan dentro de llamadas virtuales del motor (sub_00161C30 ← 0x167xxx; sub_001677D0 ← sub_0016A8D0/0016AA40/0016AE80/0016BA10/0017F200, cuyos llamadores son vtables).
- **Exe 20:31:22 (cmp iguales, sin probar):** kernel_bridge.c, con XBOX_CONTIG_LOG_ALL=1: "[CONTIG*] 0xVA cadena: ..." con todas las direcciones de retorno del juego en hasta 4 KB de pila (sin pasar la región de pila comprometida) y el hilo. Con 2 duelos se ve la cadena entera hasta el código del juego que arma esos 21 bloques. Trae también lo de 20:28 (cartel Perfil, funda del rival a la izquierda).
- **Exe 20:34:17 (cmp iguales, sin probar):** el usuario: "pone 5000 llamadas ... para que no tengas excusa". La línea [CONTIG*] ya no tiene tope: todas las direcciones de retorno del juego en toda la pila comprometida del hilo (hasta 1 MB), buffer de 256 KB por línea. Incluye también restos viejos de la pila; las de la llamada real están entre ellas.

## 2026-11-20 20:43 — la fuga de 1,9 MB por duelo era el modelo de TU mascota (registro 20:37, exe 20:34)
- Las cadenas completas ([CONTIG*]) muestran que esos bloques salen del lector de .igb del juego (hilo del cargador: ...0x167F30 ← 0x187420 ← 0x19E600 ← 0x1DE460 ← 0x1DE1D0 ← 0x187ED0 ← 0x1DE4B0 ← 0x199C00 ← 0x19F940 ← 0x1A7AB0 ← 0x2F2790 ← 0x120E95), el mismo de cualquier .igb. Ninguno de los fe*.igb tiene texturas de ese tamaño (son de 8x16 a 32x64; tools/igb.py).
- Al final del duelo 2: soltar la manija de la mascota del rival (m081) libera #1037..#1069 (~1,9 MB); soltar la de TU mascota (m716) no libera ninguno de sus bloques (#1011..#1042, 1904 KB, los que quedan). Mismos tamaños en los registros 20:09, 20:26 y 20:37 aunque el rival cambió (m000, m081): es m716. Parecía "efectos del campo" porque el modelo se termina de leer mientras se abren esos archivos. El arreglo de 20:13 (conservar fe007..fe012) no era la causa; queda (funciona y no molesta).
- **Arreglo (exe 20:43:52, cmp iguales, sin probar):** mascota.c: al terminar el duelo, el modelo de cada lado (no el de Exodia) no se suelta: se guarda (s_guard_h/n). El duelo siguiente, cargar() lo reaprovecha si es el mismo número (pasa a M_CARGA, lista() al toque) en vez de leer otra copia; si es otro, suelta el guardado. Log "[MASCOTA] i: modelo N guardado para el duelo siguiente" / "reaprovechado del duelo anterior". soltar() anota cuánto libera cada manija ("texturas X -> Y KB"). Prueba: desde el duelo 2, "[CONTIG] siguen vivos" ~0 y "texturas en uso" al final de cada duelo plano.
- Del registro: cartel Perfil sin línea "[RATON] el cartel dice Perfil" (no se pasó el puntero por el Perfil, o no anduvo).

## 2026-11-20 20:51 — el juego se cerró en un ataque (exe 20:43): era el reaprovechamiento del modelo; sacado
- Registro 20:48: duelo 1 bien ("modelo 716/81 guardado"); duelo 2 "reaprovechado" los dos, pero la mascota 0 (m716) NUNCA quedó lista (la 1 sí). En un ataque (carga de m918 y be115) el juego se cerró sin línea de error.
- Por qué: m716 es también carta de tu mazo. El juego trae por su cuenta los modelos de los monstruos de los mazos por la misma ruta (sub_0001D5F0 pone "monster/%03d/m%03d.igb" en una cola; sub_0001E040 la atiende: 1 = sub_0001AA20, 0 = sub_0001A4A0, tabla 0x631A50) y se cruza con la carga de la mascota (registro 20:37: tras cargar la mascota aparece otra vez la ruta de m716 junto a m067, m077, m092, m433...). Entre duelos el juego la soltó por su lado y la manija guardada quedó apuntando a otra cosa. Eso explica también la fuga: m081 / m000 no están en el mazo y se liberan enteras; m716 no.
- **Arreglo (exe 20:51:33, cmp iguales, sin probar):** fuera el guardado/reaprovechamiento (mascota.c vuelve a soltar el modelo al final de cada duelo). La mascota se carga desde una copia propia con otro nombre: copia_mascota() copia Game\monster\NNN\mNNN.igb (o el del mod de cartas, Mods-Extras\cartas\archivos\monster\..., si existe) a Game\monster\NNN\mNNNm.igb (una vez; de nuevo si el original cambia) y carga "monster/NNN/mNNNm.igb". El juego nunca usa esa ruta: sin cruce, se suelta entera. Si no se puede copiar, la ruta de siempre. Logs: "[MASCOTA] copia propia del modelo N", "modelo soltado (manija X): texturas A -> B KB" (por manija). Prueba: al soltar la manija de la mascota 0 tienen que volver ~1,9 MB, y "siguen vivos" ~0 desde el duelo 2.

## 2026-11-20 20:57 — PROBADO (registro 20:56, exe 20:51, 2 duelos): la fuga de la mascota está arreglada
- "copia propia del modelo 716" y "81" hechas; las dos mascotas listas en los dos duelos.
- Al soltar la mascota 0 ahora vuelven 1848 KB (15208 -> 13360, y 15112 -> 13264); antes 0.
- Texturas en uso al final: duelo 1 11644 KB, duelo 2 11548 KB (antes subía ~1,9 MB por duelo: 13460 -> 15416). Lo tomado en el duelo 2 que sigue vivo: 28 KB en 2 bloques (antes 1904 KB / 21). Heap plano (9927 / 9928 KB). Sin FATAL/STRAY.
- Cartel Perfil con la letra del juego: anduvo ("[RATON] el cartel dice Perfil ... ícono 5, texto 22"). Funda del rival a la izquierda: sin dato en el registro (lo dice el usuario).
- Diagnósticos pesados todavía puestos (XBOX_CONTIG_LOG_ALL: [CONTIG+]/[CONTIG-]/[CONTIG*] con toda la pila por bloque, [RECURSO], [EFECTOS]); sacarlos o dejarlos lo decide el usuario.

## 2026-11-20 21:00 — cartel Perfil: el texto queda puesto mientras el puntero está encima (exe 21:00:03, cmp iguales, sin probar)
- El usuario: "el cartel no funciona el de perfil". Registro 20:56: la llamada se hizo ("el cartel dice Perfil ... ícono 5, texto 22") pero el cartel siguió con el nombre del ícono: el cartel lee el texto después de la llamada (arma las letras mientras entra) y el texto se devolvía enseguida. raton.c: cartel_perfil_poner apunta el texto del ícono a "Perfil" (6 idiomas) y lo deja así mientras el puntero esté en el botón; al salir, cartel_perfil_restaurar + la llamada con el texto propio. También se restaura si la rueda sale del estado 0. Logs "[RATON] el cartel dice Perfil (...queda puesto...)" / "el cartel vuelve a su texto".
- Mascotas: toda carga de modelo de mascota (las dos, cualquier número, también el cambio en medio del duelo) pasa por cargar() -> copia_mascota(); solo Exodia usa su archivo especial (no es carta de mazo). El Perfil no carga modelos 3D (muestra la tira de imágenes); cambiar la mascota ahí solo cambia el ajuste, el modelo se carga al empezar el duelo.

## 2026-11-20 21:06 — juego trancado, funda a la derecha, cartel Perfil (exe 21:06:04, cmp iguales, sin probar)
- "El juego va como trancado": XBOX_CONTIG_LOG_ALL pasa a "0" en los ajustes portables (main.c): fuera las líneas [CONTIG+]/[CONTIG-]/[CONTIG*] (esta última recorría toda la pila en cada bloque). Quedan los resúmenes por duelo ([MEMORIA], "siguen vivos", [RECURSO], [EFECTOS]), que son una vez por duelo.
- Funda (captura del usuario): la carta boca abajo del rival vista en grande está a la DERECHA (x 512..588 de 640) y salía mi funda; lo de 20:28 era solo la izquierda. Tus propias cartas boca abajo se te muestran de frente, así que un reverso en cualquiera de las dos vistas grandes es del rival: kit g_nv2a_reverso_col2, a la derecha de 495 px también es del rival (fundas.c COLUMNA_RIVAL2; mi mazo termina ~477). Log "[REVERSO] reverso en la vista grande (x N): funda del rival" (cada 10 s como mucho). Si esa vista no se dibuja como lista de triángulos (rev_posiciones), la regla no la alcanza: el log lo dice.
- Cartel Perfil (el usuario: "se ve como se dibuja la letra pero queda la de otro boton"): el cartel no relee el texto si se le pide el mismo número que ya muestra. Ahora "Perfil" va en el texto 20 (0x14, que no es de ningún ícono) y se pide ese; al salir, el del ícono.

## 2026-11-20 21:12 — cartel Perfil interceptado; diagnóstico del parpadeo de la funda (exe 21:12:16, cmp iguales, sin probar)
- Registro 21:08: la funda del rival a la derecha ya anda ("reverso en la vista grande (x 677)"). Los cuadros de 0,9-2,1 s en el menú fueron con la ventana en segundo plano (capturas). La carga del duelo: 289 ms, igual que antes (no se ve demora extra en el registro).
- Cartel Perfil (el usuario: "solo se ve la animación como que quiere iniciar"): el juego vuelve a escribir el texto del ícono en el cartel por su cuenta (sub_0010CDE0, desde el cuadro de la rueda y desde 0xF9A20 / 0xFBB40) y pisaba "Perfil". sub_0010C6B0 envuelto (rename_pads): mientras "Perfil" está puesto, toda llamada pide el texto 20. Si ninguna tabla tiene texto 20, queda el cartel de respaldo (log). Log "[RATON] el juego quiso volver a poner el texto N en el cartel: queda Perfil".
- Funda que "tintinea" al usar Dúo delictivo (el usuario: a la derecha; viene de antes): sin datos todavía. Kit rev_draw: "[REVERSO] lote de N indices, por triangulo / por tijera, x .. y .. (corte, columnas)" una vez por segundo como mucho, para ver cómo se reparte ese lote.

## 2026-11-20 21:21 — parpadeo de la funda con Dúo delictivo (diagnóstico)
- Registro 21:18: Perfil anda en el registro ("el cartel dice Perfil", "el juego quiso volver a poner el texto 16: queda Perfil", "vuelve a su texto").
- Dúo delictivo (xbe: el rival descarta 1 que elige él + 1 al azar; el usuario no elige nada). El usuario: la funda de la derecha parpadea muchas veces, también antes con su funda propia. En el registro todos los lotes de reversos van "por triangulo"; la vista grande cae en x 676/677 (bien lejos de la columna 495), así que la funda elegida no cambia por la columna. Falta saber si el juego deja de dibujar el reverso en cuadros sueltos (animación del azar) o si el port lo pierde.
- Kit rev_draw: "[REVERSO] vista grande derecha/izquierda: sin reverso N cuadros (del A al B)" por cada hueco de 2..119 cuadros, y "[REVERSO] cuadro N: reverso cerca de la columna derecha (x, y): funda del rival/propia" (±60 px de la columna). 300 líneas como mucho. Exe 21:21 copiado (cmp iguales), sin probar.

## 2026-11-20 21:25 — Dúo delictivo, segundo intento
- Registro 21:24: mi diagnóstico de las 21:21 gastó sus 300 líneas en tu propio mazo (x 444..471, y 268..302, en todos los cuadros), y se agotó en el cuadro 2580, justo al jugar la carta. Antes de cortarse anotó: la vista grande en x 497 (cuadro ~2550, a 2 px del corte 495) y un hueco de 2 cuadros sin reverso (2573-2574). No alcanza para saber la causa.
- Cambios:
  - Corte derecho 480 (fundas.c COLUMNA_RIVAL2). El mazo propio está en 444..477, con el medio en ~458, así que no cambia de funda.
  - El aviso "cerca de la columna" ignora el mazo (hi_x > 480) y tiene su propio tope de 600 líneas; los huecos tienen otro tope de 300.
- Exe de las 21:25 copiado (cmp: iguales), sin probar.

## 2026-11-20 21:33 — Dúo delictivo: el port dibuja bien el reverso; arreglado un error mío al robar
- Registro 21:31, con la carta usada:
  - La vista grande de la derecha tiene el reverso en todos los cuadros: en toda la partida hubo un solo hueco, de 1 cuadro (39864).
  - Siempre lleva la funda del rival (x 673..677, lejos del corte).
  - El port no la pierde ni le cambia la funda. El parpadeo es el juego pasando de carta en carta al elegir y sortear; el usuario lo pensó igual, y con su funda pasaba lo mismo. Queda como en el original.
- Error mío desde las 21:0x, encontrado en este registro: las cartas que robás se deslizan desde tu mazo hacia la derecha (x 466..547, y 263..297, 28 px de ancho, cada 24 cuadros) y, al pasar el corte derecho (495, luego 480), tomaban la funda del rival a mitad del camino. También eran mías las "vista grande (x 497 / 490)" de antes.
- Arreglo:
  - Kit: las columnas solo cuentan para un triángulo de al menos g_nv2a_reverso_ancho de ancho.
  - fundas.c: ANCHO_VISTA 45; las vistas grandes miden ~75 y una carta robada 28.
  - Diagnóstico: ignora triángulos sin ancho; la línea "vista grande" anota el ancho.
- Las 570 líneas "x 494..494 y 253" eran un triángulo sin ancho (el lote de 9 índices).
- Exe de las 21:33 copiado (cmp: iguales), sin probar.

## 2026-11-20 21:36 — Dúo delictivo: hay que sacar el parpadeo
- El usuario: "no quiero que parpadee así lo haya puesto" Konami; hay que quitarlo aunque sea del original.
- El reverso se dibuja en todos los cuadros (registro 21:31), así que lo que parpadea tiene que ser el tamaño, el color o la transparencia de la carta.
- Kit: "[VISTA] derecha/izquierda cuadro N: triangulos, caja x/y, color, alfa (la anterior siguió N cuadros)". Escribe una línea cada vez que algo cambia, 2000 como mucho. Toma los triángulos de reverso con el medio en x > 560 o x < 100, y el color difuso de cada vértice (s_rev_d, en rev_posiciones).
- La regla de columnas ahora mira el ALTO (≥ 50) y no el ancho: la vista grande puede achicarse de ancho al girar. Las cartas robadas y el mazo miden 34 de alto.
- Exe de las 21:36 copiado (cmp: iguales), sin probar. Con un uso de la carta alcanza para ver qué cambia; después se fija ese valor mientras dure la vista.

## 2026-11-20 21:39 — Dúo delictivo: el diagnóstico 21:36 buscó en el lugar equivocado
- Registro 21:37:
  - La carta se usó en el cuadro ~3705 (cadena 636, -1000 PV).
  - La vista grande del rival está en x 510..590 (medio 550, ancho 80). Mi filtro [VISTA] buscaba el medio por encima de 560, así que no la tomó.
  - Las líneas [VISTA] de 548..687 con alfa de 0,55 a 0,05, cada 24 cuadros, eran las cartas de la mano inicial al repartirse, no la vista.
  - En toda la partida no hubo huecos sin reverso.
- [VISTA] ahora toma cualquier triángulo de reverso con alto ≥ 50 (solo las vistas grandes) y anota también las coordenadas de textura (uv), además de caja, color y alfa. Exe 21:39 copiado (cmp: iguales), sin probar.

## 2026-11-20 21:44 — Dúo forzado en la mano; cartel Perfil sin reiniciar
- PRUEBA TEMPORAL (fundas.c `prueba_duo`, desde ygo_fundas_tick, estado del duelo 6):
  - Dúo delictivo es k 795: es su lugar en la lista de nombres del xbe, comprobado con 8 k conocidas.
  - Mientras tu mano está vacía, la pone en el lugar 0 de tu Deck. Si el reparto la deja afuera, la pone en el lugar 0 para tu próximo robo.
  - Si no está en tu Deck, la carta del lugar 0 pasa a ser Dúo, solo en ese duelo.
  - Registro "[PRUEBA] ...". **Sacarla cuando se arregle el parpadeo.**
- Cartel Perfil vacío. Textos del juego: texto 20 = "Resultados", 16..24 son los del menú, Latin-1.
  - sub_0010C6B0 vuelve a arrancar el fundido del cartel en cada llamada: +0x20 = 1 y reinicia cada letra (sub_00013300).
  - El juego lo seguía llamando con el texto del ícono, y mi envoltorio lo cambiaba a 20. El fundido volvía a empezar todo el tiempo y las letras nunca llegaban a verse ("solo la animación como que quiere iniciar").
  - Ahora esas llamadas del juego se saltean (ret 4) mientras está Perfil; solo pasa la nuestra.
  - Registro: "el juego quiso volver a poner el texto N en el cartel (X veces): no se lo deja".
- Exe de las 21:44 copiado (cmp: iguales), sin probar.

## 2026-11-20 21:52 — Dúo delictivo: parpadeo encontrado y arreglado (sin probar); cartel Perfil: diagnóstico
- Registro 21:46, con la prueba ("[PRUEBA] Duo ... ya esta en tu mano"):
  - Mientras el rival elige (cuadros 4360-4474), la carta grande de la derecha (x 148..590, y 155..322) baja su alfa de golpe de 1,00 a 0,80 y la vuelve a subir de a 0,04 por cuadro. Lo hace cada 10 cuadros. Ese es el titileo.
  - Al aparecer el alfa sube de 0,2 a 1,0; al irse baja de 0,8 a 0,2.
- Arreglo (kit rev_draw), bit 2 en la clave del tramo:
  - Un triángulo de reverso con alto ≥ 50 y alfa entre 0,79 y 0,999 se dibuja 3 veces sobre sí mismo. Con mezcla alfa común, 0,8 queda en 0,992: la carta queda firme.
  - La entrada y la salida bajan de 0,79 y siguen iguales.
  - Solo con mezcla SRC_ALPHA / INV_SRC_ALPHA. Si la mezcla es otra lo anota una vez ("la mezcla no es la comun") y no toca nada.
  - Registro: "carta grande con alfa X (el titileo): se dibuja firme".
- Cartel Perfil:
  - Los textos del cartel salen de la tabla de textos (texto 20 = "Resultados"). El menú no los tiene como imágenes; los texdb del menú no traen el cartel.
  - Registro 21:46: ni una llamada del juego mientras está Perfil, y aun así el cartel queda vacío. El objeto de texto no lee la tabla al pedir el texto 20.
  - DIAGNÓSTICO (raton.c cartel_sonda): en los cuadros 0, 1, 5, 15, 40 y 90 de cada Perfil, y una vez con un ícono del juego, anota los dos objetos de texto del cartel (+8 el que se ve, +C el que entra): clase y función que pone el texto, letras, lista, y qué dice el texto 20. Con eso se lee la función real.
- Exe de las 21:52 copiado (cmp: iguales), sin probar.

## 2026-11-20 21:58 — Dúo: afinado y prueba sacada; cartel: dónde se pierde el texto
- El usuario: "duo solucionado parece" (exe 21:52).
- Registro 21:54: el arreglo también se activó una vez al irse la carta, que baja de a 0,05 por cuadro (1,00 → 0,95 → 0,90...), y la dejaba firme unos cuadros de más. Ahora solo actúa en el titileo:
  - una caída de golpe (0,15 o más respecto del cuadro anterior, medida por el alfa más bajo de las cartas grandes en cada cuadro), o
  - mientras vuelve a subir.
  - La bajada pareja de la salida no se toca.
- Fuera de eso, el arreglo solo toca reversos de alto ≥ 50 (las vistas grandes) con alfa entre 0,79 y 0,999 y mezcla común. Las cartas del campo, la mano y el mazo (alto 34) no cambian.
- Prueba de Dúo forzado: apagada (fundas.c, `(void)prueba_duo`).
- Cartel, registro 21:54 (sonda):
  - El cartel tiene dos objetos de texto de clase 0x427A20: +8 y +C. Pone el texto sub_0002B490 (llama a sub_0002C5C0 y después a vt+8 = sub_0002B560, que arma una letra por carácter con sub_0002A7E0 / sub_0002C490).
  - Nuestro Perfil SÍ queda en +C: 6 letras, "Perfil".
  - El fundido (sub_001034A0) usa y dibuja +C y al terminar pone +9C = 0 en cada letra (eso es normal: con los íconos pasa lo mismo).
  - Diferencia: con Perfil las letras quedan en +9C = 0 ya en el cuadro 1, con el fundido recién empezado (+1C 0,1, +20 1); con un ícono siguen en 1.
- La sonda ahora anota de cada letra lo que lee su dibujo (sub_00012B80): +7C (si es 0 no se dibuja), +9C, +9E, +EC, +80, +98, y +C..+E de la entrada.
- Exe de las 21:58 copiado (cmp: iguales), sin probar.

## 2026-11-20 22:02 — Cartel Perfil: el texto iba al lado equivocado
- El usuario (exe 21:58): el cartel "dice otra cosa".
- La sonda del registro 21:59 lo explica:
  - El cartel tiene dos textos. +8 es el que queda: en todos los cuadros tiene el nombre del ícono, con las letras encendidas. +C es el que se va: sub_001034A0 lo desvanece (1 − progreso) y al terminar apaga sus letras.
  - Al cambiar de ícono, el juego pone el nombre viejo en +C (sub_0010C6B0) y el nuevo en +8 (sub_0010CDE0 con bandera 0, [+8] vt+4 = sub_0002B490).
  - Todos mis intentos ponían "Perfil" en +C, el que se desvanece: por eso nunca quedaba.
- Arreglo (raton.c):
  - Al entrar a Perfil: el nombre del ícono va a +C (cartel_llamar, se va) y "Perfil" a +8 (cartel_vista → sub_0002B490 con el texto 20).
  - Al salir: "Perfil" a +C y el ícono vuelve a +8.
  - El texto 20 dice "Perfil" solo durante la llamada; el objeto se guarda su copia de las letras (sub_0002B560).
  - Las llamadas del juego a sub_0010C6B0 se saltean mientras está Perfil (s_cp_activo), menos las nuestras (s_cp_nuestra).
  - Si la rueda gira debajo del puntero, se vuelve a poner "Perfil" en +8, sin fundido.
- La sonda [CARTEL] sigue hasta verlo andar.
- Exe de las 22:02 copiado (cmp: iguales), sin probar.

## 2026-11-20 22:03 — Cartel Perfil COMPROBADO
- El usuario: "SIIIIIIII al fin se ve".
- Registro 22:03: "el cartel dice Perfil (... en el texto que queda)" y "vuelve a su texto" en cada pasada del puntero. El juego intentó volver a poner su texto dos veces y no se lo dejó.
- Dúo delictivo: comprobado por el usuario (exe 21:52, afinado 21:58).
- Siguen los diagnósticos, a la espera del OK del usuario para sacarlos: sonda [CARTEL] (raton.c cartel_sonda), [VISTA] / "lote de" / "cerca de la columna" / "sin reverso" (kit rev_draw).

## 2026-11-20 22:04 — Diagnósticos sacados (OK del usuario)
- Sacados:
  - raton.c: la sonda [CARTEL] (cartel_sonda, cartel_objeto y la comparación con un ícono).
  - Kit rev_draw: [VISTA] (vista_nota / vista_volcar), "lote de N indices", "reverso cerca de la columna" y "vista grande: sin reverso".
- Quedan, como avisos raros y sin peso:
  - "reverso en la vista grande" y "carta grande con alfa (el titileo)": uno cada 10 s como mucho.
  - "la mezcla no es la comun": una vez.
  - Las líneas [RATON] del cartel: una por pasada del puntero.
- La prueba de Dúo forzado ya estaba apagada.
- Exe de las 22:04 copiado (cmp: iguales). Compila sin errores nuevos.

## 2026-11-20 22:09 — Limpieza general (el usuario: "dejá todo lo más limpio posible")
- Pendientes: no queda ningún arreglo abierto. Comprobados en sus registros: la fuga de memoria (copia propia de la mascota), la funda del rival en las vistas grandes, el titileo de Dúo y el cartel Perfil.
- Sacado:
  - mascota.c:
    - recursos_volcar: las líneas [RECURSO] de todos los archivos cargados en cada duelo.
    - quien_apunta: el rastreo de toda la memoria, ya apagado.
    - Las líneas [EFECTOS].
    - El informe "siguen vivos" de cada duelo.
    - fe_conservar sigue, callado: los efectos fe007..fe012 se cargan una vez y se reaprovechan.
  - fundas.c: la prueba de Dúo forzado (prueba_duo).
  - cartas.c: el registro de cada batalla (diagnóstico de Isaac del 11-13; Isaac anda).
  - Kit:
    - kernel_bridge.c: la cadena [CONTIG*] de toda la pila y el volcado de pila al guardar la partida.
    - xbox_memory_layout.c: [CONTIG+]/[CONTIG-], el número de orden de cada bloque, xbox_ContiguousSeq y xbox_ContiguousReportRange.
    - nv2a_pb_d3d11.c: las coordenadas de textura que solo usaba [VISTA] y los avisos cada 10 s de las vistas grandes.
  - main.c: el ajuste XBOX_CONTIG_LOG_ALL.
- Queda a propósito:
  - Por duelo, "[MEMORIA] fin del duelo N: texturas en uso / heap" y las líneas [MASCOTA] de soltar modelos: sirven para vigilar sesiones largas.
  - El aviso de memoria casi llena (50 MB) y el informe "vivo" si la memoria pasa de 24 MB, de a 8 MB.
  - El cartel.png de respaldo, por si un idioma no tiene texto 20.
- RDR usa el mismo kit: no está recompilado. Sin cambios de comportamiento para RDR, solo menos registro.
- Exe de las 22:09 copiado (cmp: iguales). Compila sin errores; las advertencias son las de antes.

## 2026-11-20 22:14 — "?" en el texto de Soldado del Brillo Negro; botón "Ver carta" del Cementerio (sin probar)
- Captura del usuario: el texto de Soldado del Brillo Negro (k 391, reemplaza a Leogun) mostraba "?" donde la carta tiene un punto grande antes de cada efecto.
  - El mod trae "●", que no existe en la letra del juego. Las cartas originales con varios efectos usan 0xB7 "·" (texto 3052, en español e inglés).
  - cartas.c texto_guest: antes de pasar a 1252 convierte ●, •, ・, ◦ y ∙ en "·". Sirve para todas las cartas del mod.
- Captura del usuario: en el Cementerio, el botón de abajo "Ver carta" no anda con el mouse. Este registro no tiene esa partida.
  - La lista (sub_000C5710, list_pointer) solo tenía los botones redondos debajo de la carta, en el lugar donde los pone la Lista de objetivos.
  - En el Cementerio están en la barra de abajo a la izquierda: "Ver carta" (botón 0, la info) en x 58..205, y 346..381, y la lápida (botón 1) en x 213..249 (px del juego en la captura de 1919x1079).
  - Paso 1: ese botón y A. Paso 0: A elige la carta y el botón se aprieta cuando aparece.
  - Registro: "[RATON] list click ... Ver carta (bottom bar)".
- Exe de las 22:14 copiado (cmp: iguales), sin probar.

## 2026-11-20 22:19 — Cementerio: la flecha amarilla con el mouse, y mantener apretado pasa rápido (sin probar)
- Pedido (captura del Cementerio): "podes hacer que la flecha funcione? ... si se deja apretado vaya rapido hacia las demas".
- raton.c `list_pointer`, paso 0: la flecha amarilla de la izquierda (x 47..68, y 142..178 en px del juego según la captura; con margen 42..74 / 136..184) va una carta atrás; las cartas de los costados siguen dando un paso para su lado.
- Mantener apretado sobre la flecha o sobre una carta del costado: a los 400 ms, un paso cada 90 ms. Solo agrega un paso cuando el anterior ya entró, así al soltar no sigue de largo.
- Registro: "[RATON] list click ... -> previous card (arrow)".
- Exe de las 22:19 copiado (cmp: iguales), sin probar.

## 2026-11-20 22:25 — Cementerio: la flecha iba a la mascota; la segunda carta de cada lado no respondía (sin probar)
- Registro 22:23: los clics en la flecha (54,157 / 56,160 / 58,159 / 63,158) dieron "[MASCOTA] tocada": el clic de la mascota se revisa antes que la lista y la mascota está debajo de la flecha. Además, los clics en la segunda carta de la derecha (581,152 y 525,132) dieron "nothing".
- raton.c: el clic a la mascota no se toma mientras está abierta una lista de cartas (s_list_at / s_tl_at < 150 ms).
- list_pointer, paso 0: dos cartas de cada lado según la captura (x 37..133 y 150..246 a la izquierda, 395..490 y 508..603 a la derecha, y 91..229). El clic avanza 1 o 2 cartas; la flecha, 1 atrás. Mantener apretado repite de a una carta cada 90 ms después de 400 ms.
- Exe de las 22:25 copiado (cmp: iguales), sin probar.

## 2026-11-20 22:26 — Cementerio: flecha derecha (sin probar)
- El usuario: "la flecha del otro lado no funcionaba para nada". Registro 22:23: con la lista en la carta 0, los clics en 581,156 y 581,152 dieron "nothing" (espejo de la flecha izquierda, 640-58 = 582).
- raton.c list_pointer: la flecha derecha (x 566..598, y 136..184) pasa una carta adelante y tiene prioridad sobre la 2.ª carta de la derecha; mantener apretado repite igual que la izquierda. Registro: "next card (arrow)".
- Exe de las 22:26 copiado (cmp: iguales), sin probar.

## 2026-11-20 22:34 — El juego se cerró al terminar el duelo 2: se saca fe_conservar (sin probar)
- Registro 22:31: tras el duelo 2 (ganado), guardado bien; después [ICALL] con punteros locos (0xF6900018, 0x6C800018...) y [CRASH] en sub_00187090+0x44B. Pila: sub_0005C170+0x108 (destructor del objeto de efectos del campo) -> sub_0004B460 (suelta [obj+8]) -> sub_0019F5E0 -> árbol con objetos ya liberados.
- Lo único nuestro en ese camino era fe_conservar (cambiaba el contador de usos de fe007..fe012 en la tabla de recursos para reaprovecharlos en el duelo siguiente). No era la fuga (era la mascota m716) y no hacía falta. Se saca: el envoltorio de sub_0005C170 solo llama al original.
- Primer cierre de este tipo; no hay prueba de que sea la causa hasta jugar varios duelos seguidos sin que se cierre.
- (Corregido) Monstruo recuperación no se usó: el usuario la colocó boca abajo y no se dibujaba en el campo (captura: zona vacía, la info sí la muestra).
- Exe de las 22:34 copiado (cmp: iguales), sin probar.

## 2026-11-20 22:4x — La carta colocada invisible y el cierre: los dos en el duelo que reusó fe007..fe012
- Registro 22:31: los efectos se leen del disco en el arranque, en el duelo 1 (fe000..fe016) y en el duelo 2 todos MENOS fe007..fe012 (los que fe_conservar dejaba guardados). El duelo 2 es justo el de la carta colocada sin dibujar y el del cierre al desarmar el objeto de efectos (objetos ya liberados en su árbol). La sesión anterior (22:18, un solo duelo, sin reuso) no tuvo nada de eso.
- Conclusión: el reuso dejaba esos efectos rotos (con referencias mal contadas, se liberaban mientras el juego los seguía usando). Sacado en el exe de las 22:34: cada duelo vuelve a leer sus efectos como el juego original.
- El arreglo para jugar horas (las mascotas: copia propia del modelo m716 y visores destruidos con los del juego) NO se tocó; fe_conservar no era ese arreglo (ver 20:5x: "no era la causa").
- Sin probar: falta jugar 2+ duelos seguidos colocando cartas.

## 2026-11-20 22:58 — AUTO: botón del medio apretado = el juego pregunta (sin probar)
- `auto.c` `ygo_auto_on()`: con AUTO prendido, mientras el botón del medio del ratón está apretado devuelve 0 y el juego ofrece "¿encadenar?" como con AUTO apagado (idea "Switch + Hold" de Master Duel, elegida por el usuario). Registro: `[AUTO] boton del medio apretado: el juego pregunta encadenar` (una vez cada 2 s).
- Ojo: el botón del medio sobre una carta abre su información (raton.c); mantenerlo con el puntero fuera de las cartas.
- Exe 22:58 copiado como Yu-Gi-Oh.exe, cmp igual. Sin probar.

## 2026-11-20 23:19 — "Probar este Mazo" (mazo prestado del rival) — compilado, SIN PROBAR
- Pedido del usuario (maqueta aprobada): en el panel del mazo de un rival (Decks de los Oponentes / Estadísticas), botón "Probar este Mazo N/3" (usos que quedan). Solo una VICTORIA gasta un uso (perder, empatar o salir a mitad no). Al gastar el 3.º, el mazo vuelve solo. Con todas las cartas que da ese rival (las mismas que cuenta el cartel de premios, variante actual) queda "Ganado" para siempre: botón "Jugar con este mazo" sin números y sello dorado chico en la esquina del retrato. Mientras se usa: "Devolver el mazo N/3" en verde y "Tu mazo quedó guardado aparte".
- Código del juego leído: `sub_0007E070` arma los mazos al empezar el duelo. Duelo normal (`[sub_00103590()] != 0`): el del jugador con `sub_0007DC40(0, ranura)` (ranuras u16 0x66609A + 0x100·r, cantidad u16 0x66649A + 2r, rehechas desde la partida por `sub_000D8FF0`; ranura actual `sub_000D95F0`). El del rival con `sub_0008C760(1, d)` (lista B + relleno al azar hasta 40 de la lista A, variante `sub_0008C530`, después `sub_0008C0D0`, que copia el mazo armado a la ranura 3 0x66639A). La rama `[setup] == 0` (demo) arma los dos con mazos de CPU.
- `src/prestado.c` (nuevo, en CMakeLists): envuelve `sub_0007DC40` (rename_pads). Con préstamo activo, jugador 0 y duelo normal llama `sub_0008C760(0, d)` con los mismos dos lugares de la pila: el mazo prestado se arma con la función del juego. El mazo guardado (0x66609A y la partida) no se escribe nunca; la ranura 3 la vuelve a escribir la llamada del rival justo después, como siempre. El cofre no cambia.
- Estado en `YGO_AJUSTES\prestados.txt` (`activo N`, `personaje usos ganado`), escrito al momento. `ygo_prestado_fin(res)` desde `ygo_estadisticas_fin`.
- perfil.c: botón en la cabecera (BT_*), línea "Cartas que da X: a de b" + barra verde (PR_Y), sello (`sello_ganado`), pregunta con Aceptar/Cancelar (CF_*; clic fuera, clic derecho o Esc = no). Textos en f2_idiomas.c (6 idiomas).
- Registro: `[PRESTADO] duelo con el mazo del personaje ...`, `mazo armado: N cartas`, `mazo del jugador 0: ranura S (modo M)` (sin préstamo), `victoria ... quedan N usos`, `GANADO`.
- Exe 23:19 copiado como Yu-Gi-Oh.exe (cmp igual). Sin probar.

## 2026-11-20 23:26 — Mazo prestado: arreglo + su mascota, funda y tapete (SIN PROBAR)
- Prueba del usuario: "puse usar deck y igual me toco el mio". Registro 23:22: `activo 9` en prestados.txt, pero `[PRESTADO] mazo del jugador 0: ranura 0 (modo 0)`. Mi chequeo de "duelo normal" (`[0x66DCDC] != 0`) estaba al revés: en el duelo normal `[0x66DCDC]` es 0 y `[+8]` = 0x1C + ranura, así que las dos ramas de `sub_0007E070` que llegan a `sub_0007DC40` son el mazo guardado del jugador (la demo arma el del jugador con `sub_0008C760` y no pasa por acá). Ahora reemplaza siempre que sea el jugador 0 con un préstamo activo.
- Pedido nuevo: con el mazo prestado, también su mascota, su funda y su tapete.
  - `prestado.c` `ygo_prestado_en_duelo()`: el personaje del mazo de este duelo (o el activo), -1 el tuyo.
  - `mascota.c` `tuya_idx()`: tu mascota pasa a ser `k_rival[personaje]` (modelo_de y modelo_lado; Carta As la sigue). Registro "[MASCOTA] tuya: la del mazo prestado (N)".
  - `fundas.c`: en el primer tick de la escena del duelo, tu funda = la del rival (`k_por_mascota`, marco obsidiana) y tu tapete forzado (`s_tap_forzado`; tu ajuste guardado `s_tapete` no cambia). Fuera del duelo (`ygo_fundas_cuadro` desde la lectura del mando en mando_directo.c, 1,5 s sin tick) vuelven los tuyos. `componer` ahora con candado (el override del kit admite un solo escritor). Registros "[FUNDAS] tuya: la del mazo prestado ..." y "[FUNDAS] fuera del duelo: vuelven tu funda y tu tapete".
- Exe 23:26 copiado (cmp igual). Sin probar.

## 2026-11-20 23:33 — Mazo prestado solo para Duelos; rivales bloqueados oscuros (SIN PROBAR)
- Pedido: el prestado (no ganado) solo en Duelos. En un Torneo entrás con tu mazo y el préstamo se cancela (hay que pedirlo de nuevo). Ganado = sirve para todo, también Torneos.
  - Torneo = u32 [0x66DCDC] == 1 (sub_0008C530 usa entonces el nivel de torneo [+0x14]; premios.c igual). En el duelo normal vale 0 (registro 23:22). prestado.c `en_torneo()`: en `sub_0007DC40` con préstamo no ganado → se cancela (`[PRESTADO] torneo: ... solo para Duelos`) y carga tu mazo; `ygo_prestado_en_duelo` devuelve -1 en torneo (tu mascota/funda/tapete). Registro nuevo en cada duelo: `[PRESTADO] empieza un duelo: modo M (1 torneo), rival personaje N, prestamo P` (para confirmar el valor del torneo y del Duelo triple).
  - Avisos: debajo del botón "Solo para Duelos (no Torneos)" / "Sirve para Duelos y Torneos" (ganado); en la pregunta dos líneas nuevas (cuadro agrandado 420..1500 x 360..840).
- Pedido: en Decks de los Oponentes, los desbloqueados en Duelo normales; los demás más oscuros (retrato ×105/255, borde y nombre apagados), sin abrir su mazo (tampoco desde Estadísticas). Desbloqueado = lo mismo que mira la pantalla de elegir rival (`sub_000FC370`): u32 0x651958 + 4·personaje != 0 (personajes 0..26 y 31..33). Título: "Elegí un rival para ver su Deck (los oscuros todavía no los desbloqueaste en Duelo)".
- Textos en 6 idiomas. Exe 23:33 copiado (cmp igual). Sin probar.

## 2026-11-20 23:39 — Mazo prestado: textos cortos, ganado sin pregunta, tilde verde (SIN PROBAR)
- El usuario probó 1 duelo, bien por ahora (capturas). Pidió:
  - **Pregunta más corta:** 2 líneas: "Solo para Duelos. Cada victoria gasta 1 de sus 3 usos." / "Con todas sus cartas es tuyo para siempre." (cuadro 520..1400 x 400..730).
  - **Ganado:** el botón lo presta al instante, sin pregunta. Debajo del botón ya no dice "Sirve para Duelos y Torneos".
  - **Sin "GANADO":** un tilde verde (círculo verde oscuro con borde y tilde, `sello_ganado`) en la esquina del retrato del panel (r 20) y en la grilla de Decks de los Oponentes (r 18).
- `prestado.c`:
  - `ygo_prestado_ganado(ch)` para la grilla.
  - `ygo_prestado_revisar_todos()` al abrir el Perfil y al cambiar de página: un mazo también se gana con cartas de duelos comunes.
- Exe 23:39 copiado (cmp igual). Sin probar.

### 2026-11-20 23:52 — Revisión del mazo prestado con el registro de las 23:41
- El registro confirma que funciona: se presta el mazo del personaje 8, se arma con 40 cartas y el mazo guardado no se toca. La mascota y la funda son las del 8. Al ganar quedan 2 usos. Al salir del duelo vuelven tu funda y tu tapete, y el segundo duelo usa tu mazo y tu mascota (m717). En un duelo normal, el setup [0x66DCDC] vale 0.
- Error encontrado: la línea `[MASCOTA] tuya: la del mazo prestado` salía en cada cuadro (66.499 veces en un duelo). modelo_de(0) ahora la anota solo cuando cambia (mascota.c).
- Se corrigieron los comentarios viejos de prestado.c (duelo normal == 0, torneo == 1).
- Pendiente: falta ver un torneo para confirmar que vale 1. El registro va a mostrar `[PRESTADO] empieza un duelo: modo ...`.
- Exe de las 23:52, copiado como Yu-Gi-Oh.exe (cmp IGUAL).

### 2026-11-20 23:56 — Torneo confirmado
- Registro de las 23:56: `modo 1 (1 torneo)`, el préstamo del 8 se canceló y se jugó con la ranura 0. No aparece "tuya" en mascota ni funda. prestados.txt quedó con `activo -1` y los 2 usos del 8 intactos.

### 2026-11-21 00:11 — Mascotas según el fondo de diálogo de cada personaje
- Los fondos de diálogo están en `Game\cf_anim\bg\cfbgNN.png`, donde NN es el número de personaje (0..26); cada uno muestra el monstruo del personaje. El usuario quiere que la mascota sea esa.
- Cambiados en k_rival (mascota.c): Yugi pasa a Kuriboh (1), Tristán a Fuerza de Ataque Goblin (70) y Pegasus a Relinquished (43). Con la mascota cambian la funda, el tapete y lo que trae el mazo prestado.
- Revisados sin cambios: Odion (Slate Warrior coincide con cfbg22); Weevil (el modelo 66 ya es Polilla perfectamente final, igual que cfbg14, pero la funda y el tapete usan el arte de la Gran Polilla común).
- Pendientes, sin entrada de mascota (modelo, carta):
  - Keith: Slot Machine (m722, 4729).
  - Bonz: Clown Zombie (m097, 4104).
  - Lumis y Umbra: Melchid (m953, 5095).
  - Strings: Humanoid Slime (m946, 5060).
  Los cuatro modelos existen en Game\monster; faltan arte, funda y tapete.
- Marik, Yami Marik y Shimon: su fondo es una carta mágica o de trampa, así que quedan como están.
- La tabla de carta→modelo de todo el juego está en el scratchpad (cartas_modelos.txt, hecha con la tabla 0x44FBB0).
- Exe de las 00:11, copiado (cmp IGUAL). Sin probar.

### 2026-11-21 00:2x — Corrección: Lumis y Umbra ya estaba bien
- La captura del usuario (mascota del duelo, modelo 902) es el mismo dibujo que cfbg19: el jinete con bastón sobre la cara con pinchos. Ese es La Bestia Enmascarada, no Melchid (Melchid es solo la bola). No se cambia.
- Quedan 3 mascotas para agregar: Keith (Máquina Tragamonedas), Bonz (Payaso Zombi) y Strings (Légamo Humanoide). Las artes bajadas de ygoprodeck están en fundas/arte y arte_mascotas. La muestra está en modding/muestras/mascotas_fondo_4.png, esperando el OK. (El arte de Melchid trae la marca "SAMPLE"; no se usa.)

### 2026-11-21 00:30 — 3 mascotas reemplazadas por las de los fondos de Keith, Bonz y Strings
- El usuario eligió cuáles sacar con capturas del Perfil. Dunames se queda porque es la de Ishizu.
- Reemplazos en el mismo número de mascota:
  - 24: Comandante Cibernético (420) → Máquina Tragamonedas (722, carta 4729).
  - 56: Masaki (292) → Payaso Zombi (97, carta 4104).
  - 59: Rey Máquina (406) → Légamo Humanoide (946, carta 5060).
- k_rival: Keith 24, Bonz 56, Strings 59.
- Se sacó el retoque 0.8 de Cyber Commander (k_retoque). Las 3 nuevas se miden la primera vez que salen.
- fundas.c k_arte: maquina_tragamonedas / payaso_zombi / legamo_humanoide. main.c (mascota, funda y tapete), f2_idiomas.c y Configuracion.cs usan los nombres del juego en 6 idiomas (portugués a mano).
- Arte de ygoprodeck (3797883, 92667214, 46821314) en fundas/arte y arte_mascotas.
- Se rearmaron fundas.py y tapetes.py (100), perfil_recursos.py y las grabaciones del Perfil (visor, /grabar?m=722,97,946, 120 cuadros); perfil.py rearmó las tiras 24/56/59. Las cartas grandes de la Carta As ya existían.
- Quien tenía guardada una de esas 3 por nombre en el ini vuelve a la de fábrica. El número guardado de funda o tapete ahora apunta a la nueva.
- Copiado con copiar_al_juego.sh (exe 00:30:15 idéntico) y Configuración.exe (cmp). Sin probar. Falta ver el tamaño de las 3 en el duelo.

### 2026-11-21 00:5x — Lumis y Umbra con 2 mascotas: maqueta
- Pedido: la mascota 45 (Maldición del Dragón, modelo 38) se reemplaza por Melchid (953), segunda mascota de Lumis y Umbra (fondo cfbg19: la Bestia Enmascarada se queda).
- Hay que sumar un tercer visor del lado del rival: s_m[2] y g_nv2a_mascota[2] (que comparten mascota.c y el kit nv2a_pb_d3d11.c), entrada, reacción, fin del duelo y la liberación de memoria.
- Maqueta modding/muestras/lumis_dos_mascotas.png: A una al lado de la otra; B Melchid adelante y más chico. Esperando que el usuario elija.
- El arte de Melchid en ygoprodeck trae la marca "SAMPLE": para la funda y el tapete hay que recortarla o usar otro arte.

### 2026-11-21 00:53 — Lumis y Umbra con 2 mascotas (maqueta B) + Melchid reemplaza a Ryu-Ran
- La mascota 74 era Ryu-Ran (725) y ahora es Melchid la Bestia de Cuatro Caras (953, carta 5095).
  - Se cambió en fundas.c k_arte, main.c, f2_idiomas.c y Configuracion.cs (6 idiomas, nombres del juego), y en fundas.py / tapetes.py.
  - Arte de ygoprodeck 86569121: la marca "SAMPLE" se sacó recortando (55,0,570,515) y escalando a 624. El original quedó en arte_mascotas/melchid_con_marca.jpg.
  - La tira del Perfil se armó con la grabación m953. Muestra: modding/muestras/melchid_funda_tapete.png.
- Tercer visor (mascota.c NMASC 3, índice 2): segunda mascota del rival, solo con el personaje de segunda() (19 Lumis y Umbra → 74 Melchid).
  - Va del lado del juego 1 (LADO(i)). Las medidas se guardan con la clave del lado 1.
  - Ubicar (opción B): SEGUNDA_ESC 0.55 del ajuste, borde izquierdo en IZQ_RIVAL + SEGUNDA_DX (0) y pies en la fila SEGUNDA_PIE 296. Se dibuja después de la primera, así que queda adelante.
  - Tiene entrada pero sin destello. Al terminar el duelo cambia de pose pero no tiene aura ni tono (esas tablas del kit siguen siendo de 2). Turnos de dibujo: con 3 visores, cada 16·2/3 ms (cada una sigue a ~30 por segundo).
  - Se crea, se suelta y se destruye con las otras (soltar_todo, nodos, sub_0005B9C0).
- Kit (nv2a_pb_d3d11.c):
  - g_nv2a_mascota[3] y s_mk[3]; la medida, por turnos cada 6 presentaciones.
  - masc_draw reconoce la mascota primero por su textura y si no por el rectángulo más chico que contiene el dibujo, porque la segunda está dentro del de la primera. Mientras se mide, la caja de la segunda es de 0.25 para no confundirse.
- Exe 00:53:13 copiado (copiar_al_juego.sh) y Configuración (cmp). Sin probar. Para verlo, el registro debe decir "[MASCOTA] 2: ..." en un duelo contra Lumis y Umbra.

### 2026-11-21 01:0x — Lumis y Umbra: mascotas reubicadas (captura del usuario)
- Medidas del registro: Bestia 902 en 508..622 y filas 162..278; Melchid 508..570 y filas 233..298, se chocaban.
- Ahora, con segunda mascota, la primera termina en la columna 638 (PRIMERA_DER): 524..638.
- Melchid: SEGUNDA_ESC 0.34, desde la columna 518 (IZQ_RIVAL+10, a la derecha del borde de tu tablero, ~515), pies en la fila 320 (tus cartas empiezan en ~322). Queda en ~518..560, filas 278..320, debajo del bastón de la Bestia.
- Exe copiado. Sin probar.

### 2026-11-21 01:2x — Lumis y Umbra: juntos como en su retrato
- Bestia de 80 de ancho, columnas 558..638, pies en la fila 240. Melchid al 0.44 de su ajuste (~50x55), columnas 518..568, pies en la fila 250, adelante y tapando ~10 columnas de la Bestia. Solo se copió el exe (cmp). Sin probar.

### 2026-11-21 01:3x — Lumis y Umbra: pareja sin choque
- Captura: Melchid (518..573) pisaba las patas de la Bestia (567..627). Ahora cada una tiene un ancho fijo y los pies en la misma fila (245): Melchid 48 (518..566), 4 columnas de separación, Bestia 66 (570..636). Solo el exe (cmp). Sin probar.

### 2026-11-21 01:4x — Lumis y Umbra: maqueta 2 elegida
- Maquetas en modding/muestras/lumis_pareja_4.png; la elegida con sus números en lumis_pareja_final.png.
- Melchid: 44 de ancho, columnas 504..548, pies en la fila 226. Bestia: 88 de ancho, columnas 550..638, pies en la fila 248. No se tocan. Solo el exe (cmp). Sin probar.
- Para la próxima: primero maqueta, después juego (el usuario: "80 veces mal no avanzamos").

### 2026-11-21 01:5x — Melchid 8 columnas a la derecha (512..556)
- Pedido: "muy alejados". Maqueta muestras/lumis_melchid_derecha.png: con +12 casi toca el brazo del jinete, se dejó +8. Solo el exe (cmp). Sin probar.

### 2026-11-21 01:5x — Melchid +12 (elegido por el usuario): columnas 516..560. Solo el exe (cmp). Sin probar.

### 2026-11-21 02:0x — Mazo prestado de Lumis y Umbra: tus 2 mascotas
- Cuarta mascota (índice 3, NMASC 4): tu segunda, solo con un mazo prestado cuyo dueño tiene segunda() (19 → Melchid).
- Macros LADO(i) (1 y 2 son del rival), SEGUNDA(i) (2 y 3) y PAREJA(i). Tu pareja va en espejo de la del rival: Bestia en las columnas 2..90 con los pies en PIE_JUGADOR (226), Melchid en 80..124 con los pies en la fila 204. Maqueta: muestras/lumis_prestado.png.
- Turnos de dibujo según cuántos visores haya: orden(i), cada RENDER_MS*2/n.
- Kit: g_nv2a_mascota[4], s_mk[4], medida cada 8 presentaciones.
- Solo el exe (cmp). Sin probar. Para verlo: pedí prestado el mazo de Lumis y Umbra; el registro tiene que decir "[MASCOTA] 3: ...".

### 2026-11-21 02:1x — PRUEBA FORZADA de las 4 mascotas (el usuario no tiene a Lumis y Umbra)
- Mientras exista Ajustes\prueba_dos_mascotas.txt, segunda() devuelve Melchid para cualquier rival y para vos: 4 visores en un solo duelo. Se lee en cada crear(). El registro dice "[MASCOTA] PRUEBA: ...".
- Se agregó una línea por cada visor destruido en sub_0005B9C0: "[MASCOTA] i: visor X destruido (modelo N)".
- Pendiente: después de un duelo terminado, verificar en el registro los 4 destruidos, "soltar los modelos" y que [MEMORIA] fin del duelo no suba. Después borrar el archivo y el código de prueba.
- Solo el exe (cmp). Sin probar.
### 2026-11-21 01:42 — 4 mascotas: descarte PROBADO, prueba quitada
- Registro 01:40:37, 2 duelos con la prueba (4 visores): en los dos, "[MASCOTA] 0..3: visor ... destruido", 4 "modelo soltado" y "soltar los modelos". Fin del duelo 1: texturas 12344 KB, heap 9928 KB / 303 bloques; duelo 2: 12512 KB, heap 9928 KB / 304. Los 168 KB de diferencia salen del modelo de tu mascota, distinto en cada duelo (0 en el 1, 902 en el 2); no es una fuga.
- Notado: con 4 mascotas, la animación del final da "lento: mascotas ~23 ms" durante unos 35 cuadros. No se compara con 2 mascotas (no hay registro de antes). Solo ocurre con Lumis y Umbra o con un mazo prestado de ellos.
- Se quitó s_prueba_dos y la lectura del archivo; se borró Ajustes\prueba_dos_mascotas.txt. Queda la línea por visor destruido (diagnóstico).
- Exe 01:41:42 copiado, cmp OK.
### 2026-11-21 01:46 — duelo real contra Lumis y Umbra (registro 01:46:18)
- Rival personaje 19, ganaste 8000 a 0. 3 visores (tuyo 902, Bestia Enmascarada 902, Melchid 953); los 3 destruidos, 3 modelos soltados. Fin del duelo: texturas 11808 KB, heap 9927 KB / 298 bloques. Sin errores.
- Animación del final: unos 46 cuadros con "mascotas 16-17 ms" (con 4 mascotas daba ~23 ms).
### 2026-11-21 02:1x — Lentitud y FPS (el usuario: "revisa si va lento algo, sobre todo cuando se cierra la puerta luego de un duelo de torneo ... los fps son muy cambiantes")
- Registro 02:00:19, a 165 FPS: vblank parejo (600 cada 3,64 s, separación 4,6-7,6 ms) y las presentaciones lo siguen. En el duelo, [MASCOTA] costo da 151-165 cuadros/s.
- El contador del kit (host_menu.c) promedia ventanas de 0,5 s. Un cuadro de 480 ms deja esa ventana en ~60 o menos: ese es el "60" que se ve.
- Tirones: solo en las cargas entre pantallas. Se repiten en cada ronda del torneo:
  - cerrar el duelo (la puerta): 470-557 ms;
  - pantalla del rival: ~300-390 ms;
  - menú: ~150 ms;
  - carga del duelo: ~190 ms y ~160 ms.
  En esos cuadros el disco tarda 3-36 ms y la subida de texturas 1-50 ms. El resto es código del juego: libpng 1.0.8 y zlib 1.1.3 recompilados (texdb000-003, cf_anim large), más guardar la partida.
- En los duelos hay tirones sueltos de 25-50 ms (350-660 dibujos).
- Nuevo diag_carga.c: envuelve el cargador PNG de Alchemy (sub_0017AF80/sub_0017B1A0), png_read_row (sub_0028F180) e inflate (sub_00297940). Escribe "[CARGA] png N: X ms (filas Y, zlib Z)" para medir antes de cambiar algo. Agregados a PADS en rename_pads.py y a CMakeLists.
- Exe 02:08:58 copiado (cmp OK). Sin probar; falta un registro con una ronda de torneo.
- 02:1x registro 02:15:16 (torneo entero): 76 PNG, 1742 ms en total; filas 1695 ms, de eso zlib 1349 ms (77 %). En la puerta, los PNG son 155 de 479 ms (texdb000-003, ~42 ms cada uno). El resto no es PNG: cfl00_2 no pasa por estos cargadores (hay otros lectores PNG en sub_002A5830 y sub_003F1EE4), más guardar, desarmar el duelo y menu_m_01.igb.
- Agregado a diag_carga.c un muestreo de unas 1000 veces por segundo del hilo que carga los PNG. Si un cuadro tarda 100 ms o más, escribe "[CARGA] cuadro de N ms (M muestras): funciones %". Usa SymFromAddr con el PDB, que main.c ya inicializa. Exe 02:17:24 copiado, cmp OK. Sin probar.
- El usuario no quiere más torneos para medir: alcanza con arrancar hasta el menú principal, que tiene los mismos tirones de carga, o con un duelo libre corto para la puerta.
- 02:2x registro 02:23:37 (arranque y un duelo): el muestreo está en el hilo que carga los PNG.
  - En la puerta (556 ms, 373 muestras) no hay ninguna función que pese: la que más tiene es el 1 %, y 277 muestras caen fuera de las 48 primeras funciones. Es el motor armando la escena, repartido en cientos de funciones.
  - En los tirones del arranque y del menú (140-220 ms), ese hilo pasa el 40-70 % esperando (ZwWaitForAlertByThreadId, ZwWaitForMultipleObjects, ZwReleaseSemaphore). Falta saber a quién espera.
  - Release se compila con MaxSpeed.
  - Conclusión: reescribir el cargador entero no es realista. Lo que se puede ganar seguro es la descompresión PNG con código nativo (un tercio de la puerta, unos 120-150 ms) y averiguar esas esperas.
- 02:2x (el usuario eligió "Ver las esperas"): diag_carga.c ahora muestrea todos los hilos del proceso. En una espera (pc fuera del exe) lee la pila hacia arriba y toma las 2 primeras direcciones del exe: la espera del kit y quién la pidió. Por cuadro lento escribe una línea por hilo: el que carga, más los que trabajaron. Exe 02:27:47 copiado, cmp OK. Falta: arrancar hasta el menú.
- 02:28:50: el umbral del muestreo baja a 25 ms, para cubrir también los tirones chicos del duelo. Un solo registro (arrancar, menú, un duelo hasta la puerta) cubre todo. Exe copiado, cmp OK.
- 02:3x registro 02:31:42 (todos los hilos, 267 cuadros lentos):
  - El candado de CPU único está apagado: en el registro no aparece "[KERNEL] guest CPU lock on". Las esperas del kit van directo a Windows (sync_wait_one, EnterCriticalSection), sin sondeo ni demora agregada.
  - En las cargas, el hilo principal 19688 espera el 55-82 % del tiempo, sobre todo por el candado del juego 0x00631A64 ([CS] waiting ... held by 61004).
  - El trabajador 61004 tiene ese candado mientras descomprime: sub_0029D120, sub_0029C150, sub_00297650 y sub_0029A4B0 son zlib. Trabaja un 31-43 % del cuadro, y de eso cerca de 2/3 es zlib. Su descompresión queda en el camino crítico de la carga.
  - Entre el 20 y el 50 % del cuadro los dos esperan al mismo tiempo. Las llamadas que da el escaneo de la pila no son confiables (direcciones viejas), así que falta medir esas esperas en el kit mismo: tipo de objeto, duración y quién llama.
  - Arreglo seguro: descompresión nativa en lugar de zlib 1.1.3 recompilada. Haría falta traer miniz (tinfl, descompresor por partes, licencia MIT): se pide permiso al usuario antes de bajarlo.
### 2026-11-21 02:46 — zlib nativo (miniz) y esperas medidas en el kit (el usuario: "Sí, bajalo")
- third_party/miniz-3.0.2 (miniz.c/.h, LICENSE MIT), bajado del release oficial 3.0.2 de GitHub. Compilado sin zip, stdio, deflate ni nombres de zlib.
- src/zlib_nativo.c reemplaza inflate (sub_00297940) y crc32 (sub_00297650) del zlib 1.1.3 del juego:
  - lee y escribe el z_stream invitado en su lugar;
  - cada flujo tiene un mz_stream nativo, que se reinicia cuando total_in y total_out valen 0;
  - el modo crudo o con cabecera sale del estado invitado (+12 nowrap);
  - el crc32 compara las primeras 16 llamadas con el original y, si alguna difiere, vuelve al del juego;
  - con Ajustes\zlib_original.txt queda todo el zlib del juego;
  - escribe líneas "[ZLIB] ...".
- src/zlib_inflar.h: el comportamiento de zlib 1.1.3 encima de mz_inflate. Lo encontró la prueba fuera del juego (scratchpad prueba_png.c, que imita las llamadas de png_read_row y png_read_finish_row de libpng 1.0.8):
  1) mz_inflate se detiene cuando tomó toda la entrada aunque le quede salida: se lo llama de nuevo mientras avance;
  2) toma la entrada de golpe y deja la última fila adentro. libpng daba "Not enough image data" en 28 de 151: ahora se retiene 1 byte cuando la salida está llena.
  - Resultado: las 334 PNG de Game\ salen bien y con el mismo crc32 de filas que el zlib de Python.
- Kit: g_xbox_wait_hook (kernel.h) en sync_wait_one/many (kernel_sync.c), en la sección crítica con espera (kernel_rtl.c) y en KeDelayExecutionThread (kernel_thread.c).
- diag_carga.c suma por cuadro las esperas de 0,2 ms o más: hilo, tipo, objeto y las 3 primeras palabras de código de la pila invitada. Escribe "[CARGA] espera cuadro N ms: hilo T candado X ms (n) desde a<b<c", las 10 más largas de cada cuadro lento.
- Exe 02:46:16 copiado, cmp OK. Sin probar en el juego.
### 2026-11-21 02:54 — registro 02:50 (un duelo) revisado entero
- zlib nativo funcionando: "[ZLIB] inflate y crc32 nativos", "crc32 nativo: 16 iguales al del juego". Ningún error de zlib ni de libpng.
  - Calidad: sin pérdida, los mismos bytes (334 PNG comparadas fuera del juego).
  - Velocidad: las PNG del menú bajaron de ~42 ms a ~8 ms (zlib de 35 a ~4 ms). Quedan ~4-7 ms de filtros de libpng, recompilados.
- La puerta sigue en 472 ms (antes 470-557): las PNG no eran lo que la frenaba. El hilo principal espera el 72 % del tiempo dentro de ntdll, en algo que el gancho no mide (el candado del juego suma solo ~80 ms). Ahora el muestreo anota el nombre de la función de ntdll donde espera, para saber qué es.
- El tirón de 1435 ms fue la ventana en segundo plano ("[d3d11] paused"): no cuenta.
- El muestreo de todos los hilos se medía a sí mismo: ~200 líneas "cuadro 55 ms, 1 muestras" sin [HITCH], porque una ronda por todos los hilos del proceso tardaba ~55 ms. Ahora solo muestrea los hilos del juego (los que esperan por el gancho del kit, más el cargador), y el umbral baja a 20 ms.
- ARREGLO danio.c: en cada cambio de vida se recorrían los 64 MB del juego buscando el contador de PV (12-35 ms en ese cuadro, el tirón "al atacar"). En todos los registros, también en runs/, dio "no hallado": el número siempre duró sus 50 cuadros. Se sacó la búsqueda; se ve igual.
- Exe 02:54:10 copiado, cmp OK. Sin probar.
- 02:58:06 (el usuario: "anota todo para que en un solo duelo sepamos todo"): el muestreo guarda la cadena real de llamadas de cada muestra (desenrollado x64 con RtlLookupFunctionEntry/RtlVirtualUnwind dentro de __try, hasta 6 cuadros del exe). Por cuadro lento (≥20 ms) y por hilo del juego escribe las funciones hoja, el estado del duelo ([0x657148]) y las cadenas que pesan 5 % o más ("[CARGA]   pila N%: hoja<-...<-sub_"). Con eso, junto con las esperas del gancho, [HITCH], [MASCOTA] lento y las PNG, un solo duelo cubre la puerta, el cambio de turno, el robo y los modelos. Exe copiado, cmp OK.
### 2026-11-21 03:0x — registro 03:01 (un duelo, con la pila real de llamadas)
- Duelo:
  - Sin búsqueda del contador de daño (no aparece "[DANIO] buscando").
  - Solo 6 cuadros lentos de 25-43 ms en el duelo; en el registro de 02:50 había ~13, más los de la búsqueda.
  - Los 6 tienen la misma pila: sub_003F3218 / sub_003F3B9D / sub_003F43B6 / sub_003F4B5C (cálculo x87: fld/fmul/fistp) <- sub_003EEB15 <- sub_003F0C1B <- sub_003EC418 <- sub_0017E910 (Alchemy). En un cuadro, el 12 % era ldexp: venía de recomp_fist.
- ARREGLO recomp_types.h, en gen y en la plantilla del kit:
  - recomp_fist usa límites constantes (antes llamaba a ldexp(1.0, bits-1) en cada FIST).
  - Con RC=nearest, valor dentro del rango y MXCSR en nearest es un solo CVTSD2SI.
  - Probado fuera del juego (scratchpad prueba_fist.c): 60.000.390 casos, bordes incluidos (.5, límites de 16 y 32 bits, NaN, infinito, los 4 RC), 0 distintos.
  - Recompila todo el código generado.
- Puerta (495 ms):
  - En ese cuadro el juego desarma el duelo, guarda (respaldo y escritura), carga 8 modelos del menú (menu_m_01..14.igb) y 4 PNG, y sortea los premios.
  - El hilo principal espera el 70 %: 34 % en WaitForMultipleObjects y 14 % en ReleaseSemaphore. Lo llama la sincronización de Alchemy: vtable 0x46E9C0, método sub_0018B490 -> sub_002F1D30 -> sub_002F23C0/sub_002F2310 -> sub_0011ED96 (WaitForMultipleObjects; devuelve 0x274C cuando vence el plazo).
  - Las esperas así no pasan por el gancho: tienen plazo 0 o duran menos de 0,2 ms. Es una espera activa por sondeo, mientras el trabajador 60940 está libre el 83 % del tiempo (en su cola, objeto 600).
  - Falta saber qué sondea. Se puede averiguar leyendo el código, sin otro registro.
- Puerta, leyendo el código: sub_002F1D30 es sem_wait de una capa POSIX (errno por sub_00359AAF, EINVAL 0x16); sub_002F1CF0 es sem_trywait y sub_002F1D70 es sem_post (ReleaseSemaphore). sub_0018B490 es el método de espera del semáforo de Alchemy. El hilo principal y el trabajador del cargador se pasan pedidos chicos de a uno: miles de idas y vueltas por semáforo, cada una un cambio de núcleo en Windows. En la Xbox, con un solo CPU, no costaban nada.
- main.c: RECOMP_GUEST_CPUS por defecto ("2", o "one" si no existe el núcleo 2). El kit pone el hilo principal y los trabajadores del juego en ese núcleo; los hilos de temporizador, APU y nv2a no se tocan. Se apaga con Ajustes\varios_nucleos.txt. Registro: "[KERNEL] guest threads on one host core (cpu N)".
- Exe 03:16:21 (con recomp_fist rápido y el núcleo único) copiado, cmp OK. Sin probar. El mismo registro sirve para medir las dos cosas: la puerta contra 495 ms y los cuadros del duelo contra 25-43 ms.
### 2026-11-21 03:26 — registro 03:25 (exe 03:16): un solo núcleo QUITADO
- "[KERNEL] guest threads on one host core (cpu 2)": el núcleo único estaba activo. zlib nativo bien, sin búsqueda de daño, sin errores.
- La puerta bajó de 495 a 462 ms, pero el duelo empeoró: 22 tirones de 30-73 ms, en pares (antes 6, de 25-43 ms). En cada ataque el trabajador carga el modelo del monstruo y el efecto (monster/400/m432.igb, effect/battle_igb/be064.igb). Antes eso iba en paralelo en otro núcleo; ahora el trabajador ocupaba el núcleo del principal (candado 0x631A64 47,7 ms, el principal en KeDelayExecution).
- Se sacó el default de main.c: todos los núcleos, como antes. El efecto de recomp_fist no se puede separar en este registro (sub_003F3218 sigue en el 28 % de esos cuadros); queda puesto porque da los mismos resultados.
- Exe 03:26:20 copiado, cmp OK.
- Lo que falta: los modelos del ataque se cargan cuando se ataca (Alchemy los arma en el trabajador) y el cálculo x87 de sub_0017E910.
### 2026-11-21 03:33 — conversores de D3DX nativos y espera activa corta (el usuario: "segui con lo que falta y dame resultados no preguntes")
- Los cuadros lentos del duelo (25-73 ms; 45-60 % de cada uno) son 4 conversores de filas de píxeles de D3DX (analysis/disasm/asm/D3DX.asm). El juego los llama por sub_003EEB15<-sub_003F0C1B<-sub_003EC418<-sub_0017E910 cuando arma una textura mientras se juega (la carta robada):
  - sub_003F3218: float -> X8R8G8B8, tramado 4x4, FIST con RC=truncar (sub_003F2D7F);
  - sub_003F3B9D: float -> L8 (luminancia);
  - sub_003F43B6: A8R8G8B8 -> float;
  - sub_003F4B5C: A8L8 -> float.
- src/d3dx_nativo.c los hace en código nativo, con las mismas operaciones y el mismo redondeo (precisión x87 del juego, guardado en float, recomp_fist con RC=truncar).
  - Se comprueba en el juego: las primeras 64 filas de cada uno corren la del juego y la nativa y se comparan byte a byte. Una sola diferencia y queda la del juego toda la partida ("[D3DX] ... distinto ..."). Si salen 64 iguales: "[D3DX] ... 64 filas iguales al juego, ahora nativo".
  - Los decodificadores con clave de color ([this+0x10]) siguen con la del juego.
- Kit (kernel_sync.c): antes de bloquearse, cada espera (sync_wait_one/many) gira hasta RECOMP_WAIT_SPIN_US preguntando por el objeto. No gira con el candado de CPU único. Cuenta g_xbox_spin_hits/misses.
  - main.c pone 50 us por defecto; se apaga con Ajustes\sin_giro.txt.
  - Busca que las idas y vueltas sem_post/sem_wait de la puerta no paguen el despertar de un hilo en otro núcleo, sin juntar los hilos en un núcleo (eso empeoró el duelo).
  - diag_carga escribe "[CARGA] espera cuadro N ms: giro: X respondidas despiertas, Y que igual durmieron".
- Exe 03:33:20 copiado, cmp OK. Sin probar. Comparar con: puerta 462-495 ms, duelo 6 tirones de 25-43 ms (03:01).
### 2026-11-21 03:37 — registro 03:36 (exe 03:33): RESULTADOS
- D3DX nativo PROBADO: los 4 conversores dieron "64 filas iguales al juego, ahora nativo", sin ninguna diferencia. zlib bien, sin errores.
- Duelo (~73 s): 1 solo cuadro lento, de 36 ms (texturas subidas y sub_00375B70). Antes: 6 de 25-43 ms (03:01), ~13 más los de la búsqueda de daño (02:50). El tirón de 1771 ms fue la ventana en segundo plano.
- Puerta: 444-450 ms (antes 462-495). El giro atendió 134.452 idas y vueltas sin dormir (12 durmieron) en ese cuadro: el despertar no era lo caro. Lo caro es la cantidad, unos 3,3 us cada una (sem_post + sem_wait = 2 llamadas al sistema, más el puente del kit). En la Xbox una llamada al núcleo es una llamada común. Próximo paso: semáforos de Alchemy (sub_002F1D30/1D70/1CF0) en el espacio del usuario, sin llamadas al sistema.
- Carga del duelo 296 + 168 ms y menú ~145 ms: sin cambio.
### 2026-11-21 03:43 — semáforos de Alchemy con la cuenta en memoria (el usuario: "segui asi entonces mejora todo lo que se pueda")
- Capa POSIX del juego (text.asm): sem_init 0x2F1C70 (CreateSemaphore valor, 0x7FFFFFF), sem_destroy 0x2F1CC0, sem_trywait 0x2F1CF0, sem_wait 0x2F1D30 (WaitForMultipleObjects del semáforo + el evento de cancelación del hilo; sub_002F2120 lo activa), sem_post 0x2F1D70. El semáforo de Alchemy (vtable 0x46E9C0) los usa.
- src/sem_nativo.c: la cuenta va en memoria, al lado del mismo semáforo del núcleo, que solo se usa para dormir.
  - sem_init vacía el semáforo del núcleo y registra el handle.
  - post suma 1 y solo libera el semáforo del núcleo si alguien duerme (cuenta ≤ 0).
  - wait toma una unidad si hay, girando hasta 50 us; si no, resta y duerme con el sem_wait del juego (el mismo semáforo del núcleo y su cancelación).
  - trywait toma o falla con EAGAIN (errno del juego, sub_00359AAF).
  - Un handle desconocido usa las funciones del juego. Ajustes\semaforos_originales.txt vuelve a las del juego. "[SEM] ..." cada 30 s con lo contado.
- Prueba fuera del juego (scratchpad prueba_sem.c, el mismo algoritmo): 1.000.000 de idas y vueltas entre 2 hilos en 52 ms (0,05 us cada una; en el juego eran 3,3 us), con un tercer hilo que roba con trywait. Las cuentas terminan exactas (0, 0, 3).
- Visto: sub_00375B70 (20-31 % del único tirón que quedó en el duelo y de las cargas) es el KickOff de D3D, que espera el bit 0x10000 de 0x100410: el juego espera a que el walker del kit termine el segmento anterior (xbox_memory_layout.c). Es el costo de dibujar del kit, no una espera de más. No se tocó.
- Exe 03:43:23 copiado, cmp OK. Sin probar.
### 2026-11-21 03:47 — semáforos: el exe de 03:43 COLGABA el arranque, arreglado
- Registro 03:45: "[SEM] semaforos de Alchemy con la cuenta en memoria" y después un hilo en "NtWaitForSingleObjectEx: token=0x48000004 handle=...04F4 timeout=INFINITE" para siempre. Había 2 Yu-Gi-Oh.exe colgados; los cerré (taskkill) y puse Ajustes\semaforos_originales.txt para que abriera.
- Causa: el kit le da al juego tokens, no handles de Windows. sem_nativo.c llamaba a ReleaseSemaphore y WaitForSingleObject directo con el token: fallaban, el que dormía no despertaba nunca y la cuenta inicial no se vaciaba.
- Arreglo: todo acceso al semáforo del núcleo pasa por las funciones del juego, que van por el kit. El vaciado de sem_init es el sem_trywait del juego (llamado desde C con la pila invitada), el despertar es el sem_post del juego y el dormir es el sem_wait del juego. En sem_nativo.c ya no hay ninguna llamada a Windows.
- Exe 03:47:28 copiado, cmp OK. Se borró semaforos_originales.txt, así que corre la versión arreglada. Sin probar: si no abre, crear ese archivo de nuevo.
### 2026-11-21 03:50 — registro 03:49 (exe 03:47): semáforos PROBADOS
- Arranca y juega bien. "[SEM] post 479790 sin llamada / 51 despertando; wait 479788 sin dormir / 52 durmiendo". D3DX: 4 de 4 iguales. Sin errores.
- Tiempos, antes (registro 03:36) -> ahora:
  - arranque 147/158 -> 122/135 ms; 144 -> 112 ms;
  - carga del duelo 296 + 168 -> 209 + 135 ms;
  - puerta 450 -> 332 ms;
  - menú tras la puerta 257 -> 199 ms;
  - duelo: 1 tirón de 36 -> 1 de 26 ms.
- En la puerta el principal espera ahora el 33 % (antes 70-75 %): ~65 ms en el candado del juego 0x00631A64, compartido con el trabajador. El resto es trabajo del motor, repartido (ninguna función pasa del 4 %).
### 2026-11-21 03:53 — fluidez: diagnóstico apagado y medición barata
- Registro 03:49, en el duelo: 160-164 cuadros/s (165 Hz). El más largo de cada 10 s fue de 11-22 ms (la presentación tiene 6 ms). Nuestros agregados ([MASCOTA] lento, umbral 15 ms) aparecen una sola vez, al empezar (fundas 12, mascotas 26).
- El registro escribe ~30 líneas/s en el duelo: no cuesta nada.
- El muestreo de diag_carga.c suspendía los hilos del juego ~1000 veces/s. Ahora solo corre con Ajustes\diagnostico_cargas.txt (también el gancho de esperas). Las líneas de PNG siguen.
- [MASCOTA] costo cuenta además los cuadros de más de 8,5 ms y de más de 17 ms por cada 10 s: es lo que mide qué tan parejo va.
- Exe 03:53:04 copiado, cmp OK.
### 2026-11-21 03:58 — refresco exacto del monitor y resumen de cuadros (el usuario: "estables ... para todos los hz y todas las resoluciones")
- El kit generaba el vblank invitado con su propio reloj a un número entero (165) y presentaba con VSync real (Present intervalo 1). Los dos relojes de ~165 Hz se desfasaban: cada pocos segundos un cuadro perdía el refresco (03:49: 160-164 cuadros/s, el más largo de 11-22 ms cada 10 s). Eso es el "de 165 baja un poco".
- kernel_pacing.c: el calendario va en milihercios (vblank_due en double). locked_mhz(hz) usa el refresco real del monitor (DwmGetCompositionTimingInfo, rateRefresh):
  - con VSync, un pedido igual o mayor que el monitor (con 1 Hz de margen) es el refresco real;
  - un pedido a 1 Hz de real/k (60 en 120/240, 72 en 144, 82,5 en 165) es real/k exacto;
  - cualquier otro queda como se pidió.
  - Vale para cualquier monitor; la resolución no influye en el ritmo.
  - Registro: "[KERNEL] vblank a N.NNN Hz (pedido X, el refresco real del monitor)".
- nv2a_pb_d3d11.c: g_xbox_present_vsync, y xbox_VblankRelock() cuando cambia el VSync (al crear la ventana y en F2).
- nv2a_pb_d3d11.c: "[CUADROS] <pantalla>: N/s, tipico, 95%, 99%, peor; >8.5 ms, >17 ms; kit dibuja, Present" cada 5 s. La pantalla la pone mascota.c (g_nv2a_escena): "duelo" o "batalla 3D" (visor de batalla en los últimos 300 ms); si no, "fuera del duelo".
- [MASCOTA] costo cuenta también los cuadros de más de 8,5 y de 17 ms. El muestreo de diag_carga.c queda apagado (Ajustes\diagnostico_cargas.txt lo prende).
- Exe 03:58:48 copiado, cmp OK. Sin probar.
### 2026-11-21 04:03 — registro 04:02 (exe 03:58): fluidez medida por pantalla
- "[KERNEL] vblank a 165.013 Hz (pedido 165, el refresco real del monitor)": el bloqueo al refresco real funciona. Este monitor da 165,005-165,013 (casi exacto).
- Duelo y batalla 3D: 162-165 cuadros/s en casi todas las ventanas de 5 s. Típico 6,0 ms; 95 % 8,5; 99 % 9-10; peor 10-18 ms. Más de 17 ms: 0-2 cada 5 s. Más de 8,5 ms: 12-35 (1,5-4 % de los cuadros). Present 0,06 ms: no espera al monitor.
- El kit dibuja 2,9 ms por cuadro en el duelo (1,5 ms fuera del duelo, donde casi no hay cuadros de más de 8,5 ms). Las mascotas, 1,55 ms por dibujo y unos 55 por segundo. Con cerca de 3 ms del juego, el cuadro del duelo anda en el límite de los 6,06 ms. Los cuadros que suman una mascota o algo más se pasan a ~8 ms.
- Lo que queda para ganar margen: (a) que el kit dibuje más rápido (2,9 ms, ~400 dibujos: unos 7 us cada uno al traducir a D3D11); (b) menos dibujos de mascota por segundo; (c) repartirlos.
- 04:03: mascota.c: si el cuadro anterior tardó más de 1,15 refrescos, un dibujo de mascota que tocaba ahora espera un cuadro (como mucho RENDER_MS de atraso). Así no se suma 1,55 ms a un cuadro que ya viene tarde. Exe 04:03:51 copiado, cmp OK. Sin probar.
### 2026-11-21 04:10 — [LENTO] por cuadro (el usuario: "cuando cambia de turno baja a 155 ... cae a 110 cuando se abre el menu ... que se mantenga en 165 todo el tiempo")
- Las líneas [CUADROS] de 5 s promedian esas caídas (el contador del juego promedia 0,5 s): de 165 a 155 son ~5 cuadros perdidos en medio segundo; a 110, ~27.
- nv2a_pb_d3d11.c: cada cuadro de más de 7,5 ms y hasta 25 escribe "[LENTO] n ms pantalla: dibujos, kit ms, texturas ms, disco KB, shaders, agregados ms" (hasta 6000 por partida). Los de más de 25 ms siguen como [HITCH]. g_nv2a_extra_ms: mascota.c suma lo nuestro de cada cuadro (daño, cadena, fundas, mascotas y sus dibujos).
- Con un registro se ve, cuadro por cuadro en el cambio de turno y al abrir el menú, si pesa el juego, el dibujo del kit, las texturas nuevas o lo nuestro.
- Exe 04:09:54 copiado, cmp OK.
### 2026-11-21 04:17 — registro 04:13: qué tienen los cuadros lentos; mascotas con lugar; carta nueva sin marca
- [LENTO] (1751 cuadros de más de 7,5 ms en la partida, con un promedio de 8,5 ms y 415 dibujos):
  - el kit dibujó 3,2 ms en promedio;
  - texturas casi nunca (13 con más de 1 ms);
  - lo nuestro (agregados) pasó de 2 ms en 895 cuadros, casi todos con 2-3 ms: un dibujo de mascota;
  - 562 cuadros sin nada nuestro (8,9 ms en promedio): el juego más el kit.
  - Los peores: menú con 2430 dibujos (22 ms, kit 4,2) y batalla 3D con 895-937 dibujos (15-19 ms, kit 3,8-5,5).
- mascota.c: un dibujo de mascota se hace solo si el cuadro tiene lugar. El tiempo desde el último vblank (g_xbox_vblank_qpc, que el kit guarda en kernel_bridge.c) más 1,8 ms tiene que entrar en 3/4 del refresco, y el cuadro anterior no tiene que haber llegado tarde. Uno que esperó 2 x RENDER_MS se dibuja igual. [MASCOTA] costo cuenta "N cuadros esperaron lugar".
- Carta nueva (el usuario: "me salio la carta nueva pero no me aparecio como nueva"): k 736 (4837) salió en la ranura 2 y entró al cofre, y quedó en cartas_nuevas.txt (así que en la Biblioteca va con marca). En el premio no hubo "Nueva sobre la carta". Su huella (0C2CAFCE52A23E15, de euro/sp) está en huellas.txt. El kit ahora escribe "[MARCA] carta en pantalla con huella X ... (la lista espera Y)" para cada carta del tamaño de una carta que no coincide, mientras una marca espera. El próximo premio dice qué se dibujó.
- Exe 04:17:35 copiado, cmp OK. Sin probar.
### 2026-11-21 04:20 — perfil del dibujo del kit (el usuario: "que hacemos para mejorar lo del dibujo?")
- main.c: con Ajustes\perfil_dibujo.txt se pone RECOMP_TRACE=prof=1000,prof_out=<carpeta>\perfil_dibujo_salida.txt. Es el perfilador del kit (host_prof.c): muestrea todos los hilos, también el walker que dibuja, y lo escribe al cerrar el juego con offsets por módulo. Para simbolizarlos se usa el PDB (dbghelp).
- Creé Ajustes\perfil_dibujo.txt para la próxima partida. Se borra cuando esté el perfil (cuesta).
- Plan con el perfil, según lo que pese: (1) agrupar dibujos 2D consecutivos con el mismo estado (el menú hace 2430 por cuadro); (2) no repetir cambios de estado de D3D11 iguales; (3) subir vértices y constantes a un búfer circular, sin crear ni copiar por dibujo; (4) sacar el rehash de ~40 texturas por cuadro. Hoy son unos 7 us por dibujo: batalla 3D 900 dibujos ≈ 4-5,5 ms; menú 2430 ≈ 17 ms.
- Exe 04:19:45 copiado, cmp OK.
### 2026-11-21 04:3x — mascotas de vuelta; perfil del proceso entero leído
- El usuario: "las mascotas van ultra lentas". La regla de 04:17 (dibujar la mascota solo si el cuadro tiene lugar) dejó ~240 dibujos cada 10 s en vez de ~550 (843-967 cuadros esperando), porque el tick cae tarde en el cuadro. SACADA: las mascotas van con su reloj como antes.
- perfil_dibujo_salida.txt (host_prof, 1000 Hz, 93 s). Se simbolizó con scratchpad simbolos_perfil.py: dbghelp de Windows Kits\10\Debuggers\x64, sin carga diferida, con el PDB de una recompilación idéntica del exe de 04:19 en scratchpad prof0419. Ojo: el PDB solo tiene símbolos públicos, así que las funciones static caen en el público anterior.
  - Hilo principal: sub_00375B70 (KickOff, espera al walker) 30,5 %; ZwDelayExecution + sub_0011EC12 (la espera del cuadro) ~13 %; NtQueryVirtualMemory 5,3 %, desde nuestro rd_ok() de mascota.c.
  - Walker (nv2a-ack): espera girando ~33 % (RtlQueryPerformanceCounter + xbox_HostNapNs) más 13,5 % en NtWaitForSingleObject. Trabajo ~45 %: el backend D3D11 (static, cae en nv2a_pb_d3d11_register_from_env) 21 %, nv2a_vsh_run 7,3 %, nv2a_pb_exec_method 4,2 %, nv2a_tex_hash 3,9 %, memmove 2,5 %, nv2a_rt_own_stale 2,5 %. d3d11.dll y el driver solo ~3 %: lo caro es la traducción del kit.
  - nv2a_vsh_run lo llama, entre otros, rev_posiciones (dorsos y fundas, nuestro): corre el programa de vértices en la CPU para cada vértice de cada dibujo de dorso, en cada cuadro.
- ARREGLO mascota.c rd_ok(): guarda 8 regiones legibles durante 250 ms, para no llamar a VirtualQuery cada vez.
- Exe copiado, cmp OK. Sin probar.
### 2026-11-21 04:3x — Modo experimental (run_ahead) prendido por defecto (el usuario: "abrir el juego y que vaya a 165 clavado")
- El perfil de 04:22: el hilo principal pasa el 30 % en el KickOff de D3D (sub_00375B70), esperando al walker. run_ahead (RECOMP_DEBUG=run_ahead=1, la fila "Modo experimental" de F2) hace que el walker confirme el KickOff con hasta 1 segmento enviado de adelanto, así el juego y el dibujo del kit trabajan a la vez (en el port de RDR: el KickOff bajó de 22 % a 13 % y dio +5-8 fps).
- main.c fast_mode_from_ini(): por defecto Sí; "No" en F2 o en el ini lo apaga (se aplica al reiniciar). Registro: "[rdr] modo experimental: on (run_ahead)".
- Exe 04:32:51 copiado, cmp OK. Sin probar en este juego.

## REGLA DE TRABAJO (el usuario, 2026-11-21, después de ~20 horas y muchos duelos de prueba)
- Ser eficiente: NO pedir un duelo por cada dato y después otro duelo para el siguiente. Antes de pedir una partida, poner en el mismo exe TODO lo que haga falta medir y TODOS los arreglos posibles. Una partida tiene que cerrar el tema.
- El registro ya trae siempre: [CUADROS] cada 5 s por pantalla, [LENTO] por cuadro de más de 7,5 ms, [HITCH], [MASCOTA] costo, [SEM], [ZLIB], [D3DX], [MARCA]. Usar eso primero, sin pedir nada.
- No dejar en el exe cambios que empeoran lo que el usuario ve (pasó con las mascotas lentas): si hace falta un experimento visible, avisarlo antes.
- Lo que el usuario quiere: abrir el juego y que vaya a 165 clavado.

## 2026-11-21 04:39 — Dorsos/fundas: memo del programa de vértices en CPU (kit)
- nv2a_pb_d3d11.c rev_posiciones: cada lote boca abajo con programa corría nv2a_vsh_run por vértice en cada cuadro (perfil 04:19: vsh_run 7.3 % del hilo que dibuja, compartido con aspect/exec). Ahora: se leen las entradas, se hace huella de entradas + constantes + programa + start; si coincide con uno de 16 guardados, se copian filas/columnas/difuso sin correr el programa. vsh_run con cxt_write 0 es función pura de eso: resultado idéntico.
- [CUADROS] suma "dorsos reusados N, calculados M" cada 5 s (para ver el efecto en la próxima partida normal, sin prueba aparte).
- Huellas de texturas (3.9 %): NO tocado. La memoria del juego es un file mapping (sin GetWriteWatch) y la huella es la que nombra el pack; muestrear la textura podría perder cambios (visible). Se deja.
- Exe 04:39:00 copiado, cmp OK, sin probar.
- REGLA (usuario 2026-11-21): no cerrar pidiendo "jugá normal". Los cambios tienen que ser seguros por construcción; si aparece un log nuevo, lo leo solo, sin pedirlo.

## 2026-11-21 04:52 — Vértices leídos una vez por dibujo (kit)
- Perfil 04:19 por función (scratchpad por_funcion.py: límites de .pdata del exe prof0419, sin correr nada): en el hilo que dibuja, fetch() (leer un atributo de vértice) 11.9 %, xbox_HostNapNs 9.1 % (espera), nv2a_vsh_run 7.3 %, d3d11_on_draw_real 6.0 %, tex_hash 3.9 %, exec_method 3.0 %, rt_own_stale 2.5 %. QPC 24 % + nap = el hilo esperando trabajo, no costo.
- fetch se llamaba desde 3-5 pasadas por dibujo (uv clamp, aspecto, dorsos, mascotas, subida) sobre los mismos vértices. Ahora vtx_input guarda cada entrada de cada vértice del dibujo actual la primera vez (s_vc, hasta 4096 vértices) y las demás pasadas copian. Mismo resultado que vtx_input_raw. rev_posiciones y la subida expandida pasan por vtx_input (equivalentes: con stride 0 fetch ignora el índice).
- [CUADROS] suma "vertices leidos N, reusados M".
- Exe 04:52:36 copiado, cmp OK, sin probar.

## 2026-11-21 04:57 — Resultado de 04:52 (registro 04:55) y vuelta atrás
- Registro 04:55 (exe 04:52): duelo 164.6-164.8/s, peor 10-12 ms. PERO "kit dibuja" subió de 2.9 a 3.6 ms en duelo y de 1.6 a 2.1 fuera: la copia de vértices (s_vc) costó más de lo que ahorró (solo 22 % de reuso). QUITADA: vtx_input y la subida como antes.
- El memo de dorsos (04:39) dio 2.88-2.91 ms contra 2.9: neutro. Se deja.
- "modo experimental: off" en ambos registros: ygo_menu.ini tenía modo_rapido=No guardado (el default nuevo solo aplica sin la línea). Cambiado a "Sí (PC gama baja)" en Ajustes\ygo_menu.ini (respaldo en Ajustes\respaldos\ygo_menu_antes_modo_rapido.ini). Con eso el juego y el dibujo del kit (2.9 ms) se solapan en vez de sumarse.
- Exe 04:57:05 copiado, cmp OK.

## 2026-11-21 05:01 — Registro 04:59 (experimental prendido) y medición de la espera al kit
- Con run_ahead: duelo 164.7-165.0/s, >8.5 ms 6-7 cada 5 s (antes 9-21). Ayuda.
- [LENTO] 04:59: batalla 3D 10-16 ms por cuadro con el kit en 4 ms; duelo 7.5-9.6 ms con kit 2.8-3.2 y agregados (mascotas/fundas/daño, hilo del juego) 1.6-2.6 ms. O sea: los cuadros lentos NO los limita el dibujo del kit; repartir el dibujo del kit no los arregla.
- Agregado: sub_00375B70 (KickOff, espera del juego al kit) envuelto en diag_carga.c (PADS); kit g_nv2a_espera_ms. [LENTO] suma "juego espera al kit X ms", [CUADROS] el promedio por cuadro. Con eso el próximo registro dice si la batalla 3D es trabajo del juego o espera.
- Exe 05:01:45 copiado, cmp OK.

## 2026-11-21 05:10 — Registro 05:05 (duelo del usuario): dos arreglos
- El usuario: caídas puntuales al entrar al menú, al entrar al duelo, ~102 al atacar, ~60 por 1 s al terminar el duelo.
- "juego espera al kit" promedio 0.8-1.4 ms por cuadro: el juego casi no espera al kit. Los cuadros lentos de batalla (702) son trabajo del juego (900+ dibujos, ~10 ms del hilo del juego).
- Arreglo 1: cada 600 cuadros (LENTO 8401, 10801, 13201, 14401, 15001) el juego esperaba al kit 8-10 ms: el kit escribía ~600 líneas "[TEXPACK] miss" (RECOMP_TRACE=tex_pack puesto por defecto en main.c). Quitado el default.
- Arreglo 2: en cada ataque se volvían a subir 16-21 texturas (8-9 ms, LENTO 6070/12284). Kit: TexPooled s_tpool (96 texturas, 256 MB): una textura que sale del caché (desalojada o pisada con otro contenido) se guarda por (hash, tamaño, formato, niveles, generación del pack); si el mismo contenido vuelve, se reusa sin decodificar ni subir. Las que esperan al cargador del pack no se guardan. Se vacía con el volcado de texturas. [CUADROS] suma "texturas reusadas N (guardadas M, X MB)".
- Fin del duelo (312 ms): carga de la pantalla de premios en el hilo del juego (2 PNG 30 ms, disco 20, texturas 16, kit 17; el resto, código del juego). No tocado.
- Exe 05:10:12 copiado, cmp OK.

## 2026-11-21 05:16 — Ajustes automáticos según la PC
- Corrección: "Modo experimental" (modo_rapido) está oculto en F2 Y en Configuración.exe (el usuario, 11-14). Dije que estaba en Configuración: falso.
- main.c pc_detectar(): núcleos físicos, hilos (GetActiveProcessorCount), RAM. Decide:
  - modo experimental (run_ahead): sí con 4 o más hilos, no con 2-3. La línea modo_rapido del ini ya no se lee. RECOMP_DEBUG=run_ahead sigue mandando.
  - giro de 50 us antes de dormir (RECOMP_WAIT_SPIN_US): solo con 6 o más hilos. Ajustes\sin_giro.txt lo apaga igual.
  - log: "[PC] N nucleos (M hilos), RAM X MB: giro antes de dormir si/no".
- Kit (creación del dispositivo): tope de texturas guardadas (s_tpool_cap) = 1/10 de la memoria propia de la placa, entre 32 y 512 MB; placa integrada (<512 MB propios) 64 MB. Log "[PC] placa de video: X MB propios; texturas guardadas hasta Y MB".
- Exe 05:16:08 copiado, cmp OK.

## 2026-11-21 05:21 — Texturas del pack subidas a la placa en otro hilo (kit)
- Ya existía: el cargador del pack (nv2a_tex_pack.c) lee a RAM en segundo plano y precarga las usadas la sesión anterior (<pack>.usadas; ~1.2 GB en memoria). Lo que faltaba: la primera subida a la placa la hacía el hilo que dibuja (~0.4 ms por textura).
- Ahora (nv2a_pb_d3d11.c upj_*): una textura ART del pack de 256 KB o más se sube desde un hilo aparte (ID3D11Device_CreateTexture2D con los datos iniciales; el contexto del dibujo no se toca). Mientras tanto la entrada dibuja la textura original (como con el cargador del pack). Al terminar, va al pool (s_tpool) con la clave de la entrada y tex_bind la toma (gpu_wait). Si en 30 cuadros no está (falló o el pool no la guardó), se sube en el hilo del dibujo como antes (gpu_sync).
- El pool se rearmó: tpool_put (insertar por clave), tpool_has; tpool_park no guarda texturas provisorias (pack_wait o gpu_wait).
- [CUADROS] suma "subidas en otro hilo N (tarde M)".
- Exe 05:21:26 copiado, cmp OK. Sin probar.

## 2026-11-21 05:3x — ¿Se rompió algo? (registro 05:24)
- Registro 05:24: ~96 s de juego (present 15900). Sin errores ni texturas magenta/no decodificadas; shaders 0 fallos. Memoria al salir: RAM 1672 MB, video 516 MB (05:05: 1697 / 557). Una sesión corta NO prueba horas.
- main.c: RECOMP_TRACE=mem=60 por defecto: una línea [MEM] por minuto (RAM y video) para ver en cualquier sesión larga si la memoria crece.
- Lo nuevo tiene topes: pool de texturas 1/10 de la VRAM (máx. 512 MB), 64 trabajos de subida, el pack en RAM 1.5 GB (ya existía).

## 2026-11-21 05:32 — PNG: filtro de filas nativo (png_nativo.c)
- sub_00294C80 = libpng 1.0.8 png_read_filter_row (cdecl: png_ptr, row_info, row, prev_row, filter; row_info +4 rowbytes, +0xB pixel_depth). Confirmado leyendo el código generado (switch de 5 casos, bpp=(pixel_depth+7)>>3).
- png_nativo.c: Sub/Up/Average/Paeth nativos. Las primeras 64 filas de cada filtro se hacen con los dos y se comparan byte a byte: iguales -> "[PNG] filtro N: 64 filas iguales, ahora nativo"; distintas -> queda el del juego para ese filtro. Filtro desconocido: el del juego. Ajustes\png_original.txt: todo el del juego.
- sub_0028C280 (png_do_read_transformations) solo medido. [CARGA] png ahora dice "filtro X ms, transformaciones Y ms".
- PADS: sub_00294C80, sub_0028C280. CMake: png_nativo.c.
- Exe 05:32:49 copiado, cmp OK. Sin probar.

## 2026-11-21 05:4x — Revisión de código muerto y duplicados (sin tocar nada todavía)
- Método: recompilación completa del código propio y del kit con avisos -Wunused; búsqueda de funciones exportadas que nadie nombra (fuente propia + kit + CMake); cuerpos de función idénticos entre archivos; cada caso cruzado con ESTADO.
- Muertos en el port (seguros de quitar, esperan OK del usuario):
  - carta_as.c titulo(): el nombre debajo de la Carta As se quitó el 11-19 23:3x (pedido del usuario), pero titulo_hacer() sigue armando la textura del nombre en cada animación y pasándola al kit con alfa 0 (s_tit_box solo lo llenaba titulo()). Trabajo invisible.
  - raton.c name_page() (reemplazado el 11-08 21:58 por el camino del juego), cursor_now() (sin llamadas), glove_dump() (registro de clics viejo), abre_at (se escribe, no se lee).
  - main.c k_btn_style/get_btn_style/set_btn_style: fila "Botones en pantalla" sacada el 11-10 (era de RDR); el estilo automático sigue.
  - cadena.c borde(), mascota.c en_duelo(), perfil.c k_fx_nombre, s_retrato, s_retrato_de, cartel.c variable of, premios.c menos3, buscador.c ygo_buscador_abierto() (el comentario dice que lo usa raton.c; nadie lo llama).
- Diagnósticos que siguen corriendo: diag_memoria.c (bring-up del arranque, ~110 líneas [MEM] en cada inicio; prendido salvo YGO_MEM_TRACE=0); memo de dorsos (04:39, neutro medido: 2.88-2.91 vs 2.9 ms).
- Apagados por defecto (herramientas, se dejan): diag_escala.c (YGO_DIAG_ESCALA=1), muestreo de cargas (Ajustes\diagnostico_cargas.txt), perfil (Ajustes\perfil_dibujo.txt).
- Duplicados chicos: lienzo_de() (25 líneas) y sobre() en buscador.c y perfil.c; k_de() en cartas.c, premios.c, premios_extra.c, prestado.c.
- Kit: 23 avisos (funciones/variables sin uso en apu, d3d8, host_menu, kernel_bridge, kernel_hal, nv2a_pgraph_d3d11...). El kit es compartido con RDR: no se tocan sin revisar RDR.
- No muertos aunque parezcan: filas ocultas de F2/Configuración (el usuario las ocultó; se cargan y guardan), fuente_titulo4.h (la usa ft_dibujar para letras grandes), recomp_manual.c (lo llama el código generado).

## 2026-11-21 05:48 — Restos sin uso quitados (el usuario: "solo los seguros")
- carta_as.c: titulo(), tit_a(), titulo_hacer(), titulo_gpu() y su estado (s_tit, s_titg, s_tit_box, s_vh...) y el bloque vacío "the name" del cuadro. El nombre ya no se arma en cada Carta As; g_nv2a_as_tit[9] (alfa) queda en 0 como antes (el kit no dibuja nombre).
- raton.c: name_page(), cursor_now(), glove_dump(), abre_at. cadena.c: borde() (GROSOR y su nota quedan). mascota.c: en_duelo() y su declaración (la nota del orden de dibujo de las mascotas quedó). buscador.c: ygo_buscador_abierto(). main.c: k_btn_style, get/set_btn_style (s_btn_choice y el estilo automático quedan). perfil.c: k_fx_nombre, s_retrato, s_retrato_de. cartel.c: variable of. premios.c: menos3 local.
- Compila sin avisos de "sin uso" en src del port. No tocados: diag_memoria.c, memo de dorsos, duplicados (lienzo_de/sobre/k_de), kit (compartido con RDR).
- Exe 05:48:13 copiado, cmp OK.

## 2026-11-21 06:01 — Perfiles de calidad en Configuración.exe
- El usuario: "perfiles en Configuracion ... Bajo, Medio, Alto, Ultra y Recomendado" + "tambien personalizado".
- Configuracion.cs: fila "Perfil de calidad" arriba de Imagen (Recomendado, Bajo, Medio, Alto, Ultra, Personalizado; 6 idiomas). Detect(): hilos (ProcessorCount), RAM (GlobalMemoryStatusEx), placa con más memoria propia (registro, clase de pantalla, HardwareInformation.qwMemorySize; DriverDesc). Nivel: Ultra (8+ hilos, 15+ GB, 6+ GB video), Alto (6+, 11+, 3.5+), Medio (4+, 5.5+, 1.5+), si no Bajo. Fila de estado nueva: "Tu PC: N hilos, X GB de RAM, placa (Y GB) -> recomendado: Z".
- Cada perfil pone: render_scale, aniso, fps_juego, pack_art, mascotas. Bajo x1/No/60/No/No; Medio x2/x4/60/Sí/Sí; Alto x3/x8/120/Sí/Sí; Ultra x4/x16/Monitor/Sí/Sí. Ventana, pantalla completa, VSync, idioma y controles no se tocan.
- Cambiar a mano una de esas 5 (aquí o en F2) pasa el perfil a Personalizado. Primer arranque (sin ajustes): se aplica Recomendado. Ajustes de antes sin perfil: quedan como están (Recomendado si coinciden, si no Personalizado). "Valores por defecto" aplica Recomendado.
- Juego (main.c): fila oculta "perfil" (k_perfil) para que el guardado del juego no la borre; perfil_memoria() antes de cargar el pack: RECOMP_TEXPACK_MB por perfil (Bajo 256, Medio 768, Alto 1536, Ultra 2048) o por RAM para Recomendado/Personalizado (<6 GB 384, <12 768, <24 1536, si no 2048). El [PC] del registro lo dice.
- F2 no tiene la fila de perfil (oculta): se elige en Configuración.
- Exe 06:01:28 y Configuración.exe 06:01:31 copiados, cmp OK. Sin probar.

## 2026-11-21 06:05 — Perfil de calidad también en F2, en vivo y sincronizado
- El usuario: "se puede poner en f2? cambiar en tiempo real? ... que siempre sea lo mismo" que Configuración.
- Kit host_menu.c/.h: xbox_MenuAddFirst (fila debajo del primer título), xbox_MenuSetChoice(clave, valor), xbox_MenuChoice(clave), xbox_MenuLoading().
- main.c: la fila "perfil" ahora es la primera de Gráficos en F2 (antes oculta). set_perfil aplica en vivo las 5 filas (render_scale, aniso, fps_juego, pack_art, mascotas) con la misma tabla que Configuracion.cs y guarda; mientras se carga el ini no aplica nada (las filas del archivo mandan). get_perfil pasa a Personalizado si una de las 5 no coincide. pc_detectar suma la memoria de video (registro, igual que Configuración) y el nivel con las mismas reglas: Recomendado da lo mismo en los dos.
- f2_idiomas.c: "Perfil de calidad", Recomendado, Bajo, Medio, Alto, Ultra, Personalizado en 5 idiomas.
- Sincronía: los dos escriben perfil= y las 5 claves en Ajustes\ygo_menu.ini; cada uno toma los cambios del otro (xbox_MenuPoll / FileSystemWatcher).
- Exe 06:05:55 copiado, cmp OK (Configuración.exe de 06:01 sirve igual). Sin probar.

## 2026-11-21 06:11 — Fondo completo del Perfil/Decks de los Oponentes en HD (opción C)
- El fondo de atrás de todo (fondo_completo.jpg) era la textura del menú del juego (FDE76864287C3300) de 640x480 estirada. El usuario eligió la C de modding/muestras/fondo_menu_*.png: la entrada 4x del pack (4096x2048 BC3) recortada a 2560x1920, con nitidez suave (UnsharpMask 2/60/2), +8 % contraste y +6 % color.
- tools/perfil_recursos.py: fondo_completo() la arma desde modding/texturas/textures_bc3.pack (sin el pack, del volcado como antes). Generada y copiada a Mods-Extras\perfil\fondo_completo.jpg (cmp OK). La anterior queda en modding/texturas/perfil/juego/fondo_completo_640_antes.jpg.
- Sin cambios de código del juego (perfil.c la escala a la ventana como antes). Sin probar.
- 06:1x: el usuario: "usamos la original la b porque se ve demasiado nitido y borra la c". fondo_completo() ahora devuelve la entrada del pack tal cual (sin nitidez/contraste/color), 2560x1920; regenerada y copiada (cmp OK). La C borrada (scratchpad). Las muestras A/B/C siguen en modding/muestras/fondo_menu_*.png.

## 2026-11-21 06:14 — Flechas del cartel ocultas con un mazo abierto
- El usuario (captura del panel "Mazo de Trusdale"): "oculta las flechas arriba cuando se abra este menu de los decks".
- perfil.c: las dos flechas verdes del cartel se ocultaban solo con la carta grande; ahora con cualquier mazo abierto (s_p_mazo.ch >= 0). que_hay(): con un mazo abierto (páginas 1/2) ni flechas ni medallones cambian de página; un clic ahí cae en NADA y cierra el mazo, como cualquier clic fuera del panel. Al cerrarlo vuelven.
- Exe 06:14:48 copiado, cmp OK. Sin probar.

## 2026-11-21 18:55 — Packs a la mitad sin perder calidad (BC1) y Mods-Extras\Contenido
- El usuario: "podemos comprimir algo para que no sea tanto el peso sin perder calidad?" + "deja voces, texturas por separado y las demas juntalas en 1 sola".
- Medido: el 84 % de las texturas BC3 son opacas (todo el canal alfa en 255). tools/achicar_pack.py las pasa a BC1 (mismo bloque de color, 8 bytes en vez de 16; c0<c1: extremos cambiados e índices xor 1; c0==c1: índices 0). Comprobado por bloque al convertir y aparte con Pillow (DXT5 viejo vs DXT1 nuevo, ~330 texturas al azar): diferencia 0 en todos los píxeles. Las que tienen transparencia quedan BC3.
- Resultado: textures_bc3.pack 4088 -> 2370 MB; textures_es 934 -> 538; en/de 905 -> 526; fr/it 894 -> 515; cartas.pack 391 -> 196. Mods-Extras 11 GB -> 6.5 GB. También ocupa la mitad en memoria de video y en el presupuesto RAM del pack (entran el doble de texturas por perfil).
- Kit: XRTP_FLAG_BC1 0x20 (nv2a_tex_pack.h; la entrada lleva 0x10|0x20, e->bc3 = comprimida, e->bc1 = BC1); tamaños /2; nv2a_pb_d3d11.c: TEX_BC_OF (0 texeles, 1 BC3, 2 BC1), DXGI_FORMAT_BC1_UNORM y pitch de 8 en tex_alloc, tex_bytes, hilo de subida y subida directa. Metal: las BC1 van del juego (no se usa en Windows). Packs viejos BC3 siguen andando.
- Herramientas: parchar_pack.py convierte a BC1 la textura nueva si la del pack es BC1 (error si la nueva tiene transparencia; --deshacer salta respaldos de otro tamaño). perfil_recursos.py lee DXT1. cartas_mod.py e idiomas_pack.py achican al final (achicar_aqui, escribe encima: los enlaces duros siguen).
- Packs viejos BC3 (respaldos/packs_bc3, 8.8 GB) borrados con OK del usuario (se pueden rearmar desde los BC1: alfa 255).
- Imágenes PNG/JPG: re-guardar sin pérdida da 0-4 %, no se tocó.
- Carpetas: auto, buscador, cartas, fundas, perfil, tapetes -> Mods-Extras\Contenido. Quedan aparte texturas\ y voces\. Código: mods_carpeta.c/.h (ygo_mods_a/ygo_mods_w: "\Mods-Extras\Contenido" si existe, si no "\Mods-Extras": la estructura vieja sigue andando) en auto.c, buscador.c, carta_as.c, fundas.c, mascota.c, perfil.c, premios_extra.c; main.c ya buscaba cartas.txt/textos.txt/packs en subcarpetas. copiar_al_juego.sh, buscador_recursos.py y Editor de cartas (recompilado 18:38) apuntan a Contenido. LEEME de Mods-Extras actualizado (paquete y carpeta del juego).
- Carpetas movidas en la carpeta del juego, cartas.pack nuevo copiado (cmp OK). Exe 18:37:05 copiado, cmp OK. Sin probar.

### 2026-11-22 18:41 — Cierre a mitad de duelo (registro 18:17:19): nombre compartido soltado de más; protección y registro del culpable
- Registro: duelo de torneo contra el personaje 24 (mascotas 716 y 21), una batalla cambiando sus modelos (m924, m500). Al desarmar la escena vieja (sub_0019F5E0 -> sub_00187050 ...) sub_001B4A00 suelta un nombre compartido: sub_00030BD0 baja sus usos ([e+4]) y en 0 llama sub_001AD030(pool; e), que lo saca de la tabla ordenada ([pool]: +4 cantidad, +0x10 arreglo; entrada = [pool, usos, letras]) con búsqueda binaria (sub_00188430) y lo libera (sub_00188700). El nombre NO estaba en la tabla: índice == cantidad, el corrimiento dio -4 bytes (ecx 0x3FFFFFFF) y sub_003574A0 (memmove) leyó fuera: access violation, juego cerrado. Un nombre que no está = se soltó una vez de más antes. Sin volcado: no se sabe quién. Misma clase que el cierre del 20/11 22:31 (desarme, memoria liberada).
- Las mascotas no aparecen en ese momento: el duelo anterior con 3 mascotas (rival 19) y el de la mascota 21 (rival 23) cerraron limpios; durante el duelo la mascota no suelta nada. No se culpa a nada sin prueba.
- Nuevo src/nombres.c (en CMakeLists; rename_pads.py: sub_00030BD0, sub_001AD030 -> _pad):
  - sub_00030BD0: si los usos ya son 0 (o dieron la vuelta) no se bajan otra vez: "[NOMBRES] referencia de mas" con nombre, pila y lo que se quitó antes con esa entrada o nombre.
  - sub_001AD030: busca la entrada en la tabla (binaria como el juego, si no cae, una por una, dos veces); si no está, no se quita ni se libera (antes: cierre, o sacaba a un vecino equivocado): "[NOMBRES] NO esta en la tabla" con nombre, usos, tabla, pila, otras entradas con el mismo nombre y las quitadas anteriores (últimas 8192, con su pila) de esa entrada o nombre: ahí aparece la soltada de más.
  - Al final de cada duelo: "[NOMBRES] N nombres quitados, N salvados, N referencias de mas" (mascota.c, sub_00045750).
- El proyecto pasó a D:\Portear Juegos Xbox Clasica (el usuario lo movió por el peso). ygo-apex/build reconfigurado desde cero en D: (VS 18 2026, x64, ClangCL, mismas opciones); el build viejo quedó en ygo-apex/build_viejo_C (borrar solo con OK).
- Exe 18:40:46 (build nuevo en D:) copiado a C:\Emuladores y Juegos\Yu-Gi-Oh Port\Yu-Gi-Oh.exe, cmp OK. Sin probar.

### 2026-11-22 18:46 — Arkana lleva el Mago Oscuro II
- El usuario: "el personaje arkana tiene el mago oscuro comun ... debe llevar el mago oscuro 2". mascota.c k_rival[18] (Arkana): 2 (Mago Oscuro, modelo 34) -> 3 (Mago Oscuro II, modelo 960). Su funda sigue a la mascota (fundas.c k_por_mascota): ahora mago_oscuro_2 (arte y tapete existen en Mods-Extras\Contenido).
- Exe 18:45:53 copiado, cmp OK. Sin probar.

### 2026-11-22 18:52 — Los datos del perfil van en Partidas\Perfil
- El usuario: copió la carpeta a 2 PC sin Partidas; partidas nuevas bien, pero el historial de duelos y los rivales eran los suyos. "todo lo que sea datos o vaya de la mano con un perfil ... vaya en la carpeta Partidas".
- main.c: YGO_PERFIL = <port>\Partidas\Perfil (se crea al arrancar). Pasan ahí (antes YGO_AJUSTES): perfil.ini (perfil.c: nombre, foto), duelos.txt (estadisticas.c: historial y rivales), cartas_nuevas.txt y premios_registro.txt (premios_extra.c), prestados.txt (prestado.c), respaldos\ de savedata (guardado.c). Ajustes\ queda solo con ajustes (ygo_menu.ini, launcher.ini, mascotas_encuadre*.txt, interruptores de diagnóstico).
- Sin migración automática (en las otras PC, Ajustes\ tiene los datos del usuario mezclados: no se pueden separar). En esta PC se movieron a mano los archivos de Ajustes\ a Partidas\Perfil\ (ygo_menu_antes_modo_rapido.ini volvió a Ajustes\). En las otras PC esos archivos de Ajustes\ quedan sin usar.
- Configuración.cs: el texto del autoguardado (6 idiomas) dice Partidas\Perfil\respaldos (antes "Ajustes\respaldos" sin escapar: salía \r). Recompilado 18:51:19. LEEME (paquete y carpeta del juego): el perfil va en Partidas\Perfil; para otra PC desde cero, copiar sin Partidas. Actualizar.cs ya salteaba Partidas\.
- Exe 18:51:00 y Configuración.exe copiados al juego, cmp OK. Sin probar.

### 2026-11-23 20:48 — Opción "Guante visible al grabar (OBS)"
- El usuario: OBS (capturando el mouse) no graba la manito. En su video de las 20:33 el guante sale en la pantalla de carga y desaparece al entrar al menú.
- Causa: el guante es el puntero de Windows (raton.c glove_make: CreateIconIndirect, cursor de clase de la ventana). No va en la imagen del juego, y OBS lo captura aparte y no siempre.
- Kit (xboxrecomp-apex nv2a_pb_d3d11.c): nv2a_d3d11_pointer_sprite(bgra premultiplicado, w, h, hx, hy) o NULL. pointer_present() lo dibuja en el back buffer después de menu_present (en el juego y en los videos), donde está el puntero de Windows, si el puntero está sobre el área de la ventana (WindowFromPoint o ventana en primer plano). Tiene su propio shader y blend ONE/INV_SRC_ALPHA.
- raton.c: glove_make guarda una copia de los píxeles. glove_cursor: con la opción activa, el puntero de Windows queda transparente (blank_cursor) y el guante (normal o apretado) va al kit. Sin la opción, todo como antes. ygo_glove_in_frame_set/get.
- F2 > Controles: "Guante visible al grabar (OBS)" (raton_guante_imagen, No por defecto). Traducido en f2_idiomas.c y Configuración.cs (etiqueta y ayuda, 6 idiomas, grupo g_controls).
- Límite: los paneles que son ventanas aparte (Perfil, buscador, carteles, onda) quedan encima del guante dibujado, y la captura de juego de OBS tampoco los graba.
- Exe y Configuración.exe 20:48 copiados al juego, cmp OK. Sin probar.
- 20:51: el usuario: "no hay nada en F2". Las filas de k_ctl_rows se agregan por índice en main.c: la nueva era [4] y no se agregaba (el [4] anterior, "Jugar cartas rápido", pasó a [5] y sigue afuera). Ahora xbox_MenuAdd(&k_ctl_rows[4]). Exe 20:50:54 copiado, cmp OK. Sin probar.

### 2026-11-23 20:56 — "Modo grabación (OBS)": guante y paneles dentro de la imagen
- El usuario: "quiero que todo salga en obs es para grabar para youtube"; en OBS la mano desapareció al volver del Perfil. Esa sesión corrió sin la opción (ygo_menu.ini 20:34 no tenía raton_guante_imagen), con el puntero de Windows.
- La opción (misma clave raton_guante_imagen, No por defecto) ahora se llama "Modo grabación (OBS)" en F2 > Controles, f2_idiomas.c y Configuración.cs (etiqueta y ayuda, 6 idiomas). Prende el guante dibujado (raton.c) y además los paneles (capas.c).
- capas.c/capas.h nuevos: buscador.c, cartel.c, onda.c, perfil.c y premios.c incluyen capas.h, que redirige UpdateLayeredWindow a ygo_capa_ulw. Con el modo activo: copia la imagen del DIB de origen (premultiplicada; la transparencia constante del panel se aplica a los píxeles), la manda al kit con su lugar en pantalla y actualiza la ventana con SourceConstantAlpha 0 (la ventana queda invisible). Al apagar el modo, cada ventana recupera su última imagen (devolver) y se vacían las capas del kit.
- Kit nv2a_pb_d3d11.c: nv2a_d3d11_layer(key=HWND, px|NULL, w, h, x, y de pantalla) y nv2a_d3d11_layers_clear(). layers_present() (antes de menu_present y pointer_present, en el juego y en los videos) dibuja las ventanas visibles en su orden de Windows, recalculado cada 15 cuadros. Comparte shader y blend con el guante (ptr_init).
- Límite: lo que ya estaba abierto al prender el modo pasa a la imagen cuando el panel se vuelve a dibujar.
- Exe 20:56:22 y Configuración.exe 20:56:33 copiados, cmp OK. Sin probar.
- 21:0x: el usuario: "todo lo que agregamos está dado vuelta" (captura: el botón del Perfil al revés; el guante está bien). capas.c tomaba el DIB como de abajo hacia arriba porque GetObject devuelve biHeight positivo aunque el DIB sea de arriba hacia abajo. Todos los paneles crean DIB de arriba hacia abajo (biHeight -h): ahora siempre se lee así. Exe copiado, cmp OK. Sin probar.

## 2026-11-24 12:5x — Estrellas de nivel 12 centradas (Dragón final azul - ojos)
- El usuario: "las de 12 estrellas tienen corridas las estrellas hacia la izquierda en vez de estar como siempre a la derecha". Buscado en los datos (bits 25-28) y en cartas.txt: la única de 12 es Dragón final azul - ojos (4386); de 11, Tutor de recaudación (4380), que ya entra bien y no se tocó. Desde x 107.5 la fila de 12 llegaba al marco de la izquierda y sobraban ~6 px a la derecha (igual en el .tga original).
- tools/texto_cartas.py: nivel 12 con `EST_X0_12 = 113.25` (centrada bajo el recuadro del nombre) y `_papel_vertical`: el papel bajo la fila, columna por columna, del de justo arriba y justo abajo (con `_papel_estrellas` quedaba una franja más clara y se comía el borde del marco, visto por el usuario en la muestra 1); bajo el atributo, las columnas vecinas. También limpia donde estaba la primera estrella (x 8.5). Muestra 2 aprobada.
- Rehecha solo esa carta (scripts en el scratchpad, comprobados: con el código viejo daban idénticas a las actuales): la grande en los 5 idiomas (mejoradas/texto_cartas, idiomas/es|en|fr|de|it) y su casillero en las hojas de miniaturas texdb008 (1AA3A6196BC7CB9A en mejoradas/miniaturas e idiomas/xx/miniaturas; E367B4ECEB147934 y la variante AF2EF9A5BF553D75 en modding/cartas/salida/hd_xx, las que usa el juego con el mod).
- Packs parchados con tools/parchar_pack.py: textures_es/en/fr/de/it (carta + hoja) y cartas_es/en/fr/de/it (las 2 hojas); comprobado leyendo la entrada del pack. Copiados a Mods-Extras\texturas y Mods-Extras\Contenido\cartas, cmp OK. Respaldo de los PNG en respaldos/2026-11-24_estrellas12; bytes viejos de los packs en respaldos/pack_*. Sin probar en el juego.

## 2026-11-24 14:2x — Cartas: monstruos sin estrellas, ícono de juego rápido / continua, rearmado completo
- El usuario: "y aplicaste esto con todas las cartas que tengan las estrellas corridas? ... no puede ser parte de mods esto" y "las cartas de juego rapido, se les perdio su signo ... un rayito".
- Estrellas: fuera de la de 12, la estrella de la derecha está en su lugar en todas. Pero había monstruos con texto largo que salían como mágicas ("[CARTA MÁGICA...]" en el renglón de las estrellas, sin estrellas ni ATK/DEF): es_monstruo (la raya de ATK/DEF en el .tga) fallaba. Eran 26 en es, 9 en en, 20 en fr, 19 en de, 18 en it (y 5390, una Mágica Continua, tomada por monstruo). texto_cartas.py main() e idiomas_pack.py cartas() ahora pasan `mon = tipo < 21` (de los datos del juego).
- Juego rápido / continua: las mágicas de clase 2 son las dos cosas; se distinguen solo por el ícono del .tga original (comparado contra 4905 rayo y 4798 ∞, mismo resultado en los 5 idiomas). texto_cartas.py: `RAPIDAS` (12: 4815 4904 4905 4906 4909 5118 5129 5152 5214 5350 5432 5437), `subtipo()` y `sub_icono()` (disco gris con borde claro y el rayo o el ∞ blanco antes del "]" de la etiqueta). Las otras 26 de clase 2 llevan el ∞. Trampas continuas, de campo, equipo y ritual siguen con la etiqueta escrita. Las cartas del mod (cartas.txt) no llevan ícono.
- Rearmado completo: idiomas_pack.py (5 idiomas, 13:06-14:1x) y cartas_mod.py (MOD OK, 14:22), buscador_recursos.py y perfil_cartas.py (OK). medir_estrellas.py sobre las 1137 x 5 y las 443 HD: ninguna fuera de lugar.
- Copiados: textures_es/en/fr/de/it.pack a Mods-Extras\texturas (cmp OK), cartas\ (diff OK), buscador\ (diff OK), perfil\cartas (diff sin diferencias). Sin probar en el juego.

## 2026-11-24 14:2x — Caja negra del duelo (registro que sirve cuando algo se traba)
- Origen: registro de una PC con GT 1030 / 12 GB / HDD: el duelo quedó clavado tras la moneda de Jirai Gumo (robada con Cambio de corazón). El registro no decía en qué paso: después de "coin toss: side 0 called" y la animación (se300) no hay más nada del juego; los cuadros siguen a ~60/s, memoria 1,5 GB, sin errores; 360 underrun de XA2 (audio, no la traba). Leído el código: la moneda (sub_00090D20, pasos 0-4; repite solo con la carta 0x150F en el campo) y el ataque de Jirai Gumo (sub_0009B010: pide la moneda para el que la controla, estado 0xC8, si falla la mitad de los LP) terminan solos. Causa NO encontrada.
- caja_negra.c (nuevo, en CMakeLists): en cada lectura del mando (mando_directo.c, después de ygo_mouse_apply) compara 17 valores del duelo (escena 0x657148, acción 0x65F948, pedido 0x65F91C/0x65F920, paso 0x65F92E, resultado 0x65F930, cara 0x660B8C, ataque 0x65F8DE/0x65F8DF, batalla 0x6666E0, llamada 0x66BA8E y su carta 0x66BA80, cadena 0x66BF9A, fin 0x65F894, LP 0x65D0A4/0x65E49C, teclas 0x6589E6) y guarda los cambios en un anillo en memoria (2048 líneas). Notas de otros módulos: raton.c (clic, moneda: espera / elegida). Vuelca las últimas 400 al registro ("[CAJA] ===== ... =====", "estado ahora", tiempos) solo si: 20 s sin cambios del duelo con 3+ teclas o clics ("POSIBLE DUELO TRABADO"); el juego deja de leer el mando 8 s en pleno duelo (hilo vigía, 1 vez por segundo); se cierra la ventana en pleno duelo (recomp_exit_hook_add); fatal.c (error del juego); main.c unhandled_filter (falla).
- Costo: unas 17 lecturas y comparaciones por lectura del mando; nada al disco salvo el volcado.
- Ruido: kit nv2a_pb_d3d11.c, [LENTO] tope 6000 -> 300 líneas por sesión (las [CUADROS] ya lo resumen cada 5 s).
- Exe 14:27 compilado (cmake de VS 18) y copiado al juego, cmp OK. Sin probar.

## 2026-11-24 14:4x — Buscador: 82 miniaturas dadas vuelta
- El usuario (captura del buscador): "se ven al reves algunas" (Sociedad benéfica garbosa, Polimerización, Monstruo renacido; Ritual de ilusión negro bien).
- Causa: tools/buscador_recursos.py daba vuelta (FLIP_TOP_BOTTOM) todas las celdas de las hojas texdb, pero solo las giradas (último campo 1, 1107) vienen así; las 82 sin girar (campo 0) ya están derechas en la hoja. No venía del rearmado de hoy: las celdas del mod y del juego son iguales.
- Arreglo: la vuelta y el giro solo si gir. minis.png rearmado (1189); revisadas las 82 sin girar y 10 giradas: todas derechas. Copiado a Mods-Extras\Contenido\buscador (cmp y diff OK). El Perfil no usa las hojas (cartas grandes HD): no le pasa. Sin probar en el juego.

## 2026-11-24 14:5x — Cortes de sonido en PCs de 4 hilos; revisión del registro de la GT 1030

- Registro de la PC con GT 1030 (4 núcleos/4 hilos, HDD): 367 `[XA2] underrun` en el duelo (desde t=191 s); en la PC local, 0. La reserva de audio era 4×512 muestras = 43 ms (marca baja 21 ms) y el hilo del APU con prioridad normal.
- Cambio en `ygo-apex/src/main.c` (`fast_mode_from_ini`, tras detectar la PC): con 4 hilos o menos pone `RECOMP_AUDIO_BUF_COUNT=8` (85 ms, marca baja 43 ms) y `RECOMP_AUDIO_THREAD_PRIORITY=highest`, salvo que el usuario ya las haya puesto. PCs de más de 4 hilos: sin cambios. No es un ajuste de F2/Configuración (lo decide la PC, como run_ahead).
- La pausa de 3 s del cuadro 1537 NO es un error: entre `[d3d11] paused (window in the background)` y `resumed` (ventana minimizada).
- Pausa de 2 s del cuadro 1126: el juego leyendo 18 MB del HDD al empezar el duelo (776 ms de disco). Sin cambio.
- shaders.pak 6 s en el HDD: en segundo plano, sin trabar cuadros. Sin cambio.
- Compilado y copiado a `C:\Emuladores y Juegos\Yu-Gi-Oh Port\` (cmp igual). No probado en el juego.

## 2026-11-24 21:xx — Trailer de 2 minutos (HyperFrames)

- Carpeta `trailer/`: `maquetas.png` (4 maquetas aprobadas), `cortar.py` (24 tomas desde `Videos\2026-11-24 20-26-08.mkv` y `Videos\parte 2\parte 2.mp4`, a 30 fps, aceleradas las de mascotas x7,3 y fundas/tapetes x3,9), `proyecto/` (HyperFrames: `index.html` + 7 escenas en `compositions/`).
- Música: `sound/BGM/ds_yugi.xau` del juego. Los .xau son "SFXB" con un RIFF adentro cuyo tipo dice "XWV " en vez de "WAVE": PCM 16 bits estéreo 22050 Hz; cambiando esas 4 letras lo lee ffmpeg. 137,1 BPM; las escenas empiezan en compases (1,751 s).
- Pedidos del usuario: sin "sin trabas"; mostrar el paso por mascotas/tapetes/fundas con "más de 90 mascotas, fundas y tapetes para el jugador"; zooms, cortes y transiciones dinámicas.
- Salida: `trailer/Trailer Yu-Gi-Oh Port.mp4` (120 s, 1920x1080 30 fps). Los chips del cierre ("6 idiomas", "Texturas mejoradas", "Se actualiza solo") salen de la memoria del proyecto: a confirmar por el usuario.
- 21:4x Borrador 1 (`trailer/Borrador trailer 20s.mp4`, carpeta `trailer/borrador/`): al usuario le gustaron solo los textos (serif dorada, caída con sacudida); los zooms, no. NO BORRAR (pedido del usuario).
- 22:0x Borrador 2 (`trailer/Borrador 2 trailer.mp4`, carpeta `trailer/borrador2/`, tomas con `trailer/cortar_borrador2.py`): sin zooms. Cadena, Cambiar Turno, ataque de Duende de Gemini (Parte 1, modelo original), dragones 3D del premio, "El clásico de Xbox", personajes del Duelo Triple y del Individual (x3,4), "Ahora en PC". Fuera los monstruos del mod (Golem Centinela, Caballero Real m158, Vidente m191: no sirven para mostrar "el clásico").
- 22:2x Borrador 3 (`trailer/Borrador 3 trailer.mp4`, carpeta `trailer/borrador3/`, `trailer/cortar_borrador3.py`): pedidos del usuario: primeras tomas más largas (4 golpes; la cadena desde 120,0 s; "Cambiar Turno" a 0,4x porque el cartel dura 0,25 s en el juego), sin Duelo Triple, personajes del Individual más lentos (x1,6, de Yami Yugi a Duel Computer). 17,6 s. Borradores 1 y 2 se conservan.
- 22:4x Borrador 4 (`trailer/Borrador 4 trailer.mp4`, `trailer/borrador4/`, `trailer/cortar_borrador4.py`): "Cambiar Turno" a 0,5x exacto (60 cps → 30, sin cuadros repetidos; a 0,4x se trababa), fundidos de 0,2 s entre tomas y paso por negro en las placas, placa final "27 duelistas + 3 Duel Computer" (contado en Decks de los Oponentes: 30 casillas). El cursor del mouse está en las grabaciones: no se puede sacar sin regrabar.
- 22:5x Traba de "Cambiar Turno" ARREGLADA de verdad: el corte ponía `-t` después de `-i` (límite de la salida), así que en tomas lentas el clip quedaba a la mitad (c02: 30 cuadros en vez de 60) y el trailer congelaba el último cuadro. Ahora `-t` va antes de `-i`. Medido en el video final: todos los tramos sin cuadros repetidos (mpdecimate). Borrador 4 re-exportado.
- 23:0x Borrador 5 (`trailer/Borrador 5 trailer.mp4`, `trailer/borrador5/`, `trailer/cortar_borrador5.py`): "Cambiar Turno" reemplazado por la presentación de Mako ("¡Pescar es mucho más fácil...", Parte 1 104,2 s); ataque de las Gemelas a 0,5x y 6 golpes (163,9-165,2 s). 19 s, sin cuadros repetidos (medido).
- 23:1x Borrador 6 (`trailer/Borrador 6 trailer.mp4`, `trailer/borrador6/`): orden Gemelas (0,75x, 4 golpes, primera), cadena, Mako, dragones; 18,1 s; medido sin cuadros repetidos.
- 23:4x Parte "Ahora en PC" (`trailer/Parte ajustes.mp4`, `trailer/ajustes/`), aparte del borrador 6 para unir al final: placa "Ahora en PC"; menú F2 real (captura) con Resolución x4, Pantalla ancha 16:9, Filtrado x16; cortina antes/después de texturas en la pantalla de título; configurador cambiando entre 5 idiomas (sin los cuadros en blanco al redibujar) con "5 idiomas" y "Todo se guarda solo". Música ds_yugi desde 18,149 s (continúa al borrador 6). 14,3 s. Clips de Parte 1 (0-48 s), no usados antes.
- 23:5x Parte ajustes v2: sin la comparación de texturas (va después, con más texturas); FPS con números que ruedan (60, 90, 120, 144, 165, 200); onda de agua dorada (2 anillos) al aparecer cada texto. 12,9 s. Pendiente: aplicar la onda a los textos del borrador 6 al unir.
- 23:5x Parte ajustes v3 (`trailer/Parte ajustes v3.mp4`): onda reemplazada por el brillo blanco que recorre las letras (muestra 4 de `trailer/Muestras de onda.mp4`, 0,95 s); FPS: cada número sube en 0,32 s, salta apenas y lo recorre un brillo. Pendiente: aplicar el brillo a los textos del borrador 6 al unir.
- 00:0x Parte ajustes v4 (`trailer/Parte ajustes v4.mp4`): FPS como cuentakilómetros de 3 rodillos (muestra 1 de `trailer/Muestras de FPS.mp4`), 060 → 090 → 120 → 144 → 165 → 200, uno por golpe.
- 00:2x Parte ajustes v5 (`trailer/Parte ajustes v5.mp4`): "Todo se guarda solo" → "Todo en un lugar / imagen, controles, voces y juego".
- 00:2x Parte texturas (`trailer/Parte texturas.mp4`, `trailer/texturas/`): antes/ahora con la línea dorada en 5 zonas: pantalla de título (la de antes), menú principal (Parte 1 46,5/47,75 s), biblioteca (52,15/55,25), carta Sangan (58,0/59,2), campo de duelo (120,4/120,65). Pares elegidos midiendo la diferencia de imagen entre cuadros con la etiqueta del juego "originales"/"mejoradas". 6 golpes por zona, nombre con brillo que se va a mitad del barrido. Música desde 31,049 s (sigue a ajustes). 13,8 s.
- 00:5x Parte "Lo agregado" (`trailer/Parte agregado.mp4`, `trailer/agregado/`, tomas `trailer/cortar_agregado.py`, escena generada por `trailer/agregado/generar.py`): perfil con 10 mascotas, 5 fundas, 5 tapetes (1 golpe cada una, velocidad real; elegidas entre las selecciones que duran ≥0,53 s, medidas por cambio de imagen), recuadro dorado sobre la columna activa y título Mascotas/Fundas/Tapetes que cambia; placa cuentakilómetros 000→100; Mazos de los rivales; Estadísticas (foco en el récord; la pantalla dura 155,7-158,9 s); Buscador (foco 1437-1900 × 210-907, medido); Premios de cada rival (foco medido). Recuadros medidos con regla sobre el cuadro a resolución completa (el usuario: que no se vean corridos). 25,2 s; música desde 44,849 s.
- 01:1x Parte agregado v2 (`trailer/Parte agregado v2.mp4`, 27 s): fundas 9 que terminan en Sabio Oscuro (la que queda puesta al pasar a tapetes; antes cambiaba en el corte); rivales: grilla 4 golpes y corte a Mai abriendo su mazo (151,1 s, x0,64); estadísticas: récord y después el historial (foco 971-1673 × 332-978); biblioteca: "Te la pueden dar" (foco 20-292 × 133-307) y después el buscador. Medido: sin cuadros repetidos.
- 01:2x Parte agregado v3 (`trailer/Parte agregado v3.mp4`, 21,7 s): rivales en una toma continua a velocidad real (149,6 s, 6 golpes: se abre el mazo de Yami Yugi y el de Mai; antes el corte hacía saltar el mouse); sin premios (repetía los personajes); biblioteca solo con el buscador (no hay grabación pasando el mouse por cartas con 'Te la pueden dar' cambiando: en el video del 24 siempre dice Trusdale).
- 01:3x Parte mouse y voces (`trailer/Parte mouse y voces.mp4`, `trailer/mouse_voces/`, tomas y audio `trailer/mouse_voces/cortar.py`, 11,4 s): 'Jugá todo con el mouse' sobre parte 2 91,3 s (12 golpes, velocidad real: Tomate de Mystic de la mano al campo y abre Tributo torrencial); 'Voces en español latino' sobre Kaiba en parte 2 67,95 s (14 golpes) con su voz (x2, limitador) y la música bajada a 0,2 desde el corte. Música desde 66,545 s (empieza en golpe). Medido: sin cuadros repetidos; voz -14 dB RMS sobre música -27,5 dB.
- 01:4x Parte clímax (`trailer/Parte climax.mp4`, `trailer/climax/`, `cortar.py`, 18,4 s): parte 2 a velocidad real, 4 tomas: 129,7 s Caballero Real -2800 'Je... nada mal' (7 golpes, título 'Cada duelo como lo recordás'); 144,4 s Golem -800 y Caballero '¡Argh... eso dolió!' (12); 166,75 s golpe final y Victoria (8); 172,15 s Kaiba pierde con su voz (15). Sonido del juego x2; música 0,45 en el duelo y 0,2 con Kaiba; desde 77,924 s. Medido: sin cuadros repetidos.
- 01:4x Parte cierre (`trailer/Parte cierre.mp4`, `trailer/cierre/`, 7,9 s): placa dorada YU-GI-OH! / The Dawn of Destiny / Ahora en PC / DESCARGALO / Link en la descripción, con brillo; música desde 96,305 s con salida de 1,8 s. No se unió nada: el usuario aprueba primero clímax y cierre.
- 01:5x Cambios pedidos: cierre sin 'Link en la descripción' (`Parte cierre v2.mp4`); clímax con 'Con las cartas nuevas / Duelos renovados' (antes 'Cada duelo como lo recordás', que no cuadraba con cartas nuevas) (`Parte climax v2.mp4`). Nueva parte 'Cartas nuevas' antes del clímax (`Parte cartas nuevas.mp4`, `trailer/cartas/`, 14 s): 16 fichas de la biblioteca de parte 2 (Soldado del Brillo Negro, Caius, Caballero Real, Golem Centinela, Abare Ushioni, Cosmoremolacha, Ciclón del Cosmos, Carga del Poderoso, Escudo drenador, Golpe de Viento de Ala de Fénix, Berenjena, Hombre de Nieve Comilón, Acantilado, Chinche Hedionda, Naturia Vanidosa, Cerezas Naturia), 2 golpes cada una, velocidad real, dentro de tramos quietos medidos; franja de 200 px tapa la barra de la biblioteca. Música: cartas 77,924 s, clímax 91,928 s, cierre 110,309 s. Nada unido todavía.
- 01:6x Trailer completo (`trailer/Trailer completo.mp4`, `trailer/final/`, armado por `final/armar.py`, 118,2 s): las 8 partes aprobadas (inicio, ajustes, texturas, agregado, mouse y voces, cartas nuevas, clímax, cierre) en `final/compositions/` con sus assets en `final/assets/<parte>/`; música continua ds_yugi desde 0,025 s en una sola pista (`final/assets/musica.wav`) con las bajadas de voces (0,2) y clímax (0,45/0,2) y salida de 1,8 s; voz de Kaiba y sonido del clímax aparte. Sin los fundidos largos a negro del final de cada parte: paso corto por negro en cada corte (0,12 + 0,18 s). Brillo agregado a las dos placas del inicio. Los proyectos de cada parte no se tocaron. `Trailer completo v2.mp4` (el bueno): la música cruda quedaba ~7 dB más baja en el inicio que en las partes aprobadas (esas tenían la música nivelada a ~-15 dB RMS); armar.py ahora la nivela con ganancia suave (objetivo -17, entre 1 y 3,5) y limitador 0,89; el audio se mezcló con ffmpeg (música + voz en 71,772 s + juego en 91,903 s) sobre el video ya exportado. Medido: -14,7 dB en el inicio, voz y clímax como en sus partes; sin tomas congeladas (solo quedan quietas las placas, las imágenes de texturas y las fichas).

- 2026-11-25 Trailer terminado. El usuario se llevó `Trailer completo v2.mp4` (está en Descargas) y pidió borrar lo demás: todo el contenido de `trailer/` (borradores, partes, proyectos de HyperFrames, scripts de corte, maquetas, muestras, logs) y los temporales fueron a la Papelera de reciclaje (126 elementos). `trailer/` quedó vacía; para retocar el trailer habría que recuperar `trailer/final` y las carpetas de las partes de la Papelera.

- 2026-11-25 02:51 Cartas de los rivales: suman las dos variantes de la historia. Causa (datos del xbe): Yami Yugi (duelista 28) da 35 cartas en la variante 0 y 42 en la 1; sub_0008C530 elige la variante por el progreso y tras los créditos volvió a la 0 (registro: "variante 0: 35 cartas, faltan 3"). Ahora cuentan la unión de ambas (listas 0x453830/0x453630, r 0 y 1): cartel de premios (premios.c), "Cartas que da X" y desbloqueo Ganado del mazo prestado (prestado.c cartas), cartel de quién da cada carta (premios_extra.c da_carta) y el panel del mazo del rival en el Perfil (perfil.c mazo_sumar: las de la otra variante van a "Puede sumar al azar"). Sin cambios: el premio real tras ganar y el mazo que arma el juego (variante actual). prestados.txt ya tenía a Yami Yugi como Ganado. Exe compilado y copiado, cmp igual. Sin probar.

- 2026-11-25 03:17 Premios de Torneo (Duelo Triple) y dos barras en el Perfil (maquetas aprobadas: mismo estilo que el cartel de premios, sin romper la estetica). (1) premios.c ygo_premios_triple_tick, llamado desde raton.c sub_001057D0 (pantalla del Triple): nivel +0x3E8 (0..14) y los 3 personajes en orden en +0x41C + 4*(3*nivel+i) (leido en el intercambio 0x105D74 y en sub_001056D0). Cuenta la lista propia de torneo de cada rival (0x453FC8 + 16d, variantes 0 y 1, peso y bits de nivel como el cartel de duelo; la lista comun por nivel no se cuenta). Mismo cartel arriba a la derecha (misma caja, borde dorado, 92 % opaco): titulo Premios de torneo de estos rivales y una fila por rival (nombre | Te faltan / Ya tienes, columnas alineadas), 6 idiomas. Registro [PREMIOS] triple nivel ... El Triple usa sub_0008C9B0 porque el setup [0x66DCDC] != 0 (sub_00025BC0). premios_extra.c exporta ygo_nombre_personaje. (2) prestado.c cartas_torneo + revisar_ganado: el mazo queda Ganado al completar Duelo O Torneo (tambien en ygo_prestado_fin y revisar_todos). perfil.c: la linea bajo el record ahora son dos grupos en la misma fila, Duelo: X de Y [barra] y Torneo: X de Y [barra]; Faltan N cartas para ganarlo usa el menor de los dos. f2_idiomas.c: Duelo/Torneo %d de %d en 6 idiomas. Exe compilado y copiado 03:17, cmp igual. Sin probar.

- 2026-11-25 Cartel del Triple: los nombres no coincidian (capturas del usuario; registro: personajes 1 2 0). +0x41C es el ORDEN de pelea (0..2, el jugador lo cambia), no el personaje. El personaje = u32 0x455D50 + 8*(3*nivel + orden) (u32 personaje, u32 PV; leido en sub_00103B70 / sub_00104EE0, que lo nombran con el texto 0x56 + personaje; 0x1B = rival al azar, se muestra ???). Verificado contra las capturas: nivel 1 orden 2 1 0 = Yugi 3000, Trusdale 2000, Shimon 2000; nivel 2 orden 1 2 0 = Tristan, Joey, Tea. El cartel se lee en cada cuadro, asi que sigue el orden cuando el jugador intercambia rivales. Exe compilado y copiado, cmp igual. Sin probar.

- 2026-11-25 Premio: pantalla de los 3 dioses (Cual quieres?). sub_00025BC0 sortea 3 grupos (ranura 0..2 de [PREMIOS], bit 1<<(15-ranura) de cada entrada de las listas; premio_ranuras del mod) de N cartas (duelo comun 3; torneo/Triple 1, 2, 3 o 5 segun [setup+0x18] y [+0x30]) del rival en [setup+4] (en el Triple, el ultimo). sub_00101A00 pasa al cofre el grupo MEM32(MEM32(pantalla+0x10)+0x74) desde premio(+0x1C) + 0x5C + 10*grupo. Comprobado con el usuario (03:46, cofre 4048/4837/5061 = grupo 0): Obelisco (medio) = grupo 0. Slifer/Ra = 1/2 sin confirmar. El ranura de [GUARDADO] es la ranura de la partida, no el dios. Cartel por dios: maqueta mostrada, el usuario no decidio.

- 2026-11-25 DIOSES DEL PREMIO (que dios es cada grupo; grupo = ranura 0/1/2 de las lineas [PREMIOS] y bit 1<<(15-grupo) de las listas, premio_ranuras 1/2/3 del mod):
  - Obelisco (medio) = grupo 0 = premio_ranuras 1. CONFIRMADO (cofre 03:46: 4048/4837/5061, las del grupo 0).
  - Slifer (izquierda): el usuario lo eligio contra el rival 18, pero las cartas de los grupos 1 (4459, 5061, 4460) y 2 (4893, 4050, 4050) ya estaban en 3 copias: el cofre no cambio y no se puede saber cual era. SIN CONFIRMAR si Slifer es 1 o 2; Ra es el otro.
  - Para confirmarlo: cartas.c sub_000D9720 ahora escribe [COFRE] dios elegido: grupo N (pantalla en edi, MEM32(MEM32(pantalla+0x10)+0x74)). Exe compilado y copiado, cmp igual.

- 2026-11-25 03:55 DIOSES CONFIRMADOS (registro [COFRE] dios elegido, el usuario eligio Slifer): Obelisco (medio) = grupo 0 = premio_ranuras 1; Slifer (izquierda) = grupo 1 = premio_ranuras 2; Ra (derecha) = grupo 2 = premio_ranuras 3.

- 2026-11-25 Cartel de los dioses (maqueta A): premios.c ygo_premios_dioses_tick desde raton.c sub_00101FD0 (pantalla Cual quieres?, confirmada: llama a sub_00101A00 y lee +0x74 / +0x1C). Lee los 3 grupos ya sorteados (premio [+0x1C], [+0x3C] cartas, u16 en +0x5C + 10*grupo + 2i) y cuenta por dios las cartas distintas con 0 copias. Mismo cartel arriba a la derecha: titulo Cartas nuevas de cada dios y filas Slifer / Obelisco / Ra (orden de pantalla) con Nuevas: N de M; el dios marcado en dorado. Se va al elegir (cartas.c sub_000D9720 -> ygo_premios_dioses_hecho) y se rearma en la proxima pantalla (1,5 s sin llamadas). 6 idiomas. Registro [PREMIOS] dioses: .... Exe compilado y copiado, cmp igual. Sin probar.

- 2026-11-25 04:16 PERFILES (carpeta por perfil y, adentro, una por ranura del juego; maqueta aprobada):
  - Estructura: Partidas\<perfil>\UDATA, TDATA (la partida del juego, todas sus ranuras; RECOMP_SAVE_DIR), Partidas\<perfil>\respaldos\ (guardado.c), Partidas\<perfil>\Ranura N\ (lo nuestro de la ranura N-1 del juego: perfil.ini, duelos.txt, prestados.txt, cartas_nuevas.txt, premios_registro.txt). Perfil activo: Ajustes\perfil_activo.txt (UTF-8).
  - Juego: src/perfil_dir.c (nuevo, en CMakeLists): ygo_perfiles_iniciar(root) en main.c (reemplaza Partidas y Partidas\Perfil), ygo_ranura() = u32 0x65189C (0..4), ygo_perfil_ruta/wruta. estadisticas.c, prestado.c, perfil.c (nombre y foto, recarga en su hilo), premios_extra.c (marcas Nueva y premios_registro) leen y recargan por ranura al cambiar. F2 > Salir: fila "Perfil (se cambia en Configuración)" con el nombre, solo lectura (cambiar pide reiniciar), 6 idiomas.
  - Migracion (juego y Configuración, misma regla): Partidas\UDATA/TDATA y Partidas\Perfil\respaldos -> Partidas\<nombre de perfil.ini, Dennis>; los archivos de Perfil -> Ranura 1 (el usuario jugaba la ranura 0). Probada en una copia en el scratchpad: bien.
  - Configuración: tools/launcher/Perfiles.cs (nuevo, en compilar.ps1): recuadro Perfil arriba (lista, Nuevo perfil con nombre, Abrir su carpeta, resumen ranuras con datos / duelos), boton JUGAR COMO <perfil>, 6 idiomas. Lo demas bajo 78 px.
  - Exe y Configuración.exe compilados y copiados al juego, cmp igual. Sin probar. La carpeta del juego todavia tiene el formato viejo: se mueve sola al abrir Configuración o el juego.
- 2026-11-25 04:18 Configuración: el boton vuelve a decir JUGAR (el usuario: "pone jugar nomas"); el perfil se ve en su recuadro. Compilado; falta copiarlo al juego (Configuración estaba abierta).
- 2026-11-25 04:2x Configuración copiada al juego (cmp igual). La migracion ya corrio al abrirla: Partidas\Dennis con UDATA, TDATA, respaldos y Ranura 1. Ranura 1 = indice 0 del juego (u32 0x65189C = 0): la carpeta numera desde 1.
- 2026-11-25 04:2x AYUDAS (el usuario: una sola opcion para todos los carteles): la fila "Quien da cada carta" pasa a "Ayudas" (F2 y Configuración, 6 idiomas): Si, con fotos / Si, sin fotos / No. Clave nueva "ayudas" en ygo_menu.ini (el usuario tenia quien_da=No y con la misma clave arrancaba todo apagado). No apaga: quien da cada carta (premios_extra.c, s_quien) y los carteles de premios.c (rival, Duelo Triple, dioses) via ayudas() = ygo_quien_get() != 2; la fila oculta cartel_premios ya no los apaga. Exe y Configuración compilados y copiados, cmp igual. Sin probar.

- 2026-11-25 04:3x REVISION GENERAL ANTES DE SACARLO (pedido del usuario). Que se reviso y que salio:
  - Compilacion completa de nuestros 42 .c (touch + build): sin advertencias reales (solo deprecated del CRT y una de kernel.h).
  - Registros del usuario (registro.txt 04:26, registro_anterior.txt 04:22): ningun error, fatal, excepcion ni duelo trabado.
  - Perfiles en uso real: el duelo de las 04:21 quedo en Partidas\Dennis\Ranura 1\duelos.txt y el respaldo en Partidas\Dennis\respaldos; Partidas solo tiene Dennis (no se creo un UDATA suelto). El cartel de los dioses aparecio en el registro ([PREMIOS] dioses ...) y el grupo elegido coincide ([COFRE] dios elegido).
  - Rutas viejas: grep de UDATA/Partidas/savedata en src, tools y LEEME. guardado.c y estadisticas.c leen savedata.dat por xbox_translate_path (sigue a RECOMP_SAVE_DIR): bien. Actualizar.cs saltea Partidas\ y Ajustes\ (perfil_activo.txt se conserva). armar_paquete.ps1: el zip no lleva Partidas.
  - Arreglado: nombres de perfil con letras que la pagina de codigos ANSI no tiene (ej. japones) rompian la carpeta de la partida (RECOMP_SAVE_DIR va como ANSI): ahora pasan a '_' en el juego (perfil_dir.c limpiar) y en Configuración (Perfiles.Limpiar), misma regla; probado en Configuración con "Denis Ñoño" (queda igual), "Hermano/2" -> "Hermano2", japones -> "__", vacio -> "Jugador 1".
  - Arreglado: punteros del juego leidos sin control en el cartel de los dioses (premios.c) y en el registro del dios elegido (cartas.c): ahora solo si estan en la RAM del juego (0x10000..0x08000000).
  - Arreglado: textos que decian Partidas\Perfil\respaldos (ayuda de "Guardar al ganar" en Configuración, 6 idiomas) y el LEEME (paquete y carpeta del juego): ahora explican Partidas\<perfil>\Ranura N y como llevar un perfil a otra PC.
  - Restos de pruebas: grep TEMPORAL/prueba_/forzar/TODO en src: ninguno. Diagnosticos pesados (diag_carga muestreo, diag_escala, diag_memoria) solo con archivo/variable que no esta en la carpeta del juego.
  - Nota: la carpeta de desarrollo tiene Partidas\Jugador 1 vacia (Configuración abierta ahi a las 04:17); inofensivo.
  - Exe, Configuración.exe y LEEME.txt copiados al juego, cmp igual.
  - Pendiente para despues de sacarlo: el usuario quiere saber si se puede romper el limite de las cartas.

- 2026-11-25 05:xx INVESTIGACION (para despues de sacarlo): mas cartas, mas personajes, mas torneos. Datos leidos del xbe y del codigo traducido:
  - Cartas: tablas fijas en .data del xbe, pegadas: imagen/ID u16 en 0x5FE692 (base k 0x5FE690), datos u32 en 0x5FEF78 (empieza justo donde termina la de imagenes: no hay lugar para crecer en el sitio). Referencias en el codigo traducido: 862 a 0x5FE692, 205 a 0x5FE690, 328 a 0x5FEF78, solo esas 3 bases, en 10 archivos recomp_*.c (ninguna direccion suelta dentro de la tabla). El tope 0x473 (1139) aparece en ~6 comparaciones; 0x472 2 veces; 0x474 31 (mezclado con otros usos, hay que mirarlas una por una).
  - Cofre (copias por carta): u8 en [bloque de partida] + 0xCC1 + k (base de sub_0002DCA0; 17 accesos con +0xCC1). Esta dentro del bloque que guarda la partida (0x6518A8, 0x41C8 bytes por ranura, 5 ranuras en savedata.dat de 84480 bytes): crecer ahi pisaria lo que sigue. Limites por carta: u8 en 0x665896 + k (otro bloque guardado). Lista de limitadas del juego: 61 entradas en 0x450BC0.
  - Premios: tablas de punteros (0x453630 A, 0x453830 B, 0x453FC8 torneo; 32 duelistas x 2 variantes, u32 puntero + u16 cantidad) a 117 listas entre 0x450D48 y 0x4535E8. Como son punteros, se pueden apuntar a listas nuevas mas grandes sin tocar codigo.
  - Personajes: personaje -> duelista s8 en 0x423D60: 36 entradas reales (0..35; 27-30, 34, 35 -> 27 sin cartas; 31-33 -> 29-31), despues basura. Tablas de premios: 32 duelistas (0..31) todas usadas por 31; pegadas una a otra. 0x423D60 tiene 1 referencia en el codigo; 0x453830 4, 0x453630 3, 0x453FC8 1 (faciles de redirigir con envoltorios). Desbloqueo por personaje: u32 0x651958 + 4*ch (dentro de la partida; +0xAC es el contador de la historia).
  - Duelo Triple: 15 niveles x 3 rivales en 0x455D50 (u32 personaje, u32 PV; 5 referencias); el orden de cada nivel vive en el objeto de la pantalla (+0x41C, 15x3) y se guarda en la partida (sub_001056D0 copia 15 entradas de 12 bytes a 0x651958+0xB8). Lista comun de premios por nivel de torneo: 5 (0x453F78), nivel t = (nivel-1)/3.
  - Conclusiones en la respuesta al usuario (posible / tal vez / no). Sin cambios de codigo.

- 2026-11-25 04:4x RESPALDOS e INFORME DE ERROR (maquetas aprobadas):
  - Configuración: tools/launcher/Respaldos.cs (nuevo, en compilar.ps1). Recuadro Perfil: boton "Respaldos" (borde dorado) abre la lista de Partidas\<perfil>\respaldos\savedata_*.dat (Hoy/Ayer/fecha HH:mm, el mas nuevo primero); Restaurar pide confirmacion, no deja con el juego abierto ni si el perfil no tiene partida, copia antes la partida actual como savedata_<ahora>.dat y despues el respaldo elegido encima de UDATA\4b4e001e\0061006D1822\savedata.dat. Sin descripcion del duelo (la maqueta la mostraba): no hay de donde sacarla con certeza.
  - Informe de error: el juego escribe Ajustes\ultimo_error.txt (fecha, motivo) en fatal.c y en el filtro de fallas de main.c (perfil_dir.c ygo_marcar_error); sus avisos ahora dicen que se abra Configuración y se toque Informe de error. Configuración muestra arriba un aviso rojo (Armar informe / Descartar) y tiene un boton fijo "Informe de error" abajo (botones de abajo reacomodados a 150 px). El zip va al Escritorio ("Informe Yu-Gi-Oh <fecha>.zip"): registro.txt, registro_anterior.txt, xbox_kernel.log, escalado_log.txt, version, ygo_menu.ini, launcher.ini, perfil_activo.txt, ultimo_error.txt, *.dmp; nunca la partida. Lee los archivos compartidos (el juego puede tenerlos abiertos), borra la marca y abre el Explorador con el zip marcado. 6 idiomas.
  - Probado fuera del juego: deteccion y descarte de la marca (Hoy 04:41 -> null). Restaurar y el zip no se probaron (escriben en la partida real y en el Escritorio).
  - Exe y Configuración.exe compilados y copiados, cmp igual.
- 2026-11-25 04:5x Informe de error: antes de armarlo pregunta (Si/No) y explica que se crea un .zip en el Escritorio con registros, ajustes y version, nunca la partida (el usuario). 6 idiomas. Configuración copiada, cmp igual.

- 2026-11-25 05:xx PAQUETE PARA SUBIR (el usuario: el programa a GitHub, lo demas a Google Drive):
  - Pesos medidos (carpeta del jugador): programa ~147 MB (Yu-Gi-Oh.exe 142,4; SDL3.dll 2,7; shaders.pak 0,9; Configuración 0,2; mods_puntero 0,7); Contenido 1.525 MB (cartas 761,9 = cartas.pack 195,7 + 5 idiomas 101,3; perfil 586,4; tapetes 125,9; fundas 47,4; buscador 3,8; auto 0,1); textures_bc3.pack 2.369,9; textures_es 537,7, en/de 525,7, fr/it 515,1 (no hay pt); voces 69,0; Game 1.979,5 (nunca va).
  - Faltaba en el paquete: armar_paquete.ps1 no llevaba Contenido ni los packs de idioma (un jugador nuevo quedaba sin cartas nuevas, Perfil, fundas, tapetes, buscador ni mascotas).
  - Faltaba tambien: 3 archivos del port viven dentro de la carpeta del juego (card\normal\card_ura_rival.tga; effect\special_igb\se002_mascota.igb y se002_ataque.igb), copiados a mano por copiar_al_juego.sh. Ahora van en Mods-Extras\Contenido\juego\ y el juego los copia solo al arrancar a la carpeta que tiene default.xbe si faltan o cambian de tamaño (perfil_dir.c ygo_juego_overlay, desde main.c tras xbox_path_init; registro [JUEGO]). copiar_al_juego.sh tambien los pone ahi. Puestos en la carpeta del jugador (cmp igual).
  - Arreglado: mascota.c copiaba el modelo propio en exe\Game\monster: con el juego pegado en una subcarpeta de Game fallaba y volvia a la ruta compartida que cerraba el juego. Ahora usa la carpeta real del juego (g_ygo_game_dir).
  - Nota: la copia "original" de desarrollo (D:\...\Yu-Gi-Oh TDOD) tiene mezclados se002_ataque/se002_mascota y algunos .png.png de volcados; no es pura.
  - armar_paquete.ps1: dist\YuGiOh-TDOD-PC.zip (GitHub; Actualizar.cs toma el primer .zip del release) y dist\drive\ con tools/zip_drive.py: YuGiOh-Contenido.zip (obligatorio), YuGiOh-Texturas-HD.zip, YuGiOh-Idioma-ES/EN/FR/DE/IT.zip, YuGiOh-Voces-Latino.zip; rutas Mods-Extras/... para descomprimir tal cual en la carpeta del port; los .pack sin comprimir (STORED). Origen de Drive: -Extras (por defecto el Mods-Extras del jugador).
  - LEEME: arriba IMPORTANTE version EUROPEA (PAL, 7.254.016 bytes), QUE BAJAR (GitHub / Drive obligatorio / opcionales) y CÓMO INSTALAR; tambien Respaldos e Informe de error. LEEME de Mods-Extras y PEGAR ACA EL JUEGO (con la nota PAL) actualizados. Copiados a la carpeta del jugador.
  - VERSION del repo dice 1.0.
  - Exe compilado y copiado al juego y a la raiz de desarrollo, cmp igual.
  - 2026-11-25 05:04 paquete armado y revisado: GitHub dist\YuGiOh-TDOD-PC.zip 41,5 MB (19 entradas, exe/LEEME iguales al jugador). Drive dist\drive\: Contenido 1.480,6 MB (10248, incluye los 3 de juego\), Texturas-HD 2.369,9, Idioma ES 537,7 / EN 525,7 / DE 525,7 / FR 515,1 / IT 515,1, Voces 67,9. Todas las rutas de Drive empiezan con Mods-Extras/. Sin pt (no existe textures_pt.pack). Sin probar en juego: copia de Contenido\juego al arrancar, arreglo de mascota, Respaldos e Informe de error.
  - 2026-11-25: las voces latinas van dentro de YuGiOh-Idioma-ES.zip (605,6 MB, 3113 archivos); ya no hay Voces-Latino.zip (zip_drive.py; con 3er argumento "es" rearma solo ese). LEEME y LEEME Mods-Extras actualizados (en dist, en el zip del programa 42,2 MB y en la carpeta del jugador). VERSION queda 1.0 (confirmado por el usuario).
  - 2026-11-25: GitHub. Repo = dennisabalo18-bot/yugioh-tdod-pc (Actualizar.cs Repo; Configuración recompilado, copiado al jugador y a dist, metido en YuGiOh-TDOD-PC.zip 42,2 MB). README.md bilingüe como el de Red Dead, fotos en docs\img (duelo, perfil, biblioteca, mazos, triple, dioses: capturas del usuario y del video 2026-11-24 20-26-08.mkv, sin el contador de FPS de arriba; mascotas.gif = mascotas cambiando; titulo.gif (arriba) = pantalla de titulo del video 2026-11-25 05-30-07). Faltan los links de Drive en el README (LINK_CONTENIDO, LINK_HD, LINK_ES...). El release: tag v1.0, titulo "Yu-Gi-Oh TDOD PC v1.0", un solo zip.
  - 2026-11-25 05:45 SUBIDO A GITHUB (con OK del usuario): repo git en la raiz del proyecto (main, remoto dennisabalo18-bot/yugioh-tdod-pc), 229 archivos: ygo-apex (sin recomp\gen ni los .h con dibujos del juego: raton_guante, fuente_juego, fuente_titulo(4), danio_digitos), tools, patches (0001-0074 = format-patch blinx2/portability..rdr-port del fork; 0075 = git diff de los cambios sin commitear del fork, 35 archivos), docs, README, ESTADO. .gitignore deja afuera juego, modding, builds, exe/dll/pak, Ajustes, Partidas, dist, trailer. El usuario habia subido a mano una version vieja (README + docs): merge -s ours, queda la nuestra. README: idiomas en tabla (uno por fila), seccion Compilar. Faltan los links de Drive (LINK_*).
- 2026-11-25 Links de Drive puestos en el README (Contenido, Texturas-HD, ES/EN/FR/DE/IT). Autoria del repo pasada a dennisabalo18-bot (348090789+dennisabalo18-bot@users.noreply.github.com): el mail adrianelmanya76@gmail.com figuraba como el contribuidor Dennis-co; historia reescrita y subida con --force-with-lease. Claude queda por Co-Authored-By.
- 2026-11-25 Instalacion para jugador nuevo: 'Extraer todo' de Windows mete los zip de Drive en una carpeta con su nombre (Yu-Gi-Oh Port\YuGiOh-Contenido\Mods-Extras\...) y el Contenido quedaba fuera de lugar. Ahora el juego (mods_carpeta.c ygo_mods_acomodar, desde main.c antes de leer Mods-Extras; registro [MODS]) y Configuracion (Program.Acomodar) juntan toda carpeta Mods-Extras hasta 3 niveles abajo (no Game/Partidas/Ajustes) en la Mods-Extras del port y borran las carpetas vacias. Probado con un arbol falso (C y C#): OK. LEEME y README: pasos mas claros (Examinar y elegir la carpeta) y seccion PARA ACTUALIZAR. armar_paquete.ps1 -SinDrive (solo el zip del programa). Exe y Configuracion compilados, copiados al jugador (cmp igual); zip del programa rearmado (41,5 MB, testzip OK). Actualizador revisado: repo correcto, toma el titulo del release y el primer .zip; hasta que exista el release v1.0 el boton da error 404. No probado en el juego.
- 2026-11-25 16:3x Cartel de los dioses no salio en el torneo (captura del usuario en Cual quieres?, registro 16:26 sin ninguna linea [PREMIOS] dioses ni sin cartel; premio sorteado: rival 5, modo 1, 5 cartas por dios). Causa probable: el control de punteros de la revision (solo 0x10000..0x08000000) es posterior a la vez que funciono (04:21) y descarta la memoria contigua del kit en 0x80000000 (XBOX_CONTIG_BASE; el registro muestra [CONTIG] alloc 0x81630000 justo despues del sorteo). Ahora premios.c ygo_ram_ok acepta 0x10000..0x08000000 y 0x80000000..0x88000000 (tambien cartas.c, dios elegido). Si igual no sale, el registro dice [PREMIOS] dioses: sin cartel con las direcciones o la cantidad/marcado. Exe compilado y copiado a la raiz; NO copiado al jugador (el juego estaba abierto). Sin probar.
  - 16:3x Registro de esa partida (exe viejo): [COFRE] dios elegido: grupo 99 = el mismo control descarto el objeto de la pantalla (confirma la causa). Elegido Obelisco (ranura 0): 4550 (k 528) 0 -> 1 copias, [PREMIOS] carta nueva en el cofre: k 528 y marca Nueva sobre 1 carta: bien. Exe nuevo copiado al jugador (cmp igual); zip del programa rearmado.
  - 16:3x Segunda partida con el exe de 16:30: otra vez [COFRE] dios elegido: grupo 99 y ninguna linea del cartel (ni sin cartel): la pantalla (ecx/edi) ya cae fuera de 0x10000..0x08000000 y 0x80000000..0x88000000. El kit mapea la RAM de 64 MB de nuevo cada 64 MB hasta la ventana contigua (xbox_memory_layout.c, registro: RAM mirror 28/28, 1856 MB) y en 0x80000000. Ahora premios.c ygo_ram(a) devuelve la direccion en la copia base (a & 0x03FFFFFF; 0 si a < 0x10000 o >= 0x88000000) y se lee siempre por ahi (cartel y cartas.c). [COFRE] dios elegido ahora escribe pantalla y objeto crudos. Carta nueva de esa partida: k 203 y k 30 nuevas en el cofre, marca Nueva sobre 1 carta. Exe copiado al jugador (cmp igual), zip del programa rearmado. Sin probar.
  - 16:4x Tercera partida (exe 16:37): [COFRE] dios elegido: pantalla 74B3EB70 objeto 00000000. sub_00101A00 hace edi = ecx (la pantalla), asi que la pantalla del torneo vive en 0x74B3EB70, fuera de la RAM de 64 MB y de sus espejos, y el juego la lee ahi. Los dos arreglos anteriores eran rangos fijos equivocados (el de mascara 64 MB leia otra memoria y daba 0; el cartel se callaba porque premio 0 = s_dios_hecho 0). Ahora premios.c ygo_ram_n(a, n) pregunta a Windows (VirtualQuery sobre XBOX_PTR, MEM_COMMIT y legible) por el primer y ultimo byte y devuelve a sin tocarla; cartel y cartas.c lo usan. Exe copiado al jugador (cmp igual), zip del programa rearmado. Sin probar.
  - 16:5x Cartel de los dioses FUNCIONA en el torneo (registro: [PREMIOS] dioses: 5 cartas cada uno, nuevas Obelisco 3 Slifer 1 Ra 2; elegido grupo 0 Obelisco, pantalla 74B3EB70 objeto 74B3FB48; entraron 3 nuevas 4030/4111/4215, como decia el cartel). Origen del premio (listas leidas del xbe): las 5 cartas (4030 4111 4215 4427 4562) estan en la lista comun del torneo nivel 0 (0x453F78, 78+78); ninguna en las listas de rival. El sorteo usa solo el ultimo rival del Triple ([setup+4] = personaje 3 -> duelista 5 por s8 0x423D60) + la lista comun; las listas de los otros dos rivales (26 -> 24, 6 -> 4) no entran en ese premio. El cartel del Triple cuenta los 3 rivales: puede hacer creer que los 3 dan premio.
  - Sorteo del torneo (sub_0008C9B0 releido): cada carta junta en UNA bolsa las que pasan de la lista del rival (0x453FC8+16d) y de la comun del nivel (0x453F78+16t), cada entrada con chance max(5, peso x factor[copias]) % y el bit de la ranura; despues elige una al azar de la bolsa. Un mismo premio puede mezclar cartas del rival y de la comun; con ~10 entradas del rival contra 78 comunes, casi siempre sale de la comun. Maqueta del cartel del Triple: A (3 rivales, el ultimo marcado 'da premio') o B (solo el ultimo); el usuario pregunto si se ve igual que los otros carteles (si: misma caja).
  - Parte del rival en cada carta del premio del torneo (estimado del xbe, nivel 0, 0 copias, sin los cambios del mod): personaje 3/duelista 5: 8-13 % (variante 0), 1-4 % (variante 1); personaje 6: 8-11 %; personaje 26: 13-25 %. El usuario: si el cartel dice 'da premio' y sale todo de la lista comun, van a pensar que esta mal. Opciones propuestas: cartel con la chance real calculada en vivo, o subir la parte del rival en el mod.
- 2026-11-25 17:1x CARTEL DEL TRIPLE COMPACTO (maqueta aprobada; el usuario: 'asegurate de que baje cuando ganes las cartas'): premios.c ygo_premios_triple_tick ahora muestra de donde sale el premio. Leido en el codigo: sub_00025BC0 pasa el ULTIMO personaje del Triple ([setup+4]) a sub_0008C9B0; sub_001056D0 pone el nivel del torneo [setup+0x14] = +0x3E8 + 1; C9B0 usa la lista comun t = clamp(nivel/3, 0..4) (0x453F78+16t) y la del rival (0x453FC8+16d), las dos en la variante r = sub_0008C530(d) (premios.c variante_n con n = nivel+1). Cartel: titulo Premio de este torneo; fila Comunes nivel N: Te faltan X de Y ~A de 10; fila del ultimo rival en dorado: Te faltan X de Y ~B de 10 (o 'menos de 1 de 10'); fila 'A y B no dan premio'. Chance = parte del rival por carta: suma de max(5, (int)(peso x factor[min(copias,3)])) % de cada entrada con el bit de la ranura, rival/(rival+comun), promedio de las 3 ranuras; con el filtro de cuarta copia se sacan las cartas al limite (ygo_limite_carta). Los conteos se leen del cofre en cada cuadro: bajan al ganar. 6 idiomas. Registro: [PREMIOS] triple nivel ...: da premio el ultimo ... parte del rival N por ciento. Comprobado contra el xbe: nivel 1, Yugi (personaje 3, duelista 5): comunes 78, Yugi 21 (igual que el cartel viejo), rival 10 % = ~1 de 10. Exe copiado al jugador (cmp igual), zip del programa rearmado. Sin probar en el juego.
