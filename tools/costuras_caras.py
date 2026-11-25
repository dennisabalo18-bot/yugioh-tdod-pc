"""Costuras de ojos y boca en los retratos de los duelistas (texturas mejoradas).

Los retratos que hablan son cf_anim/large/png/cflNN_E.png (27 duelistas x 3
expresiones, 512x512). Arriba (y 0..408) va el retrato; abajo, recuadros con
otros ojos y bocas que el juego dibuja encima para animarlo. La tabla del
juego (default.xbe):

  0x60F518[E * 27 + NN] -> {rects*, cabecera*, comandos*}
  rects: (y0, x0, y1, x1) int16, el 0 es el retrato entero;
  comandos: 8 bytes (b0, b1, rect int16, x int16, y int16): el recuadro rect
  se dibuja con su esquina en x, y del retrato (b1 con el bit 0x80 es otra
  cosa: un control de la animación, no un recuadro).

En el original los bordes de cada recuadro son iguales al retrato que tapan
(no hay costura). Al escalar con IA cada recuadro se escala sin su alrededor
y el borde sale distinto: en el juego se ve el corte. Arreglo: en la escalada,
el borde de cada recuadro pasa de a poco a la escalada del retrato en ese
lugar (en ANCHO px escalados), que es lo que se ve alrededor; en cada pixel
tanto como el original del recuadro se parecía al retrato ahí (si en el
original ya era distinto, queda el recuadro).

La textura del juego es el archivo dado vuelta en vertical (el volcado,
dump/HASH_512x512.png); la escalada sale de mejoradas/upscayl_png_digital-art-4x_4x
y el resultado va a mejoradas/caras/HASH_512x512.png (texturas_pack.py le
da prioridad sobre la escalada simple).

Los retratos chicos del duelo (cf_anim/small/png/cfsNN.png, 256x512, 28)
traen las 3 expresiones en el mismo archivo: tabla 0x60BA48[NN] ->
{rects*, cabecera*, comandos*, 3}; rects 0..2 son las caras (128 de alto),
la cabecera da por expresión (cantidad, primer comando) y cada recuadro se
dibuja en x, y de la cara de su expresión (2026-11-11: "se les ve la
costura de los ojos cuando lo cierran, cuando estamos en duelo").

    py -3 tools/costuras_caras.py
"""
import pathlib
import struct
import sys

import numpy as np
from PIL import Image

PORT = pathlib.Path(__file__).resolve().parent.parent
GAME = PORT.parent / "Yu-Gi-Oh TDOD"
XBE = PORT / "xbe" / "default.xbe"
TEX = PORT / "modding" / "texturas"
UP = TEX / "mejoradas" / "upscayl_png_digital-art-4x_4x"
OUT = TEX / "mejoradas" / "caras"
TABLE = 0x60F518
SMALL = 0x60BA48     # retratos chicos del duelo
SMALLS = 28
DUELISTS, EXPRS = 27, 3
ANCHO = 10          # px escalados del degradé en el borde de cada recuadro


class Xbe:
    def __init__(self, path):
        self.d = path.read_bytes()
        base = struct.unpack_from("<I", self.d, 0x104)[0]
        n = struct.unpack_from("<I", self.d, 0x11C)[0]
        sh = struct.unpack_from("<I", self.d, 0x120)[0] - base
        self.secs = [struct.unpack_from("<5I", self.d, sh + i * 56)[1:] for i in range(n)]

    def rd(self, a, k):
        for va, vs, ra, rs in self.secs:
            if va <= a and a + k <= va + rs:
                return self.d[ra + a - va: ra + a - va + k]
        raise ValueError(hex(a))

    def u32(self, a):
        return struct.unpack("<I", self.rd(a, 4))[0]


def placements_small(x, nn):
    """Como placements, para el retrato chico nn: posiciones ya sumadas a la
    cara de cada expresión (en pixeles del archivo)."""
    s = x.u32(SMALL + 4 * nn)
    p_rects, p_head, p_cmds, n_exp = (x.u32(s + 4 * j) for j in range(4))
    n = (p_head - p_rects) // 8
    rects = [struct.unpack("<4h", x.rd(p_rects + 8 * i, 8)) for i in range(n)]
    total = (s - p_cmds) // 8
    firsts = [x.rd(p_head + 8 * e, 2)[1] for e in range(n_exp)] + [total]
    out = {}
    for e in range(n_exp):
        fy0, fx0, fy1, fx1 = rects[e]
        for i in range(firsts[e], firsts[e + 1]):
            b0, b1, r, px, py = struct.unpack("<BBhhh", x.rd(p_cmds + 8 * i, 8))
            if b1 & 0x80 or not n_exp <= r < n or not (px or py):
                continue
            y0, x0, y1, x1 = rects[r]
            if px + x1 - x0 <= fx1 - fx0 and py + y1 - y0 <= fy1 - fy0:
                out.setdefault((y0, x0, y1, x1), set()).add((fx0 + px, fy0 + py))
    return out


def placements(x, e):
    """Los recuadros (y0, x0, y1, x1) y dónde se dibujan (x, y) del retrato e."""
    s = x.u32(TABLE + 4 * e)
    p_rects, p_head, p_cmds = x.u32(s), x.u32(s + 4), x.u32(s + 8)
    n = (p_head - p_rects) // 8
    rects = [struct.unpack("<4h", x.rd(p_rects + 8 * i, 8)) for i in range(n)]
    out = {}
    a = p_cmds
    while a < s:
        b0, b1, r, px, py = struct.unpack("<BBhhh", x.rd(a, 8))
        a += 8
        if not b1 & 0x80 and 1 <= r < n and (px or py):
            y0, x0, y1, x1 = rects[r]
            if px + x1 - x0 <= rects[0][3] and py + y1 - y0 <= rects[0][2]:
                out.setdefault((y0, x0, y1, x1), set()).add((px, py))
    return out


def ramp(h, w, k):
    """1 en el borde, 0 desde k px hacia adentro."""
    yy = np.minimum(np.arange(h), np.arange(h)[::-1])[:, None]
    xx = np.minimum(np.arange(w), np.arange(w)[::-1])[None, :]
    d = np.minimum(yy, xx).astype(np.float32)
    return np.clip(1.0 - d / k, 0.0, 1.0)


def main():
    x = Xbe(XBE)
    dumps = {}
    for pat in ("*_512x512.png", "*_256x512.png"):
        for p in (TEX / "dump").glob(pat):
            dumps[p] = None
    OUT.mkdir(parents=True, exist_ok=True)
    hechos = 0
    trabajos = []
    for e in range(DUELISTS * EXPRS):
        nn, ex = e % DUELISTS, e // DUELISTS
        trabajos.append((f"cfl{nn:02d}_{ex}", GAME / "cf_anim" / "large" / "png" / f"cfl{nn:02d}_{ex}.png",
                         lambda e=e: placements(x, e)))
    for nn in range(SMALLS):
        trabajos.append((f"cfs{nn:02d}", GAME / "cf_anim" / "small" / "png" / f"cfs{nn:02d}.png",
                         lambda nn=nn: placements_small(x, nn)))
    for nombre, f, donde in trabajos:
        if not f.is_file():
            continue
        orig = np.asarray(Image.open(f).convert("RGBA"))
        flip = orig[::-1]
        # su volcado (mismo contenido, dado vuelta): de ahí sale el hash
        dump = None
        for p in dumps:
            if dumps[p] is None:
                dumps[p] = np.asarray(Image.open(p).convert("RGBA"))
            d = dumps[p]
            if d.shape == flip.shape and np.abs(d.astype(np.int16) - flip).max() < 3:
                dump = p
                break
        if dump is None:
            print(f"{nombre}: sin volcado (no apareció en el juego todavía)")
            continue
        up_p = UP / dump.name
        if not up_p.is_file():
            print(f"{nombre}: {dump.name} sin escalada")
            continue
        up = np.asarray(Image.open(up_p).convert("RGBA")).astype(np.float32)
        k = up.shape[0] // 512
        H = 512
        res = up.copy()
        pl = donde()
        n = 0
        for (y0, x0, y1, x1), where in sorted(pl.items()):
            h, w = y1 - y0, x1 - x0
            po = orig[y0:y1, x0:x1, :3].astype(np.float32)
            # si va en más de un lugar: donde su borde se parece más
            px, py = min(where, key=lambda q: float(np.abs(
                po - orig[q[1]:q[1] + h, q[0]:q[0] + w, :3]).mean()))
            bo = orig[py:py + h, px:px + w, :3].astype(np.float32)
            sim = np.clip(1.0 - np.abs(po - bo).mean(axis=2) / 24.0, 0.0, 1.0)[::-1]
            sim = np.asarray(Image.fromarray((sim * 255).astype(np.uint8)).resize(
                (w * k, h * k), Image.BILINEAR), np.float32)[..., None] / 255.0
            # a coordenadas de la textura (dada vuelta) y escaladas
            sy = (H - y1) * k
            ty = (H - (py + h)) * k
            piece = up[sy:sy + h * k, x0 * k:(x0 + w) * k]
            base = up[ty:ty + h * k, px * k:(px + w) * k]
            a = ramp(h * k, w * k, ANCHO)[..., None] * sim
            res[sy:sy + h * k, x0 * k:(x0 + w) * k] = a * base + (1 - a) * piece
            n += 1
        out = OUT / dump.name
        Image.fromarray(np.clip(res + 0.5, 0, 255).astype(np.uint8), "RGBA").save(out)
        hechos += 1
        print(f"{nombre} = {dump.stem[:16]}: {n} recuadros")
    print(f"{hechos} retratos en {OUT}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
