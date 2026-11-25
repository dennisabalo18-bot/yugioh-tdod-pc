"""El recuadro del dibujo de cada carta escalado con remacri-4x (2026-11-19,
elegido por el usuario: digital-art-4x, el del resto del pack, borra la
textura fina del dibujo). Guarda solo el recuadro (x4, orientacion del
volcado) en modding/texturas/dibujos_remacri/HASH.png; texto_cartas.py lo pega
sobre la escalada de siempre (abrir_escalada). Asi no queda otra carta entera
de mas en ningun lado: el pack tiene las mismas entradas.

    py -3 tools/dibujos_remacri.py <carpeta con las escaladas remacri HASH_128x256.png>
"""
import pathlib
import sys

from PIL import Image

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import texto_cartas as tc  # noqa: E402


def main():
    src = pathlib.Path(sys.argv[1])
    tc.DIBUJOS.mkdir(parents=True, exist_ok=True)
    x0, y0, x1, y1 = (v * 4 for v in tc.DIBUJO_CAJA)
    n = 0
    for p in sorted(src.glob("*_128x256.png")):
        im = Image.open(p)
        if im.size != (512, 1024):
            continue
        im.convert("RGB").crop((x0, y0, x1, y1)).save(tc.DIBUJOS / f"{p.name[:16].upper()}.png", optimize=True)
        n += 1
    print(f"dibujos_remacri: {n} recuadros en {tc.DIBUJOS}")


if __name__ == "__main__":
    main()
