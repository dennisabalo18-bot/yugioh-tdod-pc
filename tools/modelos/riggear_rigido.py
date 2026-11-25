"""Un modelo sin huesos (.glb de Tripo) listo para armar_igb.py (Blender, en fondo):
los mismos 19 huesos que riggear_sincronizador.py (armar_igb.py arma sobre m373),
el modelo entero rigido y una animacion de reposo suave (respira: sube y baja
un poco y se balancea), 60 cuadros en bucle.

    blender -b --python riggear_rigido.py -- modelo.glb salida.blend [cuadros_png]

El modelo tiene que mirar hacia +X con Z arriba (los .glb de Tripo ya vienen asi).
Pesos: la mitad de abajo va a Lthigh y la de arriba a Spine3 (asi armar_igb.py
reparte los triangulos en sus dos mallas); los dos cuelgan de Spine1, que es el
unico que se mueve, asi que el modelo no se deforma.
"""
import math
import os
import sys

import bpy
from mathutils import Quaternion, Vector

args = sys.argv[sys.argv.index("--") + 1:]
src, out = args[0], args[1]
frames_dir = args[2] if len(args) > 2 else None

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=src)
s = bpy.context.scene
mesh = next(o for o in s.objects if o.type == "MESH")
mesh.name = "Modelo"
for o in list(s.objects):
    if o is not mesh:
        bpy.data.objects.remove(o)
mesh.parent = None
bpy.context.view_layer.objects.active = mesh
mesh.select_set(True)
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)

# a escala de trabajo (armar_igb.py la ajusta despues): alto 1, pies en 0, centrado
pts = [v.co.copy() for v in mesh.data.vertices]
lo = Vector([min(p[i] for p in pts) for i in range(3)])
hi = Vector([max(p[i] for p in pts) for i in range(3)])
h = hi.z - lo.z
for v in mesh.data.vertices:
    v.co = Vector(((v.co.x - (lo.x + hi.x) / 2) / h, (v.co.y - (lo.y + hi.y) / 2) / h, (v.co.z - lo.z) / h))

J = {
    "Spine1":   ((0, 0, 0.00), (0, 0, 0.30), None),
    "Spine2":   ((0, 0, 0.30), (0, 0, 0.55), "Spine1"),
    "Spine3":   ((0, 0, 0.55), (0, 0, 0.80), "Spine2"),
    "Lbicep":   ((0, 0.10, 0.80), (0, 0.20, 0.70), "Spine3"),
    "Lforearm": ((0, 0.20, 0.70), (0, 0.25, 0.60), "Lbicep"),
    "Lhand":    ((0, 0.25, 0.60), (0, 0.27, 0.55), "Lforearm"),
    "Rbicep":   ((0, -0.10, 0.80), (0, -0.20, 0.70), "Spine3"),
    "Rforearm": ((0, -0.20, 0.70), (0, -0.25, 0.60), "Rbicep"),
    "Rhand":    ((0, -0.25, 0.60), (0, -0.27, 0.55), "Rforearm"),
    "Rthigh":   ((0, -0.08, 0.30), (0, -0.08, 0.15), "Spine1"),
    "Rshin":    ((0, -0.08, 0.15), (0, -0.08, 0.04), "Rthigh"),
    "Rball":    ((0, -0.08, 0.04), (0.04, -0.08, 0.01), "Rshin"),
    "Rtoe":     ((0.04, -0.08, 0.01), (0.08, -0.08, 0.01), "Rball"),
    "Rheel":    ((0, -0.08, 0.04), (-0.04, -0.08, 0.01), "Rball"),
    "Lthigh":   ((0, 0.08, 0.30), (0, 0.08, 0.15), "Spine1"),
    "Lshin":    ((0, 0.08, 0.15), (0, 0.08, 0.04), "Lthigh"),
    "Lball":    ((0, 0.08, 0.04), (0.04, 0.08, 0.01), "Lshin"),
    "Ltoe":     ((0.04, 0.08, 0.01), (0.08, 0.08, 0.01), "Lball"),
    "Lheel":    ((0, 0.08, 0.04), (-0.04, 0.08, 0.01), "Lball"),
}
arm_data = bpy.data.armatures.new("Esqueleto")
arm = bpy.data.objects.new("Esqueleto", arm_data)
s.collection.objects.link(arm)
bpy.context.view_layer.objects.active = arm
bpy.ops.object.mode_set(mode="EDIT")
for n, (a, b, p) in J.items():
    e = arm_data.edit_bones.new(n)
    e.head, e.tail = Vector(a), Vector(b)
for n, (a, b, p) in J.items():
    if p:
        arm_data.edit_bones[n].parent = arm_data.edit_bones[p]
bpy.ops.object.mode_set(mode="OBJECT")

abajo = mesh.vertex_groups.new(name="Lthigh")
arriba = mesh.vertex_groups.new(name="Spine3")
for v in mesh.data.vertices:
    (abajo if v.co.z < 0.45 else arriba).add([v.index], 1.0, "REPLACE")
mesh.parent = arm
md = mesh.modifiers.new("Esqueleto", "ARMATURE")
md.object = arm

# reposo: Spine1 sube y baja un poco y se balancea, en bucle
s.render.fps = 30
for pb in arm.pose.bones:
    pb.rotation_mode = "QUATERNION"
act = bpy.data.actions.new("reposo")
act.use_fake_user = True
arm.animation_data_create()
arm.animation_data.action = act
pb = arm.pose.bones["Spine1"]
b = arm.data.bones["Spine1"].matrix_local.to_quaternion()
for f in range(0, 61, 5):
    t = f / 60 * 2 * math.pi
    pb.location = (0, 0.012 * (1 - math.cos(t)) / 2, 0)        # Y del hueso = arriba
    pb.rotation_quaternion = b.inverted() @ Quaternion((1, 0, 0), math.radians(1.5 * math.sin(t))) @ b
    pb.keyframe_insert("location", frame=f)
    pb.keyframe_insert("rotation_quaternion", frame=f)
s.frame_start, s.frame_end = 0, 60
bpy.ops.wm.save_as_mainfile(filepath=out)
print("GUARDADO", out)

if frames_dir:
    os.makedirs(frames_dir, exist_ok=True)
    cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
    s.collection.objects.link(cam)
    s.camera = cam
    cam.location = Vector((2.0, -1.2, 0.9))
    cam.rotation_euler = (Vector((0, 0, 0.45)) - cam.location).to_track_quat("-Z", "Y").to_euler()
    s.render.engine = "BLENDER_WORKBENCH"
    s.display.shading.light = "STUDIO"
    s.display.shading.color_type = "TEXTURE"
    s.render.resolution_x, s.render.resolution_y = 320, 320
    for f in (0, 30):
        s.frame_set(f)
        s.render.filepath = os.path.join(frames_dir, "reposo_%03d.png" % f)
        bpy.ops.render.render(write_still=True)
