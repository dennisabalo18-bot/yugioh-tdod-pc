@echo off
rem Runtime switches for the apexJCL-fork build (GPU rendering through D3D11).
rem The fork's main.c already turns on vblank, title KEVENTs and USB.

rem Emulated MCPX audio chip: the title's own DirectSound drives it.
set RECOMP_AC97_READY=1
rem Execute the pushbuffer, and draw it with D3D11 on the host GPU.
set RECOMP_PB_EXEC=1
set RECOMP_PB_BACKEND=d3d11
rem 60 Hz vblank interrupts: the engine's frame limiter waits on them.
set RECOMP_VBLANK=paced
rem Keyboard and mouse as pad 1 (an XInput pad works without them).
rem Mouse: moving it moves the game's cursor a square at a time, left button
rem confirms (A), right cancels (B), middle is Y, wheel goes up/down lists.
rem F9 frees the cursor.
rem YGO_MOUSE_STEP = mouse counts per square (180 low, 120 medium, 70 high).
set RECOMP_KEYBOARD=1
set RECOMP_MOUSE=1
rem YGO_MOUSE_MODE: juego (the game's glove is the only pointer), puntero
rem (experimental: Windows pointer over the duel squares), clics.
set YGO_MOUSE_MODE=puntero
set YGO_MOUSE_STEP=120
rem Keep contiguous (DMA) memory above the 8 MB image, or the USB
rem descriptors resolve into the game's code and the pad never enumerates.
set RECOMP_CONTIG_AVOID_IMAGE=1
rem Yu-Gi-Oh: the engine heap reserves 64 KB ranges above RAM (VirtualQuery +
rem VirtualAlloc at a fixed address); needs the extended VMA tracker.
set RECOMP_EXT_VMA=1
set RECOMP_HEAP_RECLAIM=1
rem Graphics (GPU backend): internal resolution factor 1..4 (2 = 1280x960),
rem anisotropic filtering 0..16 (the scaling filter is fixed: sharp),
rem aspect 4:3 (stock) or 16:9 / 16:10 / 21:9 (hor+, experimental), and
rem borderless fullscreen at start (F11 / Alt+Enter toggles it).
set RDR_RENDER_SCALE=2
set RDR_ANISO=16
set RDR_ASPECT=4:3
rem Native 16:9: the game is told the TV is widescreen and boots in its own
rem 720x480 16:9 mode (3D and HUD widened by the game). 1 = on. F2 menu too.
set RECOMP_WIDESCREEN=0
rem Idioma del juego (al arrancar): es, en, fr, de, it. Tambien en F2.
set RECOMP_LANGUAGE=es
rem Texturas: "Volcar texturas a carpeta" en F2 (o 1 aqui) guarda cada textura
rem en ..\modding\texturas\dump\ como PNG. Las mejoradas (mismo nombre) van en
rem ..\modding\texturas\mejoradas\; crear_pack_texturas.bat hace textures.pack.
set RECOMP_TEXTURES_DUMP=0
set RECOMP_TEXTURES_DUMP_DIR=%~dp0..\modding\texturas\dump
rem El pack: modding\escalar_texturas.bat, texto_cartas.bat y
rem crear_pack_texturas.bat lo arman (textures.pack sin comprimir, y de ahi
rem textures_bc3.pack, el que se usa); F6 cambia original <-> mejorada en vivo.
set RECOMP_TEXTURES_PACK=%~dp0..\modding\texturas\textures_bc3.pack
rem Mods: un archivo en ..\modding\mods\ reemplaza al del disco con el mismo
rem nombre (meter_audio.bat deja ahi los stream*.dat con voces cambiadas).
set RECOMP_MODS_DIR=%~dp0..\modding\mods
rem Voces y textos en espanol latino de los duelistas (modding\voces_latino,
rem lo arma meter_voces.bat). Vaciar el valor para volver a los originales.
set YGO_VOCES=%~dp0..\modding\voces_latino
rem Partidas guardadas (UDATA/TDATA) dentro de la carpeta del port:
rem Yu-Gi-Oh Port\Partidas (no en %LOCALAPPDATA%\xboxrecomp).
for %%I in ("%~dp0..") do set "RECOMP_SAVE_DIR=%%~fI\Partidas"
rem Ajustes (F2 y Configuración.exe: ygo_menu.ini y launcher.ini) en
rem Yu-Gi-Oh Port\Ajustes: una version nueva del port no los pisa.
for %%I in ("%~dp0..") do set "YGO_AJUSTES=%%~fI\Ajustes"
set RDR_FULLSCREEN=0
rem Full-screen glow off: it blurs the whole picture (text and background)
rem and can leave a seam through the middle of the screen. Empty the value
rem to bring it back. (The dotted line round the edges was not the glow: the
rem letterbox half-pixel at render scale, fixed in set_vp.)
set RECOMP_D3D11_SKIP_RT=1
rem F8 in the game window: game speed x2, F8 again back to normal.
rem F2 in the game window: the settings menu (resolution, aspect, window,
rem mouse). Its choices are saved to rdr_menu.ini and win over this file;
rem delete rdr_menu.ini to go back to these values.
rem FPS and draw count in the window title.
set RECOMP_TRACE=title,tex_pack,key
