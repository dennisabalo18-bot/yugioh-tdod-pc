"""Lumis & Umbra: a mouth for Umbra (2026-11-20, the user: "le sumamos una
boca usando como base las que ya estan en el juego").

The portrait (scripts 19, 46, 73 of the table 0x60F518) animates only Lumis's
mouth; Umbra's face has eyes only. This draws two more pieces per texture,
Umbra's mouth half open and open, from his own clenched teeth: under the
teeth line the jaw drops h units, fading to 0 toward the chin (no seams);
the gap gets a black edge, the dark red inside and a lighter tongue, as
Lumis's own open mouth. The masks (Lumis's white half, Umbra's demon half)
never move (the user): the jaw stops at the mask's edge.

Frames: the dumped texture D (modding/texturas/mejoradas/caras, 4 px a unit)
is the game's texture turned: G = D rotated 270 (the rects' frame, sprite
rect (x0, y0, x1, y1)), T = G transposed (upright: the face is T (0,0)-(512,
408), a piece's T rect is (y0, x0, y1, x1), drawn at the command's (@4, @6)).
The pieces go in T's free corner (x 296..512, y 408..512). voces.c adds the
track that shows them (rects 8, 9; boca_umbra() there).

    python tools/boca_umbra.py            # writes the 3 textures (originals kept in respaldos/)
    python tools/boca_umbra.py --muestra  # only modding/muestras/boca_umbra_*.png
"""
from PIL import Image
import numpy as np
import pathlib, shutil, sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
CARAS = ROOT / "modding" / "texturas" / "mejoradas" / "caras"
DUMP = ROOT / "modding" / "texturas" / "dump"
RESP = ROOT / "respaldos" / "2026-11-20_boca_umbra"
MUESTRAS = ROOT / "modding" / "muestras"

ROJO = np.array((128, 0, 20, 255), np.float32)
LENGUA = np.array((228, 104, 76, 255), np.float32)
NEGRO = np.array((14, 6, 8, 255), np.float32)

# per texture: the piece's box in T (units, same for the textures sharing a
# script's placements), its two places in the free corner, the jaw drops
# (columns, teeth line, chin end, teeth columns, fade ramps, amount scale)
# and the half / open amounts
CAJA_46 = (302, 194, 384, 252)          # scripts 46 and 73 (textures 28 and 101)
CAJA_19 = (304, 178, 376, 232)          # script 19 (texture 98)
TEX = {
    "3C411B615CD07145_512x512.png": dict(caja=CAJA_46, h=(3.5, 7.5), mandibulas=[
        dict(xa=305, xb=340, linea=210, fin=250, dientes=(312, 339.5), rampa=(305, 6, 341, 0.5), k=1.0)]),
    "E9F0B1A8562D59C2_512x512.png": dict(caja=CAJA_46, h=(3.5, 7.5), mandibulas=[
        dict(xa=304, xb=340, linea=208, fin=248, dientes=(311, 339), rampa=(304, 6, 341, 0.5), k=1.0)]),
    "E15FAC8B3DB60129_512x512.png": dict(caja=CAJA_19, h=(2.5, 5.5), mandibulas=[
        dict(xa=307, xb=336, linea=190.5, fin=228, dientes=(313, 335), rampa=(307, 5, 337, 0.5), k=1.0)]),
}
# the free corner: half at (300, 410), open beside it
LIBRE = [(300, 410), (300 + 86, 410)]


def caer(A, B, S, m, h):
    xa, xb, linea, fin, (di, dd) = m["xa"], m["xb"], m["linea"], m["fin"], m["dientes"]
    r0, w0, r1, w1 = m["rampa"]
    for X in range(xa * S, xb * S):
        xu = X / S
        f = m["k"] * min(1.0, max(0.0, (xu - r0) / w0), max(0.0, (r1 - xu) / w1))
        H = h * f * S
        if H <= 0.2:
            continue
        col = A[:, X].copy()
        L0, F = linea * S, fin * S
        kk = H / (F - L0)
        for Y in range(int(L0), int(F)):
            s = (Y - kk * F) / (1 - kk)
            if s < L0:
                if di <= xu <= dd:
                    t = (Y - L0) / max(1.0, H)
                    borde = xu - di < 1.0 or dd - xu < 1.0 or t > 0.9 or t < 0.06
                    B[Y, X] = NEGRO if borde else (LENGUA if t > 0.58 else ROJO)
                else:
                    B[Y, X] = col[int(L0) - 1]
            else:
                s0 = int(s)
                a = s - s0
                B[Y, X] = col[s0] * (1 - a) + col[min(s0 + 1, len(col) - 1)] * a


def piezas(nombre, cfg):
    D = Image.open(CARAS / nombre).convert("RGBA")
    S = D.size[0] // 512
    T = D.transpose(Image.Transpose.ROTATE_270).transpose(Image.Transpose.TRANSPOSE)
    A = np.array(T).astype(np.float32)
    x0, y0, x1, y1 = cfg["caja"]
    out = []
    for h in cfg["h"]:
        B = A.copy()
        for m in cfg["mandibulas"]:
            caer(A, B, S, m, h)
        out.append(B[y0 * S:y1 * S, x0 * S:x1 * S].copy())
    return D, T, A, S, out


def main():
    muestra = "--muestra" in sys.argv
    MUESTRAS.mkdir(parents=True, exist_ok=True)
    for nombre, cfg in TEX.items():
        D, T, A, S, (media, abierta) = piezas(nombre, cfg)
        x0, y0, x1, y1 = cfg["caja"]
        # the sample: the face with each piece laid where the game draws it
        caras = []
        for p in (None, media, abierta):
            B = A.copy()
            if p is not None:
                B[y0 * S:y1 * S, x0 * S:x1 * S] = p
            caras.append(Image.fromarray(np.clip(B, 0, 255).astype(np.uint8)).crop((0, 0, 512 * S, 408 * S)))
        hoja = Image.new("RGBA", (3 * 512, 408), (40, 40, 40, 255))
        for i, c in enumerate(caras):
            hoja.alpha_composite(c.resize((512, 408)), (512 * i, 0))
        hoja.convert("RGB").save(MUESTRAS / ("boca_umbra_" + nombre[:8] + ".png"))
        if muestra:
            continue
        # the pieces into the free corner of T, back to D's frame
        W, H = x1 - x0, y1 - y0
        assert LIBRE[1][0] + W <= 512 and LIBRE[0][1] + H <= 512, (W, H)
        for (fx, fy), p in zip(LIBRE, (media, abierta)):
            assert not np.array(T)[fy * S:(fy + H) * S, fx * S:(fx + W) * S, 3].any(), "corner not free"
            A[fy * S:(fy + H) * S, fx * S:(fx + W) * S] = p
        Tn = Image.fromarray(np.clip(A, 0, 255).astype(np.uint8), "RGBA")
        Dn = Tn.transpose(Image.Transpose.TRANSPOSE).transpose(Image.Transpose.ROTATE_90)
        RESP.mkdir(parents=True, exist_ok=True)
        if not (RESP / nombre).exists():
            shutil.copy2(CARAS / nombre, RESP / nombre)
        Dn.save(CARAS / nombre)
        # the dumped original too (texturas_pack.py takes the alpha from it:
        # its free corner is transparent, the pieces would be cut out)
        orig = DUMP / nombre
        if not (RESP / ("dump_" + nombre)).exists():
            shutil.copy2(orig, RESP / ("dump_" + nombre))
        Do = Image.open(RESP / ("dump_" + nombre)).convert("RGBA")
        To = np.array(Do.transpose(Image.Transpose.ROTATE_270).transpose(Image.Transpose.TRANSPOSE))
        for (fx, fy), p in zip(LIBRE, (media, abierta)):
            assert not To[fy:fy + H, fx:fx + W, 3].any(), "dump corner not free"
            chico = Image.fromarray(np.clip(p, 0, 255).astype(np.uint8), "RGBA").resize((W, H), Image.LANCZOS)
            To[fy:fy + H, fx:fx + W] = np.array(chico)
        Image.fromarray(To, "RGBA").transpose(Image.Transpose.TRANSPOSE).transpose(Image.Transpose.ROTATE_90).save(orig)
        print(nombre, "pieces", W, "x", H, "at", LIBRE)


if __name__ == "__main__":
    main()
