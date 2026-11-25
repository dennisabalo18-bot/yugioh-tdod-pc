"""La letra de los títulos del juego para el Perfil (2026-11-16, el usuario:
"usa la fuente del juego para los numeros y las cosas de este menu").

La página FED7C5881DA64D4A (512x512; el volcado está dado vuelta en vertical)
es la letra con serifa de los carteles del juego ("Duelo (Individual)"):
celdas de 30 x 26 px, 15 por fila, caracteres 0x21..0x7E y 0x80..0xFF
(Windows-1252) en orden (medido: columnas desde 1, 31, 61...; filas desde 1,
27, 53...). Cada letra está a su propia altura en la celda, así que se mide
su tinta y se la pone en la línea: base = el pie de la "A" (fila 18), las que
bajan con su cola, las de arriba (comillas, acentos) arriba, las del medio
centradas en la "x".

Toma la versión escalada 4x (mejoradas/letras), la reduce a 2x y escribe
ygo-apex/src/fuente_titulo.h; la 4x tal cual en fuente_titulo4.h (letras grandes).

    py -3 tools/fuente_titulo.py
"""
import pathlib

import numpy as np
from PIL import Image

PORT = pathlib.Path(__file__).resolve().parents[1]
SRC4 = PORT / "modding/texturas/mejoradas/letras/FED7C5881DA64D4A_512x512.png"
SRC1 = PORT / "modding/texturas/dump/FED7C5881DA64D4A_512x512.png"
OUT = PORT / "ygo-apex/src/fuente_titulo.h"
OUT4 = PORT / "ygo-apex/src/fuente_titulo4.h"
CW, CH, COLS, FILAS = 30, 26, 15, 15
BAJAN = "gjpqy,;çþýÿµ()[]{}|$@ƒ"
ARRIBA = "'\"`^~*°ªº´¨¯¸²³¹‘’“”ˆ˜™"
MEDIO = "-+=·×÷¬–—•<>"


def main():
    a1 = np.array(Image.open(SRC1).convert("RGBA"))[::-1, :, 3].astype(np.float32)
    a4 = np.array(Image.open(SRC4).convert("RGBA"))[::-1, :, 3].astype(np.float32)
    lw, lh = CW * COLS, CH * FILAS                       # 450 x 390 at 1x
    a2 = a4[:lh * 4, :lw * 4].reshape(lh * 2, 2, lw * 2, 2).mean(axis=(1, 3))
    a2 = np.clip(a2 + 0.5, 0, 255).astype(np.uint8)
    codigos = list(range(0x21, 0x7F)) + list(range(0x80, 0x100))
    med = {}
    for i, c in enumerate(codigos):
        x0, y0 = (i % COLS) * CW, (i // COLS) * CH
        cel = a1[y0:y0 + CH, x0:x0 + CW]
        cols = np.where(cel.max(0) > 40)[0]
        rows = np.where(cel.max(1) > 40)[0]
        w = int(cols.max()) + 1 if len(cols) else 0
        tb = (int(rows.min()), int(rows.max())) if len(rows) else (0, 0)
        med[bytes([c]).decode("cp1252", errors="replace")] = (w, tb)
    BL, CT = med['A'][1][1], med['A'][1][0]
    HX = med['x'][1][1] - med['x'][1][0]
    HG = med['g'][1][1] - med['g'][1][0]
    anchos, alto = [], []
    for c in codigos:
        ch = bytes([c]).decode("cp1252", errors="replace")
        w, (top, bot) = med[ch]
        if ch in ARRIBA:
            y = CT - top
        elif ch in MEDIO:
            y = int(round(BL - HX / 2.0 - (top + bot) / 2.0))
        elif ch in BAJAN:
            y = BL + (HG - HX) - bot
        else:
            y = BL - bot
        anchos.append(w)
        alto.append(y)
    print("base", BL, "mayusculas desde", CT, "x", HX, "g", HG, "desplazamientos", min(alto), max(alto))
    datos = a2.tobytes()
    lineas = ["    " + ",".join(str(b) for b in datos[i:i + 32]) + "," for i in range(0, len(datos), 32)]
    OUT.write_text(
        "/* Generado por tools/fuente_titulo.py: la letra de los titulos del juego\n"
        " * (FED7C5881DA64D4A: 0x21..0xFF, celdas de 30 x 26 px, 15 por fila), 2x, solo alfa.\n"
        " * No editar a mano. */\n"
        "#define FT_CW %d\n#define FT_CH %d\n#define FT_COLS %d\n#define FT_W %d\n#define FT_H %d\n"
        "#define FT_BASE_FILA %d\n#define FT_MAYUS %d\n#define FT_ABAJO %d\n#define FT_SUBE %d\n"
        % (CW, CH, COLS, lw * 2, lh * 2, BL, BL - CT + 1, BL + (HG - HX), -min(0, min(alto)))
        + "/* per glyph: its width (1x px) and how far down its cell goes on the line (1x rows) */\n"
        + "static const unsigned char k_ft_ancho[%d] = { %s };\n" % (len(anchos), ", ".join(map(str, anchos)))
        + "static const signed char k_ft_y[%d] = { %s };\n" % (len(alto), ", ".join(map(str, alto)))
        + "static const unsigned char k_ft_px[%d] = {\n%s\n};\n" % (len(datos), "\n".join(lineas)),
        encoding="utf-8")
    print(f"{OUT}: {lw * 2}x{lh * 2}")
    # the 4x page as it is, for big letters (carta_as.c: the ace card's name;
    # fuente_titulo.c takes it when the 2x one would be stretched)
    a4c = np.clip(a4[:lh * 4, :lw * 4] + 0.5, 0, 255).astype(np.uint8)
    d4 = a4c.tobytes()
    l4 = ["    " + ",".join(str(b) for b in d4[i:i + 32]) + "," for i in range(0, len(d4), 32)]
    OUT4.write_text(
        "/* Generado por tools/fuente_titulo.py: la misma pagina a 4x (la version\n"
        " * mejorada tal cual), solo alfa. No editar a mano. */\n"
        "#define FT4_W %d\n#define FT4_H %d\n" % (lw * 4, lh * 4)
        + "static const unsigned char k_ft_px4[%d] = {\n%s\n};\n" % (len(d4), "\n".join(l4)),
        encoding="utf-8")
    print(f"{OUT4}: {lw * 4}x{lh * 4}")


if __name__ == "__main__":
    main()
