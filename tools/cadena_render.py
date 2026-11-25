"""Renders the chain's links (our own picture, 2026-11-16; the user: "se ve
horrible la cadena, no podemos conseguir una cadena asi") by sphere tracing
signed distances: each link is an elongated torus (Inigo Quilez's sdLink),
the links alternate flat / edge-on along the x axis. Shiny steel: a key light
from the top left, an environment that is bright above and dark below,
a tight specular and an occlusion term from the march.

Writes one period (two links) that tiles along x, RGBA premultiplied-ready
(straight alpha), and a preview over gray:

    py -3 tools/cadena_render.py [out.png] [preview.png]
"""
import sys
import pathlib

import numpy as np
from PIL import Image

LE, R1, R2 = 0.62, 0.78, 0.26           # half straight part, ring radius, wire radius
PASO = 2.0 * (LE + R1 - R2) - 0.06      # link to link (each through the other's hole)
W, H = 512, 160                         # one period: 2 links
ALTO = 2.4                              # world units across the picture's height


def sd_link(px, py, pz):
    """Elongated torus: long along x, ring in the x-y plane."""
    qx = np.maximum(np.abs(px) - LE, 0.0)
    return np.sqrt((np.sqrt(qx * qx + py * py) - R1) ** 2 + pz * pz) - R2


def escena(px, py, pz):
    d = np.full(px.shape, 1e9)
    for i in range(-2, 5):
        cx = i * PASO
        x = px - cx
        # each link turned about the chain's axis, the next a right angle more
        # (as a real chain hangs: both show as rings, through each other)
        a = np.radians(40.0 if i % 2 == 0 else -50.0)
        ca, sa = np.cos(a), np.sin(a)
        dd = sd_link(x, ca * py + sa * pz, -sa * py + ca * pz)
        d = np.minimum(d, dd)
    return d


def normal(px, py, pz):
    e = 1e-3
    nx = escena(px + e, py, pz) - escena(px - e, py, pz)
    ny = escena(px, py + e, pz) - escena(px, py - e, pz)
    nz = escena(px, py, pz + e) - escena(px, py, pz - e)
    n = np.stack([nx, ny, nz], -1)
    return n / np.maximum(np.linalg.norm(n, axis=-1, keepdims=True), 1e-9)


def render():
    ss = 3                              # supersampling
    w, h = W * ss, H * ss
    ancho = ALTO * W / H
    xs = (np.arange(w) + 0.5) / w * (2 * PASO) + 0.0       # exactly one period
    ys = ((np.arange(h) + 0.5) / h - 0.5) * ALTO
    X, Y = np.meshgrid(xs, -ys)
    _ = ancho
    oz = np.full(X.shape, 5.0)          # orthographic camera looking down -z
    t = np.zeros(X.shape)
    hit = np.zeros(X.shape, bool)
    pasos = np.zeros(X.shape)
    for k in range(90):
        d = escena(X, Y, oz - t)
        nuevo = (d < 1e-3) & ~hit
        hit |= nuevo
        pasos[~hit] = k
        t = np.where(hit, t, t + d * 0.9)
        if (t > 12).all():
            break
    hit &= t < 12
    pz = oz - t
    n = normal(X, Y, pz)
    v = np.array([0.0, 0.0, 1.0])
    luz = np.array([-0.45, 0.65, 0.62]); luz /= np.linalg.norm(luz)
    ndl = np.clip((n * luz).sum(-1), 0, 1)
    r = 2 * (n * v).sum(-1)[..., None] * n - v           # reflection
    env = np.clip(0.5 + 0.5 * r[..., 1], 0, 1) ** 1.6    # sky above, dark floor
    hv = luz + v; hv /= np.linalg.norm(hv)
    spec = np.clip((n * hv).sum(-1), 0, 1) ** 60
    rim = (1 - np.clip((n * v).sum(-1), 0, 1)) ** 3
    ao = np.clip(1.0 - pasos / 90.0 * 0.0, 0, 1)
    # occlusion: how close other links are along the normal
    o = np.zeros(X.shape)
    for k, s in enumerate((0.06, 0.14, 0.26)):
        dd = escena(X + n[..., 0] * s, Y + n[..., 1] * s, pz + n[..., 2] * s)
        o += (s - np.clip(dd, 0, s)) / s / (k + 1)
    ao = np.clip(1.0 - 0.55 * o, 0.25, 1.0)
    acero = np.array([0.80, 0.82, 0.86])
    col = (0.10 + 0.55 * ndl[..., None] + 0.55 * env[..., None]) * acero * ao[..., None]
    col += spec[..., None] * 1.1 + rim[..., None] * 0.18 * acero
    col = np.clip(col, 0, 1)
    rgba = np.zeros((h, w, 4))
    rgba[..., :3] = col
    rgba[..., 3] = hit
    # supersample down
    rgba = rgba.reshape(H, ss, W, ss, 4)
    a = rgba[..., 3].mean((1, 3))
    c = (rgba[..., :3] * rgba[..., 3:4]).sum((1, 3)) / np.maximum(rgba[..., 3].sum((1, 3))[..., None], 1e-9)
    out = np.dstack([c, a])
    return Image.fromarray((out * 255 + 0.5).astype(np.uint8), "RGBA")


if __name__ == "__main__":
    port = pathlib.Path(__file__).resolve().parent.parent
    dst = pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else port / "modding" / "texturas" / "cadena_eslabones.png"
    im = render()
    im.save(dst)
    if len(sys.argv) > 2:
        prev = Image.new("RGBA", (W * 3, H + 40), (70, 60, 50, 255))
        for i in range(3):
            prev.alpha_composite(im, (i * W, 20))
        prev.save(sys.argv[2])
    # the kit's copy: half size, RGBA, into ygo-apex/src/cadena_eslabones.h
    chico = im.resize((W // 2, H // 2), Image.LANCZOS)
    px = np.asarray(chico, np.uint8).reshape(-1)
    h = port / "ygo-apex" / "src" / "cadena_eslabones.h"
    with open(h, "w", newline="\n") as f:
        f.write("/* Generado por tools/cadena_render.py: dos eslabones de cadena de acero que\n"
                " * se repiten a lo largo (RGBA). No editar a mano. */\n")
        f.write("#define ESLABONES_W %d\n#define ESLABONES_H %d\n" % chico.size)
        f.write("static const unsigned char k_eslabones_px[%d] = {\n" % px.size)
        for i in range(0, px.size, 32):
            f.write(",".join(str(int(v)) for v in px[i:i + 32]) + ",\n")
        f.write("};\n")
    print(dst, im.size, h)
