// Respaldos.cs - 2026-11-25 (the user: mock-ups approved, "Sí, armalas"):
//   1. Respaldos: the profile's last saves (Partidas\<perfil>\respaldos\savedata_*.dat,
//      written by the game's guardado.c before each save after a win) and putting one
//      back as the profile's savedata.dat; the save in use is copied there first.
//   2. Informe de error: the game leaves Ajustes\ultimo_error.txt when it closes on an
//      error (perfil_dir.c ygo_marcar_error, from fatal.c and main.c's crash filter);
//      Configuración shows a notice and builds a zip on the Desktop with the logs,
//      the settings and the version (never the save).
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Windows.Forms;

static class Respaldos
{
    const string SaveRel = @"UDATA\4b4e001e\0061006D1822\savedata.dat";

    static string Save(string root, string perfil) { return Path.Combine(Perfiles.Dir(root), perfil, SaveRel); }
    static string Carpeta(string root, string perfil) { return Path.Combine(Perfiles.Dir(root), perfil, "respaldos"); }

    static string Fecha(DateTime t)
    {
        if (t.Date == DateTime.Today) return Tr.S("resp_hoy") + " " + t.ToString("HH:mm");
        if (t.Date == DateTime.Today.AddDays(-1)) return Tr.S("resp_ayer") + " " + t.ToString("HH:mm");
        return t.ToString("dd/MM/yyyy HH:mm");
    }

    static bool JuegoAbierto()
    {
        return Process.GetProcessesByName("Yu-Gi-Oh").Length > 0;
    }

    public static void Mostrar(IWin32Window owner, string root, string perfil, Color bg, Color panel, Color paper,
                               Color muted, Color accent, Color ok, Color bad)
    {
        var f = new Form {
            Text = string.Format(Tr.S("resp_t"), perfil), FormBorderStyle = FormBorderStyle.FixedDialog,
            MaximizeBox = false, MinimizeBox = false, StartPosition = FormStartPosition.CenterParent,
            BackColor = panel, ForeColor = paper, Font = new Font("Segoe UI", 10f), ClientSize = new Size(460, 330),
            ShowInTaskbar = false, AutoScaleMode = AutoScaleMode.Dpi };
        var t = new Label { Text = string.Format(Tr.S("resp_t"), perfil).ToUpper(), ForeColor = accent, AutoSize = true,
                            Font = new Font("Segoe UI", 9f, FontStyle.Bold), Location = new Point(14, 12) };
        var nota = new Label { Text = Tr.S("resp_nota"), ForeColor = muted, AutoSize = false,
                               Location = new Point(14, 36), Size = new Size(432, 36) };
        var lista = new ListBox { Location = new Point(14, 76), Size = new Size(432, 150), BackColor = bg, ForeColor = paper,
                                  BorderStyle = BorderStyle.FixedSingle, IntegralHeight = false };
        var nota2 = new Label { Text = Tr.S("resp_nota2"), ForeColor = muted, AutoSize = false,
                                Location = new Point(14, 232), Size = new Size(432, 40) };
        var bOk = new Button { Text = Tr.S("resp_restaurar"), Location = new Point(250, 282), Size = new Size(96, 34),
                               FlatStyle = FlatStyle.Flat, BackColor = accent, ForeColor = bg };
        var bNo = new Button { Text = Tr.S("resp_cerrar"), Location = new Point(352, 282), Size = new Size(94, 34),
                               FlatStyle = FlatStyle.Flat, BackColor = panel, ForeColor = paper, DialogResult = DialogResult.Cancel };
        bOk.FlatAppearance.BorderColor = accent;
        bNo.FlatAppearance.BorderColor = muted;
        var archivos = new List<string>();
        Action cargar = delegate {
            lista.Items.Clear();
            archivos.Clear();
            try {
                foreach (var a in Directory.GetFiles(Carpeta(root, perfil), "savedata_*.dat")
                                          .OrderByDescending(x => File.GetLastWriteTime(x))) {
                    archivos.Add(a);
                    lista.Items.Add(Fecha(File.GetLastWriteTime(a)));
                }
            } catch { }
            if (archivos.Count == 0) { lista.Items.Add(Tr.S("resp_vacio")); lista.Enabled = false; bOk.Enabled = false; }
            else lista.SelectedIndex = 0;
        };
        cargar();
        bOk.Click += delegate {
            int i = lista.SelectedIndex;
            if (i < 0 || i >= archivos.Count) return;
            string save = Save(root, perfil);
            if (JuegoAbierto()) { nota2.ForeColor = bad; nota2.Text = Tr.S("resp_juego"); return; }
            if (!File.Exists(save)) { nota2.ForeColor = bad; nota2.Text = Tr.S("resp_sinpartida"); return; }
            string cuando = (string)lista.Items[i];
            if (MessageBox.Show(f, string.Format(Tr.S("resp_conf"), cuando), f.Text, MessageBoxButtons.YesNo,
                                MessageBoxIcon.Question) != DialogResult.Yes) return;
            try {
                // the save in use first, as one more backup (the game keeps the newest 5)
                string dir = Carpeta(root, perfil);
                Directory.CreateDirectory(dir);
                string copia = Path.Combine(dir, "savedata_" + DateTime.Now.ToString("yyyyMMdd_HHmmss") + ".dat");
                for (int n = 2; File.Exists(copia); n++)
                    copia = Path.Combine(dir, "savedata_" + DateTime.Now.ToString("yyyyMMdd_HHmmss") + "_" + n + ".dat");
                string elegido = archivos[i];
                File.Copy(save, copia);
                File.Copy(elegido, save, true);
                nota2.ForeColor = ok;
                nota2.Text = string.Format(Tr.S("resp_ok"), cuando);
                cargar();
            } catch (Exception e) {
                nota2.ForeColor = bad;
                nota2.Text = e.Message;
            }
        };
        f.Controls.AddRange(new Control[] { t, nota, lista, nota2, bOk, bNo });
        f.CancelButton = bNo;
        f.ShowDialog(owner);
    }

    // ---------- Informe de error ----------

    public static string MarcaError(string ajustes) { return Path.Combine(ajustes, "ultimo_error.txt"); }

    // "2026-11-25 05:12" from the mark, or null when there is none
    public static string ErrorPendiente(string ajustes)
    {
        try {
            string m = MarcaError(ajustes);
            if (!File.Exists(m)) return null;
            foreach (var l in File.ReadAllLines(m))
                if (l.StartsWith("fecha=")) {
                    DateTime t;
                    if (DateTime.TryParse(l.Substring(6), out t)) return Fecha(t);
                    return l.Substring(6);
                }
            return "?";
        } catch { return null; }
    }

    public static void Descartar(string ajustes)
    {
        try { File.Delete(MarcaError(ajustes)); } catch { }
    }

    // the zip on the Desktop; its path (throws on failure)
    public static string ArmarInforme(string root, string ajustes)
    {
        string escritorio = Environment.GetFolderPath(Environment.SpecialFolder.DesktopDirectory);
        if (string.IsNullOrEmpty(escritorio) || !Directory.Exists(escritorio)) escritorio = root;
        string zip = Path.Combine(escritorio, "Informe Yu-Gi-Oh " + DateTime.Now.ToString("yyyy-MM-dd HH-mm") + ".zip");
        if (File.Exists(zip)) File.Delete(zip);
        var archivos = new List<string> {
            Path.Combine(root, "registro.txt"), Path.Combine(root, "registro_anterior.txt"),
            Path.Combine(root, "xbox_kernel.log"), Path.Combine(root, "escalado_log.txt"),
            Path.Combine(root, "version.txt"), Path.Combine(root, "VERSION"),
            Path.Combine(ajustes, "ygo_menu.ini"), Path.Combine(ajustes, "launcher.ini"),
            Path.Combine(ajustes, "perfil_activo.txt"), MarcaError(ajustes) };
        try { archivos.AddRange(Directory.GetFiles(root, "*.dmp")); } catch { }
        using (var z = ZipFile.Open(zip, ZipArchiveMode.Create)) {
            foreach (var a in archivos.Distinct()) {
                if (!File.Exists(a)) continue;
                // the game may still have its log open: read it shared
                using (var src = new FileStream(a, FileMode.Open, FileAccess.Read, FileShare.ReadWrite | FileShare.Delete))
                using (var dst = z.CreateEntry(Path.GetFileName(a), CompressionLevel.Optimal).Open())
                    src.CopyTo(dst);
            }
        }
        Descartar(ajustes);
        try { Process.Start("explorer.exe", "/select,\"" + zip + "\""); } catch { }
        return zip;
    }
}
