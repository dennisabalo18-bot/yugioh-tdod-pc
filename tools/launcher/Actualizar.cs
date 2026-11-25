// Buscar actualizaciones: the latest Release on GitHub against version.txt
// (next to the exe, written by tools\armar_paquete.ps1 from VERSION).
//
// Taken from Red Dead Revolver PC's launcher (C:\rdr-port\tools\launcher).
// The Release's name carries the version ("Yu-Gi-Oh TDOD PC v1.1"; the tag
// is the fallback). When it is newer, the zip is downloaded to %TEMP%,
// unpacked, and a small PowerShell script copies it over the folder once
// this window has closed (an exe cannot overwrite itself while it runs),
// then opens Configuración again. What the player made stays: Ajustes\ and
// Partidas\ are never in the zip and are skipped anyway, ygo_menu.ini and
// launcher.ini too, and nothing already under Game\ or Mods-Extras\ is
// replaced (the zip only brings the notes there).
// Repo empty: the button says the link is not set yet.
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Net;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading;
using System.Web.Script.Serialization;
using System.Windows.Forms;

static class Actualizar
{
    const string Repo = "dennisabalo18-bot/yugioh-tdod-pc";   // 2026-11-25

    static readonly Dictionary<string, string[]> T = new Dictionary<string, string[]> {
        { "button", new[] { "Buscar actualizaciones", "Check for updates", "Procurar atualizações",
            "Rechercher des mises à jour", "Nach Updates suchen", "Cerca aggiornamenti" } },
        { "button.tip", new[] {
            "Busca una versión nueva en GitHub y la instala. Tus ajustes, partidas, juego, texturas y voces no se tocan.",
            "Looks for a new version on GitHub and installs it. Your settings, saves, game, textures and voices are kept.",
            "Procura uma versão nova no GitHub e a instala. Suas configurações, jogos salvos, jogo, texturas e vozes ficam como estão.",
            "Cherche une nouvelle version sur GitHub et l'installe. Vos réglages, sauvegardes, jeu, textures et voix sont conservés.",
            "Sucht auf GitHub nach einer neuen Version und installiert sie. Einstellungen, Spielstände, Spiel, Texturen und Stimmen bleiben erhalten.",
            "Cerca una nuova versione su GitHub e la installa. Impostazioni, salvataggi, gioco, texture e voci restano invariati." } },
        { "no_repo", new[] { "Las actualizaciones todavía no están disponibles.", "Updates are not available yet.",
            "As atualizações ainda não estão disponíveis.", "Les mises à jour ne sont pas encore disponibles.",
            "Updates sind noch nicht verfügbar.", "Gli aggiornamenti non sono ancora disponibili." } },
        { "version", new[] { "Versión {0}", "Version {0}", "Versão {0}", "Version {0}", "Version {0}", "Versione {0}" } },
        { "checking", new[] { "Buscando actualizaciones…", "Checking for updates…", "Procurando atualizações…",
            "Recherche de mises à jour…", "Suche nach Updates…", "Ricerca aggiornamenti…" } },
        { "latest", new[] { "Ya tenés la última versión ({0}).", "You have the latest version ({0}).",
            "Você já tem a versão mais recente ({0}).", "Vous avez déjà la dernière version ({0}).",
            "Du hast bereits die neueste Version ({0}).", "Hai già l'ultima versione ({0})." } },
        { "new", new[] {
            "Hay una versión nueva: {0} (tenés {1}).\n\n¿Descargarla e instalarla? Tus ajustes, partidas, juego, texturas y voces se mantienen. Configuración se cerrará y volverá a abrirse.",
            "A new version is out: {0} (you have {1}).\n\nDownload and install it? Your settings, saves, game, textures and voices are kept. Settings will close and open again.",
            "Há uma versão nova: {0} (você tem {1}).\n\nBaixar e instalar? Suas configurações, jogos salvos, jogo, texturas e vozes são mantidos. As Configurações vão fechar e abrir de novo.",
            "Une nouvelle version est sortie : {0} (vous avez {1}).\n\nLa télécharger et l'installer ? Vos réglages, sauvegardes, jeu, textures et voix sont conservés. Les paramètres vont se fermer puis se rouvrir.",
            "Eine neue Version ist da: {0} (du hast {1}).\n\nHerunterladen und installieren? Einstellungen, Spielstände, Spiel, Texturen und Stimmen bleiben erhalten. Die Einstellungen schließen sich und öffnen sich wieder.",
            "È uscita una nuova versione: {0} (hai la {1}).\n\nScaricarla e installarla? Impostazioni, salvataggi, gioco, texture e voci restano. Le impostazioni si chiuderanno e si riapriranno." } },
        { "downloading", new[] { "Descargando {0}… {1}%", "Downloading {0}… {1}%", "Baixando {0}… {1}%",
            "Téléchargement de {0}… {1} %", "Lade {0} herunter… {1} %", "Download di {0}… {1}%" } },
        { "unpacking", new[] { "Preparando la actualización…", "Preparing the update…", "Preparando a atualização…",
            "Préparation de la mise à jour…", "Update wird vorbereitet…", "Preparazione dell'aggiornamento…" } },
        { "close_game", new[] { "Cerrá el juego antes de actualizar.", "Close the game before updating.",
            "Feche o jogo antes de atualizar.", "Fermez le jeu avant la mise à jour.",
            "Schließe das Spiel vor dem Update.", "Chiudi il gioco prima di aggiornare." } },
        { "fail", new[] { "No se pudo buscar o descargar la actualización: {0}", "Could not check or download the update: {0}",
            "Não foi possível procurar ou baixar a atualização: {0}", "Impossible de rechercher ou télécharger la mise à jour : {0}",
            "Update konnte nicht gesucht oder geladen werden: {0}", "Impossibile cercare o scaricare l'aggiornamento: {0}" } },
        { "updated", new[] { "Actualizado a la versión {0}. Tus ajustes se mantuvieron.", "Updated to version {0}. Your settings were kept.",
            "Atualizado para a versão {0}. Suas configurações foram mantidas.", "Mis à jour vers la version {0}. Vos réglages ont été conservés.",
            "Auf Version {0} aktualisiert. Deine Einstellungen wurden behalten.", "Aggiornato alla versione {0}. Le impostazioni sono state mantenute." } },
    };

    public static string S(string id) { string[] t; return T.TryGetValue(id, out t) ? t[Tr.Lang] : id; }

    // version.txt next to the exe: "1.4". "?" when there is none.
    public static string Local(string root)
    {
        try {
            var f = Path.Combine(root, "version.txt");
            if (File.Exists(f)) {
                var v = File.ReadAllText(f).Trim();
                if (v.Length > 0) return v;
            }
        } catch { }
        return "?";
    }

    // "Yu-Gi-Oh TDOD PC v1.1" -> 1.1; null when it names none.
    static Version Parse(string s)
    {
        if (string.IsNullOrEmpty(s)) return null;
        Version best = null;
        foreach (Match m in Regex.Matches(s, @"\d+(\.\d+)+|\d+")) {
            Version v;
            var t = m.Value.Contains(".") ? m.Value : m.Value + ".0";
            if (Version.TryParse(t, out v)) best = v;
        }
        return best;
    }

    static int Cmp(Version a, Version b)
    {
        Func<int, int> z = x => x < 0 ? 0 : x;
        int[] x1 = { a.Major, a.Minor, z(a.Build), z(a.Revision) }, x2 = { b.Major, b.Minor, z(b.Build), z(b.Revision) };
        for (int i = 0; i < 4; i++) if (x1[i] != x2[i]) return x1[i].CompareTo(x2[i]);
        return 0;
    }

    static WebClient Client()
    {
        ServicePointManager.SecurityProtocol |= (SecurityProtocolType)3072;   // TLS 1.2
        var w = new WebClient();
        w.Headers[HttpRequestHeader.UserAgent] = "YuGiOhTDODPC-Configuracion";
        return w;
    }

    static bool GameRunning()
    {
        return Process.GetProcessesByName("Yu-Gi-Oh").Length > 0;
    }

    // The latest Release: its version (name), its zip (url, size), url null
    // when it is not newer than have. Returns an error text, or null.
    public static string Check(string have, out string name, out string url, out long size)
    {
        string err = null;
        name = null; url = null; size = 0;
        try {
            string json;
            using (var w = Client())
                json = w.DownloadString("https://api.github.com/repos/" + Repo + "/releases/latest");
            var r = (Dictionary<string, object>)new JavaScriptSerializer().DeserializeObject(json);
            name = r.ContainsKey("name") ? r["name"] as string : null;
            var tag = r.ContainsKey("tag_name") ? r["tag_name"] as string : null;
            var vr = Parse(name) ?? Parse(tag);
            name = vr != null ? vr.ToString() : (tag ?? "?");
            foreach (var o in (object[])r["assets"]) {
                var a = (Dictionary<string, object>)o;
                var an = a["name"] as string;
                if (an != null && an.EndsWith(".zip", StringComparison.OrdinalIgnoreCase)) {
                    url = a["browser_download_url"] as string;
                    size = Convert.ToInt64(a["size"]);
                    break;
                }
            }
            var vh = Parse(have);
            if (vr == null || url == null) err = "release";
            else if (vh != null && Cmp(vr, vh) <= 0) url = null;      // not newer
        } catch (Exception e) { err = e.Message; }
        return err;
    }

    // The button's click: everything off the UI thread, results back on it.
    public static void Run(Form owner, string root, Button btn, Label msg)
    {
        if (Repo.Length == 0) { msg.Text = S("no_repo"); return; }
        btn.Enabled = false;
        msg.Text = S("checking");
        string have = Local(root);
        new Thread(() => {
            string name, url;
            long size;
            string err = Check(have, out name, out url, out size);
            owner.BeginInvoke((MethodInvoker)delegate {
                if (err != null) { msg.Text = string.Format(S("fail"), err); btn.Enabled = true; return; }
                if (url == null) { msg.Text = string.Format(S("latest"), have); btn.Enabled = true; return; }
                if (MessageBox.Show(owner, string.Format(S("new"), name, have), S("button"),
                                    MessageBoxButtons.YesNo, MessageBoxIcon.Information) != DialogResult.Yes) {
                    msg.Text = ""; btn.Enabled = true; return;
                }
                if (GameRunning()) {
                    MessageBox.Show(owner, S("close_game"), S("button"), MessageBoxButtons.OK, MessageBoxIcon.Warning);
                    btn.Enabled = true; return;
                }
                Download(owner, root, btn, msg, url, size, name);
            });
        }) { IsBackground = true }.Start();
    }

    static void Download(Form owner, string root, Button btn, Label msg, string url, long size, string name)
    {
        string tmp = Path.Combine(Path.GetTempPath(), "ygo_actualizacion");
        string zip = Path.Combine(tmp, "YuGiOh-TDOD-PC.zip");
        try {
            if (Directory.Exists(tmp)) Directory.Delete(tmp, true);
            Directory.CreateDirectory(tmp);
        } catch (Exception e) { msg.Text = string.Format(S("fail"), e.Message); btn.Enabled = true; return; }
        var w = Client();
        w.DownloadProgressChanged += (s, e) => msg.Text = string.Format(S("downloading"), name, e.ProgressPercentage);
        w.DownloadFileCompleted += (s, e) => {
            w.Dispose();
            if (e.Error != null || e.Cancelled) {
                msg.Text = string.Format(S("fail"), e.Error != null ? e.Error.Message : "cancel");
                btn.Enabled = true; return;
            }
            msg.Text = S("unpacking");
            new Thread(() => {
                string err = null, src = null;
                try {
                    if (size > 0 && new FileInfo(zip).Length != size) throw new Exception("size");
                    string x = Path.Combine(tmp, "x");
                    ZipFile.ExtractToDirectory(zip, x);
                    src = Directory.GetFiles(x, "Yu-Gi-Oh.exe", SearchOption.AllDirectories)
                                   .Select(Path.GetDirectoryName).FirstOrDefault();
                    if (src == null) throw new Exception("Yu-Gi-Oh.exe");
                } catch (Exception ex) { err = ex.Message; }
                owner.BeginInvoke((MethodInvoker)delegate {
                    if (err != null) { msg.Text = string.Format(S("fail"), err); btn.Enabled = true; return; }
                    if (GameRunning()) {
                        MessageBox.Show(owner, S("close_game"), S("button"), MessageBoxButtons.OK, MessageBoxIcon.Warning);
                        btn.Enabled = true; return;
                    }
                    try { Install(tmp, src, root, name); }
                    catch (Exception ex) { msg.Text = string.Format(S("fail"), ex.Message); btn.Enabled = true; return; }
                    Application.Exit();
                });
            }) { IsBackground = true }.Start();
        };
        w.DownloadFileAsync(new Uri(url), zip);
    }

    // Writes and starts the script that copies src over root after this
    // process has exited, then opens the launcher again with --actualizado.
    static void Install(string tmp, string src, string root, string name)
    {
        string ps = Path.Combine(tmp, "instalar.ps1");
        string script = @"param([int]$Pid0, [string]$Src, [string]$Dst, [string]$Exe, [string]$Ver)
$log = Join-Path $Dst 'actualizacion.log'
try { Wait-Process -Id $Pid0 -Timeout 30 -ErrorAction SilentlyContinue } catch {}
for ($i = 0; $i -lt 120 -and (Get-Process Yu-Gi-Oh -ErrorAction SilentlyContinue); $i++) { Start-Sleep -Milliseconds 500 }
$keep = @('ygo_menu.ini', 'launcher.ini')
$n = 0
Get-ChildItem -LiteralPath $Src -Recurse -File | ForEach-Object {
    $rel = $_.FullName.Substring($Src.Length).TrimStart('\')
    $to = Join-Path $Dst $rel
    if ($keep -contains $_.Name) { return }
    if (($rel -like 'Game\*' -or $rel -like 'Mods-Extras\*' -or $rel -like 'Ajustes\*' -or $rel -like 'Partidas\*') -and (Test-Path -LiteralPath $to)) { return }
    New-Item -ItemType Directory -Force -Path (Split-Path $to) | Out-Null
    for ($t = 0; $t -lt 20; $t++) {
        try { Copy-Item -LiteralPath $_.FullName -Destination $to -Force -ErrorAction Stop; $n++; break }
        catch { Start-Sleep -Milliseconds 500 }
    }
}
""$(Get-Date -Format s) version $Ver, $n files"" | Out-File -LiteralPath $log -Append -Encoding utf8
Start-Process -FilePath $Exe -ArgumentList '--actualizado' -WorkingDirectory $Dst
";
        File.WriteAllText(ps, script, new UTF8Encoding(true));
        var psi = new ProcessStartInfo("powershell.exe",
            "-NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File \"" + ps + "\" -Pid0 " +
            Process.GetCurrentProcess().Id + " -Src \"" + src + "\" -Dst \"" + root + "\" -Exe \"" +
            Application.ExecutablePath + "\" -Ver \"" + name + "\"") {
            UseShellExecute = false, CreateNoWindow = true };
        Process.Start(psi);
    }
}
