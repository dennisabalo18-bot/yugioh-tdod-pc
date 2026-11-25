// Perfiles.cs - the profiles (2026-11-25, the user: a folder per profile and,
// inside it, one per game slot; mock-up approved). The same layout as the
// game's perfil_dir.c:
//   Partidas\<profile>\UDATA, TDATA   the game's save (all its slots)
//   Partidas\<profile>\respaldos\     savedata copies
//   Partidas\<profile>\Ranura N\      ours for game slot N - 1
// The active one: Ajustes\perfil_activo.txt (UTF-8, one line).
// The old layout (Partidas\UDATA + Partidas\Perfil) moves into the first
// profile, named after perfil.ini's name ("Jugador 1" if none); its files go
// to Ranura 1 (the slot the user played). The game does the same at start.
using System;
using System.Collections.Generic;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Text;
using System.Windows.Forms;

static class Perfiles
{
    static readonly string[] Archivos = { "perfil.ini", "duelos.txt", "prestados.txt", "cartas_nuevas.txt",
                                          "premios_registro.txt" };

    public static string Dir(string root) { return Path.Combine(root, "Partidas"); }
    static string ActivoFile(string root) { return Path.Combine(root, "Ajustes", "perfil_activo.txt"); }

    // a folder name from a name (the game's limpiar() in perfil_dir.c)
    public static string Limpiar(string n)
    {
        var sb = new StringBuilder();
        var ansi = Encoding.GetEncoding(Encoding.Default.CodePage, EncoderFallback.ReplacementFallback, DecoderFallback.ReplacementFallback);
        foreach (char c in n ?? "")
            if (c >= 32 && "\\/:*?\"<>|".IndexOf(c) < 0) {
                // the game hands the save folder to the kit as ANSI (perfil_dir.c): a letter
                // this PC's code page lacks becomes '_', as in the game
                string back = ansi.GetString(ansi.GetBytes(c.ToString()));
                sb.Append(back == c.ToString() ? c : '_');
            }
        string s = sb.ToString().Trim(' ', '.');
        return s.Length == 0 ? "Jugador 1" : s;
    }

    static void Mover(string de, string a)
    {
        try {
            if (Directory.Exists(de)) Directory.Move(de, a);
            else if (File.Exists(de)) File.Move(de, a);
        } catch { }
    }

    public static void Migrar(string root)
    {
        string p = Dir(root), viejo = Path.Combine(p, "Perfil");
        if (!Directory.Exists(Path.Combine(p, "UDATA")) && !Directory.Exists(viejo)) return;
        string nombre = "Jugador 1";
        try {
            string ini = Path.Combine(viejo, "perfil.ini");
            if (File.Exists(ini))
                foreach (var l in File.ReadAllLines(ini, Encoding.UTF8))
                    if (l.StartsWith("nombre=") && l.Length > 7) nombre = l.Substring(7);
        } catch { }
        nombre = Limpiar(nombre);
        string b = Path.Combine(p, nombre), ran = Path.Combine(b, "Ranura 1");
        Directory.CreateDirectory(ran);
        Mover(Path.Combine(p, "UDATA"), Path.Combine(b, "UDATA"));
        Mover(Path.Combine(p, "TDATA"), Path.Combine(b, "TDATA"));
        Mover(Path.Combine(viejo, "respaldos"), Path.Combine(b, "respaldos"));
        foreach (var a in Archivos) Mover(Path.Combine(viejo, a), Path.Combine(ran, a));
        try { if (Directory.Exists(viejo) && !Directory.EnumerateFileSystemEntries(viejo).Any()) Directory.Delete(viejo); } catch { }
        if (Activo(root) == null) SetActivo(root, nombre);
    }

    public static List<string> Lista(string root)
    {
        try {
            return Directory.GetDirectories(Dir(root)).Select(Path.GetFileName)
                .OrderBy(s => s, StringComparer.CurrentCultureIgnoreCase).ToList();
        } catch { return new List<string>(); }
    }

    public static string Activo(string root)
    {
        try {
            string f = ActivoFile(root);
            if (!File.Exists(f)) return null;
            string s = File.ReadAllText(f, Encoding.UTF8).Trim('\r', '\n', ' ', '﻿');
            return s.Length > 0 ? s : null;
        } catch { return null; }
    }

    public static void SetActivo(string root, string nombre)
    {
        try {
            Directory.CreateDirectory(Path.GetDirectoryName(ActivoFile(root)));
            File.WriteAllText(ActivoFile(root), nombre + "\r\n", new UTF8Encoding(false));
        } catch { }
    }

    // the profile in use, made if missing
    public static string Asegurar(string root)
    {
        Migrar(root);
        string a = Activo(root);
        var l = Lista(root);
        if (a == null || !l.Contains(a, StringComparer.OrdinalIgnoreCase)) {
            a = l.Count > 0 ? l[0] : Limpiar(a ?? "Jugador 1");
            SetActivo(root, a);
        }
        try { Directory.CreateDirectory(Path.Combine(Dir(root), a)); } catch { }
        return a;
    }

    // "2 ranuras con datos · 48 duelos"
    public static string Resumen(string root, string nombre)
    {
        int ran = 0, duelos = 0;
        try {
            foreach (var d in Directory.GetDirectories(Path.Combine(Dir(root), nombre), "Ranura *")) {
                if (Directory.EnumerateFiles(d).Any()) ran++;
                string f = Path.Combine(d, "duelos.txt");
                if (File.Exists(f)) duelos += File.ReadAllLines(f).Count(x => x.Trim().Length > 0);
            }
        } catch { }
        return string.Format(Tr.S("perf_resumen"), "Partidas\\" + nombre, ran, duelos);
    }

    // the "new profile" window; the name, or null
    public static string PedirNombre(IWin32Window owner, string root, Color bg, Color panel, Color paper, Color muted, Color accent)
    {
        var f = new Form {
            Text = Tr.S("perf_nuevo_t"), FormBorderStyle = FormBorderStyle.FixedDialog, MaximizeBox = false,
            MinimizeBox = false, StartPosition = FormStartPosition.CenterParent, BackColor = panel, ForeColor = paper,
            Font = new Font("Segoe UI", 10f), ClientSize = new Size(380, 178), ShowInTaskbar = false,
            AutoScaleMode = AutoScaleMode.Dpi };
        var t = new Label { Text = Tr.S("perf_nuevo_t").ToUpper(), Font = new Font("Segoe UI", 9f, FontStyle.Bold),
                            ForeColor = accent, AutoSize = true, Location = new Point(14, 12) };
        var l = new Label { Text = Tr.S("perf_nombre"), AutoSize = true, Location = new Point(14, 40) };
        var box = new TextBox { Location = new Point(14, 62), Width = 352, BackColor = bg, ForeColor = paper,
                                BorderStyle = BorderStyle.FixedSingle, MaxLength = 32 };
        var nota = new Label { Text = Tr.S("perf_nuevo_nota"), ForeColor = muted, AutoSize = false,
                               Location = new Point(14, 92), Size = new Size(352, 36) };
        var ok = new Button { Text = Tr.S("perf_crear"), Location = new Point(176, 134), Size = new Size(92, 32),
                              FlatStyle = FlatStyle.Flat, BackColor = accent, ForeColor = bg };
        var no = new Button { Text = Tr.S("perf_cancelar"), Location = new Point(274, 134), Size = new Size(92, 32),
                              FlatStyle = FlatStyle.Flat, BackColor = panel, ForeColor = paper, DialogResult = DialogResult.Cancel };
        ok.FlatAppearance.BorderColor = accent;
        no.FlatAppearance.BorderColor = muted;
        string res = null;
        ok.Click += delegate {
            string n = Limpiar(box.Text);
            if (box.Text.Trim().Length == 0) { nota.ForeColor = Color.FromArgb(230, 90, 70); nota.Text = Tr.S("perf_err_vacio"); return; }
            if (Lista(root).Contains(n, StringComparer.OrdinalIgnoreCase)) {
                nota.ForeColor = Color.FromArgb(230, 90, 70); nota.Text = Tr.S("perf_err_existe"); return;
            }
            try { Directory.CreateDirectory(Path.Combine(Dir(root), n)); } catch (Exception e) {
                nota.ForeColor = Color.FromArgb(230, 90, 70); nota.Text = e.Message; return;
            }
            res = n;
            f.DialogResult = DialogResult.OK;
        };
        box.TextChanged += delegate { nota.ForeColor = muted; nota.Text = Tr.S("perf_nuevo_nota"); };
        f.Controls.AddRange(new Control[] { t, l, box, nota, ok, no });
        f.AcceptButton = ok;
        f.CancelButton = no;
        return f.ShowDialog(owner) == DialogResult.OK ? res : null;
    }
}
