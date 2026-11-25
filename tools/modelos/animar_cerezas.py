"""Animacion de reposo de las Cerezas Naturia riggeadas (Blender, en fondo):
flotan arriba y abajo y cada cereza se hamaca de su tallo, a destiempo
(60 cuadros en bucle, la unica que usa el juego).

    blender -b riggeado.blend --python animar_cerezas.py -- salida.blend [carpeta_cuadros]
"""
import math
import os
import sys

import bpy
from mathutils import Quaternion, Vector

args = sys.argv[sys.argv.index("--") + 1:]
out = args[0]
frames_dir = args[1] if len(args) > 1 else None

arm = bpy.data.objects["Esqueleto"]
s = bpy.context.scene
s.render.fps = 30
for pb in arm.pose.bones:
    pb.rotation_mode = "QUATERNION"
X, Z = Vector((1, 0, 0)), Vector((0, 0, 1))


def rot(bone, axis, deg):
    b = arm.data.bones[bone].matrix_local.to_quaternion()
    return b.inverted() @ Quaternion(axis, math.radians(deg)) @ b


a = bpy.data.actions.new("reposo")
a.use_fake_user = True
arm.animation_data_create()
arm.animation_data.action = a
for f in range(0, 61, 5):
    t = f / 60 * 2 * math.pi
    giros = {"Lbicep": [(X, 7 * math.sin(t)), (Z, 4 * math.sin(t + 0.5))],
             "Rthigh": [(X, 7 * math.sin(t + 1.6)), (Z, -4 * math.sin(t + 2.1))],
             "Spine2": [(X, 2 * math.sin(t + 0.8))]}
    for pb in arm.pose.bones:
        q = Quaternion()
        for ax, d in giros.get(pb.name, []):
            q = rot(pb.name, ax, d) @ q
        pb.rotation_quaternion = q
        pb.keyframe_insert("rotation_quaternion", frame=f)
    pb = arm.pose.bones["Spine1"]
    pb.location = (0, 1.5 * (1 - math.cos(t)) / 2, 0)   # Y del hueso = arriba
    pb.keyframe_insert("location", frame=f)
s.frame_start, s.frame_end = 0, 60
bpy.ops.wm.save_as_mainfile(filepath=out)
print("GUARDADO", out)

if frames_dir:
    os.makedirs(frames_dir, exist_ok=True)
    mesh = next(o for o in s.objects if o.type == "MESH")
    cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
    s.collection.objects.link(cam)
    s.camera = cam
    s.frame_set(0)
    e = mesh.evaluated_get(bpy.context.evaluated_depsgraph_get())
    pts = [e.matrix_world @ v.co for v in e.to_mesh().vertices]
    lo = Vector([min(p[i] for p in pts) for i in range(3)])
    hi = Vector([max(p[i] for p in pts) for i in range(3)])
    c = (lo + hi) / 2
    r = (hi - lo).length / 2
    cam.location = c + Vector((2.2, -1.2, 0.5)) * r
    cam.rotation_euler = (c - cam.location).to_track_quat("-Z", "Y").to_euler()
    s.render.engine = "BLENDER_WORKBENCH"
    s.display.shading.light = "STUDIO"
    s.display.shading.color_type = "TEXTURE"
    s.render.resolution_x, s.render.resolution_y = 360, 360
    for f in range(0, 61, 3):
        s.frame_set(f)
        s.render.filepath = os.path.join(frames_dir, "reposo_%03d.png" % f)
        bpy.ops.render.render(write_still=True)
