"""Perfil (2026-11-16): the pictures of the main menu's Perfil button and panel
(perfil.c draws them over the game, at the size of its window).

Everything comes from the game itself: modding/texturas/perfil/
menu_principal.webp is the user's capture of the main menu at 1920 x 1080
(the game's picture 1792 x 1008, from row 23): a medallion of the wheel with
its symbol painted over (a duelist's bust in the game's teal), the menu's
banner without its words, its round arrows, the walls' stone. The layout is
in that capture's pixels: perfil.c scales it by the game's height / 1008.

Out: modding/texturas/perfil/juego/ (boton.png, boton_luz.png, fondo.png,
flecha_izq_luz.png, flecha_der_luz.png, foto_luz.png, fundas/AA_M.jpg).

  py -3 tools/perfil_recursos.py
"""
import pathlib

from PIL import Image, ImageDraw, ImageEnhance, ImageFilter, ImageOps

R = pathlib.Path(__file__).resolve().parent.parent
P = R / "modding" / "texturas" / "perfil"
OUT = P / "juego"
FU = R / "modding" / "texturas" / "fundas"
src = Image.open(P / "menu_principal.webp").convert("RGB")

# the panel's place in the capture (perfil.c: FONDO_X / FONDO_Y)
FX, FY = 238, 36
X0, Y0, X1, Y1 = 250, 250, 1670, 990


def lerp(a, b, t):
    return tuple(int(a[i] + (b[i] - a[i]) * t) for i in range(3))


def borrar(im, x0, y0, x1, y1):
    """fills the box row by row between the pixels just outside it"""
    px = im.load()
    for y in range(y0, y1):
        a, b = px[x0 - 2, y], px[x1 + 2, y]
        for x in range(x0, x1):
            px[x, y] = lerp(a, b, (x - x0) / (x1 - x0))


def medallon():
    m = src.crop((890, 108, 1030, 215)).convert("RGB")
    m = m.resize((m.width * 4, m.height * 4), Image.LANCZOS)
    borrar(m, 47 * 4, 26 * 4, 95 * 4, 76 * 4)
    m = m.filter(ImageFilter.GaussianBlur(0.6))
    d = ImageDraw.Draw(m)
    cx, cy = 70 * 4, 52 * 4
    for dx, dy, col in ((5, 5, (178, 150, 40)), (0, 0, (10, 74, 68))):
        d.ellipse((cx - 36 + dx, cy - 88 + dy, cx + 36 + dx, cy - 16 + dy), fill=col)
        d.chord((cx - 80 + dx, cy - 4 + dy, cx + 80 + dx, cy + 156 + dy), 180, 360, fill=col)
    m = m.resize((m.width // 4, m.height // 4), Image.LANCZOS)
    mk = Image.new("L", m.size, 0)
    ImageDraw.Draw(mk).ellipse((6, 5, m.width - 6, m.height - 4), fill=255)
    r = m.convert("RGBA")
    r.putalpha(mk.filter(ImageFilter.GaussianBlur(1.5)))
    return r


def boton(luz):
    """the medallion 1.3 times the wheel's, on a canvas with room for the glow"""
    m = medallon()
    m = m.resize((int(m.width * 1.3), int(m.height * 1.3)), Image.LANCZOS)
    g = Image.new("RGBA", (m.width + 60, m.height + 60), (0, 0, 0, 0))
    g.alpha_composite(m, (30, 30))
    # 2026-11-16 (the user: "podemos copiar el efecto de los botones en
    # nuestro boton?"): the mouse over it lights it as the wheel's chosen
    # icon: gold, with 16 rays round it (that sun is not a loose texture of
    # the game, it is in the wheel's model: drawn after the video's frames)
    return sol(dorar(g)) if luz else g


def dorar(m):
    """the medallion in the chosen icon's gold (dark lines, orange, yellow);
    the teal bust kept"""
    import numpy as np
    a = np.asarray(m, np.float32)
    rgb, al = a[..., :3], a[..., 3:]
    teal = (rgb[..., 2] > rgb[..., 0] + 15) & (rgb[..., 1] > rgb[..., 0])
    t = np.clip((rgb.mean(-1, keepdims=True) - 22) / 55.0, 0, 1)
    oscuro = np.array([40, 18, 0], np.float32)
    naranja = np.array([215, 120, 10], np.float32)
    amarillo = np.array([255, 232, 70], np.float32)
    g = np.where(t < 0.35, oscuro + (naranja - oscuro) * (t / 0.35),
                 naranja + (amarillo - naranja) * ((t - 0.35) / 0.65))
    out = np.where(teal[..., None], rgb, g)
    return Image.fromarray(np.concatenate([out, al], -1).clip(0, 255).astype(np.uint8), "RGBA")


def sol(m, rayos=16, giro=0.0):
    """the rays behind the medallion's ellipse: a dark edge, gold, one side lit"""
    import math
    W, H = m.size
    k = 4
    lien = Image.new("RGBA", (W * k, H * k), (0, 0, 0, 0))
    d = ImageDraw.Draw(lien)
    cx, cy = W * k / 2, H * k / 2 + 2 * k
    rx, ry = (W - 60) * k / 2 * 0.98, (H - 60) * k / 2 * 0.98
    for borde in (1, 0):
        for i in range(rayos):
            ang = 2 * math.pi * (i + giro) / rayos
            ab = math.pi / rayos * (0.97 if borde else 0.85)
            e = 5 * k if borde else 0
            pts = [(cx + math.cos(aa) * (rx * rr + e), cy + math.sin(aa) * (ry * rr + e))
                   for aa, rr in ((ang - ab, 0.92), (ang, 1.26), (ang + ab, 0.92))]
            if borde:
                d.polygon(pts, fill=(25, 10, 0, 255))
            else:
                d.polygon(pts, fill=(238, 170, 20, 255))
                d.polygon([pts[0], pts[1], ((pts[0][0] + pts[2][0]) / 2, (pts[0][1] + pts[2][1]) / 2)],
                          fill=(255, 238, 120, 255))
    lien = lien.resize((W, H), Image.LANCZOS)
    lien.alpha_composite(m)
    return lien


def placa(w, h):
    # the banner's gold, top to bottom (measured on its plate: light top
    # edge, warm middle, darker foot)
    pl = Image.new("RGBA", (w, h))
    pd = ImageDraw.Draw(pl)
    paso = ((0.0, (255, 236, 130)), (0.12, (250, 200, 50)), (0.55, (236, 160, 18)), (0.85, (205, 125, 8)),
            (1.0, (150, 85, 0)))
    for y in range(h):
        t = y / (h - 1)
        for k in range(len(paso) - 1):
            if paso[k][0] <= t <= paso[k + 1][0]:
                c = lerp(paso[k][1], paso[k + 1][1], (t - paso[k][0]) / (paso[k + 1][0] - paso[k][0]))
        pd.line((0, y, w, y), fill=c + (255,))
    mk = Image.new("L", pl.size, 0)
    ImageDraw.Draw(mk).rounded_rectangle((0, 0, w - 1, h - 1), 14, fill=255)
    pl.putalpha(mk)
    out = Image.new("RGBA", (w + 8, h + 8), (0, 0, 0, 0))
    ImageDraw.Draw(out).rounded_rectangle((0, 0, w + 7, h + 7), 18, fill=(40, 15, 0, 255))
    out.alpha_composite(pl, (4, 4))
    return out


def banner():
    b = src.crop((380, 690, 1540, 940)).copy()
    borrar(b, 300, 40, 862, 108)
    W, H = b.size
    # the menu's arch behind the banner, between its two top caps
    ImageDraw.Draw(b).rectangle((250, 0, 905, 21), fill=(0, 0, 0))
    bp = b.load()
    vis = bytearray(W * H)
    st = [(x, y) for x in range(W) for y in (0, H - 1)] + [(x, y) for y in range(H) for x in (0, W - 1)]
    while st:
        x, y = st.pop()
        if x < 0 or y < 0 or x >= W or y >= H or vis[y * W + x]:
            continue
        r, g, bb = bp[x, y]
        if r > 150:                      # the gold
            continue
        vis[y * W + x] = 1
        st += [(x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)]
    mk = Image.new("L", b.size, 0)
    mp = mk.load()
    for y in range(H):
        for x in range(W):
            mp[x, y] = 0 if vis[y * W + x] else 255
    mk = mk.filter(ImageFilter.MaxFilter(5)).filter(ImageFilter.GaussianBlur(1.2))
    b = b.convert("RGBA")
    b.putalpha(mk)
    return b.resize((int(W * 0.8), int(H * 0.8)), Image.LANCZOS)


def flecha(der, luz):
    f = src.crop((1320, 745, 1460, 860) if not der else (460, 745, 600, 860)).resize((105, 86), Image.LANCZOS)
    if luz:
        f = ImageEnhance.Brightness(f).enhance(1.4)
    mk = Image.new("L", f.size, 0)
    ImageDraw.Draw(mk).ellipse((6, 6, f.width - 6, f.height - 6), fill=255)
    f = f.convert("RGBA")
    f.putalpha(mk.filter(ImageFilter.GaussianBlur(2)))
    return f


# the layout (capture px; perfil.c has the same numbers)
# 2026-11-19 (the user's approved mock): the tapete as the mascot and the
# sleeve (its box at the same height, its plate with arrows, its number),
# the player's name on the panel's top edge; the arrows are no longer in the
# background (perfil.c draws them in windows of their own, animated)
FOTO = (305, 420, 330, 240)                # x, y, w, h
PLACA_NOMBRE = (960, 220, 330, 60)         # centre x, top, w, h
MASC = (800, 380, 320, 320)
PLACA_MASC = (960, 750, 320, 56)
PLACA_FUNDA = (1450, 750, 240, 56)
PLACA_TAPETE = (470, 750, 240, 56)
FLECHAS = ()
FLECHA_D = 128                             # each frame of the arrows' strips, px (64 ref px shown)
FLECHA_D_CUADROS = 12


def flecha_d(der, luz):
    """the Perfil's arrows (2026-11-19, option D the user chose): a small
    medallion, a gold rim with 8 notches over a dark edge, a teal middle and a
    gold arrow. FLECHA_D_CUADROS frames one under the other: the rim turned by
    one notch step (45 deg) over the strip, so it loops; perfil.c steps
    through them under the pointer. luz: brighter, with a soft glow."""
    import math
    S = FLECHA_D * 4
    tira = Image.new("RGBA", (FLECHA_D, FLECHA_D * FLECHA_D_CUADROS), (0, 0, 0, 0))
    oro = (255, 205, 70) if luz else (232, 166, 32)
    oro_os = (190, 120, 20) if luz else (150, 92, 12)
    centro = (30, 92, 98) if luz else (24, 66, 72)
    for k in range(FLECHA_D_CUADROS):
        im = Image.new("RGBA", (S, S), (0, 0, 0, 0))
        d = ImageDraw.Draw(im)
        c = S / 2
        r_out, r_aro, r_in = S * 0.40, S * 0.355, S * 0.24
        if luz:
            g = Image.new("RGBA", (S, S), (0, 0, 0, 0))
            ImageDraw.Draw(g).ellipse((c - S * 0.48, c - S * 0.48, c + S * 0.48, c + S * 0.48), fill=(255, 220, 110, 150))
            im.alpha_composite(g.filter(ImageFilter.GaussianBlur(S * 0.05)))
            d = ImageDraw.Draw(im)
        d.ellipse((c - r_out, c - r_out, c + r_out, c + r_out), fill=(40, 18, 0, 255))
        d.ellipse((c - r_aro, c - r_aro, c + r_aro, c + r_aro), fill=oro + (255,))
        # the notches, turned by k / FLECHA_D_CUADROS of a step (the step is 45 deg)
        a0 = math.radians(45.0 * k / FLECHA_D_CUADROS) * (1 if der else -1)
        for n in range(8):
            a = a0 + n * math.pi / 4
            p = [(c + math.cos(a - 0.12) * r_aro * 1.0, c + math.sin(a - 0.12) * r_aro * 1.0),
                 (c + math.cos(a + 0.12) * r_aro * 1.0, c + math.sin(a + 0.12) * r_aro * 1.0),
                 (c + math.cos(a + 0.09) * r_in * 1.12, c + math.sin(a + 0.09) * r_in * 1.12),
                 (c + math.cos(a - 0.09) * r_in * 1.12, c + math.sin(a - 0.09) * r_in * 1.12)]
            d.polygon(p, fill=oro_os + (255,))
        d.ellipse((c - r_in * 1.08, c - r_in * 1.08, c + r_in * 1.08, c + r_in * 1.08), fill=(40, 18, 0, 255))
        d.ellipse((c - r_in, c - r_in, c + r_in, c + r_in), fill=centro + (255,))
        # light on the middle's top
        hl = Image.new("RGBA", (S, S), (0, 0, 0, 0))
        ImageDraw.Draw(hl).ellipse((c - r_in * 0.8, c - r_in * 0.9, c + r_in * 0.8, c - r_in * 0.05), fill=(255, 255, 255, 40))
        im.alpha_composite(hl)
        d = ImageDraw.Draw(im)
        sg = 1 if der else -1
        t = [(c - sg * r_in * 0.42, c - r_in * 0.62), (c + sg * r_in * 0.62, c), (c - sg * r_in * 0.42, c + r_in * 0.62)]
        d.polygon(t, fill=(255, 225, 120, 255) if luz else (250, 200, 70, 255), outline=(40, 18, 0, 255))
        tira.paste(im.resize((FLECHA_D, FLECHA_D), Image.LANCZOS), (0, k * FLECHA_D))
    return tira


def piedra(w, h):
    """the walls' hieroglyphs (right of the capture), tiled, in a dark brown"""
    g = ImageOps.grayscale(src.crop((1712, 770, 1845, 860)))
    # the light's falloff taken out (only the carving stays), so the tiles meet
    lo = g.filter(ImageFilter.GaussianBlur(6))
    g = Image.eval(Image.merge("LA", (g, lo)).getchannel(0), lambda v: v)
    gp, lp = g.load(), lo.load()
    for y in range(g.height):
        for x in range(g.width):
            gp[x, y] = max(0, min(255, 110 + 2 * (gp[x, y] - lp[x, y])))
    t = ImageOps.colorize(g, (14, 7, 1), (95, 66, 28))
    out = Image.new("RGB", (w, h))
    for j in range(0, h, t.height):
        for i in range(0, w, t.width):
            out.paste(t, (i, j))
    return out


def fondo_completo():
    """2026-11-21 (the user: the back looked old; option B of
    modding/muestras/fondo_menu_*.png, after C was "demasiado nitido"): the
    game's menu background (FDE76864287C3300, 640 x 480 from row 32 of a
    1024 x 512) from the texture pack's 4x entry (4096 x 2048, BC3) as it
    is: 2560 x 1920. Without the pack, the dump as before."""
    import io, struct
    pack = R / "modding" / "texturas" / "textures_bc3.pack"
    h0 = 0xFDE76864287C3300
    try:
        with open(pack, "rb") as f:
            hdr = f.read(32)
            count = struct.unpack_from("<I", hdr, 8)[0]
            f.seek(struct.unpack_from("<Q", hdr, 16)[0])
            raw = f.read(32 * count)
            ent = None
            for i in range(count):
                h, w, hh, lv, fl, off = struct.unpack_from("<QIIIIQ", raw, 32 * i)
                if h == h0:
                    ent = (w, hh, fl, off)
            w, hh, fl, off = ent
            assert fl & 0x10 and (w, hh) == (4096, 2048)
            bc1 = bool(fl & 0x20)     # halved by tools/achicar_pack.py
            n = (w // 4) * (hh // 4) * (8 if bc1 else 16)
            f.seek(off)
            data = f.read(n)
        dds = b"DDS " + struct.pack("<7I", 124, 0x1007, hh, w, n, 0, 1) + b"\0" * 44
        dds += struct.pack("<2I4s5I", 32, 4, b"DXT1" if bc1 else b"DXT5", 0, 0, 0, 0, 0) + struct.pack("<5I", 0x1000, 0, 0, 0, 0)
        return Image.open(io.BytesIO(dds + data)).convert("RGB").crop((0, 128, 2560, 2048))
    except Exception as e:
        print("fondo_completo: sin el pack (%s), del volcado" % e)
        f = Image.open(R / "modding" / "texturas" / "dump" / "FDE76864287C3300_1024x512.png").convert("RGB")
        return f.crop((0, 32, 640, 512))


def fondo():
    W, H = 1444, 964
    b = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    d = ImageDraw.Draw(b)
    o = lambda x, y: (x - FX, y - FY)
    x0, y0 = o(X0, Y0)
    x1, y1 = o(X1, Y1)
    d.rounded_rectangle((x0 - 12, y0 - 12, x1 + 12, y1 + 12), 30, fill=(30, 12, 0, 255))
    d.rounded_rectangle((x0 - 6, y0 - 6, x1 + 6, y1 + 6), 26, fill=(236, 170, 26, 255))
    d.rounded_rectangle((x0 + 4, y0 + 4, x1 - 4, y1 - 4), 22, fill=(30, 12, 0, 255))
    st = piedra(x1 - x0 - 16, y1 - y0 - 16)
    mk = Image.new("L", st.size, 0)
    ImageDraw.Draw(mk).rounded_rectangle((0, 0, st.width - 1, st.height - 1), 18, fill=255)
    b.paste(st, (x0 + 8, y0 + 8), mk)
    d = ImageDraw.Draw(b)
    d.rounded_rectangle((x0 + 22, y0 + 22, x1 - 22, y1 - 22), 16, outline=(160, 110, 20, 255), width=2)
    bn = banner()
    b.alpha_composite(bn, (960 - bn.width // 2 - FX, 48 - FY))
    # the photo's frame (the picture goes inside)
    fx, fy = o(FOTO[0], FOTO[1])
    fw, fh = FOTO[2], FOTO[3]
    d.rounded_rectangle((fx - 9, fy - 9, fx + fw + 9, fy + fh + 9), 14, fill=(30, 12, 0, 255))
    d.rounded_rectangle((fx - 6, fy - 6, fx + fw + 6, fy + fh + 6), 12, fill=(250, 200, 60, 255))
    d.rectangle((fx - 2, fy - 2, fx + fw + 2, fy + fh + 2), fill=(30, 12, 0, 255))
    d.rectangle((fx, fy, fx + fw, fy + fh), fill=(60, 40, 90, 255))
    # (2026-11-19: no "change" badge on its corner any more: arrows by its plate)
    # the mascot's stage: a dark well with a gold edge
    mx, my = o(MASC[0], MASC[1])
    d.rectangle((mx - 7, my - 7, mx + MASC[2] + 7, my + MASC[3] + 7), fill=(30, 12, 0, 255))
    d.rectangle((mx - 5, my - 5, mx + MASC[2] + 5, my + MASC[3] + 5), fill=(200, 140, 25, 255))
    d.rectangle((mx - 2, my - 2, mx + MASC[2] + 2, my + MASC[3] + 2), fill=(30, 12, 0, 255))
    pozo = Image.new("RGBA", (MASC[2], MASC[3]), (0, 0, 0, 0))
    pd = ImageDraw.Draw(pozo)
    for k in range(MASC[3]):                      # light from the top
        t = k / MASC[3]
        pd.line((0, k, MASC[2], k), fill=(int(70 - 40 * t), int(45 - 28 * t), int(20 - 12 * t), 255))
    b.alpha_composite(pozo, (mx, my))
    # the plates and the arrows
    # (2026-11-19, the user: the empty name plate showed on the other pages:
    # it is placa_nombre.png now, perfil.c draws it on the Perfil page only)
    for cx, top, w, h in (PLACA_MASC, PLACA_FUNDA, PLACA_TAPETE):
        p = placa(w, h)
        b.alpha_composite(p, (cx - w // 2 - 4 - FX, top - 4 - FY))
    for k, (cx, cy) in enumerate(FLECHAS):
        f = flecha(k % 2 == 1, False)
        b.alpha_composite(f, (cx - f.width // 2 - FX, cy - f.height // 2 - FY))
    return b


def foto_luz(fw=FOTO[2], fh=FOTO[3]):
    """the glow round a box under the pointer (2026-11-19: also the mascot's
    and the sleeve's): fw + 60 x fh + 60, drawn at the box - 30"""
    g = Image.new("RGBA", (fw + 60, fh + 60), (0, 0, 0, 0))
    ImageDraw.Draw(g).rounded_rectangle((12, 12, fw + 48, fh + 48), 20, fill=(255, 225, 100, 220))
    g = g.filter(ImageFilter.GaussianBlur(10))
    ImageDraw.Draw(g).rectangle((30 - 9, 30 - 9, 30 + fw + 9, 30 + fh + 9), fill=(0, 0, 0, 0))
    return g


def funda_luz():
    """2026-11-19 (the user: "que no sea un cuadrado ... que salga del mazo la
    luz"): the glow round the deck's own outline (perfil.c: 5 cards 200 x 292,
    each 5 right and 6 up of the one below; drawn at CARTA_X - 30, CARTA_Y -
    24 - 30, 280 x 376), the deck itself cut out"""
    W, H = 280, 376
    forma = Image.new("L", (W, H), 0)
    d = ImageDraw.Draw(forma)
    for i in range(5):
        x, y = 30 + i * 5, 30 + 24 - i * 6
        d.rounded_rectangle((x, y, x + 200, y + 292), 6, fill=255)
    halo = forma.filter(ImageFilter.MaxFilter(25)).filter(ImageFilter.GaussianBlur(10))
    a = Image.eval(halo, lambda v: int(v * 220 / 255))
    a = Image.composite(Image.new("L", (W, H), 0), a, forma)
    g = Image.new("RGBA", (W, H), (255, 225, 100, 0))
    g.putalpha(a)
    return g


def fundas():
    d = OUT / "fundas"
    d.mkdir(parents=True, exist_ok=True)
    # 2026-11-19 (the user: "no se ven las fundas nuevas" in the Perfil: this
    # list was fixed at 31): fundas.c's own k_arte, in its order
    import re
    src = (R / "ygo-apex" / "src" / "fundas.c").read_text(encoding="utf-8")
    blk = src[src.index("static const char *const k_arte[]"):]
    blk = re.sub(r"/\*.*?\*/", "", blk[blk.index("{") + 1: blk.index("};")], flags=re.S)
    artes = re.findall(r'"([a-z0-9_]+)"', blk)
    marcos = ["dorado", "plata", "obsidiana", "milenio", "neon"]
    for i, a in enumerate(artes, 1):
        for m, mn in enumerate(marcos):
            Image.open(FU / ("%s__%s.png" % (a, mn))).convert("RGB").resize((240, 350), Image.LANCZOS).save(
                d / ("%02d_%d.jpg" % (i, m)), quality=88)
    # the classic back (the game's card_ura, its lower part)
    ura = Image.open(R / "modding" / "texturas" / "dump" / "E86E1EE8D605D332_128x256.png").convert("RGB")
    ura.crop((0, 70, 128, 256)).resize((240, 350), Image.LANCZOS).save(d / "00_0.jpg", quality=90)


SOL_CUADROS = 12


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    boton(False).save(OUT / "boton.png")
    boton(True).save(OUT / "boton_luz.png")
    # 2026-11-16 (the user: "no podes hacer el efecto de que gire asi?"): the
    # rays turn as the wheel's chosen icon's (video: ~110 deg/s, against the
    # clock; the medallion and its stars still). One ray step (22.5 deg) in
    # SOL_CUADROS pictures one under the other; perfil.c steps through them
    g = boton(False)
    oro = dorar(g)
    tira = Image.new("RGBA", (g.width, g.height * SOL_CUADROS), (0, 0, 0, 0))
    for k in range(SOL_CUADROS):
        tira.paste(sol(oro, giro=-k / SOL_CUADROS), (0, k * g.height))
    tira.save(OUT / "boton_sol.png")
    fondo().save(OUT / "fondo.png")
    placa(PLACA_NOMBRE[2], PLACA_NOMBRE[3]).save(OUT / "placa_nombre.png")   # at (cx - w/2 - 4, top - 4) ref px
    flecha(False, True).save(OUT / "flecha_izq_luz.png")
    flecha(True, True).save(OUT / "flecha_der_luz.png")
    # 2026-11-16 (the tapete's arrows by its box, drawn by perfil.c): unlit too
    flecha(False, False).save(OUT / "flecha_izq.png")
    flecha(True, False).save(OUT / "flecha_der.png")
    foto_luz().save(OUT / "foto_luz.png")
    foto_luz(MASC[2], MASC[3]).save(OUT / "masc_luz.png")
    funda_luz().save(OUT / "funda_luz.png")
    # 2026-11-19: the arrows, option D (strips of FLECHA_D_CUADROS frames)
    for der, nom in ((False, "izq"), (True, "der")):
        flecha_d(der, False).save(OUT / ("flecha_d_%s.png" % nom))
        flecha_d(der, True).save(OUT / ("flecha_d_%s_luz.png" % nom))
    fundas()
    # 2026-11-16 (the user: "imitar cuando entra en cualquier menu en nuestro
    # perfil (tendrias que hacer un fondo completo que tape todo)"): the
    # menu's doors (perfil/puertas_menu.png: the user's video, the doors shut,
    # 1920 x 1080, their seam at column 961) as two halves; and the game's
    # menu background (dump FDE76864287C3300: 640 x 480 in a 1024 x 512, from
    # row 32) for under the panel
    p = Image.open(P / "puertas_menu.png").convert("RGB")
    p.crop((0, 0, 961, p.height)).save(OUT / "puerta_izq.jpg", quality=92)
    p.crop((961, 0, p.width, p.height)).save(OUT / "puerta_der.jpg", quality=92)
    fondo_completo().save(OUT / "fondo_completo.jpg", quality=94)
    print("ok", Image.open(OUT / "boton.png").size, Image.open(OUT / "fondo.png").size)


if __name__ == "__main__":
    main()
