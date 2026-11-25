"""Replaces a few textures inside an XRTP pack in place (2026-11-20, Umbra's
mouth: rebuilding the whole pack takes a 20 GB raw pack and hours).

The given mejoradas PNGs go through the same steps as a full build
(tools/texturas_pack.py: alpha from the dump, mip chain; tools/bc3/bc3pack.exe:
BC3) into a small pack; each entry's bytes then overwrite the same hash's in
the big pack, which must have the same size, levels and flags. The old bytes
are kept in respaldos/ first (--deshacer puts them back). Written in place:
a hard link to the pack (the player's Mods-Extras copy) gets it too.

    py -3 tools/parchar_pack.py PACK PNG [PNG ...]
    py -3 tools/parchar_pack.py PACK --deshacer RESPALDO_DIR
"""
import pathlib, shutil, struct, subprocess, sys, tempfile

ROOT = pathlib.Path(__file__).resolve().parent.parent
BC3 = ROOT / "tools" / "bc3" / "bc3pack.exe"
PACKPY = ROOT / "tools" / "texturas_pack.py"


def leer_indice(f):
    f.seek(0)
    hdr = f.read(32)
    assert hdr[:4] == b"XRTP", "no es un pack"
    count = struct.unpack_from("<I", hdr, 8)[0]
    index_off = struct.unpack_from("<Q", hdr, 16)[0]
    f.seek(index_off)
    raw = f.read(32 * count)
    out = {}
    for i in range(count):
        h, w, hh, lv, fl, off = struct.unpack_from("<QIIIIQ", raw, 32 * i)
        out[h] = (w, hh, lv, fl, off)
    return out


def bytes_de(w, h, lv, fl):
    n = 0
    for _ in range(lv):
        b = ((w + 3) // 4) * ((h + 3) // 4)
        n += b * 8 if fl & 0x20 else b * 16 if fl & 0x10 else w * h * 4   # 0x20 BC1 (achicar_pack.py), 0x10 BC3
        w, h = max(1, w // 2), max(1, h // 2)
    return n


def main():
    pack = pathlib.Path(sys.argv[1])
    if sys.argv[2] == "--deshacer":
        resp = pathlib.Path(sys.argv[3])
        with open(pack, "r+b") as f:
            idx = leer_indice(f)
            for b in sorted(resp.glob("*.bin")):
                h = int(b.stem, 16)
                if len(b.read_bytes()) != bytes_de(*idx[h][:4]):
                    # a backup from before tools/achicar_pack.py (BC3; the pack's is BC1)
                    print("otro tamaño (el pack fue achicado), sin tocar:", b.stem)
                    continue
                f.seek(idx[h][4])
                f.write(b.read_bytes())
                print("devuelta", b.stem)
        return 0
    pngs = [pathlib.Path(p) for p in sys.argv[2:]]
    with tempfile.TemporaryDirectory() as td:
        td = pathlib.Path(td)
        (td / "caras").mkdir()
        for p in pngs:
            shutil.copy2(p, td / "caras" / p.name)
        subprocess.run([sys.executable, "-I", str(PACKPY), str(td), str(td / "mini.pack")], check=True)
        subprocess.run([str(BC3), str(td / "mini.pack"), str(td / "mini_bc3.pack")], check=True)
        with open(td / "mini_bc3.pack", "rb") as m:
            nuevo = leer_indice(m)
            datos = {}
            for h, (w, hh, lv, fl, off) in nuevo.items():
                m.seek(off)
                datos[h] = m.read(bytes_de(w, hh, lv, fl))
    resp = ROOT / "respaldos" / ("pack_" + pack.stem)
    resp.mkdir(parents=True, exist_ok=True)
    with open(pack, "r+b") as f:
        idx = leer_indice(f)
        offs = {}
        for h, e in idx.items():
            offs.setdefault(e[4], []).append(h)
        for h, d in datos.items():
            if h not in idx:
                print(f"{h:016X}: no está en {pack.name}, sin tocar")
                continue
            w, hh, lv, fl, off = idx[h]
            nw, nh, nlv, nfl, _ = nuevo[h]
            if fl & 0x20 and not nfl & 0x20:
                # 2026-11-21: an entry halved to BC1 (tools/achicar_pack.py):
                # the new picture too, if it has no transparency
                import numpy as np
                sys.path.insert(0, str(ROOT / "tools"))
                import achicar_pack as ach
                blq = np.frombuffer(d, dtype=np.uint8).reshape(-1, 16)
                assert ach.opaco(blq), f"{h:016X}: la nueva tiene transparencia y la del pack es BC1 (opaca)"
                d = ach.a_bc1(blq).tobytes()
                nfl |= 0x20
            assert (w, hh, lv, fl) == (nw, nh, nlv, nfl), (f"{h:016X}", idx[h], nuevo[h])
            assert len(d) == bytes_de(w, hh, lv, fl)
            assert offs[off] == [h], f"{h:016X} comparte sus texeles con {offs[off]}"
            b = resp / f"{h:016X}.bin"
            if not b.exists():
                f.seek(off)
                b.write_bytes(f.read(len(d)))
            f.seek(off)
            f.write(d)
            print(f"{h:016X}: {len(d)} bytes en {off} ({w}x{hh}, {lv} niveles)")
    print("respaldo de lo viejo en", resp)
    return 0


if __name__ == "__main__":
    sys.exit(main())
