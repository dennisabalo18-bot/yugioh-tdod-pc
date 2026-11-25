"""Prueba, para cada imagen suelta del disco (png/tga/jpg, sin las cartas
grandes ni las hojas de miniaturas, que ya tienen su herramienta), varias
formas en que el juego pudo subirla a la GPU, y busca la huella resultante
entre las texturas ya volcadas (modding/texturas/dump). Sirve para aprender
la regla de cada tipo antes de generar las que faltan.

    py -3 tools/analizar_disco.py
"""
import collections
import pathlib
import sys

import numpy as np
from PIL import Image

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from cartas_volcado import tex_hash, swizzle, M, GAME, DUMP  # noqa: E402


def variants(im):
    """(nombre, huella) de cada forma candidata."""
    out = []
    if im.mode == "P":
        idx = np.asarray(im)
        rgb = np.array(im.getpalette()[:768], np.uint8).reshape(-1, 3)
        trns = im.info.get("transparency")
        for flip in (0, 1):
            a = idx[::-1] if flip else idx
            ib = swizzle(a).tobytes()
            hi = tex_hash(ib)
            for amode in ("255", "trns", "c0"):
                pal = np.zeros((256, 4), np.uint8)
                pal[:len(rgb), :3] = rgb[:, ::-1]
                pal[:, 3] = 255
                if amode == "trns":
                    if isinstance(trns, bytes):
                        pal[:len(trns), 3] = np.frombuffer(trns, np.uint8)
                    elif isinstance(trns, int):
                        pal[trns, 3] = 0
                    else:
                        continue
                if amode == "c0":
                    pal[0] = 0
                out.append((f"P8 flip{flip} {amode}",
                            hi ^ ((tex_hash(pal.tobytes()) * 31) & M)))
    rgba = np.asarray(im.convert("RGBA"))
    for flip in (0, 1):
        a = rgba[::-1] if flip else rgba
        bgra = a[..., [2, 1, 0, 3]].copy()
        h, w = bgra.shape[:2]
        flat = bgra.view(np.uint32).reshape(h, w)
        out.append((f"ARGB swz flip{flip}", tex_hash(swizzle(flat).tobytes())))
        out.append((f"ARGB lin flip{flip}", tex_hash(flat.tobytes())))
    return out


def main():
    dumped = {}
    for p in DUMP.glob("*.png"):
        try:
            dumped[int(p.name[:16], 16)] = p.name
        except ValueError:
            pass
    files = []
    for p in GAME.rglob("*"):
        if p.suffix.lower() not in (".png", ".tga", ".jpg"):
            continue
        r = p.relative_to(GAME).as_posix()
        if r.startswith("euro/") and not r.startswith("euro/sp/"):
            continue
        if r.startswith("euro/sp/card/") or r.startswith("card/mini/") or r.startswith("_resources/"):
            continue
        files.append(p)
    hits = collections.Counter()
    found = 0
    per_dir = collections.defaultdict(lambda: [0, 0])
    for p in sorted(files):
        try:
            im = Image.open(p)
            im.load()
        except OSError:
            continue
        d = p.parent.relative_to(GAME).as_posix()
        per_dir[d][1] += 1
        for name, h in variants(im):
            if h in dumped:
                hits[(d, im.mode, name)] += 1
                per_dir[d][0] += 1
                found += 1
                break
    print(f"{len(files)} imagenes del disco, {found} ya volcadas (por huella)")
    for k, v in sorted(hits.items()):
        print(f"  {v:4d}  {k[0]}  modo {k[1]}  -> {k[2]}")
    print("Por carpeta (volcadas / total):")
    for d, (f, t) in sorted(per_dir.items()):
        print(f"  {f:4d} / {t:4d}  {d}")


if __name__ == "__main__":
    sys.exit(main())
