/*
 * f2_idiomas.c - the F2 menu in the player's language.
 *
 * Every row of the F2 menu (host_menu.c's own, main.c's and mouselook.c's)
 * is written in Spanish, and rdr_menu.ini keeps the choices that way: the
 * menu and Configuración.exe both read them back by their text. Only what
 * the panel shows is translated, through xbox_MenuSetTranslator.
 *
 * The language is the launcher's (launcher.ini next to the exe, idioma=xx,
 * written by Configuración.exe's selector); without one, Windows' display
 * language; English for any language not here. Spanish shows the rows as
 * they are.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

void xbox_MenuSetTranslator(const char *(*tr)(const char *utf8));
void xbox_MenuSetLanguageHook(void (*cb)(const char *code));

enum { EN, PT, FR, DE, IT, N_LANG };
static const char *const k_codes[N_LANG] = { "en", "pt", "fr", "de", "it" };
static int s_lang = -1;          /* -1: Spanish, nothing translated */

typedef struct { const char *es; const char *t[N_LANG]; } Text;

static const Text k_texts[] = {
    /* the panel */
    { "AJUSTES", { "SETTINGS", "CONFIGURAÇÕES", "PARAMÈTRES", "EINSTELLUNGEN", "IMPOSTAZIONI" } },
    { "F2 / Esc: cerrar", { "F2 / Esc: close", "F2 / Esc: fechar", "F2 / Échap : fermer",
                            "F2 / Esc: schließen", "F2 / Esc: chiudi" } },
    { "↑↓ elegir   ←→ cambiar   (se guarda solo)",
      { "↑↓ select   ←→ change   (saved automatically)",
        "↑↓ escolher   ←→ mudar   (salva sozinho)",
        "↑↓ choisir   ←→ modifier   (enregistré automatiquement)",
        "↑↓ wählen   ←→ ändern   (wird automatisch gespeichert)",
        "↑↓ scegli   ←→ cambia   (si salva da solo)" } },
    /* F6's notice */
    { "Texturas mejoradas (modernas)", { "Improved textures (modern)", "Texturas melhoradas (modernas)",
        "Textures améliorées (modernes)", "Verbesserte Texturen (modern)", "Texture migliorate (moderne)" } },
    { "Texturas originales (antiguas)", { "Original textures (classic)", "Texturas originais (antigas)",
        "Textures d'origine (anciennes)", "Originaltexturen (klassisch)", "Texture originali (classiche)" } },

    /* sections */
    { "Gráficos", { "Graphics", "Gráficos", "Graphismes", "Grafik", "Grafica" } },
    { "Juego", { "Game", "Jogo", "Jeu", "Spiel", "Gioco" } },
    { "Velocidad", { "Speed", "Velocidade", "Vitesse", "Geschwindigkeit", "Velocità" } },
    { "Texturas", { "Textures", "Texturas", "Textures", "Texturen", "Texture" } },
    { "Rendimiento", { "Performance", "Desempenho", "Performances", "Leistung", "Prestazioni" } },
    { "Ratón", { "Mouse", "Mouse", "Souris", "Maus", "Mouse" } },

    /* rows */
    { "Resolución interna", { "Internal resolution", "Resolução interna", "Résolution interne",
        "Interne Auflösung", "Risoluzione interna" } },
    { "Panorámico (al reiniciar)", { "Widescreen (on restart)", "Widescreen (ao reiniciar)",
        "Écran large (au redémarrage)", "Breitbild (nach Neustart)", "Widescreen (al riavvio)" } },
    { "Tamaño de ventana", { "Window size", "Tamanho da janela", "Taille de la fenêtre", "Fenstergröße",
        "Dimensione finestra" } },
    { "Pantalla completa (F11)", { "Fullscreen (F11)", "Tela cheia (F11)", "Plein écran (F11)", "Vollbild (F11)",
        "Schermo intero (F11)" } },
    { "Filtrado anisotrópico", { "Anisotropic filtering", "Filtragem anisotrópica", "Filtrage anisotrope",
        "Anisotrope Filterung", "Filtro anisotropico" } },
    { "Volcar texturas a carpeta", { "Dump textures to folder", "Exportar texturas para pasta",
        "Extraire les textures", "Texturen in Ordner speichern", "Esporta texture in cartella" } },
    { "VSync (sincronía vertical)", { "VSync (vertical sync)", "VSync (sincronia vertical)",
        "VSync (synchro verticale)", "VSync (vertikale Synchronisation)", "VSync (sincronia verticale)" } },
    { "Videos pregrabados", { "Pre-rendered videos", "Vídeos pré-renderizados", "Vidéos précalculées",
        "Vorgerenderte Videos", "Video pre-renderizzati" } },
    { "Mostrar FPS", { "Show FPS", "Mostrar FPS", "Afficher les FPS", "FPS anzeigen", "Mostra FPS" } },
    { "Cuadros por segundo (F8)", { "Frame rate (F8)", "Quadros por segundo (F8)", "Images par seconde (F8)",
        "Bilder pro Sekunde (F8)", "Fotogrammi al secondo (F8)" } },
    { "Idioma (al reiniciar)", { "Language (on restart)", "Idioma (ao reiniciar)", "Langue (au redémarrage)",
        "Sprache (nach Neustart)", "Lingua (al riavvio)" } },
    { "Texturas mejoradas", { "Improved textures", "Texturas melhoradas", "Textures améliorées",
        "Verbesserte Texturen", "Texture migliorate" } },
    { "Botones en pantalla", { "On-screen buttons", "Botões na tela", "Boutons à l'écran",
        "Tasten am Bildschirm", "Pulsanti a schermo" } },
    { "Modo experimental (al reiniciar)", { "Experimental mode (on restart)", "Modo experimental (ao reiniciar)",
        "Mode expérimental (au redémarrage)", "Experimenteller Modus (nach Neustart)",
        "Modalità sperimentale (al riavvio)" } },
    { "Onda al hacer clic", { "Ripple on click", "Onda ao clicar", "Onde au clic", "Welle beim Klicken",
        "Onda al clic" } },
    { "Cartel de premios del rival", { "Opponent rewards sign", "Placa de prêmios do rival",
        "Panneau des récompenses", "Belohnungsanzeige des Gegners", "Cartello premi dell'avversario" } },
    { "Quién da cada carta (Biblioteca)", { "Who gives each card (Library)", "Quem dá cada carta (Biblioteca)",
        "Qui donne chaque carte (Bibliothèque)", "Wer jede Karte gibt (Bibliothek)",
        "Chi dà ogni carta (Biblioteca)" } },
    { "Premios sin cuarta copia", { "No 4th copy rewards", "Prêmios sem 4ª cópia",
        "Récompenses sans 4e exemplaire", "Belohnungen ohne 4. Kopie", "Premi senza 4ª copia" } },
    { "AUTO en el duelo (no preguntar encadenar)", { "AUTO in duels (no chain prompt)",
        "AUTO no duelo (não perguntar encadear)", "AUTO en duel (sans demande d'enchaîner)",
        "AUTO im Duell (keine Kettenfrage)", "AUTO nel duello (non chiedere catena)" } },
    { "Guardar al ganar", { "Save after a win", "Salvar ao vencer", "Sauvegarder après une victoire",
        "Nach einem Sieg speichern", "Salva dopo una vittoria" } },
    { "Aviso de carta nueva", { "New card notice", "Aviso de carta nova", "Avis de nouvelle carte",
        "Hinweis auf neue Karte", "Avviso carta nuova" } },
    { "Ayudas", { "Hints", "Ajudas", "Aides", "Hilfen", "Aiuti" } },
    { "Sí, con fotos", { "Yes, with photos", "Sim, com fotos", "Oui, avec photos", "Ja, mit Fotos", "Sì, con foto" } },
    { "Sí, sin fotos", { "Yes, no photos", "Sim, sem fotos", "Oui, sans photos", "Ja, ohne Fotos", "Sì, senza foto" } },
    { "Nombres y foto", { "Names and photo", "Nomes e foto", "Noms et photo", "Namen und Foto",
        "Nomi e foto" } },
    { "Solo nombres", { "Names only", "Só nomes", "Noms seulement", "Nur Namen", "Solo nomi" } },
    { "Mascotas en el duelo", { "Duel mascots", "Mascotes no duelo", "Mascottes en duel",
        "Maskottchen im Duell", "Mascotte nel duello" } },
    { "Tu mascota", { "Your mascot", "Seu mascote", "Ta mascotte", "Dein Maskottchen",
        "La tua mascotte" } },
    /* fundas.c: the card sleeve */
    /* perfil.c (2026-11-16) */
    { "Perfil", { "Profile", "Perfil", "Profil", "Profil", "Profilo" } },
    { "Duelista", { "Duelist", "Duelista", "Duelliste", "Duellant", "Duellante" } },
    { "Mascota", { "Mascot", "Mascote", "Mascotte", "Maskottchen", "Mascotte" } },
    { "Marco", { "Frame", "Moldura", "Cadre", "Rahmen", "Cornice" } },
    { "Clic en la foto o el nombre", { "Click the photo or the name", "Clique na foto ou no nome",
        "Clique sur la photo ou le nom", "Klicke auf Foto oder Namen", "Clicca sulla foto o sul nome" } },
    { "para cambiarlos", { "to change them", "para mudá-los", "pour les changer", "um sie zu ändern",
        "per cambiarli" } },
    { "Escribí tu nombre", { "Type your name", "Digite seu nome", "Écris ton nom", "Gib deinen Namen ein",
        "Scrivi il tuo nome" } },
    { "y apretá Enter", { "and press Enter", "e aperte Enter", "et appuie sur Entrée", "und drücke Enter",
        "e premi Invio" } },
    { "Ver Invocación", { "View Summon", "Ver Invocação", "Voir l'invocation", "Beschwörung ansehen",
        "Vedi evocazione" } },
    { "Cartas: %d / %d", { "Cards: %d / %d", "Cartas: %d / %d", "Cartes : %d / %d", "Karten: %d / %d",
        "Carte: %d / %d" } },
    { "Cartas distintas: %d", { "Different cards: %d", "Cartas diferentes: %d", "Cartes différentes : %d",
        "Verschiedene Karten: %d", "Carte diverse: %d" } },
    { "Copias: %d", { "Copies: %d", "Cópias: %d", "Exemplaires : %d", "Exemplare: %d", "Copie: %d" } },
    { "Clic: elegir      Clic derecho: volver", { "Click: choose      Right click: back",
        "Clique: escolher      Clique direito: voltar", "Clic : choisir      Clic droit : retour",
        "Klick: wählen      Rechtsklick: zurück", "Clic: scegli      Clic destro: indietro" } },
    { "Funda", { "Card sleeve", "Protetor", "Protège-carte", "Kartenhülle", "Bustina" } },
    { "Marco de la funda", { "Sleeve frame", "Moldura do protetor", "Cadre du protège-carte",
        "Hüllenrahmen", "Cornice della bustina" } },
    { "Clásica", { "Classic", "Clássica", "Classique", "Klassisch", "Classica" } },
    { "Clásico", { "Classic", "Clássico", "Classique", "Klassisch", "Classico" } },
    { "Tapete", { "Playmat", "Tapete", "Tapis de jeu", "Spielmatte", "Tappetino" } },
    { "Tapetes", { "Playmats", "Tapetes", "Tapis de jeu", "Spielmatten", "Tappetini" } },
    { "Estadísticas", { "Statistics", "Estatísticas", "Statistiques", "Statistiken", "Statistiche" } },
    { "Récord", { "Record", "Recorde", "Bilan", "Bilanz", "Record" } },
    { "Duelos", { "Duels", "Duelos", "Duels", "Duelle", "Duelli" } },
    { "Victorias", { "Wins", "Vitórias", "Victoires", "Siege", "Vittorie" } },
    { "Derrotas", { "Losses", "Derrotas", "Défaites", "Niederlagen", "Sconfitte" } },
    { "Ganadas: %d %%", { "Won: %d %%", "Vencidas: %d %%", "Gagnés : %d %%", "Gewonnen: %d %%", "Vinti: %d %%" } },
    { "Racha actual: %d", { "Current streak: %d", "Sequência atual: %d", "Série actuelle : %d", "Aktuelle Serie: %d", "Serie attuale: %d" } },
    { "Mejor racha: %d", { "Best streak: %d", "Melhor sequência: %d", "Meilleure série : %d", "Beste Serie: %d", "Serie migliore: %d" } },
    { "Victorias con Exodia: %d", { "Exodia wins: %d", "Vitórias com Exodia: %d", "Victoires avec Exodia : %d", "Siege mit Exodia: %d", "Vittorie con Exodia: %d" } },
    { "Rival más difícil", { "Toughest opponent", "Rival mais difícil", "Adversaire le plus coriace", "Härtester Gegner", "Avversario più duro" } },
    { "%d ganados / %d perdidos", { "%d won / %d lost", "%d vencidos / %d perdidos", "%d gagnés / %d perdus", "%d gewonnen / %d verloren", "%d vinti / %d persi" } },
    /* 2026-11-17: the rival's deck panel (Perfil, Estadísticas) */
    { "Mazo de %s", { "%s's deck", "Deck de %s", "Deck de %s", "Deck von %s", "Mazzo di %s" } },
    { "Siempre en su mazo (%d cartas)", { "Always in the deck (%d cards)", "Sempre no deck (%d cartas)", "Toujours dans le deck (%d cartes)", "Immer im Deck (%d Karten)", "Sempre nel mazzo (%d carte)" } },
    { "Puede sumar al azar", { "May add at random", "Pode somar ao acaso", "Peut ajouter au hasard", "Kann zufällig dazukommen", "Può aggiungere a caso" } },
    { "Arma 40 cartas por duelo", { "Builds 40 cards per duel", "Monta 40 cartas por duelo", "Forme 40 cartes par duel", "Stellt 40 Karten pro Duell zusammen", "Forma 40 carte per duello" } },
    { "x%d en su mazo", { "x%d in the deck", "x%d no deck", "x%d dans le deck", "x%d im Deck", "x%d nel mazzo" } },
    { "x%d al azar", { "x%d at random", "x%d ao acaso", "x%d au hasard", "x%d zufällig", "x%d a caso" } },
    { "La tenés (%d)", { "You have it (%d)", "Você tem (%d)", "Vous l'avez (%d)", "Du hast sie (%d)", "Ce l'hai (%d)" } },
    { "No la tenés", { "You don't have it", "Você não tem", "Vous ne l'avez pas", "Du hast sie nicht", "Non ce l'hai" } },
    { "Carta mágica", { "Spell card", "Carta mágica", "Carte magie", "Zauberkarte", "Carta magia" } },
    { "Carta de trampa", { "Trap card", "Carta armadilha", "Carte piège", "Fallenkarte", "Carta trappola" } },
    /* 2026-11-19: the page "Decks de los Oponentes" (Perfil) */
    { "Decks de los Oponentes", { "Opponents' Decks", "Decks dos Oponentes", "Decks des adversaires", "Decks der Gegner", "Deck degli avversari" } },
    { "Elegí un rival para ver su Deck", { "Pick an opponent to see their Deck", "Escolha um rival para ver o Deck dele", "Choisissez un adversaire pour voir son Deck", "Wähle einen Gegner, um sein Deck zu sehen", "Scegli un avversario per vedere il suo Deck" } },
    { "Hacé clic en una carta para verla en grande", { "Click a card to see it big", "Clique numa carta para vê-la em grande", "Cliquez sur une carte pour l'agrandir", "Klicke auf eine Karte, um sie groß zu sehen", "Clicca su una carta per vederla in grande" } },
    { "Clic para cerrar", { "Click to close", "Clique para fechar", "Cliquez pour fermer", "Klicken zum Schließen", "Clicca per chiudere" } },
    { "GANADO", { "EARNED", "GANHO", "GAGNÉ", "VERDIENT", "VINTO" } },
    { "Probar este Mazo", { "Try this Deck", "Testar este Deck", "Essayer ce Deck", "Dieses Deck testen", "Prova questo Mazzo" } },
    { "Jugar con este mazo", { "Play with this deck", "Jogar com este deck", "Jouer avec ce deck", "Mit diesem Deck spielen", "Gioca con questo mazzo" } },
    { "Devolver el mazo", { "Return the deck", "Devolver o deck", "Rendre le deck", "Deck zurückgeben", "Restituisci il mazzo" } },
    { "Tu mazo quedó guardado aparte", { "Your deck is kept aside", "Seu deck ficou guardado", "Votre deck est mis de côté", "Dein Deck ist beiseitegelegt", "Il tuo mazzo è messo da parte" } },
    { "Faltan %d cartas para ganarlo", { "%d cards left to earn it", "Faltam %d cartas para ganhá-lo", "Encore %d cartes pour le gagner", "Noch %d Karten, um es zu verdienen", "Mancano %d carte per vincerlo" } },
    { "Duelo: %d de %d", { "Duel: %d of %d", "Duelo: %d de %d", "Duel : %d sur %d", "Duell: %d von %d", "Duello: %d di %d" } },
    { "Torneo: %d de %d", { "Tournament: %d of %d", "Torneio: %d de %d", "Tournoi : %d sur %d", "Turnier: %d von %d", "Torneo: %d di %d" } },
    { "Cartas que da %s: %d de %d", { "Cards %s gives: %d of %d", "Cartas que %s dá: %d de %d", "Cartes que donne %s : %d sur %d", "Karten von %s: %d von %d", "Carte che dà %s: %d di %d" } },
    { "¿Jugar con el mazo de %s?", { "Play with %s's deck?", "Jogar com o deck de %s?", "Jouer avec le deck de %s ?", "Mit dem Deck von %s spielen?", "Giocare con il mazzo di %s?" } },
    { "Cada duelo usás 40 cartas armadas como las arma él.", { "Each duel you use 40 cards built the way they build them.", "Cada duelo você usa 40 cartas montadas como ele monta.", "Chaque duel, vous utilisez 40 cartes montées comme il le fait.", "Jedes Duell nutzt du 40 Karten, so zusammengestellt wie bei ihm.", "Ogni duello usi 40 carte composte come le compone lui." } },
    { "Tu mazo no se toca: vuelve cuando devolvés este.", { "Your deck is not touched: it comes back when you return this one.", "Seu deck não é tocado: volta quando você devolver este.", "Votre deck n'est pas modifié : il revient quand vous rendez celui-ci.", "Dein Deck bleibt unverändert: Es kommt zurück, wenn du dieses zurückgibst.", "Il tuo mazzo non si tocca: torna quando restituisci questo." } },
    { "Cada victoria gasta un uso; perder o empatar no.", { "Each win uses one try; a loss or a draw does not.", "Cada vitória gasta um uso; perder ou empatar não.", "Chaque victoire coûte un essai ; une défaite ou une égalité, non.", "Jeder Sieg verbraucht einen Versuch, Niederlage oder Unentschieden nicht.", "Ogni vittoria consuma un uso; perdere o pareggiare no." } },
    { "Conseguí todas las cartas que da y es tuyo para siempre.", { "Get every card they give and it is yours for good.", "Consiga todas as cartas que ele dá e será seu para sempre.", "Obtenez toutes ses cartes et il est à vous pour toujours.", "Hol dir alle Karten, die er gibt, und es gehört dir für immer.", "Ottieni tutte le carte che dà ed è tuo per sempre." } },
    { "Aceptar", { "OK", "Aceitar", "Accepter", "Annehmen", "Accetta" } },
    { "Cancelar", { "Cancel", "Cancelar", "Annuler", "Abbrechen", "Annulla" } },
    { "Elegí un rival para ver su Deck (los oscuros todavía no los desbloqueaste en Duelo)", { "Pick an opponent to see their Deck (the dark ones are not unlocked in Duel yet)", "Escolha um rival para ver seu Deck (os escuros ainda não foram desbloqueados no Duelo)", "Choisissez un adversaire pour voir son Deck (les sombres ne sont pas encore débloqués en Duel)", "Wähle einen Gegner, um sein Deck zu sehen (die dunklen sind im Duell noch nicht freigeschaltet)", "Scegli un avversario per vedere il suo Mazzo (quelli scuri non sono ancora sbloccati in Duello)" } },
    { "Solo para Duelos (no Torneos)", { "Duels only (no Tournaments)", "Só para Duelos (não Torneios)", "Duels uniquement (pas de Tournois)", "Nur für Duelle (keine Turniere)", "Solo per Duelli (non Tornei)" } },
    { "Sirve para Duelos y Torneos", { "For Duels and Tournaments", "Serve para Duelos e Torneios", "Pour Duels et Tournois", "Für Duelle und Turniere", "Per Duelli e Tornei" } },
    { "Solo para Duelos: en un Torneo jugás con tu mazo y se cancela.", { "Duels only: in a Tournament you play your deck and the loan ends.", "Só para Duelos: num Torneio você joga com seu deck e o empréstimo acaba.", "Duels uniquement : en Tournoi vous jouez votre deck et le prêt s'annule.", "Nur für Duelle: Im Turnier spielst du dein Deck und die Leihe endet.", "Solo per Duelli: in un Torneo giochi col tuo mazzo e il prestito si annulla." } },
    { "Conseguí todas sus cartas y es tuyo para siempre, también en Torneos.", { "Get all their cards and it is yours for good, Tournaments too.", "Consiga todas as cartas dele e será seu para sempre, também em Torneios.", "Obtenez toutes ses cartes et il est à vous pour toujours, Tournois compris.", "Hol dir alle seine Karten und es gehört dir für immer, auch in Turnieren.", "Ottieni tutte le sue carte ed è tuo per sempre, anche nei Tornei." } },
    { "Ganado: sirve para Duelos y Torneos.", { "Earned: for Duels and Tournaments.", "Ganho: serve para Duelos e Torneios.", "Gagné : pour Duels et Tournois.", "Verdient: für Duelle und Turniere.", "Vinto: per Duelli e Tornei." } },
    { "Solo para Duelos. Cada victoria gasta 1 de sus 3 usos.", { "Duels only. Each win uses 1 of its 3 tries.", "Só para Duelos. Cada vitória gasta 1 dos 3 usos.", "Duels uniquement. Chaque victoire coûte 1 de ses 3 essais.", "Nur für Duelle. Jeder Sieg kostet 1 der 3 Versuche.", "Solo per Duelli. Ogni vittoria consuma 1 dei 3 usi." } },
    { "Con todas sus cartas es tuyo para siempre.", { "With all their cards it is yours for good.", "Com todas as cartas dele é seu para sempre.", "Avec toutes ses cartes il est à vous pour toujours.", "Mit all seinen Karten gehört es dir für immer.", "Con tutte le sue carte è tuo per sempre." } },
    { "Sin datos de su mazo", { "No deck data", "Sem dados do deck", "Aucune donnée de deck", "Keine Deckdaten", "Nessun dato del mazzo" } },
    { "Todavía ninguno", { "None yet", "Nenhum ainda", "Aucun pour l'instant", "Noch keiner", "Ancora nessuno" } },
    { "Últimos duelos", { "Last duels", "Últimos duelos", "Derniers duels", "Letzte Duelle", "Ultimi duelli" } },
    { "Todavía no jugaste duelos", { "No duels played yet", "Você ainda não jogou duelos", "Aucun duel joué pour l'instant", "Noch keine Duelle gespielt", "Nessun duello giocato" } },
    { "Victoria", { "Win", "Vitória", "Victoire", "Sieg", "Vittoria" } },
    { "Derrota", { "Loss", "Derrota", "Défaite", "Niederlage", "Sconfitta" } },
    { "Empate", { "Draw", "Empate", "Égalité", "Unentschieden", "Pareggio" } },
    { "Exodia", { "Exodia", "Exodia", "Exodia", "Exodia", "Exodia" } },
    { "Kuriboh", { "Kuriboh", "Kuriboh", "Kuriboh", "Kuriboh", "Kuriboh" } },
    { "Dorado", { "Gold", "Dourado", "Doré", "Gold", "Dorato" } },
    { "Plata", { "Silver", "Prata", "Argent", "Silber", "Argento" } },
    { "Obsidiana", { "Obsidian", "Obsidiana", "Obsidienne", "Obsidian", "Ossidiana" } },
    { "Milenio", { "Millennium", "Milênio", "Millénaire", "Millennium", "Millennio" } },
    { "Neón", { "Neon", "Neon", "Néon", "Neon", "Neon" } },
    { "Efecto del tapete", { "Playmat effect", "Efeito do tapete", "Effet du tapis", "Spielmatten-Effekt",
        "Effetto del tappetino" } },
    { "Luz", { "Light", "Luz", "Lumière", "Licht", "Luce" } },
    { "Pulso", { "Pulse", "Pulso", "Pulsation", "Puls", "Impulso" } },
    { "Brillo", { "Sparkle", "Brilho", "Éclat", "Glitzer", "Scintillio" } },
    { "Ojos Azules", { "Blue-Eyes", "Olhos Azuis", "Yeux Bleus", "Blauäugiger", "Occhi Blu" } },
    { "Mago Oscuro", { "Dark Magician", "Mago Negro", "Magicien Sombre", "Dunkler Magier",
        "Mago Nero" } },
    { "Dragón Barril", { "Barrel Dragon", "Dragão Barril", "Dragon Barillet", "Revolverdrache", "Drago Revolver" } },
    { "Rey Rex de Dos Cabezas", { "Two-Headed King Rex", "Rei Rex de Duas Cabeças", "Roi Rex à Deux Têtes", "Zweiköpfiger König Rex", "Re Rex a Due Teste" } },
    { "Gran Polilla", { "Great Moth", "Grande Mariposa", "Grande Mite", "Große Motte", "Grande Falena" } },
    { "Pescador Legendario", { "Legendary Fisherman", "Pescador Lendário", "Pêcheur Légendaire", "Legendärer Fischer", "Pescatore Leggendario" } },
    { "Restricción de Mil Ojos", { "Thousand-Eyes Restrict", "Restrição de Mil Olhos", "Restriction aux Mille Yeux", "Tausend-Augen-Bann", "Restrizione dai Mille Occhi" } },
    { "Necrofear Oscuro", { "Dark Necrofear", "Necrofear Sombrio", "Nécrofear Obscur", "Dunkle Nekrofurcht", "Necrofear Oscuro" } },
    { "Bestia Enmascarada", { "The Masked Beast", "A Fera Mascarada", "La Bête Masquée", "Die maskierte Bestie", "La Bestia Mascherata" } },
    { "Máquina Tragamonedas", { "Slot Machine", "Máquina Caça-Níqueis", "Machine à Sous", "Spielautomat", "Slot Machine" } },
    { "Dragón Zombi", { "Dragon Zombie", "Dragão Zumbi", "Dragon Zombie", "Drachenzombie", "Drago Zombie" } },
    { "Maha Vailo", { "Maha Vailo", "Maha Vailo", "Maha Vailo", "Maha Vailo", "Maha Vailo" } },
    { "Golem del Milenio", { "Millennium Golem", "Golem do Milênio", "Golem du Millénaire", "Millenniumgolem", "Golem del Millennio" } },
    { "Dragón Definitivo de Ojos Azules", { "Blue-Eyes Ultimate Dragon", "Dragão Supremo de Olhos Azuis", "Dragon Ultime aux Yeux Bleus", "Blauäugiger ultimativer Drache", "Drago Definitivo Occhi Blu" } },
    { "Dragón Negro de Cráneo", { "B. Skull Dragon", "Dragão Caveira Negra", "Dragon Crâne Noir", "Schwarzer Schädeldrache", "Drago Teschio Nero" } },
    { "Mago del Caos Negro", { "Magician of Black Chaos", "Mago do Caos Negro", "Magicien du Chaos Noir", "Magier des Schwarzen Chaos", "Mago del Caos Nero" } },
    { "Dragón Meteoro Negro", { "Meteor B. Dragon", "Dragão Meteoro Negro", "Dragon Météore Noir", "Meteor-Schwarzdrache", "Drago Meteora Nero" } },
    { "Valkyrion el Guerrero Magna", { "Valkyrion the Magna Warrior", "Valkyrion, o Guerreiro Magna", "Valkyrion le Guerrier Magna", "Valkyrion der Magna-Krieger", "Valkyrion il Guerriero Magna" } },
    { "Dragón Toon de Ojos Azules", { "Blue-Eyes Toon Dragon", "Dragão Toon de Olhos Azuis", "Dragon Toon aux Yeux Bleus", "Blauäugiger Toon-Drache", "Drago Toon Occhi Blu" } },
    { "Gaia el Campeón Dragón", { "Gaia the Dragon Champion", "Gaia, o Campeão Dragão", "Gaia le Champion Dragon", "Gaia der Drachenchampion", "Gaia il Campione Drago" } },
    { "Sabio Oscuro", { "Dark Sage", "Sábio Sombrio", "Sage Sombre", "Dunkler Weiser", "Saggio Oscuro" } },
    { "Dragón del Trueno de Dos Cabezas", { "Twin-Headed Thunder Dragon", "Dragão do Trovão de Duas Cabeças", "Dragon Tonnerre à Deux Têtes", "Zweiköpfiger Donnerdrache", "Drago del Tuono a Due Teste" } },
    { "Dragón de Tres Cuernos", { "Tri-Horned Dragon", "Dragão de Três Chifres", "Dragon Tricorne", "Dreihörniger Drache", "Drago Tricorno" } },
    { "Reina del Cosmos", { "Cosmo Queen", "Rainha Cósmica", "Reine du Cosmos", "Kosmoskönigin", "Regina del Cosmo" } },
    { "Zera el Mant", { "Zera the Mant", "Zera, o Mant", "Zera le Mant", "Zera der Mant", "Zera il Mant" } },
    { "Sengenjin", { "Sengenjin", "Sengenjin", "Sengenjin", "Sengenjin", "Sengenjin" } },
    { "Caballero de Cráneo", { "Skull Knight", "Cavaleiro Caveira", "Chevalier Crâne", "Schädelritter", "Cavaliere Teschio" } },
    { "Dragón Mascota de Harpie", { "Harpie's Pet Dragon", "Dragão de Estimação da Harpia", "Dragon Domestique de la Harpie", "Harpyien-Schoßdrache", "Drago da Compagnia dell'Arpia" } },
    { "Guardián de la Puerta", { "Gate Guardian", "Guardião do Portão", "Gardien de la Porte", "Torwächter", "Guardiano del Cancello" } },
    { "Renunciado", { "Relinquished", "Renunciado", "Relinquished", "Relinquished", "Relinquished" } },
    { "Cráneo Convocado Toon", { "Toon Summoned Skull", "Caveira Invocada Toon", "Crâne Invoqué Toon", "Toon-Herbeigerufener Schädel", "Teschio Evocato Toon" } },
    { "Maldición del Dragón", { "Curse of Dragon", "Maldição do Dragão", "Malédiction du Dragon", "Fluch des Drachen", "Maledizione del Drago" } },
    { "Mil Dragones", { "Thousand Dragon", "Dragão Milenar", "Dragon Millénaire", "Tausend-Drache", "Drago Millenario" } },
    { "Alfa el Guerrero Magnético", { "Alpha The Magnet Warrior", "Alfa, o Guerreiro Magnético", "Alpha le Guerrier Magnétique", "Alpha der Magnetkrieger", "Alfa il Guerriero Magnetico" } },
    { "Beta el Guerrero Magnético", { "Beta The Magnet Warrior", "Beta, o Guerreiro Magnético", "Bêta le Guerrier Magnétique", "Beta der Magnetkrieger", "Beta il Guerriero Magnetico" } },
    { "Gamma el Guerrero Magnético", { "Gamma The Magnet Warrior", "Gama, o Guerreiro Magnético", "Gamma le Guerrier Magnétique", "Gamma der Magnetkrieger", "Gamma il Guerriero Magnetico" } },
    { "Dragón Meteoro", { "Meteor Dragon", "Dragão Meteoro", "Dragon Météore", "Meteordrache", "Drago Meteora" } },
    { "Elfa Mística", { "Mystical Elf", "Elfa Mística", "Elfe Mystique", "Mystische Elfe", "Elfo Mistico" } },
    { "Bruja del Bosque Negro", { "Witch of the Black Forest", "Bruxa da Floresta Negra", "Sorcière de la Forêt Noire", "Hexe aus dem Schwarzwald", "Strega della Foresta Nera" } },
    { "Tomate Místico", { "Mystic Tomato", "Tomate Místico", "Tomate Mystique", "Mystische Tomate", "Pomodoro Mistico" } },
    { "Sangan", { "Sangan", "Sangan", "Sangan", "Sangan", "Sangan" } },
    { "Gardna del Gran Escudo", { "Big Shield Gardna", "Gardna do Grande Escudo", "Gardna au Grand Bouclier", "Großschild-Gardna", "Gardna Grande Scudo" } },
    { "Payaso Zombi", { "Clown Zombie", "Palhaço Zumbi", "Clown Zombie", "Clownzombie", "Zombie Clown" } },
    { "Fantasma Ingenioso", { "Witty Phantom", "Fantasma Espirituoso", "Fantôme Spirituel", "Witziges Phantom", "Fantasma Arguto" } },
    { "Mago de la Fe", { "Magician of Faith", "Maga da Fé", "Magicienne de la Foi", "Magierin des Glaubens", "Maga della Fede" } },
    { "Légamo Humanoide", { "Humanoid Slime", "Lodo Humanoide", "Bave Humanoïde", "Menschlicher Schleim", "Melma Umanoide" } },
    { "Princesa de Fuego", { "Fire Princess", "Princesa do Fogo", "Princesse du Feu", "Feuerprinzessin", "Principessa del Fuoco" } },
    { "Hermanas Dama Arpía", { "Harpie Lady Sisters", "Irmãs Dama Harpia", "Sœurs Dame Harpie", "Harpyie-Damen-Schwestern", "Sorelle Arpia" } },
    { "Sanga del Trueno", { "Sanga of the Thunder", "Sanga do Trovão", "Sanga du Tonnerre", "Sanga des Donners", "Sanga del Tuono" } },
    { "Kazejin", { "Kazejin", "Kazejin", "Kazejin", "Kazejin", "Kazejin" } },
    { "Suijin", { "Suijin", "Suijin", "Suijin", "Suijin", "Suijin" } },
    { "Dokurorider", { "Dokurorider", "Dokurorider", "Dokurorider", "Dokurorider", "Dokurorider" } },
    { "Bruja Oscura Dunames", { "Dunames Dark Witch", "Bruxa Sombria Dunames", "Sorcière Sombre Dunames", "Dunkle Hexe Dunames", "Strega Oscura Dunames" } },
    { "Guerrero de Pizarra", { "Slate Warrior", "Guerreiro de Ardósia", "Guerrier d'Ardoise", "Schieferkrieger", "Guerriero d'Ardesia" } },
    { "Gusano Dragón Humanoide", { "Humanoid Worm Drake", "Verme Dragão Humanoide", "Ver Dragon Humanoïde", "Humanoider Wurmdrache", "Verme Drago Umanoide" } },
    { "Asaltante Vorse", { "Vorse Raider", "Saqueador Vorse", "Pillard Vorse", "Vorse-Plünderer", "Predone Vorse" } },
    { "Fuerza de Ataque Goblin", { "Goblin Attack Force", "Força de Ataque Goblin", "Force d'Attaque Gobeline", "Goblin-Angriffstruppe", "Forza d'Attacco Goblin" } },
    { "Elfas Gemelas", { "Gemini Elf", "Elfas Gêmeas", "Elfes Gémeaux", "Zwillingselfen", "Elfi Gemelli" } },
    { "Santa Juana", { "St. Joan", "Santa Joana", "Sainte Jeanne", "Heilige Johanna", "Santa Giovanna" } },
    { "Zoa", { "Zoa", "Zoa", "Zoa", "Zoa", "Zoa" } },
    { "Melchid la Bestia de Cuatro Caras", { "Melchid the Four-Face Beast", "Melchid, a Besta de Quatro Faces", "Melchid, la Bête aux Quatre Visages", "Melchid, Bestie mit vier Gesichtern", "Melchid dai 4 Volti" } },
    { "Bickuribox", { "Bickuribox", "Bickuribox", "Bickuribox", "Bickuribox", "Bickuribox" } },
    { "Elfa Oscura", { "Dark Elf", "Elfa Sombria", "Elfe Noire", "Dunkle Elfe", "Elfa Oscura" } },
    { "Tortuga Cangrejo", { "Crab Turtle", "Tartaruga Caranguejo", "Tortue Crabe", "Krabbenschildkröte", "Tartaruga Granchio" } },
    { "Espada de Garma", { "Garma Sword", "Espada de Garma", "Épée de Garma", "Garma-Schwert", "Spada di Garma" } },
    { "Escarabajo Jabalina", { "Javelin Beetle", "Besouro Dardo", "Scarabée Javelot", "Speerkäfer", "Scarabeo Giavellotto" } },
    { "Tanque Laberinto", { "Labyrinth Tank", "Tanque do Labirinto", "Tank du Labyrinthe", "Labyrinth-Panzer", "Carro Armato Labirinto" } },
    { "Ballena Fortaleza", { "Fortress Whale", "Baleia Fortaleza", "Baleine Forteresse", "Festungswal", "Balena Fortezza" } },
    { "Dragón Nocturno Serpiente", { "Serpent Night Dragon", "Dragão Noturno Serpente", "Dragon Serpent de la Nuit", "Schlangennachtdrache", "Drago Serpente della Notte" } },
    { "Chakra", { "Chakra", "Chakra", "Chakra", "Chakra", "Chakra" } },
    { "Fiend Megacyber", { "The Fiend Megacyber", "O Demônio Megaciber", "Le Démon Mégacyber", "Der Unhold Megacyber", "Il Demone Megacyber" } },
    { "Bicho Devorador de Hombres", { "Man-Eater Bug", "Inseto Devorador de Homens", "Insecte Mangeur d'Hommes", "Menschenfresser-Insekt", "Insetto Mangiauomini" } },
    { "Jarra Cibernética", { "Cyber Jar", "Jarro Cibernético", "Cyber Jarre", "Cyber-Krug", "Vaso Cyber" } },
    { "Parshath Caballero Aéreo", { "Airknight Parshath", "Parshath, o Cavaleiro Aéreo", "Parshath, Chevalier Aérien", "Luftritter Parshath", "Parshath Cavaliere dell'Aria" } },
    { "Soldado Pingüino", { "Penguin Soldier", "Soldado Pinguim", "Soldat Pingouin", "Pinguinsoldat", "Soldato Pinguino" } },
    { "Jarra Cambiante", { "Morphing Jar", "Jarro Mutante", "Jarre Morphing", "Morphender Krug", "Vaso Mutante" } },
    { "Muka Muka", { "Muka Muka", "Muka Muka", "Muka Muka", "Muka Muka", "Muka Muka" } },
    { "Soldado Gigante de Piedra", { "Giant Soldier of Stone", "Soldado Gigante de Pedra", "Soldat Géant de Pierre", "Riesensoldat aus Stein", "Soldato Gigante di Pietra" } },
    { "Muro Laberinto", { "Labyrinth Wall", "Parede do Labirinto", "Mur du Labyrinthe", "Labyrinthwand", "Muro Labirinto" } },
    { "Ninja Armado", { "Armed Ninja", "Ninja Armado", "Ninja Armé", "Bewaffneter Ninja", "Ninja Armato" } },
    { "Aprendiz de Bruja", { "Witch's Apprentice", "Aprendiz de Bruxa", "Apprentie Sorcière", "Hexenlehrling", "Apprendista Strega" } },
    { "Gilasaurus", { "Gilasaurus", "Gilasaurus", "Gilasaurus", "Gilasaurus", "Gilasaurus" } },
    { "Battleguard de Lava", { "Lava Battleguard", "Guarda de Batalha de Lava", "Gardebataille de Lave", "Lava-Kampfwache", "Guardia di Lava" } },
    { "Seiyaryu", { "Seiyaryu", "Seiyaryu", "Seiyaryu", "Seiyaryu", "Seiyaryu" } },
    { "Guerrero Pantera", { "Panther Warrior", "Guerreiro Pantera", "Guerrier Panthère", "Pantherkrieger", "Guerriero Pantera" } },
    { "Jirai Gumo", { "Jirai Gumo", "Jirai Gumo", "Jirai Gumo", "Jirai Gumo", "Jirai Gumo" } },
    { "Mago Oscuro II", { "Dark Magician II", "Mago Negro II", "Magicien Sombre II", "Dunkler Magier II", "Mago Nero II" } },
    { "Cráneo Convocado", { "Summoned Skull", "Caveira Invocada", "Crâne Invoqué",
        "Beschworener Schädel", "Teschio Evocato" } },
    { "Ojos Rojos", { "Red-Eyes", "Olhos Vermelhos", "Yeux Rouges", "Rotäugiger", "Occhi Rossi" } },
    { "Chica Maga Oscura", { "Dark Magician Girl", "Menina Maga Negra", "Magicienne des Ténèbres", "Dunkles Magier-Mädchen", "Ragazza Maga Nera" } },
    { "Gaia el Caballero Feroz", { "Gaia the Fierce Knight", "Gaia, o Cavaleiro Impetuoso", "Gaia le Chevalier Farouche", "Gaia der grimmige Ritter", "Gaia il Cavaliere Feroce" } },
    { "Guardián Celta", { "Celtic Guardian", "Guardião Celta", "Gardien Celte", "Keltischer Wächter", "Guardiano Celtico" } },
    { "Dama Arpía", { "Harpie Lady", "Dama Harpia", "Dame Harpie", "Harpyie", "Arpia" } },
    { "Mago del Tiempo", { "Time Wizard", "Mago do Tempo", "Magicien du Temps", "Zeitzauberer", "Mago del Tempo" } },
    { "Dragón Bebé", { "Baby Dragon", "Bebê Dragão", "Bébé Dragon", "Babydrache", "Drago Bebè" } },
    { "Espadachín de la Llama", { "Flame Swordsman", "Espadachim das Chamas", "Épéiste des Flammes", "Flammenschwertkämpfer", "Spadaccino di Fiamma" } },
    { "Soldado del Brillo Negro", { "Black Luster Soldier", "Soldado do Brilho Negro", "Soldat du Lustre Noir", "Schwarzglänzender Soldat", "Soldato del Lustro Nero" } },
    { "Turno del rival rápido", { "Fast opponent turn", "Turno do rival rápido",
        "Tour adverse rapide", "Schneller Gegnerzug", "Turno avversario veloce" } },
    { "Voces", { "Voices", "Vozes", "Voix", "Stimmen", "Voci" } },
    { "Cartas agregadas (al reiniciar)", { "Added cards (on restart)", "Cartas adicionadas (ao reiniciar)",
        "Cartes ajoutées (au redémarrage)", "Hinzugefügte Karten (nach Neustart)", "Carte aggiunte (al riavvio)" } },
    { "Sigue los FPS", { "Follows the FPS", "Segue os FPS", "Suit les FPS", "Folgt den FPS",
        "Segue gli FPS" } },
    { "Normal", { "Normal", "Normal", "Normale", "Normal", "Normale" } },
    { "Salir", { "Exit", "Sair", "Quitter", "Beenden", "Esci" } },
    { "Perfil (se cambia en Configuración)", { "Profile (changed in Settings)", "Perfil (muda-se nas Configurações)",
      "Profil (se change dans Paramètres)", "Profil (in den Einstellungen ändern)", "Profilo (si cambia in Impostazioni)" } },
    { "Salir a Windows", { "Exit to Windows", "Sair para o Windows", "Quitter vers Windows",
        "Zu Windows beenden", "Esci a Windows" } },
    { "Controles", { "Controls", "Controles", "Commandes", "Steuerung", "Controlli" } },
    { "Ver controles (teclado)", { "Show controls (keyboard)", "Ver controles (teclado)",
        "Voir les commandes (clavier)", "Steuerung anzeigen (Tastatur)", "Mostra controlli (tastiera)" } },
    { "Elegí las teclas en Configuración.exe", { "Pick the keys in Configuración.exe",
        "Escolha as teclas no Configuración.exe", "Choisissez les touches dans Configuración.exe",
        "Tasten in Configuración.exe festlegen", "Scegli i tasti in Configuración.exe" } },
    { "Sensibilidad", { "Sensitivity", "Sensibilidade", "Sensibilité", "Empfindlichkeit", "Sensibilità" } },
    { "Sensibilidad vertical", { "Vertical sensitivity", "Sensibilidade vertical", "Sensibilité verticale",
        "Vertikale Empfindlichkeit", "Sensibilità verticale" } },
    { "Sensibilidad al apuntar (clic der.)", { "Aiming sensitivity (right click)",
        "Sensibilidade ao mirar (botão dir.)", "Sensibilité en visée (clic droit)",
        "Empfindlichkeit beim Zielen (Rechtsklick)", "Sensibilità in mira (tasto destro)" } },
    { "Velocidad máx. de giro", { "Max turn speed", "Velocidade máx. de giro", "Vitesse de rotation max.",
        "Max. Drehgeschwindigkeit", "Velocità max di rotazione" } },
    { "Invertir eje Y", { "Invert Y axis", "Inverter eixo Y", "Inverser l'axe Y", "Y-Achse umkehren",
        "Inverti asse Y" } },
    { "Modo del ratón", { "Mouse mode", "Modo do mouse", "Mode de la souris", "Mausmodus", "Modalità mouse" } },

    { "Corregir líneas en 2D", { "Fix lines in 2D", "Corrigir linhas em 2D", "Corriger les lignes en 2D",
        "Linien in 2D korrigieren", "Correggi linee in 2D" } },
    { "Imagen 2D (cartas, menús, textos)", { "2D picture (cards, menus, text)",
        "Imagem 2D (cartas, menus, textos)", "Image 2D (cartes, menus, textes)",
        "2D-Bild (Karten, Menüs, Texte)", "Immagine 2D (carte, menu, testi)" } },
    { "Alta resolución", { "High resolution", "Alta resolução", "Haute résolution", "Hohe Auflösung",
        "Alta risoluzione" } },
    { "Original (480p)", { "Original (480p)", "Original (480p)", "Originale (480p)", "Original (480p)",
        "Originale (480p)" } },
    { "Sin corregir (puede mostrar líneas)", { "Uncorrected (may show lines)",
        "Sem correção (pode mostrar linhas)", "Non corrigée (peut afficher des lignes)",
        "Unkorrigiert (kann Linien zeigen)", "Non corretta (può mostrare linee)" } },
    { "Panorámico (estira la imagen)", { "Widescreen (stretches the picture)", "Widescreen (estica a imagem)",
        "Écran large (étire l'image)", "Breitbild (streckt das Bild)", "Widescreen (allunga l'immagine)" } },
    { "Jugar cartas rápido (animaciones)", { "Fast card plays (animations)",
        "Jogar cartas rápido (animações)", "Jouer les cartes vite (animations)",
        "Karten schnell spielen (Animationen)", "Giocare le carte veloce (animazioni)" } },
    { "Estirar a 16:9", { "Stretch to 16:9", "Esticar para 16:9", "Étirer en 16:9", "Auf 16:9 strecken",
        "Allunga a 16:9" } },
    { "Imagen", { "Picture", "Imagem", "Image", "Bild", "Immagine" } },
    { "Cursor del juego", { "Game cursor", "Cursor do jogo", "Curseur du jeu", "Spiel-Cursor",
        "Cursore del gioco" } },
    { "Puntero (estilo PC)", { "Pointer (PC style)", "Ponteiro (estilo PC)",
        "Pointeur (style PC)", "Zeiger (PC-Stil)", "Puntatore (stile PC)" } },
    { "Solo clics", { "Clicks only", "Só cliques", "Clics seulement", "Nur Klicks", "Solo clic" } },
    { "Sensibilidad del cursor", { "Cursor sensitivity", "Sensibilidade do cursor", "Sensibilité du curseur",
        "Cursor-Empfindlichkeit", "Sensibilità del cursore" } },
    { "Modo grabación (OBS)", { "Recording mode (OBS)", "Modo gravação (OBS)",
        "Mode enregistrement (OBS)", "Aufnahmemodus (OBS)",
        "Modalità registrazione (OBS)" } },

    /* choices */
    { "Sí", { "Yes", "Sim", "Oui", "Ja", "Sì" } },
    { "Entrada del duelo", { "Duel entrance", "Entrada do duelo", "Entrée du duel", "Duell-Einzug", "Ingresso del duello" } },
    { "Carta As", { "Ace card", "Carta Ás", "Carte As", "Ass-Karte", "Carta Asso" } },
    { "Siempre", { "Always", "Sempre", "Toujours", "Immer", "Sempre" } },
    { "Primera vez por duelo", { "First time per duel", "Primeira vez por duelo", "Première fois par duel", "Erstes Mal pro Duell", "Prima volta per duello" } },
    { "Completa", { "Full", "Completa", "Complète", "Vollständig", "Completo" } },
    { "Solo las mascotas", { "Mascots only", "Só os mascotes", "Mascottes seules", "Nur Maskottchen", "Solo le mascotte" } },
    { "Original", { "Original", "Original", "Originale", "Original", "Originale" } },
    { "Número de daño", { "Damage number", "Número de dano", "Nombre de dégâts", "Schadenszahl", "Numero del danno" } },
    { "Baja", { "Low", "Baixa", "Basse", "Niedrig", "Bassa" } },
    { "Media", { "Medium", "Média", "Moyenne", "Mittel", "Media" } },
    /* 2026-11-21: the quality profile (main.c k_perfil), as in Configuración */
    { "Perfil de calidad", { "Quality profile", "Perfil de qualidade", "Profil de qualité", "Qualitätsprofil",
        "Profilo di qualità" } },
    { "Recomendado", { "Recommended", "Recomendado", "Recommandé", "Empfohlen", "Consigliato" } },
    { "Bajo", { "Low", "Baixo", "Bas", "Niedrig", "Basso" } },
    { "Medio", { "Medium", "Médio", "Moyen", "Mittel", "Medio" } },
    { "Alto", { "High", "Alto", "Élevé", "Hoch", "Alto" } },
    { "Ultra", { "Ultra", "Ultra", "Ultra", "Ultra", "Ultra" } },
    { "Personalizado", { "Custom", "Personalizado", "Personnalisé", "Benutzerdefiniert", "Personalizzato" } },
    { "Alta", { "High", "Alta", "Haute", "Hoch", "Alta" } },
    { "No", { "No", "Não", "Non", "Nein", "No" } },
    { "Libre", { "Free", "Livre", "Libre", "Frei", "Libera" } },
    { "Sin límite", { "Unlimited", "Sem limite", "Illimitée", "Unbegrenzt", "Illimitata" } },
    { "Automático", { "Automatic", "Automático", "Automatique", "Automatisch", "Automatico" } },
    { "Teclado y ratón", { "Keyboard and mouse", "Teclado e mouse", "Clavier et souris", "Tastatur und Maus",
        "Tastiera e mouse" } },
    { "Mando Xbox", { "Xbox controller", "Controle Xbox", "Manette Xbox", "Xbox-Controller", "Controller Xbox" } },
    { "Mando PS4/PS5", { "PS4/PS5 controller", "Controle PS4/PS5", "Manette PS4/PS5", "PS4/PS5-Controller",
        "Controller PS4/PS5" } },
    { "Directo (como Black)", { "Direct (like Black)", "Direto (como Black)", "Direct (comme Black)",
        "Direkt (wie Black)", "Diretto (come Black)" } },
    { "Stick emulado", { "Emulated stick", "Analógico emulado", "Stick émulé", "Emulierter Stick",
        "Levetta emulata" } },
    { "x1 (como el mando)", { "x1 (like a controller)", "x1 (como o controle)", "x1 (comme la manette)",
        "x1 (wie Controller)", "x1 (come il controller)" } },
    { "Monitor", { "Monitor", "Monitor", "Moniteur", "Monitor", "Monitor" } },   /* fps_juego */
    { "No (4:3)", { "No (4:3)", "Não (4:3)", "Non (4:3)", "Nein (4:3)", "No (4:3)" } },
    { "Ultrapanorámico (según el monitor)", { "Ultrawide (fits the monitor)", "Ultrawide (conforme o monitor)",
        "Ultra-large (selon l'écran)", "Ultrawide (passend zum Monitor)", "Ultrawide (in base al monitor)" } },
    { "Sí (nativo del juego)", { "Yes (game's own)", "Sim (nativo do jogo)", "Oui (natif du jeu)",
        "Ja (spieleigen)", "Sì (nativo del gioco)" } },
    { "Sí (PC gama baja)", { "Yes (low-end PC)", "Sim (PC mais fraco)", "Oui (PC modeste)",
        "Ja (schwacher PC)", "Sì (PC poco potente)" } },
    { "x4 (Ctrl+F8 dos veces)", { "x4 (Ctrl+F8 twice)", "x4 (Ctrl+F8 duas vezes)", "x4 (Ctrl+F8 deux fois)",
        "x4 (zweimal Ctrl+F8)", "x4 (Ctrl+F8 due volte)" } },
    { "Llenar pantalla", { "Fill the screen", "Preencher a tela", "Remplir l'écran", "Bildschirm füllen",
        "Riempi lo schermo" } },
    { "Estirar (deformado)", { "Stretch (distorted)", "Esticar (distorcido)", "Étirer (déformé)",
        "Strecken (verzerrt)", "Stira (deformato)" } },
    { "4:3 (original)", { "4:3 (original)", "4:3 (original)", "4:3 (d'origine)", "4:3 (Original)",
        "4:3 (originale)" } },
};

static const char *translate(const char *es);
/* perfil.c: the text in the player's language (Spanish as is) */
const char *f2_traducir(const char *es)
{
    const char *t = translate(es);
    return t ? t : es;
}

static const char *translate(const char *es)
{
    size_t i;
    if (s_lang < 0 || !es)
        return NULL;
    for (i = 0; i < sizeof k_texts / sizeof k_texts[0]; i++)
        if (!strcmp(k_texts[i].es, es))
            return k_texts[i].t[s_lang];
    return NULL;
}

/* launcher.ini in YGO_AJUSTES (with ygo_menu.ini, where Configuración.exe
 * keeps it), else next to the exe */
static int launcher_ini(wchar_t *path)
{
    wchar_t *slash;
    const wchar_t *dir = _wgetenv(L"YGO_AJUSTES");
    if (dir && *dir && wcslen(dir) + 14 < MAX_PATH) {
        wcscpy(path, dir);
        wcscat(path, L"\\launcher.ini");
        return 1;
    }
    if (!GetModuleFileNameW(NULL, path, MAX_PATH))
        return 0;
    slash = wcsrchr(path, L'\\');
    if (!slash || (size_t)(slash - path) + 14 >= MAX_PATH)
        return 0;
    wcscpy(slash + 1, L"launcher.ini");
    return 1;
}

/* "es", "en"... from launcher.ini, or "" */
static void launcher_lang(char *out, size_t n)
{
    wchar_t path[MAX_PATH];
    char line[64];
    FILE *f;
    out[0] = 0;
    if (!launcher_ini(path) || !(f = _wfopen(path, L"r")))
        return;
    while (fgets(line, sizeof line, f))
        if (!strncmp(line, "idioma=", 7)) {
            snprintf(out, n, "%.2s", line + 7);
            break;
        }
    fclose(f);
}

static void use_code(const char *code)
{
    int i;
    s_lang = -1;
    /* Portuguese was taken out (2026-11-10: the game has none and the voices
     * are Spanish); an old launcher.ini with idioma=pt gets Spanish */
    if (!strcmp(code, "pt"))
        return;
    for (i = 0; i < N_LANG; i++)
        if (!strcmp(code, k_codes[i]))
            s_lang = i;
    if (s_lang < 0 && strcmp(code, "es"))
        s_lang = EN;
}

/* The language picked in the panel's "Idioma" row: the panel shows it at
 * once (the game takes it at the next start), and the launcher opens in it
 * (launcher.ini holds only that line). */
static void on_language(const char *code)
{
    wchar_t path[MAX_PATH];
    FILE *f;
    use_code(code);
    xbox_MenuSetTranslator(translate);
    if (launcher_ini(path) && (f = _wfopen(path, L"w"))) {
        fprintf(f, "idioma=%s\r\n", code);
        fclose(f);
    }
    fprintf(stderr, "[MENU] language: %s (from F2)\n", code);
}

/* premios.c: the language of its sign (-1 Spanish, or EN..IT) */
int f2_idioma(void) { return s_lang; }

void f2_idiomas_init(void)
{
    char code[8];
    launcher_lang(code, sizeof code);
    if (!code[0]) {
        switch (PRIMARYLANGID(GetUserDefaultUILanguage())) {
        case LANG_SPANISH:    strcpy(code, "es"); break;
        case LANG_PORTUGUESE: strcpy(code, "es"); break;  /* no Portuguese (2026-11-10) */
        case LANG_FRENCH:     strcpy(code, "fr"); break;
        case LANG_GERMAN:     strcpy(code, "de"); break;
        case LANG_ITALIAN:    strcpy(code, "it"); break;
        default:              strcpy(code, "en"); break;
        }
    }
    use_code(code);
    xbox_MenuSetTranslator(translate);
    xbox_MenuSetLanguageHook(on_language);
    fprintf(stderr, "[MENU] language: %s\n", s_lang < 0 ? "es" : k_codes[s_lang]);
}
