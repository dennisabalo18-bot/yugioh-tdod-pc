"""Exodia entero quieto, para la mascota del duelo.

El Exodia con brazos y piernas del juego (effect/special_igb/se002.igb, el
modelo 737 de la animacion de la victoria) trae UNA animacion de 19 s: parado,
gestos, carga y el golpe. Como mascota se veia el bucle entero (2026-11-15, el
usuario: "se nota" el loop; "dejarlo quieto asi como en la imagen y que se
mueva poco como cuando respira").

Esto escribe se002_mascota.igb: la misma malla y los mismos huesos, y cada
pista (igTransformSequence1_5: campo 12 tiempos en ns, 3 traslacion, 4
rotacion x,y,z,w, 5 escala) muestreada en su propia animacion alrededor de
POSE con un vaiven suave de +-AMPLITUD s, PERIODOS veces en el bucle (el
principio y el final quedan iguales: no hay salto). Mismos tiempos y mismas
cantidades de claves: solo cambian los valores. El original queda intacto
(lo usa la animacion de la victoria).

POSE = 7.6 s: la de la captura del usuario (brazos abiertos, agachado);
hoja de cuadros del 2026-11-15 (igb_gltf.py + three.js).

Y se002_ataque.igb (el usuario: "tiene animacion de ataque"; "lo quiero
solo para cuando ganas con exodia"): el golpe del final de su animacion (GOLPE: de 16.3 s, el brazo
atras, a 17.4 s, el puno adelante) repetido GOLPES veces en el bucle, cada
uno saliendo de la pose quieta y volviendo a ella (mezcla de las dos poses):
la mascota lo carga al ganar con Exodia y, se vea desde el momento que se vea del
bucle, hay un golpe entero en ~1.9 s.

    py -3 tools/modelos/exodia_mascota.py [carpeta del juego]
Escribe en <juego>/effect/special_igb/ y en modding/modelos/exodia_mascota/.
"""
import pathlib
import shutil
import sys

import numpy as np

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))
import igb  # noqa: E402

POSE = 7.6          # s
AMPLITUD = 0.10     # s a cada lado
PERIODOS = 4        # vaivenes en el bucle (~4.75 s cada uno; 7 se veia apurado: el usuario,
                    # "un poco mas lento ... mira la de jinzo o buster blader")
GOLPE = (16.3, 17.4)  # s de su animacion: preparacion -> puno adelante
GOLPES = 10         # golpes en el bucle (~1.9 s cada uno)

PORT = pathlib.Path(__file__).resolve().parents[2]
JUEGO = pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else PORT.parent / "Yu-Gi-Oh TDOD"
SRC = JUEGO / "effect" / "special_igb" / "se002.igb"
SALIDAS = PORT / "modding" / "modelos" / "exodia_mascota"


def muestrear(t, v, s, cuat):
    """el valor de la pista (tiempos t, valores v) en el tiempo s, lineal
    (cuaterniones: del mismo lado y normalizados)"""
    if s <= t[0]:
        return v[0]
    if s >= t[-1]:
        return v[-1]
    k = int(np.searchsorted(t, s, side="right")) - 1
    a, b = v[k], v[k + 1].copy()
    f = (s - t[k]) / (t[k + 1] - t[k]) if t[k + 1] > t[k] else 0.0
    if cuat and float(np.dot(a, b)) < 0.0:
        b = -b
    r = a + (b - a) * f
    if cuat:
        r = r / max(float(np.linalg.norm(r)), 1e-9)
    return r


def mezclar(a, b, w, cuat):
    if cuat and float(np.dot(a, b)) < 0.0:
        b = -b
    r = a + (b - a) * w
    if cuat:
        r = r / max(float(np.linalg.norm(r)), 1e-9)
    return r


def suave(x):
    x = min(max(x, 0.0), 1.0)
    return x * x * (3.0 - 2.0 * x)


def quieto(t, v, x, dur, cuat):
    return muestrear(t, v, POSE + AMPLITUD * np.sin(2 * np.pi * PERIODOS * x / dur), cuat)


def ataque(t, v, x, dur, cuat):
    """un golpe por ciclo: 0-0.25 de la pose quieta a la preparacion, 0.25-0.55
    el golpe (su animacion), 0.55-0.75 el puno adelante, 0.75-1 de vuelta"""
    f = (x / dur * GOLPES) % 1.0
    reposo = muestrear(t, v, POSE, cuat)
    a, b = GOLPE
    if f < 0.25:
        return mezclar(reposo, muestrear(t, v, a, cuat), suave(f / 0.25), cuat)
    if f < 0.55:
        return muestrear(t, v, a + (b - a) * suave((f - 0.25) / 0.30), cuat)
    if f < 0.75:
        return muestrear(t, v, b, cuat)
    return mezclar(muestrear(t, v, b, cuat), reposo, suave((f - 0.75) / 0.25), cuat)


def armar(modo, archivo):
    e = igb.IGBEdit(str(SRC))
    g = e.g

    def lista(ref, dt, k):
        o = g.obj(ref) if ref >= 0 else None
        if not o or not o[1][3]:
            return None, None
        return np.frombuffer(g.mem_bytes(o[1][5]), dt)[:o[1][3] * k].reshape(-1, k).astype(np.float64), o[1][5]

    pistas = 0
    for nombre, f in g.objs:
        if nombre != "igTransformSequence1_5":
            continue
        t, _ = lista(f[12], np.int64, 1)
        if t is None:
            continue
        t = t[:, 0] / 1e9
        dur = t[-1]
        for campo, k in ((3, 3), (4, 4), (5, 3)):
            v, mref = lista(f[campo], np.float32, k)
            if v is None or len(v) != len(t):
                continue
            nuevo = np.array([modo(t, v, x, dur, k == 4) for x in t], np.float32)
            viejo = e.mem(mref)
            datos = nuevo.tobytes()
            e.set_mem(mref, datos + viejo[len(datos):])
            pistas += 1
    SALIDAS.mkdir(parents=True, exist_ok=True)
    salida = SALIDAS / archivo
    e.save(str(salida))
    destino = SRC.with_name(archivo)
    shutil.copyfile(salida, destino)
    print(f"{archivo}: {pistas} pistas -> {salida} y {destino}")


def main():
    armar(quieto, "se002_mascota.igb")
    armar(ataque, "se002_ataque.igb")


if __name__ == "__main__":
    main()
