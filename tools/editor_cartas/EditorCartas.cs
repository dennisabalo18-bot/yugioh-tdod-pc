// Editor de cartas (el mod de cartas, opcional): una ventana para armar
// modding/cartas/cartas.txt sin editarlo a mano, crear el mod con
// tools/cartas_mod.py y copiarlo a Mods-Extras\cartas del juego.
// Necesita lo mismo que la herramienta: Python 3 (py), Pillow y numpy, el
// juego en ..\Yu-Gi-Oh TDOD y las cartas escaladas (modding/texturas/mejoradas).
// Compilar: tools\editor_cartas\compilar.ps1 (sale "Editor de cartas.exe" en la carpeta del port).
using System;
using System.Collections;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading;
using System.Web.Script.Serialization;
using System.Windows.Forms;

class CartaJuego
{
    public int Id, K, Tipo, Atrib, Nivel, Atk, Def, Clase;
    public string Nombre, Desc, TipoTxt;
    public bool EfectoJuego;
    public bool EsTrampa { get { return Tipo == 21; } }
    public bool EsMagica { get { return Tipo == 22; } }
    public bool EsMonstruo { get { return Tipo < 21; } }
    public string Categoria { get { return EsTrampa ? "trampa" : EsMagica ? "magica" : "monstruo"; } }
    public override string ToString()
    {
        if (EsTrampa) return Id + "  " + Nombre + "   (Trampa)";
        if (EsMagica) return Id + "  " + Nombre + "   (Mágica)";
        return Id + "  " + Nombre + "   (" + TipoTxt + ", nivel " + Nivel + ", " + Atk + "/" + Def +
            (Clase == 1 ? ", efecto" : Clase == 2 ? ", fusión" : Clase == 3 ? ", ritual" : "") + ")";
    }
}

class CartaMod
{
    public int Id;
    public List<string> Notas = new List<string>();          // los # de arriba del [ID]
    public Dictionary<string, string> Campos = new Dictionary<string, string>();
    public string Get(string k) { string v; return Campos.TryGetValue(k, out v) ? v : ""; }
    public void Set(string k, string v)
    {
        if (string.IsNullOrEmpty(v)) Campos.Remove(k); else Campos[k] = v;
    }
}

class Editor : Form
{
    static readonly Color Bg = Color.FromArgb(16, 18, 34);
    static readonly Color Pnl = Color.FromArgb(30, 33, 60);
    static readonly Color Paper = Color.FromArgb(236, 230, 210);
    static readonly Color Muted = Color.FromArgb(150, 150, 180);
    static readonly Color Accent = Color.FromArgb(212, 168, 60);
    static readonly Color Bad = Color.FromArgb(230, 90, 70);
    static readonly string[] Orden = { "nombre", "descripcion", "ilustracion", "base", "nivel", "atk", "def",
                                       "atributo", "tipo", "clase", "efecto" };

    readonly string port, txt, carpetaMod, ini;
    List<CartaJuego> juego = new List<CartaJuego>();
    List<string> tipos = new List<string>(), atributos = new List<string>();
    Dictionary<string, string[]> efectos = new Dictionary<string, string[]>();
    List<string> cabecera = new List<string>();
    List<CartaMod> mod = new List<CartaMod>();
    CartaMod actual;
    bool cargando;

    ListBox lstMod, lstJuego;
    TextBox txtBuscar, txtNombre, txtDesc, txtLog, txtJuegoDir;
    Label lblOrig, lblEfecto, lblEstado, lblIlus;
    PictureBox picIlus, picVista;
    ComboBox cbAtrib, cbTipo, cbClase, cbEfecto;
    NumericUpDown numNivel, numAtk, numDef;
    Panel pMonstruo;
    Button btnCrear;

    [STAThread]
    static void Main()
    {
        Application.EnableVisualStyles();
        Application.Run(new Editor());
    }

    Editor()
    {
        port = Path.GetDirectoryName(Application.ExecutablePath);
        if (!File.Exists(Path.Combine(port, "tools", "cartas_mod.py")))
            port = Path.GetFullPath(Path.Combine(port, "..", ".."));     // corriendo desde tools\editor_cartas
        carpetaMod = Path.Combine(port, "modding", "cartas");
        txt = Path.Combine(carpetaMod, "cartas.txt");
        ini = Path.Combine(port, "editor_cartas.ini");
        Text = "Editor de cartas — Yu-Gi-Oh! The Dawn of Destiny";
        BackColor = Bg; ForeColor = Paper; Font = new Font("Segoe UI", 9.5f);
        ClientSize = new Size(1180, 760);
        StartPosition = FormStartPosition.CenterScreen;
        try { Icon = Icon.ExtractAssociatedIcon(Application.ExecutablePath); } catch { }
        Armar();
        Shown += (s, e) => Cargar();
    }

    // ---------------------------------------------------------------- ventana

    Label Lbl(Control padre, string t, int x, int y, int w = 300, bool muted = false)
    {
        var l = new Label { Text = t, Location = new Point(x, y), AutoSize = false, Size = new Size(w, 20),
                            ForeColor = muted ? Muted : Paper };
        padre.Controls.Add(l);
        return l;
    }

    Button Btn(Control padre, string t, int x, int y, int w, EventHandler click, bool fuerte = false)
    {
        var b = new Button { Text = t, Location = new Point(x, y), Size = new Size(w, 30), FlatStyle = FlatStyle.Flat,
                             BackColor = fuerte ? Accent : Pnl, ForeColor = fuerte ? Bg : Paper };
        b.FlatAppearance.BorderColor = Accent;
        if (fuerte) b.Font = new Font(Font, FontStyle.Bold);
        b.Click += click;
        padre.Controls.Add(b);
        return b;
    }

    T Campo<T>(Control padre, T c, int x, int y, int w) where T : Control
    {
        c.Location = new Point(x, y); c.Width = w;
        c.BackColor = Pnl; c.ForeColor = Paper;
        padre.Controls.Add(c);
        return c;
    }

    ComboBox Combo(Control padre, int x, int y, int w)
    {
        var c = Campo(padre, new ComboBox { DropDownStyle = ComboBoxStyle.DropDownList, FlatStyle = FlatStyle.Flat }, x, y, w);
        c.SelectedIndexChanged += (s, e) => Cambio();
        return c;
    }

    NumericUpDown Num(Control padre, int x, int y, int max, int paso)
    {
        var n = Campo(padre, new NumericUpDown { Maximum = max, Minimum = 0, Increment = paso }, x, y, 90);
        n.ValueChanged += (s, e) => Cambio();
        return n;
    }

    void Armar()
    {
        // izquierda: las cartas del mod
        Lbl(this, "Cartas del mod", 16, 14).Font = new Font(Font, FontStyle.Bold);
        lstMod = Campo(this, new ListBox { Height = 520, IntegralHeight = false, BorderStyle = BorderStyle.FixedSingle }, 16, 38, 250);
        lstMod.SelectedIndexChanged += (s, e) => Mostrar();
        Btn(this, "+ Nueva carta", 16, 566, 122, (s, e) => Nueva());
        Btn(this, "Quitar", 144, 566, 122, (s, e) => Quitar());

        // centro: la carta
        var c = new Panel { Location = new Point(282, 10), Size = new Size(560, 600) };
        Controls.Add(c);
        Lbl(c, "1. Carta del juego que se reemplaza (escribí para buscar)", 0, 4, 560).Font = new Font(Font, FontStyle.Bold);
        txtBuscar = Campo(c, new TextBox { BorderStyle = BorderStyle.FixedSingle }, 0, 28, 560);
        txtBuscar.TextChanged += (s, e) => Filtrar();
        lstJuego = Campo(c, new ListBox { Height = 120, IntegralHeight = false, BorderStyle = BorderStyle.FixedSingle }, 0, 54, 560);
        lstJuego.SelectedIndexChanged += (s, e) => ElegirJuego();
        lblOrig = Lbl(c, "", 0, 178, 560, true);
        lblOrig.Height = 52;

        Lbl(c, "2. La carta nueva", 0, 236, 560).Font = new Font(Font, FontStyle.Bold);
        Lbl(c, "Nombre", 0, 262, 100, true);
        txtNombre = Campo(c, new TextBox { BorderStyle = BorderStyle.FixedSingle }, 100, 260, 460);
        txtNombre.TextChanged += (s, e) => Cambio();
        Lbl(c, "Descripción", 0, 292, 100, true);
        txtDesc = Campo(c, new TextBox { Multiline = true, Height = 70, BorderStyle = BorderStyle.FixedSingle,
                                         ScrollBars = ScrollBars.Vertical }, 100, 290, 460);
        txtDesc.TextChanged += (s, e) => Cambio();
        Lbl(c, "Ilustración", 0, 370, 100, true);
        Btn(c, "Elegir imagen…", 100, 366, 140, (s, e) => ElegirIlus());
        lblIlus = Lbl(c, "", 250, 372, 310, true);

        pMonstruo = new Panel { Location = new Point(0, 404), Size = new Size(560, 96) };
        c.Controls.Add(pMonstruo);
        Lbl(pMonstruo, "Atributo", 0, 4, 100, true);
        cbAtrib = Combo(pMonstruo, 100, 0, 150);
        Lbl(pMonstruo, "Tipo", 270, 4, 60, true);
        cbTipo = Combo(pMonstruo, 330, 0, 230);
        Lbl(pMonstruo, "Clase", 0, 36, 100, true);
        cbClase = Combo(pMonstruo, 100, 32, 150);
        cbClase.Items.AddRange(new object[] { "Normal", "Efecto" });
        Lbl(pMonstruo, "Nivel", 270, 36, 60, true);
        numNivel = Num(pMonstruo, 330, 32, 12, 1);
        numNivel.Minimum = 1;
        Lbl(pMonstruo, "ATK", 0, 68, 100, true);
        numAtk = Num(pMonstruo, 100, 64, 5110, 50);
        Lbl(pMonstruo, "DEF", 270, 68, 60, true);
        numDef = Num(pMonstruo, 330, 64, 5110, 50);

        Lbl(c, "Efecto", 0, 512, 100, true);
        cbEfecto = Combo(c, 100, 508, 460);
        lblEfecto = Lbl(c, "", 100, 538, 460, true);
        lblEfecto.Height = 60;

        // derecha: imagen
        Lbl(this, "Ilustración / vista previa", 860, 14, 300).Font = new Font(Font, FontStyle.Bold);
        picIlus = new PictureBox { Location = new Point(860, 38), Size = new Size(150, 150), SizeMode = PictureBoxSizeMode.Zoom,
                                   BackColor = Pnl, BorderStyle = BorderStyle.FixedSingle };
        Controls.Add(picIlus);
        picVista = new PictureBox { Location = new Point(860, 196), Size = new Size(300, 414), SizeMode = PictureBoxSizeMode.Zoom,
                                    BackColor = Pnl, BorderStyle = BorderStyle.FixedSingle };
        Controls.Add(picVista);

        // abajo: crear e instalar
        Lbl(this, "Carpeta del juego", 16, 624, 130, true);
        txtJuegoDir = Campo(this, new TextBox { BorderStyle = BorderStyle.FixedSingle }, 150, 621, 560);
        Btn(this, "…", 716, 618, 36, (s, e) => ElegirJuegoDir());
        btnCrear = Btn(this, "Crear el mod e instalarlo en el juego", 770, 616, 390, (s, e) => Crear(), true);
        lblEstado = Lbl(this, "", 16, 652, 1140, true);
        txtLog = Campo(this, new TextBox { Multiline = true, ReadOnly = true, ScrollBars = ScrollBars.Vertical,
                                           BorderStyle = BorderStyle.FixedSingle, Font = new Font("Consolas", 8.5f) }, 16, 674, 1144);
        txtLog.Height = 74;
        FormClosing += (s, e) => { Guardar(); GuardarIni(); };
    }

    // ---------------------------------------------------------------- datos

    void Cargar()
    {
        Estado("Leyendo las cartas del juego…");
        txtJuegoDir.Text = LeerIni();
        string json;
        int rc = Python(new[] { "tools\\cartas_mod.py", "--lista" }, out json, null);
        if (rc != 0) {
            Estado("No pude leer las cartas del juego (hace falta Python 3 con Pillow y numpy).", true);
            txtLog.Text = json;
            return;
        }
        var js = new JavaScriptSerializer { MaxJsonLength = int.MaxValue };
        var d = (Dictionary<string, object>)js.DeserializeObject(json);
        foreach (Dictionary<string, object> o in (object[])d["cartas"])
            juego.Add(new CartaJuego {
                Id = (int)o["id"], K = (int)o["k"], Tipo = (int)o["tipo"], Atrib = (int)o["atrib"], Nivel = (int)o["nivel"],
                Atk = (int)o["atk"], Def = (int)o["df"], Clase = (int)o["clase"], Nombre = (string)o["nombre"],
                Desc = (string)o["desc"], TipoTxt = (string)o["tipo_txt"], EfectoJuego = (bool)o["efecto_juego"] });
        juego.Sort((a, b) => string.Compare(a.Nombre, b.Nombre, StringComparison.CurrentCultureIgnoreCase));
        foreach (object t in (object[])d["tipos"]) tipos.Add((string)t);
        foreach (object t in (object[])d["atributos"]) atributos.Add((string)t);
        foreach (var kv in (Dictionary<string, object>)d["efectos"])
            efectos[kv.Key] = ((object[])kv.Value).Cast<string>().ToArray();
        cargando = true;
        foreach (var a in atributos) cbAtrib.Items.Add(Mayus(a));
        for (int i = 0; i < 21; i++) cbTipo.Items.Add(tipos[i]);
        cargando = false;
        LeerTxt();
        Filtrar();
        Estado("Listo. Elegí una carta de la izquierda o tocá \"+ Nueva carta\".");
        if (lstMod.Items.Count > 0) lstMod.SelectedIndex = 0; else Mostrar();
    }

    static string Mayus(string s) { return s.Length == 0 ? s : char.ToUpper(s[0]) + s.Substring(1); }

    void LeerTxt()
    {
        mod.Clear(); cabecera.Clear();
        if (!File.Exists(txt)) return;
        // los # pegados a un [ID] son la nota de esa carta; antes de la
        // primera, los bloques separados por una linea en blanco son la cabecera
        var notas = new List<string>();
        CartaMod c = null;
        foreach (var l0 in File.ReadAllLines(txt, Encoding.UTF8)) {
            var l = l0.Trim();
            var m = Regex.Match(l, @"^\[(\d+)\]$");
            if (m.Success) {
                c = new CartaMod { Id = int.Parse(m.Groups[1].Value), Notas = notas };
                notas = new List<string>();
                mod.Add(c);
            } else if (l.StartsWith("#")) {
                notas.Add(l0);
            } else if (l.Length == 0) {
                if (mod.Count == 0) { cabecera.AddRange(notas); cabecera.Add(l0); notas.Clear(); }
            } else if (c != null && l.Contains("=")) {
                int i = l.IndexOf('=');
                c.Campos[l.Substring(0, i).Trim().ToLower()] = l.Substring(i + 1).Trim();
            }
        }
        while (cabecera.Count > 0 && cabecera[cabecera.Count - 1].Trim().Length == 0) cabecera.RemoveAt(cabecera.Count - 1);
        Lista();
    }

    void Guardar()
    {
        if (actual == null && mod.Count == 0 && !File.Exists(txt)) return;
        var sb = new StringBuilder();
        foreach (var l in cabecera) sb.Append(l).Append("\r\n");
        foreach (var c in mod) {
            if (c.Id == 0) continue;            // todavia sin carta del juego
            sb.Append("\r\n");
            foreach (var n in c.Notas) sb.Append(n).Append("\r\n");
            sb.Append("[").Append(c.Id).Append("]\r\n");
            foreach (var k in Orden)
                if (c.Campos.ContainsKey(k)) sb.Append(k).Append(" = ").Append(c.Campos[k]).Append("\r\n");
            foreach (var kv in c.Campos)
                if (Array.IndexOf(Orden, kv.Key) < 0) sb.Append(kv.Key).Append(" = ").Append(kv.Value).Append("\r\n");
        }
        Directory.CreateDirectory(carpetaMod);
        File.WriteAllText(txt, sb.ToString(), new UTF8Encoding(false));
    }

    string LeerIni()
    {
        try { if (File.Exists(ini)) return File.ReadAllText(ini, Encoding.UTF8).Trim(); } catch { }
        var def = @"C:\Emuladores y Juegos\Yu-Gi-Oh Port";
        return Directory.Exists(def) ? def : "";
    }

    void GuardarIni() { try { File.WriteAllText(ini, txtJuegoDir.Text.Trim(), Encoding.UTF8); } catch { } }

    CartaJuego Juego(int id) { return juego.FirstOrDefault(j => j.Id == id); }

    void Lista()
    {
        cargando = true;
        int i = lstMod.SelectedIndex;
        lstMod.Items.Clear();
        foreach (var c in mod) {
            var j = Juego(c.Id);
            lstMod.Items.Add((c.Get("nombre").Length > 0 ? c.Get("nombre") : "(sin nombre)") +
                             "  ←  " + (j != null ? j.Nombre : c.Id.ToString()));
        }
        if (i >= 0 && i < lstMod.Items.Count) lstMod.SelectedIndex = i;
        cargando = false;
    }

    // ---------------------------------------------------------------- acciones

    void Nueva()
    {
        var c = new CartaMod { Id = 0 };
        mod.Add(c);
        Lista();
        lstMod.SelectedIndex = mod.Count - 1;
        txtBuscar.Focus();
        Estado("Buscá la carta del juego que querés reemplazar (una que no uses).");
    }

    void Quitar()
    {
        if (actual == null) return;
        if (MessageBox.Show(this, "¿Quitar \"" + actual.Get("nombre") + "\" del mod? La carta del juego vuelve a ser la original.",
                            "Quitar carta", MessageBoxButtons.YesNo, MessageBoxIcon.Question) != DialogResult.Yes) return;
        mod.Remove(actual);
        actual = null;
        Guardar();
        Lista();
        if (mod.Count > 0) lstMod.SelectedIndex = 0; else Mostrar();
    }

    void Filtrar()
    {
        var q = txtBuscar.Text.Trim().ToLower();
        var sel = actual != null ? Juego(actual.Id) : null;
        cargando = true;
        lstJuego.BeginUpdate();
        lstJuego.Items.Clear();
        foreach (var j in juego)
            if (q.Length == 0 || j.Nombre.ToLower().Contains(q) || j.Id.ToString() == q || j.TipoTxt.ToLower() == q)
                lstJuego.Items.Add(j);
        if (sel != null) lstJuego.SelectedItem = sel;
        lstJuego.EndUpdate();
        cargando = false;
    }

    void Mostrar()
    {
        if (cargando) return;
        actual = lstMod.SelectedIndex >= 0 && lstMod.SelectedIndex < mod.Count ? mod[lstMod.SelectedIndex] : null;
        cargando = true;
        bool hay = actual != null;
        foreach (Control x in new Control[] { txtBuscar, lstJuego, txtNombre, txtDesc, cbEfecto, pMonstruo }) x.Enabled = hay;
        txtNombre.Text = hay ? actual.Get("nombre") : "";
        txtDesc.Text = hay ? actual.Get("descripcion") : "";
        txtBuscar.Text = "";
        cargando = false;
        Filtrar();
        Rellenar();
        MostrarIlus();
        picVista.Image = null;
        if (hay) MostrarHd(actual.Id);
    }

    // los campos que dependen de la carta del juego (los de monstruo, los efectos)
    void Rellenar()
    {
        cargando = true;
        var j = actual != null ? Juego(actual.Id) : null;
        if (j == null) {
            lblOrig.Text = actual != null ? "Elegí la carta del juego de la lista." : "";
            pMonstruo.Visible = false;
            cbEfecto.Items.Clear();
            lblEfecto.Text = "";
            cargando = false;
            return;
        }
        lblOrig.Text = "Original: " + j + "\n" + j.Desc +
            (j.EfectoJuego ? "\n⚠ Tiene un efecto en el juego: la carta nueva lo conserva salvo que elijas otro efecto." : "");
        lblOrig.ForeColor = j.EfectoJuego ? Accent : Muted;
        pMonstruo.Visible = j.EsMonstruo;
        if (j.EsMonstruo) {
            var at = actual.Get("atributo");
            cbAtrib.SelectedIndex = at.Length > 0 ? atributos.IndexOf(at.ToLower()) : Math.Max(0, j.Atrib - 1);
            var tp = actual.Get("tipo");
            int ti = tp.Length > 0 ? tipos.FindIndex(t => t.ToLower() == tp.ToLower()) : j.Tipo;
            cbTipo.SelectedIndex = ti >= 0 && ti < cbTipo.Items.Count ? ti : 0;
            var cl = actual.Get("clase");
            cbClase.SelectedIndex = cl.Length > 0 ? (cl.ToLower() == "efecto" ? 1 : 0) : (j.Clase == 1 ? 1 : 0);
            cbClase.Enabled = j.Clase <= 1;          // fusion y ritual: no se cambian
            numNivel.Value = Math.Max(1, Math.Min(12, Num(actual.Get("nivel"), j.Nivel)));
            numAtk.Value = Math.Min(5110, Num(actual.Get("atk"), j.Atk));
            numDef.Value = Math.Min(5110, Num(actual.Get("def"), j.Def));
        }
        cbEfecto.Items.Clear();
        cbEfecto.Items.Add(j.EfectoJuego ? "El de la carta original" : "Ninguno");
        foreach (var kv in efectos)
            if (kv.Value[0] == j.Categoria) cbEfecto.Items.Add(kv.Key);
        var ef = actual.Get("efecto");
        cbEfecto.SelectedIndex = Math.Max(0, cbEfecto.Items.IndexOf(ef));
        cargando = false;
        DescEfecto();
    }

    static int Num(string s, int def) { int v; return int.TryParse(s, out v) ? v : def; }

    void DescEfecto()
    {
        string[] e;
        var k = cbEfecto.SelectedItem as string;
        lblEfecto.Text = k != null && efectos.TryGetValue(k, out e) ? e[1] :
            "Los efectos nuevos se programan en el juego (src/cartas.c); acá se eligen los que ya están.";
    }

    void ElegirJuego()
    {
        if (cargando || actual == null) return;
        var j = lstJuego.SelectedItem as CartaJuego;
        if (j == null || j.Id == actual.Id) return;
        if (mod.Any(m => m != actual && m.Id == j.Id)) {
            Estado("Esa carta ya la reemplaza otra del mod.", true);
            return;
        }
        var antes = Juego(actual.Id);
        if (antes != null && antes.Categoria != j.Categoria) {
            // otra categoria: los datos de monstruo y el efecto no sirven
            foreach (var k in new[] { "atributo", "tipo", "clase", "nivel", "atk", "def", "efecto", "base" }) actual.Campos.Remove(k);
        }
        actual.Id = j.Id;
        actual.Notas = new List<string> { "# " + j };
        Rellenar();
        Cambio();
        Lista();
    }

    // un campo cambio: a la carta del mod (solo lo distinto del original)
    void Cambio()
    {
        if (cargando || actual == null) return;
        actual.Set("nombre", txtNombre.Text.Trim());
        actual.Set("descripcion", Regex.Replace(txtDesc.Text, @"\s+", " ").Trim());
        var j = Juego(actual.Id);
        if (j != null && j.EsMonstruo && cbAtrib.SelectedIndex >= 0) {
            string at = atributos[cbAtrib.SelectedIndex], tp = (string)cbTipo.SelectedItem;
            actual.Set("atributo", cbAtrib.SelectedIndex + 1 != j.Atrib ? at : "");
            actual.Set("tipo", cbTipo.SelectedIndex != j.Tipo ? tp : "");
            int cl = cbClase.SelectedIndex;
            actual.Set("clase", j.Clase <= 1 && cl != j.Clase ? (cl == 1 ? "efecto" : "normal") : "");
            actual.Set("nivel", (int)numNivel.Value != j.Nivel ? ((int)numNivel.Value).ToString() : "");
            int atk = (int)numAtk.Value / 10 * 10, df = (int)numDef.Value / 10 * 10;
            actual.Set("atk", atk != j.Atk ? atk.ToString() : "");
            actual.Set("def", df != j.Def ? df.ToString() : "");
        }
        if (cbEfecto.SelectedIndex >= 0)
            actual.Set("efecto", cbEfecto.SelectedIndex == 0 ? "" : (string)cbEfecto.SelectedItem);
        DescEfecto();
        int i = lstMod.SelectedIndex;
        if (i >= 0) {
            cargando = true;
            lstMod.Items[i] = (actual.Get("nombre").Length > 0 ? actual.Get("nombre") : "(sin nombre)") +
                              "  ←  " + (j != null ? j.Nombre : "?");
            cargando = false;
        }
    }

    void ElegirIlus()
    {
        if (actual == null) return;
        using (var d = new OpenFileDialog { Title = "La ilustración de la carta (cuadrada; si no, se recorta al centro)",
                                            Filter = "Imágenes|*.webp;*.png;*.jpg;*.jpeg;*.bmp|Todos|*.*" }) {
            if (d.ShowDialog(this) != DialogResult.OK) return;
            var nombre = Regex.Replace(Path.GetFileNameWithoutExtension(d.FileName).ToLower(), @"^foto_", "");
            nombre = Regex.Replace(nombre.Replace("%3f", "_"), @"[^a-z0-9_.-]+", "_").Trim('_');
            if (nombre.Length == 0) nombre = "carta_" + actual.Id;
            var dst = Path.Combine(carpetaMod, nombre + Path.GetExtension(d.FileName).ToLower());
            try {
                if (!string.Equals(Path.GetFullPath(d.FileName), Path.GetFullPath(dst), StringComparison.OrdinalIgnoreCase))
                    File.Copy(d.FileName, dst, true);
            } catch (Exception ex) { Estado("No pude copiar la imagen: " + ex.Message, true); return; }
            actual.Set("ilustracion", Path.GetFileName(dst));
            MostrarIlus();
        }
    }

    void MostrarIlus()
    {
        picIlus.Image = null;
        var f = actual != null ? actual.Get("ilustracion") : "";
        lblIlus.Text = f.Length > 0 ? f : "(falta la imagen)";
        lblIlus.ForeColor = f.Length > 0 ? Muted : Bad;
        if (f.Length > 0) picIlus.Image = Imagen(Path.Combine(carpetaMod, f), false);
    }

    // cualquier imagen (también .webp, que .NET no abre) por la herramienta
    Image Imagen(string path, bool hd)
    {
        if (!File.Exists(path)) return null;
        try {
            var tmp = Path.Combine(Path.GetTempPath(), "editor_cartas_" + Guid.NewGuid().ToString("N") + ".png");
            string o;
            var args = new List<string> { "tools\\cartas_mod.py", "--vista", path, tmp };
            if (hd) args.Add("hd");
            if (Python(args.ToArray(), out o, null) != 0 || !File.Exists(tmp)) return null;
            Image im;
            using (var fs = File.OpenRead(tmp)) im = Image.FromStream(new MemoryStream(ReadAll(fs)));
            File.Delete(tmp);
            return im;
        } catch { return null; }
    }

    static byte[] ReadAll(Stream s) { var m = new MemoryStream(); s.CopyTo(m); return m.ToArray(); }

    // la ultima carta HD creada para ese ID (la salida de cartas_mod.py)
    Dictionary<int, string> hdPorId = new Dictionary<int, string>();

    void MostrarHd(int id)
    {
        string h;
        picVista.Image = null;
        if (hdPorId.TryGetValue(id, out h)) {
            var p = Path.Combine(carpetaMod, "salida", "hd", h + "_128x256.png");
            picVista.Image = Imagen(p, true);
        }
    }

    void ElegirJuegoDir()
    {
        using (var d = new FolderBrowserDialog { Description = "La carpeta del juego (donde está Yu-Gi-Oh.exe)",
                                                 SelectedPath = txtJuegoDir.Text }) {
            if (d.ShowDialog(this) == DialogResult.OK) txtJuegoDir.Text = d.SelectedPath;
        }
    }

    // ---------------------------------------------------------------- crear

    void Crear()
    {
        foreach (var c in mod) {
            var j = Juego(c.Id);
            string falta = j == null ? "la carta del juego" : c.Get("nombre").Length == 0 ? "el nombre" :
                           c.Get("descripcion").Length == 0 ? "la descripción" : c.Get("ilustracion").Length == 0 ? "la ilustración" : null;
            if (falta != null) {
                lstMod.SelectedIndex = mod.IndexOf(c);
                Estado("A esta carta le falta " + falta + ".", true);
                return;
            }
        }
        var dir = txtJuegoDir.Text.Trim();
        if (dir.Length == 0 || !File.Exists(Path.Combine(dir, "Yu-Gi-Oh.exe"))) {
            Estado("Elegí la carpeta del juego (la que tiene Yu-Gi-Oh.exe).", true);
            return;
        }
        Guardar();
        GuardarIni();
        btnCrear.Enabled = false;
        txtLog.Clear();
        Estado("Creando el mod (puede tardar un minuto)…");
        var t = new Thread(() => {
            string salida;
            int rc = Python(new[] { "tools\\cartas_mod.py" }, out salida, l => BeginInvoke((Action)(() => {
                txtLog.AppendText(l + "\r\n");
            })));
            BeginInvoke((Action)(() => Terminar(rc, salida, dir)));
        }) { IsBackground = true };
        t.Start();
    }

    void Terminar(int rc, string salida, string dir)
    {
        btnCrear.Enabled = true;
        if (rc != 0) {
            Estado("No se pudo crear el mod: mirá el mensaje de abajo.", true);
            return;
        }
        int id = 0;
        foreach (var l in salida.Split('\n')) {
            var m = Regex.Match(l, @"^\s+(\d+): ");
            if (m.Success) id = int.Parse(m.Groups[1].Value);
            m = Regex.Match(l, @"^\s+grande ([0-9A-F]{16})");
            if (m.Success && id != 0) hdPorId[id] = m.Groups[1].Value;
        }
        try {
            var dst = Path.Combine(dir, "Mods-Extras", "Contenido", "cartas");
            if (Directory.Exists(dst)) Directory.Delete(dst, true);
            Copiar(Path.Combine(carpetaMod, "salida", "cartas"), dst);
        } catch (Exception ex) {
            Estado("El mod está creado, pero no pude copiarlo al juego (¿está abierto? cerralo): " + ex.Message, true);
            return;
        }
        Estado("Listo: el mod quedó instalado en " + Path.Combine(dir, "Mods-Extras", "Contenido", "cartas") +
               ". Abrí el juego (F2 > Cartas nuevas: Sí).");
        if (actual != null) MostrarHd(actual.Id);
    }

    static void Copiar(string src, string dst)
    {
        Directory.CreateDirectory(dst);
        foreach (var f in Directory.GetFiles(src)) File.Copy(f, Path.Combine(dst, Path.GetFileName(f)), true);
        foreach (var d in Directory.GetDirectories(src)) Copiar(d, Path.Combine(dst, Path.GetFileName(d)));
    }

    // py -3 con los argumentos, en la carpeta del port; la salida entera (y linea a linea)
    int Python(string[] args, out string salida, Action<string> linea)
    {
        var psi = new ProcessStartInfo("py", "-3 " + string.Join(" ", args.Select(Arg))) {
            WorkingDirectory = port, UseShellExecute = false, CreateNoWindow = true,
            RedirectStandardOutput = true, RedirectStandardError = true,
            StandardOutputEncoding = Encoding.UTF8, StandardErrorEncoding = Encoding.UTF8 };
        psi.EnvironmentVariables["PYTHONIOENCODING"] = "utf-8";
        var sb = new StringBuilder();
        try {
            using (var p = Process.Start(psi)) {
                DataReceivedEventHandler h = (s, e) => {
                    if (e.Data == null) return;
                    lock (sb) sb.AppendLine(e.Data);
                    if (linea != null) linea(e.Data);
                };
                p.OutputDataReceived += h;
                p.ErrorDataReceived += h;
                p.BeginOutputReadLine();
                p.BeginErrorReadLine();
                p.WaitForExit();
                p.WaitForExit();
                salida = sb.ToString();
                return p.ExitCode;
            }
        } catch (Exception ex) {
            salida = "No pude correr Python (py): " + ex.Message;
            return -1;
        }
    }

    static string Arg(string a) { return a.Contains(" ") ? "\"" + a + "\"" : a; }

    void Estado(string s, bool error = false)
    {
        lblEstado.Text = s;
        lblEstado.ForeColor = error ? Bad : Accent;
    }
}
