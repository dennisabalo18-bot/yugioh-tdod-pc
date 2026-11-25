"""Pointer mode assets from the game's own glove (comm_13_00, 64x64 at
(0,433) of euro/<lang>/comm/2d/texdb/texdb000.png, all languages):
  ygo-apex/mods_puntero/euro/<lang>/comm/2d/texdb/texdb000.png  the atlas with
      the glove cleared, loaded instead of the disc's in pointer mode
      (RECOMP_MODS_DIR), so the Windows pointer is the only glove;
  ygo-apex/src/raton_guante.h  the glove as BGRA for the Windows pointer,
      x4 (256x256) by Upscayl's digital-art-4x (the texture pack's model;
      nearest if Upscayl is missing): raton.c shrinks it to the pointer's
      size with an area filter, so it is smooth at any window size.
    py -3 tools/guante.py
"""
import pathlib
import subprocess
import tempfile
from PIL import Image
ROOT = pathlib.Path(__file__).resolve().parent.parent
GAME = ROOT.parent / "Yu-Gi-Oh TDOD"
UPSCAYL = pathlib.Path(r"C:\Program Files\Upscayl\resources")
X, Y, S = 0, 433, 64
HD = S * 4
glove = None
for lang in ("en", "fr", "gr", "it", "sp"):
    src = GAME / "euro" / lang / "comm" / "2d" / "texdb" / "texdb000.png"
    im = Image.open(src)
    if glove is None:
        glove = im.convert("RGBA").crop((X, Y, X + S, Y + S))
    # Kept as the disc has it (paletted, tRNS): the game's own PNG reader
    # loads it. Palette index 0 is the fully transparent colour.
    assert im.mode == "P" and im.info["transparency"][0] == 0
    im.paste(0, (X, Y, X + S, Y + S))
    out = ROOT / "ygo-apex" / "mods_puntero" / "euro" / lang / "comm" / "2d" / "texdb" / "texdb000.png"
    out.parent.mkdir(parents=True, exist_ok=True)
    im.save(out, transparency=im.info["transparency"])
    print(out)

with tempfile.TemporaryDirectory() as t:
    src, dst = pathlib.Path(t) / "g.png", pathlib.Path(t) / "g4.png"
    glove.save(src)
    exe = UPSCAYL / "bin" / "upscayl-bin.exe"
    if exe.is_file():
        subprocess.run([str(exe), "-i", str(src), "-o", str(dst), "-s", "4", "-m",
                        str(UPSCAYL / "models"), "-n", "digital-art-4x", "-t", "64", "-f", "png"],
                       capture_output=True)
    if dst.is_file():
        hd = Image.open(dst).convert("RGBA").resize((HD, HD), Image.LANCZOS)
        print("glove x4: Upscayl digital-art-4x")
    else:
        hd = glove.resize((HD, HD), Image.NEAREST)
        print("glove x4: nearest (no Upscayl)")

px = hd.load()
lines = []
for y in range(HD):
    row = []
    for x in range(HD):
        r, g, b, a = px[x, y]
        row.append("0x%02X,0x%02X,0x%02X,0x%02X" % (b, g, r, a))
    lines.append("    " + ",".join(row) + ",")
h = ROOT / "ygo-apex" / "src" / "raton_guante.h"
h.write_text("/* The game's glove (comm_13_00): 64x64 in the game (GUANTE_SIZE, the\n"
             " * fingertip at (22, 14) there); k_guante is it x4, GUANTE_HD square,\n"
             " * BGRA, top row first. Made by tools/guante.py. */\n"
             "#define GUANTE_SIZE 64\n#define GUANTE_TIP_X 22\n#define GUANTE_TIP_Y 14\n"
             f"#define GUANTE_HD {HD}\n"
             f"static const unsigned char k_guante[{HD} * {HD} * 4] = {{\n" + "\n".join(lines) + "\n};\n",
             encoding="utf-8")
print(h)
