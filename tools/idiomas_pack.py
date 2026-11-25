"""Las texturas de los otros idiomas (ingles, frances, aleman, italiano) en
un pack aparte por idioma: modding/texturas/textures_<xx>.pack. El pack
grande (textures_bc3.pack) queda con las del espanol y no se rehace.

En cada idioma cambian las cartas grandes (otro texto: otra huella) y 8 o 9
hojas de los menus (titulo, menu, duelo, mazo, comunes; tambien la del modo
puntero, sin el guante). Sin la version del idioma el juego las muestra sin
escalar (visto en ingles, 2026-11-12).

- menus: la hoja del idioma ya escalada (tools/texturas_disco.py la vuelca,
  tools/escalar_texturas.py la escala; despues se pasan de mejoradas/ a
  menus_idiomas/ para que no entren en el pack grande) se copia a idiomas/<xx>/;
- cartas: no se escalan de nuevo (el dibujo es el mismo): la carta en
  espanol escalada con el nombre, el tipo, la etiqueta y la descripcion
  escritos de nuevo con los textos del juego en ese idioma
  (texto_cartas.escribir); la original del idioma va a dump/ (la
  transparencia del pack sale de ahi).
- miniaturas: las 10 hojas de card/mini (una para todos los idiomas) armadas
  con las cartas de este idioma (tools/miniaturas.py --idioma=xx);
- el pack: texturas_pack.py + bc3pack sobre idiomas/<xx>/.

El juego carga solo el textures_<xx>.pack del idioma elegido en F2, si esta en
Mods-Extras\\ (main.c, RECOMP_TEXTURES_PACK_MORE: se lee como el grande, de a
una textura; gana sobre el grande).

    py -3 tools/idiomas_pack.py [es en fr de it] [--sin-pack]   (sin idiomas: los 5)
"""
import pathlib
import re
import shutil
import subprocess
import sys

from PIL import Image

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import cartas_volcado as cv  # noqa: E402
import texto_cartas as tc  # noqa: E402
from cartas_mod import ETIQUETAS, SUFIJOS, TIPOS_TXT, texto_juego  # noqa: E402
from texturas_disco import huella as huella_png  # noqa: E402

PORT = cv.PORT
GAME = cv.GAME
TEX = PORT / "modding" / "texturas"
MEJ = TEX / "mejoradas"
DUMP = cv.DUMP
SALIDA = TEX / "idiomas"
# codigo -> (carpeta del juego, tabla de textos: 1 en, 2 de, 3 fr, 4 it)
IDIOMAS = {"en": ("en", 1), "fr": ("fr", 3), "de": ("gr", 2), "it": ("it", 4)}
NOMBRE, DESC = 849, 1988        # textos de las cartas: + k


def escaladas():
    """huella -> la escalada (no las de texto_cartas: esas ya tienen el texto en espanol)"""
    out = {}
    for p in [*MEJ.rglob("*.png"), *(TEX / "menus_idiomas").glob("*.png")]:
        if p.parent.name != "texto_cartas":
            out.setdefault(p.name[:16].upper(), p)
    return out


def menus(code, carpeta, esc, dst):
    sp = {}
    for p in (GAME / "euro" / "sp").rglob("*.png"):
        if "/card/" not in p.as_posix():
            sp[p.relative_to(GAME / "euro" / "sp").as_posix()] = f"{huella_png(Image.open(p)):016X}"
    hechas = faltan = 0
    for base in (GAME / "euro" / carpeta, PORT / "ygo-apex" / "mods_puntero" / "euro" / carpeta):
        for p in sorted(base.rglob("*.png")):
            r = p.relative_to(base).as_posix()
            if r.startswith("card/"):
                continue
            im = Image.open(p)
            h = f"{huella_png(im):016X}"
            if h == sp.get(r):
                continue        # igual que la del espanol: ya esta en el pack grande
            if h not in esc:
                faltan += 1
                print(f"  {code}: sin escalar {r} ({h})")
                continue
            shutil.copy2(esc[h], dst / esc[h].name)
            hechas += 1
    print(f"  {code}: {hechas} hojas de menus" + (f", {faltan} sin escalar" if faltan else ""))


def cartas(code, carpeta, tabla, esc, dst, xbe, datos):
    ids = {cid: k for k, cid in enumerate(
        int.from_bytes(xbe[tc.TABLE + 2 * i:tc.TABLE + 2 * i + 2], "little") for i in range(tc.NAMES)) if k}
    nombres = {cid: texto_juego(xbe, NOMBRE + k, tabla) for cid, k in ids.items()}
    hechas = 0
    sin = []
    for p in sorted((GAME / "euro" / carpeta / "card" / "normal").rglob("*.tga")):
        cid = int(p.stem)
        im = Image.open(p)
        if im.mode != "P" or im.size != (128, 256):
            continue
        sp = GAME / "euro" / "sp" / "card" / "normal" / p.parent.name / p.name
        h = f"{cv.huella(im):016X}"
        if not sp.is_file():
            continue
        h_sp = f"{cv.huella(Image.open(sp)):016X}"
        d = datos.get(cid)
        if h == h_sp or h_sp not in esc or cid not in ids or not d or not d["desc"] or d["desc"] == "Reserve":
            if h != h_sp:
                sin.append(cid)
            continue
        k = ids[cid]
        desc = texto_juego(xbe, DESC + k, tabla).replace("\n", " ")
        desc = re.sub(r"%c(\d{4})", lambda m: nombres.get(int(m.group(1)), m.group(0)), desc)
        c = dict(d)
        c["mon"] = d["tipo"] < 21    # 2026-11-24: de los datos, no de la raya de ATK/DEF (texto_cartas.main)
        c["subtipo"] = tc.subtipo(cid, d)   # 2026-11-24: rayo / infinito en la etiqueta
        c["nombre"] = nombres[cid]
        c["desc"] = " ".join(desc.split())
        c["tipo_txt"] = texto_juego(xbe, TIPOS_TXT + d["tipo"], tabla)
        c["sufijo"] = SUFIJOS[code][d["clase"]]
        c["etiqueta"] = ETIQUETAS.get(tc.etiqueta(d), {}).get(code)
        img, _, _ = tc.escribir(tc.abrir_escalada(esc[h_sp]), im.convert("RGB"), c)
        img.save(dst / f"{h}_128x256.png")
        volcada = DUMP / f"{h}_128x256.png"
        if not volcada.is_file():
            im.convert("RGBA").transpose(Image.FLIP_TOP_BOTTOM).save(volcada)
        hechas += 1
        if hechas % 200 == 0:
            print(f"  {code}: {hechas} cartas...", flush=True)
    print(f"  {code}: {hechas} cartas" + (f"; {len(sin)} sin version en espanol escalada o sin datos" if sin else ""))


def pack(code, dst):
    raw = TEX / f"textures_{code}_raw.pack"
    out = TEX / f"textures_{code}.pack"
    subprocess.run([sys.executable, "-I", str(PORT / "tools" / "texturas_pack.py"), str(dst), str(raw)],
                   check=True)
    subprocess.run([str(PORT / "tools" / "bc3" / "bc3pack.exe"), str(raw), str(out)], check=True)
    raw.unlink()
    sys.path.insert(0, str(PORT / "tools"))
    import achicar_pack                       # 2026-11-21: the opaque ones as BC1, half the size
    achicar_pack.achicar_aqui(out)
    print(f"  {code}: {out} ({out.stat().st_size / 2**20:.0f} MB)")


def espanol():
    """el pack del espanol armado como los otros: las cartas de nuevo
    (texto_cartas.py), sus miniaturas (miniaturas.py, que ganan sobre las del
    pack grande) y lo que sea solo del espanol (separar_espanol.huellas: cartas
    sin datos y hojas de menus) desde su mejor version en mejoradas/"""
    import separar_espanol as se
    subprocess.run([sys.executable, str(PORT / "tools" / "texto_cartas.py")], check=True)
    subprocess.run([sys.executable, str(PORT / "tools" / "miniaturas.py")], check=True)
    dst = SALIDA / "es"
    if dst.exists():
        shutil.rmtree(dst)
    (dst / "miniaturas").mkdir(parents=True)
    solo = {f"{h:016X}" for h in se.huellas("sp") - set().union(*(se.huellas(l) for l in se.OTROS))}
    # la mejor version de cada huella, como la elige texturas_pack.py
    especiales = ("texto_cartas", "reverso", "miniaturas", "fondo", "letras", "caras")
    mejor = {}
    for p in sorted(MEJ.rglob("*.png"), key=lambda q: (q.parent.name not in especiales, str(q))):
        mejor.setdefault(p.name[:16].upper(), p)
    n = 0
    for h in solo:
        if h in mejor:
            shutil.copy2(mejor[h], dst / mejor[h].name)
            n += 1
    for p in (MEJ / "miniaturas").glob("*.png"):
        shutil.copy2(p, dst / "miniaturas" / p.name)
    print(f"  es: {n} texturas solo del espanol (de {len(solo)}) y las miniaturas")
    return dst


def main():
    pedidos = [a for a in sys.argv[1:] if not a.startswith("--")] or ["es", *IDIOMAS]
    xbe = (GAME / "default.xbe").read_bytes()
    datos = tc.textos_bytes(xbe)
    esc = escaladas()
    for code in pedidos:
        if code == "es":
            dst = espanol()
            if "--sin-pack" not in sys.argv:
                pack(code, dst)
            continue
        carpeta, tabla = IDIOMAS[code]
        dst = SALIDA / code
        if dst.exists():
            shutil.rmtree(dst)
        dst.mkdir(parents=True)
        menus(code, carpeta, esc, dst)
        cartas(code, carpeta, tabla, esc, dst, xbe, datos)
        # las hojas de miniaturas con las cartas de este idioma (idiomas/xx/miniaturas/)
        subprocess.run([sys.executable, str(PORT / "tools" / "miniaturas.py"), f"--idioma={code}"], check=True)
        if "--sin-pack" not in sys.argv:
            pack(code, dst)
    return 0


if __name__ == "__main__":
    sys.exit(main())
