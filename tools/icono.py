"""The launchers' icon (tools/launcher/yugioh.ico): the game's glove x4 from
ygo-apex/src/raton_guante.h (tools/guante.py), on a gold millennium circle.
    py -3 tools/icono.py
"""
import pathlib
import re
from PIL import Image, ImageDraw

ROOT = pathlib.Path(__file__).resolve().parent.parent
h = (ROOT / "ygo-apex" / "src" / "raton_guante.h").read_text(encoding="utf-8")
hd = int(re.search(r"#define GUANTE_HD (\d+)", h).group(1))
body = h[h.index("{") + 1:h.rindex("}")]
vals = bytes(int(v, 16) for v in re.findall(r"0x([0-9A-Fa-f]{2})", body))
assert len(vals) == hd * hd * 4
glove = Image.frombytes("RGBA", (hd, hd), vals, "raw", "BGRA")

S = 256
ico = Image.new("RGBA", (S, S), (0, 0, 0, 0))
d = ImageDraw.Draw(ico)
d.ellipse((8, 8, S - 8, S - 8), fill=(16, 18, 34, 255), outline=(212, 168, 60, 255), width=14)
g = glove.crop(glove.getbbox())
k = 0.72 * S / max(g.size)
g = g.resize((int(g.width * k), int(g.height * k)), Image.LANCZOS)
ico.alpha_composite(g, ((S - g.width) // 2, (S - g.height) // 2))
out = ROOT / "tools" / "launcher" / "yugioh.ico"
ico.save(out, sizes=[(256, 256), (128, 128), (64, 64), (48, 48), (32, 32), (16, 16)])
ico.save(out.with_suffix(".png"))
print(out)
