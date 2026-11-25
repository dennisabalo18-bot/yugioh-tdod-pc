"""The chain between the chain's cards (2026-11-16): the picture the user chose
(modding/texturas/cadena/cadena_usuario.webp, transparent, a chain bent like
a roof). Its straight right arm (from 720,180 to 1672,880) is turned level and
cut to its links -> modding/texturas/cadena/cadena_tira.png and, half size,
RGBA, ygo-apex/src/cadena_eslabones.h (the kit lays it along each stretch).

    py -3 tools/cadena_tira.py
"""
import math
import pathlib

import numpy as np
from PIL import Image

PORT = pathlib.Path(__file__).resolve().parent.parent
DIR = PORT / "modding" / "texturas" / "cadena"


def tira():
    im = Image.open(DIR / "cadena_usuario.webp").convert("RGBA")
    ang = math.degrees(math.atan2(880 - 180, 1672 - 720))
    r = np.asarray(im.rotate(ang, resample=Image.BICUBIC, expand=True))
    sub = r[:, 720:1760]
    filas = np.where(sub[..., 3].max(1) > 20)[0]
    filas = filas[(filas > 500) & (filas < 900)]
    return Image.fromarray(np.ascontiguousarray(sub[filas.min():filas.max() + 1]))


PERIODO = 356                           # two links (measured: best match 60 px wide at x 60, 1 px lower)
DESDE = 60                              # cut from here
FUNDE = 60                              # the seam is cross-faded over this many px


def baldosa(t):
    """One period that repeats along x without a seam: [DESDE, DESDE + P), its
    first FUNDE px faded into the ones a period later."""
    a = np.asarray(t, np.float32)
    b = a[:, DESDE:DESDE + PERIODO].copy()
    sig = np.roll(a, -1, axis=0)[:, DESDE + PERIODO:DESDE + PERIODO + FUNDE]
    w = np.linspace(0.0, 1.0, FUNDE)[None, :, None]
    b[:, :FUNDE] = b[:, :FUNDE] * w + sig * (1.0 - w)
    return Image.fromarray(np.clip(b + 0.5, 0, 255).astype(np.uint8), "RGBA")


if __name__ == "__main__":
    t = tira()
    t.save(DIR / "cadena_tira.png")
    t = baldosa(t)                      # 2026-11-16 the user chose this one (muestra 3)
    t.save(DIR / "cadena_baldosa.png")
    chico = t.resize((t.size[0] // 2, t.size[1] // 2), Image.LANCZOS)
    px = np.asarray(chico, np.uint8).reshape(-1)
    h = PORT / "ygo-apex" / "src" / "cadena_eslabones.h"
    with open(h, "w", newline="\n") as f:
        f.write("/* Generado por tools/cadena_tira.py: la cadena que eligio el usuario\n"
                " * (modding/texturas/cadena/cadena_usuario.webp), un tramo recto, RGBA.\n"
                " * No editar a mano. */\n")
        f.write("#define ESLABONES_W %d\n#define ESLABONES_H %d\n" % chico.size)
        f.write("static const unsigned char k_eslabones_px[%d] = {\n" % px.size)
        for i in range(0, px.size, 32):
            f.write(",".join(str(int(v)) for v in px[i:i + 32]) + ",\n")
        f.write("};\n")
    print(t.size, chico.size, h)
