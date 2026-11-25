@echo off
rem Reescribe nitida la descripcion de las cartas grandes ya escaladas (el
rem texto sale del juego). Va despues de escalar_texturas.bat y antes de
rem crear_pack_texturas.bat.
cd /d "%~dp0"
py -3 -I "%~dp0..\tools\texto_cartas.py"
pause
