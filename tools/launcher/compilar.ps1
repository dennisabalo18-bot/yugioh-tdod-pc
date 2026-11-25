# Compila, en la carpeta del port, con el compilador de C# que trae Windows
# (.NET Framework 4): no hay que instalar nada.
#   Configuración.exe   los ajustes (los mismos de F2)
#   (el juego es Yu-Gi-Oh.exe, que copia ahí la compilación de ygo-apex)
#     powershell -ExecutionPolicy Bypass -File tools\launcher\compilar.ps1
# El ícono (la mano del juego en alta resolución) lo hace tools\icono.py.
Set-Location $PSScriptRoot
$csc = "$env:WINDIR\Microsoft.NET\Framework64\v4.0.30319\csc.exe"
$icon = Join-Path $PSScriptRoot 'yugioh.ico'
$base = @('/nologo', '/target:winexe', '/codepage:65001', '/optimize+')
if (Test-Path $icon) { $base += "/win32icon:$icon" }
$out1 = Join-Path $PSScriptRoot '..\..\Configuración.exe'
& $csc @base "/out:$out1" '/r:System.Windows.Forms.dll' '/r:System.Drawing.dll' '/r:System.Core.dll' '/r:System.IO.Compression.dll' '/r:System.IO.Compression.FileSystem.dll' '/r:System.Web.Extensions.dll' 'Configuracion.cs' 'Actualizar.cs' 'Perfiles.cs' 'Respaldos.cs'
exit $LASTEXITCODE
