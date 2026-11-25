"""Zonas de una textura escaladas con otro modelo de Upscayl.

digital-art-4x (el de todo el pack) a veces deforma una cara chica. Acá, por
textura, una zona (px del volcado) sale de otro modelo y se funde con la
escalada de siempre (FUNDIDO px del volcado). Resultado en
mejoradas/caras/HASH_WxH.png (texturas_pack.py le da prioridad).

  48E137E53FCE3B33  cf_anim/bg, el Mago Oscuro detrás de Yami Yugi (dado
                    vuelta): la cara con ultramix-balanced-4x (comparados 6
                    modelos el 2026-11-10; remacri-4x queda parecido).

    py -3 tools/zonas_otro_modelo.py
"""
import pathlib
import subprocess
import sys

import numpy as np
from PIL import Image

PORT = pathlib.Path(__file__).resolve().parent.parent
TEX = PORT / "modding" / "texturas"
BASE = TEX / "mejoradas" / "upscayl_png_digital-art-4x_4x"
OUT = TEX / "mejoradas" / "caras"
MODELOS = TEX / "modelos"            # las escaladas con otros modelos
UPSCAYL = pathlib.Path(r"C:\Program Files\Upscayl\resources")
K = 4

ZONAS = {
    "48E137E53FCE3B33_512x256.png": ("ultramix-balanced-4x", [(84, 140, 160, 205)], 8),
}


def escalar(src, modelo):
    out = MODELOS / modelo / src.name
    if not out.is_file():
        out.parent.mkdir(parents=True, exist_ok=True)
        r = subprocess.run([str(UPSCAYL / "bin" / "upscayl-bin.exe"), "-i", str(src), "-o", str(out),
                            "-s", "4", "-m", str(UPSCAYL / "models"), "-n", modelo, "-f", "png"],
                           capture_output=True, text=True, errors="replace")
        if r.returncode or not out.is_file():
            raise RuntimeError(f"Upscayl falló con {src.name}: {r.stderr[-300:]}")
    return out


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    for name, (modelo, cajas, fundido) in ZONAS.items():
        src = TEX / "dump" / name
        base = Image.open(BASE / name).convert("RGBA")
        otro = Image.open(escalar(src, modelo)).convert("RGBA")
        img = base.copy()
        for x0, y0, x1, y1 in cajas:
            w, h = (x1 - x0) * K, (y1 - y0) * K
            yy = np.minimum(np.arange(h), np.arange(h)[::-1])[:, None]
            xx = np.minimum(np.arange(w), np.arange(w)[::-1])[None, :]
            m = np.clip(np.minimum(yy, xx) / (fundido * K), 0, 1)
            m = m * m * (3 - 2 * m)
            box = (x0 * K, y0 * K, x1 * K, y1 * K)
            img.paste(otro.crop(box), box[:2], Image.fromarray((m * 255).astype(np.uint8), "L"))
        img.save(OUT / name)
        print(f"{name}: {len(cajas)} zona(s) con {modelo}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
