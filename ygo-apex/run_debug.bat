@echo off
rem Diagnostic launch: logs in runs\last_out.txt / last_err.txt.
rem RECOMP_WATCHDOG_SECS: when the game makes no progress for that many
rem seconds, the guest registers, the call trace and the stack go to the log
rem (names the loop the game is stuck in). RECOMP_STUB_LOG: each unresolved
rem stub the first time it runs.
cd /d "%~dp0"
rem A previous run left hanging (no window) keeps the logs open: close it first.
taskkill /im ygo_recomp.exe /f >nul 2>&1
if not exist runs mkdir runs
call "%~dp0env.bat"
set RECOMP_WATCHDOG_SECS=15
set RECOMP_STUB_LOG=1
echo Ejecutando con diagnostico. Espera unos 30 segundos y cierra esta ventana (o el juego).
build\Release\ygo_recomp.exe > runs\last_out.txt 2> runs\last_err.txt
