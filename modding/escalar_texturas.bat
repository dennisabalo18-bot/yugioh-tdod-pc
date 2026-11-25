@echo off
rem Escala con Upscayl (x4, modelo Digital Art) las texturas de texturas\dump\
rem que todavia no estan en texturas\mejoradas\. Si se corta, ejecutalo de nuevo
rem y sigue donde quedo. Otro modelo: escalar_texturas.bat ultrasharp-4x
cd /d "%~dp0"
py -3 -I "%~dp0..\tools\escalar_texturas.py" %1
pause
