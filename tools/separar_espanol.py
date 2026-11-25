"""Separa del pack grande (textures_bc3.pack) lo que es solo del espanol: las
cartas grandes y las hojas de menus de euro/sp (y la del modo puntero) cuya
huella no aparece igual en ningun otro idioma. Van a textures_es.pack, como
los textures_en/fr/de/it.pack (tools/idiomas_pack.py); el grande queda con
lo comun a todos los idiomas.

No se descomprime ni se rehace nada: se copian los bytes de cada textura (ya
en BC3) del pack viejo a los dos nuevos, con su indice (XRTP v2: cabecera
<4sIIIQII, entradas <QIIIIQ hash, ancho, alto, niveles, flags, offset;
ordenadas por hash). El pack viejo se reemplaza al final (y su .usadas queda).

Se puede correr de nuevo despues de rehacer el pack grande con
crear_pack_texturas.bat (que vuelve a meter el espanol).

    py -3 tools/separar_espanol.py
"""
import pathlib
import struct
import sys

from PIL import Image

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import cartas_volcado as cv  # noqa: E402
from texturas_disco import huella as huella_png  # noqa: E402

PORT = cv.PORT
GAME = cv.GAME
TEX = PORT / "modding" / "texturas"
GRANDE = TEX / "textures_bc3.pack"
ES = TEX / "textures_es.pack"
OTROS = ("en", "fr", "gr", "it")
HDR = struct.Struct("<4sIIIQII")
ENT = struct.Struct("<QIIIIQ")
BLOQUE = 64 << 20


def huellas(carpeta):
    """las huellas de las cartas grandes y de las hojas de un idioma"""
    out = set()
    bases = [GAME / "euro" / carpeta, PORT / "ygo-apex" / "mods_puntero" / "euro" / carpeta]
    for base in bases:
        if not base.is_dir():
            continue
        for p in base.rglob("*.tga"):
            im = Image.open(p)
            if im.mode == "P" and im.size == (128, 256):
                out.add(cv.huella(im))
        for p in base.rglob("*.png"):
            im = Image.open(p)
            im.load()
            if im.mode in ("P", "RGBA", "RGB"):
                out.add(huella_png(im))
    return out


def escribir(dst, cab, ents, src):
    """un pack con esas entradas (hash, ancho, alto, niveles, flags, offset, largo) de src"""
    n = len(ents)
    with open(dst, "wb") as f:
        f.write(b"\0" * HDR.size)
        nuevas = []
        for h, w, hh, lv, fl, off, largo in ents:
            pos = f.tell()
            src.seek(off)
            resto = largo
            while resto:
                b = src.read(min(resto, BLOQUE))
                if not b:
                    raise SystemExit(f"{GRANDE.name}: se corta en la textura {h:016X}")
                f.write(b)
                resto -= len(b)
            nuevas.append(ENT.pack(h, w, hh, lv, fl, pos))
        idx = f.tell()
        f.write(b"".join(nuevas))
        f.seek(0)
        f.write(HDR.pack(cab[0], cab[1], n, cab[3], idx, cab[5], cab[6]))


def main():
    es = huellas("sp")
    otros = set()
    for l in OTROS:
        otros |= huellas(l)
    solo_es = es - otros
    print(f"espanol: {len(es)} texturas, {len(solo_es)} solo del espanol")
    tam = GRANDE.stat().st_size
    with open(GRANDE, "rb") as src:
        cab = HDR.unpack(src.read(HDR.size))
        if cab[0] != b"XRTP" or cab[1] != 2:
            raise SystemExit(f"{GRANDE}: no es un pack XRTP v2")
        n, idx = cab[2], cab[4]
        src.seek(idx)
        raw = src.read(n * ENT.size)
        ents = [ENT.unpack_from(raw, i * ENT.size) for i in range(n)]
        # el largo de cada textura: hasta la siguiente (o hasta el indice)
        offs = sorted(e[5] for e in ents) + [idx]
        sig = {o: offs[i + 1] for i, o in enumerate(offs[:-1])}
        todas = [(*e, sig[e[5]] - e[5]) for e in ents]
        del tam
        a_es = [e for e in todas if e[0] in solo_es]
        comun = [e for e in todas if e[0] not in solo_es]
        if not a_es:
            print("el pack grande ya no tiene texturas solo del espanol: nada que hacer")
            return 0
        nuevo = GRANDE.with_name("textures_bc3_nuevo.pack")
        escribir(ES, cab, a_es, src)
        print(f"{ES.name}: {len(a_es)} texturas, {ES.stat().st_size / 2**20:.0f} MB")
        escribir(nuevo, cab, comun, src)
        print(f"{GRANDE.name}: {len(comun)} texturas, {nuevo.stat().st_size / 2**30:.2f} GB")
    nuevo.replace(GRANDE)
    return 0


if __name__ == "__main__":
    sys.exit(main())
