@echo off
rem Launch the recompiled Yu-Gi-Oh! The Dawn of Destiny with GPU (D3D11) rendering.
rem The log of the last run is kept in runs\last_err.txt (run_debug.bat adds
rem more tracing).
cd /d "%~dp0"
rem A previous run left hanging (no window) keeps the logs open: close it first.
taskkill /im ygo_recomp.exe /f >nul 2>&1
if not exist runs mkdir runs
call "%~dp0env.bat"
rem Smoke / reticle diagnosis: the fixed-function sprites' matrices, every 2 s.
rem set RECOMP_FF_TRACE=1
build\Release\ygo_recomp.exe > runs\last_out.txt 2> runs\last_err.txt
