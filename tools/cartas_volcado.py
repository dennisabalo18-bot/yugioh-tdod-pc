"""Pone en modding/texturas/dump/ todas las cartas grandes del juego, como si
se hubieran visto en la Biblioteca con el volcado prendido: asi no hace falta
abrir las ~1190 cartas una por una.

El nombre de cada textura (HASH_128x256.png) es la huella que calcula el port
(nv2a_tex_hash, xboxrecomp-apex/src/kernel/nv2a_backend_common.c) sobre los
bytes que el juego deja en memoria. Para una carta (P8, 128x256, sin mipmaps):
- indices = el .tga dado vuelta en vertical y "swizzled" (orden Morton del
  NV2A: bits de x e y intercalados, x primero), 32768 bytes;
- paleta = 256 entradas B,G,R,A con A = 255;
- huella = hash(indices) ^ (hash(paleta) * 31).
Comprobado 2026-11-09 contra las 43 cartas que el juego volco de verdad.

    py -3 tools/cartas_volcado.py [carpeta_del_juego]
"""
import pathlib
import sys

import numpy as np
from PIL import Image

PORT = pathlib.Path(__file__).resolve().parent.parent
DUMP = PORT / "modding" / "texturas" / "dump"
GAME = PORT.parent / "Yu-Gi-Oh TDOD"
M = (1 << 64) - 1
PRIME = 0x100000001B3
SEED = 0xCBF29CE484222325


def tex_hash(b, h=SEED):
    l = [h, h ^ 1, h ^ 2, h ^ 3]
    n = len(b) // 32 * 32
    for row in np.frombuffer(b[:n], np.uint64).reshape(-1, 4).tolist():
        for j in range(4):
            l[j] = ((l[j] ^ row[j]) * PRIME) & M
    for c in b[n:]:
        l[0] = ((l[0] ^ c) * PRIME) & M
    return ((l[0] ^ ((l[1] << 1) & M) ^ ((l[2] << 2) & M) ^ ((l[3] << 3) & M)) * PRIME) & M


_orden = {}


def swizzle(a):
    h, w = a.shape
    if (h, w) not in _orden:
        ys, xs = np.mgrid[0:h, 0:w]
        idx = np.zeros((h, w), np.int64)
        sx = sy = i = 0
        while (1 << sx) < w or (1 << sy) < h:
            if (1 << sx) < w:
                idx |= ((xs >> sx) & 1) << i
                sx += 1
                i += 1
            if (1 << sy) < h:
                idx |= ((ys >> sy) & 1) << i
                sy += 1
                i += 1
        _orden[(h, w)] = idx.ravel()
    out = np.empty(h * w, a.dtype)
    out[_orden[(h, w)]] = a.ravel()
    return out


def huella(im):
    a = np.asarray(im)[::-1]
    rgb = np.array(im.getpalette()[:768], np.uint8).reshape(-1, 3)
    pal = np.zeros((256, 4), np.uint8)
    pal[:len(rgb), :3] = rgb[:, ::-1]
    pal[:, 3] = 255
    return tex_hash(swizzle(a).tobytes()) ^ ((tex_hash(pal.tobytes()) * 31) & M)


def main():
    game = pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else GAME
    tgas = game / "euro" / "sp" / "card" / "normal"
    if not tgas.is_dir():
        print(f"No encuentro las cartas en {tgas}")
        return 1
    DUMP.mkdir(parents=True, exist_ok=True)
    ya = {p.name[:16].upper() for p in DUMP.glob("*.png")}
    nuevas = coinciden = raras = 0
    for p in sorted(tgas.rglob("*.tga")):
        im = Image.open(p)
        if im.mode != "P" or im.size != (128, 256):
            raras += 1
            continue
        h = f"{huella(im):016X}"
        if h in ya:
            coinciden += 1
            continue
        im.convert("RGBA").transpose(Image.FLIP_TOP_BOTTOM).save(DUMP / f"{h}_128x256.png")
        nuevas += 1
    print(f"{nuevas} cartas nuevas en {DUMP}; {coinciden} ya estaban volcadas por el juego"
          + (f"; {raras} con otro formato, salteadas" if raras else ""))
    return 0


if __name__ == "__main__":
    sys.exit(main())
