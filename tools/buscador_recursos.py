"""The Biblioteca's search panel pictures (buscador.c), from the game's own:
    python tools/buscador_recursos.py <game folder> [<out folder> [<mod texdb folder>]]
<game folder>: the player's (has Game/ and Mods-Extras/perfil/); out: its Mods-Extras/buscador by
default; the card mod's thumbnail sheets (2026-11-16, the user: "aparece con la carta vieja") win
over the game's (tools/copiar_al_juego.sh runs this after cartas_mod.py). Writes:
  panel.png   the panel: the Perfil's frame (fondo.png: the menu's gold edge and stone)
              rebuilt 9-slice at 520 x 1040, no boxes
  lupa.png, lupa_luz.png  the search button (2026-11-17, the user: "Mete una Lupa bien Hecha en el
              medio al lado de donde esta lo redondo"): a medallion like the game's round one between the
              card number boxes (black edge, orange ring, dark stone), a magnifier inside; _luz lit
  placa.png   the gold plate of the Perfil's name (the search field, the chips lit)
  iconos.png  8 icons 64 x 64 in a row: attributes 1..6, magic, trap (texto_cartas.py's,
              the cards' own HD icons)
  minis.png   the game's card thumbnails (Game/card/mini/texdb, 35 x 51) by image ID:
              cell (ID - 4000), 40 a row, 43 rows (IDs up to 5695)
"""
import pathlib
import sys

from PIL import Image, ImageDraw, ImageFilter

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import texto_cartas as tc  # noqa: E402

game = pathlib.Path(sys.argv[1])
out = pathlib.Path(sys.argv[2]) if len(sys.argv) > 2 else game / "Mods-Extras" / "Contenido" / "buscador"
mod = pathlib.Path(sys.argv[3]) if len(sys.argv) > 3 else game / "Mods-Extras" / "Contenido" / "cartas" / "archivos" / "card" / "mini" / "texdb"
out.mkdir(parents=True, exist_ok=True)
fondo = Image.open(game / "Mods-Extras" / "Contenido" / "perfil" / "fondo.png").convert("RGBA")
F = fondo.size[0] / 900.0          # the measures below are on a 900-wide view

def r(x0, y0, x1, y1):
    return fondo.crop((round(x0 * F), round(y0 * F), round(x1 * F), round(y1 * F)))

# ---- the panel: the big frame's corners and edges, its stone inside
frame = r(0, 126, 900, 600)          # the whole lower panel
fw, fh = frame.size
c = round(40 * F)                    # corner size (the gold edge and the inner line)
cb = round(13 * F)                   # the outer gold band alone (2026-11-17: the inner line cut the title)
W, H = 520, 1040
panel = Image.new("RGBA", (W, H))
stone = r(560, 150, 880, 430)        # an empty stretch of stone
sw_, sh_ = stone.size
for j, y in enumerate(range(0, H, sh_)):
    for i, x in enumerate(range(0, W, sw_)):
        t = stone
        if i % 2:
            t = t.transpose(Image.FLIP_LEFT_RIGHT)
        if j % 2:
            t = t.transpose(Image.FLIP_TOP_BOTTOM)
        panel.alpha_composite(t, (x, y))
# edges (stretched along), then the corners
c = cb
top = frame.crop((c, 0, fw - c, c)).resize((W - 2 * c, c))
bot = frame.crop((c, fh - c, fw - c, fh)).resize((W - 2 * c, c))
# the sides from a short clean stretch under the corners (lower down the
# boxes and a medallion touch them)
lef = frame.crop((0, round(40 * F), c, round(60 * F))).resize((c, H - 2 * c))
rig = frame.crop((fw - c, round(40 * F), fw, round(60 * F))).resize((c, H - 2 * c))
for part, pos in ((top, (c, 0)), (bot, (c, H - c)), (lef, (0, c)), (rig, (W - c, c)),
                  (frame.crop((0, 0, c, c)), (0, 0)), (frame.crop((fw - c, 0, fw, c)), (W - c, 0)),
                  (frame.crop((0, fh - c, c, fh)), (0, H - c)), (frame.crop((fw - c, fh - c, fw, fh)), (W - c, H - c))):
    # the band over the stone; where the band is see-through the stone goes too
    hole = Image.new("RGBA", part.size, (0, 0, 0, 0))
    panel.paste(hole, pos)
    panel.alpha_composite(part, pos)
panel.save(out / "panel.png")

# ---- the search button: a medallion and a magnifier, drawn 4x and made small
def medallon(luz):
    Q = 4
    N = 128 * Q
    im = Image.new("RGBA", (N, N), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    def circ(r, fill):
        d.ellipse((N / 2 - r, N / 2 - r, N / 2 + r, N / 2 + r), fill=fill)
    R = N / 2 - 2 * Q
    circ(R, (12, 8, 2, 255))                                # black edge
    # orange ring, lighter on top
    ring = Image.new("RGBA", (N, N), (0, 0, 0, 0))
    rd = ImageDraw.Draw(ring)
    for y in range(N):
        t = y / N
        k = 1.25 if luz else 1.0
        col = (min(255, int((255 - 30 * t) * k)), min(255, int((178 - 80 * t) * k)), int((40 - 20 * t) * k), 255)
        rd.line((0, y, N, y), fill=col)
    mask = Image.new("L", (N, N), 0)
    md = ImageDraw.Draw(mask)
    r1 = R - 4 * Q
    md.ellipse((N / 2 - r1, N / 2 - r1, N / 2 + r1, N / 2 + r1), fill=255)
    im.paste(ring, (0, 0), mask)
    r2 = R - 15 * Q
    circ(r2 + 2.5 * Q, (14, 10, 4, 255))                   # the inner black line
    # dark stone inside, lighter on top
    st = Image.new("RGBA", (N, N), (0, 0, 0, 0))
    sd = ImageDraw.Draw(st)
    for y in range(N):
        t = y / N
        v = int(70 - 34 * t)
        sd.line((0, y, N, y), fill=(v, v + 2, v - 6, 255))
    mask = Image.new("L", (N, N), 0)
    ImageDraw.Draw(mask).ellipse((N / 2 - r2, N / 2 - r2, N / 2 + r2, N / 2 + r2), fill=255)
    im.paste(st, (0, 0), mask)
    # the magnifier: a gold rim, a pale blue glass with a shine, a dark wood
    # handle with a gold ferrule, all with a black edge
    cx, cy, lr = N * 0.44, N * 0.43, N * 0.19
    ang = 0.7071
    hx0, hy0 = cx + lr * ang * 1.02, cy + lr * ang * 1.02
    hx1, hy1 = cx + lr * 2.35 * ang, cy + lr * 2.35 * ang
    capa = Image.new("RGBA", (N, N), (0, 0, 0, 0))
    cd = ImageDraw.Draw(capa)
    cd.line((hx0, hy0, hx1, hy1), fill=(10, 6, 2, 255), width=int(N * 0.15))
    cd.ellipse((hx1 - N * 0.075, hy1 - N * 0.075, hx1 + N * 0.075, hy1 + N * 0.075), fill=(10, 6, 2, 255))
    cd.ellipse((cx - lr - N * 0.06, cy - lr - N * 0.06, cx + lr + N * 0.06, cy + lr + N * 0.06), fill=(10, 6, 2, 255))
    # handle: wood
    cd.line((hx0 + N * 0.05, hy0 + N * 0.05, hx1, hy1), fill=(120, 62, 22, 255), width=int(N * 0.095))
    cd.ellipse((hx1 - N * 0.047, hy1 - N * 0.047, hx1 + N * 0.047, hy1 + N * 0.047), fill=(120, 62, 22, 255))
    cd.line((hx0 + N * 0.06, hy0 + N * 0.035, hx1 - N * 0.01, hy1 - N * 0.03), fill=(170, 98, 44, 255), width=int(N * 0.025))
    # ferrule
    fx, fy = hx0 + N * 0.035, hy0 + N * 0.035
    cd.line((fx - N * 0.02, fy - N * 0.02, fx + N * 0.03, fy + N * 0.03), fill=(250, 214, 60, 255), width=int(N * 0.11))
    # rim
    cd.ellipse((cx - lr - N * 0.035, cy - lr - N * 0.035, cx + lr + N * 0.035, cy + lr + N * 0.035), fill=(250, 220, 60, 255))
    cd.ellipse((cx - lr - N * 0.015, cy - lr - N * 0.015, cx + lr + N * 0.015, cy + lr + N * 0.015), fill=(200, 140, 20, 255))
    # glass
    gl = Image.new("RGBA", (N, N), (0, 0, 0, 0))
    gd = ImageDraw.Draw(gl)
    for y in range(int(cy - lr), int(cy + lr) + 1):
        t = (y - (cy - lr)) / (2 * lr)
        gd.line((0, y, N, y), fill=(int(170 - 70 * t), int(220 - 60 * t), int(240 - 30 * t), 255))
    gm = Image.new("L", (N, N), 0)
    ImageDraw.Draw(gm).ellipse((cx - lr, cy - lr, cx + lr, cy + lr), fill=255)
    capa.paste(gl, (0, 0), gm)
    # shine
    sh = Image.new("L", (N, N), 0)
    ImageDraw.Draw(sh).arc((cx - lr * 0.72, cy - lr * 0.72, cx + lr * 0.72, cy + lr * 0.72), 195, 265,
                            fill=255, width=int(N * 0.035))
    capa.paste(Image.new("RGBA", (N, N), (255, 255, 255, 230)), (0, 0), sh)
    im.alpha_composite(capa)
    if luz:
        glow = Image.new("RGBA", (N, N), (0, 0, 0, 0))
        ImageDraw.Draw(glow).ellipse((N / 2 - R, N / 2 - R, N / 2 + R, N / 2 + R), outline=(255, 240, 150, 200),
                                     width=int(3 * Q))
        im.alpha_composite(glow.filter(ImageFilter.GaussianBlur(2 * Q)))
    return im.resize((128, 128), Image.LANCZOS)


medallon(False).save(out / "lupa.png")
medallon(True).save(out / "lupa_luz.png")

# ---- the gold plate (the Perfil's name)
# 2026-11-19 (the user: "salen cortadas los botones"): it was cut from a fixed
# spot of fondo.png, and the Perfil's plate moved (now apart: placa_nombre.png)
Image.open(game / "Mods-Extras" / "Contenido" / "perfil" / "placa_nombre.png").convert("RGBA").save(out / "placa.png")

# ---- the icons
iconos = Image.new("RGBA", (64 * 8, 64))
for i, clave in enumerate([1, 2, 3, 4, 5, 6, "magia", "trampa"]):
    k = 64 / (2 * tc.ICONO_R + 2)
    tmp = Image.new("RGBA", (round(130 * k), round(40 * k)))
    cc = {"tipo": 21 if clave == "trampa" else 22 if clave == "magia" else 0, "atrib": clave}
    tc.icono(tmp, cc, k)
    cx, cy = tc.ICONO_C[0] * k, tc.ICONO_C[1] * k
    iconos.alpha_composite(tmp.crop((round(cx - 32), round(cy - 32), round(cx + 32), round(cy + 32))), (64 * i, 0))
iconos.save(out / "iconos.png")

# ---- the thumbnails
minis = Image.new("RGBA", (40 * 35, 43 * 51))     # IDs 4007..5695
n = 0
for txt in sorted((game / "Game" / "card" / "mini" / "texdb").glob("texdb*.txt")):
    png = mod / txt.with_suffix(".png").name
    hoja = Image.open(png if png.is_file() else txt.with_suffix(".png")).convert("RGBA")
    S = hoja.size[0]
    # 2026-11-17 (the user: Soldado del Brillo Negro came out black): a half
    # sheet (texdb009: 56 cards) is a 512 x 256 texture in a 512 x 512 PNG,
    # its v over 256 rows; the game's own sheet tells (nothing below row 256)
    orig = Image.open(txt.with_suffix(".png")).convert("RGBA")
    T = 256 if orig.getchannel("A").crop((0, 256, S, S)).getbbox() is None else S
    for line in txt.read_text(errors="replace").splitlines():
        p = line.split()
        if len(p) < 8 or not p[0].endswith(".png"):
            continue
        cid = int(p[0][:-4])
        u0, v0, u1, v1, gir = float(p[3]), float(p[4]), float(p[5]), float(p[6]), int(p[7])
        x0, x1 = round(u0 * S), round(u1 * S)
        y0, y1 = round((1 - v1) * T), round((1 - v0) * T)       # v from the bottom
        # 2026-11-24 (the user: "se ven al reves algunas", Polimerizacion,
        # Monstruo renacido...): a cell that is not turned (last field 0, ~30
        # cards) is upright in the sheet as it is; only the turned ones are
        # flipped and turned
        im = hoja.crop((x0, y0, x1, y1))
        if gir:
            im = im.transpose(Image.FLIP_TOP_BOTTOM).transpose(Image.ROTATE_90)
        im = im.resize((35, 51), Image.LANCZOS)
        im.putalpha(255)
        i = cid - 4000
        if 0 <= i < 40 * 43:
            minis.paste(im, ((i % 40) * 35, (i // 40) * 51))
            n += 1
# an ID with no thumbnail in the game's tables (Maryokutai, 5260): the game's
# own stand-in, card/mini/0000.png
cero = game / "Game" / "card" / "mini" / "0000.png"
if cero.is_file():
    c0 = Image.open(cero).convert("RGBA").resize((35, 51), Image.LANCZOS)
    c0.putalpha(255)
    for i in range(40 * 43):
        x, y = (i % 40) * 35, (i // 40) * 51
        if minis.getpixel((x + 17, y + 25))[3] == 0:
            minis.paste(c0, (x, y))
minis.save(out / "minis.png")
print("listo:", out, n, "miniaturas")
