# Whole pipeline again: disassembly (with the seed files in analysis\),
# func_id, ABI, C generation, rename_pads, build. ~20 min.
#   powershell -File tools\regenerar.ps1
$ErrorActionPreference = "Continue"   # python writes progress to stderr; exit codes are checked
$p = Resolve-Path (Join-Path $PSScriptRoot "..")
$k = Resolve-Path (Join-Path $p "..\xboxrecomp-apex")
$a = "$p\analysis"; $x = "$p\xbe\default.xbe"; $y = "$p\ygo-apex"
$seeds = Get-ChildItem "$a\semillas_*.json" | ForEach-Object { "--seed-functions"; $_.FullName }
Push-Location $k
try {
    "[1/6] disasm ($($seeds.Count / 2) seed files)"
    py -3 -m tools.disasm $x -o "$a\disasm" --analysis-json "$p\xbe\default_analysis.json" --force @seeds *> "$a\2_disasm.log"
    if ($LASTEXITCODE) { throw "disasm failed" }
    "[2/6] func_id"
    py -3 -m tools.func_id $x --functions "$a\disasm\functions.json" --strings "$a\disasm\strings.json" --xrefs "$a\disasm\xrefs.json" -o "$a\func_id" *> "$a\3_funcid.log"
    if ($LASTEXITCODE) { throw "func_id failed" }
    "[3/6] abi"
    py -3 -m tools.abi_analysis $x --disasm-dir "$a\disasm" --func-id-dir "$a\func_id" --output-dir "$a\abi" *> "$a\4_abi.log"
    if ($LASTEXITCODE) { throw "abi failed" }
    "[4/6] recomp"
    Remove-Item "$y\src\recomp\gen\*" -Force
    py -3 -m tools.recomp $x --all --split 250 --game-name "Yu-Gi-Oh! The Dawn of Destiny" --disasm-dir "$a\disasm" --func-id-dir "$a\func_id" --abi-dir "$a\abi" -o "$a\recomp" --gen-dir "$y\src\recomp\gen" *> "$a\5_recomp.log"
    if ($LASTEXITCODE) { throw "recomp failed" }
} finally { Pop-Location }
"[5/6] rename_pads"
py -3 "$p\tools\rename_pads.py"
if ($LASTEXITCODE) { throw "rename_pads failed" }
"[6/6] build"
$cm = Get-ChildItem "C:\Program Files\Microsoft Visual Studio\18" -Recurse -Filter cmake.exe -ErrorAction SilentlyContinue | Select-Object -First 1 -ExpandProperty FullName
& $cm --build "$y\build" --config Release --parallel 8 *> "$y\build_log.txt"
if ($LASTEXITCODE) { Select-String -Path "$y\build_log.txt" -Pattern ": error" | Select-Object -First 10 | ForEach-Object Line; throw "build failed" }
"done: " + (Get-Item "$y\build\Release\ygo_recomp.exe").LastWriteTime
