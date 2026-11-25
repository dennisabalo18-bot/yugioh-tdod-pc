"""Arma el .igb de un modelo propio sobre el de un monstruo del juego.

    py -3 tools/modelos/armar_igb.py volcado.npz textura.png salida.igb [donante.igb] [--alto=N]

--alto: el alto en el juego (por defecto ALTO, el de m129).
--cuerpo: centrar en X/Y por el cuerpo (mitad entre los percentiles 5 y 95
  de los vertices) en vez de por la caja: un brazo o rama larga de un lado
  corre la caja y el cuerpo queda fuera de la casilla (Hombre de Nieve).
--como=mNNN: el tamano sale del modelo original que se reemplaza (medidas en
  modding/modelos/medidas_originales.txt, medidas en Blender con la pose del
  primer cuadro): el nuestro ocupa la misma silueta de frente (alto x ancho)
  que el original. Asi la camara de la batalla, que esta pensada para el
  original, lo encuadra igual (el Hombre de Nieve con alto 15 era casi el
  doble de ancho que el Sectario y tapaba media pantalla). Pone --cuerpo.
--derecha=N: corre el modelo N unidades a la derecha (vista del jugador).
--girar=G: gira el modelo G grados sobre su eje vertical (positivo: hacia la
  izquierda visto desde arriba; el mismo sentido que el visor).
--suelo: a que altura queda la base (por defecto 0: los pies en el piso; las
  cerezas flotan, para que en la batalla entren en cuadro como un monstruo alto).

Donante: m373 (Tutor de recaudacion): sus 19 huesos tienen los mismos nombres
que el esqueleto del volcado (tools/modelos/riggear_sincronizador.py).

Lo que se cambia en el archivo del donante (igb.IGBEdit):
- huesos: matriz local (campo 8), inversa de bind (campo 12), su
  igMatrixObject (campo 13) y las claves de su animacion (igTransformSequence1_5:
  traslacion, rotacion x,y,z,w, escala; mismos tiempos). Los igTransform
  intermedios (RarmRT...) quedan en identidad; m373all (padre de todo) lleva el
  movimiento de Spine1, que no tiene animacion propia.
- mallas: se usan las dos con varios huesos por vertice (formato 0x30223: 3 UV
  iguales, 2 pesos y 2 indices por vertice seguidos, en un bloque de 4 por
  vertice con la segunda mitad en cero) con
  hasta 16 huesos cada una (el maximo del juego). Las otras 36 salen de la
  escena (la lista de hijos de su igAttrSet).
- texturas: asi.png (color) pasa a 512x512 con la del modelo; asi_s.png (brillo)
  a gris segun la luz de la textura.
Piel del juego: vertice x campo 12 x matriz del hueso en el mundo (comprobado
con igb_gltf.py en m373). Ejes: el juego tiene Y arriba y el monstruo mira a +Z
(su izquierda +X); Blender Z arriba, mira a +X, izquierda +Y.
"""
import pathlib
import struct
import sys

import numpy as np
from PIL import Image

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent.parent))
from igb import IGBEdit  # noqa: E402

GAME = pathlib.Path(__file__).resolve().parents[3] / "Yu-Gi-Oh TDOD"
DONANTE = GAME / "monster" / "300" / "m373.igb"
MALLAS = (1697, 1690)          # igGeometry del donante con varios huesos
PADRE_MALLAS = 634             # igAttrSet que las tiene como hijas (con las otras)
HIJOS = (1703, 1692)           # los igAttrSet de esas dos mallas (sus hijos directos)
MAX_PALETA = 16
SUELO = 0.0                    # los pies en el piso: en el juego el donante tiene los talones
                               # en -22.6 en el bind, pero su animacion lo sube hasta 0
ALTO = 22.8                    # alto del modelo que se reemplaza (m129 de Control de clima,
                               # medido en su pose): el juego agranda cada modelo segun su
                               # numero; con el tamano del nuestro (67) se veia 3 veces grande

C = np.array([[0, 1, 0, 0], [0, 0, 1, 0], [1, 0, 0, 0], [0, 0, 0, 1]], np.float64)   # Blender -> juego


def a_juego(m):
    return C @ m @ C.T


def d3d(m):
    """matriz de columnas -> los 16 floats del archivo (fila de D3D)"""
    return m.T.reshape(-1)


def quat(r):
    """cuaternion x,y,z,w de una matriz de rotacion 3x3 (columnas)"""
    t = np.trace(r)
    if t > 0:
        s = np.sqrt(t + 1) * 2
        w, x, y, z = s / 4, (r[2, 1] - r[1, 2]) / s, (r[0, 2] - r[2, 0]) / s, (r[1, 0] - r[0, 1]) / s
    elif r[0, 0] > r[1, 1] and r[0, 0] > r[2, 2]:
        s = np.sqrt(1 + r[0, 0] - r[1, 1] - r[2, 2]) * 2
        w, x, y, z = (r[2, 1] - r[1, 2]) / s, s / 4, (r[0, 1] + r[1, 0]) / s, (r[0, 2] + r[2, 0]) / s
    elif r[1, 1] > r[2, 2]:
        s = np.sqrt(1 + r[1, 1] - r[0, 0] - r[2, 2]) * 2
        w, x, y, z = (r[0, 2] - r[2, 0]) / s, (r[0, 1] + r[1, 0]) / s, s / 4, (r[1, 2] + r[2, 1]) / s
    else:
        s = np.sqrt(1 + r[2, 2] - r[0, 0] - r[1, 1]) * 2
        w, x, y, z = (r[1, 0] - r[0, 1]) / s, (r[0, 2] + r[2, 0]) / s, (r[1, 2] + r[2, 1]) / s, s / 4
    q = np.array([x, y, z, w])
    return q / np.linalg.norm(q)


CUERPO = False
COMO = None
DERECHA = 0.0
GIRAR = 0.0
MEDIDAS = pathlib.Path(__file__).resolve().parents[2] / "modding" / "modelos" / "medidas_originales.txt"


def medida(m):
    for ln in MEDIDAS.read_text().splitlines():
        t = ln.split()
        if len(t) > 2 and t[0] == "RES" and t[1] == m:
            return {t[i]: float(t[i + 1]) for i in range(2, len(t) - 1, 2)}
    raise SystemExit(f"{m} no esta en {MEDIDAS}: medirlo primero (igb_gltf + Blender)")


def armar(npz, tex_png, salida, donante=DONANTE):
    v = dict(np.load(npz))
    # bajar todo para que los pies queden en SUELO (en Blender el piso es z=0)
    dz = SUELO - float(v["pos"][:, 2].min())
    v["pos"] = v["pos"] + np.array([0, 0, dz], np.float32)
    for k in ("rest", "pose"):
        v[k] = v[k].copy()
        v[k][..., 2, 3] += dz
    if CUERPO:
        c = (np.percentile(v["pos"][:, :2], 5, axis=0) + np.percentile(v["pos"][:, :2], 95, axis=0)) / 2
        v["pos"][:, :2] -= c
        for k in ("rest", "pose"):
            v[k][..., :2, 3] -= c
        print("centrado por el cuerpo: x %.3f y %.3f" % tuple(c))
    if GIRAR:
        # sobre el eje vertical (Blender Z) por el centro del cuerpo: con --cuerpo
        # es el origen; si no, el centro de la caja
        c = np.zeros(3, np.float32)
        if not CUERPO:
            c[:2] = (v["pos"][:, :2].min(axis=0) + v["pos"][:, :2].max(axis=0)) / 2
        g = np.radians(GIRAR)
        R = np.array([[np.cos(g), -np.sin(g), 0], [np.sin(g), np.cos(g), 0], [0, 0, 1]], np.float32)
        v["pos"] = (v["pos"] - c) @ R.T + c
        v["nrm"] = v["nrm"] @ R.T
        M = np.eye(4, dtype=np.float32)
        M[:3, :3] = R
        M[:3, 3] = c - R @ c
        for k in ("rest", "pose"):
            v[k] = M @ v[k]
        print("girado %.1f grados" % GIRAR)
    if COMO:
        o = medida(COMO)
        h = float(v["pos"][:, 2].max() - v["pos"][:, 2].min())
        w = float(v["pos"][:, 1].max() - v["pos"][:, 1].min())   # ancho de frente (Blender Y = juego X)
        ALTO_ = ((o["alto"] - o["base"]) * o["ancho"] * h / w) ** 0.5
        print("como %s (%.1f x %.1f de frente): alto %.1f, ancho %.1f"
              % (COMO, o["alto"] - o["base"], o["ancho"], ALTO_, ALTO_ * w / h))
    else:
        ALTO_ = ALTO
    esc = ALTO_ / float(v["pos"][:, 2].max() - SUELO)
    v["pos"] = (v["pos"] - [0, 0, SUELO]) * esc + [0, 0, SUELO]
    for k in ("rest", "pose"):
        v[k][..., :3, 3] = (v[k][..., :3, 3] - [0, 0, SUELO]) * esc + [0, 0, SUELO]
    if DERECHA:
        # a la derecha de la camara propia (juego +X = Blender +Y): en la batalla
        # el monstruo propio sale abajo a la izquierda y se corta
        v["pos"][:, 1] += DERECHA
        for k in ("rest", "pose"):
            v[k][..., 1, 3] += DERECHA
    print("bajado %.1f, escala %.3f, derecha %.1f" % (dz, esc, DERECHA))
    names = [str(n) for n in v["names"]]
    rest = np.array([a_juego(m) for m in v["rest"]])
    pose = np.array([[a_juego(m) for m in fr] for fr in v["pose"]])
    nfr = len(pose)
    e = IGBEdit(str(donante))
    g = e.g
    inv = {i: r for r, i in g.ent_obj.items()}

    # arbol del donante
    par = {}
    for i, (n, f) in enumerate(g.objs):
        if n in ("igGroup", "igTransform", "igJoint", "igAttrSet", "igSimpleShader") and f.get(7, -1) >= 0:
            for c in g.list_refs(f[7]):
                par[c] = inv[i]
    nodos = {inv[i]: (n, f) for i, (n, f) in enumerate(g.objs) if n in ("igTransform", "igJoint")}
    por_nombre = {f[3]: r for r, (n, f) in nodos.items()}
    faltan = [n for n in names if n not in por_nombre or nodos[por_nombre[n]][0] != "igJoint"]
    if faltan:
        raise SystemExit("huesos que no estan en el donante: %s" % faltan)
    bi = {por_nombre[n]: i for i, n in enumerate(names)}
    raiz = por_nombre["m373all"]
    s1 = por_nombre["Spine1"]

    # locales por cuadro: m373all lleva el movimiento de Spine1; los igTransform
    # sin animacion en identidad; los huesos relativos a su padre real
    def tiene_seq(r):
        return nodos[r][1].get(11, -1) >= 0

    local = {}
    for t in range(nfr):
        W = {}

        def mundo(r):
            if r in W:
                return W[r]
            p = par.get(r)
            pw = mundo(p) if p in nodos else np.eye(4)
            if r == raiz:
                L = pose[t][bi[s1]] @ np.linalg.inv(pose[0][bi[s1]])
            elif r in bi:
                if tiene_seq(r) or t == 0:
                    L = np.linalg.inv(pw) @ pose[t][bi[r]]
                else:
                    L = local[r][0]
            else:
                L = np.eye(4)
            local.setdefault(r, []).append(L)
            W[r] = pw @ L
            return W[r]

        for r in nodos:
            mundo(r)
    # el bind: el reposo del esqueleto con m373all en identidad
    for r, (n, f) in nodos.items():
        e.set_mat(r, 8, d3d(local[r][0]))
        if n == "igJoint":
            e.set_mat(r, 12, d3d(np.linalg.inv(rest[bi[r]])))
            mo = f[13]
            if mo >= 0 and g.obj(mo) and g.obj(mo)[0] == "igMatrixObject":
                e.set_mat(mo, 3, d3d(rest[bi[r]]))
        sq = f.get(11, -1)
        if sq < 0:
            continue
        sf = g.obj(sq)[1]
        tl = g.obj(sf[12])[1]
        tiempos = np.frombuffer(e.mem(tl[5]), np.int64)[:tl[3]]
        dur = tiempos[-1] / 1e9 if len(tiempos) > 1 else 1.0
        frames = [min(nfr - 1, round(x / 1e9 / dur * (nfr - 1))) for x in tiempos]
        Ls = [local[r][k] for k in frames]
        for campo, datos in ((3, [L[:3, 3] for L in Ls]),
                             (4, [quat(L[:3, :3].T) for L in Ls]),
                             (5, [np.ones(3) for _ in Ls])):
            ref = sf.get(campo, -1)
            if ref < 0:
                continue
            lo = g.obj(ref)[1]
            arr = np.array(datos, np.float32)
            if lo[3] != len(arr):
                raise SystemExit("claves distintas en %s" % n)
            e.set_mem(lo[5], arr.tobytes())

    # mallas: triangulos repartidos en 2 grupos de huesos (<= 16 cada uno)
    P = (v["pos"].astype(np.float64) @ C[:3, :3].T).astype(np.float32)
    N = (v["nrm"].astype(np.float64) @ C[:3, :3].T).astype(np.float32)
    UV = v["uv"].astype(np.float32)
    UV[:, 1] = 1 - UV[:, 1]    # Blender: v=0 abajo; el juego (Direct3D): v=0 = primera fila
    Jn, Wt = v["joints"], v["weights"].copy()
    # dos pesos por vertice (el formato del donante), normalizados
    Wt[:, 2:] = 0
    Wt /= np.maximum(Wt.sum(axis=1, keepdims=True), 1e-6)
    arriba = {"Spine1", "Spine2", "Spine3", "Rbicep", "Rforearm", "Rhand", "Lbicep", "Lforearm", "Lhand"}
    grupos = [set(names.index(n) for n in arriba),
              set(i for i, n in enumerate(names) if n not in arriba - {"Spine1", "Spine2"})]
    tri_grupo = []
    for t in range(len(P) // 3):
        hs = set(int(Jn[3 * t + k, j]) for k in range(3) for j in range(2) if Wt[3 * t + k, j] > 0)
        tri_grupo.append(0 if hs <= grupos[0] else 1 if hs <= grupos[1] else
                         (0 if len(hs & grupos[0]) >= len(hs & grupos[1]) else 1))
    tri_grupo = np.array(tri_grupo)
    for gi, (geo, attrset) in enumerate(zip(MALLAS, HIJOS)):
        pal = sorted(grupos[gi])
        assert len(pal) <= MAX_PALETA
        pidx = {b: i for i, b in enumerate(pal)}
        sel = np.repeat(tri_grupo == gi, 3)
        p, n_, uv, j, w = P[sel], N[sel], UV[sel], Jn[sel][:, :2], Wt[sel][:, :2].copy()
        # un hueso fuera de la paleta: su peso pasa al otro
        jj = np.zeros((len(p), 2), np.uint8)
        for k in range(len(p)):
            for c in range(2):
                b = int(j[k, c])
                if b in pidx:
                    jj[k, c] = pidx[b]
                else:
                    w[k, c] = 0
            if w[k].sum() <= 0:
                w[k] = (1, 0)
                jj[k, 0] = pidx.get(int(j[k, 0]), pidx[names.index("Spine1")])
        w /= w.sum(axis=1, keepdims=True)
        # formato 0x?223: 2 pesos y 2 indices por vertice, seguidos; el bloque del
        # juego mide 4 por vertice y la otra mitad queda en cero
        w4 = np.concatenate([w.astype(np.float32).reshape(-1), np.zeros(2 * len(p), np.float32)])
        j4 = np.concatenate([jj.reshape(-1), np.zeros(2 * len(p), np.uint8)])
        gf = g.obj(geo)[1]
        ga_ref = g.list_refs(gf[8])[0]
        va_ref = g.obj(ga_ref)[1][4]
        va = g.obj(va_ref)[1]
        tab = struct.unpack_from("<19i", e.mem(va[3]), 0)
        e.set_mem(tab[0], p.astype(np.float32).tobytes())
        e.set_mem(tab[1], n_.astype(np.float32).tobytes())
        for k in (11, 12, 13):
            if tab[k] >= 0:
                e.set_mem(tab[k], uv.astype(np.float32).tobytes())
        e.set_mem(va[8], w4.tobytes())
        e.set_mem(va[9], j4.tobytes())
        e.set_i32(va_ref, 4, len(p))
        e.set_i32(ga_ref, 7, len(p) // 3)
        # la paleta: los igMatrixObject de esos huesos
        for a in g.list_refs(g.obj(attrset)[1][8]):
            ao = g.obj(a)
            if ao and ao[0] == "igVertexBlendMatrixListAttr":
                lst = ao[1][4]
                mos = [nodos[por_nombre[names[b]]][1][13] for b in pal]
                e.set_mem(g.obj(lst)[1][5], np.array(mos, np.int32).tobytes())
                e.set_i32(lst, 3, len(mos))
                e.set_i32(lst, 4, len(mos))
        print("malla %d: %d triangulos, %d huesos" % (gi, len(p) // 3, len(pal)))

    # solo esas dos mallas en la escena
    lst = g.obj(PADRE_MALLAS)[1][7]
    e.set_mem(g.obj(lst)[1][5], np.array(HIJOS, np.int32).tobytes())
    e.set_i32(lst, 3, len(HIJOS))
    e.set_i32(lst, 4, len(HIJOS))

    # texturas
    tex = Image.open(tex_png).convert("RGB")
    for i, (n, f) in enumerate(g.objs):
        if n != "igImage":
            continue
        ref = inv[i]
        if f.get(23) == "asi.png":
            im = tex.resize((512, 512), Image.LANCZOS)
            e.set_mem(f[14], np.asarray(im, np.uint8).tobytes())
            for campo, val in ((3, 512), (4, 512), (13, 512 * 512 * 3), (20, 512 * 3)):
                e.set_i32(ref, campo, val)
        elif f.get(23) == "asi_s.png":
            gr = np.asarray(tex.convert("L").resize((f[3], f[4]), Image.LANCZOS), np.float32)
            gr = np.clip(gr * (80.0 / max(gr.mean(), 1)), 0, 255).astype(np.uint8)
            e.set_mem(f[14], gr.tobytes())
    e.save(str(salida))
    print("guardado", salida)


if __name__ == "__main__":
    for a in sys.argv[1:]:
        if a.startswith("--alto="):
            ALTO = float(a[7:])
        if a == "--cuerpo":
            CUERPO = True
        if a.startswith("--derecha="):
            DERECHA = float(a[10:])
        if a.startswith("--como="):
            COMO = a[7:]
            CUERPO = True
        if a.startswith("--girar="):
            GIRAR = float(a[8:])
        if a.startswith("--suelo="):
            SUELO = float(a[8:])
    pos = [a for a in sys.argv[1:] if not a.startswith("--")]
    armar(pos[0], pos[1], pos[2], *(pos[3:4] or []))
