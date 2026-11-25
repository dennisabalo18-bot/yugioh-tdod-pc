"""Saca las texturas de los modelos 3D (.igb de Alchemy: monstruos, efectos,
menus) y las pone en modding/texturas/dump/ con la huella que les da el port,
para escalarlas sin tener que verlas en el juego (como texturas_disco.py con
las imagenes sueltas).

Formato IGB (cabecera de 12 u32: entradas, meta-objetos, objetos, memoria,
meta-campos con sus tamanos y cantidades, 0xFADA, version 0x80000005):
- meta-campos (12 bytes c/u + nombres), bloque de alineaciones (su tamano
  primero), meta-objetos (24 bytes c/u, despues nombre + campos de 6 bytes);
- entradas del directorio y objetos: (indice del meta-objeto, tamano, campos
  empaquetados); un u32 entre las entradas y los objetos;
- la memoria al final: los bloques de cada igMemoryDirEntry (tamano en +4)
  uno tras otro, sin relleno (algunos archivos traen unos bytes de mas al
  final).
igImage: ancho, alto, bytes por pixel (+0, +4, +8); formato (+36), tamano
(+40), bloque de pixeles (+44, indice de entrada); nombre del archivo al final.

Reglas de la huella (2026-11-09, comparadas con el volcado del juego); un
solo nivel, nada dado vuelta, y el volcado guarda la imagen sin dar vuelta:
- formato 5 (RGB): A8R8G8B8 = B,G,R,0 en orden Morton (swizzle); volcado
  opaco. 195 coinciden pixel por pixel;
- formato 7 (RGBA): B,G,R,A, swizzle; volcado con su alfa. 23 iguales;
- formato 0 (gris, los mapas _s): el byte en swizzle; volcado gris opaco. 214;
- formato 14 (DXT1): los bloques tal cual; 6 (iguales salvo el redondeo del
  decodificador DXT, 8 niveles como mucho);
- formato 65536 (paleta): indices en swizzle; paleta del igClut (+12 su
  bloque) R,G,B,A -> B,G,R,A; huella = indices ^ paleta*31. 24 iguales.
igImage: el igClut en +60 (un ObjectRef es el numero de su entrada en el
directorio, object_refs). Fuera: los fondos de 16 bits (formatos 3/8/9/10, 27
imagenes: ninguna conversion probada coincide y no hay volcado de ellos) y
los niveles de las cadenas de mipmaps (igImageMipMapList, 3 en todo el
juego).

    py -3 tools/texturas_igb.py           (pone las nuevas en dump/)
    py -3 tools/texturas_igb.py --check   (solo compara con el volcado)
"""
import io
import pathlib
import struct
import sys

import numpy as np
from PIL import Image

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from cartas_volcado import tex_hash, swizzle, M, GAME, DUMP  # noqa: E402

FMT = {0: "gris", 5: "RGB", 7: "RGBA", 14: "DXT1", 65536: "paleta"}


def parse(d):
    """(objetos [(clase, bytes)], {entrada: (desplazamiento, tamano)})."""
    eb, ec, mob, moc, ob, oc, mb, mc, mfb, mfc, magic, ver = struct.unpack_from("<12I", d, 0)
    if magic != 0xFADA:
        raise ValueError("no es IGB")
    p = 0x30 + 12 * mfc
    for i in range(mfc):
        p += struct.unpack_from("<I", d, 0x30 + 12 * i)[0]
    p += struct.unpack_from("<I", d, p)[0]
    hdr = [struct.unpack_from("<6i", d, p + 24 * i) for i in range(moc)]
    q = p + 24 * moc
    names = []
    for nl, _ma, _mi, fc, _par, _sc in hdr:
        names.append(d[q:q + nl].decode("latin-1").strip("\0 "))
        q += nl + 6 * fc
    ents = []
    for _ in range(ec):
        t, sz = struct.unpack_from("<II", d, q)
        ents.append((names[t], d[q + 8:q + sz]))
        q += sz
    q += 4
    objs = []
    for _ in range(oc):
        t, sz = struct.unpack_from("<II", d, q)
        objs.append((names[t], d[q + 8:q + sz]))
        q += sz
    mem = {}
    for i, (n, b) in enumerate(ents):
        if n == "igMemoryDirEntry":
            sz = struct.unpack_from("<i", b, 4)[0]
            mem[i] = (q, sz)
            q += sz
    if not 0 <= len(d) - q < 64:
        raise ValueError(f"la memoria no cierra ({q} de {len(d)})")
    return objs, mem


def object_refs(d):
    """{entrada del directorio: indice del objeto}: un ObjectRef es el indice
    de su igObjectDirEntry entre todas las entradas (los objetos van en el
    orden de esas entradas)."""
    eb, ec, mob, moc, ob, oc, mb, mc, mfb, mfc, magic, ver = struct.unpack_from("<12I", d, 0)
    p = 0x30 + 12 * mfc
    for i in range(mfc):
        p += struct.unpack_from("<I", d, 0x30 + 12 * i)[0]
    p += struct.unpack_from("<I", d, p)[0]
    hdr = [struct.unpack_from("<6i", d, p + 24 * i) for i in range(moc)]
    q = p + 24 * moc
    names = []
    for nl, _ma, _mi, fc, _par, _sc in hdr:
        names.append(d[q:q + nl].decode("latin-1").strip("\0 "))
        q += nl + 6 * fc
    out = {}
    for i in range(ec):
        t, sz = struct.unpack_from("<II", d, q)
        if names[t] == "igObjectDirEntry":
            out[i] = len(out)
        q += sz
    return out


def _hash(a):
    return tex_hash(np.ascontiguousarray(a).tobytes())


def _dxt1(raw, w, h):
    """RGBA de un DXT1 (con el lector DDS de PIL)."""
    hdr = struct.pack("<4sIIIIIII44xII4s20xI16x", b"DDS ", 124, 0x1007, h, w,
                      w * h // 2, 0, 1, 32, 4, b"DXT1", 0x1000)
    return np.asarray(Image.open(io.BytesIO(hdr + bytes(raw))).convert("RGBA"))


def images(d):
    """(nombre, ancho, alto, formato, huella, RGBA HxWx4 como en el volcado) de
    cada igImage de los formatos comprobados."""
    objs, mem = parse(d)
    ref = object_refs(d)
    # los niveles de una cadena de mipmaps (igImageMipMapList) van juntos en la
    # GPU: sueltos su huella no es la del juego (no comprobado), quedan fuera
    in_chain = set()
    for n, b in objs:
        if n == "igImageMipMapList" and len(b) >= 12:
            c, _cap, mr = struct.unpack_from("<iii", b, 0)
            if c > 1 and mr in mem:
                in_chain.update(struct.unpack_from("<%di" % c, d, mem[mr][0]))
    inv = {v: k for k, v in ref.items()}
    for i, (n, b) in enumerate(objs):
        if n != "igImage" or len(b) < 64 or inv.get(i) in in_chain:
            continue
        w, h, _bpp = struct.unpack_from("<3I", b, 0)
        fmt, _size, mref = struct.unpack_from("<IiI", b, 36)
        clut = struct.unpack_from("<i", b, 60)[0]
        if mref not in mem:
            continue
        off, sz = mem[mref]
        raw = np.frombuffer(d, np.uint8, sz, off)
        if fmt in (5, 7) and sz == w * h * (3 if fmt == 5 else 4):
            a = raw.reshape(h, w, -1)
            alpha = np.zeros((h, w), np.uint8) if fmt == 5 else a[..., 3]
            bgra = np.ascontiguousarray(np.dstack([a[..., 2], a[..., 1], a[..., 0], alpha]))
            key = _hash(swizzle(bgra.view(np.uint32).reshape(h, w)))
            rgba = np.dstack([a[..., :3], np.full((h, w), 255, np.uint8) if fmt == 5 else alpha])
        elif fmt == 0 and sz == w * h:
            g = raw.reshape(h, w)
            key = _hash(swizzle(g))
            rgba = np.dstack([g, g, g, np.full((h, w), 255, np.uint8)])
        elif fmt == 14 and sz == w * h // 2 and w >= 4 and h >= 4:
            key = _hash(raw)
            rgba = _dxt1(raw, w, h)
        elif fmt == 65536 and sz == w * h and clut in ref and objs[ref[clut]][0] == "igClut":
            cb = objs[ref[clut]][1]
            _cf, _cnt, _st, cm, _cs = struct.unpack_from("<5i", cb, 0)
            if cm not in mem:
                continue
            pal = np.frombuffer(d, np.uint8, mem[cm][1], mem[cm][0]).reshape(-1, 4)
            pal = np.vstack([pal, np.zeros((max(0, 256 - len(pal)), 4), np.uint8)])[:256]
            idx = raw.reshape(h, w)
            key = _hash(swizzle(idx)) ^ ((_hash(pal[:, [2, 1, 0, 3]]) * 31) & M)
            rgba = pal[idx]
        else:
            continue
        name = b[84:].split(b"\0")[0].decode("latin-1", "replace") if len(b) > 84 else ""
        yield name, w, h, fmt, key, rgba


def pow2(n):
    return n >= 8 and n & (n - 1) == 0


def main():
    check = "--check" in sys.argv
    dumped = {int(p.name[:16], 16) for p in DUMP.glob("*.png") if p.name[16:17] == "_"}
    files = []
    for p in sorted(GAME.rglob("*.igb")):
        r = p.relative_to(GAME).as_posix()
        if r.startswith("euro/") and not r.startswith("euro/sp/"):
            continue
        files.append(p)
    total = found = new = bad = 0
    per = {}
    done = set()
    for p in files:
        try:
            imgs = list(images(p.read_bytes()))
        except (ValueError, struct.error, IndexError) as ex:
            print(f"  no se pudo leer {p.relative_to(GAME)}: {ex}")
            bad += 1
            continue
        for _name, w, h, fmt, key, rgba in imgs:
            if not (pow2(w) and pow2(h)):
                continue
            if key in done:
                continue
            done.add(key)
            total += 1
            per[fmt] = per.get(fmt, 0) + 1
            if key in dumped:
                found += 1
                continue
            if check:
                continue
            Image.fromarray(np.ascontiguousarray(rgba), "RGBA").save(DUMP / f"{key:016X}_{w}x{h}.png")
            dumped.add(key)
            new += 1
    print(f"{len(files)} archivos igb ({bad} ilegibles), {total} texturas distintas:"
          f" {found} ya estaban en el volcado, {new} nuevas en dump/")
    print("  por formato:", ", ".join(f"{FMT.get(k, k)} {v}" for k, v in sorted(per.items())))
    return 0


if __name__ == "__main__":
    sys.exit(main())
