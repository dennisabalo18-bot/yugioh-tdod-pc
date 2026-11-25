"""Pone en modding/texturas/dump/ las imagenes sueltas del disco (png) que el
juego todavia no volco, con la misma huella que les daria el port, para
escalarlas sin tener que verlas en el juego (final, animaciones de los
retratos cf_anim, atlas 2D de cada pantalla, en los 5 idiomas).

Regla (tools/analizar_disco.py, 2026-11-09: 27 de 27 coinciden con el volcado
real):
- PNG con paleta: P8, indices dados vuelta en vertical y en orden Morton,
  paleta B,G,R,A con el alfa del tRNS del archivo (255 si no hay);
- PNG RGBA/RGB: A8R8G8B8 dado vuelta y en orden Morton;
- huella como cartas_volcado.py; la imagen del volcado va dada vuelta.
Las de un canal (L/LA: mascara del duelo, sombra) no: su formato en la GPU
no esta comprobado y no ganan nada escaladas.
El atlas comun con el guante borrado (ygo-apex/mods_puntero, modo puntero)
tambien entra: es el que se carga con el mouse.

    py -3 tools/texturas_disco.py
"""
import pathlib
import sys

import numpy as np
from PIL import Image

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from cartas_volcado import tex_hash, swizzle, M, GAME, DUMP, PORT  # noqa: E402


def huella(im):
    if im.mode == "P":
        idx = np.asarray(im)[::-1]
        rgb = np.array(im.getpalette()[:768], np.uint8).reshape(-1, 3)
        pal = np.zeros((256, 4), np.uint8)
        pal[:len(rgb), :3] = rgb[:, ::-1]
        pal[:, 3] = 255
        trns = im.info.get("transparency")
        if isinstance(trns, bytes):
            pal[:len(trns), 3] = np.frombuffer(trns, np.uint8)
        elif isinstance(trns, int):
            pal[trns, 3] = 0
        return tex_hash(swizzle(idx).tobytes()) ^ ((tex_hash(pal.tobytes()) * 31) & M)
    a = np.asarray(im.convert("RGBA"))[::-1]
    h, w = a.shape[:2]
    flat = np.ascontiguousarray(a[..., [2, 1, 0, 3]]).view(np.uint32).reshape(h, w)
    return tex_hash(swizzle(flat).tobytes())


def pow2(n):
    return n >= 8 and n & (n - 1) == 0


def main():
    files = []
    for p in sorted(GAME.rglob("*.png")):
        r = p.relative_to(GAME).as_posix()
        # los menus de los 5 idiomas (2026-11-12: en ingles se veian sin escalar);
        # las cartas grandes no (cartas_volcado.py, texto_cartas.py)
        if (r.startswith("euro/") and r.split("/")[2:3] == ["card"]) or r.startswith("card/")                 or r.startswith("_resources/"):
            continue
        files.append(p)
    mods = PORT / "ygo-apex" / "mods_puntero" / "euro"
    files += sorted(mods.rglob("*.png")) if mods.is_dir() else []
    have = {p.name[:16].upper() for p in DUMP.glob("*.png")}
    new = skip = 0
    for p in files:
        im = Image.open(p)
        im.load()
        w, h = im.size
        if im.mode not in ("P", "RGBA", "RGB") or not (pow2(w) and pow2(h)):
            print(f"  fuera ({im.mode} {w}x{h}): {p.name}")
            skip += 1
            continue
        key = f"{huella(im):016X}"
        if key in have:
            continue
        im.convert("RGBA").transpose(Image.FLIP_TOP_BOTTOM).save(DUMP / f"{key}_{w}x{h}.png")
        have.add(key)
        new += 1
    print(f"{len(files)} imagenes, {new} nuevas en dump/, {skip} fuera")
    return 0


if __name__ == "__main__":
    sys.exit(main())
