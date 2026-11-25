/*
 * premios_extra.c - three optional additions about the cards the opponents
 * give (pedido del usuario, 2026-11-12), each with its row in F2 and in
 * Configuración.exe:
 *
 * 1. "Quién da cada carta" (quien_da): in the Biblioteca, on a card you
 *    don't have, a sign with the opponents that can give it, with their
 *    portrait ("Nombres y foto", the default) or their names only ("Solo
 *    nombres"). The grid's place (top + row) * 10 + col is the place-th card
 *    of the save's list (u16 at save + 0x3DC + 2i, i = 0..0x472, 0 = not in
 *    the Biblioteca: sub at 0x10F4B6 builds 0x66DD20 that way); its card is
 *    k = i + 1 and you have it if the u8 at save + 0xCC2 + i is not 0 (the
 *    same test). The opponents: characters 0..27 (names texts 86-113,
 *    portraits cfsNN), duelist d = s8 0x423D60[character], its two reward
 *    lists (premios.c), the current variant, any level.
 * 2. "Premios sin cuarta copia" (premios_sin_cuarta): the reward slots of a
 *    win (sub_00025BC0 calls sub_0008CBB0(flag, d, slot) once per slot; it
 *    is sub_0008C7E0 or sub_0008C9B0 by the flag) are drawn again, up to
 *    REINTENTOS times, while the card is one you already have 3 of; then in
 *    the rival's other slots (2026-11-14); if every try still gives one, the
 *    last stays (no other card to give). Normal duels and tournaments alike.
 * 3. "Aviso de carta nueva" (aviso_nueva): a card of those slots you didn't
 *    have, once it reaches your chest (the u8 at save + 0xCC1 + k goes above
 *    0), brings a sign "¡Carta nueva!" with its name for a few seconds.
 *    And (2026-11-13) it stays marked "Nueva" in the Biblioteca's grid (a red
 *    badge on the card's top edge; the card at column c, shown row r is
 *    centred at frame px 94 + 40c, 120 + 57r, 35 x 51: raton.c) until you
 *    open it there (mode [0x66E684] = 2). The marked cards are kept in
 *    YGO_PERFIL\cartas_nuevas.txt (one k per line). Same switch.
 */
#include "mods_carpeta.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <intrin.h>
#include "recomp/gen/recomp_types.h"
#include "cartel.h"

int f2_idioma(void);                    /* f2_idiomas.c: -1 Spanish, 0 en, 1 pt, 2 fr, 3 de, 4 it */
int ygo_premios_variante(int d);        /* premios.c: sub_0008C530(d) */

#define SAVE 0x00651E70u
#define T_IMAGEN 0x005FE692u
#define NCARTAS 1139u
#define COLECCION (SAVE + 0xCC1u)
#define TXT_NOMBRE_CARTA 849u
#define TXT_PERSONAJE 86u
#define NPERSONAJES 28
#define REINTENTOS 200

static volatile LONG s_quien = 0;       /* 0 names and photo, 1 names only, 2 no */
static volatile LONG s_sin4 = 1;
static volatile LONG s_nueva = 1;
int ygo_quien_get(void) { return (int)s_quien; }
void ygo_quien_set(int i) { InterlockedExchange(&s_quien, i < 0 || i > 2 ? 0 : i); }
int ygo_sin4_get(void) { return s_sin4 ? 0 : 1; }
void ygo_sin4_set(int i) { InterlockedExchange(&s_sin4, i == 0); }
int ygo_nueva_get(void) { return s_nueva ? 0 : 1; }
void ygo_nueva_set(int i) { InterlockedExchange(&s_nueva, i == 0); }

static int lang6(void)
{
    int l = f2_idioma() + 1;
    return l < 0 || l > 5 ? 0 : l;
}

static int k_de(uint32_t id)
{
    uint32_t k;
    for (k = 1; k < NCARTAS; ++k)
        if (MEM16(T_IMAGEN + 2u * k) == id)
            return (int)k;
    return -1;
}

static unsigned copias_k(int k)
{
    return k > 0 ? MEM8(COLECCION + (uint32_t)k) : 0u;
}

static void texto_juego(uint32_t n, char *out, int cap);
/* a character's name (premios.c's Triple sign, 2026-11-25) */
void ygo_nombre_personaje(int ch, char *out, int cap)
{
    out[0] = 0;
    if (ch >= 0 && ch < 40)
        texto_juego(TXT_PERSONAJE + (uint32_t)ch, out, cap);
}

/* text n of the game in its current language, as UTF-8 (the tables are
 * Windows-1252: voces.c / cartas.c) */
static void texto_juego(uint32_t n, char *out, int cap)
{
    uint32_t lang = MEM32(0x005FC2C0u), tab, base, p;
    wchar_t w[256];
    out[0] = 0;
    if (lang >= 6u)
        return;
    tab = MEM32(0x005FC2DCu + 4u * lang);
    base = MEM32(0x005FC2C4u + 4u * lang);
    if (!tab || n >= MEM32(tab))
        return;
    p = MEM32(tab + 4u + 4u * n) + base;
    {
        char a[256];
        int i;
        for (i = 0; i < 255 && MEM8(p + (uint32_t)i); ++i)
            a[i] = (char)MEM8(p + (uint32_t)i);
        a[i] = 0;
        if (!MultiByteToWideChar(1252, 0, a, -1, w, 256))
            return;
    }
    WideCharToMultiByte(CP_UTF8, 0, w, -1, out, cap, NULL, NULL);
}

/* does duelist d give card id (any level; 2026-11-25: either story variant)? */
static int da_carta(int d, uint32_t id)
{
    uint32_t t;
    for (t = 0; t < 4u; ++t) {
        uint32_t tab = (t & 1u) ? 0x00453630u : 0x00453830u, r = t >> 1;
        uint32_t p = MEM32(tab + 16u * (uint32_t)d + 4u * (uint32_t)r);
        uint32_t c = MEM16(tab + 16u * (uint32_t)d + 8u + 2u * (uint32_t)r), e;
        if (!p || c > 512u)
            continue;
        for (e = 0; e < c; ++e)
            if (MEM16(p + 4u * e) == id && (MEM16(p + 4u * e + 2u) & 0x1FFFu))
                return 1;
    }
    return 0;
}

/* ---- the "Nueva" marks ---------------------------------------------------- */

static volatile char s_marca[NCARTAS + 1];  /* 1: given, not opened in the Biblioteca yet */
static CRITICAL_SECTION s_mcs;
static int s_mcs_ok;

void ygo_perfil_ruta(const char *archivo, char *out, size_t cap);   /* perfil_dir.c */
unsigned ygo_ranura(void);
static unsigned s_marcas_ranura;

static int marcas_ruta(char *out, size_t cap)
{
    if (!getenv("YGO_PERFIL"))
        return 0;
    ygo_perfil_ruta("cartas_nuevas.txt", out, cap);
    return 1;
}

static void marcas_guardar(void)
{
    char path[MAX_PATH];
    FILE *f;
    uint32_t k;
    if (!s_mcs_ok || s_marcas_ranura != ygo_ranura() || !marcas_ruta(path, sizeof path))
        return;                         /* (another slot's marks are not loaded yet) */
    EnterCriticalSection(&s_mcs);
    if ((f = fopen(path, "w"))) {
        for (k = 1; k <= NCARTAS; ++k)
            if (s_marca[k])
                fprintf(f, "%u\n", k);
        fclose(f);
    }
    LeaveCriticalSection(&s_mcs);
}

static void marcas_cargar(void)
{
    char path[MAX_PATH], line[32];
    FILE *f;
    s_marcas_ranura = ygo_ranura();
    if (!marcas_ruta(path, sizeof path) || !(f = fopen(path, "r")))
        return;
    while (fgets(line, sizeof line, f)) {
        unsigned long k = strtoul(line, NULL, 10);
        if (k >= 1 && k <= NCARTAS)
            s_marca[k] = 1;
    }
    fclose(f);
}

/* buscador.c: "Solo nuevas" */
int ygo_carta_nueva(int k)
{
    return k >= 1 && k <= (int)NCARTAS && s_marca[k];
}

static const char *const k_marca[6] = { "Nueva", "New", "Nova", "Nouvelle", "Neu", "Nuova" };

/* each frame of the Biblioteca (from ygo_quien_da_tick) */
static void marcas_tick(void)
{
    uint32_t modo = MEM16(0x0066E684u), top = MEM16(0x0066E690u), i;
    float x[CARTEL_MARCAS], y[CARTEL_MARCAS];
    int n = 0;
    if (modo != 0u)
        ygo_cartel_marcas_quitar();     /* the card's detail (or a prompt) is open: none over it */
    if (modo != 0u && modo != 4u) {
        /* the open card: the cursor's. 2026-11-17 (the user: "cuando ves las
         * cartas igual no se va el cartel de nuevas"; the log had no "vista"
         * line after three opens): the card is read from the grid's own list
         * (0x66DD20: place -> k, as raton.c's tick), in any of the open
         * card's modes (1 opening, 2 open, 3), not only 2 */
        static uint32_t dicho = 0xFFFFFFFFu;
        uint32_t idx = (top + MEM16(0x0066E68Eu)) * 10u + MEM16(0x0066E68Cu), k;
        k = idx < 0x473u ? MEM16(0x0066DD20u + 2u * idx) : 0u;
        if (modo != dicho) {
            dicho = modo;
            fprintf(stderr, "[PREMIOS] Biblioteca modo %u, lugar %u carta k %u%s\n", modo, idx, k,
                    k && k <= NCARTAS && s_marca[k] ? " (nueva)" : "");
            fflush(stderr);
        }
        if (k && k <= NCARTAS && s_marca[k]) {
            s_marca[k] = 0;
            marcas_guardar();
            fprintf(stderr, "[PREMIOS] carta k %u vista en la Biblioteca: sin marca de nueva\n", k);
            fflush(stderr);
        }
        return;
    }
    if (modo != 0u || !s_nueva)
        return;
    /* the 50 places shown, from the grid's own list */
    for (i = 0; i < 50u && n < CARTEL_MARCAS; ++i) {
        uint32_t rel = i, idx = top * 10u + i, k = idx < 0x473u ? MEM16(0x0066DD20u + 2u * idx) : 0u;
        if (k && k <= NCARTAS && s_marca[k] && MEM8(SAVE + 0xCC1u + k)) {
            x[n] = 94.f + 40.f * (float)(rel % 10u);
            y[n] = 120.f + 57.f * (float)(rel / 10u) - 25.5f;
            ++n;
        }
    }
    if (n)
        ygo_cartel_marcas(x, y, n, k_marca[lang6()], 36.f, 250);
}

/* ---- 1. the Biblioteca ---------------------------------------------------- */

static const char *const k_titulo[6] = {
    "Te la pueden dar:", "Can be won from:", "Podem te dar:", "Peut être gagnée contre :",
    "Erhältlich von:", "Te la possono dare:" };
static const char *const k_nadie[6] = {
    "Ningún rival da esta carta", "No opponent gives this card", "Nenhum rival dá esta carta",
    "Aucun adversaire ne donne cette carte", "Kein Gegner gibt diese Karte",
    "Nessun avversario dà questa carta" };

/* raton.c's wrapper of the Biblioteca's key handler (sub_0010F790), each
 * frame of that screen, on the game's thread */
void ygo_quien_da_tick(void)
{
    static int last_k = -1, last_modo = -1, last_lang = -1;
    static ygo_cartel c;
    uint32_t idx, i, place = 0;
    int k = -1, modo = (int)s_quien, lang;
    marcas_tick();
    if (modo == 2 || MEM16(0x0066E684u) != 0u)
        return;
    idx = (MEM16(0x0066E690u) + MEM16(0x0066E68Eu)) * 10u + MEM16(0x0066E68Cu);
    for (i = 0; i < 0x473u; ++i) {
        if (!MEM16(SAVE + 0x3DCu + 2u * i))
            continue;
        if (place == idx) {
            k = (int)i + 1;
            break;
        }
        ++place;
    }
    /* 2026-11-12: only on cards you have (who gave it / gives more copies);
     * an unowned one shows face down with no name, so the sign was useless */
    if (k < 0 || !MEM8(SAVE + 0xCC2u + (uint32_t)(k - 1)))
        return;                         /* no card there, or one you don't have */
    lang = lang6();
    if (k != last_k || modo != last_modo || lang != last_lang) {
        uint32_t id = MEM16(T_IMAGEN + 2u * (uint32_t)k);
        int ch, seen[64] = { 0 }, extra = 0;
        memset(&c, 0, sizeof c);
        for (ch = 0; ch < NPERSONAJES; ++ch) {
            int d = (int)(int8_t)MEM8(0x00423D60u + (uint32_t)ch);
            if (d < 0 || d >= 64 || seen[d])
                continue;
            seen[d] = 1;
            if (!da_carta(d, id) || c.nitems >= CARTEL_ITEMS)
                continue;
            texto_juego(TXT_PERSONAJE + (uint32_t)ch, c.item[c.nitems], sizeof c.item[0]);
            c.foto[c.nitems] = ch;
            ++c.nitems;
        }
        /* the characters past the 28 named ones (other forms of a duelist):
         * only logged, to see whether some card comes from them alone */
        for (ch = NPERSONAJES; ch < 40; ++ch) {
            int d = (int)(int8_t)MEM8(0x00423D60u + (uint32_t)ch);
            if (d >= 0 && d < 64 && !seen[d] && da_carta(d, id))
                ++extra;
        }
        snprintf(c.titulo, sizeof c.titulo, "%s", c.nitems ? k_titulo[lang] : k_nadie[lang]);
        c.fotos = modo == 0;
        c.y_mil = 130;
        {
            static unsigned said;
            if (said < 200) {
                int j;
                ++said;
                fprintf(stderr, "[QUIEN] lugar %u carta k %d id %u: %d rivales%s", idx, k, id, c.nitems,
                        extra ? " (y otros sin nombre)" : "");
                for (j = 0; j < c.nitems; ++j)
                    fprintf(stderr, "%s%s", j ? ", " : ": ", c.item[j]);
                fprintf(stderr, "\n");
                fflush(stderr);
            }
        }
        last_k = k;
        last_modo = modo;
        last_lang = lang;
    }
    c.lado = 1;                         /* always on the left (user's choice) */
    ygo_cartel_poner(CARTEL_BIBLIOTECA, &c, 250);
}

/* ---- 2 and 3. the reward slots ------------------------------------------- */

#define NNUEVAS 8
static volatile LONG s_nk[NNUEVAS];     /* k of a card given that you didn't have, 0 free */
static volatile ULONGLONG s_nat;        /* when the last ones were given */

void sub_0008CBB0_pad(void);
static void marca_premio(int k, int vaciar);

/* the cards already drawn for this reward screen: the slots are drawn one
 * after another and only reach the chest afterwards, so two of the same card
 * in one reward would both pass the "3 copies" test (2026-11-13: a 4th Mystic
 * Tomato). A new reward screen starts when the slot goes back down or after
 * 15 s: a tournament draws slot 0 five times, then 1, then 2 (registro
 * 2026-11-14); before, every slot 0 cleared this and a tournament could give
 * the same card twice. */
#define NSORTEO 32
static int s_sorteo[NSORTEO], s_nsorteo;
static ULONGLONG s_sorteo_at;
static uint32_t s_ult_ranura;

/* 2026-11-18 (the user: "si esta limitada a 1 copia que solo te de una, me dio
 * mas ya"): how many copies of k are worth having: the save's limit (u8 at
 * 0x665896 + k, from sub_0007EF00, the mod's "copias" brought down by
 * ygo_cartas_limites; 0 = forbidden), 3 if it holds no limit */
/* 2026-11-19 14:2x (14:15 log: Sangan, limited to 1 in the game's own list,
 * given with 1 already in the chest, twice: its byte in the save was not 1,
 * a save made before the list was read): the lower of the save's byte and
 * the card's own limit (ygo_limite_carta: the mod's, then the game's) */
/* 2026-11-19 16:3x (the user, sarcastic: "me tocaron 3 sanganos"; 16:30 log:
 * "límite 0" for every card, even ones never owned; the 14:15 log too, a card
 * with 0 copies given only after 600 tries): at the reward screen the save's
 * byte reads 0 for all cards, so every draw failed and the last one stayed
 * (Sangan four times). The save's byte is not used here any more: the card's
 * own limit only (ygo_limite_carta: the mod's, then the game's list, 3). */
unsigned ygo_limite_carta(uint32_t id);  /* cartas.c */
static unsigned tope_k(int k)
{
    unsigned l;
    if (k <= 0 || k > (int)NCARTAS)
        return 3u;
    l = ygo_limite_carta(MEM16(T_IMAGEN + 2u * (uint32_t)k));
    return l > 3u ? 3u : l;
}

static unsigned copias_premio(int k)
{
    unsigned c = copias_k(k);
    int i;
    for (i = 0; i < s_nsorteo; ++i)
        if (s_sorteo[i] == k)
            ++c;
    return c;
}

/* 2026-11-19 16:4x (the user, after 3 copies of one new card: "la cosa es
 * que te ganes cartas variadas"): a card is not taken when you have it to its
 * limit, nor (rep 0) when it already came in this same reward screen */
static int mala(int k, int rep)
{
    unsigned c = copias_premio(k);
    return c >= tope_k(k) || (!rep && c > copias_k(k));
}

/* the same lines as the log, also appended to YGO_PERFIL\premios_registro.txt,
 * which is not cleared at each start (to check a win afterwards) */
static void premio_log(const char *line)
{
    const char *dir = getenv("YGO_PERFIL");
    char path[MAX_PATH];
    FILE *f;
    fputs(line, stderr);
    fflush(stderr);
    if (!dir || !*dir)
        return;
    ygo_perfil_ruta("premios_registro.txt", path, sizeof path);   /* 2026-11-25: per game slot */
    f = fopen(path, "a");
    if (f) {
        fputs(line, f);
        fclose(f);
    }
}

/* 2026-11-19 16:1x (the user: "ME DIO AL SANGANO DE NUEVO"; 16:13 log: the
 * rival's draws gave only cards you had to the limit for 600 tries, and the
 * last one stayed: Sangan, limited to 1, with 1 in the chest): when every try
 * fails, a card from duelist d's own reward lists (both, the current variant,
 * as da_carta) still below its limit, by the lists' weights (bits 0-12);
 * 0 if there is none */
static uint32_t otra_carta(int d)
{
    static uint32_t ids[1024];
    static unsigned pes[1024];
    unsigned n = 0, tot = 0, i, x;
    int r, t, rep;
    if (d < 0 || d >= 64)
        return 0u;
    r = ygo_premios_variante(d);
    /* a card not given yet in this reward first; a repeat only if none */
    for (rep = 0; rep < 2 && !tot; ++rep)
    for (t = 0; t < 2; ++t) {
        uint32_t tab = t ? 0x00453630u : 0x00453830u;
        uint32_t lp = MEM32(tab + 16u * (uint32_t)d + 4u * (uint32_t)r);
        uint32_t c = MEM16(tab + 16u * (uint32_t)d + 8u + 2u * (uint32_t)r), e;
        if (!lp || c > 512u)
            continue;
        for (e = 0; e < c; ++e) {
            uint32_t id = MEM16(lp + 4u * e);
            unsigned w = MEM16(lp + 4u * e + 2u) & 0x1FFFu;
            int kk;
            if (!id || !w || (kk = k_de(id)) <= 0 || mala(kk, rep))
                continue;
            for (i = 0; i < n && ids[i] != id; ++i)
                ;
            if (i == n) {
                if (n >= 1024u)
                    continue;
                ids[n] = id;
                pes[n++] = 0;
            }
            pes[i] += w;
            tot += w;
        }
    }
    if (!tot)
        return 0u;
    x = (unsigned)(__rdtsc() % tot);
    for (i = 0; i < n; ++i) {
        if (x < pes[i])
            return ids[i];
        x -= pes[i];
    }
    return ids[n - 1];
}

void sub_0008CBB0(void)
{
    uint32_t sp = g_esp, flag = MEM32(sp + 4), d = MEM32(sp + 8), slot = MEM32(sp + 12);
    uint32_t id, first;
    int tries = 0, k;
    unsigned cop;
    char line[256];
    ULONGLONG now = GetTickCount64();
    int nueva = !s_sorteo_at || slot < s_ult_ranura || now - s_sorteo_at > 15000u;
    uint32_t otra;
    if (nueva)
        s_nsorteo = 0;
    s_ult_ranura = slot;
    s_sorteo_at = now;
    sub_0008CBB0_pad();
    id = first = g_eax & 0xFFFFu;
    k = id ? k_de(id) : -1;
    cop = copias_premio(k);
    if (s_sin4 && id) {
        /* this slot first; if none of its cards is one you lack (2026-11-14,
         * rival 29 slot 0: 200 tries, every card with 3 copies), the rival's
         * other slots (the slot argument changed for those calls) */
        for (otra = 0; otra < 3u && mala(k, 0); ++otra) {
            int n = 0;
            MEM32(sp + 12) = (slot + otra) % 3u;
            while (mala(k, 0) && n < REINTENTOS) {
                uint32_t again;
                g_esp = sp;             /* the return address and the args are still there */
                sub_0008CBB0_pad();
                ++tries;
                ++n;
                again = g_eax & 0xFFFFu;
                if (!again)
                    break;
                id = again;
                k = k_de(id);
                cop = copias_premio(k);
            }
        }
        MEM32(sp + 12) = slot;
        if (mala(k, 0)) {
            uint32_t alt = otra_carta((int)d);
            snprintf(line, sizeof line, "[PREMIOS] ranura %u: tras %d sorteos seguía la %u (k %d, tenías %u, "
                     "límite %u): %s %u\n", slot, tries, id, k, cop, tope_k(k),
                     alt ? "se da otra de la lista del rival, la" : "no queda ninguna otra, queda la",
                     alt ? alt : id);
            premio_log(line);
            if (alt) {
                id = alt;
                k = k_de(id);
                cop = copias_premio(k);
            }
        }
        g_esp = sp + 4u;                /* as after the pad's own ret */
        g_eax = (g_eax & 0xFFFF0000u) | id;
    }
    if (id != first)
        snprintf(line, sizeof line, "[PREMIOS] ranura %u (rival %u, modo %u, filtro %s): carta %u (k %d), ya tenías %u "
                 "(límite %u); salió antes la %u (al límite o ya salida en este premio): %d sorteos más\n",
                 slot, d, flag, s_sin4 ? "si" : "no", id, k, cop, tope_k(k), first, tries);
    else
        snprintf(line, sizeof line, "[PREMIOS] ranura %u (rival %u, modo %u, filtro %s): carta %u (k %d), ya tenías %u "
                 "(límite %u)\n", slot, d, flag, s_sin4 ? "si" : "no", id, k, cop, tope_k(k));
    premio_log(line);
    if (k > 0 && s_nsorteo < NSORTEO)
        s_sorteo[s_nsorteo++] = k;
    if (nueva) {
        int j;
        for (j = 0; j < NNUEVAS; ++j)
            InterlockedExchange(&s_nk[j], 0);
        marca_premio(0, 1);
    }
    if (k > 0 && cop == 0u) {
        int j;
        marca_premio(k, 0);
        for (j = 0; j < NNUEVAS; ++j)
            if (s_nk[j] == k)
                break;
        for (j = j < NNUEVAS ? NNUEVAS : 0; j < NNUEVAS; ++j)
            if (InterlockedCompareExchange(&s_nk[j], k, 0) == 0)
                break;
        s_nat = GetTickCount64();
    }
}

/* ---- "Nueva" over each new card of the reward screen ----------------------
 * 2026-11-19 (the user: two new cards in one reward showed one big sign after
 * the other, unreadable, and it stayed a while after leaving the screen; "que
 * aparezca arriba de cada carta"). The big sign is gone. Each card given that
 * you didn't have goes to the kit's watch list by its face texture's hash
 * (Mods-Extras\cartas\huellas.txt, "k HASH" per line, tools/cartas_mod.py: the
 * same hash the kit computes; Ctrl+F9 dump 18:3x: the reward screen draws
 * each card as a 2D quad with its 128x256 face, 128 frame px wide at x 85,
 * 255, 425, y 75..260 for 3 cards). While the kit sees one drawn (last 200
 * ms), a "Nueva" badge sits on its top edge; when no face is seen any more,
 * the badges go within 250 ms, and 3 s later the list is emptied (so the deck
 * editor does not show it). A new reward screen empties it too. */
struct nv2a_marca_kit {
    volatile unsigned long long hash[16];
    volatile int n;
    volatile float x0[16], y0[16], x1[16], y1[16];
    volatile ULONGLONG visto[16];
};
extern struct nv2a_marca_kit g_nv2a_marca;
static unsigned long long *s_huella;    /* [NCARTAS], 0 unknown */
static ULONGLONG s_marca_vista;         /* last time a watched face was seen */
static int s_marca_dicho;               /* logged for this reward */
static int s_marca_alguna;              /* a watched face was seen in this reward */
static unsigned long long s_marca_dicha[16]; /* faces already logged in this reward */

static void huellas_cargar(void)
{
    char p[MAX_PATH], *barra, linea[64];
    FILE *f;
    DWORD n = GetModuleFileNameA(NULL, p, MAX_PATH);
    unsigned cuantas = 0;
    if (!n || n >= MAX_PATH || !(barra = strrchr(p, '\\')))
        return;
    *barra = 0;
    if (strlen(p) + 56 >= MAX_PATH)
        return;
    strcat(p, ygo_mods_a());
    strcat(p, "\\cartas\\huellas.txt");
    s_huella = (unsigned long long *)calloc(NCARTAS, sizeof *s_huella);
    if (!s_huella || !(f = fopen(p, "r"))) {
        fprintf(stderr, "[PREMIOS] sin huellas de cartas (%s): no hay marca Nueva en el premio\n", p);
        return;
    }
    while (fgets(linea, sizeof linea, f)) {
        unsigned k;
        unsigned long long h;
        if (sscanf(linea, "%u %llx", &k, &h) == 2 && k >= 1 && k < NCARTAS) {
            s_huella[k] = h;
            ++cuantas;
        }
    }
    fclose(f);
    fprintf(stderr, "[PREMIOS] huellas de %u cartas leídas\n", cuantas);
}

/* sub_0008CBB0: a new reward screen (vaciar) and each card given you lack */
static void marca_premio(int k, int vaciar)
{
    int n = g_nv2a_marca.n, i;
    if (vaciar) {
        g_nv2a_marca.n = 0;
        n = 0;
        s_marca_dicho = 0;
        s_marca_alguna = 0;
        memset(s_marca_dicha, 0, sizeof s_marca_dicha);
    }
    if (k <= 0 || k >= (int)NCARTAS || !s_huella || !s_huella[k] || n >= 16)
        return;
    for (i = 0; i < n; ++i)
        if (g_nv2a_marca.hash[i] == s_huella[k])
            return;
    g_nv2a_marca.visto[n] = 0;
    g_nv2a_marca.hash[n] = s_huella[k];
    g_nv2a_marca.n = n + 1;
    s_marca_vista = GetTickCount64();     /* the 3 s start now, if never seen */
}

/* every 50 ms (nueva_thread) */
static void marcas_premio_tick(void)
{
    float x[CARTEL_MARCAS], y[CARTEL_MARCAS], ancho = 0.f;
    int n = g_nv2a_marca.n, i, m = 0;
    ULONGLONG now = GetTickCount64();
    if (n <= 0)
        return;
    {
        /* 2026-11-20 (the user: "al cerrar la puerta se vieron por un segundo
         * mas"): the game still draws the cards under its closing doors (the
         * transition [0x66DCD8] +0x10: 1 closing, 2 shut; raton.c), so the
         * kit kept seeing them while the badges sat over the doors. Gone
         * 120 ms into the 1 (the gap is still ~0.8 wide then, raton.c's
         * table with its 80 ms lag) and while shut. */
        static uint32_t ts_antes;
        static ULONGLONG cierra_at;
        uint32_t tr = MEM32(0x66DCD8u), ts = tr ? MEM32(tr + 0x10u) : 0u;
        if (ts == 1u && ts_antes != 1u)
            cierra_at = now;
        ts_antes = ts;
        if (ts == 2u || (ts == 1u && now - cierra_at > 120u)) {
            ygo_cartel_marcas_quitar();
            return;
        }
    }
    for (i = 0; i < n && i < 16 && m < CARTEL_MARCAS; ++i) {
        float w;
        if (!g_nv2a_marca.visto[i] || now - g_nv2a_marca.visto[i] > 200u)
            continue;
        w = g_nv2a_marca.x1[i] - g_nv2a_marca.x0[i];
        /* 2026-11-20: each card's badge once, with where it went (proof that
         * every new card got its own) */
        {
            unsigned long long h = g_nv2a_marca.hash[i];
            int j;
            for (j = 0; j < 16 && s_marca_dicha[j] && s_marca_dicha[j] != h; ++j)
                ;
            if (j < 16 && !s_marca_dicha[j]) {
                s_marca_dicha[j] = h;
                fprintf(stderr, "[PREMIOS] Nueva sobre la carta %016llX en %.0f,%.0f - %.0f,%.0f\n", h,
                        g_nv2a_marca.x0[i], g_nv2a_marca.y0[i], g_nv2a_marca.x1[i], g_nv2a_marca.y1[i]);
            }
        }
        x[m] = (g_nv2a_marca.x0[i] + g_nv2a_marca.x1[i]) * 0.5f;
        y[m] = g_nv2a_marca.y0[i] + 2.f;
        if (w * 0.5f > ancho)
            ancho = w * 0.5f;
        ++m;
    }
    if (m) {
        s_marca_vista = now;
        s_marca_alguna = 1;
        if (s_nueva)
            ygo_cartel_marcas(x, y, m, k_marca[lang6()], ancho, 250);
        if (!s_marca_dicho) {
            s_marca_dicho = 1;
            fprintf(stderr, "[PREMIOS] marca Nueva sobre %d carta(s) del premio (primera en %.0f,%.0f)\n", m, x[0], y[0]);
            fflush(stderr);
        }
    } else if (now - s_marca_vista > (s_marca_alguna ? 3000u : 300000u)) {
        /* 2026-11-20 16:0x (the user: new cards won with no "Nueva"; 15:33,
         * 15:48 and 15:52 rewards of the 15:52 log): the cards are drawn
         * when the rival's speech balloon is closed, and the 3 s used to
         * count from the draw, so a speech read for longer emptied the list
         * before the cards showed (the 4 that worked showed within ~3 s).
         * Now the 3 s count only after the cards were seen; never seen, the
         * list waits up to 5 min (a new reward empties it anyway). */
        if (s_marca_alguna || g_nv2a_marca.n)
            fprintf(stderr, "[PREMIOS] lista de marcas vaciada (%s)\n",
                    s_marca_alguna ? "cartas ya vistas" : "5 min sin ver las cartas");
        g_nv2a_marca.n = 0;
    }
}

/* watches the chest for the given cards you didn't have (10 times a second) */
void ygo_cartas_limites(void);          /* cartas.c: the mod's copy limits in the save */
void ygo_cofre_vigilar(void);           /* cartas.c: logs every change of the chest */
void ygo_cadena_textos(void);
static DWORD WINAPI nueva_thread(LPVOID unused)
{
    (void)unused;
    unsigned vuelta = 0;
    for (;;) {
        int j;
        Sleep(50);
        marcas_premio_tick();           /* "Nueva" over the reward's cards, every 50 ms */
        if (++vuelta & 1u)
            continue;                   /* the rest 10 times a second, as before */
        ygo_cartas_limites();
        ygo_cofre_vigilar();
        if (s_marcas_ranura != ygo_ranura()) {   /* 2026-11-25: another game slot, its own "Nueva" marks */
            uint32_t k;
            EnterCriticalSection(&s_mcs);
            for (k = 0; k <= NCARTAS; ++k)
                s_marca[k] = 0;
            marcas_cargar();
            LeaveCriticalSection(&s_mcs);
        }
        ygo_cadena_textos();            /* cadena.c: "Card Status" in the player's language */
        if (s_nat && GetTickCount64() - s_nat > 20u * 60u * 1000u) {
            for (j = 0; j < NNUEVAS; ++j)
                InterlockedExchange(&s_nk[j], 0);
            s_nat = 0;
        }
        for (j = 0; j < NNUEVAS; ++j) {
            int k = (int)s_nk[j];
            if (k <= 0 || !copias_k(k))
                continue;
            InterlockedExchange(&s_nk[j], 0);
            if (k <= (int)NCARTAS && !s_marca[k]) {
                s_marca[k] = 1;
                marcas_guardar();
            }
            /* 2026-11-19: no big "¡Carta nueva!" sign any more (the badge
             * over the card on the reward screen, marcas_premio_tick) */
            fprintf(stderr, "[PREMIOS] carta nueva en el cofre: k %d\n", k);
            fflush(stderr);
        }
    }
}

void ygo_premios_extra_start(void)
{
    static int started;
    if (started)
        return;
    started = 1;
    InitializeCriticalSection(&s_mcs);
    s_mcs_ok = 1;
    marcas_cargar();
    huellas_cargar();
    ygo_cartel_start();
    CloseHandle(CreateThread(NULL, 0, nueva_thread, NULL, 0, NULL));
}
