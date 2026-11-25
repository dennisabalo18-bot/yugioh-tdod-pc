"""Reverso de carta nuevo (card/normal/card_ura.tga, 128x256, huella
E86E1EE8D605D332) desde modding/texturas/reverso/reverso_fuente.png.

- el fondo (el remolino) se escala con Upscayl x4;
- el logo (los kanji) sale de la imagen fuente sin IA (LANCZOS): la IA
  deforma las letras;
- "KONAMI(R)" (Arial Black) y los dos renglones en japones
  ("オフィシャルカードゲーム", "デュエルモンスターズ™", Yu Gothic Bold) se
  escriben de nuevo, nitidos;
- la carta ocupa 128x185 de la textura (filas no negras de card_ura.tga);
  sale a x4 en modding/texturas/mejoradas/reverso/HASH_128x256.png.

Zonas en pixeles de la fuente (393x567):
  KONAMI x 30..118, y 24..43; logo x 258..372, y 476..548;
  renglon 1 x 267..355, y 521..528; renglon 2 x 248..368, y 528..545
  (los dos van sobre una franja negra).

    py -3 tools/reverso.py
"""
import pathlib
import subprocess
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import cartas_volcado as cv  # noqa: E402

PORT = pathlib.Path(__file__).resolve().parent.parent
DIR = PORT / "modding" / "texturas" / "reverso"
OUT = PORT / "modding" / "texturas" / "mejoradas" / "reverso"
UPSCAYL = pathlib.Path(r"C:\Program Files\Upscayl\resources")
ARIAL_BLACK = r"C:\Windows\Fonts\ariblk.ttf"
YU_GOTHIC = r"C:\Windows\Fonts\YuGothB.ttc"
K = 4


def tapar(img, box, radio):
    """Tapa el rectangulo con el fondo de alrededor muy desenfocado."""
    x0, y0, x1, y1 = box
    m = radio * 2
    zona = img.crop((x0 - m, y0 - m, x1 + m, y1 + m))
    # las letras viejas (claras) se cambian por el oscuro de alrededor antes
    # de difuminar: si no, dejan un halo claro
    z = np.asarray(zona).copy()
    lum = z[..., :3].mean(axis=2)
    claro = lum > 120
    if (~claro).any():
        z[claro] = np.median(z[~claro], axis=0).astype(np.uint8)
    zona = Image.fromarray(z, zona.mode).filter(ImageFilter.GaussianBlur(radio))
    mask = Image.new("L", zona.size, 0)
    ImageDraw.Draw(mask).rectangle((m, m, m + x1 - x0, m + y1 - y0), fill=255)
    mask = mask.filter(ImageFilter.GaussianBlur(radio / 3))
    img.paste(zona, (x0 - m, y0 - m), mask)


def texto(img, box, s, fuente, color, borde=0, alinear="c"):
    """Escribe s ajustado al alto del recuadro y angostado al ancho."""
    x0, y0, x1, y1 = box
    w, h = x1 - x0, y1 - y0
    size = h * 2
    while size > 4:
        f = ImageFont.truetype(fuente, size)
        l, t, r, b = f.getbbox(s, stroke_width=borde)
        if b - t <= h:
            break
        size -= 1
    capa = Image.new("RGBA", (r - l + 4, b - t + 4), (0, 0, 0, 0))
    ImageDraw.Draw(capa).text((2 - l, 2 - t), s, font=f, fill=color,
                              stroke_width=borde, stroke_fill=(0, 0, 0, 255))
    if capa.size[0] > w:
        capa = capa.resize((w, capa.size[1]), Image.LANCZOS)
    x = x0 + (w - capa.size[0]) // 2 if alinear == "c" else x0
    img.alpha_composite(capa, (x, y0 + (h - capa.size[1]) // 2))


def main():
    fuente = DIR / "reverso_fuente.png"
    src = Image.open(fuente).convert("RGBA")
    # esquinas redondeadas transparentes: van del color del borde
    a = np.asarray(src)
    borde = a[a[..., 3] == 255][:, :3]
    anillo = np.concatenate([a[3:8, 20:-20, :3].reshape(-1, 3), a[-8:-3, 20:-20, :3].reshape(-1, 3)])
    color = tuple(int(v) for v in np.median(anillo if len(anillo) else borde, axis=0))
    fondo = Image.new("RGBA", src.size, color + (255,))
    fondo.alpha_composite(src)
    src = fondo.convert("RGB")
    plano = DIR / "_fuente_rgb.png"
    src.save(plano)

    ia = DIR / "_ia_x4.png"
    if not ia.is_file():
        exe = UPSCAYL / "bin" / "upscayl-bin.exe"
        r = subprocess.run([str(exe), "-i", str(plano), "-o", str(ia), "-s", "4",
                            "-m", str(UPSCAYL / "models"), "-n", "digital-art-4x", "-t", "128", "-f", "png"],
                           capture_output=True, text=True, errors="replace")
        if r.returncode or not ia.is_file():
            print("Upscayl fallo:", r.stdout[-300:], r.stderr[-300:])
            return 1

    W, H = 128 * K, 185 * K
    s = W / src.size[0]
    S = lambda *v: tuple(round(x * s) for x in v)
    img = Image.open(ia).convert("RGBA").resize((W, H), Image.LANCZOS)

    # logo sin IA, con el borde fundido
    lb = (258, 476, 372, 548)
    logo = src.crop(lb).resize((S(lb[2] - lb[0])[0], S(lb[3] - lb[1])[0]), Image.LANCZOS).convert("RGBA")
    mask = Image.new("L", logo.size, 0)
    ImageDraw.Draw(mask).rectangle((6, 6, logo.size[0] - 7, logo.size[1] - 7), fill=255)
    img.paste(logo, S(lb[0], lb[1]), mask.filter(ImageFilter.GaussianBlur(4)))

    # KONAMI(R)
    tapar(img, S(28, 22, 120, 45), 6)
    texto(img, S(31, 27, 108, 41), "KONAMI", ARIAL_BLACK, (255, 255, 255, 255), alinear="i")
    f = ImageFont.truetype(ARIAL_BLACK, round(7 * s))
    ImageDraw.Draw(img).text(S(109, 33), "®", font=f, fill=(255, 255, 255, 255))

    # renglones en japones
    dr = ImageDraw.Draw(img)
    dr.rectangle(S(266, 521, 356, 528), fill=(8, 8, 8, 255))
    texto(img, S(268, 521, 354, 528), "オフィシャルカードゲーム", YU_GOTHIC, (245, 245, 245, 255))
    # renglon 2 en una franja negra como el 1 (el original va sobre el
    # remolino con borde negro: taparlo dejaba restos y una mancha gris)
    dr.rounded_rectangle(S(245, 528, 373, 546), radius=round(2 * s), fill=(8, 8, 8, 255))
    texto(img, S(249, 529, 361, 545), "デュエルモンスターズ", YU_GOTHIC, (240, 240, 245, 255))
    f = ImageFont.truetype(ARIAL_BLACK, round(4 * s))
    ImageDraw.Draw(img).text(S(362, 539), "TM", font=f, fill=(240, 240, 245, 255))

    # a la textura: 128x256 a x4, la carta arriba y negro abajo (como el .tga)
    tex = Image.new("RGBA", (W, 256 * K), (0, 0, 0, 255))
    tex.paste(img, (0, 0))
    ura = Image.open(cv.GAME / "card" / "normal" / "card_ura.tga")
    h = f"{cv.huella(ura):016X}"
    OUT.mkdir(parents=True, exist_ok=True)
    tex.transpose(Image.FLIP_TOP_BOTTOM).save(OUT / f"{h}_128x256.png")
    img.convert("RGB").save(DIR / "vista_previa.png")
    print(f"reverso -> {OUT / (h + '_128x256.png')}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
