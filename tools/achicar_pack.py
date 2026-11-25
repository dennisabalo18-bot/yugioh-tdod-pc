"""Halves the opaque textures of an XRTP pack (2026-11-21, the user: "podemos
comprimir algo para que no sea tanto el peso sin perder calidad?").

A BC3 (DXT5) block is 8 bytes of alpha + 8 bytes of colour. A texture whose
every alpha block says 255 (both endpoints 255 and no index 6, which is 0 in
that mode) keeps only its colour blocks as BC1 (DXT1, 8 bytes, flag 0x20,
XRTP_FLAG_BC1 in the kit): the same colours, half the bytes, on disk, in RAM
and in video memory. BC3 decodes its colour block always with 4 colours; BC1
does so only when c0 > c1, so a block with c0 < c1 gets its endpoints
swapped and every index xor 1 (the same colours), and one with c0 == c1
(one colour) gets index 0 everywhere.

    py -3 tools/achicar_pack.py PACK [PACK ...]

Each pack is written beside itself (.nuevo), checked (every converted block
decoded both ways must give the same pixels), the old one moved to
respaldos/packs_bc3/ and the new one put in its place; the player's copy of
a pack that is a hard link of it (Mods-Extras) is linked to the new one.
Already halved entries (flag 0x20) are kept as they are.
"""
import os, pathlib, struct, subprocess, sys
import numpy as np

ROOT = pathlib.Path(__file__).resolve().parent.parent
RESP = ROOT / "respaldos" / "packs_bc3"
F_BC3, F_BC1 = 0x10, 0x20


def niveles_bytes(w, h, lv, fl):
    n = 0
    for _ in range(lv):
        b = ((w + 3) // 4) * ((h + 3) // 4)
        n += b * 8 if fl & F_BC1 else b * 16 if fl & F_BC3 else w * h * 4
        w, h = max(1, w // 2), max(1, h // 2)
    return n


def opaco(blq):
    """blq: (n,16) uint8 BC3 blocks; True when every pixel's alpha is 255"""
    if not (np.all(blq[:, 0] == 255) and np.all(blq[:, 1] == 255)):
        return False
    a = blq[:, 2:8].astype(np.uint64)
    bits = np.zeros(len(blq), dtype=np.uint64)
    for i in range(6):
        bits |= a[:, i] << np.uint64(8 * i)
    for k in range(16):
        if np.any(((bits >> np.uint64(3 * k)) & np.uint64(7)) == 6):
            return False
    return True


def a_bc1(blq):
    """the colour halves of opaque BC3 blocks as BC1 blocks, same colours"""
    c = blq[:, 8:16].copy()
    c0 = c[:, 0].astype(np.uint16) | (c[:, 1].astype(np.uint16) << 8)
    c1 = c[:, 2].astype(np.uint16) | (c[:, 3].astype(np.uint16) << 8)
    idx = c[:, 4:8].view(np.uint32).reshape(-1).copy()
    menor = c0 < c1
    igual = c0 == c1
    n0 = np.where(menor, c1, c0)
    n1 = np.where(menor, c0, c1)
    idx = np.where(menor, idx ^ np.uint32(0x55555555), idx)
    idx = np.where(igual, np.uint32(0), idx)
    out = np.empty((len(blq), 8), dtype=np.uint8)
    out[:, 0] = n0 & 0xFF; out[:, 1] = n0 >> 8
    out[:, 2] = n1 & 0xFF; out[:, 3] = n1 >> 8
    out[:, 4:8] = idx.astype('<u4').view(np.uint8).reshape(-1, 4)
    return out


def _paleta(c0, c1, cuatro):
    def rgb(c):
        r = (c >> 11) & 31; g = (c >> 5) & 63; b = c & 31
        return np.stack([(r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2)], -1).astype(np.int32)
    p0, p1 = rgb(c0.astype(np.int32)), rgb(c1.astype(np.int32))
    p2 = np.where(cuatro[:, None], (2 * p0 + p1) // 3, (p0 + p1) // 2)
    p3 = np.where(cuatro[:, None], (p0 + 2 * p1) // 3, 0)
    return np.stack([p0, p1, p2, p3], 1)        # (n,4,3)


def colores(c8, bc1):
    """(n,8) colour blocks -> (n,16,3) pixels, BC3 rule or BC1 rule"""
    c0 = c8[:, 0].astype(np.int32) | (c8[:, 1].astype(np.int32) << 8)
    c1 = c8[:, 2].astype(np.int32) | (c8[:, 3].astype(np.int32) << 8)
    cuatro = (c0 > c1) if bc1 else np.ones(len(c8), bool)
    pal = _paleta(c0, c1, cuatro)
    idx = c8[:, 4:8].copy().view(np.uint32).reshape(-1)
    sel = np.stack([(idx >> (2 * k)) & 3 for k in range(16)], 1).astype(np.int64)
    return np.take_along_axis(pal, sel[:, :, None].repeat(3, 2), 1), sel, cuatro


def comprobar(blq, nuevo):
    a, _, _ = colores(blq[:, 8:16], False)
    b, sel, cuatro = colores(nuevo, True)
    if np.any(~cuatro & np.any(sel == 3, 1)):
        return False                             # a transparent pixel
    return np.array_equal(a, b)


def achicar(path):
    path = pathlib.Path(path)
    nuevo = path.with_name(path.name + ".nuevo")
    with open(path, "rb") as f:
        hdr = bytearray(f.read(32))
        assert hdr[:4] == b"XRTP" and struct.unpack_from("<I", hdr, 4)[0] == 2, f"{path}: no es un pack v2"
        cnt = struct.unpack_from("<I", hdr, 8)[0]
        f.seek(struct.unpack_from("<Q", hdr, 16)[0])
        raw = f.read(32 * cnt)
        ents = [list(struct.unpack_from("<QIIIIQ", raw, 32 * i)) for i in range(cnt)]
        antes = os.path.getsize(path)
        hechas = 0
        with open(nuevo, "wb") as o:
            o.write(hdr)
            for k, e in enumerate(sorted(range(cnt), key=lambda i: ents[i][5])):
                h, w, hh, lv, fl, off = ents[e]
                f.seek(off)
                data = f.read(niveles_bytes(w, hh, lv, fl))
                if fl & F_BC3 and not fl & F_BC1:
                    blq = np.frombuffer(data, dtype=np.uint8).reshape(-1, 16)
                    if opaco(blq):
                        b1 = a_bc1(blq)
                        assert comprobar(blq, b1), f"{h:016X}: BC1 distinto"
                        data = b1.tobytes()
                        fl |= F_BC1             # BC3 bit kept: "block-compressed"
                        hechas += 1
                pos = o.tell()
                if pos % 16:
                    o.write(b"\0" * (16 - pos % 16)); pos = o.tell()
                ents[e][4], ents[e][5] = fl, pos
                o.write(data)
                if k % 500 == 0:
                    print(f"  {path.name}: {k}/{cnt}", flush=True)
            pos = o.tell()
            if pos % 16:
                o.write(b"\0" * (16 - pos % 16)); pos = o.tell()
            struct.pack_into("<Q", hdr, 16, pos)
            for e in ents:                       # the index stays sorted by hash
                o.write(struct.pack("<QIIIIQ", *e))
            o.seek(0); o.write(hdr)
    despues = os.path.getsize(nuevo)
    print(f"{path.name}: {hechas}/{cnt} a BC1, {antes / 2**20:.0f} -> {despues / 2**20:.0f} MB")
    return nuevo


def achicar_aqui(path):
    """for the tools that build a pack (cartas_mod.py, idiomas_pack.py):
    halved in place, written over the same file so its hard links (the
    player's Mods-Extras copy) get it too; no backup (just built)"""
    import shutil
    path = pathlib.Path(path)
    nuevo = achicar(path)
    with open(nuevo, "rb") as s, open(path, "r+b") as d:
        shutil.copyfileobj(s, d, 16 << 20)
        d.truncate()
    nuevo.unlink()


def enlaces(path):
    """the other hard links of path (fsutil), as Paths"""
    try:
        out = subprocess.run(["fsutil", "hardlink", "list", str(path)], capture_output=True,
                             text=True, check=True).stdout.split("\n")
    except Exception:
        return []
    drive = str(path.resolve())[:2]
    otros = [pathlib.Path(drive + l.strip()) for l in out if l.strip()]
    return [p for p in otros if p.resolve() != path.resolve()]


def main():
    RESP.mkdir(parents=True, exist_ok=True)
    for a in sys.argv[1:]:
        path = pathlib.Path(a).resolve()
        otros = enlaces(path)
        nuevo = achicar(path)
        resp = RESP / (path.parent.name + "_" + path.name)
        if resp.exists():
            resp.unlink()
        os.replace(path, resp)                   # the old bytes stay (and in its other links)
        os.replace(nuevo, path)
        for l in otros:
            os.replace(l, l.with_name(l.name + ".viejo"))
            os.link(path, l)
            os.unlink(l.with_name(l.name + ".viejo"))   # only a link: the bytes are in resp
            print(f"  {l}: enlazado al nuevo")
        print(f"  viejo en {resp}")


if __name__ == "__main__":
    main()
