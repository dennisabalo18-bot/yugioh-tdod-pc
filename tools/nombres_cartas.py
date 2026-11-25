"""Nombre (lang 5 = espanol, 1 ingles) de cartas por ID de imagen, leido del default.xbe.

    py -3 tools/nombres_cartas.py 4966 4989 ...
"""
import pathlib
import struct
import sys

XBE = pathlib.Path(__file__).resolve().parents[2] / "Yu-Gi-Oh TDOD" / "default.xbe"
SECC = [(0x11000, 0x1000), (0x423080, 0x413000), (0x5FB9E0, 0x5EC000)]   # .text .rdata .data
d = XBE.read_bytes()


def off(va):
    base = max(s for s in SECC if s[0] <= va)
    return va - base[0] + base[1]


def u16(va): return struct.unpack_from("<H", d, off(va))[0]
def u32(va): return struct.unpack_from("<I", d, off(va))[0]


def texto(idx, lang):
    tab, base = u32(0x5FC2DC + 4 * lang), u32(0x5FC2C4 + 4 * lang)
    a = off(base + u32(tab + 4 + 4 * idx))
    return d[a:d.index(b"\0", a)].decode("cp1252", "replace")


def k_de(cid):
    return next((k for k in range(1, 1139) if u16(0x5FE692 + 2 * k) == cid), None)


def nombre(cid, lang=5):
    k = k_de(cid)
    return None if k is None else texto(849 + k, lang)


if __name__ == "__main__":
    for a in sys.argv[1:]:
        print(a, nombre(int(a, 0)))
