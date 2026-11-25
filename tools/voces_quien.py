"""Who talks when, in the voices of a two-voice character (2026-11-20,
Lumis & Umbra: the user: "separamos los dialogos por personaje ... y hicimos
las voces por separado (menos las que estan juntas)").

tools/doblaje_elevenlabs.py made each line of a duo from parts ({Lumis},
{Umbra}, {Ambos}: both at once), one TTS each, joined one after the other
(ffmpeg concat) with no record of where each part starts. The parts meet in
a pause (each TTS has its own silence at both ends), so the cuts are found in
N.env (tools/voces_envolvente.py, 50 a second): of the quiet runs, the K-1
(K parts) that best fit both "long pause" and "where the text length puts
the cut". Writes N.quien beside N.env (and in x1.5/, x2/, the same cuts
scaled to their length): one byte per 20 ms, 1 = the first voice (Lumis),
2 = the second (Umbra), 3 = both. voces.c moves only that one's mouth.

    py -3 tools/voces_quien.py [carpeta_voces]     # default modding/voces_latino
"""
import itertools
import json
import pathlib
import re
import sys

PORT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(PORT / "tools"))
from doblaje_elevenlabs import DUOS, AMBOS, partes  # noqa: E402

GUION = PORT / "modding" / "dialogos" / "guion.json"
QUIETO = 30          # env below this: a pause
MIN_PAUSA = 6        # frames (120 ms)


def largo(t):
    return max(1, len(re.sub(r"\[[^\]]*\]", "", t).strip()))


def pausas(env):
    out, i = [], 0
    while i < len(env):
        if env[i] < QUIETO:
            j = i
            while j < len(env) and env[j] < QUIETO:
                j += 1
            if j - i >= MIN_PAUSA and i > 0 and j < len(env):
                out.append((i, j))
            i = j
        else:
            i += 1
    return out


def cortes(env, ps):
    """the K-1 cut frames for the parts ps [(who, text)]"""
    k = len(ps)
    if k == 1:
        return []
    # the voice itself: from its first loud frame to its last
    sonido = [i for i, v in enumerate(env) if v >= QUIETO]
    a, b = (sonido[0], sonido[-1] + 1) if sonido else (0, len(env))
    tot = sum(largo(t) for _, t in ps)
    acum, esperado = 0, []
    for _, t in ps[:-1]:
        acum += largo(t)
        esperado.append(a + (b - a) * acum / tot)
    cand = pausas(env)
    if len(cand) < k - 1:
        return [int(e) for e in esperado]
    mx = max(j - i for i, j in cand)
    best, bs = None, -1e9
    for comb in itertools.combinations(cand, k - 1):
        s = 0.0
        for (i, j), e in zip(comb, esperado):
            s += (j - i) / mx - 3.0 * abs((i + j) / 2 - e) / (b - a)
        if s > bs:
            bs, best = s, comb
    return [(i + j) // 2 for i, j in best]


def main():
    voces = pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else PORT / "modding" / "voces_latino"
    g = json.loads(GUION.read_text(encoding="utf-8"))
    hechas = 0
    for n, ln in sorted(g["lineas"].items(), key=lambda kv: int(kv[0])):
        p = ln["personaje"]
        duo = DUOS.get(p)
        if not duo or p != "Lumis & Umbra":          # the Duel Computer is one face
            continue
        env_f = voces / f"{n}.env"
        if not env_f.is_file():
            continue
        env = env_f.read_bytes()
        ps = partes(ln.get("generado", [ln["texto"]])[0], p, ln["momento"])
        if not ps:
            continue
        cs = cortes(env, ps)
        codigo = [3 if q == AMBOS else duo.index(q) + 1 for q, _ in ps]
        bordes = [0] + cs + [len(env)]
        for sub in ("", "x1.5", "x2"):
            e2 = voces / sub / f"{n}.env"
            if not e2.is_file():
                continue
            m = len(e2.read_bytes())
            f = m / len(env)
            out = bytearray(m)
            for c, a, b in zip(codigo, bordes, bordes[1:]):
                for i in range(int(a * f), min(m, int(b * f) + 1)):
                    out[i] = c
            (voces / sub / f"{n}.quien").write_bytes(bytes(out))
        hechas += 1
        print(n, " | ".join(f"{q}@{bordes[i] / 50:.2f}s" for i, (q, _) in enumerate(ps)), f"(fin {len(env) / 50:.2f}s)")
    print(hechas, "frases con N.quien en", voces)


if __name__ == "__main__":
    main()
