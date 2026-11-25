"""Perfil (2026-11-16): the mascots' animations for perfil.c.

tools/visor_modelos/grabar.html renders each mascot's idle animation
(its frames, clear background) into modding/texturas/perfil/crudo/
mNNN_FFF.png and mNNN.json (frames, ms each). This crops every model to what its frames cover (the same box
for all frames, so it does not jump), makes it square, LADO px, and joins the
frames one under the other: modding/texturas/perfil/juego/mascotas/II.png,
II = the mascot's place in mascota.c k_modelo (the F2 "Tu mascota" order).
256 colours with alpha; II.txt beside it: the ms each frame shows.

  py -3 tools/perfil.py            all the mascots that have frames
  py -3 tools/perfil.py --lista    the model numbers, for grabar.html?m=...
  py -3 tools/perfil.py --hoja     also a contact sheet (perfil/hoja.png)
"""
import json
import pathlib
import re
import sys

from PIL import Image

R = pathlib.Path(__file__).resolve().parent.parent
CRUDO = R / "modding" / "texturas" / "perfil" / "crudo"
# 2026-11-16 (the user: "algunos estan bien, solo graba los que no estan de
# frente o se ven mal"): grabar.html?vista=frente records apart; the model
# numbers listed in camaras.txt are taken from there
CRUDO_FRENTE = R / "modding" / "texturas" / "perfil" / "crudo_frente"
CAMARAS = R / "modding" / "texturas" / "perfil" / "camaras.txt"


def de_frente():
    if not CAMARAS.exists():
        return set()
    txt = re.sub(r"#.*", "", CAMARAS.read_text(encoding="utf-8"))
    return {int(x) for x in re.findall(r"\d+", txt)}
SALIDA = R / "modding" / "texturas" / "perfil" / "juego" / "mascotas"
LADO = 320
# 2026-11-19 (the user, looking at them in the Perfil): by model number,
# (size, right, down): size > 1 bigger (may cut the edges), < 1 smaller;
# right / down move it, in parts of the box
AJUSTE = {}     # (the 2026-11-19 captures were the duel's mascot: mascota.c k_retoque)


def modelos():
    s = (R / "ygo-apex" / "src" / "mascota.c").read_text(encoding="utf-8")
    b = s[s.index("k_modelo[NMODELOS] = {"):]
    b = b[b.index("{") + 1:b.index("};")]
    b = re.sub(r"/\*.*?\*/", "", b, flags=re.S)
    return [int(x) for x in re.findall(r"\b(\d+)\b", b)]


def armar(i, m, frente=frozenset()):
    crudo = CRUDO_FRENTE if m in frente else CRUDO
    info = crudo / ("m%03d.json" % m)
    if not info.exists():
        return False
    inf = json.loads(info.read_text())
    frames = inf["frames"]
    fr = [crudo / ("m%03d_%03d.png" % (m, f)) for f in range(frames)]
    if not all(p.exists() for p in fr):
        return False
    ims = [Image.open(p).convert("RGBA") for p in fr]
    caja = None
    for im in ims:
        b = im.getchannel("A").point(lambda a: 255 if a > 10 else 0).getbbox()
        if b:
            caja = b if not caja else (min(caja[0], b[0]), min(caja[1], b[1]), max(caja[2], b[2]), max(caja[3], b[3]))
    if not caja:
        return False
    # 2026-11-16 (the user: "centra los que no lo esten"): on its box, unless
    # the body's middle (where its pixels weigh, over all the frames) is
    # aside of it (a staff, a tail, a wing): then sideways on the body; up
    # and down on the box; the square still holds everything
    import numpy as np
    peso = np.zeros((ims[0].height, ims[0].width), np.float64)
    for im in ims:
        peso += np.asarray(im.getchannel("A"), np.float64)
    col = peso.sum(axis=0)
    cuerpo = int(round((col * np.arange(len(col))).sum() / max(col.sum(), 1.0)))
    cx, cy = (caja[0] + caja[2]) // 2, (caja[1] + caja[3]) // 2
    if abs(cuerpo - cx) > 0.04 * max(caja[2] - caja[0], caja[3] - caja[1]):
        cx = cuerpo
    w = 2 * max(cx - caja[0], caja[2] - cx)
    h = caja[3] - caja[1]
    lado = int(max(w, h) * 1.06) + 2
    esc, der, aba = AJUSTE.get(m, (1.0, 0.0, 0.0))
    lado = int(lado / esc)
    cx, cy = int(cx - der * lado), int(cy - aba * lado)
    tira = Image.new("RGBA", (LADO, LADO * frames), (0, 0, 0, 0))
    for f, im in enumerate(ims):
        sq = Image.new("RGBA", (lado, lado), (0, 0, 0, 0))
        sq.paste(im.crop((cx - lado // 2, cy - lado // 2, cx - lado // 2 + lado, cy - lado // 2 + lado)), (0, 0))
        tira.paste(sq.resize((LADO, LADO), Image.LANCZOS), (0, f * LADO))
    SALIDA.mkdir(parents=True, exist_ok=True)
    tira.quantize(256, method=Image.Quantize.FASTOCTREE, dither=Image.Dither.NONE).save(
        SALIDA / ("%02d.png" % i), optimize=True)
    # how long each frame shows (perfil.c): the model's own speed
    (SALIDA / ("%02d.txt" % i)).write_text("%.3f" % inf["ms"])
    return True


def main():
    ms = modelos()
    if "--lista" in sys.argv:
        print(",".join(map(str, ms)))
        return
    frente = de_frente()
    hechos = [i for i, m in enumerate(ms) if armar(i, m, frente)]
    print("armadas %d de %d: %s" % (len(hechos), len(ms), " ".join(map(str, hechos))))
    if "--hoja" in sys.argv and hechos:
        cols = 10
        hoja = Image.new("RGBA", (cols * 160, ((len(hechos) + cols - 1) // cols) * 160), (60, 38, 14, 255))
        for k, i in enumerate(hechos):
            t = Image.open(SALIDA / ("%02d.png" % i)).convert("RGBA").crop((0, 0, LADO, LADO)).resize((160, 160))
            hoja.alpha_composite(t, ((k % cols) * 160, (k // cols) * 160))
        hoja.save(SALIDA.parent.parent / "hoja.png")


if __name__ == "__main__":
    main()
