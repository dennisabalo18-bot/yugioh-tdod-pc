"""Las cartas del panel "Mazo de ..." (perfil.c, Estadisticas): la misma version
mejorada que muestra el juego (la Biblioteca), una por carta e idioma, a un
tamano de panel.

El juego dibuja la carta grande de euro/<idioma>/card/normal/<NN00>/<ID>.tga
(128 x 256) y el pack de texturas la cambia por su version x4 (512 x 1024),
buscada por la huella de la textura (cartas_volcado.huella):
- modding/texturas/idiomas/<xx>/HUELLA_128x256.png (es, en, de, fr, it);
- modding/texturas/mejoradas/texto_cartas/ (espanol, por si falta);
- las del mod de cartas: su .tga en <mod>/euro/..., su x4 en
  modding/cartas/salida/hd/.
La x4 esta dada vuelta en vertical respecto del .tga; la carta son sus 740
filas de arriba (185 x 4). Sale <salida>/<sp|en|gr|fr|it>/<ID>.jpg, 144 x 208
(casi el doble de lo que ocupa en una pantalla de 1080), solo si falta o cambio.

    py -3 tools/perfil_cartas.py <juego\\Game> <mod archivos> <salida>
"""
import pathlib
import sys

from PIL import Image

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import cartas_volcado as cv  # noqa: E402

PORT = pathlib.Path(__file__).resolve().parent.parent
TEX = PORT / "modding" / "texturas"
MOD_HD = PORT / "modding" / "cartas" / "salida" / "hd"
IDIOMAS = {"sp": "es", "en": "en", "gr": "de", "fr": "fr", "it": "it"}
W, H = 144, 208
GW, GH = 512, 740                       # 2026-11-19: the big card (a click in the panel), x4 as is


def cartas_de_rivales(game):
    """the IDs in the opponents' lists B and A (perfil.c mazo_lista): only
    those get the big picture (624 cards, ~200 MB for 5 languages)"""
    import struct
    x = (game / "default.xbe").read_bytes()
    off = lambda va: va - 0x423080 + 0x413000
    ids = set()
    for tab in (0x453830, 0x453630):
        for d in range(64):
            for r in range(4):
                p = struct.unpack_from("<I", x, off(tab + 16 * d + 4 * r))[0]
                n = struct.unpack_from("<H", x, off(tab + 8 + 16 * d + 2 * r))[0]
                if not p or n > 400 or not (0x423080 <= p < 0x5FB9C8):
                    continue
                for i in range(n):
                    ids.add(struct.unpack_from("<H", x, off(p + 4 * i))[0])
    return ids


def cartas_mascotas(game):
    """2026-11-19 (Carta As, carta_as.c): the cards of the 100 mascots
    (mascota.c k_modelo: 3D model -> card by the game's pair table 0x44FBB0)"""
    import re
    import struct
    x = (game / "default.xbe").read_bytes()
    off = lambda va: va - 0x423080 + 0x413000
    src = (PORT / "ygo-apex" / "src" / "mascota.c").read_text(encoding="utf-8")
    blk = src[src.index("k_modelo[NMODELOS] = {"):src.index("static volatile LONG s_tuya")]
    modelos = {int(m) for m in re.findall(r"(\d+),\s*/\*", blk)}
    ids = set()
    for i in range(2000):
        a, m = struct.unpack_from("<HH", x, off(0x44FBB0 + 4 * i))
        if a < 4000 or a > 5300:
            break
        if m in modelos:
            ids.add(a)
    return ids


def main():
    game, mod, out = (pathlib.Path(a) for a in sys.argv[1:4])
    hechas = faltan = 0
    rivales = cartas_de_rivales(game) | cartas_mascotas(game)
    for d, xx in IDIOMAS.items():
        base = game / "euro" / d / "card" / "normal"
        dirs = [TEX / "idiomas" / xx] + ([TEX / "mejoradas" / "texto_cartas"] if xx == "es" else [])
        (out / d).mkdir(parents=True, exist_ok=True)
        (out / d / "g").mkdir(parents=True, exist_ok=True)
        # the newest x4 of each folder: an output newer than it and than its
        # .tga is up to date without hashing (the copy script runs this often)
        nuevo = {p: max((f.stat().st_mtime for f in p.glob("*.png")), default=0) for p in dirs + [MOD_HD]}
        for tga in sorted(base.rglob("*.tga")):
            rel = tga.relative_to(base)
            mt = mod / "euro" / d / "card" / "normal" / rel
            src_tga, hd_dirs = (mt, [MOD_HD]) if mt.is_file() else (tga, dirs)
            dst = out / d / (tga.stem + ".jpg")
            dg = out / d / "g" / (tga.stem + ".jpg")
            quiere_g = tga.stem.isdigit() and int(tga.stem) in rivales
            al_dia = max([src_tga.stat().st_mtime] + [nuevo[p] for p in hd_dirs])
            if dst.exists() and dst.stat().st_mtime >= al_dia and (not quiere_g or (dg.exists() and dg.stat().st_mtime >= al_dia)):
                continue
            h = "%016X_128x256.png" % cv.huella(Image.open(src_tga))
            hd = next((p / h for p in hd_dirs if (p / h).is_file()), None)
            if not hd:
                faltan += 1
                if dst.exists():
                    dst.unlink()                # the panel falls back to the .tga
                if dg.exists():
                    dg.unlink()
                continue
            im = Image.open(hd).convert("RGB").transpose(Image.FLIP_TOP_BOTTOM)
            im = im.crop((0, 0, im.width, im.height * 185 // 256))
            im.resize((W, H), Image.LANCZOS).save(dst, quality=85)
            if quiere_g:
                im.resize((GW, GH), Image.LANCZOS).save(dg, quality=88)
            hechas += 1
    print("perfil_cartas: %d hechas, %d sin version mejorada" % (hechas, faltan))


if __name__ == "__main__":
    main()
