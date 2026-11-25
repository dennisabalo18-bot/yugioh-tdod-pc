"""Vuelca un modelo riggeado de Blender para armar el .igb (sincronizador_igb.py).

    blender -b animado.blend --python volcar_blend.py -- salida.npz textura.png [accion] [cuadros]

- vertices por esquina de triangulo (sin indices, como el juego): posicion y
  normal en el espacio del esqueleto, UV, y los huesos/pesos del vertice;
- los huesos: nombre, padre, matriz de reposo (espacio del esqueleto);
- la accion (por defecto "reposo") muestreada cuadro por cuadro (0..cuadros):
  la matriz de cada hueso en el espacio del esqueleto;
- la textura de color de la malla, guardada como PNG.
"""
import sys

import bpy
import numpy as np

args = sys.argv[sys.argv.index("--") + 1:]
out, tex_out = args[0], args[1]
accion = args[2] if len(args) > 2 else "reposo"
cuadros = int(args[3]) if len(args) > 3 else 60

arm = next(o for o in bpy.data.objects if o.type == "ARMATURE")
mesh = next(o for o in bpy.data.objects if o.type == "MESH")
names = [b.name for b in arm.data.bones]
idx = {n: i for i, n in enumerate(names)}
parents = [arm.data.bones[n].parent.name if arm.data.bones[n].parent else "" for n in names]
rest = np.array([np.array(arm.data.bones[n].matrix_local) for n in names])

# la malla en reposo (sin el modificador del esqueleto), triangulada
for m in mesh.modifiers:
    m.show_viewport = False
dg = bpy.context.evaluated_depsgraph_get()
ev = mesh.evaluated_get(dg)
me = ev.to_mesh()
me.calc_loop_triangles()
uvl = me.uv_layers.active.data
mw = np.array(mesh.matrix_world)
gname = {g.index: g.name for g in mesh.vertex_groups}
P, N, UV, J, W = [], [], [], [], []
for tri in me.loop_triangles:
    for k, li in enumerate(tri.loops):
        v = me.vertices[tri.vertices[k]]
        p = mw @ np.array([*v.co, 1.0])
        n = mw[:3, :3] @ np.array(me.corner_normals[li].vector)
        P.append(p[:3])
        N.append(n / (np.linalg.norm(n) or 1))
        UV.append(uvl[li].uv[:])
        gs = sorted(((g.weight, idx[gname[g.group]]) for g in v.groups
                     if gname.get(g.group) in idx and g.weight > 0), reverse=True)[:4]
        gs += [(0.0, 0)] * (4 - len(gs))
        W.append([a for a, _ in gs])
        J.append([b for _, b in gs])
ev.to_mesh_clear()

# textura de color
img = None
for s in mesh.material_slots:
    if s.material and s.material.use_nodes:
        for nd in s.material.node_tree.nodes:
            if nd.type == "TEX_IMAGE" and nd.image and "Color" in nd.image.name or (
                    nd.type == "TEX_IMAGE" and nd.image and img is None and
                    any(l.links and l.links[0].to_socket.name == "Base Color" for l in nd.outputs)):
                img = nd.image
if img is None:
    img = next(i for i in bpy.data.images if i.size[0] > 0 and "Color" in i.name)
img.filepath_raw = tex_out
img.file_format = "PNG"
img.save()

# la accion, cuadro por cuadro
arm.animation_data.action = bpy.data.actions[accion]
s = bpy.context.scene
pose = []
for f in range(cuadros + 1):
    s.frame_set(f)
    pose.append([np.array(arm.matrix_world) @ np.array(arm.pose.bones[n].matrix) for n in names])

np.savez(out, names=np.array(names), parents=np.array(parents), rest=np.array(arm.matrix_world) @ rest,
         pos=np.array(P, np.float32), nrm=np.array(N, np.float32), uv=np.array(UV, np.float32),
         joints=np.array(J, np.int32), weights=np.array(W, np.float32), pose=np.array(pose),
         fps=s.render.fps)
print("VOLCADO", len(P), "esquinas,", len(names), "huesos,", cuadros + 1, "cuadros; textura", img.name, tuple(img.size))
