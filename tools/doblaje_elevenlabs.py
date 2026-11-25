"""Doblaje de los dialogos de duelo con ElevenLabs.

Una ventana para:
- conectarse con tu clave de ElevenLabs (se guarda solo en esta PC, en
  modding/dialogos/.clave_elevenlabs);
- elegir para cada personaje una de TUS voces (las que creaste o agregaste
  a tu cuenta) y el modelo (eleven_v3 / eleven_v4 leen las emociones);
- editar cada frase con emociones entre corchetes, por ejemplo:
      [determinado] Espero que vengas preparado... [desafiante] porque...
      [gritando con emocion] ¡ES HORA DEL DUELO!
- generar una frase, un personaje o todo lo que falta, y escucharlo.

El guion (textos editados, voz de cada personaje) queda en
modding/dialogos/guion.json; los audios en modding/dialogos/audio/N.mp3,
N = numero de texto del juego (el que el port va a reproducir).
Si cambias el texto de una frase ya generada, queda marcada "cambio" y
"Generar lo que falta" la vuelve a hacer.

    py -3 tools/doblaje_elevenlabs.py            (la ventana)
    py -3 tools/doblaje_elevenlabs.py --revisar  (arma el guion y muestra un
                                                  pedido de ejemplo, sin
                                                  conectarse)
"""
import glob
import json
import os
import pathlib
import re
import shutil
import subprocess
import sys
import tempfile
import threading
import urllib.error
import urllib.request

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import dialogos as D  # noqa: E402

OUT = D.OUT
GUION = OUT / "guion.json"
AUDIO = OUT / "audio"
CLAVE = OUT / ".clave_elevenlabs"
API = "https://api.elevenlabs.io/v1"
FORMATO = "mp3_44100_128"     # el que dan todos los planes

# emocion inicial de cada momento (se puede cambiar frase por frase)
EMOCION = {
    "Empieza el duelo": "[determinado]",
    "Hace una fusion": "[gritando con emoción]",
    "Activa una carta magica": "[seguro de sí mismo]",
    "Activa una trampa": "[astuto]",
    "Recibe dano (poco)": "[molesto]",
    "Recibe dano (mucho)": "[dolido]",
    "Recibe dano (se enoja)": "[enojado]",
    "El rival hace una fusion": "[sorprendido]",
    "El rival activa una magica": "[sorprendido]",
    "El rival activa una trampa": "[alarmado]",
    "Gana el duelo": "[triunfante]",
    "Empate": "[frustrado]",
    "Pierde el duelo": "[frustrado]",
}
# Personajes de dos voces: cada frase se escribe por partes,
#   {Lumis} [burlón] ¿Para qué pelear? {Umbra} ¡Ya sabemos quién gana! {Ambos} ¡Nosotros!
# cada parte sale con la voz de quien la dice ({Ambos}: las dos a la vez) y
# se juntan en un solo mp3 con ffmpeg. Sin marca, la frase es del primero.
DUOS = {"Lumis & Umbra": ("Lumis", "Umbra"),
        # La Duel Computer: en el duelo habla la maquina; la bienvenida y la
        # despedida (gana, pierde, empate) las dice Kaiba: la voz de Seto.
        "Duel Computer": ("Robot", "Seto")}
AMBOS = "Ambos"
# Quien dice una frase sin marca, segun el momento (si no: el primero del duo).
KAIBA = ("Empieza", "Gana", "Pierde", "Empate")
QUIEN_POR_MOMENTO = {"Duel Computer": lambda m: "Seto" if m.startswith(KAIBA) else "Robot"}
ESTABILIDAD = {"Creativa (más emoción)": 0.0, "Natural": 0.5, "Robusta (más pareja)": 1.0}


def texto_inicial(original, momento):
    """El texto del juego con una emocion al principio; las notas del
    juego como "(REACCION: panico)" pasan a ser emociones: "[panico]"."""
    t = re.sub(r"\(\s*REACCI[ÓO]N\s*:?\s*([^)]*)\)", lambda m: f"[{m.group(1).strip().lower()}]",
               original)
    t = re.sub(r"\(([^)]*)\)", lambda m: f"[{m.group(1).strip().lower()}]", t)
    t = re.sub(r"\]\s*[.,:;]+", "]", t)            # "[panico]. Esto..." -> "[panico] Esto..."
    t = " ".join(t.split())
    if not re.search(r"\w", re.sub(r"\[[^\]]*\]", "", t)):
        return t                       # "..." (Strings): sin voz
    base = re.sub(r" \(\d\)$", "", momento)
    if t.startswith("["):
        return t
    return f"{EMOCION.get(base, '')} {t}".strip()


def tiene_voz(t):
    return bool(re.search(r"\w", re.sub(r"\[[^\]]*\]|\{[^}]*\}", "", t)))


def partes(texto, personaje, momento=""):
    """[(quien, texto)]: quien es una voz del duo o AMBOS; un personaje
    normal es una sola parte."""
    duo = DUOS.get(personaje)
    if not duo:
        return [(personaje, texto)]
    nombres = "|".join(map(re.escape, duo + (AMBOS,)))
    trozos = re.split(r"\{\s*(" + nombres + r")\s*\}", texto, flags=re.I)
    quien = QUIEN_POR_MOMENTO.get(personaje, lambda m: duo[0])(momento)
    out = []
    for k, t in enumerate(trozos):
        if k % 2:
            quien = next(n for n in duo + (AMBOS,) if n.lower() == t.lower())
        elif tiene_voz(t):
            out.append((quien, " ".join(t.split())))
    return out


def voz_de(g, ln):
    """La voz (o las que usa la frase de un duo, unidas con +) que lleva una
    frase; None si falta elegir alguna."""
    p = ln["personaje"]
    duo = DUOS.get(p)
    if not duo:
        return g["voces"].get(p)
    if p not in QUIEN_POR_MOMENTO:     # Lumis & Umbra: siempre las dos (como antes)
        vs = [g["voces"].get(n) for n in duo]
        return "+".join(vs) if all(vs) else None
    usa = []
    for quien, _ in partes(ln["texto"], p, ln["momento"]):
        for q in (duo if quien == AMBOS else (quien,)):
            if q not in usa:
                usa.append(q)
    vs = [g["voces"].get(n) for n in sorted(usa, key=duo.index)]
    return "+".join(vs) if vs and all(vs) else None


def ffmpeg():
    f = shutil.which("ffmpeg")
    if f:
        return f
    w = glob.glob(os.path.expandvars(
        r"%LOCALAPPDATA%\Microsoft\WinGet\Packages\*FFmpeg*\*\bin\ffmpeg.exe"))
    return w[0] if w else None


def cargar_guion():
    d = D.XBE.read_bytes()
    es, en = D.texts(d, D.TABLES["es"]), D.texts(d, D.TABLES["en"])
    g = json.loads(GUION.read_text(encoding="utf-8")) if GUION.is_file() else {}
    g.setdefault("modelo", "eleven_v3")
    g.setdefault("estabilidad", "Natural")
    g.setdefault("voces", {})
    lineas = g.setdefault("lineas", {})
    for c in range(D.CHARS):
        name = es[D.NAMES + c]
        for s in range(D.PER):
            i = D.FIRST + c * D.PER + s
            ln = lineas.setdefault(str(i), {})
            ln.update(personaje=name, momento=D.SLOTS[s], original=es[i], ingles=en[i])
            ln.setdefault("texto", texto_inicial(es[i], D.SLOTS[s]))
    g["personajes"] = [es[D.NAMES + c] for c in range(D.CHARS)]
    return g


def guardar_guion(g):
    tmp = GUION.with_suffix(".tmp")
    tmp.write_text(json.dumps(g, ensure_ascii=False, indent=1), encoding="utf-8")
    tmp.replace(GUION)


def estado(g, n):
    ln = g["lineas"][n]
    if not tiene_voz(ln["texto"]):
        return "sin voz"
    if not (AUDIO / f"{n}.mp3").is_file():
        return "falta"
    if ln.get("generado") != [ln["texto"], voz_de(g, ln)]:
        return "cambió"
    return "listo"


# ── ElevenLabs ────────────────────────────────────────────────────────────

def pedir(clave, metodo, ruta, cuerpo=None, consulta=""):
    req = urllib.request.Request(
        f"{API}{ruta}{consulta}", method=metodo,
        data=json.dumps(cuerpo).encode() if cuerpo is not None else None,
        headers={"xi-api-key": clave, "Content-Type": "application/json",
                 "Accept": "application/json" if metodo == "GET" else "audio/mpeg"})
    try:
        with urllib.request.urlopen(req, timeout=120) as r:
            return r.read()
    except urllib.error.HTTPError as e:
        cuerpo = e.read().decode("utf-8", "replace")
        try:
            det = json.loads(cuerpo).get("detail")
            cuerpo = det.get("message", det) if isinstance(det, dict) else det
        except (ValueError, AttributeError):
            pass
        raise RuntimeError(f"ElevenLabs respondió {e.code}: {cuerpo}") from None
    except urllib.error.URLError as e:
        raise RuntimeError(f"No se pudo conectar con ElevenLabs: {e.reason}") from None


def voces(clave):
    v = json.loads(pedir(clave, "GET", "/voices"))["voices"]
    # las tuyas primero (clonadas / creadas / agregadas), despues las de fabrica
    v.sort(key=lambda x: (x.get("category") == "premade", x["name"].lower()))
    return [(x["name"], x["voice_id"], x.get("category", "")) for x in v]


def modelos(clave):
    m = json.loads(pedir(clave, "GET", "/models"))
    return [x["model_id"] for x in m if x.get("can_do_text_to_speech", True)]


def cuerpo_pedido(g, texto):
    return {"text": texto, "model_id": g["modelo"], "language_code": "es",
            "voice_settings": {"stability": ESTABILIDAD.get(g["estabilidad"], 0.5),
                               "similarity_boost": 0.75}}


def tts(clave, g, voz, texto):
    return pedir(clave, "POST", f"/text-to-speech/{voz}", cuerpo_pedido(g, texto),
                 f"?output_format={FORMATO}")


def juntar(trozos, destino):
    """trozos: [[mp3, ...], ...]. Un trozo con mas de un mp3 se mezcla (las
    voces a la vez); los trozos van uno detras de otro."""
    ff = ffmpeg()
    if not ff:
        raise RuntimeError("Para Lumis & Umbra hace falta ffmpeg (winget install ffmpeg)")
    with tempfile.TemporaryDirectory() as tmp:
        args, filtros, k = [], [], 0
        for i, voces_ in enumerate(trozos):
            ins = []
            for audio in voces_:
                f = pathlib.Path(tmp, f"{k}.mp3")
                f.write_bytes(audio)
                args += ["-i", str(f)]
                ins.append(f"[{k}:a]")
                k += 1
            if len(ins) > 1:
                filtros.append("".join(ins)
                               + f"amix=inputs={len(ins)}:duration=longest:normalize=0[t{i}]")
            else:
                filtros.append(f"{ins[0]}anull[t{i}]")
        filtros.append("".join(f"[t{i}]" for i in range(len(trozos)))
                       + f"concat=n={len(trozos)}:v=0:a=1[out]")
        r = subprocess.run([ff, "-y", "-loglevel", "error", *args,
                            "-filter_complex", ";".join(filtros), "-map", "[out]",
                            "-ar", "44100", "-b:a", "128k", "-f", "mp3", str(destino)],
                           capture_output=True, text=True,
                           creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
        if r.returncode:
            raise RuntimeError(f"ffmpeg no pudo juntar las voces: {r.stderr.strip()[:300]}")


def generar(clave, g, n):
    ln = g["lineas"][n]
    p = ln["personaje"]
    voz = voz_de(g, ln)
    if not voz:
        raise RuntimeError(f"{p} no tiene voz elegida")
    AUDIO.mkdir(parents=True, exist_ok=True)
    tmp = AUDIO / f"{n}.tmp"
    duo = DUOS.get(p)
    if not duo:
        tmp.write_bytes(tts(clave, g, voz, ln["texto"]))
    else:
        trozos = []
        for quien, texto in partes(ln["texto"], p, ln["momento"]):
            quienes = duo if quien == AMBOS else (quien,)
            trozos.append([tts(clave, g, g["voces"][q], texto) for q in quienes])
        if len(trozos) == 1 and len(trozos[0]) == 1:
            tmp.write_bytes(trozos[0][0])
        else:
            juntar(trozos, tmp)
    tmp.replace(AUDIO / f"{n}.mp3")
    ln["generado"] = [ln["texto"], voz]


# ── La ventana ────────────────────────────────────────────────────────────

def ventana():
    import tkinter as tk
    from tkinter import messagebox, ttk

    g = cargar_guion()
    guardar_guion(g)
    root = tk.Tk()
    root.title("Doblaje Yu-Gi-Oh! con ElevenLabs")
    root.geometry("1100x720")
    st = {"clave": CLAVE.read_text(encoding="utf-8").strip() if CLAVE.is_file() else "",
          "voces": [], "parar": False, "ocupado": False}

    # fila de arriba: clave, conectar, modelo, estabilidad
    top = ttk.Frame(root, padding=6)
    top.pack(fill="x")
    ttk.Label(top, text="Clave de ElevenLabs:").pack(side="left")
    clave_v = tk.StringVar(value=st["clave"])
    ttk.Entry(top, textvariable=clave_v, show="•", width=40).pack(side="left", padx=4)
    con_b = ttk.Button(top, text="Conectar")
    con_b.pack(side="left")
    ttk.Label(top, text="  Modelo:").pack(side="left")
    modelo_v = tk.StringVar(value=g["modelo"])
    modelo_c = ttk.Combobox(top, textvariable=modelo_v, width=22, values=[g["modelo"]])
    modelo_c.pack(side="left")
    ttk.Label(top, text="  Estabilidad:").pack(side="left")
    estab_v = tk.StringVar(value=g["estabilidad"])
    ttk.Combobox(top, textvariable=estab_v, width=22, state="readonly",
                 values=list(ESTABILIDAD)).pack(side="left")

    body = ttk.Frame(root, padding=6)
    body.pack(fill="both", expand=True)
    izq = ttk.Frame(body)
    izq.pack(side="left", fill="y")
    ttk.Label(izq, text="Personajes").pack(anchor="w")
    pers = tk.Listbox(izq, width=24, exportselection=False)
    pers.pack(fill="y", expand=True)

    der = ttk.Frame(body)
    der.pack(side="left", fill="both", expand=True, padx=(8, 0))
    fila_voz = ttk.Frame(der)
    fila_voz.pack(fill="x")
    voz_l = ttk.Label(fila_voz, text="Voz de este personaje:")
    voz_l.pack(side="left")
    voz_v = tk.StringVar()
    voz_c = ttk.Combobox(fila_voz, textvariable=voz_v, width=40, state="readonly")
    voz_c.pack(side="left", padx=4)
    voz2_l = ttk.Label(fila_voz, text="")
    voz2_v = tk.StringVar()
    voz2_c = ttk.Combobox(fila_voz, textvariable=voz2_v, width=40, state="readonly")
    duo_ayuda = ttk.Label(der, foreground="#2a5d9f", text="", wraplength=900)

    cols = ("n", "momento", "estado", "texto")
    tabla = ttk.Treeview(der, columns=cols, show="headings", height=19)
    for c, w in zip(cols, (50, 190, 70, 560)):
        tabla.heading(c, text={"n": "N°", "momento": "Momento", "estado": "Estado",
                               "texto": "Texto"}[c])
        tabla.column(c, width=w, stretch=(c == "texto"))
    tabla.pack(fill="both", expand=True, pady=6)

    ttk.Label(der, text="Texto de la frase (emociones entre corchetes):").pack(anchor="w")
    editor = tk.Text(der, height=4, wrap="word", font=("Segoe UI", 11))
    editor.pack(fill="x")
    ref = ttk.Label(der, text="", foreground="#666", wraplength=900, justify="left")
    ref.pack(anchor="w", pady=(2, 4))
    botones = ttk.Frame(der)
    botones.pack(fill="x")

    abajo = ttk.Frame(root, padding=6)
    abajo.pack(fill="x")
    barra = ttk.Progressbar(abajo, mode="determinate")
    barra.pack(fill="x")
    msg = ttk.Label(abajo, text="")
    msg.pack(anchor="w")

    def personaje():
        s = pers.curselection()
        return g["personajes"][s[0]] if s else None

    def numeros(p):
        return [n for n, ln in g["lineas"].items() if ln["personaje"] == p]

    def refrescar_personajes():
        sel = pers.curselection()
        pers.delete(0, "end")
        for p in g["personajes"]:
            ns = numeros(p)
            hechos = sum(estado(g, n) in ("listo", "sin voz") for n in ns)
            pers.insert("end", f"{p}  ({hechos}/{len(ns)})")
        if sel:
            pers.selection_set(sel[0])

    def refrescar_tabla():
        p = personaje()
        sel = tabla.selection()
        tabla.delete(*tabla.get_children())
        if not p:
            return
        for n in numeros(p):
            ln = g["lineas"][n]
            tabla.insert("", "end", iid=n, values=(n, ln["momento"], estado(g, n), ln["texto"]))
        if sel and tabla.exists(sel[0]):
            tabla.selection_set(sel[0])
        def nombre(voz):
            return next((f"{a}  ({c})" for a, b, c in st["voces"] if b == voz), voz or "")
        duo = DUOS.get(p)
        if duo:
            voz_l.config(text=f"Voz de {duo[0]}:")
            voz2_l.config(text=f"  Voz de {duo[1]}:")
            voz2_l.pack(side="left")
            voz2_c.pack(side="left", padx=4)
            if p in QUIEN_POR_MOMENTO:
                duo_ayuda.config(text="Dos voces: en el duelo habla el Robot; la bienvenida "
                                      "y la despedida (gana, pierde, empate) las dice Kaiba "
                                      "con la voz de Seto (cambiarla acá la cambia también "
                                      "para Seto). Para que una parte la diga el otro, "
                                      "escribí {Robot} o {Seto} antes.")
            else:
                duo_ayuda.config(text=f"Dos voces: escribí {{{duo[0]}}}, {{{duo[1]}}} o "
                                      f"{{{AMBOS}}} antes de cada parte. Ej.: {{{duo[0]}}} "
                                      f"[burlón] ¿Para qué pelear? {{{duo[1]}}} ¡Ya sabemos "
                                      f"quién gana! {{{AMBOS}}} ¡Nosotros!")
            duo_ayuda.pack(after=fila_voz, anchor="w")
            voz_v.set(nombre(g["voces"].get(duo[0])))
            voz2_v.set(nombre(g["voces"].get(duo[1])))
        else:
            voz_l.config(text="Voz de este personaje:")
            voz2_l.pack_forget()
            voz2_c.pack_forget()
            duo_ayuda.pack_forget()
            voz_v.set(nombre(g["voces"].get(p)))

    def al_elegir_personaje(_=None):
        refrescar_tabla()
        kids = tabla.get_children()
        if kids:
            tabla.selection_set(kids[0])

    def al_elegir_frase(_=None):
        s = tabla.selection()
        if not s:
            return
        ln = g["lineas"][s[0]]
        editor.delete("1.0", "end")
        editor.insert("1.0", ln["texto"])
        ref.config(text=f"Juego: {ln['original']}\nInglés: {ln['ingles']}")

    def guardar_texto(_=None):
        s = tabla.selection()
        if not s:
            return
        g["lineas"][s[0]]["texto"] = " ".join(editor.get("1.0", "end").split())
        guardar_guion(g)
        refrescar_tabla()
        refrescar_personajes()

    def al_elegir_voz(cual):
        p = personaje()
        i = (voz_c if cual == 0 else voz2_c).current()
        if p and i >= 0:
            duo = DUOS.get(p)
            g["voces"][duo[cual] if duo else p] = st["voces"][i][1]
            guardar_guion(g)
            refrescar_tabla()
            refrescar_personajes()

    def opciones():
        g["modelo"] = modelo_v.get().strip() or "eleven_v3"
        g["estabilidad"] = estab_v.get()
        guardar_guion(g)

    def conectar():
        st["clave"] = clave_v.get().strip()
        if not st["clave"]:
            messagebox.showinfo("Clave", "Pegá tu clave de ElevenLabs (Perfil → API Keys).")
            return
        msg.config(text="Conectando…")

        def trabajo():
            try:
                v, m = voces(st["clave"]), modelos(st["clave"])
            except RuntimeError as e:
                root.after(0, lambda: (msg.config(text=str(e)),
                                       messagebox.showerror("ElevenLabs", str(e))))
                return

            def listo():
                CLAVE.write_text(st["clave"], encoding="utf-8")
                st["voces"] = v
                voz_c.config(values=[f"{a}  ({c})" for a, _, c in v])
                voz2_c.config(values=[f"{a}  ({c})" for a, _, c in v])
                modelo_c.config(values=m)
                if g["modelo"] not in m:
                    modelo_v.set(next((x for x in ("eleven_v3", "eleven_v4") if x in m),
                                      m[0] if m else g["modelo"]))
                    opciones()
                msg.config(text=f"Conectado: {len(v)} voces, {len(m)} modelos.")
                refrescar_tabla()
            root.after(0, listo)
        threading.Thread(target=trabajo, daemon=True).start()

    def lote(ns, confirmar):
        if st["ocupado"]:
            return
        guardar_texto()
        opciones()
        if not st["clave"]:
            messagebox.showinfo("Clave", "Primero conectá con tu clave.")
            return
        ns = [n for n in ns if estado(g, n) != "sin voz"]
        sin = sorted({g["lineas"][n]["personaje"] for n in ns
                      if not voz_de(g, g["lineas"][n])})
        if sin:
            messagebox.showinfo("Falta la voz", "Elegí la voz de: " + ", ".join(sin))
            return
        if not ns:
            msg.config(text="No hay nada para generar.")
            return
        letras = sum(len(g["lineas"][n]["texto"]) for n in ns)
        if confirmar and not messagebox.askyesno(
                "Generar", f"Se van a generar {len(ns)} frases, unos {letras} caracteres "
                           f"de tu cuota de ElevenLabs. ¿Seguir?"):
            return
        st["parar"], st["ocupado"] = False, True
        barra.config(maximum=len(ns), value=0)

        def trabajo():
            hechas = 0
            for k, n in enumerate(ns):
                if st["parar"]:
                    break
                root.after(0, lambda k=k, n=n: msg.config(
                    text=f"Generando {k + 1}/{len(ns)}: [{n}] {g['lineas'][n]['personaje']}"))
                try:
                    generar(st["clave"], g, n)
                    hechas += 1
                    guardar_guion(g)
                except RuntimeError as e:
                    root.after(0, lambda e=e: messagebox.showerror("ElevenLabs", str(e)))
                    break
                root.after(0, lambda k=k: (barra.config(value=k + 1), refrescar_tabla()))
            st["ocupado"] = False
            root.after(0, lambda: (msg.config(text=f"Listo: {hechas} de {len(ns)} generadas."),
                                   refrescar_tabla(), refrescar_personajes()))
        threading.Thread(target=trabajo, daemon=True).start()

    def escuchar():
        s = tabla.selection()
        f = AUDIO / f"{s[0]}.mp3" if s else None
        if f and f.is_file():
            os.startfile(f)
        else:
            msg.config(text="Esa frase todavía no tiene audio.")

    def esta():
        s = tabla.selection()
        if s:
            lote([s[0]], False)

    def personaje_entero():
        p = personaje()
        if p:
            lote([n for n in numeros(p) if estado(g, n) != "listo"], True)

    def todo():
        lote([n for n in g["lineas"] if estado(g, n) in ("falta", "cambió")], True)

    for t, f in (("Guardar texto", guardar_texto), ("Generar esta frase", esta),
                 ("▶ Escuchar", escuchar), ("Generar este personaje", personaje_entero),
                 ("Generar TODO lo que falta", todo),
                 ("Parar", lambda: st.__setitem__("parar", True))):
        ttk.Button(botones, text=t, command=f).pack(side="left", padx=(0, 6))

    con_b.config(command=conectar)
    pers.bind("<<ListboxSelect>>", al_elegir_personaje)
    tabla.bind("<<TreeviewSelect>>", al_elegir_frase)
    tabla.bind("<Double-1>", lambda e: escuchar())
    voz_c.bind("<<ComboboxSelected>>", lambda e: al_elegir_voz(0))
    voz2_c.bind("<<ComboboxSelected>>", lambda e: al_elegir_voz(1))
    modelo_c.bind("<<ComboboxSelected>>", lambda e: opciones())
    editor.bind("<Control-s>", guardar_texto)
    refrescar_personajes()
    pers.selection_set(0)
    al_elegir_personaje()
    if st["clave"]:
        conectar()
    root.mainloop()


def revisar():
    g = cargar_guion()
    guardar_guion(g)
    con = sum(tiene_voz(ln["texto"]) for ln in g["lineas"].values())
    print(f"guion: {len(g['lineas'])} frases ({con} con voz) de {len(g['personajes'])} personajes"
          f" -> {GUION}")
    for n in ("336", "350", "353", "355", "621", "697", "830"):
        ln = g["lineas"][n]
        print(f"  [{n}] {ln['personaje']} / {ln['momento']}: {ln['texto'][:90]}")
    print("pedido de ejemplo: POST", f"{API}/text-to-speech/<voz>?output_format={FORMATO}")
    print(" ", json.dumps(cuerpo_pedido(g, g["lineas"]["355"]["texto"]), ensure_ascii=False))


if __name__ == "__main__":
    revisar() if "--revisar" in sys.argv else ventana()
