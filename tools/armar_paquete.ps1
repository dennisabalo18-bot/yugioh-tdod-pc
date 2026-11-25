# Arma el port de Yu-Gi-Oh! The Dawn of Destiny para jugar fuera de la carpeta de desarrollo.
#
#   tools\armar_paquete.ps1
#       dist\Yu-Gi-Oh Port\             el programa solo (lo que va en el zip)
#       dist\YuGiOh-TDOD-PC.zip         el programa: va a GitHub (Releases; Actualizar.cs lo baja)
#       dist\drive\                     lo que va a Google Drive (2026-11-25, tools\zip_drive.py),
#                                        cada zip se descomprime tal cual en la carpeta del port:
#           YuGiOh-Contenido.zip         OBLIGATORIO: cartas nuevas, Perfil, fundas, tapetes, buscador,
#                                        AUTO y Contenido\juego (los archivos del port que el juego
#                                        copia solo a la carpeta del juego al arrancar)
#           YuGiOh-Texturas-HD.zip       texturas mejoradas (opcional)
#           YuGiOh-Idioma-XX.zip         las cartas y textos en ese idioma (opcional, uno por idioma)
#                                        (el ES trae tambien las voces, Mods-Extras\voces)
#       Los de Drive salen de -Extras (por defecto el Mods-Extras de la carpeta del jugador).
#   tools\armar_paquete.ps1 -Completo "C:\Emuladores y Juegos\Yu-Gi-Oh Port"
#       además una carpeta lista para jugar: el programa + Game\ (la copia del juego de esta PC)
#       + Mods-Extras\ (texturas y voces) + Ajustes\ y Partidas\ de esta carpeta.
#
# Nunca va en el zip: el juego, ygo_menu.ini (ajustes del que lo arma), registros ni volcados.
# El zip trae ygo_menu.default.ini: el exe lo copia la primera vez y después vale Ajustes\.
# -SinDrive: solo el zip del programa (los de Drive ya subidos no cambian).
param([string]$Completo = "", [string]$Extras = "C:\Emuladores y Juegos\Yu-Gi-Oh Port\Mods-Extras", [switch]$SinDrive)

$ErrorActionPreference = "Stop"
$port = Split-Path -Parent $PSScriptRoot
$dist = Join-Path $port "dist"
$prog = Join-Path $dist "Yu-Gi-Oh Port"
$paq = Join-Path $PSScriptRoot "paquete"
$xiso = "C:\Emuladores y Juegos\Red Dead Revolver PC\Herramientas\extract-xiso"
$juego = Join-Path (Split-Path -Parent $port) "Yu-Gi-Oh TDOD"
$pack = Join-Path $port "modding\texturas\textures_bc3.pack"
$voces = Join-Path $port "modding\voces_latino"

# Compress-Archive de PowerShell 5.1 guarda la "ó" de Configuración.exe rota y las carpetas con "\"
# (algunos descompresores no lo entienden): el zip lo hace Python, con nombres UTF-8 y "/".
function Zip($carpeta, $zip) {
    if (Test-Path $zip) { Remove-Item -Force $zip }
    $py = @'
import os, sys, zipfile
src, dst = sys.argv[1], sys.argv[2]
base = os.path.dirname(src)
with zipfile.ZipFile(dst, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
    for d, dirs, files in os.walk(src):
        rel = os.path.relpath(d, base).replace(os.sep, "/")
        if not files and not dirs:
            z.writestr(rel + "/", "")
        for f in files:
            z.write(os.path.join(d, f), rel + "/" + f)
'@
    $tmp = Join-Path $env:TEMP "ygo_zip.py"
    Set-Content -LiteralPath $tmp -Value $py -Encoding UTF8
    & py -3 -I $tmp $carpeta $zip
    if ($LASTEXITCODE) { throw "no se pudo armar $zip" }
}

function Copiar($de, $a) {
    New-Item -ItemType Directory -Force (Split-Path -Parent $a) | Out-Null
    Copy-Item -LiteralPath $de -Destination $a -Force
}

# ---- el programa ----
if (Test-Path $prog) { Remove-Item -Recurse -Force $prog }
New-Item -ItemType Directory -Force $prog | Out-Null
Copiar (Join-Path $port "Yu-Gi-Oh.exe") (Join-Path $prog "Yu-Gi-Oh.exe")
Copiar (Join-Path $port "SDL3.dll") (Join-Path $prog "SDL3.dll")
Copiar (Join-Path $port "shaders.pak") (Join-Path $prog "shaders.pak")
Copiar (Join-Path $port "Configuración.exe") (Join-Path $prog "Configuración.exe")
Copiar (Join-Path $paq "LEEME.txt") (Join-Path $prog "LEEME.txt")
# la versión que muestra Configuración y compara con GitHub (Actualizar.cs)
Set-Content -LiteralPath (Join-Path $prog "version.txt") -Value (Get-Content -LiteralPath (Join-Path $port "VERSION") -Raw).Trim() -Encoding ASCII
Copiar (Join-Path $paq "ygo_menu.default.ini") (Join-Path $prog "ygo_menu.default.ini")
Copiar (Join-Path $paq "Instalar juego desde ISO.bat") (Join-Path $prog "Instalar juego desde ISO.bat")
Copiar (Join-Path $paq "PEGAR ACA EL JUEGO.txt") (Join-Path $prog "Game\PEGAR ACA EL JUEGO.txt")
Copiar (Join-Path $paq "LEEME Mods-Extras.txt") (Join-Path $prog "Mods-Extras\LEEME.txt")
New-Item -ItemType Directory -Force (Join-Path $prog "Mods-Extras\texturas"), (Join-Path $prog "Mods-Extras\voces") | Out-Null
# el guante del puntero: el atlas común sin el guante del juego (main.c lo busca junto al exe)
Copy-Item -Recurse -Force (Join-Path $port "ygo-apex\mods_puntero") (Join-Path $prog "mods_puntero")
Copy-Item -Recurse -Force $xiso (Join-Path $prog "Herramientas\extract-xiso")

# ---- los zip ----
$zip = Join-Path $dist "YuGiOh-TDOD-PC.zip"
Zip $prog $zip
"{0}: {1:N1} MB" -f $zip, ((Get-Item $zip).Length / 1MB)
# lo de Google Drive (2026-11-25): Contenido (obligatorio), texturas HD, un idioma por zip, voces
$drive = Join-Path $dist "drive"
if (-not $SinDrive) {
    & py -3 -I (Join-Path $PSScriptRoot "zip_drive.py") $Extras $drive
    if ($LASTEXITCODE) { throw "no se pudieron armar los zip de Drive" }
}

# ---- la carpeta completa para jugar ----
if ($Completo) {
    New-Item -ItemType Directory -Force $Completo | Out-Null
    Copy-Item -Recurse -Force (Join-Path $prog "*") $Completo
    Remove-Item -Force (Join-Path $Completo "Game\PEGAR ACA EL JUEGO.txt") -ErrorAction SilentlyContinue
    robocopy $juego (Join-Path $Completo "Game") /E /NFL /NDL /NJH /NP | Out-Null
    # con su lista de precarga (<pack>.usadas, kit nv2a_tex_pack.c): el primer arranque ya lee en
    # segundo plano las texturas de siempre, sin ir de a una (lo que en disco mecánico se nota)
    robocopy (Split-Path $pack) (Join-Path $Completo "Mods-Extras\texturas") (Split-Path -Leaf $pack) ((Split-Path -Leaf $pack) + ".usadas") /NFL /NDL /NJH /NP | Out-Null
    robocopy $voces (Join-Path $Completo "Mods-Extras\voces") /E /NFL /NDL /NJH /NP | Out-Null
    robocopy (Join-Path $port "Ajustes") (Join-Path $Completo "Ajustes") /E /NFL /NDL /NJH /NP | Out-Null
    robocopy (Join-Path $port "Partidas") (Join-Path $Completo "Partidas") /E /NFL /NDL /NJH /NP | Out-Null
    "Carpeta completa: $Completo"
}
