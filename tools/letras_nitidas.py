"""Las 3 paginas de letras del juego (512x512, letras blancas, la forma en
el alfa) agrandadas x4 sin IA y con el borde afilado: todo el texto de la
interfaz (dialogos, menus, mensajes del duelo, nombres en las listas) sale
de estas paginas. La IA inventa trazos en las letras (por eso estan en
no_escalar.txt); esto no inventa nada: la misma forma, en el mismo lugar.

- el alfa se agranda con interpolacion bicubica;
- el borde se pone donde el original cruza la mitad de su intensidad local
  (el maximo cerca), con una rampa de ~2 px de suavizado: un trazo fino o
  tenue del original no se pierde y el grosor de cada letra no cambia;
- el color queda blanco.

Salida: modding/texturas/mejoradas/letras/HASH_512x512.png (2048x2048);
texturas_pack.py la toma antes que cualquier otra y sin rehacerle el alfa.
Comprobacion: la pagina nueva achicada a 512 contra la original (diferencia
media del alfa) y una hoja de comparacion en revision/letras_zoom.png.

    py -3 tools/letras_nitidas.py
"""
import pathlib
import sys

import numpy as np
from PIL import Image, ImageFilter

ROOT = pathlib.Path(__file__).resolve().parent.parent / "modding" / "texturas"
PAGES = ("9BF7092A495E4E38", "F867714BEE53B873", "FED7C5881DA64D4A")
K = 4
RAMP = 1.0       # medio ancho de la rampa del borde, en pixeles de salida


def sharpen(alpha):
    """alfa 512x512 uint8 -> 2048x2048 uint8 con borde afilado."""
    a = Image.fromarray(alpha)
    up = np.asarray(a.resize((a.width * K, a.height * K), Image.BICUBIC), np.float32) / 255.0
    # intensidad local: el maximo del original en 3x3, agrandado
    peak = np.asarray(a.filter(ImageFilter.MaxFilter(3)).resize(
        (a.width * K, a.height * K), Image.NEAREST), np.float32) / 255.0
    mid = np.maximum(peak * 0.5, 1e-3)
    # pendiente del alfa agrandado: la rampa se mide en pixeles de salida
    gy, gx = np.gradient(up)
    slope = np.maximum(np.hypot(gx, gy), 1e-4)
    t = np.clip(0.5 + (up - mid) / (2.0 * RAMP * slope), 0.0, 1.0)
    out = t * peak
    out[peak < 1e-3] = 0.0
    return (np.clip(out, 0.0, 1.0) * 255.0 + 0.5).astype(np.uint8)


def main():
    dst = ROOT / "mejoradas" / "letras"
    dst.mkdir(parents=True, exist_ok=True)
    zoom = []
    for h in PAGES:
        src = ROOT / "dump" / f"{h}_512x512.png"
        if not src.is_file():
            print(f"  falta {src.name} en dump/")
            continue
        o = np.asarray(Image.open(src).convert("RGBA"))
        a = sharpen(o[..., 3])
        img = np.dstack([np.full(a.shape, 255, np.uint8)] * 3 + [a])
        Image.fromarray(img, "RGBA").save(dst / f"{h}_512x512.png")
        small = np.asarray(Image.fromarray(a).resize((512, 512), Image.BOX), np.float32)
        diff = np.abs(small - o[..., 3].astype(np.float32))
        ink = o[..., 3] > 0
        print(f"  {h}: diferencia media del alfa {diff[ink].mean():.1f} (sobre las letras),"
              f" maxima {diff.max():.0f}; tinta {o[..., 3].sum() / 255:.0f} -> {small.sum() / 255:.0f}")
        zoom.append((o[..., 3], a))
    # hoja: un recorte de cada pagina, original ampliado sin suavizar | nuevo
    if zoom:
        x0, y0, w, h = 0, 300, 200, 100
        sheet = Image.new("RGB", (2 * w * K + 30, len(zoom) * (h * K + 10) + 10), (30, 30, 46))
        for i, (oa, na) in enumerate(zoom):
            oc = Image.fromarray(oa[y0:y0 + h, x0:x0 + w]).resize((w * K, h * K), Image.NEAREST)
            nc = Image.fromarray(na[y0 * K:(y0 + h) * K, x0 * K:(x0 + w) * K])
            y = 10 + i * (h * K + 10)
            for j, c in enumerate((oc, nc)):
                tile = Image.new("RGB", c.size, (30, 30, 46))
                tile.paste(Image.new("RGB", c.size, (255, 255, 255)), (0, 0), c)
                sheet.paste(tile.transpose(Image.FLIP_TOP_BOTTOM), (10 + j * (w * K + 10), y))
        (ROOT / "revision").mkdir(exist_ok=True)
        sheet.save(ROOT / "revision" / "letras_zoom.png")
    return 0


if __name__ == "__main__":
    sys.exit(main())
