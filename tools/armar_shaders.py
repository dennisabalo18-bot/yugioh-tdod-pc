"""Arma shaders.pak (junto al exe) con los shaders que pidió el Yu-Gi-Oh.

El kit (nv2a_pb_d3d11.c, shader_pak_thread) lee shaders.pak junto al exe y,
al arrancar, compila en segundo plano los que falten en el cache de esa PC:
la primera vez que aparece un efecto el cuadro no espera a compilarlo
(registro 2026-11-11: 8 a 270 ms por shader, tirones de 0,2-0,3 s).

El cache (%LOCALAPPDATA%/xboxrecomp/ShaderCache/dxbc) lo comparte con Red
Dead: se dejan afuera los que ya están en el shaders.pak de Red Dead.
Formato igual que C:/rdr-port/tools/armar_shaders.py.

    py -3 tools/armar_shaders.py
"""
import glob
import os
import pathlib
import struct
import sys

PORT = pathlib.Path(__file__).resolve().parent.parent
CACHE = pathlib.Path(os.environ["LOCALAPPDATA"]) / "xboxrecomp" / "ShaderCache" / "dxbc"
RDR = pathlib.Path(r"C:\rdr-port\assets\shaders.pak")
OUT = PORT / "shaders.pak"


def leer_pak(path):
    out = set()
    if not path.is_file():
        return out
    b = path.read_bytes()
    if b[:8] != b"RDRSHPK1":
        return out
    n, p = struct.unpack_from("<I", b, 8)[0], 12
    for _ in range(n):
        v = []
        for _ in range(2):
            k = struct.unpack_from("<I", b, p)[0]; p += 4
            v.append(b[p:p + k]); p += k
        f1, f2, k = struct.unpack_from("<III", b, p); p += 12
        v += [f1, f2, b[p:p + k]]; p += k
        out.add(tuple(v))
    return out


def main():
    rdr = leer_pak(RDR)
    entries = {}
    for f in sorted(glob.glob(str(CACHE / "*.bin"))):
        b = open(f, "rb").read()
        if b[:4] != b"XRSC":
            continue
        p = 32
        def field():
            nonlocal p
            n = struct.unpack_from("<I", b, p)[0]
            p += 4
            v = b[p:p + n]
            p += n
            return v
        field()                         # compilador: el de esta PC, no va
        tgt, ent = field(), field()
        f1, f2 = struct.unpack_from("<II", b, p)
        p += 8
        src = field()
        if tgt and src and (tgt, ent, f1, f2, src) not in rdr:
            entries[(tgt, ent, f1, f2, src)] = None
    data = bytearray(b"RDRSHPK1" + struct.pack("<I", len(entries)))
    for tgt, ent, f1, f2, src in entries:
        for s in (tgt, ent):
            data += struct.pack("<I", len(s)) + s
        data += struct.pack("<III", f1, f2, len(src)) + src
    OUT.write_bytes(data)
    print(f"{len(entries)} shaders ({len(rdr)} de Red Dead fuera) -> {OUT} ({len(data) // 1024} KB)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
