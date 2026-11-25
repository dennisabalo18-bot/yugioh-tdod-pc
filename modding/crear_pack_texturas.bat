@echo off
rem Arma texturas\textures.pack con texturas\mejoradas\ (las de
rem mejoradas\texto_cartas\ ganan) y lo comprime a texturas\textures_bc3.pack,
rem que es el que carga el juego. F6 cambia entre las originales y las mejoradas.
cd /d "%~dp0"
py -3 -I "%~dp0..\tools\texturas_pack.py" || goto fin
"%~dp0..\tools\bc3\bc3pack.exe" "%~dp0texturas\textures.pack" "%~dp0texturas\textures_bc3.pack" || goto fin
rem textures.pack (~20 GB sin comprimir) es solo el paso intermedio: se rearma desde mejoradas\
del "%~dp0texturas\textures.pack"
:fin
pause
