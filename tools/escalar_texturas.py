"""Escala con Upscayl (x4) las texturas de modding/texturas/dump/ que aun no
estan en modding/texturas/mejoradas/.

Adaptado de rdr-port/tools/escalar_texturas.py. Una imagen por proceso, en
mosaicos chicos: si la tarjeta se reinicia solo se pierde esa imagen, que se
reintenta con mosaicos mas chicos. Se puede cortar y volver a ejecutar: sigue
donde quedo.

Quedan afuera: las de 32 px o menos (no ganan nada) y las de
modding/texturas/no_escalar.txt (un hash por linea; las paginas de letras:
la IA inventa trazos en el texto).

    py -3 tools/escalar_texturas.py [modelo]      (digital-art-4x por defecto)
"""
import pathlib
import re
import subprocess
import sys

UPSCAYL = pathlib.Path(r"C:\Program Files\Upscayl\resources")
ROOT = pathlib.Path(__file__).resolve().parent.parent / "modding" / "texturas"
NAME = re.compile(r"^([0-9A-Fa-f]{16})_(\d+)x(\d+)")
SMALL = 32


def no_escalar():
    f = ROOT / "no_escalar.txt"
    if not f.is_file():
        return set()
    return {l.split()[0].upper() for l in f.read_text(encoding="utf-8").splitlines()
            if l.strip() and not l.startswith("#")}


def main():
    modelo = sys.argv[1] if len(sys.argv) > 1 else "digital-art-4x"
    exe = UPSCAYL / "bin" / "upscayl-bin.exe"
    if not exe.is_file():
        print(f"No encuentro Upscayl en {exe}")
        return 1
    if not (ROOT / "dump").is_dir():
        print(f"No hay volcado en {ROOT / 'dump'}: F2 > Volcar texturas, y recorrer las pantallas")
        return 1
    salida = ROOT / "mejoradas" / f"upscayl_png_{modelo}_4x"
    salida.mkdir(parents=True, exist_ok=True)
    hechas = {p.name[:16].upper() for p in (ROOT / "mejoradas").rglob("*.png")}
    fuera = no_escalar()

    def sirve(p):
        m = NAME.match(p.stem)
        return (m and p.name[:16].upper() not in fuera
                and max(int(m.group(2)), int(m.group(3))) > SMALL)

    todas = sorted((ROOT / "dump").glob("*.png"))
    faltan = [p for p in todas if p.name[:16].upper() not in hechas and sirve(p)]
    print(f"{len(todas)} volcadas, {len(hechas)} ya escaladas, faltan {len(faltan)} (modelo {modelo})")
    fallidas = []
    for n, p in enumerate(faltan, 1):
        dest = salida / p.name
        ok = False
        for tile in ("128", "64"):
            try:
                r = subprocess.run([str(exe), "-i", str(p), "-o", str(dest), "-s", "4",
                                    "-m", str(UPSCAYL / "models"), "-n", modelo, "-t", tile, "-f", "png"],
                                   capture_output=True, text=True, errors="replace", timeout=300)
                ok = r.returncode == 0 and dest.is_file() and "failed" not in r.stdout + r.stderr
            except subprocess.TimeoutExpired:
                ok = False
            if ok:
                break
            dest.unlink(missing_ok=True)
        if not ok:
            fallidas.append(p.name)
        print(f"\r  {n}/{len(faltan)}" + (f"  ({len(fallidas)} fallaron)" if fallidas else ""), end="", flush=True)
    print()
    for f in fallidas:
        print(f"  no se pudo: {f} (queda la original)")
    print("Listo. Ahora ejecuta crear_pack_texturas.bat")
    return 0


if __name__ == "__main__":
    sys.exit(main())
