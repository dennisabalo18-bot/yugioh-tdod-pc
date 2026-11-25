# Unattended run: plays an input script (kbm_script) and saves one picture a
# second, so the game can be driven and looked at without a person.
#   powershell -File tools\auto.ps1 -Script runs\x.txt -Segundos 60 [-Extra "K=V;K=V"]
# Pictures: ygo-apex\runs\auto\frame_NNNN.bmp (NNNN = seconds of frames / 60).
param([string]$Script, [int]$Segundos = 60, [string]$Extra = "")
$y = Join-Path $PSScriptRoot "..\ygo-apex" | Resolve-Path
Get-Process ygo_recomp -ErrorAction SilentlyContinue | Stop-Process -Force
$d = "$y\runs\auto"
if (Test-Path $d) { Get-ChildItem $d -Filter *.bmp | Remove-Item }
New-Item -ItemType Directory -Force $d | Out-Null
$s = (Resolve-Path (Join-Path $y $Script)).Path
$sets = ($Extra -split ';' | Where-Object { $_ } | ForEach-Object { "set `"$_`" && " }) -join ''
$cmd = "cd /d `"$y`" && call `"$y\env.bat`" && set RECOMP_PAUSE_UNFOCUSED=0 && " +
       "set `"RECOMP_DEBUG=kbm_script=$s`" && set `"RECOMP_D3D11_DUMP=$d\`" && " + $sets +
       "`"$y\build\Release\ygo_recomp.exe`" > `"$y\runs\last_out.txt`" 2> `"$y\runs\last_err.txt`""
$p = Start-Process cmd.exe -ArgumentList "/s /c `"$cmd`"" -WorkingDirectory "$y" -WindowStyle Minimized -PassThru
$p | Wait-Process -Timeout $Segundos -ErrorAction SilentlyContinue
Get-Process ygo_recomp -ErrorAction SilentlyContinue | Stop-Process -Force
"pictures: " + (Get-ChildItem $d -Filter *.bmp).Count
