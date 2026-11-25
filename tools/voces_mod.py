"""Arma el mod de voces: modding/voces_latino/ con
- N.mp3: los audios listos de modding/dialogos/audio (los de frases que
  cambiaron despues de generarse no se copian: no dirian lo que dice el globo),
  sin silencios de mas (ffmpeg silenceremove): nada al principio y las
  pausas de mas de PAUSA s quedan en PAUSA s (natural entre frases). Los de
  dialogos/audio no se tocan; uno ya recortado y mas nuevo que el suyo no se
  rehace;
- x1.5/N.mp3 y x2/N.mp3: el mismo audio mas rapido sin cambiar el tono
  (ffmpeg atempo): voces.c los usa cuando el juego va a 90 o 120 fps y en
  F2 "Velocidad de las voces" esta en "Sigue los FPS";
- textos.txt: "N<TAB>texto" en Latin-1, el texto de cada frase del guion sin
  las emociones [..] ni las marcas {Lumis}/{Umbra}/{Ambos}: lo que muestra el
  globo (ygo-apex/src/voces.c lo pone en lugar del texto de Espana).

El juego lo toma con YGO_VOCES (env.bat). Rearmarlo cuando generes o
cambies frases:
    py -3 tools/voces_mod.py      (o modding/meter_voces.bat)
"""
import json
import pathlib
import re
import shutil
import subprocess
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import doblaje_elevenlabs as E  # noqa: E402

MOD = E.D.ROOT / "modding" / "voces_latino"
PAUSA = 0.35    # s: la pausa mas larga que queda dentro de una frase
UMBRAL = "-45dB"
# ffmpeg 9: un silencio de mas de stop_duration queda en stop_silence (medido
# en 794.mp3: 0.73 s -> 0.37 s con 0.05/0.3). detection=peak como silencedetect.
FILTRO = (f"silenceremove=detection=peak:start_periods=1:start_threshold={UMBRAL}"
          f":start_silence=0.03:stop_periods=-1:stop_duration=0.05:stop_silence={PAUSA}"
          f":stop_threshold={UMBRAL}")


RAPIDOS = {"x1.5": 1.5, "x2": 2.0}

# 2026-11-12: las frases que dice solo el Robot (Duel Computer) suenan a
# maquina: voz plana sin entonacion (afftfilt: fase en 0), +8.5 dB para
# quedar pareja con las demas. Elegida por el usuario ("Robot clasico").
ROBOT = (r"afftfilt=real='hypot(re\,im)*sin(0)':imag='hypot(re\,im)*cos(0)'"
         ":win_size=512:overlap=0.75,volume=8.5dB,alimiter=limit=0.9")
ROBOT_HECHO = MOD / "robot.txt"    # las frases ya pasadas por ROBOT


def es_robot(ln):
    return [q for q, _ in E.partes(ln["texto"], ln["personaje"], ln["momento"])] == ["Robot"]


def acelerar(ff, src, dst, f):
    """src a f veces su velocidad, con el mismo tono, en dst."""
    if dst.is_file() and dst.stat().st_mtime >= src.stat().st_mtime:
        return True
    tmp = dst.with_suffix(".tmp")
    r = subprocess.run([ff, "-v", "error", "-y", "-i", str(src), "-af", f"atempo={f}",
                        "-c:a", "libmp3lame", "-b:a", "128k", "-f", "mp3", str(tmp)],
                       capture_output=True)
    if r.returncode or not tmp.is_file() or tmp.stat().st_size < 500:
        tmp.unlink(missing_ok=True)
        return False
    tmp.replace(dst)
    return True


def recortar(ff, src, dst, robot=False, rehacer=False):
    """src sin silencios de mas en dst (con la voz de maquina si robot);
    False (y una copia tal cual) si ffmpeg falla."""
    if not rehacer and dst.is_file() and dst.stat().st_mtime >= src.stat().st_mtime:
        return True
    tmp = dst.with_suffix(".tmp")
    r = subprocess.run([ff, "-v", "error", "-y", "-i", str(src), "-af",
                        FILTRO + ("," + ROBOT if robot else ""),
                        "-c:a", "libmp3lame", "-b:a", "128k", "-f", "mp3", str(tmp)],
                       capture_output=True)
    if r.returncode or not tmp.is_file() or tmp.stat().st_size < 1000:
        tmp.unlink(missing_ok=True)
        shutil.copy2(src, dst)
        return False
    tmp.replace(dst)
    return True


def globo(texto):
    t = re.sub(r"\[[^\]]*\]|\{[^}]*\}", "", texto)
    t = " ".join(t.split())
    t = re.sub(r"\s+([,.!?;:])", r"\1", t)          # "hola ," -> "hola,"
    return t


def main():
    g = json.loads(E.GUION.read_text(encoding="utf-8"))
    MOD.mkdir(parents=True, exist_ok=True)
    ff = E.ffmpeg()
    listos = {n for n in g["lineas"] if E.estado(g, n) == "listo"}
    for f in MOD.glob("*.mp3"):
        # los que ya no estan listos, y los recortados antes de que se
        # regenerara su frase (se rehacen abajo)
        if f.stem not in listos:
            f.unlink()
    for carpeta in RAPIDOS:
        (MOD / carpeta).mkdir(exist_ok=True)
        for f in (MOD / carpeta).glob("*.mp3"):
            if f.stem not in listos:
                f.unlink()
    hechos = set(ROBOT_HECHO.read_text().split()) if ROBOT_HECHO.is_file() else set()
    robots = set()
    lineas, voces, sin, raros, fallas = [], 0, [], [], []
    for n in sorted(g["lineas"], key=int):
        ln = g["lineas"][n]
        t = globo(ln["texto"]) or ln["original"]
        try:
            t.encode("latin-1")
        except UnicodeEncodeError:
            raros.append(n)
            t = t.encode("latin-1", "replace").decode("latin-1")
        lineas.append(f"{n}\t{t}")
        if n in listos:
            src, dst = E.AUDIO / f"{n}.mp3", MOD / f"{n}.mp3"
            if not ff:
                shutil.copy2(src, dst)
            else:
                robot = es_robot(ln)
                if robot:
                    robots.add(n)
                if not recortar(ff, src, dst, robot, robot != (n in hechos)):
                    fallas.append(n)
            if ff:
                for carpeta, f in RAPIDOS.items():
                    if not acelerar(ff, dst, MOD / carpeta / f"{n}.mp3", f):
                        fallas.append(f"{carpeta}/{n}")
            voces += 1
        elif E.tiene_voz(ln["texto"]):
            sin.append(n)
    if ff:
        ROBOT_HECHO.write_text("".join(f"{n}\n" for n in sorted(robots, key=int)))
    (MOD / "textos.txt").write_bytes(("\n".join(lineas) + "\n").encode("latin-1"))
    print(f"{len(lineas)} textos y {voces} voces en {MOD}")
    if sin:
        print(f"  sin voz todavia ({len(sin)}): {', '.join(sin)}")
    if not ff:
        print("  sin ffmpeg: los audios van tal cual (con sus silencios)")
    if fallas:
        print(f"  no se pudieron recortar (van tal cual): {', '.join(fallas)}")
    if raros:
        print(f"  letras que el juego no tiene (cambiadas por ?): {', '.join(raros)}")


if __name__ == "__main__":
    main()
