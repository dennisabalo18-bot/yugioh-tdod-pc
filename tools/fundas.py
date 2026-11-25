"""Sleeves (idea 13, 2026-11-16): an art and a frame, each chosen apart.

Arts: the user's Exodia (modding/texturas/fundas/exodia.png, 1038 x 1516, in
its own gold frame: its window is the art) and the cards' art
(fundas/arte/*.jpg, 624 x 624, images.ygoprodeck.com/images/cards_cropped/,
downloaded with the user's OK). A square art either fills the window cut
upright ("corte") or, when the cut would lose the monster (the user: Mago
Oscuro and Ojos Rojos "salen cortados"), sits whole across the window's
width over its own blurred, enlarged copy ("entera").

Frames (5, the user: "5 marcos diferentes, que se tengan que poder elegir"):
dorado (Exodia's own carved gold), plata, obsidiana, milenio (dark with the
Millennium Eye from the duel atlas at the corners), neon (thin glowing
Master Duel line).

    py -3 tools/fundas.py   -> fundas/<art>__<frame>.png, fundas/todas.png
"""
import pathlib

import numpy as np
from PIL import Image, ImageDraw, ImageEnhance, ImageFilter, ImageOps

PORT = pathlib.Path(__file__).resolve().parent.parent
DIR = PORT / "modding" / "texturas" / "fundas"
W, H = 1038, 1516
VENTANA = (86, 86, 968, 1432)            # the art's window (measured on exodia.png)
# how each square art goes in: ("corte", centre 0..1) or ("entera",)
COMO = {"ojos_azules": ("corte", 0.52), "mago_oscuro": ("corte", 0.42), "ojos_rojos": ("corte", 0.35),
        "chica_maga": ("corte", 0.42), "kuriboh": ("corte", 0.5), "craneo": ("corte", 0.5),
        "soldado_brillo": ("corte", 0.5), "definitivo": ("corte", 0.45),
        # 2026-11-16 (the user: "la de los personajes clasicos de cada uno como
        # hicimos con las mascotas"): each opponent's mascot card, 15 more
        "jinzo": ("corte", 0.5), "arpia": ("corte", 0.5), "dragon_barril": ("corte", 0.5),
        "pescador": ("corte", 0.5), "rey_rex": ("corte", 0.68), "gran_polilla": ("corte", 0.5),
        "mil_ojos": ("corte", 0.5), "necrofear": ("corte", 0.5), "maha_vailo": ("corte", 0.5),
        "bestia_enmascarada": ("corte", 0.5), "guardian_celta": ("corte", 0.5),
        "maquina_tragamonedas": ("corte", 0.5), "dragon_zombi": ("corte", 0.5),
        "golem_milenio": ("corte", 0.5), "espadachin": ("corte", 0.5),
        # 2026-11-16 (the user: "acomoda la mascota de todos", from each
        # opponent's own deck in the game's reward lists): 7 more
        "princesa_fuego": ("corte", 0.42), "elfa_mistica": ("corte", 0.5), "dokurorider": ("corte", 0.55),
        "dunames": ("corte", 0.5), "guerrero_pizarra": ("corte", 0.5), "gusano_humanoide": ("corte", 0.48),
        "tomate": ("corte", 0.5),
        # 2026-11-19 (the user: "el combo completo", every mascot its sleeve and
        # tapete): the 38 mascots without one (arte_mascotas/mNN.jpg, ygoprodeck)
        "gaia": ("corte", 0.5),
        "mago_tiempo": ("corte", 0.5),
        "dragon_bebe": ("corte", 0.5),
        "espadachin_llama": ("corte", 0.5),
        "dragon_craneo_negro": ("corte", 0.62),
        "mago_caos_negro": ("corte", 0.5),
        "dragon_meteoro_negro": ("corte", 0.3),
        "valkyrion": ("corte", 0.5),
        "dragon_toon": ("corte", 0.3),
        "gaia_campeon": ("corte", 0.5),
        "sabio_oscuro": ("corte", 0.5),
        "dragon_trueno": ("corte", 0.5),
        "dragon_tres_cuernos": ("corte", 0.5),
        "reina_cosmos": ("corte", 0.5),
        "zera": ("corte", 0.5),
        "caballero_craneo": ("corte", 0.5),
        "dragon_mascota_harpie": ("corte", 0.5),
        "guardian_puerta": ("corte", 0.5),
        "renunciado": ("corte", 0.5),
        "craneo_toon": ("corte", 0.5),
        "maldicion_dragon": ("corte", 0.5),
        "mil_dragones": ("corte", 0.35),
        "alfa": ("corte", 0.5),
        "beta": ("corte", 0.5),
        "gamma": ("corte", 0.5),
        "dragon_meteoro": ("corte", 0.25),
        "bruja_bosque": ("corte", 0.5),
        "sangan": ("corte", 0.5),
        "gardna": ("corte", 0.5),
        "payaso_zombi": ("corte", 0.5),
        "fantasma": ("corte", 0.5),
        "mago_fe": ("corte", 0.5),
        "legamo_humanoide": ("corte", 0.5),
        "hermanas_arpia": ("corte", 0.5),
        "sanga": ("corte", 0.5),
        "kazejin": ("corte", 0.5),
        "suijin": ("corte", 0.5),
        "vorse": ("corte", 0.5),
        "mago_oscuro_2": ("corte", 0.45),
        # 2026-11-19: the 30 new mascots (70 -> 100)
        "goblin": ("corte", 0.5),
        "elfas_gemelas": ("corte", 0.5),
        "santa_juana": ("corte", 0.5),
        "zoa": ("corte", 0.3),
        "melchid": ("corte", 0.5),
        "bickuribox": ("corte", 0.5),
        "elfo_oscuro": ("corte", 0.5),
        "tortuga_cangrejo": ("corte", 0.5),
        "espada_garma": ("corte", 0.5),
        "escarabajo_jabalina": ("corte", 0.5),
        "tanque_laberinto": ("corte", 0.5),
        "ballena_fortaleza": ("corte", 0.5),
        "dragon_nocturno": ("corte", 0.25),
        "chakra": ("corte", 0.5),
        "megacyber": ("corte", 0.5),
        "bicho_devorador": ("corte", 0.5),
        "jarra_cibernetica": ("corte", 0.4),
        "parshath": ("corte", 0.5),
        "soldado_pinguino": ("corte", 0.5),
        "jarra_cambiante": ("corte", 0.5),
        "muka_muka": ("corte", 0.5),
        "soldado_piedra": ("corte", 0.5),
        "muro_laberinto": ("corte", 0.5),
        "ninja_armado": ("corte", 0.5),
        "aprendiz_bruja": ("corte", 0.5),
        "gilasaurus": ("corte", 0.5),
        "battleguard_lava": ("corte", 0.5),
        "seiyaryu": ("corte", 0.3),
        "guerrero_pantera": ("corte", 0.6),
        "jirai_gumo": ("corte", 0.5),   # 2026-11-19: Dark Magician II (card 5165 = ygoprodeck 36996508)
        }
NUEVAS2 = ("princesa_fuego", "elfa_mistica", "dokurorider", "dunames", "guerrero_pizarra",
           "gusano_humanoide", "tomate")
NUEVAS = ("jinzo", "arpia", "dragon_barril", "pescador", "rey_rex", "gran_polilla", "mil_ojos",
          "necrofear", "maha_vailo", "bestia_enmascarada", "guardian_celta", "maquina_tragamonedas",
          "dragon_zombi", "golem_milenio", "espadachin")
MARCOS = ("dorado", "plata", "obsidiana", "milenio", "neon")


def ventana_de(nombre):
    x0, y0, x1, y1 = VENTANA
    w, h = x1 - x0, y1 - y0
    if nombre == "exodia":
        return Image.open(DIR / "exodia.png").convert("RGB").crop(VENTANA)
    arte = Image.open(DIR / "arte" / (nombre + ".jpg")).convert("RGB")
    como = COMO.get(nombre, ("corte", 0.5))
    if como[0] == "corte":
        ancho = int(arte.size[1] * w / h)
        c = int(como[1] * arte.size[0])
        izq = max(0, min(arte.size[0] - ancho, c - ancho // 2))
        v = arte.crop((izq, 0, izq + ancho, arte.size[1])).resize((w, h), Image.LANCZOS)
    else:
        # the whole art across the width, over itself blurred and darker
        fondo = ImageOps.fit(arte, (w, h), Image.LANCZOS).filter(ImageFilter.GaussianBlur(28))
        fondo = ImageEnhance.Brightness(fondo).enhance(0.55)
        a = arte.resize((w, w), Image.LANCZOS)
        m = Image.new("L", (w, w), 255)
        dm = ImageDraw.Draw(m)
        for k in range(60):                       # soft top and bottom edges
            dm.line((0, k, w, k), fill=int(255 * k / 60))
            dm.line((0, w - 1 - k, w, w - 1 - k), fill=int(255 * k / 60))
        fondo.paste(a, (0, (h - w) // 2), m)
        v = fondo
    return v.filter(ImageFilter.UnsharpMask(radius=2, percent=60, threshold=2))


def mascara_marco():
    """255 on the frame, 0 in the window (with Exodia's 6 px inner bevel kept)."""
    m = Image.new("L", (W, H), 255)
    x0, y0, x1, y1 = VENTANA
    ImageDraw.Draw(m).rectangle((x0 + 6, y0 + 6, x1 - 6, y1 - 6), fill=0)
    return m


def marco(nombre):
    ex = Image.open(DIR / "exodia.png").convert("RGB")
    if nombre == "dorado":
        return ex
    if nombre == "plata":
        g = ImageOps.grayscale(ex).convert("RGB")
        g = ImageEnhance.Contrast(ImageEnhance.Brightness(g).enhance(1.25)).enhance(1.25)
        return Image.blend(g, Image.new("RGB", g.size, (200, 215, 235)), 0.12)
    if nombre == "obsidiana":
        a = np.asarray(ex).astype(np.float32) / 255
        lum = a.mean(-1, keepdims=True)
        piedra = np.array([0.10, 0.09, 0.12]) + lum * np.array([0.25, 0.22, 0.30])
        lineas = np.clip((lum - 0.55) * 4, 0, 1)          # the carving's highlights stay gold
        out = piedra * (1 - lineas) + a * lineas
        return Image.fromarray((np.clip(out, 0, 1) * 255).astype(np.uint8))
    if nombre == "milenio":
        base = Image.new("RGB", (W, H), (32, 10, 48))
        d = ImageDraw.Draw(base)
        for k, c in ((0, (210, 165, 60)), (14, (120, 80, 30)), (22, (230, 190, 80))):
            d.rectangle((k, k, W - 1 - k, H - 1 - k), outline=c, width=6)
        x0, y0, x1, y1 = VENTANA
        for k, c in ((0, (230, 190, 80)), (8, (120, 80, 30))):
            d.rectangle((x0 - 14 + k, y0 - 14 + k, x1 + 14 - k, y1 + 14 - k), outline=c, width=5)
        ojo = Image.open(PORT / "modding" / "texturas" / "idiomas" / "es" / "82D3B7DD2E4DECF3_1024x512.png")
        ojo = ojo.convert("RGBA").transpose(Image.FLIP_TOP_BOTTOM)
        k = ojo.size[0] / 2000
        ojo = ojo.crop((int(860 * k), int(560 * k), int(1180 * k), int(850 * k)))   # the eye itself
        a = np.asarray(ojo).astype(np.float32)
        lum = a[..., :3].mean(-1)
        oro = np.zeros_like(a)
        oro[..., 0], oro[..., 1], oro[..., 2] = 235, 190, 80
        oro[..., 3] = np.clip((90 - lum) * 4, 0, 255)       # the dark eye lines, in gold
        ojo = Image.fromarray(oro.astype(np.uint8), "RGBA").resize((150, 136), Image.LANCZOS)
        b = base.convert("RGBA")
        for (x, y) in ((W // 2 - 75, 0), (W // 2 - 75, H - 120)):
            b.alpha_composite(ojo, (x, y - 8 if y else y))
        return b.convert("RGB")
    if nombre == "neon":
        base = Image.new("RGB", (W, H), (8, 14, 26))
        glow = Image.new("RGB", (W, H), (0, 0, 0))
        d = ImageDraw.Draw(glow)
        x0, y0, x1, y1 = VENTANA
        ch = 40
        def chaflan(m):
            return [(x0 - m + ch, y0 - m), (x1 + m - ch, y0 - m), (x1 + m, y0 - m + ch), (x1 + m, y1 + m - ch),
                    (x1 + m - ch, y1 + m), (x0 - m + ch, y1 + m), (x0 - m, y1 + m - ch), (x0 - m, y0 - m + ch)]
        d.line(chaflan(20) + [chaflan(20)[0]], fill=(60, 220, 255), width=10)
        d.line(chaflan(48) + [chaflan(48)[0]], fill=(40, 120, 200), width=4)
        g = glow.filter(ImageFilter.GaussianBlur(14))
        out = Image.fromarray(np.clip(np.asarray(base, np.int32) + np.asarray(g, np.int32) * 2
                                      + np.asarray(glow, np.int32), 0, 255).astype(np.uint8))
        return out
    raise ValueError(nombre)


def funda(arte, marco_img):
    out = Image.new("RGB", (W, H))
    out.paste(ventana_de(arte), VENTANA[:2])
    out.paste(marco_img, (0, 0), mascara_marco())
    return out


if __name__ == "__main__":
    marcos = {m: marco(m) for m in MARCOS}
    artes = ["exodia"] + [n for n in COMO if (DIR / "arte" / (n + ".jpg")).exists()]
    for a in artes:
        for m in MARCOS:
            funda(a, marcos[m]).save(DIR / f"{a}__{m}.png")
    # sheet: the 5 frames on two arts, and every art in gold
    tw = 240
    th = int(tw * H / W)
    filas = [[("exodia", m) for m in MARCOS], [("mago_oscuro", m) for m in MARCOS],
             [(a, "dorado") for a in artes[:5]], [(a, "dorado") for a in artes[5:]]]
    hoja = Image.new("RGB", (5 * (tw + 16) + 16, len(filas) * (th + 16) + 16), (24, 20, 26))
    for r, fila in enumerate(filas):
        for c, (a, m) in enumerate(fila):
            im = Image.open(DIR / f"{a}__{m}.png").resize((tw, th), Image.LANCZOS)
            hoja.paste(im, (16 + c * (tw + 16), 16 + r * (th + 16)))
    hoja.save(DIR / "todas.png")
    # the opponents' 15, each with the whole art beside it (what the cut keeps)
    hoja = Image.new("RGB", (5 * (tw + 16) + 16, 3 * (th + 16) + 16), (24, 20, 26))
    for i, a in enumerate(NUEVAS):
        im = Image.open(DIR / f"{a}__dorado.png").resize((tw, th), Image.LANCZOS)
        hoja.paste(im, (16 + i % 5 * (tw + 16), 16 + i // 5 * (th + 16)))
    hoja.save(DIR / "rivales.png")
    hoja = Image.new("RGB", (4 * (tw + 16) + 16, 2 * (th + 16) + 16), (24, 20, 26))
    for i, a in enumerate(NUEVAS2):
        im = Image.open(DIR / f"{a}__dorado.png").resize((tw, th), Image.LANCZOS)
        hoja.paste(im, (16 + i % 4 * (tw + 16), 16 + i // 4 * (th + 16)))
    hoja.save(DIR / "rivales2.png")
    # the game's pieces (ygo-apex/src/fundas.c puts them together): each art
    # and each frame apart, at the card's size in the 4x card back texture
    # (512 x 740: card_ura.tga's card is 128 x 185), the frame with its window
    # see-through; copiar_al_juego.sh takes fundas/juego/ to Mods-Extras\fundas\
    JW, JH = 512, 740
    jue = DIR / "juego"
    (jue / "arte").mkdir(parents=True, exist_ok=True)
    (jue / "marco").mkdir(parents=True, exist_ok=True)
    for a in artes:
        lienzo = Image.new("RGB", (W, H))
        lienzo.paste(ventana_de(a), VENTANA[:2])
        lienzo.resize((JW, JH), Image.LANCZOS).save(jue / "arte" / (a + ".png"))
    for m in MARCOS:
        rgba = marcos[m].convert("RGBA")
        rgba.putalpha(mascara_marco())
        rgba.resize((JW, JH), Image.LANCZOS).save(jue / "marco" / (m + ".png"))
    print(len(artes), "artes x", len(MARCOS), "marcos")
