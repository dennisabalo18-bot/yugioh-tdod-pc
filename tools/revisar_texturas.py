"""Revisa las texturas escaladas por la IA contra sus originales del volcado,
para encontrar las que perdieron algo importante:

- la escalada se achica al tamano original (promedio de area) y se compara
  con el original, solo donde el original no es transparente;
- "cambio": diferencia media grande (la IA invento o borro contenido);
- "color": el color promedio se corrio;
- "detalle": la escalada achicada tiene mucho menos borde fino que el
  original (la IA aliso dibujo que era real: tramas, letras chicas);
- "tinte": un original gris salio con color.

Escribe modding/texturas/revision/informe.txt, hojas de comparacion
(original ampliado sin suavizar | escalada) de las peores de cada tipo y de
una muestra al azar, y revision/fuera_ia.txt: las que pierden algo de verdad
("cambio" o "detalle"), que texturas_pack.py deja con el original.
Limites elegidos mirando las hojas (2026-11-09): debajo de 0.25 de detalle y
encima de 14 de cambio son ruido, follaje y tramas de puntos borrados; entre
0.25 y 0.6 la IA solo limpio el grano (se ve bien). Un color corrido solo
casi siempre es una mejora (brillos, lava): se informa, no se saca.

    py -3 tools/revisar_texturas.py
"""
import pathlib
import random
import re
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

ROOT = pathlib.Path(__file__).resolve().parent.parent / "modding" / "texturas"
UP = ROOT / "mejoradas" / "upscayl_png_digital-art-4x_4x"
OUT = ROOT / "revision"
NAME = re.compile(r"^([0-9A-Fa-f]{16})_(\d+)x(\d+)")

LIM = {"cambio": 14.0, "color": 8.0, "detalle": 0.25, "tinte": 8.0}
FUERA = ("cambio", "detalle")


def edges(a):
    """Energia de borde fino (laplaciano) de una imagen gris float."""
    return float(np.abs(a[1:-1, 1:-1] * 4 - a[:-2, 1:-1] - a[2:, 1:-1] - a[1:-1, :-2] - a[1:-1, 2:]).mean())


def measure(orig, up):
    o = np.asarray(orig.convert("RGBA"), np.float32)
    small = np.asarray(up.convert("RGB").resize(orig.size, Image.BOX), np.float32)
    mask = o[..., 3] > 0
    if mask.sum() < 16:
        return None
    oc, sc = o[..., :3][mask], small[mask]
    mad = float(np.abs(oc - sc).mean())
    shift = float(np.abs(oc.mean(0) - sc.mean(0)).max())
    og, sg = o[..., :3].mean(2), small.mean(2)
    eo = edges(og)
    det = edges(sg) / eo if eo > 2.0 else 1.0
    gray = float(np.abs(oc - oc.mean(1, keepdims=True)).max()) < 2
    tint = float(np.abs(sc - sc.mean(1, keepdims=True)).mean()) if gray else 0.0
    return {"cambio": mad, "color": shift, "detalle": det, "tinte": tint}


def sheet(items, title, path, cell=192):
    cols = 2
    rows = len(items)
    img = Image.new("RGB", (cols * cell + 30, rows * (cell + 18) + 30), (24, 24, 28))
    dr = ImageDraw.Draw(img)
    dr.text((10, 8), title, fill=(230, 230, 230))
    for r, (name, orig, up, note) in enumerate(items):
        y = 30 + r * (cell + 18)
        for c, im in enumerate((orig.convert("RGB").resize((cell, cell * orig.size[1] // max(orig.size)), Image.NEAREST),
                                up.convert("RGB").resize((cell, cell * up.size[1] // max(up.size)), Image.LANCZOS))):
            img.paste(im, (10 + c * (cell + 10), y))
        dr.text((10, y + cell + 2), f"{name[:16]}  {note}", fill=(200, 200, 120))
    img.save(path)


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    dump = {p.name[:16].upper(): p for p in (ROOT / "dump").glob("*.png")}
    res = []
    for p in sorted(UP.glob("*.png")):
        m = NAME.match(p.stem)
        if not m or m.group(1).upper() not in dump:
            continue
        try:
            orig = Image.open(dump[m.group(1).upper()])
            up = Image.open(p)
            r = measure(orig, up)
        except OSError:
            continue
        if r:
            res.append((p.name, r))
    bad = {k: [] for k in LIM}
    for n, r in res:
        if r["cambio"] > LIM["cambio"]:
            bad["cambio"].append((r["cambio"], n))
        if r["color"] > LIM["color"]:
            bad["color"].append((r["color"], n))
        if r["detalle"] < LIM["detalle"]:
            bad["detalle"].append((-r["detalle"], n))
        if r["tinte"] > LIM["tinte"]:
            bad["tinte"].append((r["tinte"], n))
    lines = [f"{len(res)} texturas escaladas revisadas contra su original"]
    allbad = set()
    for k, v in bad.items():
        v.sort(reverse=True)
        allbad.update(n for _, n in v)
        lines.append(f"  {k}: {len(v)} pasan el limite {LIM[k]}")
        for s, n in v[:40]:
            lines.append(f"      {n}  {abs(s):.2f}")
    mads = np.array([r["cambio"] for _, r in res])
    dets = np.array([r["detalle"] for _, r in res])
    lines.append(f"diferencia media: mediana {np.median(mads):.2f}, 95% debajo de {np.percentile(mads, 95):.2f}")
    lines.append(f"detalle conservado: mediana {np.median(dets):.2f}, 5% peor debajo de {np.percentile(dets, 5):.2f}")
    fuera = sorted({n[:16].upper() for k in FUERA for _, n in bad[k]})
    lines.append(f"fuera (queda el original): {len(fuera)} de {len(res)}")
    (OUT / "informe.txt").write_text("\n".join(lines) + "\n", encoding="utf-8")
    (OUT / "fuera_ia.txt").write_text("\n".join(fuera) + "\n", encoding="utf-8")
    print("\n".join(lines[:1] + [l for l in lines if not l.startswith("      ")][1:]))

    def items(names, note):
        out = []
        for n in names:
            orig = Image.open(dump[n[:16].upper()])
            out.append((n, orig, Image.open(UP / n), note(n)))
        return out

    scores = dict(res)
    for k, v in bad.items():
        if v:
            sheet(items([n for _, n in v[:8]], lambda n: f"{k} {scores[n][k]:.2f}"),
                  f"Peores: {k} (izq. original ampliado, der. escalada)", OUT / f"peores_{k}.png")
    random.seed(1)
    ok = [n for n, _ in res if n not in allbad]
    big = [n for n in ok if int(NAME.match(n).group(2)) >= 128]
    for i in range(2):
        pick = random.sample(big, min(8, len(big)))
        sheet(items(pick, lambda n: f"cambio {scores[n]['cambio']:.1f} detalle {scores[n]['detalle']:.2f}"),
              f"Muestra al azar {i + 1} (izq. original ampliado, der. escalada)", OUT / f"muestra_{i + 1}.png")
    return 0


if __name__ == "__main__":
    sys.exit(main())
