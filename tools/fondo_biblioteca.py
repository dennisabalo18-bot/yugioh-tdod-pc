"""Fondo nuevo de la Biblioteca: cuero marron con costura dorada entre filas
(opcion D elegida por el usuario 2026-11-09).

El fondo es la franja libr_00_02 (518x57, 6 filas) de la hoja
euro/sp/libr/2d/texdb/texdb000.png (RGBA 1024x256, huella 7736AB5A3CB3943D:
texeles B,G,R,A "swizzled", invertida en vertical); el juego la repite hacia
abajo. Aca se pinta encima de la hoja ya escalada (x4) y se guarda en
modding/texturas/mejoradas/fondo/, que texturas_pack.py prefiere.
- el cuero es ruido que se repite sin cortes (FFT), asi la franja empalma
  consigo misma arriba y abajo;
- las marcas naranjas de la izquierda (14 px) quedan como estaban.

    py -3 tools/fondo_biblioteca.py
"""
import pathlib
import sys

import numpy as np
from PIL import Image

PORT = pathlib.Path(__file__).resolve().parent.parent
ROOT = PORT / "modding" / "texturas"
HOJA = "7736AB5A3CB3943D_1024x256.png"
OUT = ROOT / "mejoradas" / "fondo"
FRANJA = (0, 114, 518, 171)   # x0, y0, x1, y1 en la hoja (orientacion del .png del juego)
FILAS = 6
MARCAS = 14
CUERO = np.array([62, 36, 20], np.float32)
VARIA = np.array([10, 7, 4], np.float32)
HILO = np.array([150, 115, 55], np.float32)


def ruido(rng, h, w, sig):
    """Ruido suave que se repite sin costura en los dos ejes."""
    f = np.fft.fft2(rng.standard_normal((h, w)))
    fy = np.fft.fftfreq(h)[:, None]
    fx = np.fft.fftfreq(w)[None, :]
    r = np.real(np.fft.ifft2(f * np.exp(-((fx * sig) ** 2 + (fy * sig) ** 2) * 20)))
    return (r - r.mean()) / r.std()


def main():
    base = next((ROOT / "mejoradas").glob(f"upscayl*/{HOJA}"), None)
    if not base:
        print(f"Falta la hoja escalada {HOJA}: primero escalar_texturas.bat")
        return 1
    hoja = Image.open(base).convert("RGBA").transpose(Image.FLIP_TOP_BOTTOM)
    k = hoja.size[0] // 1024
    x0, y0, x1, y1 = (v * k for v in FRANJA)
    W, H = x1 - x0, y1 - y0
    rng = np.random.default_rng(3)
    n = ruido(rng, H, W, 6 * k / 4) * 0.7 + ruido(rng, H, W, 1.5 * k / 4) * 0.3
    a = CUERO + n[..., None] * VARIA
    # costura: trazos de 12 px cada 24 (a x4), 4 px de alto, centrados en el
    # borde de cada fila (la fila 0 empalma con la 6 de la franja de abajo)
    g = max(1, k // 2)
    for r in range(FILAS):
        y = round(r * H / FILAS)
        filas = [(y + d) % H for d in range(-g, g)]
        for x in range(MARCAS * k, W, 6 * k):
            a[filas, x:x + 3 * k] = HILO
    franja = np.asarray(hoja.crop((x0, y0, x1, y1)), np.float32).copy()
    franja[:, MARCAS * k:, :3] = a[:, MARCAS * k:]
    franja[:, MARCAS * k:, 3] = 255
    hoja.paste(Image.fromarray(franja.clip(0, 255).astype(np.uint8), "RGBA"), (x0, y0))
    OUT.mkdir(parents=True, exist_ok=True)
    hoja.transpose(Image.FLIP_TOP_BOTTOM).save(OUT / HOJA)
    print(f"fondo de cuero -> {OUT / HOJA}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
