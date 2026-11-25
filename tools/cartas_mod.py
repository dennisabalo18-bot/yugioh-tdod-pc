"""Arma el mod de cartas (opcional) desde modding/cartas/cartas.txt.

Salida: modding/cartas/salida/cartas/ = lo que va en Mods-Extras\\cartas\\
del jugador:
- cartas.txt: los datos que el juego cambia al arrancar (ygo-apex/src/cartas.c):
  nombre, descripcion y efecto de cada carta;
- archivos/: los archivos del disco que el mod reemplaza (el juego los lee de
  ahi en vez del disco, RECOMP_MODS_DIR):
  * euro/XX/card/normal/NN00/ID.tga, la carta grande (128x256, P8) en los 5
    idiomas (el texto es el de cartas.txt en todos);
  * card/mini/texdb/texdbNNN.png, la hoja de miniaturas con la carta nueva;
  * monster/NNN/mNNN.igb, el modelo 3D (campo "modelo"), en el numero de
    modelo que la tabla del juego (0x44FBB0: id de imagen, modelo) da a la
    carta reemplazada; su textura grande (campo "modelo_hd") va al pack;
- cartas.pack: las mismas en alta resolucion (XRTP, BC3), con la huella de la
  carta nueva: el juego lo carga ademas del pack de texturas
  (RECOMP_TEXTURES_PACK_EXTRA).
- cartas_es/en/fr/de/it.pack: las hojas de miniaturas con cartas nuevas en
  alta resolucion, cada una con las cartas de ese idioma (la hoja es la misma
  para todos: misma huella); el juego carga solo la del idioma elegido
  (main.c, RECOMP_TEXTURES_PACK_MORE). Las de los otros idiomas toman las
  demas cartas de modding/texturas/idiomas/xx/ (tools/idiomas_pack.py).

La carta grande sale de la del juego que se reemplaza: su version mejorada
(modding/texturas/mejoradas/texto_cartas, x4) con la ilustracion nueva en el
recuadro del dibujo (x 18..109, y 40..131 del .tga) y el nombre y la
descripcion escritos de nuevo (tools/texto_cartas.py). La de 128x256 es esa
achicada; la miniatura, la grande achicada en su casillero de la hoja.

    py -3 tools/cartas_mod.py [carpeta_del_juego]
"""
import configparser
import pathlib
import shutil
import struct
import subprocess
import zlib
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFont

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import cartas_volcado as cv  # noqa: E402
import miniaturas as mi  # noqa: E402
import texto_cartas as tc  # noqa: E402

PORT = pathlib.Path(__file__).resolve().parent.parent
SRC = PORT / "modding" / "cartas"
OUT = SRC / "salida" / "cartas"
HD = SRC / "salida" / "hd"
MEJ = PORT / "modding" / "texturas" / "mejoradas"
IDIOMAS = ("sp", "en", "fr", "gr", "it")
# carpeta del juego -> (sufijo en cartas.txt: nombre_en..., tabla de textos del juego)
LENGUA = {"sp": ("", 5), "en": ("en", 1), "fr": ("fr", 3), "gr": ("de", 2), "it": ("it", 4)}
# las etiquetas de las cartas en cada idioma (las del juego: [WARRIOR/EFFECT], [CARTE PIÈGE]...)
SUFIJOS = {"en": ("", "/EFFECT", "/FUSION", "/RITUAL"), "fr": ("", "/EFFET", "/FUSION", "/RITUEL"),
           "de": ("", "/EFFEKT", "/FUSION", "/RITUAL"), "it": ("", "/EFFETTO", "/FUSIONE", "/RITUALE")}
ETIQUETAS = {
    "[CARTA DE TRAMPA]": {"en": "[TRAP CARD]", "fr": "[CARTE PIÈGE]", "de": "[FALLENKARTE]",
                          "it": "[CARTA TRAPPOLA]"},
    "[CARTA DE TRAMPA CONTINUA]": {"en": "[CONTINUOUS TRAP CARD]", "fr": "[CARTE PIÈGE CONTINUE]",
                                   "de": "[PERMANENTE FALLENKARTE]", "it": "[CARTA TRAPPOLA CONTINUA]"},
    "[CARTA MÁGICA]": {"en": "[SPELL CARD]", "fr": "[CARTE MAGIE]", "de": "[ZAUBERKARTE]",
                       "it": "[CARTA MAGIA]"},
    "[CARTA MÁGICA DE RITUAL]": {"en": "[RITUAL SPELL CARD]", "fr": "[CARTE MAGIE RITUELLE]",
                                "de": "[RITUALZAUBERKARTE]", "it": "[CARTA MAGIA RITUALE]"},
    "[CARTA MÁGICA DE CAMPO]": {"en": "[FIELD SPELL CARD]", "fr": "[CARTE MAGIE DE TERRAIN]",
                                "de": "[SPIELFELDZAUBERKARTE]", "it": "[CARTA MAGIA TERRENO]"},
    "[CARTA MÁGICA DE EQUIPO]": {"en": "[EQUIP SPELL CARD]", "fr": "[CARTE MAGIE D'ÉQUIPEMENT]",
                                 "de": "[AUSRÜSTUNGSZAUBERKARTE]", "it": "[CARTA MAGIA EQUIPAGGIAMENTO]"},
}
TIPOS_TXT = 3432                 # los nombres de los tipos en las tablas de textos (+ tipo)


def texto_juego(xbe, idx, tabla):
    """el texto idx de una tabla de textos del juego (1 ingles, 2 aleman, 3 frances,
    4 italiano, 5 espanol: punteros en 0x5FC2DC, bases en 0x5FC2C4)"""
    def off(va):
        return va - 0x5FB9E0 + 0x5EC000 if va >= 0x5FB9E0 else va - 0x423080 + 0x413000
    u32 = lambda va: struct.unpack_from("<I", xbe, off(va))[0]
    tab, base = u32(0x5FC2DC + 4 * tabla), u32(0x5FC2C4 + 4 * tabla)
    a = off(base + u32(tab + 4 + 4 * idx))
    return xbe[a:xbe.index(b"\0", a)].decode("cp1252", "replace")


def datos_idioma(c, d, xbe, lang):
    """los datos de la carta para dibujarla en otro idioma: nombre_xx y
    descripcion_xx del cartas.txt (si faltan, los de siempre), el tipo y las
    etiquetas del juego en ese idioma"""
    suf, tabla = LENGUA[lang]
    if not suf:
        return d
    es, d = d, dict(d)
    d["etiqueta"] = ETIQUETAS.get(tc.etiqueta(es), {}).get(suf)   # se decide con el texto en espanol
    d["nombre"] = c.get("nombre_" + suf) or d["nombre"]
    d["desc"] = c.get("descripcion_" + suf) or d["desc"]
    d["tipo_txt"] = texto_juego(xbe, TIPOS_TXT + d["tipo"], tabla)
    d["sufijo"] = SUFIJOS[suf][d["clase"]]
    return d
ARTE = (18, 40, 92, 92)          # x, y, ancho, alto del dibujo en el .tga
K = 4
# efectos programados en ygo-apex/src/cartas.c: nombre -> (para que clase de carta, que hace)
EFECTOS = {
    "prision_dimensional": ("trampa", "Cuando un monstruo del adversario declara un ataque: destierra al atacante."),
    "krebons": ("monstruo", "Cuando esta carta boca arriba es atacada: pagas 800 LP y se niega el ataque "
                            "(siempre, si tienes mas de 800 LP)."),
    "sincronizador": ("monstruo", "Al ser Invocada de Modo Normal: puedes Invocar de Modo Especial en Defensa 1 "
                                  "monstruo de Nivel 2 o menor de tu Cementerio."),
    "escudo_drenador": ("trampa", "Cuando un monstruo del adversario declara un ataque: niega el ataque y ganas "
                                  "LP iguales al ATK del atacante."),
    "ciclon_cosmos": ("magica", "Paga 1000 LP; selecciona 1 Magica/Trampa en el Campo y destierrala."),
    "cerezas": ("monstruo", "Al ir al Cementerio por una batalla: puedes Invocar de Modo Especial desde tu "
                            "Deck hasta 2 copias de esta carta boca abajo en Defensa (como Momonga agil)."),
    "golpe_fenix": ("trampa", "Descarta 1 carta; selecciona 1 carta que controle tu adversario y ponla en la "
                              "parte superior del Deck."),
    "des_lacooda": ("monstruo", "Como Des Lacooda: una vez por turno se puede poner boca abajo; al ser Invocada "
                                "por Volteo roba 1 carta."),
    "hombre_nieve": ("monstruo", "Como Come Hombres: VOLTEO, destruye 1 monstruo del Campo."),
    "vidente": ("monstruo", "VOLTEO: roba 2 cartas y despues devuelve 1 carta de tu mano al Deck y lo baraja "
                            "(el disparo de Skelengel, los pasos de la Caridad y de El centinela energico)."),
    "golem_centinela": ("monstruo", "Una vez por turno se puede poner boca abajo (como Des Lacooda); VOLTEO: "
                                    "devuelve a la mano 1 monstruo del adversario (como Guardia del Guardatumba)."),
    "soldado_brillo": ("monstruo", "Invocacion Especial desterrando 1 LUZ y 1 OSCURIDAD del Cementerio; una vez "
                                   "por turno: destierra 1 monstruo (no ataca ese turno) o, si destruye un monstruo "
                                   "atacando, ataca otra vez."),
    "goblin_entrenador": ("monstruo", "En tu End Phase, si esta boca arriba: puedes devolver 1 monstruo normal "
                                      "de tu mano a la parte inferior del Deck y robar 1 (una vez por turno)."),
    "drillago": ("monstruo", "Puede atacar directo si el adversario solo controla monstruos boca arriba con "
                             "1600 ATK o mas (el permiso de Lampara mistica)."),
    "fortaleza": ("monstruo", "Una vez por turno no se destruye en batalla; su duenio no recibe dano de batalla "
                              "de sus batallas (el resultado de la batalla, como Parca de Espiritu)."),
    "caius": ("monstruo", "Al ser Invocada por Sacrificio: elige 1 carta del Campo y la destierra; si era un monstruo "
                          "de OSCURIDAD, 1000 de dano al adversario (el disparo de Buscador de dragon)."),
    "isaac": ("monstruo", "El monstruo que destruye en batalla se destierra en vez de ir al Cementerio "
                          "(el resultado de la batalla y el envio al Cementerio, como Ciclon)."),
    "caballero_real": ("monstruo", "Al destruir un monstruo en batalla y mandarlo al Cementerio, su duenio gana LP "
                                   "iguales a la DEF de ese monstruo (el camino de Isaac y los LP de Escudo drenador)."),
    "acantilado_naturia": ("monstruo", "Al ir del Campo al Cementerio: puedes Invocar de Modo Especial del Deck 1 "
                                       "monstruo Naturia de Nivel 4 o menor en ataque (disparo de Sangan, Rata gigante)."),
    "berenjena_naturia": ("monstruo", "Al ir del Campo al Cementerio: elige 1 monstruo Naturia de tu Cementerio (no "
                                      "Berenjena) y lo agrega a la mano (disparo de Sangan, Mago de la fe)."),
    "naturia_vanidosa": ("monstruo", "Responde a una Magica o Trampa del adversario sacrificandose con otro Naturia: "
                                     "niega y destruye (Maryokutai, tambien contra Trampas)."),
    "cosmo_naturia": ("monstruo", "Cuando el adversario hace una Llamada Normal, se llama de Modo Especial desde la "
                                  "mano (disparo de Garuda, la salida de la mano de Llamada de la Momia)."),
    "chinche_naturia": ("monstruo", "Si atacan a un Naturia boca arriba de su duenio, va al Cementerio y niega el ataque "
                                    "terminando la Battle Phase (como Krebons, con la orden de Anule el ataque)."),
    "abare_ushioni": ("monstruo", "Una vez por turno, en tu Fase Principal: lanza una moneda; si aciertas, 1000 de dano "
                                  "al adversario, si fallas, 1000 a ti (la moneda de Mago de tiempo)."),
    "carga_poderoso": ("magica", "Magica Continua: cada monstruo boca arriba del adversario pierde 100 ATK x su "
                                 "propio Nivel."),
}
ATRIBUTOS = {"luz": 1, "oscuridad": 2, "agua": 3, "fuego": 4, "tierra": 5, "viento": 6}
CLASES = {"normal": 0, "efecto": 1}
STATS = 0x5EF598                 # u32 por k en default.xbe (tools/texto_cartas.py)
EFX = 0x601318 - 0x5FB9E0 + 0x5EC000   # tabla de efectos (24 bytes por carta, ID primero)
N_EFX = 742


def leer():
    ini = configparser.ConfigParser(interpolation=None, comment_prefixes=("#",))
    ini.read(SRC / "cartas.txt", encoding="utf-8")
    cartas = []
    for s in ini.sections():
        c = dict(ini[s])
        c["id"] = int(s)
        if c.get("copias") and c["copias"] not in ("0", "1", "2", "3"):
            raise SystemExit(f"[{s}] copias tiene que ser 0, 1, 2 o 3")
        # premios (cartas.c, premios_ajustar): peso en las listas, premios 1-3, rival que la suma
        if c.get("premio_peso") and not (c["premio_peso"].isdigit() and int(c["premio_peso"]) <= 8191):
            raise SystemExit(f"[{s}] premio_peso tiene que ser un numero de 0 a 8191")
        if c.get("premio_ranuras") and not (set(c["premio_ranuras"]) <= set("123")):
            raise SystemExit(f"[{s}] premio_ranuras: solo 1, 2 y/o 3 (por ejemplo 3 o 23)")
        if c.get("premio_rival") and not (c["premio_rival"].isdigit() and int(c["premio_rival"]) < 32):
            raise SystemExit(f"[{s}] premio_rival tiene que ser un rival de 0 a 31")
        if c.get("efecto") and c["efecto"] not in EFECTOS:
            raise SystemExit(f"[{s}] efecto desconocido: {c['efecto']} (hay: {', '.join(EFECTOS)})")
        cartas.append(c)
    return cartas


def modelo_de(xbe, cid):
    """numero de modelo 3D de una carta: tabla de pares u16 (id, modelo) en
    0x44FBB0 (.rdata), que recorre sub_0007E690"""
    o = 0x44FBB0 - 0x423080 + 0x413000
    i = 0
    while True:
        a, b = struct.unpack_from("<HH", xbe, o + 4 * i)
        if not 4000 <= a <= 5300:
            return None
        if a == cid:
            return b
        i += 1


def imagenes_rgb(igb_path):
    """(ancho, alto, huella) de las texturas RGB (formato 5) de un .igb: la
    huella del juego es B,G,R,0 en orden Morton (tools/texturas_igb.py)"""
    import igb
    g = igb.IGB(str(igb_path))
    for n, f in g.objs:
        if n == "igImage" and f[12] == 5 and f[3] >= 256:
            w, h = f[3], f[4]
            a = np.frombuffer(g.mem_bytes(f[14]), np.uint8)[:w * h * 3].reshape(h, w, 3)
            bgra = np.ascontiguousarray(np.dstack([a[..., 2], a[..., 1], a[..., 0], np.zeros((h, w), np.uint8)]))
            yield w, h, cv.tex_hash(np.ascontiguousarray(cv.swizzle(bgra.view(np.uint32).reshape(h, w))).tobytes())


def guardar_tga(img, ref, dst):
    """img (P, 128x256, orientacion normal) como .tga igual al del juego:
    colormap de 256 BGR y filas de abajo hacia arriba (el encabezado de ref)."""
    hdr = bytearray(ref.read_bytes()[:18])
    if hdr[1] != 1 or hdr[2] != 1 or hdr[16] != 8 or hdr[17] & 0x20:
        raise SystemExit(f"{ref}: .tga con otro formato")
    pal = np.array(img.getpalette()[:768], np.uint8).reshape(-1, 3)
    pal = np.vstack([pal, np.zeros((256 - len(pal), 3), np.uint8)])
    dst.parent.mkdir(parents=True, exist_ok=True)
    dst.write_bytes(bytes(hdr) + pal[:, ::-1].tobytes() + np.asarray(img)[::-1].tobytes())
    back = Image.open(dst)
    assert np.array_equal(np.asarray(back), np.asarray(img)), dst


def png_igual(ref, idx, dst):
    """idx (P8) como PNG con los mismos bloques que ref (PLTE, tRNS de 256,
    sBIT...): solo cambian los pixeles. Con el tRNS corto que escribe PIL el
    juego arma otra paleta en memoria, la huella de la hoja cambia y no se
    usa su version HD (todas las miniaturas de la hoja borrosas, 2026-11-11)."""
    b = ref.read_bytes()
    h, w = idx.shape
    if b[24:26] != b"\x08\x03" or struct.unpack(">II", b[16:24]) != (w, h) or b[28]:
        raise SystemExit(f"{ref}: PNG con otro formato")
    def bloque(t, data):
        return struct.pack(">I", len(data)) + t + data + struct.pack(">I", zlib.crc32(t + data))
    out, i, idat = [b[:8]], 8, False
    while i < len(b):
        n, t = struct.unpack(">I4s", b[i:i + 8])
        if t == b"IDAT":
            if not idat:
                crudo = b"".join(b"\x00" + fila.tobytes() for fila in idx.astype(np.uint8))
                out.append(bloque(b"IDAT", zlib.compress(crudo, 9)))
                idat = True
        else:
            out.append(b[i:i + 12 + n])
        i += 12 + n
    dst.write_bytes(b"".join(out))


def a_paleta(rgb, pal, desde=0):
    """Cada pixel al color mas cercano de la paleta (indices >= desde)."""
    p = np.array(pal[:768], np.int32).reshape(-1, 3)[desde:]
    a = np.asarray(rgb.convert("RGB"), np.int32).reshape(-1, 3)
    best = np.zeros(len(a), np.int64)
    dist = np.full(len(a), 1 << 40, np.int64)
    for i, c in enumerate(p):
        d = ((a - c) ** 2).sum(1)
        m = d < dist
        dist[m], best[m] = d[m], i
    return (best + desde).astype(np.uint8).reshape(rgb.size[1], rgb.size[0])


def marco(tipo, clase, nivel):
    """Lo que hace distinto al dibujo de la carta: trampa, magica, o monstruo
    con su color (clase) y sus estrellas (nivel)."""
    if tipo == 21:
        return "trampa"
    if tipo == 22:
        return "magica"
    return f"monstruo {clase} {nivel}"


def todas(xbe):
    """Todas las cartas del juego, por k (1..1138)."""
    d = tc.textos_bytes(xbe)
    ids = [struct.unpack_from("<H", xbe, tc.TABLE + 2 * i)[0] for i in range(tc.NAMES)]
    efx = {struct.unpack_from("<I", xbe, EFX + 24 * i)[0] for i in range(N_EFX)}
    out = []
    for k, cid in enumerate(ids):
        if not k:
            continue
        w = struct.unpack_from("<I", xbe, STATS + 4 * k)[0]
        c = dict(d[cid])
        c.update(id=cid, k=k, nivel=(w >> 25) & 15, efecto_juego=cid in efx)
        c["marco"] = marco(c["tipo"], c["clase"], c["nivel"])
        out.append(c)
    return out


def mejorada(game, cid):
    """La carta grande escalada (mejoradas/texto_cartas), o None."""
    h = f"{cv.huella(Image.open(tga(game, cid))):016X}"
    src = MEJ / "texto_cartas" / f"{h}_128x256.png"
    return src if src.is_file() else next(MEJ.rglob(f"{h}_128x256.png"), None)


def tga(game, cid):
    return game / "euro" / "sp" / "card" / "normal" / f"{cid // 100 * 100}" / f"{cid}.tga"


def datos_nuevos(c, d, xbe):
    """Los datos de la carta con los cambios de cartas.txt, y el u32 de la
    tabla del juego (DEF/10 bits 0-8, ATK/10 9-17, clase 18-19, tipo 20-24,
    nivel 25-28, atributo 29-31) si cambia algo."""
    d = dict(d)
    d["nombre"], d["desc"] = c["nombre"], c["descripcion"]
    ids = [struct.unpack_from("<H", xbe, tc.TABLE + 2 * i)[0] for i in range(tc.NAMES)]
    k = ids.index(c["id"], 1)
    w0 = w = struct.unpack_from("<I", xbe, STATS + 4 * k)[0]
    tipos = tc.tipos(xbe)
    if "atk" in c:
        d["atk"] = int(c["atk"])
        w = (w & ~(511 << 9)) | ((d["atk"] // 10) << 9)
    if "def" in c:
        d["df"] = int(c["def"])
        w = (w & ~511) | (d["df"] // 10)
    if "clase" in c:
        d["clase"] = CLASES[c["clase"].lower()]
        w = (w & ~(3 << 18)) | (d["clase"] << 18)
    if "tipo" in c:
        d["tipo"] = [t.lower() for t in tipos].index(c["tipo"].lower())
        d["tipo_txt"] = tipos[d["tipo"]]
        w = (w & ~(31 << 20)) | (d["tipo"] << 20)
    if "atributo" in c:
        d["atrib"] = ATRIBUTOS[c["atributo"].lower()]
        w = (w & ~(7 << 29)) | (d["atrib"] << 29)
    d["nivel"] = (w0 >> 25) & 15
    if "nivel" in c:
        d["nivel"] = int(c["nivel"])
        w = (w & ~(15 << 25)) | (d["nivel"] << 25)
    d["marco"] = marco(d["tipo"], d["clase"], d["nivel"])
    d["mon"] = d["tipo"] < 21
    return d, (w if w != w0 else None)


def elegir_base(c, game, todas_):
    """Sin "base": la misma carta, salvo que cambie el marco (otro color u
    otras estrellas); entonces una carta del juego con ese marco (mejor del
    mismo atributo) que tenga su version escalada."""
    if c.get("base"):
        return int(c["base"])
    d = c["datos"]
    orig = next(t for t in todas_ if t["id"] == c["id"])

    # 2026-11-13 (Soldado del Brillo Negro): primero las de marco comun (de una
    # carta Toon salia otra cosa) y texto corto (las de texto largo dejan restos
    # y su raya de ATK/DEF no se ve), despues el mismo atributo
    def limpia(t):
        try:
            a = np.asarray(Image.open(tga(game, t["id"])).convert("RGB"), np.int16)
            return tc.es_monstruo(a) == d["mon"]
        except Exception:
            return False
    # 2026-11-18 (Cyber pote, texto corregido): una carta de texto tan largo
    # que su version mejorada la tomo por Magica (etiqueta sobre las estrellas,
    # restos abajo): otro marco limpio, con su propio dibujo (carta_hd)
    if orig["marco"] == d["marco"] and (c.get("ilustracion") or limpia(orig)):
        return c["id"]
    cand = [t for t in todas_ if t["marco"] == d["marco"] and t["id"] != c["id"]]
    cand.sort(key=lambda t: (not limpia(t), "toon" in t["nombre"].lower(), len(t.get("desc", "")) > 200,
                             t["atrib"] != d["atrib"], t["id"]))
    for t in cand:
        if mejorada(game, t["id"]):
            print(f"  {c['id']}: marco de {t['id']} {t['nombre']}")
            return t["id"]
    raise SystemExit(f"[{c['id']}] ninguna carta del juego tiene ese marco ({d['marco']})")


def carta_hd(c, game, d):
    """La carta nueva en x4, orientacion normal, la .tga del marco y la .tga
    que se reemplaza."""
    ref = tga(game, c["id"])
    orig = Image.open(tga(game, c["base_id"]))
    src = mejorada(game, c["base_id"])
    if src is None:
        raise SystemExit(f"[{c['id']}] no esta la version mejorada de la carta {c['base_id']}")
    up = Image.open(src).convert("RGBA").transpose(Image.FLIP_TOP_BOTTOM)
    k = up.size[0] // 128
    x, y, w, hh = ARTE
    if c.get("ilustracion"):            # sin ilustracion (solo_espanol): el dibujo de siempre
        ilus = Image.open(SRC / c["ilustracion"]).convert("RGB")
        # el dibujo es cuadrado; si no, se recorta al centro
        s = min(ilus.size)
        ilus = ilus.crop(((ilus.size[0] - s) // 2, (ilus.size[1] - s) // 2,
                          (ilus.size[0] + s) // 2, (ilus.size[1] + s) // 2))
        up.paste(ilus.resize((w * k, hh * k), Image.LANCZOS), (x * k, y * k))
    elif c["base_id"] != c["id"]:         # su propio dibujo sobre el marco de otra
        propia = Image.open(mejorada(game, c["id"])).convert("RGBA").transpose(Image.FLIP_TOP_BOTTOM)
        kp = propia.size[0] // 128
        art = propia.crop((x * kp, y * kp, (x + w) * kp, (y + hh) * kp))
        up.paste(art.resize((w * k, hh * k), Image.LANCZOS), (x * k, y * k))
    out, size, n = tc.escribir(up.transpose(Image.FLIP_TOP_BOTTOM), orig.convert("RGB"), d)
    print(f"  {c['id']}: {d['nombre']}, descripcion {size} px en {n} lineas")
    return out.transpose(Image.FLIP_TOP_BOTTOM).convert("RGB"), orig, ref


# 2026-11-18 (el usuario: "si esta limitada que aparezca arriba de la carta
# cuantas copias se puede llevar"; despues: "como en Master Duel ... solo en la
# biblioteca y cuando te armas el Deck", "que tape el inicio del nombre", "en
# el duelo nada"; muestra aprobada): aro rojo, centro negro y el numero en
# amarillo, arriba a la izquierda. En las cartas del mod con "copias" 1 o 2 y
# en las limitadas del juego (lista de 61 u16 en 0x450BC0: id en los bits
# 0-13, copias en 14-15; sub_0007EF00). Los archivos del juego (.tga, hojas)
# quedan SIN circulo; la carta con circulo va a los packs con la huella
# ^ VARIANTE (kit: nv2a_tex_pack_set_variant), que el juego usa solo fuera
# del duelo (ygo-apex/src/auto.c).
INSIGNIA_COLOR = {1: None, 2: None}
VARIANTE = 0x4C494D4954414441       # kit: NV2A_TEX_PACK_VARIANT_SALT


def var_h(h):
    return f"{int(h, 16) ^ VARIANTE:016X}"


def limitadas_juego(xbe):
    o = 0x450BC0 - 0x423080 + 0x413000
    return {v & 0x3FFF: v >> 14 for v in struct.unpack_from("<61H", xbe, o) if 1 <= v >> 14 <= 2}


def insignia(img, n, cx=9.0 * K, cy=9.0 * K, r=9.0 * K):
    """img: la carta HD (x K) en orientacion normal; el circulo de n, como
    Master Duel: tapa el inicio del nombre. En las miniaturas (hojas) va mas
    grande y metido adentro del borde de la casilla (2026-11-19: se cortaba)."""
    base = img.convert("RGBA")
    capa = Image.new("RGBA", base.size, (0, 0, 0, 0))
    d = ImageDraw.Draw(capa)
    d.ellipse((cx - r, cy - r, cx + r, cy + r), fill=(250, 5, 5, 255))
    r1 = r * (1 - 1.5 / 9)
    d.ellipse((cx - r1, cy - r1, cx + r1, cy + r1), fill=(8, 3, 3, 255))
    try:
        f = ImageFont.truetype("C:/Windows/Fonts/ariblk.ttf", int(r1 * 1.7))
    except OSError:
        f = ImageFont.load_default()
    t = str(n)
    x0, y0, x1, y1 = d.textbbox((0, 0), t, font=f)
    d.text((cx - (x1 - x0) / 2 - x0, cy - (y1 - y0) / 2 - y0), t, font=f, fill=(250, 205, 15, 255))
    return Image.alpha_composite(base, capa).convert("RGB")


def mejoradas_de(lang):
    """hash -> la carta mejorada de ese idioma (como las hojas HD)"""
    orig_mej = {}
    if lang == "sp":
        for sub in ("texto_cartas", None):
            base = MEJ / sub if sub else MEJ
            for p in base.rglob("*_128x256.png"):
                if p.parent.name != "miniaturas":
                    orig_mej.setdefault(p.name[:16].upper(), p)
    else:
        for p in (MEJ.parent / "idiomas" / LENGUA[lang][0]).glob("*_128x256.png"):
            orig_mej[p.name[:16].upper()] = p
    return orig_mej


def solo_es(c):
    return c.get("solo_espanol", "").lower() in ("si", "sí", "1", "yes")


def lista(game):
    """Para el editor (tools/editor_cartas): todo en JSON por la salida."""
    import json
    xbe = (game / "default.xbe").read_bytes()
    sys.stdout.reconfigure(encoding="utf-8")
    json.dump({"cartas": todas(xbe), "tipos": tc.tipos(xbe)[:23],
               "atributos": list(ATRIBUTOS), "efectos": {k: list(v) for k, v in EFECTOS.items()}},
              sys.stdout, ensure_ascii=False)
    return 0


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    if "--vista" in sys.argv:
        # para el editor: una imagen cualquiera (.webp...) como PNG
        #   cartas_mod.py --vista ENTRADA SALIDA [hd]  (hd: la carta del pack, dada vuelta)
        im = Image.open(args[0])
        if len(args) > 2 and args[2] == "hd":
            im = im.transpose(Image.FLIP_TOP_BOTTOM)
        im.convert("RGB").save(args[1])
        return 0
    game = pathlib.Path(args[0]) if args else cv.GAME
    if "--lista" in sys.argv:
        return lista(game)
    cartas = leer()
    datos = tc.textos(game / "default.xbe")
    xbe = (game / "default.xbe").read_bytes()
    todas_ = todas(xbe)
    if OUT.exists():
        shutil.rmtree(OUT)
    if HD.exists():
        shutil.rmtree(HD)
    HD.mkdir(parents=True)
    # las hojas de miniaturas HD, una por idioma (la hoja es la misma para
    # todos: misma huella): cada una a su cartas_<xx>.pack, que el juego carga
    # solo con ese idioma (main.c)
    hd_l = {lang: SRC / "salida" / f"hd_{LENGUA[lang][0] or 'es'}" for lang in IDIOMAS}
    for d in hd_l.values():
        if d.exists():
            shutil.rmtree(d)
        d.mkdir(parents=True)
    arch = OUT / "archivos"
    grandes_l = {lang: {} for lang in IDIOMAS}   # idioma -> id -> (hd normal, 128 normal RGB)
    grandes = grandes_l["sp"]
    ins_l = {lang: {} for lang in IDIOMAS}       # idioma -> id -> la HD con circulo (variante)
    lim_juego = limitadas_juego(xbe)
    for c in cartas:
        c["datos"], c["w"] = datos_nuevos(c, datos[c["id"]], xbe)
        c["base_id"] = elegir_base(c, game, todas_)
        n_lim = int(c["copias"]) if c.get("copias") else lim_juego.get(c["id"])
        # una carta por idioma (euro/sp, en, fr, gr, it), cada una con su texto;
        # "solo_espanol = si" (2026-11-18: textos del juego en espanol de
        # Espana corregidos): solo la espanola, las otras quedan las del juego
        for lang in (("sp",) if solo_es(c) else IDIOMAS):
            hd, orig, ref = carta_hd(c, game, datos_idioma(c, c["datos"], xbe, lang))
            # 128x256: la HD achicada; abajo de la carta (negro) queda el original
            g = np.asarray(orig.convert("L")).max(axis=1)
            alto = int(np.nonzero(g > 16)[0].max()) + 1
            chica = orig.convert("RGB")
            chica.paste(hd.crop((0, 0, 128 * K, alto * K)).resize((128, alto), Image.LANCZOS), (0, 0))
            p8 = chica.quantize(256, method=Image.Quantize.MEDIANCUT)
            guardar_tga(p8, ref, arch / "euro" / lang / "card" / "normal" / ref.parent.name / ref.name)
            h = f"{cv.huella(p8):016X}"
            hd.convert("RGBA").transpose(Image.FLIP_TOP_BOTTOM).save(HD / f"{h}_128x256.png")
            grandes_l[lang][c["id"]] = (hd.crop((0, 0, 128 * K, alto * K)),
                                        p8.convert("RGB").crop((0, 0, 128, alto)))
            if n_lim in INSIGNIA_COLOR:
                hi = insignia(hd, n_lim)
                hi.convert("RGBA").transpose(Image.FLIP_TOP_BOTTOM).save(HD / f"{var_h(h)}_128x256.png")
                ins_l[lang][c["id"]] = (hd.crop((0, 0, 128 * K, alto * K)), n_lim)
            print(f"    grande {lang} {h}")
    # las limitadas del juego (las que el mod no reemplaza): la misma carta con el circulo
    del_mod = {c["id"]: (("sp",) if solo_es(c) else IDIOMAS) for c in cartas}
    mej_l = {lang: mejoradas_de(lang) for lang in IDIOMAS}
    for cid, n_lim in sorted(lim_juego.items()):
        for lang in IDIOMAS:
            if lang in del_mod.get(cid, ()):
                continue
            tgas = game / "euro" / lang / "card" / "normal"
            ref = tgas / f"{cid // 100 * 100}" / f"{cid}.tga"
            big = mi.carta_grande(cid, tgas, mej_l[lang])
            if big is None:
                continue
            orig = Image.open(ref)
            h = f"{cv.huella(orig):016X}"
            hi = insignia(big, n_lim)
            alto = big.size[1] // K
            full = orig.convert("RGB").resize((128 * K, 256 * K), Image.LANCZOS)
            full.paste(hi, (0, 0))
            full.convert("RGBA").transpose(Image.FLIP_TOP_BOTTOM).save(HD / f"{var_h(h)}_128x256.png")
            ins_l[lang][cid] = (big, n_lim)
        print(f"  {cid}: limitada a {n_lim} (circulo, variante)")
    # hojas de miniaturas con alguna carta nueva
    texdb = game / "card" / "mini" / "texdb"
    for png in sorted(texdb.glob("texdb*.png")):
        filas = []
        for ln in png.with_suffix(".txt").read_text(encoding="latin-1").splitlines():
            f = ln.split()
            if len(f) >= 8 and f[0].lower().endswith(".png"):
                filas.append((int(f[0][:-4]), *map(float, f[3:8])))
        con_mod = any(r[0] in grandes for r in filas)
        con_lim = any(r[0] in ins_l[lang] for lang in IDIOMAS for r in filas)
        if not con_mod and not con_lim:
            continue
        hoja = Image.open(png)
        W, H = hoja.size
        idx = np.asarray(hoja).copy()
        for cid, u0, v0, u1, v1, girada in filas:
            if cid not in grandes:
                continue
            x0, x1 = round(u0 * W), round(u1 * W)
            y0, y1 = round((1 - v1) * H), round((1 - v0) * H)
            c = grandes[cid][0]
            if girada:
                c = c.rotate(-90, expand=True)
            cel = c.resize((x1 - x0, y1 - y0), Image.LANCZOS)
            # el color 0 de la hoja es transparente: no se usa en el dibujo
            idx[y0:y1, x0:x1] = a_paleta(cel, hoja.getpalette(), desde=1)
        dst = png
        if con_mod:
            dst = arch / "card" / "mini" / "texdb" / png.name
            dst.parent.mkdir(parents=True, exist_ok=True)
            png_igual(png, idx, dst)
            assert np.array_equal(np.asarray(Image.open(dst)), idx), dst
        # la hoja HD, una por idioma: como tools/miniaturas.py, con las cartas
        # nuevas y las demas de ese idioma (espanol: texto_cartas; los otros:
        # modding/texturas/idiomas/xx/, tools/idiomas_pack.py)
        h = f"{mi.huella_hoja(Image.open(dst)):016X}"
        for lang in IDIOMAS:
            orig_mej = {}
            if lang == "sp":
                for sub in ("texto_cartas", None):
                    base = MEJ / sub if sub else MEJ
                    for p in base.rglob("*_128x256.png"):
                        if p.parent.name != "miniaturas":
                            orig_mej.setdefault(p.name[:16].upper(), p)
            else:
                for p in (MEJ.parent / "idiomas" / LENGUA[lang][0]).glob("*_128x256.png"):
                    orig_mej[p.name[:16].upper()] = p
            tgas = game / "euro" / lang / "card" / "normal"
            # la hoja comun (solo si el mod la cambia) y la variante con circulos
            for var in ((False, True) if con_mod else (True,)):
                out = hoja.convert("RGB").resize((W * K, H * K), Image.LANCZOS)
                for cid, u0, v0, u1, v1, girada in filas:
                    x0, x1 = round(u0 * W * K), round(u1 * W * K)
                    y0, y1 = round((1 - v1) * H * K), round((1 - v0) * H * K)
                    g_l = grandes_l[lang]
                    n_lim = None
                    if var and cid in ins_l[lang]:
                        c, n_lim = ins_l[lang][cid]
                    else:
                        c = g_l[cid][0] if cid in g_l else mi.carta_grande(cid, tgas, orig_mej)
                    if c is None:
                        continue
                    # la casilla derecha (sin girar), su borde y el circulo; despues se gira
                    cw, ch = (y1 - y0, x1 - x0) if girada else (x1 - x0, y1 - y0)
                    cel = c.resize((cw, ch), Image.LANCZOS)
                    ImageDraw.Draw(cel).rectangle((0, 0, cw - 1, ch - 1), outline=mi.BORDE, width=K + 2)
                    if n_lim:
                        rr = cw * 0.12
                        cel = insignia(cel, n_lim, K + 2 + rr, K + 2 + rr, rr)
                    if girada:
                        cel = cel.rotate(-90, expand=True)
                    out.paste(cel, (x0, y0))
                nom = var_h(h) if var else h
                out.convert("RGBA").transpose(Image.FLIP_TOP_BOTTOM).save(hd_l[lang] / f"{nom}_{W}x{H}.png")
        print(f"  {png.name}: hoja {h} (5 idiomas)")
    # modelos 3D (tools/modelos/armar_igb.py)
    for c in cartas:
        if not c.get("modelo"):
            continue
        n = modelo_de(xbe, c["id"])
        if n is None:
            raise SystemExit(f"[{c['id']}] la carta no tiene modelo 3D en el juego")
        src = (SRC / c["modelo"]).resolve()
        dst = arch / "monster" / f"{n // 100 * 100:03d}" / f"m{n:03d}.igb"
        dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(src, dst)
        print(f"  {c['id']}: modelo {src.name} -> {dst.relative_to(OUT)}")
        if c.get("modelo_hd"):
            hd_tex = Image.open(SRC / c["modelo_hd"]).convert("RGB")
            for w, h, key in imagenes_rgb(src):
                hd_tex.resize((w * K, h * K), Image.LANCZOS).convert("RGBA").save(HD / f"{key:016X}_{w}x{h}.png")
                print(f"    textura {key:016X} {w}x{h} -> HD x{K}")
                break
    # pack HD: XRTP sin comprimir y despues BC3
    raw = SRC / "salida" / "cartas_raw.pack"
    subprocess.run([sys.executable, str(PORT / "tools" / "texturas_pack.py"), str(HD), str(raw)], check=True)
    subprocess.run([str(PORT / "tools" / "bc3" / "bc3pack.exe"), str(raw), str(OUT / "cartas.pack")], check=True)
    sys.path.insert(0, str(PORT / "tools"))
    import achicar_pack                       # 2026-11-21: the opaque ones as BC1, half the size
    achicar_pack.achicar_aqui(OUT / "cartas.pack")
    raw.unlink()
    for lang, d in hd_l.items():
        if not any(d.iterdir()):
            continue
        code = LENGUA[lang][0] or "es"
        subprocess.run([sys.executable, str(PORT / "tools" / "texturas_pack.py"), str(d), str(raw)], check=True)
        subprocess.run([str(PORT / "tools" / "bc3" / "bc3pack.exe"), str(raw), str(OUT / f"cartas_{code}.pack")],
                       check=True)
        achicar_pack.achicar_aqui(OUT / f"cartas_{code}.pack")
        raw.unlink()
    # los datos para el juego
    with open(OUT / "cartas.txt", "w", encoding="utf-8", newline="\r\n") as f:
        f.write("# Mod de cartas (tools/cartas_mod.py): lo lee el juego al arrancar.\n")
        f.write("# F2 > Cartas nuevas: Si / No (se aplica al reiniciar).\n\n")
        for c in cartas:
            f.write(f"[{c['id']}]\nnombre = {c['nombre']}\ndescripcion = {c['descripcion']}\n")
            for suf in ("en", "fr", "de", "it"):
                for campo in ("nombre_", "descripcion_"):
                    if c.get(campo + suf):
                        f.write(f"{campo}{suf} = {c[campo + suf]}\n")
            if c["w"] is not None:
                f.write(f"datos = 0x{c['w']:08X}\n")
            if c.get("efecto"):
                f.write(f"efecto = {c['efecto']}\n")
            if c.get("copias"):
                f.write(f"copias = {int(c['copias'])}\n")
            if solo_es(c):
                f.write("solo_espanol = si\n")
            for campo in ("premio_peso", "premio_ranuras", "premio_rival"):
                if c.get(campo):
                    f.write(f"{campo} = {c[campo]}\n")
            f.write("\n")
    # 2026-11-19: la huella de la carta grande de cada k (la del mod si la
    # reemplaza), la misma que calcula el kit: premios_extra.c la busca entre
    # lo que se dibuja para poner "Nueva" sobre las cartas del premio
    with open(OUT / "huellas.txt", "w", encoding="ascii", newline="\r\n") as f:
        for c in todas_:
            p = tga(game, c["id"])
            m = OUT / "archivos" / p.relative_to(game)
            src = m if m.is_file() else p
            if src.is_file():
                f.write(f"{c['k']} {cv.huella(Image.open(src)):016X}\n")
    print(f"Mod listo en {OUT}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
