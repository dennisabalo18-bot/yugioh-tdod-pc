"""Le pone huesos al Sincronizador de Basura del usuario (Blender, en fondo).

    blender -b original.blend --python riggear_sincronizador.py -- salida.blend

Esqueleto: los 19 huesos de m373 (Tutor de recaudacion), con sus nombres y su
jerarquia (Spine1-3, brazos, piernas con ball/toe/heel), puestos sobre el
modelo del usuario, escalado x70. Coordenadas del modelo (sin escalar): Z arriba, mira hacia +X, su
izquierda es +Y; mide 0,95.
"""
import sys

import bpy
from mathutils import Vector

out = sys.argv[sys.argv.index("--") + 1]

for o in list(bpy.data.objects):
    if o.type in ("CAMERA", "LIGHT"):
        bpy.data.objects.remove(o)
mesh = next(o for o in bpy.data.objects if o.type == "MESH")
mesh.name = "Sincronizador"
bpy.context.view_layer.objects.active = mesh
mesh.select_set(True)
# al tamano de los monstruos del juego (m373 mide ~80): los pesos automaticos
# de Blender fallan con modelos de menos de 1 unidad
S = 70.0
mesh.scale = (S, S, S)
mesh.location = mesh.location * S
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)

# articulaciones (cabeza del hueso, cola del hueso, padre)
J = {
    "Spine1":   ((0.00, 0.00, 0.31), (0.00, 0.00, 0.44), None),
    "Spine2":   ((0.00, 0.00, 0.44), (0.00, 0.00, 0.56), "Spine1"),
    "Spine3":   ((0.00, 0.00, 0.56), (0.00, 0.00, 0.90), "Spine2"),
    "Rbicep":   ((0.00, -0.22, 0.58), (-0.12, -0.33, 0.43), "Spine3"),
    "Rforearm": ((-0.12, -0.33, 0.43), (-0.29, -0.36, 0.33), "Rbicep"),
    "Rhand":    ((-0.29, -0.36, 0.33), (-0.40, -0.37, 0.27), "Rforearm"),
    "Lbicep":   ((0.02, 0.22, 0.58), (0.15, 0.36, 0.50), "Spine3"),
    "Lforearm": ((0.15, 0.36, 0.50), (0.29, 0.38, 0.47), "Lbicep"),
    "Lhand":    ((0.29, 0.38, 0.47), (0.43, 0.40, 0.45), "Lforearm"),
    "Lthigh":   ((0.00, 0.11, 0.30), (0.03, 0.22, 0.20), "Spine1"),
    "Lshin":    ((0.03, 0.22, 0.20), (-0.07, 0.28, 0.11), "Lthigh"),
    "Lball":    ((-0.07, 0.28, 0.11), (0.06, 0.30, 0.05), "Lshin"),
    "Ltoe":     ((0.06, 0.30, 0.05), (0.14, 0.31, 0.04), "Lball"),
    "Lheel":    ((-0.07, 0.28, 0.11), (-0.20, 0.28, 0.04), "Lball"),
}
for k in list(J):
    if k[0] == "L" and k not in ("Lheel",) or k == "Lheel":
        h, t, p = J[k]
        J["R" + k[1:]] = J.get("R" + k[1:]) or ((h[0], -h[1], h[2]), (t[0], -t[1], t[2]),
                                               p if p in (None, "Spine1", "Spine3") else "R" + p[1:])

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
        b.use_connect = (Vector(h) * S - eb[p].tail).length < 1e-3 and name not in ("Lheel", "Rheel")
bpy.ops.object.mode_set(mode="OBJECT")

# pesos por distancia: los automaticos de Blender (calor) fallan con esta
# malla de IA (piezas sueltas, no cerrada), tambien en una copia remallada.
# Cada vertice: peso 1/d^4 a cada hueso (d = distancia al segmento del hueso),
# los 4 mayores, normalizados (el juego usa hasta 4).
import numpy as np
V = np.array([v.co[:] for v in mesh.data.vertices])
names = [b.name for b in arm_data.bones]
D = []
for b in arm_data.bones:
    h, t = np.array(b.head_local[:]), np.array(b.tail_local[:])
    ab = t - h
    u = np.clip(((V - h) @ ab) / max(ab @ ab, 1e-9), 0, 1)
    D.append(np.linalg.norm(V - (h + u[:, None] * ab), axis=1))
D = np.maximum(np.array(D).T, 0.5)            # vertices x huesos
W = 1.0 / D ** 4
top = np.argsort(-W, axis=1)[:, :4]
Wt = np.take_along_axis(W, top, 1)
Wt /= Wt.sum(axis=1, keepdims=True)
for n in names:
    mesh.vertex_groups.new(name=n)
for i in range(len(V)):
    for j in range(4):
        if Wt[i, j] > 0.01:
            mesh.vertex_groups[names[top[i, j]]].add([i], float(Wt[i, j]), "REPLACE")
mesh.parent = arm
mod = mesh.modifiers.new("Esqueleto", "ARMATURE")
mod.object = arm
vg = {g.name for g in mesh.vertex_groups}
sin = sum(1 for v in mesh.data.vertices if not v.groups)
print("PESOS", len(vg), "grupos;", sin, "vertices sin hueso de", len(mesh.data.vertices))
bpy.ops.wm.save_as_mainfile(filepath=out)
