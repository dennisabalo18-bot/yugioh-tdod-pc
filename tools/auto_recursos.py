"""La placa AUTO del duelo (auto.c): Mods-Extras\auto\auto_on.png y auto_off.png;
al lado, el boton de la Carta As: as_on.png y as_off.png (ON / OFF).

La placa es la del nombre del jugador (modding/texturas/auto/placa_nombre.png:
recortada de una captura del juego a 1919x1079, 378x70), acortada (sus dos
puntas y el medio repetido) y con "AUTO" en la letra del juego (la pagina de
letras escalada, mejoradas/letras/9BF7092A495E4E38) y una luz: verde encendida,
oscura apagada. Muestra aprobada el 2026-11-18.

    py -3 tools/auto_recursos.py [carpeta de salida]
"""
import pathlib
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

PORT = pathlib.Path(__file__).resolve().parents[1]
SRC = PORT / "modding/texturas/auto"
OUT = pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else SRC / "juego"
PAGINA = PORT / "modding/texturas/mejoradas/letras/9BF7092A495E4E38_512x512.png"
C = 104                                  # celda de la pagina x4


def letras(texto, alto, color):
    a_pag = np.asarray(Image.open(PAGINA).transpose(Image.FLIP_TOP_BOTTOM).convert("RGBA"))[:, :, 3]
    gs = []
    for ch in texto:
        r, c = divmod(ord(ch) - 0x21, 15)
        g = a_pag[r * C:(r + 1) * C, c * C:(c + 1) * C]
        cols = np.where(g.max(axis=0) > 40)[0]
        gs.append(g[:, cols[0]:cols[-1] + 1])
    sp = 8
    a = np.zeros((C, sum(g.shape[1] for g in gs) + sp * (len(gs) - 1)), np.uint8)
    x = 0
    for g in gs:
        a[:, x:x + g.shape[1]] = np.maximum(a[:, x:x + g.shape[1]], g)
        x += g.shape[1] + sp
    filas = np.where(a.max(axis=1) > 40)[0]
    al = Image.fromarray(a[filas[0]:filas[-1] + 1])
    al = al.resize((int(al.size[0] * alto / al.size[1]), alto), Image.LANCZOS)
    img = Image.new("RGBA", al.size, color + (0,))
    img.putalpha(al)
    sombra = Image.new("RGBA", al.size, (40, 15, 5, 0))
    sombra.putalpha(al.filter(ImageFilter.MaxFilter(3)))
    out = Image.new("RGBA", (al.size[0] + 3, alto + 3), (0, 0, 0, 0))
    out.alpha_composite(sombra, (2, 2))
    out.alpha_composite(img, (0, 0))
    return out


def placa(on):
    p0 = Image.open(SRC / "placa_nombre.png").convert("RGBA")
    izq, der, medio = p0.crop((0, 0, 62, 70)), p0.crop((316, 0, 378, 70)), p0.crop((290, 0, 316, 70))
    W = 280
    p = Image.new("RGBA", (W, 70))
    p.paste(izq, (0, 0))
    for x in range(62, W - 62, 26):
        p.paste(medio, (x, 0))
    p.paste(der, (W - 62, 0))
    t = letras("AUTO", 30, (236, 206, 112) if on else (150, 135, 100))
    p.alpha_composite(t, (68, 20))
    d = ImageDraw.Draw(p)
    lx, ly = 68 + t.size[0] + 26, 35
    if on:
        brillo = Image.new("RGBA", p.size, (0, 0, 0, 0))
        ImageDraw.Draw(brillo).ellipse((lx - 17, ly - 17, lx + 17, ly + 17), fill=(90, 255, 120, 140))
        p.alpha_composite(brillo.filter(ImageFilter.GaussianBlur(5)))
        d.ellipse((lx - 11, ly - 11, lx + 11, ly + 11), fill=(70, 220, 90), outline=(250, 230, 150), width=2)
        d.ellipse((lx - 5, ly - 7, lx + 1, ly - 3), fill=(200, 255, 210))
    else:
        d.ellipse((lx - 11, ly - 11, lx + 11, ly + 11), fill=(70, 45, 30), outline=(170, 150, 100), width=2)
    return p


def boton(on):
    """2026-11-19 (the user: "al lado de auto ... un pequeno boton que diga Off y
    si lo presionas On"): the Carta As switch, a short copy of the same plate"""
    p0 = Image.open(SRC / "placa_nombre.png").convert("RGBA")
    izq, der, medio = p0.crop((0, 0, 62, 70)), p0.crop((316, 0, 378, 70)), p0.crop((290, 0, 316, 70))
    W = 196
    p = Image.new("RGBA", (W, 70))
    p.paste(izq, (0, 0))
    for x in range(62, W - 62, 26):
        p.paste(medio, (x, 0))
    p.paste(der, (W - 62, 0))
    t = letras("ON" if on else "OFF", 30, (236, 206, 112) if on else (150, 135, 100))
    p.alpha_composite(t, ((W - t.size[0]) // 2, 20))
    return p


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    placa(True).save(OUT / "auto_on.png")
    placa(False).save(OUT / "auto_off.png")
    boton(True).save(OUT / "as_on.png")
    boton(False).save(OUT / "as_off.png")
    print(f"AUTO: {OUT}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
