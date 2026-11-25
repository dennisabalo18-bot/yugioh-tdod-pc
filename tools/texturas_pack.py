"""Arma modding/texturas/textures.pack (XRTP v2, sin comprimir) con las
imagenes de modding/texturas/mejoradas/ (nombre HASH_ANCHOxALTO como en el
volcado). El juego lo carga al arrancar (RECOMP_TEXTURES_PACK en env.bat) y
F6 cambia en vivo entre las originales y estas.

Adaptado de rdr-port/tools/texturas_pack.py, solo la parte general (sin las
letras ni los botones de Red Dead):
- la transparencia sale del original volcado (dump/): los escaladores la
  pierden o la ensucian; un recorte (alfa 0/255) queda nitido, un alfa suave
  se suaviza, y donde el texel es transparente queda el color del original;
- cada entrada lleva toda su cadena de mipmaps (LANCZOS), en A8R8G8B8 (BGRA
  en el archivo);
- el tamano es un multiplo entero exacto del original.

    py -3 tools/texturas_pack.py [carpeta_mejoradas] [salida]
"""
import pathlib
import re
import struct
import sys

import numpy as np
from PIL import Image, ImageFilter

ROOT = pathlib.Path(__file__).resolve().parent.parent / "modding" / "texturas"
NAME = re.compile(r"^([0-9A-Fa-f]{16})_(\d+)x(\d+)")
MAX_DIM = 16384
SMALL = 32


def mip_chain(img):
    levels = [img]
    w, h = img.size
    while w > 1 or h > 1:
        w, h = max(1, w // 2), max(1, h // 2)
        # BOX, not LANCZOS: LANCZOS rings at an alpha edge (a light band
        # inside the cut, 237 -> 254 on the dialog balloon's mip 3: the
        # light line down its middle, 2026-11-09 screenshot)
        levels.append(levels[-1].resize((w, h), Image.BOX))
    return levels


def level_sizes(w, h):
    out = [(w, h)]
    while w > 1 or h > 1:
        w, h = max(1, w // 2), max(1, h // 2)
        out.append((w, h))
    return out


def restore(img, orig):
    a = orig.getchannel("A")
    cutout = sum(a.histogram()[1:255]) == 0
    soft = np.asarray(a.resize(img.size, Image.BICUBIC), np.float32)[..., None] / 255.0
    base = np.asarray(orig.convert("RGB").resize(img.size, Image.BICUBIC), np.float32)
    up = np.asarray(img.convert("RGB"), np.float32)
    rgb = np.clip(up * soft + base * (1.0 - soft), 0, 255).astype(np.uint8)
    out = Image.fromarray(rgb, "RGB").convert("RGBA")
    a = a.resize(img.size, Image.BICUBIC)
    out.putalpha(a.point(lambda v: 255 if v >= 128 else 0) if cutout else a)
    return out


# Edges where two pieces of one drawing meet on screen: (hash, original
# column where the opaque part ends, exclusive). The dialog balloon is two
# mirrored halves of 0E1804B57B783074 (u 0..0.8916 = column 913) that touch
# at x 320; the upscale brightens its last columns and restore() blends the
# transparent column 913 in, a dark seam down the middle (2026-11-09
# screenshot). Rows opaque up to the edge get their last texel's upscaled
# columns from the colour just inside, fully opaque, and nothing after.
# CAE5437E911B62D9: the plain balloon's half (edge 473) and the shout
# balloon's (690), straight edges the same way. The two columns past the
# edge (transparent in the original, no geometry reaches them) take the
# inside colour too, so a filtered sample at the edge reads no transparency.
SEAMS = {0x0E1804B57B783074: (913,), 0xCAE5437E911B62D9: (473, 690)}


def fix_seams(img, orig, cols, k):
    o = orig.load()
    px = img.load()
    ow, oh = orig.size
    for c in cols:
        for y in range(oh):
            if o[c - 1, y][3] < 255 or (c < ow and o[c, y][3] != 0):
                continue
            for yy in range(y * k, (y + 1) * k):
                inside = px[(c - 1) * k - 13, yy]
                for x in range((c - 1) * k - 12, c * k):
                    px[x, yy] = inside[:3] + (255,)
                for cc in range(c, min(c + 2, ow)):
                    if o[cc, y][3] != 0:
                        break
                    for x in range(cc * k, (cc + 1) * k):
                        px[x, yy] = inside[:3] + (255,)
    return img


# Pieces of an atlas the AI must not redraw: (hash, [(x0, y0, x1, y1) in the
# dump's pixels, end exclusive]). Each goes back as the original enlarged
# without AI (bicubic). 2026-11-10 (captures of the title with and without
# F6, Ctrl+F9 dump): the title's sparks are menu_06_00, a 16x16 black square
# with a 5-texel cross (255 centre, 71 arms) added over the picture (batch of
# 899 vertices on F937F533CB06E816, title texdb000); the AI took the cross
# for noise and blurred it out. The main menu's atlas (603BA796E198985F)
# holds the same dot twice (menu_06_00, menu_06_01).
# Un quinto valor: el borde se funde con la escalada en tantos px del original
# (para una zona en medio del dibujo, que no se note el recuadro).
#  La cara del Mago Oscuro (48E137E53FCE3B33) ya no va acá: sin IA se veía baja
#  de resolución; sale de otro modelo de IA (tools/zonas_otro_modelo.py).
KEEP_PIECES = {
    0xF937F533CB06E816: [(974, 441, 990, 457)],
    0x603BA796E198985F: [(1002, 445, 1018, 461), (1002, 427, 1018, 443)],
}


def keep_pieces(img, orig, boxes, k):
    for b in boxes:
        x0, y0, x1, y1 = b[:4]
        w, h = (x1 - x0) * k, (y1 - y0) * k
        piece = orig.crop((x0, y0, x1, y1)).resize((w, h), Image.LANCZOS)
        if len(b) > 4:
            # sin IA pero nítido, y el borde fundido con lo escalado
            piece = piece.filter(ImageFilter.UnsharpMask(radius=2, percent=80, threshold=2))
            f = b[4] * k
            yy = np.minimum(np.arange(h), np.arange(h)[::-1])[:, None]
            xx = np.minimum(np.arange(w), np.arange(w)[::-1])[None, :]
            m = np.clip(np.minimum(yy, xx) / f, 0, 1)
            m = m * m * (3 - 2 * m)
            mask = Image.fromarray((m * 255).astype(np.uint8), "L")
            img.paste(piece, (x0 * k, y0 * k), mask)
        else:
            img.paste(orig.crop((x0, y0, x1, y1)).resize((w, h), Image.BICUBIC), (x0 * k, y0 * k))
    return img


def main():
    src = pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "mejoradas"
    out = pathlib.Path(sys.argv[2]) if len(sys.argv) > 2 else ROOT / "textures.pack"
    if not src.is_dir():
        print(f"No existe la carpeta {src}")
        return 1
    dump = ROOT / "dump"
    originals = {p.name[:16].upper(): p for p in dump.glob("*.png")} if dump.is_dir() else {}
    entries = {}
    # tools/revisar_texturas.py: escaladas que perdieron contenido (ruido,
    # follaje, tramas de puntos): queda el original
    f = ROOT / "revision" / "fuera_ia.txt"
    fuera = {l.strip().upper() for l in f.read_text(encoding="utf-8").splitlines()
             if l.strip()} if f.is_file() else set()
    # texto_cartas/ (tools/texto_cartas.py: the card with its description
    # rewritten sharp) wins over the plain upscale of the same hash.
    # hechas a mano (texto_cartas, reverso, miniaturas, fondo, caras) ganan a la escalada simple
    # (caras: tools/costuras_caras.py, los retratos sin costura en ojos y boca)
    for p in sorted(src.rglob("*"), key=lambda q: (q.parent.name not in ("texto_cartas", "reverso", "miniaturas", "fondo", "letras", "caras"), str(q))):
        if not p.is_file() or p.suffix.lower() not in (".png", ".jpg", ".jpeg", ".webp", ".bmp"):
            continue
        m = NAME.match(p.stem)
        if not m:
            print(f"  ignorada (el nombre no empieza por HASH_ANCHOxALTO): {p.name}")
            continue
        h, gw, gh = int(m.group(1), 16), int(m.group(2)), int(m.group(3))
        if max(gw, gh) <= SMALL or h in entries:
            continue
        if f"{h:016X}" in fuera and p.parent.name.startswith("upscayl"):
            continue
        try:
            with Image.open(p) as img:
                iw = img.size[0]
        except OSError:
            print(f"  ignorada (imagen rota o a medio escribir): {p.name}")
            continue
        # 2026-11-08: the AI took the menu's sparks (553E66, 36 dots on
        # black, brightness 128) for noise and gave back a black square: the
        # sparks vanished with F6. An upscale that lost its original's
        # brightness stays out (the original is used).
        o = originals.get(f"{h:016X}")
        if o:
            ob = max(Image.open(o).convert("L").getextrema()[1], 1)
            ub = Image.open(p).convert("L").getextrema()[1]
            if ob >= 32 and ub < ob * 0.5:
                print(f"  fuera (la IA borro su contenido, brillo {ob} -> {ub}): {p.name}")
                continue
        k = max(1, round(iw / gw))
        while k > 1 and max(gw, gh) * k > MAX_DIM:
            k -= 1
        entries[h] = (p, gw * k, gh * k, k)
    if not entries:
        print("No hay texturas mejoradas: primero escalar_texturas.bat")
        return 1
    hashes = sorted(entries)
    index_off = 32
    off = index_off + 32 * len(hashes)
    index = bytearray()
    for h in hashes:
        p, w, hh, _ = entries[h]
        sizes = level_sizes(w, hh)
        index += struct.pack("<QIIIIQ", h, w, hh, len(sizes), 0, off)
        off += sum(a * b * 4 for a, b in sizes)
    scale = max(e[3] for e in entries.values())
    out.parent.mkdir(parents=True, exist_ok=True)
    tmp = out.with_suffix(".tmp")
    with open(tmp, "wb") as f:
        f.write(struct.pack("<4sIIIQII", b"XRTP", 2, len(hashes), scale, index_off, 0, 0))
        f.write(index)
        for n, h in enumerate(hashes, 1):
            p, w, hh, _ = entries[h]
            img = Image.open(p).convert("RGBA")
            if img.size != (w, hh):
                img = img.resize((w, hh), Image.LANCZOS)
            o = originals.get(f"{h:016X}")
            # miniaturas/ (tools/miniaturas.py) va opaca: el color 0 de la hoja
            # original deja agujeros en el dibujo de las cartas
            if o and p.parent.name not in ("miniaturas", "letras"):
                img = restore(img, Image.open(o).convert("RGBA"))
            if o and h in SEAMS:
                img = fix_seams(img, Image.open(o).convert("RGBA"), SEAMS[h], entries[h][3])
            if o and h in KEEP_PIECES:
                img = keep_pieces(img, Image.open(o).convert("RGBA"), KEEP_PIECES[h], entries[h][3])
            for lv in mip_chain(img):
                f.write(lv.tobytes("raw", "BGRA"))
            if n % 50 == 0:
                print(f"  {n}/{len(hashes)}")
    tmp.replace(out)
    print(f"{len(hashes)} texturas en {out} ({off / (1024 * 1024):.1f} MB, escala hasta x{scale})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
