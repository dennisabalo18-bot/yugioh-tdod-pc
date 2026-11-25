"""Reescribe nitidos el nombre, el tipo, el ATK/DEF y la descripcion de las
cartas grandes (128x256) ya escaladas: la IA deforma esas letras chicas, y en
la original la descripcion mide 2-3 pixeles (ningun escalador la recupera).

Para cada carta volcada (modding/texturas/dump/HASH_128x256.png) que ya tiene
su escalada en modding/texturas/mejoradas/:
- la identifica comparandola con euro/sp/card/normal/*/ID.tga del juego
  (el volcado esta dado vuelta en vertical respecto del .tga);
- saca sus datos de default.xbe:
  * la lista de nombres empieza en " " + "Dragon blanco azul - ojos" (1139
    entradas, indice k) y las descripciones siguen a continuacion (1139 + k);
  * la tabla uint16 en el archivo 0x5EECB2 da el ID de imagen de cada k
    (el ID NO es 4006 + k);
  * la tabla uint32 en 0x5EF598 (por k): DEF/10 bits 0-8, ATK/10 bits 9-17,
    clase bits 18-19, tipo bits 20-24 (indice en la lista "Monstruo",
    "Dragon", "Zombi"...: 21 trampa, 22 magica), nivel/atributo bits 25-31;
  * "%cNNNN" en el texto es el nombre de la carta con imagen NNNN;
- tapa cada texto y lo escribe de nuevo con una fuente real;
- guarda el resultado en modding/texturas/mejoradas/texto_cartas/ con el
  mismo nombre; crear_pack_texturas.bat prefiere esa carpeta.

Zonas (pixeles de la carta original, orientacion del .tga):
  nombre x 12..101, y 11..23 (a la derecha el icono del atributo)
  monstruo: tipo y 142..147, descripcion x 12..116 y 148..167, ATK/DEF y 169..173
  magica/trampa: [CARTA ...] y 26..34, descripcion x 12..116 y 142..173

    py -3 tools/texto_cartas.py [carpeta_del_juego]
"""
import functools
import math
import pathlib
import re
import struct
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

PORT = pathlib.Path(__file__).resolve().parent.parent
ROOT = PORT / "modding" / "texturas"
GAME = PORT.parent / "Yu-Gi-Oh TDOD"
OUT = ROOT / "mejoradas" / "texto_cartas"
# 2026-11-19 (el usuario: digital-art-4x borra la textura fina del dibujo:
# la piedra del Mago oscuro, la piel del Idolo mil ojos; eligio remacri-4x):
# el recuadro del dibujo sale de remacri-4x. Aca solo ese recuadro, x4, en la
# orientacion del volcado (HASH.png, 368 x 368): tools/dibujos_remacri.py.
# DIBUJO_OTRO: las cartas que el usuario prefiera con otro modelo (huella -> carpeta).
DIBUJOS = ROOT / "dibujos_remacri"
DIBUJO_OTRO = {
    "ED0B4851B3117253": ROOT / "dibujos_ultramix",   # Sengenjin (4709): el usuario prefirio ultramix-balanced-4x
}
# recuadro del dibujo en el .tga: x 18..109, y 40..131 -> volcado (dado vuelta) y 124..215
DIBUJO_CAJA = (18, 124, 110, 216)
DIBUJO_FUNDIDO = 3                       # px de la carta original
FONT = pathlib.Path(r"C:\Windows\Fonts\calibri.ttf")
BOLD = pathlib.Path(r"C:\Windows\Fonts\arialbd.ttf")
KANJI = pathlib.Path(r"C:\Windows\Fonts\YuGothB.ttc")
NAMES = 1139
TABLE = 0x5EECB2
STATS = 0x5EF598
INK = (35, 28, 22)
NEGRO = (10, 8, 6)
# clase (bits 18-19): monstruo 0 normal 1 efecto 2 fusion 3 ritual.
# Magica: 3 ritual; 1 equipo O campo (Bosque, Umi... y Espada legendaria
# comparten el 1); 2 mezcla continuas, rapidas y normales (Peaje, Tifon,
# Dados garbosos): ahi solo [CARTA MAGICA]. Trampa: 2 continua.
MON_SUFIJO = {0: "", 1: "/EFECTO", 2: "/FUSIÓN", 3: "/RITUAL"}


def etiqueta(c):
    d = c["desc"].lower()
    if c["tipo"] == 21:
        return "[CARTA DE TRAMPA CONTINUA]" if c["clase"] == 2 else "[CARTA DE TRAMPA]"
    if c["clase"] == 3:
        return "[CARTA MÁGICA DE RITUAL]"
    if c["clase"] == 1:
        # campo: "Aumenta ... de todos los monstruos" (Bosque, Umi, Coro del
        # asilo); Anillo del magnetismo y Hoja de relampago (equipo) nombran
        # "todos los monstruos" pero no empiezan con "Aumenta"
        if (d.startswith("aumenta") and "todos los monstruos" in d) \
                or "puerta de fusi" in c["nombre"].lower() \
                or "considerado como" in d:
            return "[CARTA MÁGICA DE CAMPO]"
        return "[CARTA MÁGICA DE EQUIPO]"
    return "[CARTA MÁGICA]"


def textos(xbe):
    """ID de imagen -> dict(nombre, desc, tipo, tipo_txt, atk, df, clase)."""
    return textos_bytes(xbe.read_bytes())


def textos_bytes(d):
    """textos() con el default.xbe ya leido."""
    s = d.find(b"\x00 \x00Drag\xf3n blanco azul") + 1
    if s <= 0:
        raise SystemExit("No encuentro la lista de nombres en default.xbe")
    parts = [p.decode("latin-1") for p in d[s:s + 800000].split(b"\x00")]
    ids = [struct.unpack_from("<H", d, TABLE + 2 * i)[0] for i in range(NAMES)]
    if ids[1:4] != [4007, 4008, 4009]:
        raise SystemExit("La tabla carta -> imagen no esta donde se esperaba")
    t0 = parts.index("Monstruo")
    tipos = parts[t0:t0 + 24]
    nombre = {cid: parts[k] for k, cid in enumerate(ids) if k}
    out = {}
    for k, cid in enumerate(ids):
        if not k:
            continue
        t = parts[NAMES + k].replace("\n", " ")
        t = re.sub(r"%c(\d{4})", lambda m: nombre.get(int(m.group(1)), m.group(0)), t)
        w = struct.unpack_from("<I", d, STATS + 4 * k)[0]
        tp = (w >> 20) & 31
        out[cid] = dict(nombre=parts[k], desc=" ".join(t.split()), tipo=tp,
                        tipo_txt=tipos[tp] if tp < len(tipos) else "",
                        atk=((w >> 9) & 511) * 10, df=(w & 511) * 10, clase=(w >> 18) & 3, atrib=w >> 29,
                        nivel=(w >> 25) & 15)
    return out


def tipos(d):
    """Los nombres de los tipos ("Monstruo", "Dragón"...: el indice de los bits 20-24)."""
    s = d.find(b"\x00 \x00Drag\xf3n blanco azul") + 1
    parts = [p.decode("latin-1") for p in d[s:s + 800000].split(b"\x00")]
    t0 = parts.index("Monstruo")
    return parts[t0:t0 + 24]


def es_monstruo(a):
    """El monstruo tiene la linea de ATK/DEF: una raya oscura en y 168."""
    g = a.mean(axis=2)
    return g[168, 14:110].mean() < g[160, 14:110].mean() - 25


def partir(texto, font, ancho):
    lineas, cur = [], ""
    for w in texto.split():
        t = (cur + " " + w).strip()
        if font.getlength(t) <= ancho or not cur:
            cur = t
        else:
            lineas.append(cur)
            cur = w
    if cur:
        lineas.append(cur)
    return lineas


def rellenar(img, box):
    """Tapa el rectangulo: cada renglon toma el color de su mitad mas clara
    (el fondo; las letras son oscuras). Los bordes del recuadro son oscuros,
    por eso no se toma su color (2026-11-08: dejaban una mancha)."""
    X0, Y0, X1, Y1 = box
    a = np.asarray(img, np.float32).copy()
    r = a[Y0:Y1, X0:X1]
    lum = r[..., :3].mean(axis=2)
    filas = []
    for y in range(r.shape[0]):
        sel = lum[y] >= np.percentile(lum[y], 50)
        filas.append(np.median(r[y][sel], axis=0))
    filas = np.array(filas)
    k = max(1, r.shape[0] // 4)          # suaviza entre renglones
    filas = np.array([filas[max(0, i - k):i + k + 1].mean(axis=0) for i in range(len(filas))])
    a[Y0:Y1, X0:X1] = filas[:, None, :]
    return Image.fromarray(a.clip(0, 255).astype(np.uint8), img.mode)


def rellenar_papel(img, box, mx0, mx1):
    """Tapa el rectangulo con el papel de la misma carta en esa altura, de
    x mx0..mx1 (sin letras), repetido en espejo: mismo color y mismas manchas
    sea cual sea el color de la carta (amarilla, naranja, violeta, azul...).
    El color liso de rellenar() salia mas claro y naranja que el papel de al
    lado (captura del usuario 2026-11-10, el ATK/DEF)."""
    X0, Y0, X1, Y1 = box
    a = np.asarray(img, np.float32).copy()
    src = a[Y0:Y1, mx0:mx1]
    partes, w, espejo = [], 0, True
    while w < X1 - X0:
        partes.append(src[:, ::-1] if espejo else src)
        w += src.shape[1]
        espejo = not espejo
    a[Y0:Y1, X0:X1] = np.concatenate(partes, axis=1)[:, :X1 - X0]
    return Image.fromarray(a.clip(0, 255).astype(np.uint8), img.mode)


def limpiar_papel(img, box, k):
    """Borra las letras del rectangulo y deja el papel de esa misma carta: un
    cierre (maximo y despues minimo, por canal) se come los trazos oscuros y
    finos, y un suavizado leve le saca lo cuadrado. Queda el tono y el degrade
    del papel de alrededor, no un color liso: el liso de la descripcion y del
    tipo se veia de otro tono, como un sticker (usuario, 2026-11-12, en las
    cartas en ingles; igual que el ATK/DEF el 2026-11-10)."""
    X0, Y0, X1, Y1 = box
    m = 4 * k                           # margen: el cierre ve el papel de alrededor
    caja = (max(0, X0 - m), max(0, Y0 - m), min(img.size[0], X1 + m), min(img.size[1], Y1 + m))
    r = img.crop(caja).convert("RGB")
    t = 4 * k + 1                       # impar, ~ el grueso de una letra
    r = r.filter(ImageFilter.MaxFilter(t)).filter(ImageFilter.MinFilter(t))
    r = r.filter(ImageFilter.GaussianBlur(k))
    out = img.copy()
    out.paste(r.crop((X0 - caja[0], Y0 - caja[1], X1 - caja[0], Y1 - caja[1])), (X0, Y0))
    return out


def caber(texto, fuente, maximo, ancho):
    for size in range(maximo, 7, -1):
        font = ImageFont.truetype(str(fuente), size)
        if font.getlength(texto) <= ancho:
            break
    return font


def angosto(texto, font, color, sq):
    """El texto en una capa propia, angostada al factor sq (como hace el juego
    con los nombres largos: letras mas angostas antes que mas chicas)."""
    l, t, r, b = font.getbbox(texto)
    capa = Image.new("RGBA", (max(1, r), b + 2), (0, 0, 0, 0))
    ImageDraw.Draw(capa).text((0, 0), texto, font=font, fill=color + (255,))
    if sq < 1:
        capa = capa.resize((max(1, round(capa.size[0] * sq)), capa.size[1]), Image.LANCZOS)
    return capa


def linea(img, box, texto, fuente, maximo, color, derecha=False, papel=None):
    if papel is None:
        img = rellenar(img, box)
    elif papel is not False:      # False: ya tapado
        img = rellenar_papel(img, box, *papel)
    pad = (box[3] - box[1]) // 6
    ancho = box[2] - box[0] - 2 * pad
    font = ImageFont.truetype(str(fuente), maximo)
    sq = min(1.0, ancho / max(1, font.getlength(texto)))
    if sq < 0.6:          # muy largo: angosta hasta 0.6 y achica el resto
        font = caber(texto, fuente, maximo, ancho / 0.6)
        sq = min(1.0, ancho / max(1, font.getlength(texto)))
    capa = angosto(texto, font, color, sq)
    asc = font.getbbox("ÁHgj")
    y = (box[1] + box[3]) // 2 - (asc[1] + asc[3]) // 2
    x = box[2] - pad - capa.size[0] if derecha else box[0] + pad
    img.alpha_composite(capa, (x, max(0, y)))
    return img


# icono del atributo (arriba a la derecha): círculo de 13 px con centro en
# (109.5, 17.5) del .tga. La IA deforma el kanji; se dibuja de nuevo.
# atributo = bits 29-31 de la tabla (1 luz 2 oscuridad 3 agua 4 fuego
# 5 tierra 6 viento); mágicas y trampas tienen el suyo. Colores medidos en
# los originales: (arriba, abajo, kanji).
ATRIB = {
    # luz: como la imagen del usuario (2026-11-12): disco marrón oscuro, aro
    # dorado que brilla y kanji blanco (6º valor = color del aro)
    1: ("光", (92, 70, 46), (24, 19, 13), (255, 255, 255), (40, 30, 18), (250, 236, 110)),
    2: ("闇", (205, 40, 60), (128, 50, 140), (246, 226, 250)),
    3: ("水", (52, 190, 240), (12, 112, 172), (255, 255, 255)),
    4: ("炎", (244, 70, 52), (196, 24, 24), (255, 240, 232)),
    5: ("地", (64, 64, 68), (18, 18, 20), (222, 222, 228)),
    6: ("風", (96, 186, 92), (40, 108, 46), (236, 255, 232)),
    "magia": ("魔", (110, 196, 228), (28, 104, 140), (255, 255, 255)),
    "trampa": ("罠", (208, 50, 40), (44, 44, 120), (255, 255, 255)),
}
ICONO_C, ICONO_R = (109.5, 17.5), 6.5


def icono(img, c, k):
    clave = "trampa" if c["tipo"] == 21 else "magia" if c["tipo"] == 22 else c["atrib"]
    if clave not in ATRIB:
        return
    kan, arriba, abajo, tinta = ATRIB[clave][:4]
    borde = ATRIB[clave][4] if len(ATRIB[clave]) > 4 else tuple(int(v * 0.45) for v in abajo)
    q = 4                       # se dibuja 4 veces más grande y se achica
    R = (ICONO_R + 0.45) * k    # un poco más: tapa el halo de la IA
    n = int(2 * R + 4)
    N = n * q
    yy, xx = (np.mgrid[0:N, 0:N] + 0.5) / N * 2 - 1    # -1..1
    rr = np.hypot(xx, yy) * n / (2 * R)               # 1 = borde
    t = np.clip((yy * n / (2 * R) + 1) / 2, 0, 1)[..., None]
    col = np.array(arriba, np.float32) * (1 - t) + np.array(abajo, np.float32) * t
    # brillo arriba a la izquierda y borde más oscuro: bolita como el original
    luz = np.clip(1 - np.hypot(xx + 0.3, yy + 0.4) / 0.75, 0, 1)[..., None] ** 2
    col = col + (255 - col) * luz * 0.45
    col = col * (1 - 0.35 * np.clip((rr - 0.82) / 0.18, 0, 1)[..., None])
    if len(ATRIB[clave]) > 5:   # aro: naranja por dentro, amarillo claro por fuera
        aro = np.array(ATRIB[clave][5], np.float32)
        nar = np.array((214, 120, 40), np.float32)
        w = np.clip((rr - 0.80) / 0.20, 0, 1)[..., None]
        rim = nar * (1 - w) + aro * w
        f = np.clip((rr - 0.74) / 0.10, 0, 1)[..., None]
        col = col * (1 - f) + rim * f
    a =np.clip((1 - rr) * N / 2 / q * 1.5, 0, 1) * 255   # borde suave
    disco = Image.fromarray(np.dstack([np.clip(col, 0, 255), a]).astype(np.uint8), "RGBA")
    capa = Image.new("RGBA", (N, N))
    luz = PORT / "tools" / "kanji_luz.png"
    if clave == 1 and luz.is_file():
        # luz: el 光 sacado de la imagen que pasó el usuario (2026-11-12); con
        # Yu Gothic los trazos de arriba se pegan o quedan muy finos
        m = Image.open(luz).convert("L")
        h = int(R * 2 * 0.66 * q)
        w = round(m.size[0] * h / m.size[1])
        m = m.resize((w, h), Image.LANCZOS).filter(ImageFilter.MaxFilter(5))  # que no se pierda achicado
        pos = (round(N / 2 - w / 2), round(N / 2 - h / 2 + 0.02 * N))
        capa.paste(Image.new("RGBA", (w, h), tinta + (255,)), pos, m)
    else:
        font = ImageFont.truetype(str(KANJI), int(R * 2 * 0.68 * q))
        d = ImageDraw.Draw(capa)
        d.text((N / 2, N / 2 + 0.03 * N), kan, font=font, anchor="mm", fill=tinta + (255,),
               stroke_width=max(1, int(0.045 * N)), stroke_fill=borde + (230,))
    disco.alpha_composite(capa)
    disco = disco.resize((n, n), Image.LANCZOS)
    cx, cy = ICONO_C[0] * k, ICONO_C[1] * k
    img.alpha_composite(disco, (round(cx - n / 2), round(cy - n / 2)))


# Las estrellas de nivel (2026-11-17, el usuario: la IA las dejo como
# manchas; "asi se tienen que ver": las de las cartas reales). En el .tga son
# bolitas de ~9 px: centros cada 9 px de x 107.5 hacia la izquierda, y 30.6,
# radio ~4.15. Se dibujan de nuevo: disco rojo-naranja con aro oscuro y una
# estrella amarilla de 5 puntas, del mismo rojo en todas las cartas (con el
# color de la carta las de ritual y fusion quedaban verdosas), con la luz del
# papel de esa carta, una sombra suave abajo a la derecha y el grano del papel.
EST_PASO, EST_X0, EST_Y, EST_R = 9.0, 107.5, 30.6, 4.15


@functools.lru_cache(maxsize=4)
def _estrella(k, q=6):
    n = int(2 * (EST_R + 0.6) * k) + 2
    N = n * q
    c, Rd = N / 2, EST_R * k * q
    yy, xx = np.mgrid[0:N, 0:N] + 0.5
    rr = np.hypot(xx - c, yy - c) / Rd
    d = np.hypot(xx - c + 0.35 * Rd, yy - c + 0.35 * Rd) / Rd      # luz arriba a la izquierda
    t = np.clip(d / 1.3, 0, 1)[..., None]
    col = np.array((250, 120, 30), np.float32) * (1 - t) + np.array((190, 30, 20), np.float32) * t
    ar = np.clip((rr - 0.86) / 0.06, 0, 1)[..., None]               # aro marron oscuro
    col = col * (1 - ar) + np.array((70, 22, 12), np.float32) * ar
    a = np.clip((1.0 - rr) * Rd / q + 0.5, 0, 1) * 255
    disco = Image.fromarray(np.dstack([col.clip(0, 255), a]).astype(np.uint8), "RGBA")

    def puntas(re_, ri):
        return [(c + (re_ if i % 2 == 0 else ri) * math.cos(-math.pi / 2 + i * math.pi / 5),
                 c + 0.04 * Rd + (re_ if i % 2 == 0 else ri) * math.sin(-math.pi / 2 + i * math.pi / 5))
                for i in range(10)]
    m_borde = Image.new("L", (N, N))
    ImageDraw.Draw(m_borde).polygon(puntas(0.84 * Rd, 0.34 * Rd), fill=255)
    m_rell = Image.new("L", (N, N))
    ImageDraw.Draw(m_rell).polygon(puntas(0.72 * Rd, 0.29 * Rd), fill=255)
    g = np.clip((yy - (c - 0.6 * Rd)) / (1.2 * Rd), 0, 1)[..., None]
    rell = np.array((255, 246, 120), np.float32) * (1 - g) + np.array((250, 190, 20), np.float32) * g
    disco.paste(Image.new("RGBA", (N, N), (205, 90, 10, 255)), (0, 0), m_borde)
    disco.paste(Image.fromarray(np.dstack([rell, np.full((N, N), 255)]).astype(np.uint8), "RGBA"), (0, 0), m_rell)
    disco.putalpha(Image.fromarray(a.astype(np.uint8)))
    return disco.resize((n, n), Image.LANCZOS)


def _papel_estrellas(img, k, centros):
    """Borra las manchas de la IA: solo la franja de la primera a la ultima
    estrella (una capsula algo mayor, mas ancha a la derecha donde caia la
    sombra oscura), fundida en k px. El tono, por renglon, el del papel justo
    antes de la primera y justo despues de la ultima (de uno al otro); encima
    la textura del papel de la izquierda de la misma carta. Un rectangulo
    entero dejaba un tono distinto a la derecha (usuario, 2026-11-17)."""
    izq = min(centros) - EST_R - 1.2
    X0, Y0, X1, Y1 = int(izq * k), int(25.6 * k), int(119 * k), int(36.2 * k)
    f0, f1 = 14 * k, max(15 * k, min(X0 - 2 * k, 40 * k))
    a = np.asarray(img, np.float32).copy()
    src = a[Y0:Y1, f0:f1, :3]
    tiras, w, esp = [], 0, True
    while w < X1 - X0:
        tiras.append(src[:, ::-1] if esp else src)
        w += src.shape[1]
        esp = not esp
    tex = np.concatenate(tiras, axis=1)[:, :X1 - X0]
    tex = tex - tex.mean(axis=1, keepdims=True)
    if f1 - f0 < 6 * k:         # nivel 10-12: la tira cae bajo las estrellas viejas (manchas rojas)
        tex = tex * 0
    yy, xx = np.mgrid[Y0:Y1, X0:X1] + 0.5
    x0c, x1c = min(centros) * k, (max(centros) + 0.6) * k
    dx = np.maximum(np.maximum(0, x0c - xx) / 1.12, np.maximum(0, xx - x1c) / 1.35)
    dd = np.hypot(dx, yy - EST_Y * k) / k
    f = np.clip(EST_R + 1.6 - dd, 0, 1)[..., None]
    xl, xr = int(x0c - (EST_R + 2.3) * k), int(x1c + (EST_R + 2.3) * 1.12 * k)
    R = a[Y0:Y1, xr:xr + 2 * k, :3].mean(axis=1)
    # nivel 11-12: la primera estrella llega al marco (x ~5), no hay papel a
    # su izquierda: todo con el tono de la derecha
    L = a[Y0:Y1, xl - 2 * k:xl, :3].mean(axis=1) if xl - 2 * k >= 8 * k else R
    u = np.clip((xx[0] - xl) / max(1, xr - xl), 0, 1)[None, :, None]
    nuevo = L[:, None, :] * (1 - u) + R[:, None, :] * u + tex * 0.6
    a[Y0:Y1, X0:X1, :3] = a[Y0:Y1, X0:X1, :3] * (1 - f) + nuevo * f
    return Image.fromarray(a.clip(0, 255).astype(np.uint8), img.mode)


# Nivel 12 (2026-11-24, el usuario: "las de 12 estrellas tienen corridas las
# estrellas hacia la izquierda"): desde x 107.5 la fila llega al marco de la
# izquierda y le sobran ~6 px a la derecha; centrada bajo el recuadro del
# nombre (x ~10..117.5). Solo Dragon final azul - ojos (4386) es de 12.
EST_X0_12 = 113.25


def _papel_vertical(img, k, centros):
    """Nivel 12: el papel bajo la fila, columna por columna, del papel de
    justo arriba y justo abajo de la fila (el color de la carta y el borde del
    marco de la izquierda siguen; con _papel_estrellas quedaba una franja mas
    clara y se comia el marco, usuario 2026-11-24). Bajo el atributo la fila
    de arriba toca el circulo: ahi, la de las columnas vecinas."""
    a = np.asarray(img, np.float32).copy()
    x0c, x1c = min(centros) * k, max(centros) * k
    Y0, Y1 = int(25.3 * k), int(36.9 * k)
    X1 = a.shape[1]
    yy, xx = np.mgrid[Y0:Y1, 0:X1] + 0.5
    dx = np.maximum(np.maximum(0, x0c - xx), np.maximum(0, xx - x1c))
    dy = yy - EST_Y * k
    dy = np.where(dy > 0, dy / 1.4, dy)          # mas alto hacia abajo: la mancha vieja baja mas
    f = np.clip(EST_R + 1.4 - np.hypot(dx, dy) / k, 0, 1)[..., None]
    arr = a[Y0 - k:Y0, :, :3].mean(axis=0)
    aba = a[Y1:Y1 + k, :, :3].mean(axis=0)
    xs = np.arange(X1)
    malo = (xs > 95 * k) & (arr.mean(axis=1) < aba.mean(axis=1) - 12)
    malo = np.convolve(malo.astype(float), np.ones(2 * k + 1), "same") > 0
    if malo.any() and (~malo).any():
        for c in range(3):
            arr[malo, c] = np.interp(xs[malo], xs[~malo], arr[~malo, c])
    t = ((np.arange(Y0, Y1) + 0.5 - Y0) / (Y1 - Y0))[:, None, None]
    nuevo = arr[None] * (1 - t) + aba[None] * t
    a[Y0:Y1, :, :3] = a[Y0:Y1, :, :3] * (1 - f) + nuevo * f
    return Image.fromarray(a.clip(0, 255).astype(np.uint8), img.mode)


def estrellas(img, nivel, k):
    if not 1 <= nivel <= 12:
        return img
    if nivel == 12:
        centros = [EST_X0_12 - EST_PASO * i for i in range(nivel)]
        # tambien donde estaba la primera (las manchas de la IA, x 8.5)
        img = _papel_vertical(img, k, centros + [EST_X0 - EST_PASO * 11])
    else:
        centros = [EST_X0 - EST_PASO * i for i in range(nivel)]
        img = _papel_estrellas(img, k, centros)
    a = np.asarray(img, np.float32)
    papel = np.median(a[int(26 * k):int(35 * k), 16 * k:36 * k, :3].reshape(-1, 3), axis=0)
    luz = float(np.clip((papel.mean() / 153.3) ** 0.3, 0.88, 1.08))   # 153.3: el papel de una normal
    bb = np.asarray(_estrella(k), np.float32).copy()
    bb[..., :3] = np.clip(bb[..., :3] * luz, 0, 255)
    b = Image.fromarray(bb.astype(np.uint8), "RGBA")
    sombra = Image.new("RGBA", b.size, (0, 0, 0, 0))
    sombra.putalpha(b.getchannel("A").point(lambda v: v * 0.45))
    sombra = sombra.filter(ImageFilter.GaussianBlur(0.6 * k))
    lienzo = Image.new("RGBA", img.size, (0, 0, 0, 0))
    capa = Image.new("RGBA", img.size, (0, 0, 0, 0))
    for cx in centros:
        x, y = cx * k - b.size[0] / 2, EST_Y * k - b.size[1] / 2
        lienzo.alpha_composite(sombra, (round(x + 0.35 * k), round(y + 0.45 * k)))
        capa.alpha_composite(b, (round(x), round(y)))
    img.alpha_composite(lienzo)
    ca = np.asarray(capa, np.float32).copy()
    base = np.asarray(img, np.float32)[..., :3]
    grano = (base - np.asarray(img.filter(ImageFilter.GaussianBlur(k)), np.float32)[..., :3]).mean(axis=2, keepdims=True)
    ca[..., :3] = np.clip(ca[..., :3] + grano * 0.5, 0, 255)
    img.alpha_composite(Image.fromarray(ca.astype(np.uint8), "RGBA").filter(ImageFilter.GaussianBlur(0.12 * k)))
    return img


def abrir_escalada(path):
    """la escalada de la carta (digital-art-4x) con el recuadro del dibujo de
    DIBUJOS (remacri-4x), fundido en el borde para que el marco quede igual"""
    up = Image.open(path)
    h = pathlib.Path(path).name[:16].upper()
    dib = DIBUJO_OTRO.get(h, DIBUJOS) / f"{h}.png"
    k = up.size[0] // 128
    if not dib.is_file() or k != 4:
        return up
    up = up.convert("RGBA")
    x0, y0, x1, y1 = (v * k for v in DIBUJO_CAJA)
    otro = Image.open(dib).convert("RGBA")
    if otro.size != (x1 - x0, y1 - y0):
        return up
    w, hh = otro.size
    yy = np.minimum(np.arange(hh), np.arange(hh)[::-1])[:, None]
    xx = np.minimum(np.arange(w), np.arange(w)[::-1])[None, :]
    m = np.clip(np.minimum(yy, xx) / (DIBUJO_FUNDIDO * k), 0, 1)
    m = m * m * (3 - 2 * m)
    up.paste(otro, (x0, y0), Image.fromarray((m * 255).astype(np.uint8), "L"))
    return up


def escribir(up, orig, c):
    k = up.size[0] // 128
    img = up.convert("RGBA").transpose(Image.FLIP_TOP_BOTTOM)
    a = np.asarray(orig.convert("RGB"), np.int16)
    S = lambda *v: tuple(x * k for x in v)
    # el mod de cartas dice si es monstruo (con textos largos la raya de ATK/DEF
    # de la carta base no se ve: Dark Necrofear, 2026-11-13)
    mon = c["mon"] if "mon" in c else es_monstruo(a)
    # descripcion: las letras de la IA borradas sobre el papel de la carta
    # (limpiar_papel; antes un recuadro liso, de otro tono)
    # x 12..115: en 116-118 empieza el borde del recuadro (con 116 se borraba la
    # linea del borde derecho, usuario 2026-11-12)
    y0, y1 = (148, 167) if mon else (142, 173)
    x0, x1 = 12, 115
    X0, Y0, X1, Y1 = S(x0, y0, x1 + 1, y1 + 1)
    img = limpiar_papel(img, (X0, Y0, X1, Y1), k)
    pad = k
    ancho, alto = X1 - X0 - 2 * pad, Y1 - Y0 - 2 * pad
    def mejor(sq):
        for size in range(min(int(alto / 2), 24), 7, -1):
            font = ImageFont.truetype(str(FONT), size)
            lineas = partir(c["desc"], font, ancho / sq)
            if len(lineas) * round(size * 1.08) <= alto:
                return size, font, lineas
        return size, font, lineas
    # angostar solo si la letra gana al menos 3 px (si no, se ve apretada)
    sq = 1.0
    size, font, lineas = mejor(1.0)
    for q in (0.88, 0.78):
        r = mejor(q)
        if r[0] >= size + 3:
            sq, (size, font, lineas) = q, r
            break
    lh = round(size * 1.08)
    y = Y0 + pad
    for ln in lineas:
        img.alpha_composite(angosto(ln, font, INK, sq), (X0 + pad, y))
        y += lh
    # el recuadro del nombre se tapa entero, también detrás del icono (hasta
    # el borde en x 117): si no, se ve el corte entre el liso y el papel de la IA
    img = rellenar(img, S(12, 11, 117, 24))
    img = linea(img, S(12, 11, 102, 24), c["nombre"], BOLD, 9 * k, NEGRO, papel=False)
    icono(img, c, k)
    if mon:
        t = "[" + c["tipo_txt"].upper() + c.get("sufijo", MON_SUFIJO[c["clase"]]) + "]"   # sufijo: otro idioma (cartas_mod)
        img = limpiar_papel(img, S(12, 142, 116, 148), k)
        img = linea(img, S(12, 142, 116, 148), t, BOLD, 5 * k, INK, papel=False)
        t = f"ATK/{c['atk']}  DEF/{c['df']}"
        # el papel de la izquierda de esa franja (x 16..54, no hay letras)
        img = linea(img, S(56, 169, 114, 174), t, BOLD, 5 * k + 2, INK, derecha=True,
                    papel=S(16, 54))
        if "nivel" in c:
            img = estrellas(img, int(c["nivel"]), k)
    else:
        img = limpiar_papel(img, S(30, 26, 116, 35), k)
        etq = c.get("etiqueta") or etiqueta(c)
        box = S(30, 26, 117, 35)
        if c.get("subtipo") in SUB_ICONO and etq.endswith("]"):
            # "[CARTA MÁGICA <icono>]": el corchete a la derecha, el icono y el texto antes
            pad = (box[3] - box[1]) // 6
            img = linea(img, box, "]", BOLD, 6 * k, NEGRO, derecha=True, papel=False)
            wb = ImageFont.truetype(str(BOLD), 6 * k).getlength("]")
            d = SUB_D * k
            cx = box[2] - pad - wb - 0.7 * k - d / 2
            img = sub_icono(img, c["subtipo"], cx, 30.6 * k, d)
            img = linea(img, (box[0], box[1], int(cx - d / 2 - 0.9 * k) + pad, box[3]), etq[:-1].rstrip(),
                        BOLD, 6 * k, NEGRO, derecha=True, papel=False)
        else:
            img = linea(img, box, etq, BOLD, 6 * k, NEGRO, derecha=True, papel=False)
    return img.transpose(Image.FLIP_TOP_BOTTOM), f"{size}" + ("" if sq == 1 else f" x{sq}"), len(lineas)


# El icono de la magica de juego rapido (rayo) y de la continua (infinito)
# en la etiqueta (2026-11-24, el usuario: "las cartas de juego rapido, se les
# perdio su signo ... un rayito"). Los datos del juego solo dicen clase 2 para
# las dos; cual es cual sale del icono del .tga original (igual en los 5
# idiomas): 12 de juego rapido, las otras 26 de clase 2 continuas.
RAPIDAS = {4815, 4904, 4905, 4906, 4909, 5118, 5129, 5152, 5214, 5350, 5432, 5437}
SUB_ICONO = ("rapida", "continua")
SUB_D = 6.4                      # diametro (px del .tga); el original ~7


def subtipo(cid, c):
    if c["tipo"] != 22 or c["clase"] != 2:
        return None
    return "rapida" if cid in RAPIDAS else "continua"


def sub_icono(img, sub, cx, cy, d, q=6):
    n = int(d) + 4
    N = n * q
    R = d / 2 * q
    yy, xx = (np.mgrid[0:N, 0:N] + 0.5)
    rr = np.hypot(xx - N / 2, yy - N / 2) / R
    t = np.clip((yy - N / 2) / R * 0.5 + 0.5, 0, 1)[..., None]
    col = np.array((78, 80, 86), np.float32) * (1 - t) + np.array((22, 22, 26), np.float32) * t
    aro = np.clip((rr - 0.80) / 0.08, 0, 1)[..., None]           # aro gris claro
    col = col * (1 - aro) + np.array((196, 198, 204), np.float32) * aro
    a = np.clip((1 - rr) * R / q * 1.6, 0, 1) * 255
    disco = Image.fromarray(np.dstack([col.clip(0, 255), a]).astype(np.uint8), "RGBA")
    m = Image.new("L", (N, N))
    dr = ImageDraw.Draw(m)
    c0 = N / 2
    if sub == "rapida":
        pts = [(0.20, -0.66), (-0.34, 0.08), (-0.02, 0.08), (-0.20, 0.68), (0.36, -0.10), (0.04, -0.10), (0.28, -0.66)]
        dr.polygon([(c0 + x * R, c0 + y * R) for x, y in pts], fill=255)
    else:
        w = max(1, int(0.13 * R))
        for sx in (-1, 1):
            ex = c0 + sx * 0.27 * R
            dr.ellipse((ex - 0.29 * R, c0 - 0.22 * R, ex + 0.29 * R, c0 + 0.22 * R), outline=255, width=w)
    disco.paste(Image.new("RGBA", (N, N), (250, 250, 252, 255)), (0, 0), m)
    disco.putalpha(Image.fromarray(a.astype(np.uint8)))
    disco = disco.resize((n, n), Image.LANCZOS)
    img.alpha_composite(disco, (round(cx - n / 2), round(cy - n / 2)))
    return img


def main():
    game = pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else GAME
    xbe, tgas = game / "default.xbe", game / "euro" / "sp" / "card" / "normal"
    if not xbe.is_file() or not tgas.is_dir():
        print(f"No encuentro el juego en {game}")
        return 1
    datos = textos(xbe)
    cartas = {}
    for p in tgas.rglob("*.tga"):
        im = Image.open(p).convert("RGB")
        cartas[im.transpose(Image.FLIP_TOP_BOTTOM).tobytes()] = (int(p.stem), im)
    escaladas = {}
    for p in (ROOT / "mejoradas").rglob("*.png"):
        if OUT not in p.parents:
            escaladas.setdefault(p.name[:16].upper(), p)
    OUT.mkdir(parents=True, exist_ok=True)
    hechas = 0
    for d in sorted((ROOT / "dump").glob("*_128x256.png")):
        h = d.name[:16].upper()
        hit = cartas.get(Image.open(d).convert("RGB").tobytes())
        if not hit or h not in escaladas:
            continue
        cid, orig = hit
        c = datos.get(cid)
        if not c or not c["desc"] or c["desc"] == "Reserve":
            print(f"  sin datos: {d.name} (imagen {cid})")
            continue
        # 2026-11-24: monstruo o no, de los datos del juego (con texto largo la
        # raya de ATK/DEF del .tga no se ve: 26 monstruos salian como magicas
        # de equipo, sin estrellas; el usuario: "busca todas las cartas que
        # tengan las estrellas asi")
        c = dict(c, mon=c["tipo"] < 21, subtipo=subtipo(cid, c))
        img, size, n = escribir(abrir_escalada(escaladas[h]), orig, c)
        img.save(OUT / d.name)
        hechas += 1
        print(f"  {cid}: {c['nombre']} ({c['tipo_txt']} {c['atk']}/{c['df']}), descripcion {size} px en {n} lineas")
    print(f"{hechas} cartas reescritas en {OUT}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
