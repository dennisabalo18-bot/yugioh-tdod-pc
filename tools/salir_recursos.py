"""The "Salir a Windows" medallion (2026-11-20, the user: "agregar un Botón
para salir a Windows. abajo del ultimo Boton que Combine con la estetica del
juego"): the wheel's medallion as the Perfil button's (perfil_recursos.py),
with a teal door and an arrow leaving it instead of the bust.

Out: modding/texturas/perfil/juego/salir.png, salir_luz.png, salir_sol.png
(the same sizes and layout as boton*.png), and a sample over the user's
capture: modding/muestras/salir_muestra.png.

  py -3 tools/salir_recursos.py [captura]
"""
import pathlib
import sys

from PIL import Image, ImageDraw

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import perfil_recursos as pr  # noqa: E402


def medallon_salir():
    m = pr.src.crop((890, 108, 1030, 215)).convert("RGB")
    m = m.resize((m.width * 4, m.height * 4), Image.LANCZOS)
    pr.borrar(m, 47 * 4, 26 * 4, 95 * 4, 76 * 4)
    m = m.filter(pr.ImageFilter.GaussianBlur(0.6))
    d = ImageDraw.Draw(m)
    cx, cy = 70 * 4, 52 * 4
    for dx, dy, col in ((5, 5, (178, 150, 40)), (0, 0, (10, 74, 68))):
        # the door: a frame open on the right, its leaf ajar
        x0, y0, x1, y1 = cx - 78 + dx, cy - 88 + dy, cx + 6 + dx, cy + 92 + dy
        d.rectangle((x0, y0, x1, y0 + 14), fill=col)
        d.rectangle((x0, y0, x0 + 14, y1), fill=col)
        d.rectangle((x0, y1 - 14, x1, y1), fill=col)
        d.rectangle((x1 - 14, y0, x1, y0 + 40), fill=col)
        d.rectangle((x1 - 14, y1 - 40, x1, y1), fill=col)
        d.polygon([(x0 + 14, y0 + 14), (x0 + 58, y0 + 34), (x0 + 58, y1 - 2), (x0 + 14, y1 - 14)], fill=col)
        # the arrow leaving through it
        ay = cy + 2 + dy
        d.rectangle((cx - 28 + dx, ay - 13, cx + 52 + dx, ay + 13), fill=col)
        d.polygon([(cx + 40 + dx, ay - 44), (cx + 96 + dx, ay), (cx + 40 + dx, ay + 44)], fill=col)
    m = m.resize((m.width // 4, m.height // 4), Image.LANCZOS)
    mk = Image.new("L", m.size, 0)
    ImageDraw.Draw(mk).ellipse((6, 5, m.width - 6, m.height - 4), fill=255)
    r = m.convert("RGBA")
    r.putalpha(mk.filter(pr.ImageFilter.GaussianBlur(1.5)))
    return r


def boton(luz, giro=0.0):
    m = medallon_salir()
    m = m.resize((int(m.width * 1.3), int(m.height * 1.3)), Image.LANCZOS)
    g = Image.new("RGBA", (m.width + 60, m.height + 60), (0, 0, 0, 0))
    g.alpha_composite(m, (30, 30))
    return pr.sol(pr.dorar(g), giro=giro) if luz else g


def main():
    pr.OUT.mkdir(parents=True, exist_ok=True)
    b, l = boton(False), boton(True)
    b.save(pr.OUT / "salir.png")
    l.save(pr.OUT / "salir_luz.png")
    tira = Image.new("RGBA", (b.width, b.height * pr.SOL_CUADROS), (0, 0, 0, 0))
    for i in range(pr.SOL_CUADROS):
        tira.alpha_composite(boton(True, i / pr.SOL_CUADROS), (0, i * b.height))
    tira.save(pr.OUT / "salir_sol.png")
    if len(sys.argv) > 1:
        cap = Image.open(sys.argv[1]).convert("RGBA")
        k = cap.height / 1080.0
        out = Image.new("RGBA", (cap.width * 2, cap.height), (0, 0, 0, 255))
        for j, img in enumerate((b, l)):
            c = cap.copy()
            s = img.resize((int(img.width * k * 0.78), int(img.height * k * 0.78)), Image.LANCZOS)
            c.alpha_composite(s, (int(1458 * k - s.width / 2), int(645 * k - s.height / 2)))
            out.paste(c, (j * cap.width, 0))
        dest = pr.R / "modding" / "muestras" / "salir_muestra.png"
        out.convert("RGB").save(dest)
        print("muestra", dest)
    print("ok", b.size)


if __name__ == "__main__":
    main()
