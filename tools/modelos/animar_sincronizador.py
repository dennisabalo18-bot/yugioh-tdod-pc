"""Tres animaciones basicas para el Sincronizador riggeado (Blender, en fondo):
reposo (2 s en bucle, la que usa el juego), ataque y dano. Guarda el .blend
con las tres acciones y, si se pide, renderiza cuadros para la vista previa.

    blender -b riggeado.blend --python animar_sincronizador.py -- salida.blend [carpeta_cuadros]

Los giros se dan en ejes del mundo (el modelo mira a +X, izquierda +Y,
arriba +Z) y se pasan al espacio de cada hueso.
"""
import math
import os
import sys

import bpy
from mathutils import Matrix, Quaternion, Vector

args = sys.argv[sys.argv.index("--") + 1:]
out = args[0]
frames_dir = args[1] if len(args) > 1 else None

arm = bpy.data.objects["Esqueleto"]
s = bpy.context.scene
s.render.fps = 30
for pb in arm.pose.bones:
    pb.rotation_mode = "QUATERNION"

X, Y, Z = Vector((1, 0, 0)), Vector((0, 1, 0)), Vector((0, 0, 1))


def rot(bone, axis, deg):
    """cuaternion del hueso para girar 'deg' grados en el eje del mundo 'axis'"""
    b = arm.data.bones[bone].matrix_local.to_quaternion()
    w = Quaternion(axis, math.radians(deg))
    return b.inverted() @ w @ b


def key(action_bones, f):
    """action_bones: {hueso: [(eje, grados), ...]} en el cuadro f"""
    for pb in arm.pose.bones:
        q = Quaternion()
        for ax, d in action_bones.get(pb.name, []):
            q = rot(pb.name, ax, d) @ q
        pb.rotation_quaternion = q
        pb.keyframe_insert("rotation_quaternion", frame=f)
    loc = action_bones.get("_sube", 0.0)
    pb = arm.pose.bones["Spine1"]
    pb.location = (0, loc, 0)   # el eje Y del hueso Spine1 apunta hacia arriba
    pb.keyframe_insert("location", frame=f)


def accion(nombre, poses):
    a = bpy.data.actions.new(nombre)
    a.use_fake_user = True
    arm.animation_data_create()
    arm.animation_data.action = a
    for f, p in poses:
        key(p, f)
    return a


# reposo: respira, se balancea y mueve un poco los brazos (60 cuadros, bucle)
reposo = []
for f in range(0, 61, 5):
    t = f / 60 * 2 * math.pi
    sw = math.sin(t)
    reposo.append((f, {
        "_sube": 0.8 * (1 - math.cos(t)) / 2,
        "Spine2": [(Y, 3 * sw)],
        "Spine3": [(Y, 2 * sw), (Z, 3 * math.sin(t + 1))],
        "Lbicep": [(Y, -6 * sw)],
        "Lforearm": [(Y, -4 * sw)],
        "Rbicep": [(Y, 5 * sw)],
        "Lthigh": [(Y, 2 * sw)],
        "Rthigh": [(Y, 2 * sw)],
        "Lshin": [(Y, -2 * sw)],
        "Rshin": [(Y, -2 * sw)],
    }))

# ataque: toma impulso, pega con el brazo izquierdo (el de adelante) y vuelve
base = {}
atras = {"Spine2": [(Y, -10)], "Spine3": [(Y, -8), (Z, 15)], "Lbicep": [(Y, 25), (Z, 20)],
         "Lforearm": [(Y, 30)], "Rbicep": [(Y, -15)], "_sube": 0.5}
golpe = {"Spine2": [(Y, 18)], "Spine3": [(Y, 12), (Z, -20)], "Lbicep": [(Y, -45), (Z, -15)],
         "Lforearm": [(Y, -15)], "Rbicep": [(Y, 20)], "Lthigh": [(Y, -12)], "Rthigh": [(Y, 15)],
         "Lshin": [(Y, 10)], "_sube": -1.5}
ataque = [(0, base), (8, atras), (13, golpe), (18, golpe), (30, base)]

# dano: se echa para atras con los brazos arriba y se recupera
golpeado = {"Spine2": [(Y, -22)], "Spine3": [(Y, -15), (X, 8)], "Lbicep": [(Y, 35), (X, -25)],
            "Rbicep": [(Y, 35), (X, 25)], "Lforearm": [(Y, 20)], "Rforearm": [(Y, 20)],
            "Lthigh": [(Y, 10)], "Rthigh": [(Y, 10)], "_sube": 1.5}
dano = [(0, base), (4, golpeado), (9, golpeado), (24, base)]

acts = [("reposo", accion("reposo", reposo), 60),
        ("ataque", accion("ataque", ataque), 30),
        ("dano", accion("dano", dano), 24)]
arm.animation_data.action = acts[0][1]
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
    print("CAJA", tuple(lo), tuple(hi))
    cam.location = c + Vector((2.2, -1.5, 0.35)) * r
    cam.rotation_euler = (c - cam.location).to_track_quat("-Z", "Y").to_euler()
    cam.data.lens = 50
    s.render.engine = "BLENDER_WORKBENCH"
    s.display.shading.light = "STUDIO"
    s.display.shading.color_type = "TEXTURE"
    s.render.resolution_x, s.render.resolution_y = 360, 400
    if not s.world:
        s.world = bpy.data.worlds.new("w")
    for nombre, a, n in acts:
        arm.animation_data.action = a
        for f in range(0, n + 1, 2):
            s.frame_set(f)
            s.render.filepath = os.path.join(frames_dir, "%s_%03d.png" % (nombre, f))
            bpy.ops.render.render(write_still=True)
