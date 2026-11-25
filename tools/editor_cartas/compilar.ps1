# Compila el editor del mod de cartas, en la carpeta del port, con el
# compilador de C# que trae Windows (.NET Framework 4):
#   Editor de cartas.exe
#     powershell -ExecutionPolicy Bypass -File tools\editor_cartas\compilar.ps1
Set-Location $PSScriptRoot
$csc = "$env:WINDIR\Microsoft.NET\Framework64\v4.0.30319\csc.exe"
$icon = Join-Path $PSScriptRoot '..\launcher\yugioh.ico'
$base = @('/nologo', '/target:winexe', '/codepage:65001', '/optimize+')
if (Test-Path $icon) { $base += "/win32icon:$icon" }
$out = Join-Path $PSScriptRoot '..\..\Editor de cartas.exe'
& $csc @base "/out:$out" '/r:System.Windows.Forms.dll' '/r:System.Drawing.dll' '/r:System.Core.dll' '/r:System.Web.Extensions.dll' 'EditorCartas.cs'
exit $LASTEXITCODE
