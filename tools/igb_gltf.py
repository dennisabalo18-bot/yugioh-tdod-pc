"""Exporta un modelo del juego (.igb de Alchemy) a glTF para abrirlo en
Blender: malla, UV, texturas, esqueleto y pesos.

    py -3 tools/igb_gltf.py monster/000/m001.igb carpeta_salida

Lo que dice el archivo (comprobado en m001, 2026-11-12; lector en igb.py):
- igSceneInfo campo 6 = raiz de la escena. Arbol de igGroup / igAttrSet /
  igTransform / igJoint / igGeometry; los hijos en el campo 7 (igNodeList).
- igTransform / igJoint: campo 3 nombre, 8 matriz local (16 floats, fila
  mayor de D3D: la traslacion en los ultimos 4 = mismo orden que glTF), 11 la
  animacion (igTransformSequence1_5). igJoint: 12 otra matriz, 13 su
  igMatrixObject (la matriz del hueso en tiempo de juego).
- igAttrSet campo 8 = lista de atributos que valen para sus hijos:
  igTextureBindAttr (campo 4 igTextureAttr -> campo 12 igImage, campo 5 la
  unidad: 0 color, 1 mapa _s gris), igVertexBlendMatrixListAttr (campo 4
  igMatrixObjectList: la paleta de huesos de esa malla, por su igMatrixObject).
- igGeometry campo 8 = lista con igGeometryAttr1_5: campo 4 igVertexArray1_1,
  6 primitiva (3 = triangulos, 4 = tiras), 7 cantidad, 9 indices (-1 = sin
  indices), 13 igIntList con el largo de cada tira.
- igVertexArray1_1: campo 3 tabla de 19 MemoryRef (0 posicion f32x3,
  1 normal f32x3, 11 UV f32x2), 4 vertices, 7 formato, 8 pesos f32x4,
  9 indices de hueso u8 x3 o x4 (en la paleta de su malla; pesos f32 igual).
- igImage: 3 ancho, 4 alto, 12 formato (5 RGB, 7 RGBA, 0 gris, 14 DXT1,
  65536 paleta con igClut en 18), 14 pixeles (lineales en el archivo),
  23 nombre.
"""
import base64
import io
import json
import pathlib
import struct
import sys

import numpy as np
from PIL import Image

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from igb import IGB  # noqa: E402

NODES = ("igGroup", "igAttrSet", "igTransform", "igJoint", "igGeometry",
         "igSimpleShader", "igUvAnimeShader")
# 2026-11-16 (perfil: the Mystic Tomato without its eyes, the Dark Sage's
# staff without its glow): igUvAnimeShader is a group too (its children in
# field 7) and was skipped. It moves the texture of a unit (field 28; name
# "UnitID:n") over time: fields 20..23 are igUvAnimeKeyList of u offset, v
# offset, u scale, v scale; each igUvAnimeKey: 3 interpolation (0 step,
# 1 linear, 2 hold, 3 the end), 4 time (s), 5 value; 13 the loop (s). Kept
# as the material's extras "uvanim" (unit 0's, the one drawn here). The
# additive parts (igBlendStateAttr on + igBlendFunctionAttr dst ONE, as the
# Sage's "blendmode_add") as extras "blend": "add".


def image_rgba(g, ref):
    o = g.obj(ref)
    if not o or o[0] != "igImage":
        return None, None
    f = o[1]
    w, h, fmt, raw = f[3], f[4], f[12], g.mem_bytes(f[14])
    a = np.frombuffer(raw, np.uint8)
    try:
        if fmt == 5:
            rgba = np.dstack([a[:w * h * 3].reshape(h, w, 3), np.full((h, w), 255, np.uint8)])
        elif fmt == 7:
            rgba = a[:w * h * 4].reshape(h, w, 4)
        elif fmt == 0:
            g1 = a[:w * h].reshape(h, w)
            rgba = np.dstack([g1, g1, g1, np.full((h, w), 255, np.uint8)])
        elif fmt == 14:
            hdr = struct.pack("<4sIIIIIII44xII4s20xI16x", b"DDS ", 124, 0x1007, h, w,
                              w * h // 2, 0, 1, 32, 4, b"DXT1", 0x1000)
            rgba = np.asarray(Image.open(io.BytesIO(hdr + bytes(raw[:w * h // 2]))).convert("RGBA"))
        elif fmt == 65536:
            c = g.obj(f[18])[1]
            pal = np.frombuffer(g.mem_bytes(c[6] if 6 in c else c[5]), np.uint8).reshape(-1, 4)
            rgba = pal[a[:w * h].reshape(h, w)]
        else:
            return f.get(23, ""), None
    except Exception:
        return f.get(23, ""), None
    return f.get(23, ""), Image.fromarray(np.ascontiguousarray(rgba), "RGBA")


def export(src, out):
    sys.setrecursionlimit(10000)
    g = IGB(str(src))
    out.mkdir(parents=True, exist_ok=True)
    root = next(f[6] for n, f in g.objs if n == "igSceneInfo")
    nodes, meshes, images, textures, materials = [], [], [], [], []
    accessors, views = [], []
    blob = bytearray()
    tex_of_image = {}
    joint_node = {}      # igMatrixObject del hueso -> nodo glTF
    node_of_ref = {}
    geoms = []           # (geometria, atributos heredados)

    def add(arr, target=None, comp=5126, typ="VEC3", minmax=False):
        arr = np.ascontiguousarray(arr)
        while len(blob) % 4:
            blob.append(0)
        views.append({"buffer": 0, "byteOffset": len(blob), "byteLength": arr.nbytes,
                      **({"target": target} if target else {})})
        blob.extend(arr.tobytes())
        acc = {"bufferView": len(views) - 1, "componentType": comp, "count": len(arr), "type": typ}
        if minmax:
            acc["min"] = np.atleast_1d(arr.min(axis=0)).tolist()
            acc["max"] = np.atleast_1d(arr.max(axis=0)).tolist()
        accessors.append(acc)
        return len(accessors) - 1

    def uvanim(f):
        def keys(ref):
            out = []
            for r in g.list_refs(ref):
                k = g.obj(r)
                if k and k[0] == "igUvAnimeKey":
                    out.append([float(k[1][4]), float(k[1][5]), int(k[1][3])])
            return out
        return {"dur": float(f.get(13, 0.0)), "u": keys(f[20]), "v": keys(f[21]),
                "su": keys(f[22]), "sv": keys(f[23])}

    def walk(ref, parent, inherited):
        o = g.obj(ref)
        if not o or o[0] not in NODES:
            return
        n, f = o
        me = parent
        if n in ("igTransform", "igJoint"):
            nodes.append({"name": f[3] or n, "matrix": list(f[8])})
            me = len(nodes) - 1
            node_of_ref[ref] = me
            if f.get(11, -1) >= 0:
                seqs.append((me, f[11]))
            if parent is not None:
                nodes[parent].setdefault("children", []).append(me)
            else:
                top.append(me)
            if n == "igJoint":
                joint_node[f[13]] = me
                bind[me] = np.array(f[12], np.float64).reshape(4, 4)
        attrs = dict(inherited)
        if n == "igAttrSet":
            for a in g.list_refs(f[8]):
                ao = g.obj(a)
                if not ao:
                    continue
                if ao[0] == "igTextureBindAttr":
                    attrs["tex%d" % ao[1][5]] = ao[1][4]
                elif ao[0] == "igBlendStateAttr":
                    attrs["blend_on"] = ao[1].get(4, 0)
                elif ao[0] == "igBlendFunctionAttr":
                    attrs["blend_fn"] = (ao[1].get(4, -1), ao[1].get(5, -1))
                elif ao[0] == "igVertexBlendMatrixListAttr":
                    attrs["paleta"] = g.list_refs(ao[1][4])
        if n == "igUvAnimeShader" and f.get(28, 0) == 0:
            attrs["uvanim"] = uvanim(f)
        if n == "igGeometry":
            geoms.append((f, attrs))
            return
        if 7 in f and f[7] >= 0:
            for c in g.list_refs(f[7]):
                walk(c, me, attrs)

    top = []
    seqs = []            # (nodo, igTransformSequence1_5)
    bind = {}            # nodo del hueso -> campo 12 (matriz inversa de bind)
    walk(root, None, {})

    # matrices globales de reposo e inversas (para la piel)
    glob = {}

    def world(i, m):
        glob[i] = m
        for c in nodes[i].get("children", []):
            world(c, np.array(nodes[c]["matrix"], np.float64).reshape(4, 4) @ m)

    for t in top:
        world(t, np.array(nodes[t]["matrix"], np.float64).reshape(4, 4))
    joints = sorted(set(joint_node.values()))
    jidx = {n: i for i, n in enumerate(joints)}
    # la inversa de bind es el campo 12 del igJoint (los vertices estan en la
    # pose de bind, que no es el primer cuadro de la animacion)
    ibm = np.array([bind.get(j, np.linalg.inv(glob[j])) for j in joints], np.float32)

    tex_index = {}       # imagen -> textura glTF

    def material(texref, extras=None):
        if texref is None:
            return None
        ta = g.obj(texref)
        img = ta[1].get(12, -1) if ta else -1
        clave = (img, json.dumps(extras, sort_keys=True) if extras else "")
        if clave not in tex_of_image:
            if img not in tex_index:
                name, im = image_rgba(g, img)
                if im is None:
                    tex_index[img] = None
                else:
                    fn = pathlib.Path(name or "tex%d" % img).stem + ".png"
                    im.save(out / fn)
                    images.append({"uri": fn})
                    textures.append({"source": len(images) - 1})
                    tex_index[img] = (len(textures) - 1, pathlib.Path(fn).stem)
            if tex_index[img] is None:
                tex_of_image[clave] = None
            else:
                ti, nombre = tex_index[img]
                mat = {"name": nombre,
                       "pbrMetallicRoughness": {"baseColorTexture": {"index": ti},
                                                "metallicFactor": 0, "roughnessFactor": 1},
                       "doubleSided": True}
                if extras:
                    mat["extras"] = extras
                materials.append(mat)
                tex_of_image[clave] = len(materials) - 1
        return tex_of_image[clave]

    prims = []
    tris = 0
    for f, attrs in geoms:
        for a in g.list_refs(f[8]):
            ga = g.obj(a)
            if not ga or ga[0] != "igGeometryAttr1_5" or ga[1][6] not in (3, 4):
                continue
            va = g.obj(ga[1][4])[1]
            nv = va[4]
            if nv < 3:
                continue
            tab = struct.unpack_from("<19i", g.mem_bytes(va[3]), 0)
            pos = np.frombuffer(g.mem_bytes(tab[0]), np.float32).reshape(-1, 3)[:nv]
            prim = {"attributes": {"POSITION": add(pos, 34962, minmax=True)}, "mode": 4}
            ntri = nv // 3
            if ga[1][6] == 4:       # tiras: campo 13 = igIntList con el largo de cada una
                largos = [nv]
                lo = g.obj(ga[1][13]) if ga[1].get(13, -1) >= 0 else None
                if lo and lo[0].startswith("igPrimLengthArray"):   # 3 memoria, 4 cantidad, 5 bits
                    largos = np.frombuffer(g.mem_bytes(lo[1][3]),
                                           np.uint16 if lo[1][5] == 16 else np.uint32)[:lo[1][4]].tolist()
                elif lo:
                    largos = g.list_refs(ga[1][13])
                idx, base = [], 0
                for L in largos:
                    for i in range(L - 2):
                        a, b, c = base + i, base + i + 1, base + i + 2
                        if i % 2:
                            a, b = b, a
                        if len({a, b, c}) == 3:
                            idx.append((a, b, c))
                    base += L
                prim["indices"] = add(np.array(idx, np.uint16).reshape(-1), 34963, comp=5123, typ="SCALAR")
                ntri = len(idx)
            if tab[1] >= 0:
                nrm = np.frombuffer(g.mem_bytes(tab[1]), np.float32).reshape(-1, 3)[:nv]
                prim["attributes"]["NORMAL"] = add(nrm, 34962)
            if tab[11] >= 0:
                uv = np.frombuffer(g.mem_bytes(tab[11]), np.float32).reshape(-1, 2)[:nv].copy()
                # v como en Direct3D (0 = primera fila de la imagen), igual que glTF: sin
                # voltear. (Antes se volteaba; las texturas del juego se ven bien de las dos
                # formas, pero un modelo propio en el juego mostro que el juego no voltea.)
                prim["attributes"]["TEXCOORD_0"] = add(uv, 34962, typ="VEC2")
            pal = attrs.get("paleta")
            if pal and va.get(8, -1) >= 0 and va.get(9, -1) >= 0:
                # huesos por vertice: el digito 0xF00 del formato (0x223 = 2, 0x333 = 3...);
                # los pesos van seguidos, k por vertice (el bloque puede ser mas largo)
                k = (va[7] >> 8) & 0xF or len(g.mem_bytes(va[9])) // nv
                w = np.frombuffer(g.mem_bytes(va[8]), np.float32)[:nv * k].reshape(nv, k)
                ix = np.frombuffer(g.mem_bytes(va[9]), np.uint8)[:nv * k].reshape(nv, k)
                if k < 4:
                    w = np.hstack([w, np.zeros((nv, 4 - k), np.float32)])
                    ix = np.hstack([ix, np.zeros((nv, 4 - k), np.uint8)])
                elif k > 4:                                 # glTF: los 4 pesos mayores
                    o = np.argsort(-w, axis=1)[:, :4]
                    w = np.take_along_axis(w, o, 1)
                    ix = np.take_along_axis(ix, o, 1)
                w = w.copy()
                lut = np.array([jidx.get(joint_node.get(m, -1), 0) for m in pal] + [0] * 256, np.uint16)
                j = lut[np.minimum(ix, len(pal) - 1)] if len(pal) else np.zeros_like(ix, np.uint16)
                w[ix >= len(pal)] = 0
                s = w.sum(axis=1, keepdims=True)
                w = np.where(s > 0, w / np.where(s > 0, s, 1), np.array([1, 0, 0, 0], np.float32))
                prim["attributes"]["JOINTS_0"] = add(j.astype(np.uint16), 34962, comp=5123, typ="VEC4")
                prim["attributes"]["WEIGHTS_0"] = add(w.astype(np.float32), 34962, typ="VEC4")
            extras = {}
            if attrs.get("uvanim"):
                extras["uvanim"] = attrs["uvanim"]
            if attrs.get("blend_on") and attrs.get("blend_fn", (-1, -1))[1] == 1:
                extras["blend"] = "add"
            m = material(attrs.get("tex0"), extras or None)
            if m is not None:
                prim["material"] = m
            prims.append(prim)
            tris += ntri
    meshes.append({"name": src.stem, "primitives": prims})
    nodes.append({"name": src.stem, "mesh": 0, **({"skin": 0} if joints else {})})
    top.append(len(nodes) - 1)
    gl = {"asset": {"version": "2.0", "generator": "ygo-port igb_gltf.py"},
          "scene": 0, "scenes": [{"nodes": top}], "nodes": nodes, "meshes": meshes,
          "materials": materials, "textures": textures, "images": images,
          "samplers": [{}]}
    # animacion: igTransformSequence1_5 campo 12 tiempos (ns, igLongList),
    # 3 traslacion, 4 rotacion (x,y,z,w de la matriz de D3D: en glTF va
    # conjugada), 5 escala; una sola animacion en bucle por archivo
    chans, samps = [], []

    def lista(ref, dt, k):
        o = g.obj(ref) if ref >= 0 else None
        if not o or not o[1][3]:
            return None
        return np.frombuffer(g.mem_bytes(o[1][5]), dt).reshape(-1, k)[:o[1][3]]

    for node, sref in seqs:
        so = g.obj(sref)
        if not so or so[0] != "igTransformSequence1_5":
            continue
        sf = so[1]
        t = lista(sf[12], np.int64, 1)
        if t is None:
            continue
        ta = add((t[:, 0] / 1e9).astype(np.float32), typ="SCALAR", minmax=True)
        for campo, path, k in ((3, "translation", 3), (4, "rotation", 4), (5, "scale", 3)):
            v = lista(sf[campo], np.float32, k)
            if v is None or len(v) != len(t):
                continue
            if path == "rotation":
                v = v * np.array([-1, -1, -1, 1], np.float32)
            samps.append({"input": ta, "output": add(v.astype(np.float32), typ="VEC4" if k == 4 else "VEC3"),
                          "interpolation": "LINEAR"})
            chans.append({"sampler": len(samps) - 1, "target": {"node": node, "path": path}})
    if chans:
        gl["animations"] = [{"name": "reposo", "channels": chans, "samplers": samps}]
    if joints:
        gl["skins"] = [{"joints": joints, "inverseBindMatrices": add(ibm.reshape(-1, 16), typ="MAT4")}]
    gl["accessors"] = accessors
    gl["bufferViews"] = views
    gl["buffers"] = [{"byteLength": len(blob), "uri": src.stem + ".bin"}]
    for t in gl["textures"]:
        t["sampler"] = 0
    (out / (src.stem + ".bin")).write_bytes(bytes(blob))
    (out / (src.stem + ".gltf")).write_text(json.dumps(gl, indent=1))
    print(f"{src.name}: {len(prims)} mallas, {tris} triangulos, {len(joints)} huesos, "
          f"{len(images)} texturas -> {out / (src.stem + '.gltf')}")
    return gl


if __name__ == "__main__":
    src = pathlib.Path(sys.argv[1])
    if not src.is_file():
        src = pathlib.Path(__file__).resolve().parent.parent.parent / "Yu-Gi-Oh TDOD" / sys.argv[1]
    export(src, pathlib.Path(sys.argv[2]) if len(sys.argv) > 2 else pathlib.Path("modelos_gltf") / src.stem)
