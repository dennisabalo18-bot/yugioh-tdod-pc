"""Arma en alta resolucion las 10 hojas de cartas miniatura (card/mini/texdb/
texdbNNN.png, 512x512, ~120 cartas cada una) con las cartas grandes ya
mejoradas: la IA convierte estas hojas en manchas (2026-11-08), pero cada
miniatura es la carta grande achicada.

- texdbNNN.txt da cada casillero: "ID.png 35 51 u0 v0 u1 v1 girada"
  (v medida desde abajo; girada = 1: la carta acostada, su parte de arriba
  hacia la derecha).
- la carta de cada ID sale de mejoradas/texto_cartas/ (o de la escalada, o
  del .tga) por su huella (tools/cartas_volcado.py).
- huella de la hoja: P8 512x512 como las cartas, pero el color 0 de la
  paleta es transparente en memoria (B,G,R,A = 0): comprobado con las 9
  hojas que el juego volco.
- salida: mejoradas/miniaturas/HASH_512x512.png a x4 (2048), toda opaca
  (texturas_pack.py no le copia la transparencia del original: en las
  miniaturas el color 0 deja agujeros en el dibujo).

    py -3 tools/miniaturas.py [carpeta_del_juego] [--idioma=en|fr|de|it]

--idioma: las hojas con las cartas de ese idioma (modding/texturas/idiomas/xx/,
tools/idiomas_pack.py) a idiomas/xx/miniaturas/, para su pack: la hoja es la
misma para todos los idiomas (misma huella) y el juego carga solo el pack del
idioma elegido (main.c), que gana sobre el grande.
"""
import pathlib
import sys

import numpy as np
from PIL import Image, ImageDraw

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import cartas_volcado as cv  # noqa: E402

PORT = pathlib.Path(__file__).resolve().parent.parent
ROOT = PORT / "modding" / "texturas"
OUT = ROOT / "mejoradas" / "miniaturas"
K = 4
BORDE = (235, 195, 80)


def huella_hoja(im):
    a = np.asarray(im)[::-1]
    rgba = np.array(im.getpalette("RGBA"), np.uint8).reshape(-1, 4)
    pal = np.zeros((256, 4), np.uint8)
    pal[:len(rgba)] = rgba[:, [2, 1, 0, 3]]
    pal[0] = 0
    return cv.tex_hash(cv.swizzle(a).tobytes()) ^ ((cv.tex_hash(pal.tobytes()) * 31) & cv.M)


def carta_grande(cid, tgas, mejoradas):
    """La carta mejorada en orientacion normal (arriba = nombre), recortada
    al rectangulo de la carta, o None."""
    p = tgas / f"{cid // 100 * 100}" / f"{cid}.tga"
    if not p.is_file():
        return None
    im = Image.open(p)
    h = f"{cv.huella(im):016X}"
    src = mejoradas.get(h)
    big = (Image.open(src).convert("RGB").transpose(Image.FLIP_TOP_BOTTOM) if src
           else im.convert("RGB").resize((128 * K, 256 * K), Image.LANCZOS))
    # alto de la carta dentro de la textura 128x256: filas no negras del .tga
    g = np.asarray(im.convert("L")).max(axis=1)
    alto = int(np.nonzero(g > 16)[0].max()) + 1
    k = big.size[0] // 128
    return big.crop((0, 0, 128 * k, alto * k))


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    idioma = next((a[9:] for a in sys.argv[1:] if a.startswith("--idioma=")), None)
    game = pathlib.Path(args[0]) if args else cv.GAME
    texdb = game / "card" / "mini" / "texdb"
    carpeta = {"en": "en", "fr": "fr", "de": "gr", "it": "it"}.get(idioma, "sp")
    tgas = game / "euro" / carpeta / "card" / "normal"
    out_dir = ROOT / "idiomas" / idioma / "miniaturas" if idioma else OUT
    if not texdb.is_dir() or not tgas.is_dir():
        print(f"No encuentro el juego en {game}")
        return 1
    mejoradas = {}
    if idioma:      # las cartas de ese idioma; las que no estan: la escalada en espanol no (otro texto)
        for p in (ROOT / "idiomas" / idioma).glob("*_128x256.png"):
            mejoradas[p.name[:16].upper()] = p
    for sub in (() if idioma else ("texto_cartas", None)):
        base = ROOT / "mejoradas" / sub if sub else ROOT / "mejoradas"
        for p in base.rglob("*_128x256.png"):
            if p.parent.name in ("miniaturas",):
                continue
            mejoradas.setdefault(p.name[:16].upper(), p)
    out_dir.mkdir(parents=True, exist_ok=True)
    for png in sorted(texdb.glob("texdb*.png")):
        txt = png.with_suffix(".txt")
        hoja = Image.open(png)
        W, H = hoja.size
        out = hoja.convert("RGB").resize((W * K, H * K), Image.LANCZOS)
        puestas = faltan = 0
        for ln in txt.read_text(encoding="latin-1").splitlines():
            f = ln.split()
            if len(f) < 8 or not f[0].lower().endswith(".png"):
                continue
            cid = int(f[0][:-4])
            u0, v0, u1, v1, girada = map(float, f[3:8])
            x0, x1 = round(u0 * W * K), round(u1 * W * K)
            y0, y1 = round((1 - v1) * H * K), round((1 - v0) * H * K)
            c = carta_grande(cid, tgas, mejoradas)
            if c is None:
                faltan += 1
                continue
            if girada:
                c = c.rotate(-90, expand=True)
            out.paste(c.resize((x1 - x0, y1 - y0), Image.LANCZOS), (x0, y0))
            # borde dorado por dentro del casillero (2026-11-09, el usuario
            # pidio que las cartas se destaquen del fondo de la Biblioteca)
            ImageDraw.Draw(out).rectangle((x0, y0, x1 - 1, y1 - 1), outline=BORDE, width=K + 2)
            puestas += 1
        h = f"{huella_hoja(hoja):016X}"
        out.convert("RGBA").transpose(Image.FLIP_TOP_BOTTOM).save(out_dir / f"{h}_{W}x{H}.png")
        print(f"  {png.name}: {puestas} cartas" + (f", {faltan} sin carta grande" if faltan else "") + f" -> {h}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
