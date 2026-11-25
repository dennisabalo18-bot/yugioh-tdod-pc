@echo off
rem Herramienta para el raton estilo Joey the Passion.
rem En un duelo, recorre con la cruceta o las flechas todas las casillas: tu
rem mano, tus monstruos, tus magias/trampas, las del rival, deck, cementerio...
rem Cada casilla en la que el cursor se queda quieto medio segundo se guarda como
rem foto en fotos_cursor\. Tambien: F10 = foto de memoria (Shift+F10 de nuevo).
cd /d "%~dp0"
taskkill /im ygo_recomp.exe /f >nul 2>&1
call env.bat
set YGO_SCAN=1
build\Release\ygo_recomp.exe > runs\last_out.txt 2> runs\last_err.txt
