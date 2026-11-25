"""Los dialogos de duelo de cada personaje, sacados de default.xbe.

El ejecutable guarda 3680 textos por idioma: una tabla (cantidad + offset de
cada texto) seguida de los textos en latin-1 terminados en 0; mismo orden en
los 6 idiomas. Los textos 86-113 son los 28 personajes y los 317-848 sus
dialogos: 19 por personaje, en el mismo orden que los nombres.

Salida: modding/dialogos/dialogos_por_personaje.csv (se abre con Excel) y
un .txt por personaje.

    py -3 tools/dialogos.py
"""
import csv
import pathlib
import struct

ROOT = pathlib.Path(__file__).resolve().parent.parent
XBE = ROOT.parent / "Yu-Gi-Oh TDOD" / "default.xbe"
OUT = ROOT / "modding" / "dialogos"
TABLES = {"es": 0x4A1FB6, "en": 0x58777C}   # offsets en el archivo
NAMES, FIRST, PER, CHARS = 86, 317, 19, 28
# lo que es cada una de las 19 frases, deducido del texto (Joey, Kaiba, Mai)
SLOTS = ["Empieza el duelo (1)", "Empieza el duelo (2)", "Empieza el duelo (3)",
         "Hace una fusion", "Activa una carta magica", "Activa una trampa",
         "Recibe dano (poco)", "Recibe dano (mucho)", "Recibe dano (se enoja)",
         "El rival hace una fusion", "El rival activa una magica",
         "El rival activa una trampa",
         "Gana el duelo (1)", "Gana el duelo (2)", "Gana el duelo (3)", "Empate",
         "Pierde el duelo (1)", "Pierde el duelo (2)", "Pierde el duelo (3)"]


def texts(d, t):
    n = struct.unpack_from("<I", d, t)[0]
    offs = struct.unpack_from(f"<{n}I", d, t + 4)
    base = t + 4 + 4 * n
    return [d[base + o:d.index(b"\0", base + o)].decode("latin-1").replace("\r", "")
            for o in offs]


def main():
    d = XBE.read_bytes()
    es, en = texts(d, TABLES["es"]), texts(d, TABLES["en"])
    OUT.mkdir(parents=True, exist_ok=True)
    rows = []
    for c in range(CHARS):
        name = es[NAMES + c]
        lines = []
        for s in range(PER):
            i = FIRST + c * PER + s
            rows.append([name, i, SLOTS[s], es[i], en[i]])
            lines.append(f"[{i}] {SLOTS[s]}\n  ES: {es[i]}\n  EN: {en[i]}\n")
        safe = "".join(ch if ch.isalnum() or ch in " &-" else "_" for ch in name).strip()
        (OUT / f"{c + 1:02d} {safe}.txt").write_text(
            f"{name}\n{'=' * len(name)}\n\n" + "\n".join(lines), encoding="utf-8")
    with open(OUT / "dialogos_por_personaje.csv", "w", newline="", encoding="utf-8-sig") as f:
        w = csv.writer(f, delimiter=";")
        w.writerow(["Personaje", "Numero de texto", "Momento", "Espanol (juego)", "Ingles (juego)"])
        w.writerows(rows)
    print(f"{len(rows)} dialogos de {CHARS} personajes en {OUT}")


if __name__ == "__main__":
    main()
