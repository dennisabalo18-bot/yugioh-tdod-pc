"""Huesos para las Cerezas Naturia del usuario (Blender, en fondo).

    blender -b original.blend --python riggear_cerezas.py -- salida.blend

Mismos 19 huesos (nombres y padres) que riggear_sincronizador.py, porque
armar_igb.py arma sobre m373. Las cerezas no tienen brazos ni piernas: se usan
tres huesos:
- Lbicep (en el grupo de malla de arriba): la cereza de la izquierda del
  modelo (+Y, la de ojos abiertos), colgada del cruce de los tallos;
- Rthigh (en el grupo de abajo): la de la derecha (-Y, la dormida), igual;
- Spine2: los tallos de arriba y las hojas.
Los demas quedan sin peso. Coordenadas del modelo (sin escalar): Z arriba,
mira hacia +X, izquierda +Y; mide 0,82 de alto y 0,98 de ancho.
"""
import sys

import bpy
import numpy as np
from mathutils import Vector

out = sys.argv[sys.argv.index("--") + 1]

for o in list(bpy.data.objects):
    if o.type in ("CAMERA", "LIGHT"):
        bpy.data.objects.remove(o)
mesh = next(o for o in bpy.data.objects if o.type == "MESH")
mesh.name = "Cerezas"
bpy.context.view_layer.objects.active = mesh
mesh.select_set(True)
S = 70.0
mesh.scale = (S, S, S)
mesh.location = mesh.location * S
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)

PIVOTE = (0.0, 0.0, 0.62)       # el cruce de los tallos: de ahi cuelgan las dos
J = {
    "Spine1":   ((0.00, 0.00, 0.00), (0.00, 0.00, 0.30), None),
    "Spine2":   ((0.00, 0.00, 0.30), (0.00, 0.00, 0.62), "Spine1"),
    "Spine3":   ((0.00, 0.00, 0.62), (0.00, 0.00, 0.80), "Spine2"),
    "Lbicep":   (PIVOTE, (0.00, 0.24, 0.22), "Spine3"),
    "Lforearm": ((0.00, 0.24, 0.22), (0.00, 0.28, 0.12), "Lbicep"),
    "Lhand":    ((0.00, 0.28, 0.12), (0.00, 0.30, 0.06), "Lforearm"),
    "Rbicep":   ((0.00, 0.00, 0.80), (0.00, -0.10, 0.75), "Spine3"),
    "Rforearm": ((0.00, -0.10, 0.75), (0.00, -0.15, 0.72), "Rbicep"),
    "Rhand":    ((0.00, -0.15, 0.72), (0.00, -0.18, 0.70), "Rforearm"),
    "Rthigh":   (PIVOTE, (0.00, -0.24, 0.22), "Spine1"),
    "Rshin":    ((0.00, -0.24, 0.22), (0.00, -0.28, 0.12), "Rthigh"),
    "Rball":    ((0.00, -0.28, 0.12), (0.05, -0.29, 0.05), "Rshin"),
    "Rtoe":     ((0.05, -0.29, 0.05), (0.10, -0.29, 0.04), "Rball"),
    "Rheel":    ((0.00, -0.28, 0.12), (-0.08, -0.29, 0.04), "Rball"),
    "Lthigh":   ((0.00, 0.10, 0.02), (0.00, 0.12, 0.01), "Spine1"),
    "Lshin":    ((0.00, 0.12, 0.01), (0.00, 0.14, 0.01), "Lthigh"),
    "Lball":    ((0.00, 0.14, 0.01), (0.02, 0.15, 0.01), "Lshin"),
    "Ltoe":     ((0.02, 0.15, 0.01), (0.04, 0.15, 0.01), "Lball"),
    "Lheel":    ((0.00, 0.14, 0.01), (-0.02, 0.15, 0.01), "Lball"),
}
arm_data = bpy.data.armatures.new("Esqueleto")
arm = bpy.data.objects.new("Esqueleto", arm_data)
bpy.context.scene.collection.objects.link(arm)
bpy.context.view_layer.objects.active = arm
bpy.ops.object.mode_set(mode="EDIT")
eb = arm_data.edit_bones
for name in ["Spine1", "Spine2", "Spine3", "Rbicep", "Rforearm", "Rhand", "Lbicep", "Lforearm",
             "Lhand", "Lthigh", "Lshin", "Lball", "Ltoe", "Lheel", "Rthigh", "Rshin", "Rball",
             "Rtoe", "Rheel"]:
    h, t, p = J[name]
    b = eb.new(name)
    b.head, b.tail = Vector(h) * S, Vector(t) * S
    if p:
        b.parent = eb[p]
bpy.ops.object.mode_set(mode="OBJECT")

# pesos: cada cereza (con su tallo hasta 0,55) a su hueso; entre 0,55 y 0,68
# pasa a Spine2 (tallos de arriba y hojas).
# De que cereza es cada vertice: las dos son una sola malla soldada donde se
# besan, y la cara de una pasa al otro lado de y=0 (un corte por y la partia).
# Se camina por la superficie (Dijkstra por las aristas, con los vertices de
# igual posicion unidos: la malla de Tripo esta cortada por la textura) desde
# el punto mas a la izquierda y el mas a la derecha: cada vertice es de la que
# llega primero, y el corte cae donde se tocan. Sin mezcla: con mezcla se
# estiraba un puente entre las dos al hamacarse (visto en el juego).
import heapq
V = np.array([v.co[:] for v in mesh.data.vertices]) / S
y, z = V[:, 1], V[:, 2]
vec = [[] for _ in V]
for e in mesh.data.edges:
    a, b = e.vertices
    d = float(np.linalg.norm(V[a] - V[b]))
    vec[a].append((b, d))
    vec[b].append((a, d))
pos = {}
for i, p in enumerate(np.round(V / 0.002).astype(int)):
    k = tuple(p)
    if k in pos:
        vec[i].append((pos[k], 0.0))
        vec[pos[k]].append((i, 0.0))
    else:
        pos[k] = i
dist = np.full(len(V), np.inf)
lado = np.where(y >= 0, 0, 1)
cola = []
for s_, sel in enumerate((y > 0.2, y < -0.2)):     # la mitad de afuera de cada una
    for sd in np.nonzero(sel & (z < 0.45))[0]:
        dist[sd] = 0.0
        lado[sd] = s_
        heapq.heappush(cola, (0.0, int(sd), s_))
while cola:
    d, i, s_ = heapq.heappop(cola)
    if d > dist[i]:
        continue
    for j, w in vec[i]:
        if d + w < dist[j]:
            dist[j] = d + w
            lado[j] = s_
            heapq.heappush(cola, (d + w, j, s_))
wl = (lado == 0).astype(float)                      # izquierda (+Y)
wc = np.clip((0.68 - z) / 0.13, 0, 1)              # 1 = cereza, 0 = hojas
pesos = {"Lbicep": wc * wl, "Rthigh": wc * (1 - wl), "Spine2": 1 - wc}
# separadas (pedido del usuario: en el modelo se tocan en el medio): cada
# cereza SEPARA hacia afuera; los tallos de arriba y las hojas quedan
SEPARA = 0.03
for i, v in enumerate(mesh.data.vertices):
    v.co.y += SEPARA * S * wc[i] * (1.0 if wl[i] >= 0.5 else -1.0)
# los triangulos con vertices de las dos cerezas (la union donde se besan) se
# estirarian entre las dos: fuera (quedan escondidos entre ellas)
import bmesh
bm = bmesh.new()
bm.from_mesh(mesh.data)
bm.verts.ensure_lookup_table()
mixtos = [f for f in bm.faces
          if len({int(lado[v.index]) for v in f.verts if wc[v.index] > 0.01}) > 1]
bmesh.ops.delete(bm, geom=mixtos, context="FACES_ONLY")
bm.to_mesh(mesh.data)
bm.free()
print("UNION", len(mixtos), "triangulos sacados")
for n in J:
    mesh.vertex_groups.new(name=n)
for n, w in pesos.items():
    g = mesh.vertex_groups[n]
    for i in np.nonzero(w > 0.01)[0]:
        g.add([int(i)], float(w[i]), "REPLACE")
mesh.parent = arm
mod = mesh.modifiers.new("Esqueleto", "ARMATURE")
mod.object = arm
print("PESOS", {n: int((w > 0.01).sum()) for n, w in pesos.items()}, "de", len(V))
bpy.ops.wm.save_as_mainfile(filepath=out)
