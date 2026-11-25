# Boot test without the user: runs ygo_recomp.exe with run_debug.bat's
# settings for -Segundos seconds, then closes it. Logs: ygo-apex\runs\.
#   powershell -File tools\prueba.ps1 [-Segundos 25] [-Extra "RECOMP_WATCH=0x00670D1C"]
# -Extra: more environment, "K=V" pairs separated by ';'.
param([int]$Segundos = 25, [string]$Extra = "")
$y = Join-Path $PSScriptRoot "..\ygo-apex" | Resolve-Path
Get-Process ygo_recomp -ErrorAction SilentlyContinue | Stop-Process -Force
New-Item -ItemType Directory -Force "$y\runs" | Out-Null
$sets = ($Extra -split ';' | Where-Object { $_ } | ForEach-Object { "set $_ & " }) -join ''
# cd /d and absolute quoted paths: cmd did not start in -WorkingDirectory
# (the path has spaces), so env.bat was never found and its settings never
# applied.
# RECOMP_PAUSE_UNFOCUSED=0: the test window is never in front, and the
# runtime would pause the game (clocks, vblanks) the whole run.
$cmd = "cd /d `"$y`" && call `"$y\env.bat`" && set RECOMP_DEBUG=watchdog=15 && set RECOMP_TRACE=stub && set RECOMP_PAUSE_UNFOCUSED=0 && " +
       ($sets -replace ' & ', ' && ') +
       "`"$y\build\Release\ygo_recomp.exe`" > `"$y\runs\last_out.txt`" 2> `"$y\runs\last_err.txt`""
$p = Start-Process cmd.exe -ArgumentList "/s /c `"$cmd`"" -WorkingDirectory "$y" -WindowStyle Minimized -PassThru
$p | Wait-Process -Timeout $Segundos -ErrorAction SilentlyContinue
$vivo = [bool](Get-Process ygo_recomp -ErrorAction SilentlyContinue)
Get-Process ygo_recomp -ErrorAction SilentlyContinue | Stop-Process -Force
"seguia corriendo al cortar: $vivo"
"registro: {0:N0} bytes" -f (Get-Item "$y\runs\last_err.txt").Length
# Unresolved indirect-call targets of this run -> analysis\semillas_runtime.json
py -3 (Join-Path $PSScriptRoot "semillas_runtime.py")
