#!/bin/sh
# Copia el build y el mod de cartas a la carpeta del jugador y lo comprueba.
# El juego corre Yu-Gi-Oh.exe: el build (ygo_recomp.exe) se copia con ESE nombre.
#   sh tools/copiar_al_juego.sh
# Sale con error (y no copia nada) si el juego esta abierto o si algo no coincide.
set -e
R="/c/Portear Juegos Xbox Clasica/Yu-Gi-Oh Port"
G="/c/Emuladores y Juegos/Yu-Gi-Oh Port"
EXE="$R/ygo-apex/build/Release/ygo_recomp.exe"
MOD="$R/modding/cartas/salida/cartas"
C="$G/Mods-Extras/Contenido/cartas"

if tasklist | grep -qi "yu-gi-oh.exe"; then
    echo "JUEGO ABIERTO: no se copio nada. Cerralo y volve a correr esto."
    exit 1
fi
# el mod se rearma siempre (los .igb del visor de modelos y cartas.txt): si no,
# se copiaba la salida vieja (2026-11-13: los modelos armados en el visor no llegaban)
(cd "$R" && py -3 tools/cartas_mod.py > /dev/null) || { echo "cartas_mod.py fallo: no se copio nada"; exit 1; }
cp "$EXE" "$G/Yu-Gi-Oh.exe"
cmp "$EXE" "$G/Yu-Gi-Oh.exe"
rm -f "$G/ygo_recomp.exe"
if [ -d "$MOD" ]; then
    rm -rf "$C/archivos"
    cp -r "$MOD/." "$C/"
    diff -rq "$MOD" "$C"
fi
# las fundas (tools/fundas.py: ilustraciones y marcos de 512x740; fundas.c las arma)
FU="$R/modding/texturas/fundas/juego"
if [ -d "$FU" ]; then
    mkdir -p "$G/Mods-Extras/Contenido/fundas"
    cp -r "$FU/." "$G/Mods-Extras/Contenido/fundas/"
    diff -rq "$FU" "$G/Mods-Extras/Contenido/fundas"
fi
# el Perfil del menu principal (tools/perfil_recursos.py y tools/perfil.py; perfil.c)
PE="$R/modding/texturas/perfil/juego"
if [ -d "$PE" ]; then
    mkdir -p "$G/Mods-Extras/Contenido/perfil"
    cp -r "$PE/." "$G/Mods-Extras/Contenido/perfil/"
    diff -rq -x cartas "$PE" "$G/Mods-Extras/Contenido/perfil"   # cartas: perfil_cartas.py, below
fi
# el buscador de la Biblioteca (tools/buscador_recursos.py; buscador.c)
BU="$R/modding/texturas/buscador/juego"
# las miniaturas del buscador con las cartas del mod (la hoja del mod gana)
(cd "$R" && py -3 tools/buscador_recursos.py "$G" "$BU" "$MOD/archivos/card/mini/texdb" > /dev/null)     || { echo "buscador_recursos.py fallo"; exit 1; }
if [ -d "$BU" ]; then
    mkdir -p "$G/Mods-Extras/Contenido/buscador"
    cp -r "$BU/." "$G/Mods-Extras/Contenido/buscador/"
    diff -rq "$BU" "$G/Mods-Extras/Contenido/buscador"
fi
# las cartas del panel "Mazo de ..." del Perfil: la version mejorada de cada
# carta e idioma (tools/perfil_cartas.py; perfil.c carta_img)
PC="$R/modding/texturas/perfil/cartas"
(cd "$R" && py -3 tools/perfil_cartas.py "$G/Game" "$MOD/archivos" "$PC" > /dev/null) || { echo "perfil_cartas.py fallo"; exit 1; }
mkdir -p "$G/Mods-Extras/Contenido/perfil/cartas"
cp -ru "$PC/." "$G/Mods-Extras/Contenido/perfil/cartas/"
# 2026-11-19: las que perfil_cartas.py ya no genera (imagenes que no estan en
# la lista de cartas del juego) se borran tambien en el juego: si no, el diff frena
(cd "$G/Mods-Extras/Contenido/perfil/cartas" && find . -type f -name "*.jpg") | while read -r f; do
    [ -f "$PC/$f" ] || rm -f "$G/Mods-Extras/Contenido/perfil/cartas/$f"
done
diff -rq "$PC" "$G/Mods-Extras/Contenido/perfil/cartas"
# los tapetes del duelo (tools/tapetes.py; fundas.c, perfil.c)
TA="$R/modding/texturas/tapetes/juego"
if [ -d "$TA" ]; then
    mkdir -p "$G/Mods-Extras/Contenido/tapetes"
    cp -r "$TA/." "$G/Mods-Extras/Contenido/tapetes/"
    diff -rq "$TA" "$G/Mods-Extras/Contenido/tapetes"
fi
# la placa AUTO del duelo (tools/auto_recursos.py; auto.c)
AU="$R/modding/texturas/auto/juego"
if [ -d "$AU" ]; then
    mkdir -p "$G/Mods-Extras/Contenido/auto"
    cp -r "$AU/." "$G/Mods-Extras/Contenido/auto/"
    diff -rq "$AU" "$G/Mods-Extras/Contenido/auto"
fi
# el reverso del rival (fundas.c: otra huella para la funda del rival)
cp "$R/modding/texturas/fundas/card_ura_rival.tga" "$G/Game/card/normal/card_ura_rival.tga"
# 2026-11-25: tambien en Contenido/juego (el paquete; el juego los copia solo a la carpeta del juego)
mkdir -p "$G/Mods-Extras/Contenido/juego/card/normal" "$G/Mods-Extras/Contenido/juego/effect/special_igb"
cp "$R/modding/texturas/fundas/card_ura_rival.tga" "$G/Mods-Extras/Contenido/juego/card/normal/card_ura_rival.tga"
cmp "$R/modding/texturas/fundas/card_ura_rival.tga" "$G/Game/card/normal/card_ura_rival.tga"
# Exodia entero quieto para la mascota (tools/modelos/exodia_mascota.py)
for EXO in "$R"/modding/modelos/exodia_mascota/*.igb; do
    [ -f "$EXO" ] || continue
    cp "$EXO" "$G/Game/effect/special_igb/$(basename "$EXO")"
    cp "$EXO" "$G/Mods-Extras/Contenido/juego/effect/special_igb/$(basename "$EXO")"
    cmp "$EXO" "$G/Game/effect/special_igb/$(basename "$EXO")"
done
echo "LISTO: Yu-Gi-Oh.exe identico al build ($(date -r "$EXE" '+%H:%M:%S')) y mod de cartas igual."
