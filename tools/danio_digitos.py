"""Los numeros del propio juego para el numero de dano (2026-11-15).

El atlas del duelo (82D3B7DD2E4DECF3_1024x512, el mismo en todos los idiomas
en esa franja) trae una tira "0123456789-+" blanca con borde negro (filas
249..297, columnas 0..455 del atlas 1x; el volcado esta dado vuelta en
vertical). Se toma de la version escalada 4x (mejoradas/upscayl...), se
reduce a 2x y se guarda como luminancia + alfa en
ygo-apex/src/danio_digitos.h, con la caja (x0, x1) de cada simbolo.

    py -3 tools/danio_digitos.py
"""
import pathlib

import numpy as np
from PIL import Image

PORT = pathlib.Path(__file__).resolve().parents[1]
SRC = PORT / "modding/texturas/mejoradas/upscayl_png_digital-art-4x_4x/82D3B7DD2E4DECF3_1024x512.png"
OUT = PORT / "ygo-apex/src/danio_digitos.h"

# 1x: filas 248..298, columnas de cada simbolo medidas por el alfa (> 40)
FILAS = (247, 299)
CAJAS = [(1, 36), (42, 66), (77, 112), (115, 150), (153, 188), (191, 226), (230, 261),
         (267, 300), (305, 340), (344, 375), (382, 414), (420, 452)]   # 0..9, -, +


def main():
    im = Image.open(SRC).convert("RGBA")
    y0, y1 = FILAS[0] * 4, FILAS[1] * 4
    banda = im.crop((0, y0, 456 * 4, y1)).transpose(Image.FLIP_TOP_BOTTOM)
    w, h = banda.size[0] // 2, banda.size[1] // 2
    a = np.array(banda, np.float32) / 255.0
    # reducido con alfa premultiplicado
    pm = a.copy()
    pm[..., :3] *= pm[..., 3:4]
    pm = pm.reshape(h, 2, w, 2, 4).mean(axis=(1, 3))
    al = pm[..., 3]
    lum = np.where(al > 1e-4, pm[..., :3].max(-1) / np.maximum(al, 1e-4), 0.0)
    px = np.stack([np.clip(lum, 0, 1), np.clip(al, 0, 1)], -1)
    px = (px * 255 + 0.5).astype(np.uint8)
    Image.fromarray(np.dstack([px[..., 0]] * 3 + [px[..., 1]]), "RGBA").save(PORT / "modding/texturas/danio_digitos_2x.png")
    datos = px.tobytes()
    lineas = []
    for i in range(0, len(datos), 24):
        lineas.append("    " + ",".join(str(b) for b in datos[i:i + 24]) + ",")
    cajas = ", ".join("{%d, %d}" % (x0 * 2 - 1, x1 * 2 + 2) for x0, x1 in CAJAS)
    OUT.write_text(
        "/* Generado por tools/danio_digitos.py: la tira \"0123456789-+\" del atlas del duelo,\n"
        " * 2x, luminancia y alfa (R8G8). No editar a mano. */\n"
        "#define DANIO_W %d\n#define DANIO_H %d\n" % (w, h)
        + "static const int k_danio_caja[12][2] = { %s };\n" % cajas
        + "static const unsigned char k_danio_px[%d] = {\n%s\n};\n" % (len(datos), "\n".join(lineas)),
        encoding="utf-8")
    print(f"{OUT}: {w}x{h}, {len(datos)} bytes")


if __name__ == "__main__":
    main()
