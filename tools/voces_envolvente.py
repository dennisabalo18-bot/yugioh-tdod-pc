"""La envolvente de volumen de cada voz, para mover la boca del retrato.

Por cada N.mp3 de modding/voces_latino (y de x1.5/, x2/) escribe N.env al
lado: un byte por cada 20 ms (50 por segundo), el volumen (RMS) de ese tramo
llevado a 0..255 con 255 = el percentil 98 de esa misma voz (así una voz
baja también abre la boca). voces.c lo lee al empezar la voz y elige la
boca cerrada / media / abierta según el byte del momento.

    py -3 tools/voces_envolvente.py [carpeta]
"""
import pathlib
import subprocess
import sys

import numpy as np

PORT = pathlib.Path(__file__).resolve().parent.parent
VOCES = PORT / "modding" / "voces_latino"
RATE = 8000
STEP = RATE // 50   # 20 ms


def envelope(mp3):
    pcm = subprocess.run(
        ["ffmpeg", "-v", "error", "-i", str(mp3), "-ac", "1", "-ar", str(RATE), "-f", "s16le", "-"],
        check=True, capture_output=True).stdout
    a = np.frombuffer(pcm, np.int16).astype(np.float32)
    n = len(a) // STEP
    if n == 0:
        return b""
    rms = np.sqrt((a[: n * STEP].reshape(n, STEP) ** 2).mean(axis=1))
    top = np.percentile(rms, 98) or 1.0
    return np.clip(rms / top * 255.0, 0, 255).astype(np.uint8).tobytes()


def main():
    root = pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else VOCES
    n = 0
    for mp3 in sorted(root.rglob("*.mp3")):
        env = mp3.with_suffix(".env")
        if env.is_file() and env.stat().st_mtime >= mp3.stat().st_mtime:
            continue
        env.write_bytes(envelope(mp3))
        n += 1
        if n % 100 == 0:
            print(f"  {n}")
    print(f"{n} envolventes nuevas en {root}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
