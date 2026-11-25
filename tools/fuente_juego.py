"""La letra del juego para nuestros carteles (2026-11-15, el usuario: "estas
letras y fuentes que usaste podes usarlas para todo lo que hicimos").

La pagina de letras del juego (textura 9BF7092A495E4E38, 512x512; el volcado
esta dado vuelta en vertical) es una grilla de celdas de 26 px, 15 por fila,
con los caracteres 0x21..0xFF (Latin-1: acentos, n, signos) en orden; cada
letra arranca en el borde izquierdo de su celda (medido: filas de letras en
1, 27, 53...; "0123456789:;<=>" en la fila 1, "?@ABC..." en la 2).

Toma la version escalada 4x (mejoradas/letras), la reduce a 2x y escribe
ygo-apex/src/fuente_juego.h: el alfa de la grilla (780x780) y, por caracter,
su ancho en px de la pagina 1x.

    py -3 tools/fuente_juego.py
"""
import pathlib

import numpy as np
from PIL import Image

PORT = pathlib.Path(__file__).resolve().parents[1]
SRC4 = PORT / "modding/texturas/mejoradas/letras/9BF7092A495E4E38_512x512.png"
SRC1 = PORT / "modding/texturas/dump/9BF7092A495E4E38_512x512.png"
OUT = PORT / "ygo-apex/src/fuente_juego.h"
CELDA, COLS, FILAS = 26, 15, 15
PRIMERO, ULTIMO = 0x21, 0xFF
# the page skips 0x7F: 0x21..0x7E, then 0x80..0xFF (Windows-1252: euro, dashes,
# quotes, then the accents); every glyph sits at the top of its cell, so each
# gets its height on the line here: baseline at row 14 of the cell, the ones
# that go below it down to row 18, the marks up high at the top, the
# middle ones centred on the lowercase (rows 5..14)
BAJAN = "gjpqy,;çþýÿµ()[]{}|$@ƒ"
ARRIBA = "'\"`^~*°ªº´¨¯¸²³¹‘’“”ˆ˜™"
MEDIO = "-+=·×÷¬–—•<>"


def main():
    a1 = np.array(Image.open(SRC1).convert("RGBA"))[::-1, :, 3].astype(np.float32)   # upright
    a4 = np.array(Image.open(SRC4).convert("RGBA"))[::-1, :, 3].astype(np.float32)
    lado = CELDA * COLS                                   # 390 at 1x
    a2 = a4[:lado * 4, :lado * 4].reshape(lado * 2, 2, lado * 2, 2).mean(axis=(1, 3))
    a2 = np.clip(a2 + 0.5, 0, 255).astype(np.uint8)
    anchos, alto = [], []
    codigos = list(range(PRIMERO, 0x7F)) + list(range(0x80, ULTIMO + 1))
    for i, c in enumerate(codigos):
        x0, y0 = (i % COLS) * CELDA, (i // COLS) * CELDA
        cel = a1[y0:y0 + CELDA, x0:x0 + CELDA]
        cols = np.where(cel.max(0) > 40)[0]
        rows = np.where(cel.max(1) > 40)[0]
        anchos.append(int(cols.max()) + 1 if len(cols) else 0)
        h = int(rows.max()) if len(rows) else 0          # its rows: 1..h
        ch = bytes([c]).decode("cp1252", errors="replace")
        if ch in ARRIBA or ch == 'Q':
            y = 0                                       # Q: its tail is under the cap height
        elif ch in MEDIO:
            y = int(round(9.5 - (h + 1) / 2.0))
        elif ch in BAJAN:
            y = 18 - h
        else:
            y = 14 - h
        alto.append(y)
    # the glyphs' rows in a cell: top and bottom over all of them
    filas = np.array([1, 18])
    print('desplazamientos', min(alto), max(alto))
    lineas = []
    datos = a2.tobytes()
    for i in range(0, len(datos), 32):
        lineas.append("    " + ",".join(str(b) for b in datos[i:i + 32]) + ",")
    OUT.write_text(
        "/* Generado por tools/fuente_juego.py: la pagina de letras del juego (0x21..0xFF,\n"
        " * celdas de 26 px, 15 por fila), 2x, solo alfa. No editar a mano. */\n"
        "#define FJ_CELDA %d\n#define FJ_COLS %d\n#define FJ_LADO %d\n#define FJ_PRIMERO 0x%02X\n#define FJ_ULTIMO 0x%02X\n"
        "#define FJ_ARRIBA %d\n#define FJ_ABAJO %d\n" % (CELDA, COLS, lado * 2, PRIMERO, ULTIMO, filas.min(), filas.max())
        + "/* per glyph (0x21..0x7E, 0x80..0xFF): its width and how far down its top goes on the line\n"
        " * (rows of the 1x page; the baseline at row 14) */\n"
        + "static const unsigned char k_fj_ancho[%d] = { %s };\n" % (len(anchos), ", ".join(map(str, anchos)))
        + "static const signed char k_fj_y[%d] = { %s };\n" % (len(alto), ", ".join(map(str, alto)))
        + "static const unsigned char k_fj_px[%d] = {\n%s\n};\n" % (len(datos), "\n".join(lineas)),
        encoding="utf-8")
    Image.fromarray(a2).save(PORT / "modding/texturas/fuente_juego_2x.png")
    print(f"{OUT}: {lado * 2}x{lado * 2}, anchos {anchos[:20]}...")


if __name__ == "__main__":
    main()
