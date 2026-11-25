"""Visor de los modelos 3D propios, sin entrar al juego.

    py -3 tools/visor_modelos/visor.py        (abre http://localhost:8765)

Muestra cada modelo del mod (modding/cartas/cartas.txt, campo "modelo") tal
como queda en su .igb (exportado con tools/igb_gltf.py), con su animacion de
reposo, al lado del modelo original que reemplaza y de otros para comparar el
tamano. Los controles (alto, suelo, derecha, cuerpo) se ven al instante en la
pagina; "Armar" rearma el .igb con tools/modelos/armar_igb.py (guarda el
anterior en <carpeta>/anteriores/), lo vuelve a exportar y actualiza la tabla
modding/modelos/ajustes_modelos.md. "Copiar al juego" corre cartas_mod.py y
tools/copiar_al_juego.sh (no copia si el juego esta abierto).

Ejes (los del juego y de glTF): Y arriba, el monstruo mira a +Z; "derecha"
del armado es +X.
"""
import http.server
import json
import pathlib
import re
import shutil
import subprocess
import sys
import threading
import time
import urllib.parse
import webbrowser

PORT_DIR = pathlib.Path(__file__).resolve().parents[2]
TOOLS = PORT_DIR / "tools"
GAME = PORT_DIR.parent / "Yu-Gi-Oh TDOD"
CARTAS = PORT_DIR / "modding" / "cartas" / "cartas.txt"
MODELOS = PORT_DIR / "modding" / "modelos"
TABLA = MODELOS / "ajustes_modelos.md"
MEDIDAS = MODELOS / "medidas_originales.txt"
CACHE = MODELOS / "_visor"
AQUI = pathlib.Path(__file__).resolve().parent
PUERTO = 8765

sys.path.insert(0, str(TOOLS / "modelos"))
sys.path.insert(0, str(TOOLS))
_lock = threading.Lock()


def leer_cartas():
    """[{id, nombre, carpeta, m, igb}] de las cartas con modelo propio"""
    out, cur = [], None
    for ln in CARTAS.read_text(encoding="utf-8").splitlines():
        t = ln.strip()
        m = re.match(r"^\[(\d+)\]$", t)
        if m:
            cur = {"id": int(m.group(1))}
            continue
        if cur is None or "=" not in t or t.startswith("#"):
            continue
        k, v = (s.strip() for s in t.split("=", 1))
        if k == "nombre":
            cur["nombre"] = v
        if k == "modelo":
            igb = (CARTAS.parent / v).resolve()
            cur.update(carpeta=igb.parent.name, m=igb.stem, igb=igb)
            out.append(cur)
    return out


def medidas():
    r = {}
    for ln in MEDIDAS.read_text().splitlines():
        t = ln.split()
        if len(t) > 2 and t[0] == "RES":
            r[t[1]] = {t[i]: float(t[i + 1]) for i in range(2, len(t) - 1, 2)}
    return r


def original_igb(m):
    n = int(m[1:])
    return GAME / "monster" / ("%03d" % (n // 100 * 100)) / (m + ".igb")


def ajuste_de(c):
    """los valores con que esta armado: ajuste.json, si no la tabla"""
    j = MODELOS / c["carpeta"] / "ajuste.json"
    if j.exists():
        return json.loads(j.read_text(encoding="utf-8"))
    a = {"alto": 22.8, "suelo": 0.0, "derecha": 0.0, "girar": 0.0, "cuerpo": False, "como": None}
    for ln in TABLA.read_text(encoding="utf-8").splitlines():
        if ln.startswith("|") and "| %s " % c["m"] in ln:
            f = re.search(r"`([^`]*)`", ln)
            if not f:
                f = re.search(r"\| (alto [\d,]+)", ln)
            s = f.group(1) if f else ""
            for k in ("alto", "suelo", "derecha", "girar"):
                v = re.search(r"--%s=([-\d.]+)" % k, s)
                if v:
                    a[k] = float(v.group(1))
            v = re.search(r"--como=(m\d+)", s)
            if v:
                a["como"] = v.group(1)
                a["cuerpo"] = True
            if "--cuerpo" in s:
                a["cuerpo"] = True
            v = re.match(r"alto ([\d,]+)", s)
            if v:
                a["alto"] = float(v.group(1).replace(",", "."))
    if a["como"]:
        a["alto"] = alto_como(c, a["como"])
    return a


def alto_como(c, como):
    """el alto que da --como=mNNN (la misma cuenta que armar_igb.py)"""
    import numpy as np
    o = medidas()[como]
    v = np.load(MODELOS / c["carpeta"] / "volcado.npz")
    p = v["pos"]
    h = float(p[:, 2].max() - p[:, 2].min())
    w = float(p[:, 1].max() - p[:, 1].min())
    return round(((o["alto"] - o["base"]) * o["ancho"] * h / w) ** 0.5, 2)


# La camara de la batalla de cada monstruo (sub_0005A810 / sub_000585E0 /
# sub_000583A0 del juego): tabla de 1000 entradas en default.xbe 0x42A718, una
# por numero de modelo, 19 doubles: 0, destino A, posicion A, arriba A,
# destino B, posicion B, arriba B. La camara va de A a B en 45 cuadros;
# perspectiva 45 grados en una textura de 480x480 (sub_00059AA0).
TABLA_CAMARAS = 0x42A718
XBE_DELTA = 0x10080           # direccion virtual - posicion en el archivo


def camara(m):
    import struct
    n = int(m[1:]) if isinstance(m, str) else int(m)
    if not 0 <= n < 1000:
        raise ValueError("modelo fuera de la tabla de camaras")
    x = (GAME / "default.xbe").read_bytes()
    d = struct.unpack_from("<19d", x, TABLA_CAMARAS - XBE_DELTA + 0x98 * n)
    return {"A": {"destino": d[1:4], "pos": d[4:7], "arriba": d[7:10]},
            "B": {"destino": d[10:13], "pos": d[13:16], "arriba": d[16:19]}}


def exportar(igb, nombre):
    """glTF de un .igb en la cache (rehecho si el .igb cambio)"""
    out = CACHE / nombre
    gl = out / (igb.stem + ".gltf")
    # rehecho tambien si el conversor cambio (2026-11-16: animacion de textura)
    if not gl.exists() or gl.stat().st_mtime < max(igb.stat().st_mtime, (TOOLS / "igb_gltf.py").stat().st_mtime):
        if out.exists():
            shutil.rmtree(out)
        r = subprocess.run([sys.executable, str(TOOLS / "igb_gltf.py"), str(igb), str(out)],
                           capture_output=True, text=True)
        if not gl.exists():
            raise RuntimeError("igb_gltf fallo: " + r.stdout + r.stderr)
    return "/cache/%s/%s.gltf?t=%d" % (nombre, igb.stem, int(gl.stat().st_mtime))


def lista():
    med = medidas()
    res = []
    for c in leer_cartas():
        a = ajuste_de(c)
        res.append({"id": c["id"], "nombre": c["nombre"], "carpeta": c["carpeta"], "m": c["m"],
                     "ajuste": a, "original": med.get(c["m"]),
                     "existe": c["igb"].exists()})
    return {"cartas": res, "medidas": med}


def carta(carpeta):
    for c in leer_cartas():
        if c["carpeta"] == carpeta:
            return c
    raise KeyError(carpeta)


def actualizar_tabla(c, a, flags):
    rango = "%s–%s" % (fmt(a["suelo"]), fmt(a["suelo"] + a["alto"]))
    s = TABLA.read_text(encoding="utf-8")
    lineas = s.split("\n")
    for i, ln in enumerate(lineas):
        if ln.startswith("|") and "| %s " % c["m"] in ln:
            col = ln.split("|")
            # | carta | modelo | original | armar | queda | usuario |
            col[4] = " `%s` " % flags
            col[5] = " %s " % rango
            if "visor" not in col[6]:
                col[6] = col[6].rstrip() + "; ajustado en el visor "
            lineas[i] = "|".join(col)
            break
    else:
        return
    TABLA.write_bytes("\n".join(lineas).encode("utf-8"))   # sin pasar a CRLF (write_text en Windows)


def fmt(x):
    return ("%.2f" % x).rstrip("0").rstrip(".").replace(".", ",")


def armar(carpeta, a):
    c = carta(carpeta)
    d = MODELOS / carpeta
    alto, suelo, der = float(a["alto"]), float(a["suelo"]), float(a["derecha"])
    gir = float(a.get("girar") or 0)
    if not (2 <= alto <= 60 and -10 <= suelo <= 40 and -20 <= der <= 20 and -180 <= gir <= 180):
        raise ValueError("valores fuera de rango")
    args = ["--alto=%g" % alto, "--suelo=%g" % suelo, "--derecha=%g" % der]
    if gir:
        args.append("--girar=%g" % gir)
    if a.get("cuerpo"):
        args.insert(0, "--cuerpo")
    with _lock:
        if c["igb"].exists():
            ant = d / "anteriores"
            ant.mkdir(exist_ok=True)
            shutil.copy2(c["igb"], ant / ("%s_%s.igb" % (c["m"], time.strftime("%Y%m%d_%H%M%S"))))
        r = subprocess.run([sys.executable, str(TOOLS / "modelos" / "armar_igb.py"), "volcado.npz",
                            "textura.png", c["igb"].name] + args, cwd=d, capture_output=True, text=True)
        if r.returncode:
            raise RuntimeError(r.stdout + r.stderr)
        aj = {"alto": alto, "suelo": suelo, "derecha": der, "girar": gir, "cuerpo": bool(a.get("cuerpo")), "como": None}
        (d / "ajuste.json").write_text(json.dumps(aj, indent=1), encoding="utf-8")
        actualizar_tabla(c, aj, " ".join(args))
    return {"log": r.stdout, "url": exportar(c["igb"], "propio_" + carpeta), "ajuste": aj}


def deshacer(carpeta):
    c = carta(carpeta)
    ant = sorted((MODELOS / carpeta / "anteriores").glob(c["m"] + "_*.igb"))
    if not ant:
        raise RuntimeError("no hay armados anteriores de este modelo (anteriores/)")
    with _lock:
        shutil.copy2(ant[-1], c["igb"])
        c["igb"].touch()
        ant[-1].unlink()
        j = MODELOS / carpeta / "ajuste.json"
        if j.exists():
            j.unlink()       # sin json vuelve a leer la tabla (que quedo con el armado deshecho)
    return {"url": exportar(c["igb"], "propio_" + carpeta),
            "aviso": "Se volvio al .igb anterior. Los valores de la tabla pueden no coincidir: revisalos."}


def copiar():
    out = []
    with _lock:
        r = subprocess.run([sys.executable, str(TOOLS / "cartas_mod.py")], cwd=PORT_DIR,
                           capture_output=True, text=True, encoding="utf-8", errors="replace")
        out.append(r.stdout[-1500:] + r.stderr[-1500:])
        if r.returncode:
            return {"ok": False, "log": "\n".join(out)}
        sh = shutil.which("sh") or r"C:\Program Files\Git\bin\sh.exe"
        r = subprocess.run([sh, "tools/copiar_al_juego.sh"], cwd=PORT_DIR,
                           capture_output=True, text=True, encoding="utf-8", errors="replace")
        out.append(r.stdout[-1500:] + r.stderr[-1500:])
    return {"ok": "LISTO" in r.stdout, "log": "\n".join(out)}


class H(http.server.SimpleHTTPRequestHandler):
    def log_message(self, *a):
        pass

    def enviar(self, code, obj):
        b = json.dumps(obj, ensure_ascii=False).encode("utf-8")
        self.send_response(code)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(b)))
        self.end_headers()
        self.wfile.write(b)

    def archivo(self, p):
        if not p.is_file():
            self.send_error(404)
            return
        tipo = {".html": "text/html; charset=utf-8", ".js": "text/javascript", ".gltf": "model/gltf+json",
                ".bin": "application/octet-stream", ".png": "image/png", ".webp": "image/webp"}.get(p.suffix, "application/octet-stream")
        b = p.read_bytes()
        self.send_response(200)
        self.send_header("Content-Type", tipo)
        self.send_header("Content-Length", str(len(b)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(b)

    def do_GET(self):
        u = urllib.parse.urlparse(self.path)
        q = urllib.parse.parse_qs(u.query)
        try:
            if u.path == "/":
                return self.archivo(AQUI / "index.html")
            if u.path == "/grabar":              # perfil: the mascots' animations (grabar.html)
                return self.archivo(AQUI / "grabar.html")
            if u.path == "/calibracion.webp":
                return self.archivo(AQUI / "calibracion.webp")
            if u.path == "/api/lista":
                return self.enviar(200, lista())
            if u.path == "/api/propio":
                c = carta(q["carpeta"][0])
                return self.enviar(200, {"url": exportar(c["igb"], "propio_" + c["carpeta"])})
            if u.path == "/api/camara":
                m = q["m"][0]
                if not re.fullmatch(r"m\d{3}", m):
                    raise ValueError("modelo invalido")
                return self.enviar(200, camara(m))
            if u.path == "/api/original":
                m = q["m"][0]
                if not re.fullmatch(r"m\d{3}", m):
                    raise ValueError("modelo invalido")
                igb = original_igb(m)
                if not igb.exists():
                    raise FileNotFoundError("no existe " + str(igb))
                return self.enviar(200, {"url": exportar(igb, "orig_" + m)})
            if u.path.startswith("/cache/"):
                p = (CACHE / urllib.parse.unquote(u.path[7:])).resolve()
                if CACHE.resolve() not in p.parents:
                    return self.send_error(403)
                return self.archivo(p)
            self.send_error(404)
        except Exception as e:  # noqa: BLE001
            self.enviar(500, {"error": str(e)})

    def do_POST(self):
        n = int(self.headers.get("Content-Length") or 0)
        body = json.loads(self.rfile.read(n) or b"{}")
        try:
            if self.path == "/api/armar":
                return self.enviar(200, armar(body["carpeta"], body["ajuste"]))
            if self.path == "/api/deshacer":
                return self.enviar(200, deshacer(body["carpeta"]))
            if self.path == "/api/captura":       # depuracion: el render del visor como PNG
                import base64
                (CACHE / "_captura.png").write_bytes(base64.b64decode(body["png"].split(",", 1)[1]))
                return self.enviar(200, {"ok": True})
            if self.path == "/api/cuadro":        # perfil: one frame of a mascot (grabar.html)
                import base64
                nom = body["nombre"]
                if not re.fullmatch(r"m\d{3}_\d{3}", nom):
                    raise ValueError("nombre invalido")
                d = MODELOS.parent / "texturas" / "perfil" / ("crudo_frente" if body.get("carpeta") == "frente" else "crudo")
                d.mkdir(parents=True, exist_ok=True)
                (d / (nom + ".png")).write_bytes(base64.b64decode(body["png"].split(",", 1)[1]))
                return self.enviar(200, {"ok": True})
            if self.path == "/api/info":          # perfil: frames and ms of a mascot (grabar.html)
                nom = body["nombre"]
                if not re.fullmatch(r"m\d{3}", nom):
                    raise ValueError("nombre invalido")
                d = MODELOS.parent / "texturas" / "perfil" / ("crudo_frente" if body.get("carpeta") == "frente" else "crudo")
                d.mkdir(parents=True, exist_ok=True)
                (d / (nom + ".json")).write_text(json.dumps({"frames": int(body["frames"]), "ms": float(body["ms"])}))
                return self.enviar(200, {"ok": True})
            if self.path == "/api/copiar":
                return self.enviar(200, copiar())
            self.send_error(404)
        except Exception as e:  # noqa: BLE001
            self.enviar(500, {"error": str(e)})


def main():
    CACHE.mkdir(parents=True, exist_ok=True)
    srv = http.server.ThreadingHTTPServer(("127.0.0.1", PUERTO), H)
    url = "http://localhost:%d/" % PUERTO
    print("Visor de modelos en", url, "(Ctrl+C para cerrar)")
    if "--sin-navegador" not in sys.argv:
        threading.Timer(0.8, lambda: webbrowser.open(url)).start()
    try:
        srv.serve_forever()
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
