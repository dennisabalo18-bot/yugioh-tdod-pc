@echo off
chcp 65001 >nul
setlocal
cd /d "%~dp0"
set "ISO=%~1"
if not "%ISO%"=="" goto tengo
for %%f in (*.iso Game\*.iso) do set "ISO=%%f"
if not "%ISO%"=="" goto tengo
echo No encontré ninguna ISO.
echo Arrastrá la ISO de Yu-Gi-Oh! The Dawn of Destiny (Xbox) sobre este archivo,
echo o copiala a esta carpeta y volvé a abrirlo.
pause
exit /b 1

:tengo
dir /s /b "Game\default.xbe" >nul 2>&1
if errorlevel 1 goto extraer
echo Ya hay un juego en la carpeta Game. Si querés reinstalarlo, vaciala primero.
pause
exit /b 0

:extraer
echo Extrayendo "%ISO%" en Game...
"Herramientas\extract-xiso\extract-xiso.exe" -d "Game" -x "%ISO%"
dir /s /b "Game\default.xbe" >nul 2>&1
if errorlevel 1 goto mal
echo.
echo Listo. Ya podés abrir Configuración.exe y jugar.
echo La ISO ya no hace falta: si querés, borrala.
pause
exit /b 0

:mal
echo.
echo No se pudo extraer el juego. ¿Es la ISO de Yu-Gi-Oh! The Dawn of Destiny de Xbox?
pause
exit /b 1