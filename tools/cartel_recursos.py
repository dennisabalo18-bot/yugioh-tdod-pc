"""The main menu banner's middle without its text (2026-11-20, the user: the
Perfil button "abajo dice que es en un texto en el medio de la pantalla"):
perfil.c lays it over the banner's text with "Perfil" while the pointer is on
the Perfil button.

From the user's capture of the main menu (1906 x 1064 of the stretched
640 x 480 frame): the box between the banner's arrows, each row filled from
the pixels just outside it. Its place in game px is CARTEL_* in perfil.c.

Out: modding/texturas/perfil/juego/cartel.png

  py -3 tools/cartel_recursos.py captura.webp
"""
import pathlib
import sys

from PIL import Image

R = pathlib.Path(__file__).resolve().parent.parent
OUT = R / "modding" / "texturas" / "perfil" / "juego"
X0, Y0, X1, Y1 = 646, 768, 1271, 838          # capture px (1906 x 1064)


def main():
    cap = Image.open(sys.argv[1]).convert("RGB")
    assert cap.size == (1906, 1064), cap.size
    im = cap.crop((X0 - 3, Y0, X1 + 3, Y1))
    px = im.load()
    w = im.width
    for y in range(im.height):
        a, b = px[0, y], px[w - 1, y]
        for x in range(1, w - 1):
            t = x / (w - 1)
            px[x, y] = tuple(int(a[i] + (b[i] - a[i]) * t) for i in range(3))
    im = im.crop((3, 0, w - 3, im.height))
    im.convert("RGBA").save(OUT / "cartel.png")
    print("ok", im.size, "game px", X0 * 640 / 1906, Y0 * 480 / 1064, X1 * 640 / 1906, Y1 * 480 / 1064)


if __name__ == "__main__":
    main()


# 2026-11-20 (the user: with the pointer on the Perfil "los otros siguen
# encendidos"): the chosen icon is drawn bright in the row itself, so perfil.c
# lays its unlit look over it: icons 0..5 cut from the same capture (where
# the last one was the chosen), each the game's 60 x 60 px quad round its
# middle (the dark pixels' centroid), the corners faded out
CENTROS = [(466, 500), (529, 319), (707, 193), (955, 145), (1200, 190), (1419, 371)]


def iconos(cap):
    import numpy as np
    from PIL import ImageDraw, ImageFilter
    hw, hh = 30 * 1906 / 640, 30 * 1064 / 480
    for k, (cx, cy) in enumerate(CENTROS):
        im = cap.crop((round(cx - hw), round(cy - hh), round(cx + hw), round(cy + hh))).convert("RGBA")
        mk = Image.new("L", im.size, 0)
        w, h = im.size
        ImageDraw.Draw(mk).ellipse((w * 0.11, h * 0.11, w * 0.89, h * 0.89), fill=255)
        im.putalpha(mk.filter(ImageFilter.GaussianBlur(w * 0.03)))
        im.save(OUT / ("rueda_%d.png" % k))
    print("iconos", w, h)


if __name__ == "__main__" and len(sys.argv) > 1:
    iconos(Image.open(sys.argv[1]).convert("RGB"))
