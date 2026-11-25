"""Tapetes (2026-11-16, the user: "capaz podriamos poner Tapetes ... como
master duel"): the duel board under your cards with a monster's art.

The board is one game texture (BFBD8339E5E8D7E0, 512 x 128, here its 4x
upscale from modding/texturas/mejoradas): one player's half, its gold frames
and slots over a see-through brown. The game draws it for both halves; the
kit (nv2a_tapete) draws your half (below the frame's row 200) with the
tapete. Your half shows the texture upside down, so each art is put in
upside down here. The frames stay as they are; the brown becomes the art:
sharp in the middle, stretched, blurred and darker to the sides, darker
again inside the slots so the cards read.

Out: modding/texturas/tapetes/juego/<name>.png (2048 x 512, straight
alpha, the texture's own layout) and <name>_vista.jpg (the Perfil's box,
660 x 480: the tapete's middle the right way up). "clasico_vista.jpg" is
the game's own. Names and order: fundas.c k_arte (index 0 = Clasico).

  py -3 tools/tapetes.py
"""
import pathlib

import numpy as np
from PIL import Image, ImageEnhance, ImageFilter

R = pathlib.Path(__file__).resolve().parent.parent
T = R / "modding" / "texturas"
OUT = T / "tapetes" / "juego"
BASE = T / "mejoradas" / "upscayl_png_digital-art-4x_4x" / "BFBD8339E5E8D7E0_512x128.png"

# fundas.c k_arte (1..), and where the subject's middle is, from the top (0..1)
ARTES = [("exodia", 0.30), ("ojos_azules", 0.40), ("mago_oscuro", 0.38), ("ojos_rojos", 0.42),
         ("chica_maga", 0.40), ("kuriboh", 0.50), ("craneo", 0.40), ("soldado_brillo", 0.38),
         ("definitivo", 0.40), ("jinzo", 0.35), ("arpia", 0.38), ("dragon_barril", 0.45),
         ("pescador", 0.40), ("rey_rex", 0.40), ("gran_polilla", 0.40), ("mil_ojos", 0.45),
         ("necrofear", 0.40), ("maha_vailo", 0.38), ("bestia_enmascarada", 0.38), ("guardian_celta", 0.38),
         ("maquina_tragamonedas", 0.45), ("dragon_zombi", 0.40), ("golem_milenio", 0.40), ("espadachin", 0.38),
         ("princesa_fuego", 0.38), ("elfa_mistica", 0.38), ("dokurorider", 0.40), ("dunames", 0.38),
         ("guerrero_pizarra", 0.40), ("gusano_humanoide", 0.42), ("tomate", 0.45)]
# 2026-11-19: the 38 mascots' (fundas.c k_arte order), lighter (LUZ_NUEVAS: the
# user, on the samples: "los noto un poco oscuros"; approved lighter)
NUEVAS = ["gaia", "mago_tiempo", "dragon_bebe", "espadachin_llama", "dragon_craneo_negro", "mago_caos_negro", "dragon_meteoro_negro", "valkyrion", "dragon_toon", "gaia_campeon", "sabio_oscuro", "dragon_trueno", "dragon_tres_cuernos", "reina_cosmos", "zera", "caballero_craneo", "dragon_mascota_harpie", "guardian_puerta", "renunciado", "craneo_toon", "maldicion_dragon", "mil_dragones", "alfa", "beta", "gamma", "dragon_meteoro", "bruja_bosque", "sangan", "gardna", "payaso_zombi", "fantasma", "mago_fe", "legamo_humanoide", "hermanas_arpia", "sanga", "kazejin", "suijin", "vorse", "mago_oscuro_2",
          "goblin", "elfas_gemelas", "santa_juana", "zoa", "melchid", "bickuribox", "elfo_oscuro", "tortuga_cangrejo", "espada_garma", "escarabajo_jabalina", "tanque_laberinto", "ballena_fortaleza", "dragon_nocturno", "chakra", "megacyber", "bicho_devorador", "jarra_cibernetica", "parshath", "soldado_pinguino", "jarra_cambiante", "muka_muka", "soldado_piedra", "muro_laberinto", "ninja_armado", "aprendiz_bruja", "gilasaurus", "battleguard_lava", "seiyaryu", "guerrero_pantera", "jirai_gumo"]
ARTES += [(a, 0.42) for a in NUEVAS]
LUZ_NUEVAS = 1.2


def arte(nombre):
    if nombre == "exodia":
        im = Image.open(T / "fundas" / "exodia.png").convert("RGB")
        lado = min(im.size)
        return im.crop(((im.width - lado) // 2, 0, (im.width - lado) // 2 + lado, lado))
    return Image.open(T / "fundas" / "arte" / (nombre + ".jpg")).convert("RGB")


def fondo(a, W, H, cy, claro=False):
    lado = a.resize((W, W), Image.LANCZOS)
    top = int(W * cy - H / 2)
    lado = lado.crop((0, top, W, top + H)).filter(ImageFilter.GaussianBlur(18))
    lado = ImageEnhance.Brightness(lado).enhance(0.85 if claro else 0.6)
    c = a.resize((int(H * 1.6), int(H * 1.6)), Image.LANCZOS)
    t2 = max(0, int(c.height * cy - H / 2))
    c = c.crop((0, t2, c.width, t2 + H))
    xs = np.linspace(-1, 1, c.width)
    mk = np.tile(np.clip((1 - np.abs(xs)) * 2.2, 0, 1), (c.height, 1))
    lado.paste(c, ((W - c.width) // 2, 0), Image.fromarray((mk * 255).astype(np.uint8)))
    return np.asarray(lado, np.float32)


def tapete(B, nombre, cy):
    H, W = B.shape[:2]
    claro = nombre in NUEVAS
    art = fondo(arte(nombre), W, H, cy, claro)[::-1]       # your half shows the texture upside down
    rgb, al = B[..., :3], B[..., 3:]
    lum = rgb.mean(-1, keepdims=True)
    oro = np.clip((rgb[..., :1] - 115) / 70.0, 0, 1)
    k = np.clip(lum / 55.0, 0.8 if claro else 0.55, 1.15) * (LUZ_NUEVAS if claro else 1.0)
    out = oro * rgb + (1 - oro) * art * k * 0.95
    a = al                                          # the original's see-through (2026-11-16, the user)
    return Image.fromarray(np.concatenate([out, a], -1).clip(0, 255).astype(np.uint8), "RGBA")


def vista(im):
    """the Perfil's box: the tapete's middle the right way up, on dark"""
    up = im.transpose(Image.FLIP_TOP_BOTTOM)
    W, H = up.size
    w = int(H * 660 / 480)
    c = up.crop(((W - w) // 2, 0, (W - w) // 2 + w, H))
    bg = Image.new("RGBA", c.size, (24, 16, 6, 255))
    bg.alpha_composite(c)
    return bg.convert("RGB").resize((660, 480), Image.LANCZOS)


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    base = Image.open(BASE).convert("RGBA")
    B = np.asarray(base, np.float32)
    vista(base.transpose(Image.FLIP_TOP_BOTTOM)).save(OUT / "clasico_vista.jpg", quality=90)
    for nombre, cy in ARTES:
        t = tapete(B, nombre, cy)
        t.save(OUT / (nombre + ".png"), optimize=True)
        vista(t).save(OUT / (nombre + "_vista.jpg"), quality=90)
    print("tapetes:", len(ARTES))


if __name__ == "__main__":
    main()
