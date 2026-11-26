// Configuración.exe: the settings of the Yu-Gi-Oh! The Dawn of Destiny port,
// outside the game.
//
// Sits in the port's folder (Yu-Gi-Oh Port\). Edits Ajustes\ygo_menu.ini, the
// same file and values as the in-game F2 menu:
// - every change here is saved at once, and the game, if it is running,
//   takes it within half a second (host_menu.c xbox_MenuPoll);
// - every change made in F2 shows here at once (a FileSystemWatcher on the
//   file).
// Ajustes\ is the port's own folder, like Partidas\: a new version of the
// port (a zip from GitHub) does not bring one, so the settings stay.
//
// The window speaks Spanish, English, French, German and Italian (Portuguese texts kept, not offered)
// (selector at the top right; Ajustes\launcher.ini remembers it, and F2's
// panel follows it). ygo_menu.ini keeps the F2 menu's own values (Spanish):
// only what is shown is translated.
//
// Built by tools\launcher\compilar.ps1 with the C# compiler that comes with
// Windows (.NET Framework 4, C# 5): nothing to install.
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Windows.Forms;
using System.Reflection;

[assembly: AssemblyTitle("Configuración - Yu-Gi-Oh! The Dawn of Destiny")]
[assembly: AssemblyDescription("Ajustes del port de PC de Yu-Gi-Oh! The Dawn of Destiny (los mismos del menú F2)")]
[assembly: AssemblyProduct("Yu-Gi-Oh! The Dawn of Destiny - Port de PC")]
[assembly: AssemblyCompany("Yu-Gi-Oh Port")]
[assembly: AssemblyCopyright("Port de fans, sin fines de lucro")]
[assembly: AssemblyVersion("1.0.0.0")]
[assembly: AssemblyFileVersion("1.0.0.0")]

static class Program
{
    [DllImport("user32.dll")] static extern bool SetProcessDPIAware();

    [STAThread]
    static void Main()
    {
        try { SetProcessDPIAware(); } catch { }
        Application.EnableVisualStyles();
        Application.SetCompatibleTextRenderingDefault(false);
        Acomodar(AppDomain.CurrentDomain.BaseDirectory.TrimEnd('\\'));
        Application.Run(new Launcher());
    }

    // 2026-11-25: the Drive zips unpacked with Windows' "Extraer todo" land in
    // a folder named after the zip (root\YuGiOh-Contenido\Mods-Extras\...), or
    // in Mods-Extras\Mods-Extras\...: merged into root\Mods-Extras, like the
    // game does (mods_carpeta.c ygo_mods_acomodar).
    static void Acomodar(string root)
    {
        try {
            string mods = Path.Combine(root, "Mods-Extras");
            var halla = new List<string>();
            Buscar(root, mods, 0, halla);
            foreach (var h in halla) {
                Juntar(h, mods);
                for (var p = h; p.Length > root.Length && !string.Equals(p, mods, StringComparison.OrdinalIgnoreCase);
                     p = Path.GetDirectoryName(p)) {
                    if (Directory.EnumerateFileSystemEntries(p).Any()) break;
                    Directory.Delete(p);
                }
            }
        } catch { }
    }

    static void Buscar(string dir, string mods, int prof, List<string> halla)
    {
        foreach (var d in Directory.GetDirectories(dir)) {
            var n = Path.GetFileName(d);
            if (prof == 0 && (n.Equals("Game", StringComparison.OrdinalIgnoreCase) || n.Equals("Partidas", StringComparison.OrdinalIgnoreCase)
                              || n.Equals("Ajustes", StringComparison.OrdinalIgnoreCase)))
                continue;
            if (n.Equals("Mods-Extras", StringComparison.OrdinalIgnoreCase) && !string.Equals(d, mods, StringComparison.OrdinalIgnoreCase))
                halla.Add(d);
            else if (prof < 2)
                Buscar(d, mods, prof + 1, halla);
        }
    }

    static void Juntar(string src, string dst)
    {
        Directory.CreateDirectory(dst);
        foreach (var f in Directory.GetFiles(src)) {
            var to = Path.Combine(dst, Path.GetFileName(f));
            try { if (File.Exists(to)) File.Delete(to); File.Move(f, to); } catch { }
        }
        foreach (var d in Directory.GetDirectories(src)) {
            Juntar(d, Path.Combine(dst, Path.GetFileName(d)));
            try { if (!Directory.EnumerateFileSystemEntries(d).Any()) Directory.Delete(d); } catch { }
        }
    }
}

class Opt
{
    public string Key, Def;          // Def: one of Choices
    public string[] Choices;         // exactly as the F2 menu writes them in ygo_menu.ini
    public ComboBox Box;
    public Opt(string key, string def, params string[] choices)
    { Key = key; Def = def; Choices = choices; }
}

// ---------- texts: es, en, pt, fr, de, it ----------
static class Tr
{
    public static readonly string[] Codes = { "es", "en", "pt", "fr", "de", "it" };
    public static readonly string[] Names = { "Español", "English", "Português", "Français", "Deutsch", "Italiano" };
    public static int Lang;

    static readonly Dictionary<string, string[]> T = new Dictionary<string, string[]> {
        { "title", new[] { "Yu-Gi-Oh! The Dawn of Destiny - Configuración", "Yu-Gi-Oh! The Dawn of Destiny - Settings",
            "Yu-Gi-Oh! The Dawn of Destiny - Configurações", "Yu-Gi-Oh! The Dawn of Destiny - Paramètres",
            "Yu-Gi-Oh! The Dawn of Destiny - Einstellungen", "Yu-Gi-Oh! The Dawn of Destiny - Impostazioni" } },
        { "subtitle", new[] {
            "Port de PC  ·  Los cambios se guardan solos y el juego los toma al instante (también los de F2).",
            "PC port  ·  Changes save themselves and the game takes them at once (F2's too).",
            "Port para PC  ·  As mudanças se salvam sozinhas e o jogo as aplica na hora (as do F2 também).",
            "Portage PC  ·  Les changements s'enregistrent seuls et le jeu les applique aussitôt (ceux de F2 aussi).",
            "PC-Portierung  ·  Änderungen speichern sich selbst, das Spiel übernimmt sie sofort (auch die aus F2).",
            "Port per PC  ·  Le modifiche si salvano da sole e il gioco le applica subito (anche quelle di F2)." } },
        { "lang", new[] { "Idioma:", "Language:", "Idioma:", "Langue :", "Sprache:", "Lingua:" } },
        { "g_status", new[] { "Estado", "Status", "Status", "État", "Status", "Stato" } },
        { "g_image", new[] { "Imagen", "Graphics", "Imagem", "Image", "Grafik", "Grafica" } },
        { "g_controls", new[] { "Controles y texturas", "Controls and textures", "Controles e texturas",
            "Commandes et textures", "Steuerung und Texturen", "Controlli e texture" } },
        { "g_game", new[] { "Juego", "Game", "Jogo", "Jeu", "Spiel", "Gioco" } },

        { "render_scale", new[] { "Resolución interna", "Internal resolution", "Resolução interna",
            "Résolution interne", "Interne Auflösung", "Risoluzione interna" } },
        { "render_scale.tip", new[] {
            "Cuántos píxeles dibuja el juego. Más alto = más nítido y más exigente.",
            "How many pixels the game draws. Higher = sharper and more demanding.",
            "Quantos pixels o jogo desenha. Mais alto = mais nítido e mais exigente.",
            "Nombre de pixels que le jeu dessine. Plus haut = plus net et plus exigeant.",
            "Wie viele Pixel das Spiel zeichnet. Höher = schärfer und anspruchsvoller.",
            "Quanti pixel disegna il gioco. Più alto = più nitido e più pesante." } },
        { "widescreen", new[] { "Panorámico", "Widescreen", "Widescreen", "Écran large", "Breitbild", "Widescreen" } },
        { "widescreen.tip", new[] {
            "Estira la imagen a 16:9 para llenar pantallas anchas.",
            "Stretches the picture to 16:9 to fill wide screens.",
            "Estica a imagem para 16:9 para preencher telas largas.",
            "Étire l'image en 16:9 pour remplir les écrans larges.",
            "Streckt das Bild auf 16:9 für breite Bildschirme.",
            "Allarga l'immagine a 16:9 per riempire gli schermi larghi." } },
        { "window", new[] { "Tamaño de ventana", "Window size", "Tamanho da janela",
            "Taille de la fenêtre", "Fenstergröße", "Dimensione finestra" } },
        { "window.tip", new[] {
            "Tamaño de la ventana cuando no está en pantalla completa.", "Window size when not in fullscreen.",
            "Tamanho da janela fora da tela cheia.", "Taille de la fenêtre hors du plein écran.",
            "Fenstergröße außerhalb des Vollbilds.", "Dimensione della finestra fuori dallo schermo intero." } },
        { "fullscreen", new[] { "Pantalla completa", "Fullscreen", "Tela cheia", "Plein écran", "Vollbild", "Schermo intero" } },
        { "fullscreen.tip", new[] {
            "El juego arranca así. En el juego, F11 cambia entre ventana y pantalla completa.",
            "The game starts this way. In game, F11 switches between window and fullscreen.",
            "O jogo abre assim. No jogo, F11 alterna entre janela e tela cheia.",
            "Le jeu démarre ainsi. En jeu, F11 bascule entre fenêtre et plein écran.",
            "So startet das Spiel. Im Spiel wechselt F11 zwischen Fenster und Vollbild.",
            "Il gioco parte così. Nel gioco, F11 passa tra finestra e schermo intero." } },
        { "aniso", new[] { "Filtrado anisotrópico", "Anisotropic filtering", "Filtragem anisotrópica",
            "Filtrage anisotrope", "Anisotrope Filterung", "Filtro anisotropico" } },
        { "aniso.tip", new[] {
            "Texturas más nítidas vistas de costado (el campo de duelo).",
            "Sharper textures seen at an angle (the duel field).",
            "Texturas mais nítidas vistas de lado (o campo de duelo).",
            "Textures plus nettes vues de biais (le terrain de duel).",
            "Schärfere Texturen in Schräglage (das Duellfeld).",
            "Texture più nitide viste di sbieco (il campo di duello)." } },
        { "vsync", new[] { "Sincronía vertical (VSync)", "Vertical sync (VSync)", "Sincronia vertical (VSync)",
            "Synchro verticale (VSync)", "Vertikale Synchr. (VSync)", "Sincronia verticale (VSync)" } },
        { "vsync.tip", new[] {
            "Evita el desgarro de la imagen.", "Prevents screen tearing.", "Evita que a imagem se rasgue.",
            "Évite le déchirement de l'image.", "Verhindert Bildreißen (Tearing).", "Evita lo strappo dell'immagine (tearing)." } },
        { "fps_juego", new[] { "Cuadros por segundo", "Frame rate", "Quadros por segundo", "Images par seconde", "Bilder pro Sekunde", "Fotogrammi al secondo" } },
        { "fps_juego.tip", new[] {
            "A cuántos cuadros por segundo corre el juego (en el juego también con F8). Se guarda.",
            "How many frames per second the game runs at (in game also with F8). It is saved.",
            "A quantos quadros por segundo o jogo roda (no jogo também com F8). Fica salvo.",
            "Le nombre d'images par seconde du jeu (en jeu aussi avec F8). Enregistré.",
            "Mit wie vielen Bildern pro Sekunde das Spiel läuft (im Spiel auch mit F8). Wird gespeichert.",
            "A quanti fotogrammi al secondo gira il gioco (in gioco anche con F8). Viene salvato." } },
        { "fps", new[] { "Mostrar FPS", "Show FPS", "Mostrar FPS", "Afficher les FPS", "FPS anzeigen", "Mostra FPS" } },
        { "fps.tip", new[] {
            "Los cuadros por segundo en una esquina.", "Frames per second in a corner.",
            "Os quadros por segundo num canto.", "Les images par seconde dans un coin.",
            "Bilder pro Sekunde in einer Ecke.", "I fotogrammi al secondo in un angolo." } },
        { "imagen_2d", new[] { "Imagen 2D", "2D picture", "Imagem 2D", "Image 2D", "2D-Bild", "Immagine 2D" } },
        { "imagen_2d.tip", new[] {
            "Cartas, menús y textos. Alta resolución es la más nítida; Original se ve como en la consola.",
            "Cards, menus and text. High resolution is the sharpest; Original looks as on the console.",
            "Cartas, menus e textos. Alta resolução é a mais nítida; Original fica como no console.",
            "Cartes, menus et textes. Haute résolution est la plus nette ; Original ressemble à la console.",
            "Karten, Menüs und Texte. Hohe Auflösung ist am schärfsten; Original wie auf der Konsole.",
            "Carte, menu e testi. Alta risoluzione è la più nitida; Originale come sulla console." } },

        { "pack_art", new[] { "Texturas mejoradas", "Improved textures", "Texturas melhoradas",
            "Textures améliorées", "Verbesserte Texturen", "Texture migliorate" } },
        { "pack_art.tip", new[] {
            "Cartas y menús redibujados en alta resolución. En el juego, F6 las prende y apaga.",
            "Cards and menus redrawn in high resolution. In game, F6 turns them on and off.",
            "Cartas e menus redesenhados em alta resolução. No jogo, F6 liga e desliga.",
            "Cartes et menus redessinés en haute résolution. En jeu, F6 les active ou les désactive.",
            "Karten und Menüs in hoher Auflösung neu gezeichnet. Im Spiel schaltet F6 sie ein und aus.",
            "Carte e menu ridisegnati in alta risoluzione. Nel gioco, F6 le attiva e disattiva." } },
        { "botones", new[] { "Botones en pantalla", "On-screen buttons", "Botões na tela",
            "Boutons à l'écran", "Tasten am Bildschirm", "Pulsanti a schermo" } },
        { "botones.tip", new[] {
            "Qué botones muestra el juego. Automático sigue al último mando o teclado que usaste.",
            "Which buttons the game shows. Automatic follows the last controller or keyboard you used.",
            "Quais botões o jogo mostra. Automático segue o último controle ou teclado que você usou.",
            "Les boutons affichés par le jeu. Automatique suit la dernière manette ou le clavier utilisé.",
            "Welche Tasten das Spiel zeigt. Automatisch folgt dem zuletzt benutzten Controller oder der Tastatur.",
            "Quali pulsanti mostra il gioco. Automatico segue l'ultimo controller o la tastiera che hai usato." } },
        { "raton_modo", new[] { "Modo del ratón", "Mouse mode", "Modo do mouse", "Mode de la souris", "Mausmodus", "Modalità mouse" } },
        { "raton_modo.tip", new[] {
            "Puntero: el ratón apunta como en un juego de PC. Cursor del juego: mueve el guante casilla por casilla. Solo clics: el ratón solo hace clic.",
            "Pointer: the mouse points as in a PC game. Game cursor: moves the glove square by square. Clicks only: the mouse only clicks.",
            "Ponteiro: o mouse aponta como num jogo de PC. Cursor do jogo: move a luva casa por casa. Só cliques: o mouse só clica.",
            "Pointeur : la souris pointe comme dans un jeu PC. Curseur du jeu : déplace le gant case par case. Clics seulement : la souris ne fait que cliquer.",
            "Zeiger: Die Maus zeigt wie in einem PC-Spiel. Spielcursor: bewegt den Handschuh Feld für Feld. Nur Klicks: Die Maus klickt nur.",
            "Puntatore: il mouse punta come in un gioco PC. Cursore del gioco: muove il guanto casella per casella. Solo clic: il mouse fa solo clic." } },
        { "raton_sensibilidad", new[] { "Sensibilidad del cursor", "Cursor sensitivity", "Sensibilidade do cursor",
            "Sensibilité du curseur", "Cursorempfindlichkeit", "Sensibilità del cursore" } },
        { "raton_sensibilidad.tip", new[] {
            "Cuánto hay que mover el ratón para pasar de casilla (modo Cursor del juego).",
            "How far the mouse moves to step one square (Game cursor mode).",
            "Quanto mover o mouse para passar de casa (modo Cursor do jogo).",
            "La distance de souris pour passer d'une case (mode Curseur du jeu).",
            "Wie weit die Maus für ein Feld bewegt wird (Modus Spielcursor).",
            "Quanto muovere il mouse per passare di casella (modalità Cursore del gioco)." } },
        { "raton_guante_imagen", new[] { "Modo grabación (OBS)", "Recording mode (OBS)",
            "Modo gravação (OBS)", "Mode enregistrement (OBS)",
            "Aufnahmemodus (OBS)", "Modalità registrazione (OBS)" } },
        { "raton_guante_imagen.tip", new[] {
            "Dibuja el guante y los paneles (Perfil, buscador, carteles, premios) dentro de la imagen del juego, así OBS los graba todos con la captura de juego o de ventana.",
            "Draws the glove and the panels (Profile, search, notices, rewards) inside the game's picture, so OBS records them all with game or window capture.",
            "Desenha a luva e os painéis (Perfil, busca, avisos, prêmios) dentro da imagem do jogo, assim o OBS grava tudo com a captura de jogo ou de janela.",
            "Dessine le gant et les panneaux (Profil, recherche, avis, récompenses) dans l'image du jeu : OBS enregistre tout en capture de jeu ou de fenêtre.",
            "Zeichnet den Handschuh und die Fenster (Profil, Suche, Hinweise, Preise) ins Spielbild, damit OBS alles per Spiel- oder Fensteraufnahme aufnimmt.",
            "Disegna il guanto e i pannelli (Profilo, ricerca, avvisi, premi) nell'immagine del gioco, così OBS registra tutto con la cattura di gioco o finestra." } },
        { "onda_clic", new[] { "Onda al hacer clic", "Ripple on click", "Onda ao clicar",
            "Onde au clic", "Welle beim Klicken", "Onda al clic" } },
        { "onda_clic.tip", new[] {
            "Un anillo que se abre desde la mano cada vez que hacés clic.",
            "A ring that spreads from the hand each time you click.",
            "Um anel que se abre da mão cada vez que você clica.",
            "Un anneau qui s'ouvre depuis la main à chaque clic.",
            "Ein Ring, der sich bei jedem Klick von der Hand ausbreitet.",
            "Un anello che si apre dalla mano a ogni clic." } },
        { "cartel_premios", new[] { "Cartel de premios del rival", "Opponent rewards sign", "Placa de prêmios do rival",
            "Panneau des récompenses", "Belohnungsanzeige des Gegners", "Cartello premi dell'avversario" } },
        { "cartel_premios.tip", new[] {
            "Al elegir oponente, un cartel dice cuántas cartas puede darte ese rival, cuántas te faltan y cuántas ya tenés.",
            "When choosing an opponent, a sign shows how many cards that opponent can give you, how many you are missing and how many you already have.",
            "Ao escolher o oponente, uma placa mostra quantas cartas esse rival pode dar, quantas faltam e quantas você já tem.",
            "En choisissant l'adversaire, un panneau indique combien de cartes il peut vous donner, combien il vous manque et combien vous avez déjà.",
            "Bei der Gegnerwahl zeigt eine Anzeige, wie viele Karten dieser Gegner geben kann, wie viele dir fehlen und wie viele du schon hast.",
            "Scegliendo l'avversario, un cartello indica quante carte può darti, quante te ne mancano e quante ne hai già." } },
        { "ayudas", new[] { "Ayudas", "Hints", "Ajudas", "Aides", "Hilfen", "Aiuti" } },
        { "ayudas.tip", new[] {
            "Los carteles de ayuda: premios de cada rival, del Duelo Triple y de cada dios, y quién da cada carta en la Biblioteca (con fotos o sin). No los apaga todos.",
            "The help signs: each opponent's rewards, the Triple Duel's and each god's, and who gives each card in the Library (with or without photos). No turns them all off.",
            "As placas de ajuda: prêmios de cada rival, do Duelo Triplo e de cada deus, e quem dá cada carta na Biblioteca (com ou sem fotos). Não desliga todas.",
            "Les panneaux d'aide : récompenses de chaque adversaire, du Duel Triple et de chaque dieu, et qui donne chaque carte dans la Bibliothèque (avec ou sans photos). Non les éteint tous.",
            "Die Hilfeanzeigen: Belohnungen jedes Gegners, des Dreifach-Duells und jedes Gottes, und wer jede Karte in der Bibliothek gibt (mit oder ohne Fotos). Nein schaltet alle aus.",
            "I cartelli di aiuto: premi di ogni avversario, del Duello Triplo e di ogni dio, e chi dà ogni carta nella Biblioteca (con o senza foto). No li spegne tutti." } },
        { "premios_sin_cuarta", new[] { "Premios sin cuarta copia", "No 4th copy rewards", "Prêmios sem 4ª cópia",
            "Récompenses sans 4e exemplaire", "Belohnungen ohne 4. Kopie", "Premi senza 4ª copia" } },
        { "premios_sin_cuarta.tip", new[] {
            "Al ganar, el rival no te da una carta de la que ya tenés 3, salvo que no le quede otra para darte.",
            "When you win, the opponent doesn't give you a card you already have 3 of, unless it has no other card to give.",
            "Ao vencer, o rival não dá uma carta da qual você já tem 3, a menos que não reste outra.",
            "Quand vous gagnez, l'adversaire ne vous donne pas une carte que vous avez déjà en 3 exemplaires, sauf s'il n'en a pas d'autre.",
            "Wenn du gewinnst, gibt dir der Gegner keine Karte, von der du schon 3 hast, außer er hat keine andere.",
            "Quando vinci, l'avversario non ti dà una carta di cui hai già 3 copie, a meno che non ne abbia altre." } },
        { "aviso_nueva", new[] { "Aviso de carta nueva", "New card notice", "Aviso de carta nova",
            "Avis de nouvelle carte", "Hinweis auf neue Karte", "Avviso carta nuova" } },
        { "aviso_nueva.tip", new[] {
            "Cuando ganás una carta que no tenías, un cartel avisa \"¡Carta nueva!\" con su nombre.",
            "When you win a card you didn't have, a sign says \"New card!\" with its name.",
            "Quando você ganha uma carta que não tinha, uma placa avisa \"Carta nova!\" com o nome.",
            "Quand vous gagnez une carte que vous n'aviez pas, un panneau affiche \"Nouvelle carte !\" avec son nom.",
            "Wenn du eine Karte gewinnst, die du noch nicht hattest, zeigt eine Anzeige \"Neue Karte!\" mit ihrem Namen.",
            "Quando vinci una carta che non avevi, un cartello avvisa \"Carta nuova!\" con il suo nome." } },
        { "mascotas", new[] { "Mascotas en el duelo", "Duel mascots", "Mascotes no duelo",
            "Mascottes en duel", "Maskottchen im Duell", "Mascotte nel duello" } },
        { "mascota_tuya", new[] { "Tu mascota", "Your mascot", "Seu mascote", "Ta mascotte", "Dein Maskottchen", "La tua mascotte" } },
        { "funda", new[] { "Funda", "Card sleeve", "Protetor", "Protège-carte", "Kartenhülle", "Bustina" } },
        { "funda.tip", new[] {
            "El dorso de las cartas boca abajo (las tuyas y las del rival), con un monstruo en alta resolución. Clásica: el reverso del juego.",
            "The back of face-down cards (yours and the opponent's), with a high-resolution monster. Classic: the game's own back.",
            "O verso das cartas viradas (as suas e as do rival), com um monstro em alta resolução. Clássica: o verso do jogo.",
            "Le dos des cartes face cachée (les vôtres et celles de l'adversaire), avec un monstre en haute résolution. Classique : le dos du jeu.",
            "Die Rückseite verdeckter Karten (deine und die des Gegners), mit einem Monster in hoher Auflösung. Klassisch: die Rückseite des Spiels.",
            "Il retro delle carte coperte (le tue e quelle dell'avversario), con un mostro ad alta risoluzione. Classica: il retro del gioco." } },
        { "tapete", new[] { "Tapete", "Playmat", "Tapete", "Tapis de jeu", "Spielmatte", "Tappetino" } },
        { "tapete.tip", new[] {
            "Tu mitad del tablero en el duelo, con un monstruo en alta resolución (la del rival queda como en el juego). Clásico: el tablero del juego.",
            "Your half of the duel board, with a high-resolution monster (the opponent's stays as in the game). Classic: the game's board.",
            "A sua metade do tabuleiro no duelo, com um monstro em alta resolução (a do rival fica como no jogo). Clássico: o tabuleiro do jogo.",
            "Votre moitié du plateau en duel, avec un monstre en haute résolution (celle de l'adversaire reste comme dans le jeu). Classique : le plateau du jeu.",
            "Deine Hälfte des Duellfelds, mit einem Monster in hoher Auflösung (die des Gegners bleibt wie im Spiel). Klassisch: das Feld des Spiels.",
            "La tua metà del campo nel duello, con un mostro ad alta risoluzione (quella dell'avversario resta come nel gioco). Classico: il campo del gioco." } },
        { "tapete_efecto", new[] { "Efecto del tapete", "Playmat effect", "Efeito do tapete", "Effet du tapis", "Spielmatten-Effekt", "Effetto del tappetino" } },
        { "tapete_efecto.tip", new[] {
            "Una luz en tu tapete durante el duelo, debajo de las cartas. Luz: una franja que lo cruza cada tanto. Pulso: luz que recorre los marcos de las casillas. Brillo: partículas de luz que suben.",
            "A light on your playmat during the duel, under the cards. Light: a band crossing it now and then. Pulse: light running along the zones' frames. Sparkle: motes of light rising.",
            "Uma luz no seu tapete durante o duelo, sob as cartas. Luz: uma faixa que o cruza de vez em quando. Pulso: luz que percorre as molduras das casas. Brilho: partículas de luz que sobem.",
            "Une lumière sur ton tapis pendant le duel, sous les cartes. Lumière : une bande qui le traverse de temps en temps. Pulsation : une lumière qui parcourt les cadres des zones. Éclat : des particules de lumière qui montent.",
            "Ein Licht auf deiner Spielmatte im Duell, unter den Karten. Licht: ein Streifen, der sie ab und zu überquert. Puls: Licht, das die Rahmen der Zonen entlangläuft. Glitzer: aufsteigende Lichtpartikel.",
            "Una luce sul tuo tappetino durante il duello, sotto le carte. Luce: una striscia che lo attraversa ogni tanto. Impulso: luce che percorre le cornici delle zone. Scintillio: particelle di luce che salgono." } },
        { "funda_marco", new[] { "Marco de la funda", "Sleeve frame", "Moldura do protetor", "Cadre du protège-carte", "Hüllenrahmen", "Cornice della bustina" } },
        { "funda_marco.tip", new[] {
            "El marco alrededor del monstruo de la funda.",
            "The frame around the sleeve's monster.",
            "A moldura em volta do monstro do protetor.",
            "Le cadre autour du monstre du protège-carte.",
            "Der Rahmen um das Monster der Hülle.",
            "La cornice intorno al mostro della bustina." } },
        { "carta_as", new[] { "Carta As", "Ace card", "Carta Ás", "Carte As", "Ass-Karte", "Carta Asso" } },
        { "carta_as.tip", new[] {
            "Cuando llamás boca arriba la carta de tu mascota (o el rival la de la suya), el duelo espera unos segundos mientras la carta cae con su animación.",
            "When you summon your mascot's card face up (or the opponent summons theirs), the duel waits a few seconds while the card lands with its animation.",
            "Quando você invoca com a face para cima a carta do seu mascote (ou o rival a dele), o duelo espera alguns segundos enquanto a carta cai com a sua animação.",
            "Quand tu invoques face recto la carte de ta mascotte (ou l'adversaire la sienne), le duel attend quelques secondes pendant que la carte tombe avec son animation.",
            "Wenn du die Karte deines Maskottchens offen beschwörst (oder der Gegner seine), wartet das Duell ein paar Sekunden, während die Karte mit ihrer Animation landet.",
            "Quando evochi scoperta la carta della tua mascotte (o l'avversario la sua), il duello aspetta qualche secondo mentre la carta cade con la sua animazione." } },
        { "Siempre", new[] { "Siempre", "Always", "Sempre", "Toujours", "Immer", "Sempre" } },
        { "Primera vez por duelo", new[] { "Primera vez por duelo", "First time per duel", "Primeira vez por duelo", "Première fois par duel", "Erstes Mal pro Duell", "Prima volta per duello" } },
        { "entrada_duelo", new[] { "Entrada del duelo", "Duel entrance", "Entrada do duelo", "Entrée du duel", "Duell-Einzug", "Ingresso del duello" } },
        { "entrada_duelo.tip", new[] {
            "Completa: al abrirse las puertas el templo se acerca y las mascotas entran con los carteles. Solo las mascotas: sin el templo. Original: como el juego.",
            "Full: as the doors open the temple comes toward you and the mascots come in with the name plates. Mascots only: without the temple. Original: as the game was.",
            "Completa: ao abrir as portas o templo se aproxima e os mascotes entram com as placas. Só os mascotes: sem o templo. Original: como o jogo.",
            "Complète : à l'ouverture des portes le temple s'approche et les mascottes entrent avec les plaques. Mascottes seules : sans le temple. Originale : comme le jeu.",
            "Vollständig: Beim Öffnen der Tore kommt der Tempel näher und die Maskottchen erscheinen mit den Namensschildern. Nur Maskottchen: ohne den Tempel. Original: wie im Spiel.",
            "Completo: all'apertura delle porte il tempio si avvicina e le mascotte entrano con le targhe. Solo le mascotte: senza il tempio. Originale: come il gioco." } },
        { "mascota_tuya.tip", new[] {
            "El monstruo que te acompaña en el duelo, sobre tu nombre. Cada rival tiene el suyo.",
            "The monster by your side in the duel, above your name. Each opponent has its own.",
            "O monstro que te acompanha no duelo, acima do seu nome. Cada rival tem o seu.",
            "Le monstre qui vous accompagne en duel, au-dessus de votre nom. Chaque adversaire a le sien.",
            "Das Monster, das dich im Duell begleitet, über deinem Namen. Jeder Gegner hat sein eigenes.",
            "Il mostro che ti accompagna nel duello, sopra il tuo nome. Ogni avversario ha il suo." } },
        { "mascotas.tip", new[] {
            "Un monstruo 3D del juego, con su animación, junto al nombre de cada jugador.",
            "A 3D monster from the game, with its animation, next to each player's name.",
            "Um monstro 3D do jogo, com sua animação, junto ao nome de cada jogador.",
            "Un monstre 3D du jeu, avec son animation, à côté du nom de chaque joueur.",
            "Ein 3D-Monster des Spiels mit seiner Animation neben dem Namen jedes Spielers.",
            "Un mostro 3D del gioco, con la sua animazione, accanto al nome di ogni giocatore." } },
        { "rival_rapido", new[] { "Turno del rival rápido", "Fast opponent turn", "Turno do rival rápido",
            "Tour adverse rapide", "Schneller Gegnerzug", "Turno avversario veloce" } },
        { "auto_cadena", new[] { "AUTO en el duelo", "AUTO in duels", "AUTO no duelo", "AUTO en duel",
            "AUTO im Duell", "AUTO nel duello" } },
        { "auto_cadena.tip", new[] {
            "Encendido, el juego no te pregunta si querés encadenar una carta: el duelo sigue solo. También se cambia con un clic en la placa AUTO, debajo de tu nombre.",
            "On, the game never asks whether you want to chain a card: the duel goes on by itself. Also switched by clicking the AUTO plate under your name.",
            "Ligado, o jogo não pergunta se você quer encadear uma carta: o duelo segue sozinho. Também muda com um clique na placa AUTO, embaixo do seu nome.",
            "Activé, le jeu ne vous demande jamais si vous voulez enchaîner une carte : le duel continue seul. Se change aussi d'un clic sur la plaque AUTO, sous votre nom.",
            "An: Das Spiel fragt nie, ob du eine Karte verketten willst; das Duell läuft weiter. Auch per Klick auf das AUTO-Schild unter deinem Namen umschaltbar.",
            "Attivo, il gioco non ti chiede mai se vuoi concatenare una carta: il duello prosegue da solo. Si cambia anche con un clic sulla targa AUTO, sotto il tuo nome." } },
        { "guardar_ganar", new[] { "Guardar al ganar", "Save after a win", "Salvar ao vencer",
            "Sauvegarder après une victoire", "Nach einem Sieg speichern", "Salva dopo una vittoria" } },
        { "guardar_ganar.tip", new[] {
            "Al ganar un duelo (o al terminar un torneo), la partida se guarda sola al volver al menú, con el guardado del juego. Antes se copia la anterior en Partidas\\<perfil>\\respaldos (las últimas 5).",
            "When you win a duel (or finish a tournament), the game saves itself back at the menu, with its own save. The previous save is copied to Partidas\\<profile>\\respaldos first (the last 5).",
            "Ao vencer um duelo (ou terminar um torneio), o jogo salva sozinho ao voltar ao menu. Antes, o anterior é copiado em Partidas\\<perfil>\\respaldos (os últimos 5).",
            "Quand vous gagnez un duel (ou finissez un tournoi), la partie se sauvegarde seule au retour au menu. L'ancienne est copiée dans Partidas\\<profil>\\respaldos (les 5 dernières).",
            "Wenn du ein Duell gewinnst (oder ein Turnier beendest), speichert das Spiel zurück im Menü von selbst. Der alte Spielstand wird vorher in Partidas\\<Profil>\\respaldos kopiert (die letzten 5).",
            "Quando vinci un duello (o finisci un torneo), la partita si salva da sola al ritorno al menu. Prima la precedente viene copiata in Partidas\\<profilo>\\respaldos (le ultime 5)." } },
        { "numero_danio", new[] { "Número de daño", "Damage number", "Número de dano", "Nombre de dégâts", "Schadenszahl", "Numero del danno" } },
        { "numero_danio.tip", new[] {
            "Cuando bajan o suben los PV, sale el número al lado de la barra (-1500 en rojo, +500 en verde), con los números del juego.",
            "When life points go down or up, the amount shows by the bar (-1500 in red, +500 in green), in the game's own digits.",
            "Quando os PV descem ou sobem, o número aparece ao lado da barra (-1500 em vermelho, +500 em verde), com os números do jogo.",
            "Quand les PV baissent ou montent, le nombre s'affiche près de la barre (-1500 en rouge, +500 en vert), avec les chiffres du jeu.",
            "Wenn die LP sinken oder steigen, erscheint die Zahl neben der Leiste (-1500 rot, +500 grün), mit den Ziffern des Spiels.",
            "Quando i PV scendono o salgono, il numero appare accanto alla barra (-1500 in rosso, +500 in verde), con le cifre del gioco." } },
        { "rival_rapido.tip", new[] {
            "Mientras juega el rival, el juego va x2 o x4. Vuelve a lo normal cuando te toca responder.",
            "While the opponent plays, the game runs x2 or x4. Back to normal when you have to answer.",
            "Enquanto o rival joga, o jogo vai x2 ou x4. Volta ao normal quando é sua vez de responder.",
            "Pendant que l'adversaire joue, le jeu va x2 ou x4. Retour à la normale quand vous devez répondre.",
            "Während der Gegner spielt, läuft das Spiel x2 oder x4. Normal, sobald du antworten musst.",
            "Mentre gioca l'avversario, il gioco va x2 o x4. Torna normale quando tocca a te rispondere." } },
        { "cartas_nuevas", new[] { "Cartas agregadas", "Added cards", "Cartas adicionadas",
            "Cartes ajoutées", "Hinzugefügte Karten", "Carte aggiunte" } },
        { "cartas_nuevas.tip", new[] {
            "Las cartas que el port agrega al juego (YuGiOh-Contenido). Con No, el juego queda con sus cartas originales. Se aplica al reiniciar.",
            "The cards the port adds to the game (YuGiOh-Contenido). With No, the game keeps only its original cards. Applied on restart.",
            "As cartas que o port adiciona ao jogo (YuGiOh-Contenido). Com Não, o jogo fica só com as cartas originais. Aplica-se ao reiniciar.",
            "Les cartes que le port ajoute au jeu (YuGiOh-Contenido). Avec Non, le jeu garde seulement ses cartes d'origine. Appliqué au redémarrage.",
            "Die Karten, die der Port dem Spiel hinzufügt (YuGiOh-Contenido). Mit Nein behält das Spiel nur seine Originalkarten. Gilt nach Neustart.",
            "Le carte che il port aggiunge al gioco (YuGiOh-Contenido). Con No, il gioco tiene solo le sue carte originali. Si applica al riavvio." } },
        { "tex_dump", new[] { "Volcar texturas a carpeta", "Dump textures to folder", "Exportar texturas para pasta",
            "Exporter les textures", "Texturen in Ordner speichern", "Esporta texture in cartella" } },
        { "tex_dump.tip", new[] {
            "Para hacer mods: guarda cada textura del juego como PNG en modding\\texturas\\dump.",
            "For modding: saves every game texture as PNG in modding\\texturas\\dump.",
            "Para mods: salva cada textura do jogo como PNG em modding\\texturas\\dump.",
            "Pour les mods : enregistre chaque texture du jeu en PNG dans modding\\texturas\\dump.",
            "Für Mods: speichert jede Spieltextur als PNG in modding\\texturas\\dump.",
            "Per le mod: salva ogni texture del gioco come PNG in modding\\texturas\\dump." } },

        { "language", new[] { "Idioma del juego", "Game language", "Idioma do jogo",
            "Langue du jeu", "Spielsprache", "Lingua del gioco" } },
        { "language.tip", new[] {
            "Idioma de los textos del juego (al reiniciar). Las voces en español latino suenan con Español.",
            "Language of the game's text (on restart). The Latin American Spanish voices play with Español.",
            "Idioma dos textos do jogo (ao reiniciar; o jogo não tem português). As vozes em espanhol latino tocam com Español.",
            "Langue des textes du jeu (au redémarrage). Les voix en espagnol latino jouent avec Español.",
            "Sprache der Spieltexte (nach Neustart). Die lateinamerikanischen Stimmen laufen mit Español.",
            "Lingua dei testi del gioco (al riavvio). Le voci in spagnolo latino suonano con Español." } },
        { "voces_velocidad", new[] { "Voces", "Voices", "Vozes",
            "Voix", "Stimmen", "Voci" } },
        { "voces_velocidad.tip", new[] {
            "Sigue los FPS: si el juego va a más de 60 fps, las voces hablan más rápido (mismo tono) para no quedar atrás de los globos. Normal: siempre a su velocidad. No: sin voces.",
            "Follows the FPS: above 60 fps the voices speak faster (same pitch) to keep up with the balloons. Normal: always at their own speed. No: no voices.",
            "Segue os FPS: acima de 60 fps as vozes falam mais rápido (mesmo tom) para acompanhar os balões. Normal: sempre na velocidade delas. Não: sem vozes.",
            "Suit les FPS : au-delà de 60 fps les voix parlent plus vite (même ton) pour suivre les bulles. Normale : toujours à leur vitesse. Non : sans voix.",
            "Folgt den FPS: Über 60 fps sprechen die Stimmen schneller (gleiche Tonhöhe), um mit den Sprechblasen mitzuhalten. Normal: immer im eigenen Tempo. Nein: keine Stimmen.",
            "Segue gli FPS: oltre i 60 fps le voci parlano più veloci (stesso tono) per stare al passo con i fumetti. Normale: sempre alla loro velocità. No: senza voci." } },
        { "modo_rapido", new[] { "Modo experimental", "Experimental mode", "Modo experimental",
            "Mode expérimental", "Experimenteller Modus", "Modalità sperimentale" } },
        { "modo_rapido.tip", new[] {
            "Para PC de gama baja: el juego y el dibujo trabajan a la vez (al reiniciar). Si ves fallas, apagalo.",
            "For low-end PCs: the game and the renderer work at the same time (on restart). If you see glitches, turn it off.",
            "Para PCs mais fracos: o jogo e o desenho trabalham ao mesmo tempo (ao reiniciar). Se notar falhas, desative.",
            "Pour les PC modestes : le jeu et le rendu travaillent en même temps (au redémarrage). En cas de problèmes, désactivez-le.",
            "Für schwache PCs: Spiel und Grafik arbeiten gleichzeitig (nach Neustart). Bei Fehlern ausschalten.",
            "Per PC poco potenti: il gioco e il disegno lavorano insieme (al riavvio). Se vedi problemi, disattivala." } },

        { "play", new[] { "JUGAR", "PLAY", "JOGAR", "JOUER", "SPIELEN", "GIOCA" } },
        // 2026-11-25: Respaldos and Informe de error (Respaldos.cs)
        { "perf_respaldos", new[] { "Respaldos", "Backups", "Backups", "Sauvegardes", "Sicherungen", "Backup" } },
        { "resp_t", new[] { "Respaldos de {0}", "{0}'s backups", "Backups de {0}", "Sauvegardes de {0}", "Sicherungen von {0}", "Backup di {0}" } },
        { "resp_nota", new[] { "La partida se guarda sola al ganar; antes se copia la anterior acá (las últimas 5).",
            "The game saves itself when you win; the previous save is copied here first (the last 5).",
            "O jogo salva sozinho ao vencer; antes o anterior é copiado aqui (os últimos 5).",
            "La partie se sauvegarde seule à la victoire ; l'ancienne est d'abord copiée ici (les 5 dernières).",
            "Das Spiel speichert sich beim Sieg selbst; der alte Stand wird vorher hierher kopiert (die letzten 5).",
            "La partita si salva da sola quando vinci; prima la precedente viene copiata qui (le ultime 5)." } },
        { "resp_nota2", new[] { "Tu partida de ahora se guarda como respaldo antes de reemplazarla.",
            "Your current save is kept as a backup before it is replaced.",
            "Seu jogo atual vira um backup antes de ser substituído.",
            "Votre partie actuelle est gardée en sauvegarde avant d'être remplacée.",
            "Dein aktueller Spielstand wird vorher als Sicherung behalten.",
            "La partita attuale viene tenuta come backup prima di essere sostituita." } },
        { "resp_restaurar", new[] { "Restaurar", "Restore", "Restaurar", "Restaurer", "Wiederherstellen", "Ripristina" } },
        { "resp_cerrar", new[] { "Cerrar", "Close", "Fechar", "Fermer", "Schließen", "Chiudi" } },
        { "resp_hoy", new[] { "Hoy", "Today", "Hoje", "Aujourd'hui", "Heute", "Oggi" } },
        { "resp_ayer", new[] { "Ayer", "Yesterday", "Ontem", "Hier", "Gestern", "Ieri" } },
        { "resp_vacio", new[] { "Todavía no hay respaldos.", "No backups yet.", "Ainda não há backups.", "Pas encore de sauvegardes.",
            "Noch keine Sicherungen.", "Ancora nessun backup." } },
        { "resp_sinpartida", new[] { "Este perfil todavía no tiene partida.", "This profile has no save yet.",
            "Este perfil ainda não tem jogo salvo.", "Ce profil n'a pas encore de partie.", "Dieses Profil hat noch keinen Spielstand.",
            "Questo profilo non ha ancora una partita." } },
        { "resp_juego", new[] { "Cerrá el juego antes de restaurar.", "Close the game before restoring.", "Feche o jogo antes de restaurar.",
            "Fermez le jeu avant de restaurer.", "Schließe das Spiel vor dem Wiederherstellen.", "Chiudi il gioco prima di ripristinare." } },
        { "resp_conf", new[] { "¿Restaurar la partida de {0}? La de ahora queda como respaldo.",
            "Restore the save from {0}? The current one is kept as a backup.",
            "Restaurar o jogo de {0}? O atual fica como backup.",
            "Restaurer la partie de {0} ? L'actuelle est gardée en sauvegarde.",
            "Den Spielstand von {0} wiederherstellen? Der aktuelle bleibt als Sicherung.",
            "Ripristinare la partita di {0}? Quella attuale resta come backup." } },
        { "resp_ok", new[] { "Partida restaurada ({0}).", "Save restored ({0}).", "Jogo restaurado ({0}).", "Partie restaurée ({0}).",
            "Spielstand wiederhergestellt ({0}).", "Partita ripristinata ({0})." } },
        { "err_aviso", new[] { "El juego se cerró por un error ({0}). Armá un informe para mandarlo.",
            "The game closed on an error ({0}). Build a report to send it.",
            "O jogo fechou por um erro ({0}). Monte um relatório para enviar.",
            "Le jeu s'est fermé sur une erreur ({0}). Créez un rapport pour l'envoyer.",
            "Das Spiel wurde wegen eines Fehlers geschlossen ({0}). Erstelle einen Bericht zum Senden.",
            "Il gioco si è chiuso per un errore ({0}). Crea un rapporto da inviare." } },
        { "err_armar", new[] { "Armar informe", "Build report", "Montar relatório", "Créer le rapport", "Bericht erstellen", "Crea rapporto" } },
        { "err_descartar", new[] { "Descartar", "Dismiss", "Descartar", "Ignorer", "Verwerfen", "Ignora" } },
        { "err_boton", new[] { "Informe de error", "Error report", "Relatório de erro", "Rapport d'erreur", "Fehlerbericht", "Rapporto errori" } },
        { "err_boton.tip", new[] {
            "Arma un zip en el Escritorio con los registros, los ajustes y la versión (nunca tu partida), para mandarlo si algo falló.",
            "Builds a zip on the Desktop with the logs, settings and version (never your save), to send it if something went wrong.",
            "Cria um zip na Área de Trabalho com os registros, ajustes e versão (nunca seu jogo), para enviar se algo falhou.",
            "Crée un zip sur le Bureau avec les journaux, les réglages et la version (jamais votre partie), à envoyer en cas de problème.",
            "Erstellt ein Zip auf dem Desktop mit Protokollen, Einstellungen und Version (nie dein Spielstand), zum Senden bei Problemen.",
            "Crea uno zip sul Desktop con i registri, le impostazioni e la versione (mai la tua partita), da inviare se qualcosa non va." } },
        { "err_preg", new[] {
            "¿Armar un informe de error?\n\nSe va a crear un archivo .zip en tu Escritorio con los registros del juego, tus ajustes y la versión (nunca tu partida). Mandalo a quien te ayude con el problema.",
            "Build an error report?\n\nA .zip file will be created on your Desktop with the game's logs, your settings and the version (never your save). Send it to whoever is helping you.",
            "Montar um relatório de erro?\n\nSerá criado um arquivo .zip na sua Área de Trabalho com os registros do jogo, seus ajustes e a versão (nunca seu jogo salvo). Envie para quem estiver ajudando.",
            "Créer un rapport d'erreur ?\n\nUn fichier .zip sera créé sur votre Bureau avec les journaux du jeu, vos réglages et la version (jamais votre partie). Envoyez-le à la personne qui vous aide.",
            "Fehlerbericht erstellen?\n\nAuf deinem Desktop wird eine .zip-Datei mit den Spielprotokollen, deinen Einstellungen und der Version erstellt (nie dein Spielstand). Schick sie an die Person, die dir hilft.",
            "Creare un rapporto di errore?\n\nVerrà creato un file .zip sul tuo Desktop con i registri del gioco, le tue impostazioni e la versione (mai la tua partita). Invialo a chi ti sta aiutando." } },
        { "err_listo", new[] { "Informe listo: {0} (registros, ajustes y versión; sin tu partida).",
            "Report ready: {0} (logs, settings and version; not your save).",
            "Relatório pronto: {0} (registros, ajustes e versão; sem seu jogo).",
            "Rapport prêt : {0} (journaux, réglages et version ; sans votre partie).",
            "Bericht fertig: {0} (Protokolle, Einstellungen und Version; ohne Spielstand).",
            "Rapporto pronto: {0} (registri, impostazioni e versione; senza la tua partita)." } },
        { "err_fallo", new[] { "No se pudo armar el informe: ", "Could not build the report: ", "Não foi possível montar o relatório: ",
            "Impossible de créer le rapport : ", "Bericht konnte nicht erstellt werden: ", "Impossibile creare il rapporto: " } },
        // 2026-11-25: profiles (Perfiles.cs)
        { "play_as", new[] { "JUGAR COMO {0}", "PLAY AS {0}", "JOGAR COMO {0}", "JOUER EN TANT QUE {0}", "SPIELEN ALS {0}",
            "GIOCA COME {0}" } },
        { "g_perfil", new[] { "Perfil", "Profile", "Perfil", "Profil", "Profil", "Profilo" } },
        { "perf_nuevo", new[] { "Nuevo perfil", "New profile", "Novo perfil", "Nouveau profil", "Neues Profil", "Nuovo profilo" } },
        { "perf_carpeta", new[] { "Abrir su carpeta", "Open its folder", "Abrir a pasta", "Ouvrir son dossier",
            "Ordner öffnen", "Apri la cartella" } },
        { "perf_resumen", new[] { "{0}  ·  {1} ranuras con datos  ·  {2} duelos", "{0}  ·  {1} slots with data  ·  {2} duels",
            "{0}  ·  {1} espaços com dados  ·  {2} duelos", "{0}  ·  {1} emplacements utilisés  ·  {2} duels",
            "{0}  ·  {1} Plätze mit Daten  ·  {2} Duelle", "{0}  ·  {1} slot con dati  ·  {2} duelli" } },
        { "perf_nuevo_t", new[] { "Nuevo perfil", "New profile", "Novo perfil", "Nouveau profil", "Neues Profil", "Nuovo profilo" } },
        { "perf_nombre", new[] { "Nombre", "Name", "Nome", "Nom", "Name", "Nome" } },
        { "perf_nuevo_nota", new[] { "Empieza de cero: partida nueva, sin historial ni mazos ganados.",
            "Starts from scratch: a new save, no history, no earned decks.",
            "Começa do zero: jogo novo, sem histórico nem decks ganhos.",
            "Repart de zéro : nouvelle partie, sans historique ni decks gagnés.",
            "Beginnt bei null: neuer Spielstand, ohne Verlauf und gewonnene Decks.",
            "Parte da zero: nuova partita, senza cronologia né mazzi vinti." } },
        { "perf_crear", new[] { "Crear", "Create", "Criar", "Créer", "Erstellen", "Crea" } },
        { "perf_cancelar", new[] { "Cancelar", "Cancel", "Cancelar", "Annuler", "Abbrechen", "Annulla" } },
        { "perf_err_vacio", new[] { "Escribí un nombre.", "Type a name.", "Digite um nome.", "Écrivez un nom.",
            "Gib einen Namen ein.", "Scrivi un nome." } },
        { "perf_err_existe", new[] { "Ya hay un perfil con ese nombre.", "A profile with that name already exists.",
            "Já existe um perfil com esse nome.", "Un profil porte déjà ce nom.", "Ein Profil mit diesem Namen gibt es schon.",
            "Esiste già un profilo con quel nome." } },
        { "defaults", new[] { "Valores por defecto", "Defaults", "Padrões", "Par défaut", "Standardwerte", "Predefiniti" } },
        { "open_ajustes", new[] { "Abrir carpeta Ajustes", "Open Ajustes folder", "Abrir pasta Ajustes",
            "Ouvrir le dossier Ajustes", "Ordner Ajustes öffnen", "Apri cartella Ajustes" } },
        { "saved", new[] { "Guardado en Ajustes\\ygo_menu.ini.", "Saved to Ajustes\\ygo_menu.ini.",
            "Salvo em Ajustes\\ygo_menu.ini.", "Enregistré dans Ajustes\\ygo_menu.ini.",
            "In Ajustes\\ygo_menu.ini gespeichert.", "Salvato in Ajustes\\ygo_menu.ini." } },
        { "from_game", new[] { "Cambiado desde el juego (F2).", "Changed from the game (F2).", "Mudado pelo jogo (F2).",
            "Modifié depuis le jeu (F2).", "Im Spiel geändert (F2).", "Cambiato dal gioco (F2)." } },
        { "defaults_msg", new[] { "Valores por defecto guardados.", "Default values saved.", "Valores padrão salvos.",
            "Valeurs par défaut enregistrées.", "Standardwerte gespeichert.", "Valori predefiniti salvati." } },
        { "started", new[] { "Juego abierto. Podés seguir cambiando ajustes acá.",
            "Game started. You can keep changing settings here.",
            "Jogo aberto. Você pode continuar mudando os ajustes aqui.",
            "Jeu lancé. Vous pouvez continuer à modifier les réglages ici.",
            "Spiel gestartet. Du kannst hier weiter Einstellungen ändern.",
            "Gioco avviato. Puoi continuare a cambiare le impostazioni qui." } },

        { "st_exe_ok", new[] { "✔  Programa del juego: {0}", "✔  Game program: {0}", "✔  Programa do jogo: {0}",
            "✔  Programme du jeu : {0}", "✔  Spielprogramm: {0}", "✔  Programma del gioco: {0}" } },
        { "st_exe_no", new[] { "✖  Falta Yu-Gi-Oh.exe (el juego) en esta carpeta.",
            "✖  Yu-Gi-Oh.exe (the game) is missing from this folder.",
            "✖  Falta Yu-Gi-Oh.exe (o jogo) nesta pasta.",
            "✖  Yu-Gi-Oh.exe (le jeu) est absent de ce dossier.",
            "✖  Yu-Gi-Oh.exe (das Spiel) fehlt in diesem Ordner.",
            "✖  Manca Yu-Gi-Oh.exe (il gioco) in questa cartella." } },
        { "st_tex_ok", new[] { "✔  Texturas mejoradas: {0:0.0} GB", "✔  Improved textures: {0:0.0} GB",
            "✔  Texturas melhoradas: {0:0.0} GB", "✔  Textures améliorées : {0:0.0} Go",
            "✔  Verbesserte Texturen: {0:0.0} GB", "✔  Texture migliorate: {0:0.0} GB" } },
        { "st_tex_no", new[] { "–  Texturas mejoradas: no hay (opcional).", "–  Improved textures: none (optional).",
            "–  Texturas melhoradas: nenhuma (opcional).", "–  Textures améliorées : aucune (facultatif).",
            "–  Verbesserte Texturen: keine (optional).", "–  Texture migliorate: nessuna (opzionale)." } },
        { "st_voz_ok", new[] { "✔  Voces en español latino: {0} frases", "✔  Latin American Spanish voices: {0} lines",
            "✔  Vozes em espanhol latino: {0} falas", "✔  Voix en espagnol latino : {0} répliques",
            "✔  Lateinamerikanische Stimmen: {0} Sätze", "✔  Voci in spagnolo latino: {0} battute" } },
        { "st_voz_no", new[] { "–  Voces en español latino: no hay (opcional).", "–  Latin American Spanish voices: none (optional).",
            "–  Vozes em espanhol latino: nenhuma (opcional).", "–  Voix en espagnol latino : aucune (facultatif).",
            "–  Lateinamerikanische Stimmen: keine (optional).", "–  Voci in spagnolo latino: nessuna (opzionale)." } },
        { "st_ajustes", new[] { "✔  Ajustes en: {0}", "✔  Settings in: {0}", "✔  Ajustes em: {0}",
            "✔  Réglages dans : {0}", "✔  Einstellungen in: {0}", "✔  Impostazioni in: {0}" } },
        // 2026-11-21: the quality profiles (the user: "Bajo, Medio, Alto, Ultra y Recomendado ... tambien personalizado")
        { "st_pc", new[] { "✔  Tu PC: {0} hilos, {1} GB de RAM, {2} ({3} GB)  →  recomendado: {4}",
            "✔  Your PC: {0} threads, {1} GB RAM, {2} ({3} GB)  →  recommended: {4}",
            "✔  Seu PC: {0} threads, {1} GB de RAM, {2} ({3} GB)  →  recomendado: {4}",
            "✔  Votre PC : {0} threads, {1} Go de RAM, {2} ({3} Go)  →  recommandé : {4}",
            "✔  Dein PC: {0} Threads, {1} GB RAM, {2} ({3} GB)  →  empfohlen: {4}",
            "✔  Il tuo PC: {0} thread, {1} GB di RAM, {2} ({3} GB)  →  consigliato: {4}" } },
        { "perfil", new[] { "Perfil de calidad", "Quality profile", "Perfil de qualidade", "Profil de qualité",
            "Qualitätsprofil", "Profilo di qualità" } },
        { "perfil.tip", new[] {
            "Elige resolución, filtrado, cuadros por segundo, texturas mejoradas y mascotas de una vez. Recomendado: el que mejor va con tu PC (se detecta al abrir). Si cambias una de esas opciones a mano, pasa a Personalizado.",
            "Sets resolution, filtering, frame rate, enhanced textures and mascots at once. Recommended: the best one for your PC (detected on opening). Changing one of those by hand makes it Custom.",
            "Escolhe resolução, filtragem, quadros por segundo, texturas melhoradas e mascotes de uma vez. Recomendado: o melhor para o seu PC (detectado ao abrir). Mudar uma dessas à mão o torna Personalizado.",
            "Règle d'un coup la résolution, le filtrage, les images par seconde, les textures améliorées et les mascottes. Recommandé : le meilleur pour votre PC (détecté à l'ouverture). Modifier l'un d'eux à la main passe en Personnalisé.",
            "Stellt Auflösung, Filterung, Bildrate, verbesserte Texturen und Maskottchen auf einmal ein. Empfohlen: das beste für deinen PC (beim Öffnen erkannt). Eine davon von Hand ändern macht es Benutzerdefiniert.",
            "Imposta in una volta risoluzione, filtro, fotogrammi al secondo, texture migliorate e mascotte. Consigliato: il migliore per il tuo PC (rilevato all'apertura). Cambiarne una a mano lo rende Personalizzato." } },
        { "err_save", new[] { "No pude guardar los ajustes:\n", "Could not save the settings:\n",
            "Não foi possível salvar os ajustes:\n", "Impossible d'enregistrer les réglages :\n",
            "Die Einstellungen konnten nicht gespeichert werden:\n", "Impossibile salvare le impostazioni:\n" } },
        { "err_run", new[] { "No pude abrir el juego:\n", "Could not start the game:\n", "Não foi possível abrir o jogo:\n",
            "Impossible de lancer le jeu :\n", "Das Spiel konnte nicht gestartet werden:\n", "Impossibile avviare il gioco:\n" } },
    };

    // The shown text of a value ygo_menu.ini stores in Spanish.
    static readonly Dictionary<string, string[]> V = new Dictionary<string, string[]> {
        { "Sí", new[] { "Sí", "Yes", "Sim", "Oui", "Ja", "Sì" } },
        { "Clásica", new[] { "Clásica", "Classic", "Clássica", "Classique", "Klassisch", "Classica" } },
        { "Clásico", new[] { "Clásico", "Classic", "Clássico", "Classique", "Klassisch", "Classico" } },
        { "Dorado", new[] { "Dorado", "Gold", "Dourado", "Doré", "Gold", "Dorato" } },
        { "Plata", new[] { "Plata", "Silver", "Prata", "Argent", "Silber", "Argento" } },
        { "Obsidiana", new[] { "Obsidiana", "Obsidian", "Obsidiana", "Obsidienne", "Obsidian", "Ossidiana" } },
        { "Milenio", new[] { "Milenio", "Millennium", "Milênio", "Millénaire", "Millennium", "Millennio" } },
        { "Neón", new[] { "Neón", "Neon", "Neon", "Néon", "Neon", "Neon" } },
        { "Monitor", new[] { "Monitor", "Monitor", "Monitor", "Moniteur", "Monitor", "Monitor" } },
        { "Luz", new[] { "Luz", "Light", "Luz", "Lumière", "Licht", "Luce" } },
        { "Pulso", new[] { "Pulso", "Pulse", "Pulso", "Pulsation", "Puls", "Impulso" } },
        { "Brillo", new[] { "Brillo", "Sparkle", "Brilho", "Éclat", "Glitzer", "Scintillio" } },
        { "Completa", new[] { "Completa", "Full", "Completa", "Complète", "Vollständig", "Completo" } },
        { "Solo las mascotas", new[] { "Solo las mascotas", "Mascots only", "Só os mascotes", "Mascottes seules", "Nur Maskottchen", "Solo le mascotte" } },
        { "Original", new[] { "Original", "Original", "Original", "Originale", "Original", "Originale" } },
        { "No", new[] { "No", "No", "Não", "Non", "Nein", "No" } },
        { "Libre", new[] { "Libre", "Free", "Livre", "Libre", "Frei", "Libera" } },
        { "No (4:3)", new[] { "No (4:3)", "No (4:3)", "Não (4:3)", "Non (4:3)", "Nein (4:3)", "No (4:3)" } },
        { "Estirar a 16:9", new[] { "Estirar a 16:9", "Stretch to 16:9", "Esticar para 16:9", "Étirer en 16:9",
            "Auf 16:9 strecken", "Allarga a 16:9" } },
        { "Alta resolución", new[] { "Alta resolución", "High resolution", "Alta resolução", "Haute résolution",
            "Hohe Auflösung", "Alta risoluzione" } },
        { "Original (480p)", new[] { "Original (480p)", "Original (480p)", "Original (480p)", "Original (480p)",
            "Original (480p)", "Originale (480p)" } },
        { "Sin corregir (puede mostrar líneas)", new[] { "Sin corregir (puede mostrar líneas)",
            "Uncorrected (may show lines)", "Sem correção (pode mostrar linhas)", "Non corrigée (lignes possibles)",
            "Unkorrigiert (evtl. Linien)", "Non corretta (possibili linee)" } },
        { "Automático", new[] { "Automático", "Automatic", "Automático", "Automatique", "Automatisch", "Automatico" } },
        { "Recomendado", new[] { "Recomendado", "Recommended", "Recomendado", "Recommandé", "Empfohlen", "Consigliato" } },
        { "Bajo", new[] { "Bajo", "Low", "Baixo", "Bas", "Niedrig", "Basso" } },
        { "Medio", new[] { "Medio", "Medium", "Médio", "Moyen", "Mittel", "Medio" } },
        { "Alto", new[] { "Alto", "High", "Alto", "Élevé", "Hoch", "Alto" } },
        { "Ultra", new[] { "Ultra", "Ultra", "Ultra", "Ultra", "Ultra", "Ultra" } },
        { "Personalizado", new[] { "Personalizado", "Custom", "Personalizado", "Personnalisé", "Benutzerdefiniert",
            "Personalizzato" } },
        { "Teclado y ratón", new[] { "Teclado y ratón", "Keyboard and mouse", "Teclado e mouse",
            "Clavier et souris", "Tastatur und Maus", "Tastiera e mouse" } },
        { "Mando Xbox", new[] { "Mando Xbox", "Xbox controller", "Controle Xbox", "Manette Xbox", "Xbox-Controller", "Controller Xbox" } },
        { "Mando PS4/PS5", new[] { "Mando PS4/PS5", "PS4/PS5 controller", "Controle PS4/PS5", "Manette PS4/PS5",
            "PS4/PS5-Controller", "Controller PS4/PS5" } },
        { "Puntero (estilo PC)", new[] { "Puntero (estilo PC)", "Pointer (PC style)", "Ponteiro (estilo PC)",
            "Pointeur (style PC)", "Zeiger (PC-Stil)", "Puntatore (stile PC)" } },
        { "Cursor del juego", new[] { "Cursor del juego", "Game cursor", "Cursor do jogo", "Curseur du jeu",
            "Spielcursor", "Cursore del gioco" } },
        { "Solo clics", new[] { "Solo clics", "Clicks only", "Só cliques", "Clics seulement", "Nur Klicks", "Solo clic" } },
        { "Baja", new[] { "Baja", "Low", "Baixa", "Basse", "Niedrig", "Bassa" } },
        { "Media", new[] { "Media", "Medium", "Média", "Moyenne", "Mittel", "Media" } },
        { "Alta", new[] { "Alta", "High", "Alta", "Haute", "Hoch", "Alta" } },
        { "Sí, con fotos", new[] { "Sí, con fotos", "Yes, with photos", "Sim, com fotos", "Oui, avec photos", "Ja, mit Fotos", "Sì, con foto" } },
        { "Sí, sin fotos", new[] { "Sí, sin fotos", "Yes, no photos", "Sim, sem fotos", "Oui, sans photos", "Ja, ohne Fotos", "Sì, senza foto" } },
        { "Nombres y foto", new[] { "Nombres y foto", "Names and photo", "Nomes e foto", "Noms et photo", "Namen und Foto", "Nomi e foto" } },
        { "Solo nombres", new[] { "Solo nombres", "Names only", "Só nomes", "Noms seulement", "Nur Namen", "Solo nomi" } },
        { "Ojos Azules", new[] { "Ojos Azules", "Blue-Eyes", "Olhos Azuis", "Yeux Bleus", "Blauäugiger", "Occhi Blu" } },
        { "Kuriboh", new[] { "Kuriboh", "Kuriboh", "Kuriboh", "Kuriboh", "Kuriboh", "Kuriboh" } },
        { "Mago Oscuro", new[] { "Mago Oscuro", "Dark Magician", "Mago Negro", "Magicien Sombre", "Dunkler Magier", "Mago Nero" } },
        { "Jinzo", new[] { "Jinzo", "Jinzo", "Jinzo", "Jinzo", "Jinzo", "Jinzo" } },
        { "Cráneo Convocado", new[] { "Cráneo Convocado", "Summoned Skull", "Caveira Invocada", "Crâne Invoqué", "Beschworener Schädel", "Teschio Evocato" } },
        { "Ojos Rojos", new[] { "Ojos Rojos", "Red-Eyes", "Olhos Vermelhos", "Yeux Rouges", "Rotäugiger", "Occhi Rossi" } },
        { "Chica Maga Oscura", new[] { "Chica Maga Oscura", "Dark Magician Girl", "Menina Maga Negra", "Magicienne des Ténèbres", "Dunkles Magier-Mädchen", "Ragazza Maga Nera" } },
        { "Gaia el Caballero Feroz", new[] { "Gaia el Caballero Feroz", "Gaia the Fierce Knight", "Gaia, o Cavaleiro Impetuoso", "Gaia le Chevalier Farouche", "Gaia der grimmige Ritter", "Gaia il Cavaliere Feroce" } },
        { "Guardián Celta", new[] { "Guardián Celta", "Celtic Guardian", "Guardião Celta", "Gardien Celte", "Keltischer Wächter", "Guardiano Celtico" } },
        { "Dama Arpía", new[] { "Dama Arpía", "Harpie Lady", "Dama Harpia", "Dame Harpie", "Harpyie", "Arpia" } },
        { "Mago del Tiempo", new[] { "Mago del Tiempo", "Time Wizard", "Mago do Tempo", "Magicien du Temps", "Zeitzauberer", "Mago del Tempo" } },
        { "Dragón Bebé", new[] { "Dragón Bebé", "Baby Dragon", "Bebê Dragão", "Bébé Dragon", "Babydrache", "Drago Bebè" } },
        { "Espadachín de la Llama", new[] { "Espadachín de la Llama", "Flame Swordsman", "Espadachim das Chamas", "Épéiste des Flammes", "Flammenschwertkämpfer", "Spadaccino di Fiamma" } },
        { "Soldado del Brillo Negro", new[] { "Soldado del Brillo Negro", "Black Luster Soldier", "Soldado do Brilho Negro", "Soldat du Lustre Noir", "Schwarzglänzender Soldat", "Soldato del Lustro Nero" } },
        { "Exodia", new[] { "Exodia", "Exodia", "Exodia", "Exodia", "Exodia", "Exodia" } },
        { "Dragón Barril", new[] { "Dragón Barril", "Barrel Dragon", "Dragão Barril", "Dragon Barillet", "Revolverdrache", "Drago Revolver" } },
        { "Rey Rex de Dos Cabezas", new[] { "Rey Rex de Dos Cabezas", "Two-Headed King Rex", "Rei Rex de Duas Cabeças", "Roi Rex à Deux Têtes", "Zweiköpfiger König Rex", "Re Rex a Due Teste" } },
        { "Gran Polilla", new[] { "Gran Polilla", "Great Moth", "Grande Mariposa", "Grande Mite", "Große Motte", "Grande Falena" } },
        { "Pescador Legendario", new[] { "Pescador Legendario", "Legendary Fisherman", "Pescador Lendário", "Pêcheur Légendaire", "Legendärer Fischer", "Pescatore Leggendario" } },
        { "Restricción de Mil Ojos", new[] { "Restricción de Mil Ojos", "Thousand-Eyes Restrict", "Restrição de Mil Olhos", "Restriction aux Mille Yeux", "Tausend-Augen-Bann", "Restrizione dai Mille Occhi" } },
        { "Necrofear Oscuro", new[] { "Necrofear Oscuro", "Dark Necrofear", "Necrofear Sombrio", "Nécrofear Obscur", "Dunkle Nekrofurcht", "Necrofear Oscuro" } },
        { "Bestia Enmascarada", new[] { "Bestia Enmascarada", "The Masked Beast", "A Fera Mascarada", "La Bête Masquée", "Die maskierte Bestie", "La Bestia Mascherata" } },
        { "Máquina Tragamonedas", new[] { "Máquina Tragamonedas", "Slot Machine", "Máquina Caça-Níqueis", "Machine à Sous", "Spielautomat", "Slot Machine" } },
        { "Dragón Zombi", new[] { "Dragón Zombi", "Dragon Zombie", "Dragão Zumbi", "Dragon Zombie", "Drachenzombie", "Drago Zombie" } },
        { "Maha Vailo", new[] { "Maha Vailo", "Maha Vailo", "Maha Vailo", "Maha Vailo", "Maha Vailo", "Maha Vailo" } },
        { "Golem del Milenio", new[] { "Golem del Milenio", "Millennium Golem", "Golem do Milênio", "Golem du Millénaire", "Millenniumgolem", "Golem del Millennio" } },
        { "Dragón Definitivo de Ojos Azules", new[] { "Dragón Definitivo de Ojos Azules", "Blue-Eyes Ultimate Dragon", "Dragão Supremo de Olhos Azuis", "Dragon Ultime aux Yeux Bleus", "Blauäugiger ultimativer Drache", "Drago Definitivo Occhi Blu" } },
        { "Dragón Negro de Cráneo", new[] { "Dragón Negro de Cráneo", "B. Skull Dragon", "Dragão Caveira Negra", "Dragon Crâne Noir", "Schwarzer Schädeldrache", "Drago Teschio Nero" } },
        { "Mago del Caos Negro", new[] { "Mago del Caos Negro", "Magician of Black Chaos", "Mago do Caos Negro", "Magicien du Chaos Noir", "Magier des Schwarzen Chaos", "Mago del Caos Nero" } },
        { "Dragón Meteoro Negro", new[] { "Dragón Meteoro Negro", "Meteor B. Dragon", "Dragão Meteoro Negro", "Dragon Météore Noir", "Meteor-Schwarzdrache", "Drago Meteora Nero" } },
        { "Valkyrion el Guerrero Magna", new[] { "Valkyrion el Guerrero Magna", "Valkyrion the Magna Warrior", "Valkyrion, o Guerreiro Magna", "Valkyrion le Guerrier Magna", "Valkyrion der Magna-Krieger", "Valkyrion il Guerriero Magna" } },
        { "Dragón Toon de Ojos Azules", new[] { "Dragón Toon de Ojos Azules", "Blue-Eyes Toon Dragon", "Dragão Toon de Olhos Azuis", "Dragon Toon aux Yeux Bleus", "Blauäugiger Toon-Drache", "Drago Toon Occhi Blu" } },
        { "Gaia el Campeón Dragón", new[] { "Gaia el Campeón Dragón", "Gaia the Dragon Champion", "Gaia, o Campeão Dragão", "Gaia le Champion Dragon", "Gaia der Drachenchampion", "Gaia il Campione Drago" } },
        { "Sabio Oscuro", new[] { "Sabio Oscuro", "Dark Sage", "Sábio Sombrio", "Sage Sombre", "Dunkler Weiser", "Saggio Oscuro" } },
        { "Dragón del Trueno de Dos Cabezas", new[] { "Dragón del Trueno de Dos Cabezas", "Twin-Headed Thunder Dragon", "Dragão do Trovão de Duas Cabeças", "Dragon Tonnerre à Deux Têtes", "Zweiköpfiger Donnerdrache", "Drago del Tuono a Due Teste" } },
        { "Dragón de Tres Cuernos", new[] { "Dragón de Tres Cuernos", "Tri-Horned Dragon", "Dragão de Três Chifres", "Dragon Tricorne", "Dreihörniger Drache", "Drago Tricorno" } },
        { "Reina del Cosmos", new[] { "Reina del Cosmos", "Cosmo Queen", "Rainha Cósmica", "Reine du Cosmos", "Kosmoskönigin", "Regina del Cosmo" } },
        { "Zera el Mant", new[] { "Zera el Mant", "Zera the Mant", "Zera, o Mant", "Zera le Mant", "Zera der Mant", "Zera il Mant" } },
        { "Sengenjin", new[] { "Sengenjin", "Sengenjin", "Sengenjin", "Sengenjin", "Sengenjin", "Sengenjin" } },
        { "Caballero de Cráneo", new[] { "Caballero de Cráneo", "Skull Knight", "Cavaleiro Caveira", "Chevalier Crâne", "Schädelritter", "Cavaliere Teschio" } },
        { "Dragón Mascota de Harpie", new[] { "Dragón Mascota de Harpie", "Harpie's Pet Dragon", "Dragão de Estimação da Harpia", "Dragon Domestique de la Harpie", "Harpyien-Schoßdrache", "Drago da Compagnia dell'Arpia" } },
        { "Guardián de la Puerta", new[] { "Guardián de la Puerta", "Gate Guardian", "Guardião do Portão", "Gardien de la Porte", "Torwächter", "Guardiano del Cancello" } },
        { "Renunciado", new[] { "Renunciado", "Relinquished", "Renunciado", "Relinquished", "Relinquished", "Relinquished" } },
        { "Cráneo Convocado Toon", new[] { "Cráneo Convocado Toon", "Toon Summoned Skull", "Caveira Invocada Toon", "Crâne Invoqué Toon", "Toon-Herbeigerufener Schädel", "Teschio Evocato Toon" } },
        { "Maldición del Dragón", new[] { "Maldición del Dragón", "Curse of Dragon", "Maldição do Dragão", "Malédiction du Dragon", "Fluch des Drachen", "Maledizione del Drago" } },
        { "Mil Dragones", new[] { "Mil Dragones", "Thousand Dragon", "Dragão Milenar", "Dragon Millénaire", "Tausend-Drache", "Drago Millenario" } },
        { "Alfa el Guerrero Magnético", new[] { "Alfa el Guerrero Magnético", "Alpha The Magnet Warrior", "Alfa, o Guerreiro Magnético", "Alpha le Guerrier Magnétique", "Alpha der Magnetkrieger", "Alfa il Guerriero Magnetico" } },
        { "Beta el Guerrero Magnético", new[] { "Beta el Guerrero Magnético", "Beta The Magnet Warrior", "Beta, o Guerreiro Magnético", "Bêta le Guerrier Magnétique", "Beta der Magnetkrieger", "Beta il Guerriero Magnetico" } },
        { "Gamma el Guerrero Magnético", new[] { "Gamma el Guerrero Magnético", "Gamma The Magnet Warrior", "Gama, o Guerreiro Magnético", "Gamma le Guerrier Magnétique", "Gamma der Magnetkrieger", "Gamma il Guerriero Magnetico" } },
        { "Dragón Meteoro", new[] { "Dragón Meteoro", "Meteor Dragon", "Dragão Meteoro", "Dragon Météore", "Meteordrache", "Drago Meteora" } },
        { "Elfa Mística", new[] { "Elfa Mística", "Mystical Elf", "Elfa Mística", "Elfe Mystique", "Mystische Elfe", "Elfo Mistico" } },
        { "Bruja del Bosque Negro", new[] { "Bruja del Bosque Negro", "Witch of the Black Forest", "Bruxa da Floresta Negra", "Sorcière de la Forêt Noire", "Hexe aus dem Schwarzwald", "Strega della Foresta Nera" } },
        { "Tomate Místico", new[] { "Tomate Místico", "Mystic Tomato", "Tomate Místico", "Tomate Mystique", "Mystische Tomate", "Pomodoro Mistico" } },
        { "Sangan", new[] { "Sangan", "Sangan", "Sangan", "Sangan", "Sangan", "Sangan" } },
        { "Gardna del Gran Escudo", new[] { "Gardna del Gran Escudo", "Big Shield Gardna", "Gardna do Grande Escudo", "Gardna au Grand Bouclier", "Großschild-Gardna", "Gardna Grande Scudo" } },
        { "Payaso Zombi", new[] { "Payaso Zombi", "Clown Zombie", "Palhaço Zumbi", "Clown Zombie", "Clownzombie", "Zombie Clown" } },
        { "Fantasma Ingenioso", new[] { "Fantasma Ingenioso", "Witty Phantom", "Fantasma Espirituoso", "Fantôme Spirituel", "Witziges Phantom", "Fantasma Arguto" } },
        { "Mago de la Fe", new[] { "Mago de la Fe", "Magician of Faith", "Maga da Fé", "Magicienne de la Foi", "Magierin des Glaubens", "Maga della Fede" } },
        { "Légamo Humanoide", new[] { "Légamo Humanoide", "Humanoid Slime", "Lodo Humanoide", "Bave Humanoïde", "Menschlicher Schleim", "Melma Umanoide" } },
        { "Princesa de Fuego", new[] { "Princesa de Fuego", "Fire Princess", "Princesa do Fogo", "Princesse du Feu", "Feuerprinzessin", "Principessa del Fuoco" } },
        { "Hermanas Dama Arpía", new[] { "Hermanas Dama Arpía", "Harpie Lady Sisters", "Irmãs Dama Harpia", "Sœurs Dame Harpie", "Harpyie-Damen-Schwestern", "Sorelle Arpia" } },
        { "Sanga del Trueno", new[] { "Sanga del Trueno", "Sanga of the Thunder", "Sanga do Trovão", "Sanga du Tonnerre", "Sanga des Donners", "Sanga del Tuono" } },
        { "Kazejin", new[] { "Kazejin", "Kazejin", "Kazejin", "Kazejin", "Kazejin", "Kazejin" } },
        { "Suijin", new[] { "Suijin", "Suijin", "Suijin", "Suijin", "Suijin", "Suijin" } },
        { "Dokurorider", new[] { "Dokurorider", "Dokurorider", "Dokurorider", "Dokurorider", "Dokurorider", "Dokurorider" } },
        { "Bruja Oscura Dunames", new[] { "Bruja Oscura Dunames", "Dunames Dark Witch", "Bruxa Sombria Dunames", "Sorcière Sombre Dunames", "Dunkle Hexe Dunames", "Strega Oscura Dunames" } },
        { "Guerrero de Pizarra", new[] { "Guerrero de Pizarra", "Slate Warrior", "Guerreiro de Ardósia", "Guerrier d'Ardoise", "Schieferkrieger", "Guerriero d'Ardesia" } },
        { "Gusano Dragón Humanoide", new[] { "Gusano Dragón Humanoide", "Humanoid Worm Drake", "Verme Dragão Humanoide", "Ver Dragon Humanoïde", "Humanoider Wurmdrache", "Verme Drago Umanoide" } },
        { "Asaltante Vorse", new[] { "Asaltante Vorse", "Vorse Raider", "Saqueador Vorse", "Pillard Vorse", "Vorse-Plünderer", "Predone Vorse" } },
        { "Fuerza de Ataque Goblin", new[] { "Fuerza de Ataque Goblin", "Goblin Attack Force", "Força de Ataque Goblin", "Force d'Attaque Gobeline", "Goblin-Angriffstruppe", "Forza d'Attacco Goblin" } },
        { "Elfas Gemelas", new[] { "Elfas Gemelas", "Gemini Elf", "Elfas Gêmeas", "Elfes Gémeaux", "Zwillingselfen", "Elfi Gemelli" } },
        { "Santa Juana", new[] { "Santa Juana", "St. Joan", "Santa Joana", "Sainte Jeanne", "Heilige Johanna", "Santa Giovanna" } },
        { "Zoa", new[] { "Zoa", "Zoa", "Zoa", "Zoa", "Zoa", "Zoa" } },
        { "Melchid la Bestia de Cuatro Caras", new[] { "Melchid la Bestia de Cuatro Caras", "Melchid the Four-Face Beast", "Melchid, a Besta de Quatro Faces", "Melchid, la Bête aux Quatre Visages", "Melchid, Bestie mit vier Gesichtern", "Melchid dai 4 Volti" } },
        { "Bickuribox", new[] { "Bickuribox", "Bickuribox", "Bickuribox", "Bickuribox", "Bickuribox", "Bickuribox" } },
        { "Elfa Oscura", new[] { "Elfa Oscura", "Dark Elf", "Elfa Sombria", "Elfe Noire", "Dunkle Elfe", "Elfa Oscura" } },
        { "Tortuga Cangrejo", new[] { "Tortuga Cangrejo", "Crab Turtle", "Tartaruga Caranguejo", "Tortue Crabe", "Krabbenschildkröte", "Tartaruga Granchio" } },
        { "Espada de Garma", new[] { "Espada de Garma", "Garma Sword", "Espada de Garma", "Épée de Garma", "Garma-Schwert", "Spada di Garma" } },
        { "Escarabajo Jabalina", new[] { "Escarabajo Jabalina", "Javelin Beetle", "Besouro Dardo", "Scarabée Javelot", "Speerkäfer", "Scarabeo Giavellotto" } },
        { "Tanque Laberinto", new[] { "Tanque Laberinto", "Labyrinth Tank", "Tanque do Labirinto", "Tank du Labyrinthe", "Labyrinth-Panzer", "Carro Armato Labirinto" } },
        { "Ballena Fortaleza", new[] { "Ballena Fortaleza", "Fortress Whale", "Baleia Fortaleza", "Baleine Forteresse", "Festungswal", "Balena Fortezza" } },
        { "Dragón Nocturno Serpiente", new[] { "Dragón Nocturno Serpiente", "Serpent Night Dragon", "Dragão Noturno Serpente", "Dragon Serpent de la Nuit", "Schlangennachtdrache", "Drago Serpente della Notte" } },
        { "Chakra", new[] { "Chakra", "Chakra", "Chakra", "Chakra", "Chakra", "Chakra" } },
        { "Fiend Megacyber", new[] { "Fiend Megacyber", "The Fiend Megacyber", "O Demônio Megaciber", "Le Démon Mégacyber", "Der Unhold Megacyber", "Il Demone Megacyber" } },
        { "Bicho Devorador de Hombres", new[] { "Bicho Devorador de Hombres", "Man-Eater Bug", "Inseto Devorador de Homens", "Insecte Mangeur d'Hommes", "Menschenfresser-Insekt", "Insetto Mangiauomini" } },
        { "Jarra Cibernética", new[] { "Jarra Cibernética", "Cyber Jar", "Jarro Cibernético", "Cyber Jarre", "Cyber-Krug", "Vaso Cyber" } },
        { "Parshath Caballero Aéreo", new[] { "Parshath Caballero Aéreo", "Airknight Parshath", "Parshath, o Cavaleiro Aéreo", "Parshath, Chevalier Aérien", "Luftritter Parshath", "Parshath Cavaliere dell'Aria" } },
        { "Soldado Pingüino", new[] { "Soldado Pingüino", "Penguin Soldier", "Soldado Pinguim", "Soldat Pingouin", "Pinguinsoldat", "Soldato Pinguino" } },
        { "Jarra Cambiante", new[] { "Jarra Cambiante", "Morphing Jar", "Jarro Mutante", "Jarre Morphing", "Morphender Krug", "Vaso Mutante" } },
        { "Muka Muka", new[] { "Muka Muka", "Muka Muka", "Muka Muka", "Muka Muka", "Muka Muka", "Muka Muka" } },
        { "Soldado Gigante de Piedra", new[] { "Soldado Gigante de Piedra", "Giant Soldier of Stone", "Soldado Gigante de Pedra", "Soldat Géant de Pierre", "Riesensoldat aus Stein", "Soldato Gigante di Pietra" } },
        { "Muro Laberinto", new[] { "Muro Laberinto", "Labyrinth Wall", "Parede do Labirinto", "Mur du Labyrinthe", "Labyrinthwand", "Muro Labirinto" } },
        { "Ninja Armado", new[] { "Ninja Armado", "Armed Ninja", "Ninja Armado", "Ninja Armé", "Bewaffneter Ninja", "Ninja Armato" } },
        { "Aprendiz de Bruja", new[] { "Aprendiz de Bruja", "Witch's Apprentice", "Aprendiz de Bruxa", "Apprentie Sorcière", "Hexenlehrling", "Apprendista Strega" } },
        { "Gilasaurus", new[] { "Gilasaurus", "Gilasaurus", "Gilasaurus", "Gilasaurus", "Gilasaurus", "Gilasaurus" } },
        { "Battleguard de Lava", new[] { "Battleguard de Lava", "Lava Battleguard", "Guarda de Batalha de Lava", "Gardebataille de Lave", "Lava-Kampfwache", "Guardia di Lava" } },
        { "Seiyaryu", new[] { "Seiyaryu", "Seiyaryu", "Seiyaryu", "Seiyaryu", "Seiyaryu", "Seiyaryu" } },
        { "Guerrero Pantera", new[] { "Guerrero Pantera", "Panther Warrior", "Guerreiro Pantera", "Guerrier Panthère", "Pantherkrieger", "Guerriero Pantera" } },
        { "Jirai Gumo", new[] { "Jirai Gumo", "Jirai Gumo", "Jirai Gumo", "Jirai Gumo", "Jirai Gumo", "Jirai Gumo" } },
        { "Mago Oscuro II", new[] { "Mago Oscuro II", "Dark Magician II", "Mago Negro II", "Magicien Sombre II", "Dunkler Magier II", "Mago Nero II" } },
        { "Buster Blader", new[] { "Buster Blader", "Buster Blader", "Buster Blader", "Buster Blader", "Buster Blader", "Buster Blader" } },
        { "Sigue los FPS", new[] { "Sigue los FPS", "Follows the FPS", "Segue os FPS", "Suit les FPS",
            "Folgt den FPS", "Segue gli FPS" } },
        { "Normal", new[] { "Normal", "Normal", "Normal", "Normale", "Normal", "Normale" } },
        { "Sí (PC gama baja)", new[] { "Sí (PC gama baja)", "Yes (low-end PC)", "Sim (PC mais fraco)",
            "Oui (PC modeste)", "Ja (schwacher PC)", "Sì (PC poco potente)" } },
    };

    public static string S(string id)
    {
        string[] t;
        return T.TryGetValue(id, out t) ? t[Lang] : id;
    }

    public static string Value(string stored)
    {
        string[] t;
        return V.TryGetValue(stored, out t) ? t[Lang] : stored;
    }
}

class Launcher : Form
{
    // Yu-Gi-Oh!: night blue, millennium gold
    static readonly Color Bg = Color.FromArgb(16, 18, 34);
    static readonly Color Panel = Color.FromArgb(30, 33, 60);
    static readonly Color Paper = Color.FromArgb(236, 230, 210);
    static readonly Color Muted = Color.FromArgb(150, 150, 180);
    static readonly Color Accent = Color.FromArgb(212, 168, 60);
    static readonly Color Ok = Color.FromArgb(120, 200, 120);
    static readonly Color Bad = Color.FromArgb(230, 90, 70);

    readonly string root = AppDomain.CurrentDomain.BaseDirectory.TrimEnd('\\');
    string AjustesDir { get { return Path.Combine(root, "Ajustes"); } }
    string Ini { get { return Path.Combine(AjustesDir, "ygo_menu.ini"); } }
    string LangIni { get { return Path.Combine(AjustesDir, "launcher.ini"); } }

    readonly List<KeyValuePair<string, Opt[]>> groups = new List<KeyValuePair<string, Opt[]>>();
    readonly ToolTip tips = new ToolTip { AutoPopDelay = 15000, InitialDelay = 300 };
    Label stExe, stTex, stVoz, stIni, msg;
    string perfil;                   // the profile played (Perfiles.cs)
    Button playBtn;
    readonly Panel body = new Panel { Dock = DockStyle.Fill, AutoScroll = true, AutoScrollMargin = new Size(24, 12) };
    readonly Panel footer = new Panel { Dock = DockStyle.Bottom, Height = 94 };
    FileSystemWatcher watcher;
    bool loading;                    // the boxes being set from the file: no save
    string lastWritten;              // what this program wrote last (its own change is not news)

    // ---------- quality profiles (2026-11-21) ----------
    // What each profile sets, in the F2 menu's own values. Window, full
    // screen, VSync, language, controls: never touched by a profile.
    static readonly string[] PerfKeys = { "render_scale", "aniso", "fps_juego", "pack_art", "mascotas" };
    static readonly Dictionary<string, string[]> Perf = new Dictionary<string, string[]> {
        { "Bajo",  new[] { "x1 (640x480)",   "No",  "60",      "No", "No" } },
        { "Medio", new[] { "x2 (1280x960)",  "x4",  "60",      "Sí", "Sí" } },
        { "Alto",  new[] { "x3 (1920x1440)", "x8",  "120",     "Sí", "Sí" } },
        { "Ultra", new[] { "x4 (2560x1920)", "x16", "Monitor", "Sí", "Sí" } },
    };
    Label stPc;
    string pcTier = "Medio", pcGpu = "?";
    int pcThreads;
    double pcRamGb, pcVramGb;

    [StructLayout(LayoutKind.Sequential)]
    class MemStatus
    {
        public uint dwLength = 64, dwMemoryLoad;
        public ulong ullTotalPhys, ullAvailPhys, ullTotalPageFile, ullAvailPageFile,
                     ullTotalVirtual, ullAvailVirtual, ullAvailExtendedVirtual;
    }
    [DllImport("kernel32.dll")] static extern bool GlobalMemoryStatusEx([In, Out] MemStatus m);

    // The PC: hardware threads, RAM, and the graphics card with the most
    // memory of its own (the display class's HardwareInformation.qwMemorySize,
    // which is right past 4 GB, unlike WMI's AdapterRAM). Then the profile.
    void Detect()
    {
        pcThreads = Environment.ProcessorCount;
        try { var m = new MemStatus(); if (GlobalMemoryStatusEx(m)) pcRamGb = m.ullTotalPhys / 1073741824.0; } catch { }
        try {
            using (var cls = Microsoft.Win32.Registry.LocalMachine.OpenSubKey(
                       @"SYSTEM\CurrentControlSet\Control\Class\{4d36e968-e325-11ce-bfc1-08002be10318}")) {
                if (cls != null)
                    foreach (var n in cls.GetSubKeyNames()) {
                        try {
                            using (var k = cls.OpenSubKey(n)) {
                                if (k == null) continue;
                                double gb = 0;
                                object q = k.GetValue("HardwareInformation.qwMemorySize");
                                object d = k.GetValue("HardwareInformation.MemorySize");
                                if (q is long) gb = (long)q / 1073741824.0;
                                else if (q is byte[] && ((byte[])q).Length >= 8) gb = BitConverter.ToInt64((byte[])q, 0) / 1073741824.0;
                                else if (d is int) gb = (uint)(int)d / 1073741824.0;
                                else if (d is byte[] && ((byte[])d).Length >= 4) gb = BitConverter.ToUInt32((byte[])d, 0) / 1073741824.0;
                                if (gb > pcVramGb) {
                                    pcVramGb = gb;
                                    pcGpu = (k.GetValue("DriverDesc") as string) ?? "?";
                                }
                            }
                        } catch { }
                    }
            }
        } catch { }
        if (pcThreads >= 8 && pcRamGb >= 15 && pcVramGb >= 6) pcTier = "Ultra";
        else if (pcThreads >= 6 && pcRamGb >= 11 && pcVramGb >= 3.5) pcTier = "Alto";
        else if (pcThreads >= 4 && pcRamGb >= 5.5 && pcVramGb >= 1.5) pcTier = "Medio";
        else pcTier = "Bajo";
    }

    Opt Find(string key) { return All().FirstOrDefault(o => o.Key == key); }
    string Cur(Opt o) { return o.Choices[Math.Max(0, o.Box.SelectedIndex)]; }

    // the profile in force: Recomendado is the PC's; Personalizado none
    string Tier(string perfil) { return perfil == "Recomendado" ? pcTier : Perf.ContainsKey(perfil) ? perfil : null; }

    bool Matches(string tier)
    {
        if (tier == null) return false;
        for (int i = 0; i < PerfKeys.Length; i++) {
            var o = Find(PerfKeys[i]);
            if (o == null || Cur(o) != Perf[tier][i]) return false;
        }
        return true;
    }

    // the chosen profile's values into their boxes (no save here)
    void ApplyProfile()
    {
        var tier = Tier(Cur(Find("perfil")));
        if (tier == null) return;
        bool was = loading;
        loading = true;
        for (int i = 0; i < PerfKeys.Length; i++) {
            var o = Find(PerfKeys[i]);
            int k = o == null ? -1 : Array.IndexOf(o.Choices, Perf[tier][i]);
            if (k >= 0) o.Box.SelectedIndex = k;
        }
        loading = was;
    }

    // a profile whose values no longer hold becomes Personalizado; true if it changed
    bool CheckCustom()
    {
        var p = Find("perfil");
        if (p == null || Cur(p) == "Personalizado" || Matches(Tier(Cur(p)))) return false;
        bool was = loading;
        loading = true;
        p.Box.SelectedIndex = Array.IndexOf(p.Choices, "Personalizado");
        loading = was;
        return true;
    }

    public Launcher()
    {
        groups.Add(new KeyValuePair<string, Opt[]>("g_image", new[] {
            new Opt("perfil", "Recomendado", "Recomendado", "Bajo", "Medio", "Alto", "Ultra", "Personalizado"),
            new Opt("render_scale", "x2 (1280x960)", "x1 (640x480)", "x2 (1280x960)", "x3 (1920x1440)", "x4 (2560x1920)"),
            new Opt("widescreen", "No (4:3)", "No (4:3)", "Estirar a 16:9"),
            new Opt("window", "Libre",
                "Libre", "640x480", "800x600", "960x720", "1024x768", "1280x720", "1280x800", "1280x960",
                "1280x1024", "1366x768", "1440x900", "1440x1080", "1600x900", "1600x1200", "1680x1050",
                "1920x1080", "1920x1200", "2560x1080", "2560x1440", "2560x1600", "3440x1440", "3840x1600",
                "3840x2160", "5120x1440", "5120x2160"),
            new Opt("fullscreen", "No", "No", "Sí"),
            new Opt("aniso", "x16", "No", "x2", "x4", "x8", "x16"),
            new Opt("vsync", "No", "No", "Sí"),
            new Opt("fps_juego", "60", "60", "90", "120", "144", "165", "200", "Monitor"),
            new Opt("fps", "No", "No", "Sí"),
        }));
        groups.Add(new KeyValuePair<string, Opt[]>("g_controls", new[] {
            new Opt("raton_sensibilidad", "Media", "Baja", "Media", "Alta"),
            new Opt("raton_guante_imagen", "No", "No", "Sí"),
            new Opt("pack_art", "Sí", "No", "Sí"),
        }));
        groups.Add(new KeyValuePair<string, Opt[]>("g_game", new[] {
            new Opt("language", "Español", "English", "Español", "Français", "Deutsch", "Italiano"),
            new Opt("voces_velocidad", "Sigue los FPS", "Sigue los FPS", "Normal", "No"),
            new Opt("rival_rapido", "No", "No", "x2", "x4"),
            new Opt("mascotas", "Sí", "Sí", "No"),
            // 2026-11-20: entrada_duelo, carta_as, numero_danio, auto_cadena, mascota_tuya, funda,
            // funda_marco, tapete, tapete_efecto and cartas_nuevas hidden here (the user); the
            // ini keeps them (Save writes only these keys), the game and the Perfil set them
            new Opt("ayudas", "Sí, con fotos", "Sí, con fotos", "Sí, sin fotos", "No"),   // 2026-11-25: Ayudas
            new Opt("cartas_nuevas", "Sí", "Sí", "No"),   // 2026-11-26: shown again as "Cartas agregadas" (the user)
        }));
        try { Directory.CreateDirectory(AjustesDir); } catch { }
        MoveOldIni();
        Tr.Lang = LoadLang();
        Detect();
        Setup();
        BuildContent();
        LoadIni(false);
        Refresh_();
        Watch();
    }

    // The settings of a version before Ajustes\ (ygo-apex\ygo_menu.ini), taken once.
    void MoveOldIni()
    {
        try {
            var old = Path.Combine(root, "ygo-apex", "ygo_menu.ini");
            if (!File.Exists(Ini) && File.Exists(old)) File.Copy(old, Ini);
        } catch { }
    }

    // ---------- launcher language (Ajustes\launcher.ini, shared with F2) ----------
    int LoadLang()
    {
        try {
            if (File.Exists(LangIni))
                foreach (var line in File.ReadAllLines(LangIni, Encoding.UTF8))
                    if (line.StartsWith("idioma=")) {
                        int i = Array.IndexOf(Tr.Codes, line.Substring(7).Trim());
                        if (i == PT) return 0;
                        if (i >= 0) return i;
                    }
        } catch { }
        int w = Array.IndexOf(Tr.Codes, CultureInfo.CurrentUICulture.TwoLetterISOLanguageName);
        return w == PT ? 0 : w >= 0 ? w : 1;
    }

    // Portuguese taken out (2026-11-10: the game has none and the voices are
    // Spanish); its texts stay in Tr so the indices do not move. The list
    // shows the others; LangShown[k] is the Tr index of the list's line k.
    const int PT = 2;
    static readonly int[] LangShown = { 0, 1, 3, 4, 5 };

    void SaveLang()
    {
        try { File.WriteAllText(LangIni, "idioma=" + Tr.Codes[Tr.Lang] + "\r\n", new UTF8Encoding(false)); } catch { }
    }

    // ---------- interface ----------
    void Setup()
    {
        BackColor = Bg;
        ForeColor = Paper;
        Font = new Font("Segoe UI", 10f);
        FormBorderStyle = FormBorderStyle.Sizable;
        MaximizeBox = false;
        StartPosition = FormStartPosition.CenterScreen;
        AutoScaleMode = AutoScaleMode.Dpi;
        Controls.Add(body);
        Controls.Add(footer);
        Load += delegate { FitScreen(); };
        try { Icon = Icon.ExtractAssociatedIcon(Application.ExecutablePath); } catch { }
    }

    void BuildContent()
    {
        var keep = new Dictionary<string, int>();
        foreach (var o in All())
            if (o.Box != null) keep[o.Key] = o.Box.SelectedIndex;
        int scroll = body.VerticalScroll.Value;
        loading = true;
        body.SuspendLayout();
        footer.SuspendLayout();
        tips.RemoveAll();
        body.Controls.Clear();
        footer.Controls.Clear();
        body.AutoScrollPosition = new Point(0, 0);

        Text = Tr.S("title");
        var title = new Label {
            Text = "YU-GI-OH!", Font = new Font("Georgia", 26f, FontStyle.Bold), ForeColor = Accent,
            AutoSize = true, Location = new Point(24, 10) };
        var title2 = new Label {
            Text = "THE DAWN OF DESTINY", Font = new Font("Georgia", 13f, FontStyle.Bold), ForeColor = Paper,
            AutoSize = true, Location = new Point(28, 58) };
        var sub = new Label {
            Text = Tr.S("subtitle"), Font = new Font("Segoe UI", 9.5f), ForeColor = Muted,
            AutoSize = false, AutoEllipsis = true, Location = new Point(28, 86), Size = new Size(808, 22) };
        var bar = new Panel { BackColor = Accent, Location = new Point(24, 112), Size = new Size(812, 3) };
        var langBox = new ComboBox {
            DropDownStyle = ComboBoxStyle.DropDownList, Location = new Point(686, 24), Width = 150,
            BackColor = Bg, ForeColor = Paper, FlatStyle = FlatStyle.Flat };
        foreach (int k in LangShown)
            langBox.Items.Add(Tr.Names[k]);
        langBox.SelectedIndex = Math.Max(0, Array.IndexOf(LangShown, Tr.Lang));
        var langLbl = new Label {
            Text = Tr.S("lang"), ForeColor = Muted, AutoSize = false, TextAlign = ContentAlignment.MiddleRight,
            Location = new Point(560, 26), Size = new Size(122, 24) };
        langBox.SelectedIndexChanged += delegate {
            if (langBox.SelectedIndex < 0 || LangShown[langBox.SelectedIndex] == Tr.Lang) return;
            Tr.Lang = LangShown[langBox.SelectedIndex];
            SaveLang();
            BeginInvoke((MethodInvoker)delegate { BuildContent(); Refresh_(); });
        };
        // the version (version.txt, from VERSION by armar_paquete.ps1), under the language
        var ver = new Label {
            Text = string.Format(Actualizar.S("version"), Actualizar.Local(root)), ForeColor = Muted,
            AutoSize = false, TextAlign = ContentAlignment.MiddleRight,
            Location = new Point(636, 56), Size = new Size(200, 24) };
        body.Controls.AddRange(new Control[] { title, title2, sub, bar, langLbl, langBox, ver });

        // 2026-11-25: the profile (Perfiles.cs), above everything
        perfil = Perfiles.Asegurar(root);
        // 2026-11-25: the game closed on an error (Respaldos.cs): a notice on top
        int off = 0;
        string err = Respaldos.ErrorPendiente(AjustesDir);
        if (err != null) {
            var ep = new Panel { BackColor = Color.FromArgb(74, 28, 20), Location = new Point(24, 128), Size = new Size(812, 44) };
            ep.Paint += (s, e) => { using (var pen = new Pen(Bad, 2)) e.Graphics.DrawRectangle(pen, 1, 1, ep.Width - 2, ep.Height - 2); };
            var el = new Label { Text = string.Format(Tr.S("err_aviso"), err), AutoSize = false, AutoEllipsis = true,
                                 Location = new Point(12, 12), Size = new Size(540, 22), ForeColor = Paper };
            var eb = Btn(Tr.S("err_armar"), 560, 7, 130, 30, true);
            eb.Font = new Font("Segoe UI", 9.5f, FontStyle.Bold);
            var ed = Btn(Tr.S("err_descartar"), 696, 7, 104, 30, false);
            eb.Click += delegate { Informe(); };
            ed.Click += delegate { Respaldos.Descartar(AjustesDir); BuildContent(); };
            ep.Controls.AddRange(new Control[] { el, eb, ed });
            body.Controls.Add(ep);
            off = 54;
        }
        var pf = Box(Tr.S("g_perfil"), 24, 128 + off, 812, 66);
        pf.Paint += (s, e) => { using (var pen = new Pen(Accent, 2)) e.Graphics.DrawRectangle(pen, 1, 1, pf.Width - 2, pf.Height - 2); };
        var pcb = new ComboBox { DropDownStyle = ComboBoxStyle.DropDownList, Location = new Point(12, 30), Width = 180,
                                 BackColor = Bg, ForeColor = Paper, FlatStyle = FlatStyle.Flat };
        foreach (var n in Perfiles.Lista(root)) pcb.Items.Add(n);
        pcb.SelectedItem = perfil;
        var pNew = Btn(Tr.S("perf_nuevo"), 200, 28, 124, 30, false);
        var pDir = Btn(Tr.S("perf_carpeta"), 330, 28, 136, 30, false);
        var pResp = Btn(Tr.S("perf_respaldos"), 472, 28, 110, 30, false);
        pResp.FlatAppearance.BorderColor = Accent;
        var pRes = new Label { Text = Perfiles.Resumen(root, perfil), ForeColor = Muted, AutoSize = false, AutoEllipsis = true,
                               Location = new Point(590, 34), Size = new Size(212, 22) };
        tips.SetToolTip(pRes, pRes.Text);
        pResp.Click += delegate { Respaldos.Mostrar(this, root, perfil, Bg, Panel, Paper, Muted, Accent, Ok, Bad); };
        pcb.SelectedIndexChanged += delegate {
            if (loading || pcb.SelectedItem == null) return;
            perfil = (string)pcb.SelectedItem;
            Perfiles.SetActivo(root, perfil);
            pRes.Text = Perfiles.Resumen(root, perfil);
            tips.SetToolTip(pRes, pRes.Text);
        };
        pNew.Click += delegate {
            var n = Perfiles.PedirNombre(this, root, Bg, Panel, Paper, Muted, Accent);
            if (n == null) return;
            Perfiles.SetActivo(root, n);
            BuildContent();
        };
        pDir.Click += delegate { OpenDir(Path.Combine(Perfiles.Dir(root), perfil)); };
        pf.Controls.AddRange(new Control[] { pcb, pNew, pDir, pResp, pRes });

        var st = Box(Tr.S("g_status"), 24, 206 + off, 812, 177);
        stExe = Status(st, 0); stTex = Status(st, 1); stVoz = Status(st, 2); stIni = Status(st, 3);
        stPc = Status(st, 4);

        int y = 397 + off, colW = 400;
        var left = Box(Tr.S(groups[0].Key), 24, y, colW, 34 + 36 * groups[0].Value.Length + 8);
        AddOpts(left, groups[0].Value);
        var right = Box(Tr.S(groups[1].Key), 24 + colW + 12, y, colW, 34 + 36 * groups[1].Value.Length + 8);
        AddOpts(right, groups[1].Value);
        var game = Box(Tr.S(groups[2].Key), 24 + colW + 12, right.Bottom + 12, colW, 34 + 36 * groups[2].Value.Length + 8);
        AddOpts(game, groups[2].Value);
        int contentBottom = Math.Max(left.Bottom, game.Bottom);

        int by = 12;
        var play = Btn(Tr.S("play"), 24, by, 180, 46, true);
        playBtn = play;
        play.Click += delegate { Play(); };
        var defs = Btn(Tr.S("defaults"), 212, by, 150, 46, false);
        defs.Click += delegate { Defaults(); };
        var fAj = Btn(Tr.S("open_ajustes"), 686, by, 150, 46, false);
        fAj.Click += delegate { OpenDir(AjustesDir); };
        // 2026-11-25: the error report, at any time (Respaldos.cs)
        var inf = Btn(Tr.S("err_boton"), 528, by, 150, 46, false);
        tips.SetToolTip(inf, Tr.S("err_boton.tip"));
        inf.Click += delegate { Informe(); };
        // look for a new version on GitHub (Actualizar.cs)
        var upd = Btn(Actualizar.S("button"), 370, by, 150, 46, false);
        tips.SetToolTip(upd, Actualizar.S("button.tip"));
        msg = new Label { AutoSize = false, Location = new Point(24, by + 54), Size = new Size(812, 24), ForeColor = Muted };
        upd.Click += delegate { Actualizar.Run(this, root, upd, msg); };
        footer.Controls.AddRange(new Control[] { play, defs, upd, inf, fAj, msg });
        if (keep.Count == 0 && Environment.GetCommandLineArgs().Contains("--actualizado"))
            msg.Text = string.Format(Actualizar.S("updated"), Actualizar.Local(root));

        foreach (var o in All()) {
            int k;
            if (keep.TryGetValue(o.Key, out k) && k >= 0) o.Box.SelectedIndex = k;
        }
        body.ResumeLayout();
        footer.ResumeLayout();
        loading = false;
        if (keep.Count > 0)
            body.AutoScrollPosition = new Point(0, scroll);
        else {
            ClientSize = new Size(860, contentBottom + 16 + footer.Height);
            MinimumSize = new Size(560, 360);
        }
    }

    void FitScreen()
    {
        var wa = Screen.FromPoint(Cursor.Position).WorkingArea;
        int w = Math.Min(Width, wa.Width), h = Math.Min(Height, wa.Height);
        if (w == Width && h == Height) return;
        Size = new Size(w, h);
        Location = new Point(wa.Left + (wa.Width - w) / 2, wa.Top + (wa.Height - h) / 2);
    }

    Panel Box(string caption, int x, int y, int w, int h)
    {
        var p = new Panel { BackColor = Panel, Location = new Point(x, y), Size = new Size(w, h) };
        p.Controls.Add(new Label {
            Text = caption.ToUpper(), Font = new Font("Segoe UI", 9f, FontStyle.Bold), ForeColor = Accent,
            AutoSize = true, Location = new Point(12, 8) });
        body.Controls.Add(p);
        return p;
    }

    Label Status(Panel p, int row)
    {
        var l = new Label { AutoSize = false, AutoEllipsis = true,
                            Location = new Point(14, 34 + row * 27), Size = new Size(p.Width - 28, 25) };
        p.Controls.Add(l);
        return l;
    }

    void AddOpts(Panel p, Opt[] opts)
    {
        int y = 34;
        foreach (var o in opts) {
            var l = new Label { Text = Tr.S(o.Key), AutoSize = false, AutoEllipsis = true,
                                Location = new Point(12, y + 3), Size = new Size(190, 24) };
            o.Box = new ComboBox {
                DropDownStyle = ComboBoxStyle.DropDownList, Location = new Point(204, y), Width = p.Width - 216,
                BackColor = Bg, ForeColor = Paper, FlatStyle = FlatStyle.Flat };
            o.Box.Items.AddRange(o.Choices.Select(c => (object)Tr.Value(c)).ToArray());
            o.Box.SelectedIndexChanged += delegate {
                if (loading) return;
                if (o.Key == "perfil") ApplyProfile();
                else if (PerfKeys.Contains(o.Key)) CheckCustom();
                if (Save()) msg.Text = Tr.S("saved");
                if (o.Key == "language") Refresh_();
            };
            string tip = Tr.S(o.Key) + "\n" + Tr.S(o.Key + ".tip");
            tips.SetToolTip(l, tip);
            tips.SetToolTip(o.Box, tip);
            p.Controls.Add(l);
            p.Controls.Add(o.Box);
            y += 36;
        }
    }

    Button Btn(string text, int x, int y, int w, int h, bool main)
    {
        var b = new Button {
            Text = text, Location = new Point(x, y), Size = new Size(w, h), FlatStyle = FlatStyle.Flat,
            BackColor = main ? Accent : Panel, ForeColor = main ? Bg : Paper, AutoEllipsis = true,
            Font = new Font("Segoe UI", main ? 12f : 9.5f, main ? FontStyle.Bold : FontStyle.Regular) };
        b.FlatAppearance.BorderColor = main ? Accent : Muted;
        return b;
    }

    // ---------- status ----------
    void Set(Label l, Color c, string text) { l.ForeColor = c; l.Text = text; tips.SetToolTip(l, text); }

    // The game: Yu-Gi-Oh.exe in the port's folder (the build copies it there).
    string RunBat()
    {
        var game = Path.Combine(root, "Yu-Gi-Oh.exe");
        return File.Exists(game) ? game : null;
    }

    void Refresh_()
    {
        var bat = RunBat();
        Set(stExe, bat != null ? Ok : Bad, bat != null ? string.Format(Tr.S("st_exe_ok"), Short(bat)) : Tr.S("st_exe_no"));
        // the player's folder keeps the extras in Mods-Extras\ (any subfolder,
        // as the game looks); the developer's in modding\
        var pack = FindIn(Path.Combine(root, "Mods-Extras"), "textures_bc3.pack")
                   ?? FindIn(Path.Combine(root, "Mods-Extras"), "textures.pack")
                   ?? Path.Combine(root, "modding", "texturas", "textures_bc3.pack");
        if (File.Exists(pack))
            Set(stTex, Ok, string.Format(Tr.S("st_tex_ok"), new FileInfo(pack).Length / 1073741824.0));
        else
            Set(stTex, Muted, Tr.S("st_tex_no"));
        int n = 0;
        try {
            var t = FindIn(Path.Combine(root, "Mods-Extras"), "textos.txt");
            var v = t != null ? Path.GetDirectoryName(t) : Path.Combine(root, "modding", "voces_latino");
            if (Directory.Exists(v)) n = Directory.GetFiles(v, "*.mp3").Length;
        } catch { }
        Set(stVoz, n > 0 ? Ok : Muted, n > 0 ? string.Format(Tr.S("st_voz_ok"), n) : Tr.S("st_voz_no"));
        Set(stIni, Ok, string.Format(Tr.S("st_ajustes"), Short(Ini)));
        bool es = true;
        foreach (var o in All())
            if (o.Key == "language" && o.Box != null && o.Box.SelectedIndex >= 0)
                es = o.Choices[o.Box.SelectedIndex] == "Español";
        stVoz.Visible = es;
        stIni.Top = 34 + (es ? 3 : 2) * 27;
        stPc.Top = stIni.Top + 27;
        Set(stPc, Ok, string.Format(Tr.S("st_pc"), pcThreads, Math.Round(pcRamGb), pcGpu,
                                    Math.Round(pcVramGb, 1), Tr.Value(pcTier)));
    }

    // The first file called name in dir or its subfolders (4 levels), or null.
    static string FindIn(string dir, string name)
    {
        try {
            if (!Directory.Exists(dir)) return null;
            var f = Path.Combine(dir, name);
            if (File.Exists(f)) return f;
            return FindDeep(dir, name, 4);
        } catch { return null; }
    }

    static string FindDeep(string dir, string name, int depth)
    {
        if (depth <= 0) return null;
        foreach (var d in Directory.GetDirectories(dir)) {
            if (Path.GetFileName(d).Equals("dump", StringComparison.OrdinalIgnoreCase)) continue;
            var f = Path.Combine(d, name);
            if (File.Exists(f)) return f;
            var r = FindDeep(d, name, depth - 1);
            if (r != null) return r;
        }
        return null;
    }

    string Short(string p) { return p.StartsWith(root) ? p.Substring(root.Length).TrimStart('\\') : p; }

    // ---------- Ajustes\ygo_menu.ini ----------
    IEnumerable<Opt> All() { return groups.SelectMany(g => g.Value); }

    Dictionary<string, string> ReadIni()
    {
        var d = new Dictionary<string, string>();
        for (int tries = 0; tries < 5; tries++) {
            try {
                if (!File.Exists(Ini)) return d;
                using (var fs = new FileStream(Ini, FileMode.Open, FileAccess.Read, FileShare.ReadWrite | FileShare.Delete))
                using (var r = new StreamReader(fs, Encoding.UTF8)) {
                    string line;
                    while ((line = r.ReadLine()) != null) {
                        int i = line.IndexOf('=');
                        if (i > 0 && !line.StartsWith("#")) d[line.Substring(0, i).Trim()] = line.Substring(i + 1).Trim();
                    }
                }
                return d;
            } catch (IOException) {
                System.Threading.Thread.Sleep(40);    // the game writing it
            }
        }
        return d;
    }

    // live: a change from the game; only the keys the file has are taken
    // (the game may be half way through writing it).
    void LoadIni(bool live)
    {
        var d = ReadIni();
        bool changed = false;
        loading = true;
        foreach (var o in All()) {
            string v;
            int k = d.TryGetValue(o.Key, out v) ? Array.IndexOf(o.Choices, v) : -1;
            if (k < 0) {
                if (live) continue;
                k = Array.IndexOf(o.Choices, o.Def);
            }
            if (o.Box.SelectedIndex != k) { o.Box.SelectedIndex = k; changed = true; }
        }
        loading = false;
        if (!live && !d.ContainsKey("perfil")) {
            // no profile saved yet: a first start (no settings) takes the
            // recommended one; settings from before the profiles stay as
            // they are (Recomendado if they match it, else Personalizado)
            if (!d.ContainsKey("render_scale")) {
                ApplyProfile();
                Save();
            } else if (CheckCustom()) {
                Save();
            }
        } else if (CheckCustom()) {
            Save();                         /* F2 changed a profile's value */
        }
        if (live && changed) { msg.Text = Tr.S("from_game"); Refresh_(); }
    }

    void Defaults()
    {
        loading = true;
        foreach (var o in All()) o.Box.SelectedIndex = Array.IndexOf(o.Choices, o.Def);
        ApplyProfile();                     /* the default profile is Recomendado */
        loading = false;
        if (Save()) msg.Text = Tr.S("defaults_msg");
    }

    bool Save()
    {
        try {
            Directory.CreateDirectory(AjustesDir);
            var lines = new List<string>();
            try {
                if (File.Exists(Ini)) lines = File.ReadAllLines(Ini, Encoding.UTF8).ToList();
            } catch (IOException) { }
            if (lines.Count == 0)
                lines.Add("# Ajustes del menú F2 (se reescribe solo). Configuración.exe también los cambia.");
            foreach (var o in All()) {
                string val = o.Choices[Math.Max(0, o.Box.SelectedIndex)];
                int at = lines.FindIndex(l => l.StartsWith(o.Key + "="));
                if (at >= 0) lines[at] = o.Key + "=" + val; else lines.Add(o.Key + "=" + val);
            }
            var text = string.Join("\n", lines) + "\n";
            // written beside, then put in place: the game never reads half a file
            var tmp = Ini + ".tmp";
            File.WriteAllText(tmp, text, new UTF8Encoding(false));
            lastWritten = text;
            if (File.Exists(Ini)) File.Replace(tmp, Ini, null); else File.Move(tmp, Ini);
            return true;
        } catch (Exception e) {
            MessageBox.Show(this, Tr.S("err_save") + e.Message, Text, MessageBoxButtons.OK, MessageBoxIcon.Warning);
            return false;
        }
    }

    // F2's changes, shown here as they happen.
    void Watch()
    {
        try {
            watcher = new FileSystemWatcher(AjustesDir, "ygo_menu.ini") {
                NotifyFilter = NotifyFilters.LastWrite | NotifyFilters.FileName | NotifyFilters.Size };
            FileSystemEventHandler h = delegate {
                BeginInvoke((MethodInvoker)delegate {
                    string now = null;
                    try {
                        using (var fs = new FileStream(Ini, FileMode.Open, FileAccess.Read, FileShare.ReadWrite | FileShare.Delete))
                        using (var r = new StreamReader(fs, Encoding.UTF8)) now = r.ReadToEnd().Replace("\r", "");
                    } catch { return; }
                    if (now == lastWritten) return;       // our own save
                    LoadIni(true);
                });
            };
            watcher.Changed += h;
            watcher.Created += h;
            watcher.Renamed += delegate(object s, RenamedEventArgs e) { h(s, e); };
            watcher.EnableRaisingEvents = true;
        } catch { }
    }

    // ---------- actions ----------
    // Yu-Gi-Oh.exe, the game itself, as Red Dead's launcher opens its exe: it
    // reads env.bat and writes its log by itself (main.c). No program in
    // between: a small one starting the game hidden was taken for malware.
    void Play()
    {
        var game = RunBat();
        if (game == null) { Refresh_(); MessageBox.Show(this, Tr.S("st_exe_no"), Text); return; }
        if (Process.GetProcessesByName(Path.GetFileNameWithoutExtension(game)).Length > 0) {
            msg.Text = Tr.S("started");
            return;
        }
        try {
            Process.Start(new ProcessStartInfo(game) { WorkingDirectory = root, UseShellExecute = true });
            msg.Text = Tr.S("started");
        } catch (Exception e) {
            MessageBox.Show(this, Tr.S("err_run") + e.Message, Text);
        }
    }

    void Informe()
    {
        // 2026-11-25 (the user): ask first, and say where it goes
        if (MessageBox.Show(this, Tr.S("err_preg"), Tr.S("err_boton"), MessageBoxButtons.YesNo,
                            MessageBoxIcon.Question) != DialogResult.Yes)
            return;
        try {
            string z = Respaldos.ArmarInforme(root, AjustesDir);
            BuildContent();
            msg.ForeColor = Ok;
            msg.Text = string.Format(Tr.S("err_listo"), z);
        } catch (Exception e) {
            msg.ForeColor = Bad;
            msg.Text = Tr.S("err_fallo") + e.Message;
        }
    }

    void OpenDir(string d)
    {
        try { Directory.CreateDirectory(d); Process.Start("explorer.exe", "\"" + d + "\""); } catch { }
    }

    protected override void OnActivated(EventArgs e) { base.OnActivated(e); if (stExe != null) Refresh_(); }
}
