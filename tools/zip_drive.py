"""Zips for Google Drive (2026-11-25): each one unpacks straight into the port's
folder (paths start with Mods-Extras/...). Called by tools/armar_paquete.ps1.

    py -3 -I tools/zip_drive.py <Mods-Extras folder> <out folder>

Writes (names with UTF-8, "/" separators, ZIP64 for the big packs):
    YuGiOh-Contenido.zip        Mods-Extras/Contenido/**   (required: the port's own content)
    YuGiOh-Texturas-HD.zip      Mods-Extras/texturas/textures_bc3.pack (+ .usadas)
    YuGiOh-Idioma-XX.zip        Mods-Extras/texturas/textures_xx.pack, one per language;
                                the ES one also carries Mods-Extras/voces/** (Latin American voices)
"""
import os
import sys
import zipfile

LANGS = ["es", "en", "pt", "fr", "de", "it"]


def tree(base, rel):
    out = []
    top = os.path.join(base, rel)
    for d, _dirs, files in os.walk(top):
        for f in sorted(files):
            p = os.path.join(d, f)
            out.append((p, "Mods-Extras/" + os.path.relpath(p, base).replace(os.sep, "/")))
    return out


def write(dst, items):
    if os.path.exists(dst):
        os.remove(dst)
    with zipfile.ZipFile(dst, "w", zipfile.ZIP_DEFLATED, compresslevel=6, allowZip64=True) as z:
        for src, arc in items:
            # the texture packs hardly shrink: stored, so they unpack fast
            kind = zipfile.ZIP_STORED if src.endswith(".pack") else zipfile.ZIP_DEFLATED
            z.write(src, arc, compress_type=kind)
    mb = os.path.getsize(dst) / 1048576
    print("%s: %d archivos, %.1f MB" % (os.path.basename(dst), len(items), mb))


def langs(mods, out, tex, only=""):
    voces = tree(mods, "voces")
    for l in LANGS:
        if only and l != only:
            continue
        n = "textures_%s.pack" % l
        if os.path.exists(os.path.join(tex, n)):
            items = [(os.path.join(tex, n), "Mods-Extras/texturas/" + n)]
            if l == "es":  # 2026-11-25: the voices go with Spanish (they only play in Spanish)
                items += voces
            write(os.path.join(out, "YuGiOh-Idioma-%s.zip" % l.upper()), items)
    old = os.path.join(out, "YuGiOh-Voces-Latino.zip")
    if os.path.exists(old):
        os.remove(old)


def main():
    mods, out = sys.argv[1], sys.argv[2]
    only = sys.argv[3] if len(sys.argv) > 3 else ""  # e.g. "es": rebuild just that language zip
    os.makedirs(out, exist_ok=True)
    tex = os.path.join(mods, "texturas")
    if only:
        return langs(mods, out, tex, only)
    cont = tree(mods, "Contenido")
    if not cont or not any(a.endswith("/cartas.txt") for _, a in cont):
        sys.exit("falta Mods-Extras/Contenido (o su cartas.txt) en " + mods)
    if not any("/Contenido/juego/" in a for _, a in cont):
        sys.exit("falta Mods-Extras/Contenido/juego (los archivos del port que van en la carpeta del juego)")
    write(os.path.join(out, "YuGiOh-Contenido.zip"), cont)
    hd = [(os.path.join(tex, n), "Mods-Extras/texturas/" + n)
          for n in ("textures_bc3.pack", "textures_bc3.pack.usadas") if os.path.exists(os.path.join(tex, n))]
    if hd:
        write(os.path.join(out, "YuGiOh-Texturas-HD.zip"), hd)
    langs(mods, out, tex)


if __name__ == "__main__":
    main()
