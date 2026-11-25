/*
 * prestado.c - "Probar este Mazo": play with an opponent's deck (pedido del
 * usuario, 2026-11-20, mock-up approved). Rules the user chose:
 *   - 3 uses per opponent; only a WON duel uses one (a loss or a draw does
 *     not); after the last one the deck goes back by itself;
 *   - once you own every card that opponent gives (the same count as the
 *     rewards sign, premios.c), the deck is "Ganado": no limit, for good.
 *
 * What the game does (read in the code): at a duel's start sub_0007E070
 * builds both decks. Normal duels ([setup] == 0, setup = sub_00103590()):
 * the player's from the saved deck slot, sub_0007DC40(0, slot) (slot =
 * sub_000D95F0(); the slots at u16 0x66609A + 0x100 * slot, count u16
 * 0x66649A + 2 * slot, rebuilt from the save by sub_000D8FF0 just before).
 * The opponent's with sub_0008C760(1, d): clears that player's deck, puts in
 * its list B (sub_0008C6D0) and fills up to 40 at random from list A
 * (sub_0008C5E0), both for the story's variant (sub_0008C530(d)), then
 * sub_0008C0D0. It takes the player as an argument, so the loaned deck is
 * that very call for player 0: sub_0007DC40 is wrapped and, with a loan on,
 * calls sub_0008C760(0, d) with the same two stack slots. The saved deck
 * (0x66609A and the save) is never written: your deck stays as it was, the
 * chest (sub_000D9420 counts the saved deck) does not change.
 *
 * Kept in YGO_PERFIL\prestados.txt:
 *   activo <character | -1>
 *   <character> <uses left 0..3> <ganado 0/1>
 * Written as soon as it changes (a win, a loan, a return).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include "recomp/gen/recomp_types.h"

#define T_IMAGEN 0x005FE692u
#define NCARTAS 1139u
#define COLECCION (0x00651E70u + 0xCC1u)
#define NCH 40
#define USOS 3

int ygo_premios_variante(int d);        /* premios.c: sub_0008C530(d) */

static int s_cargado;
static int s_activo = -1;               /* the character whose deck you play with, -1 none */
static signed char s_usos[NCH];         /* uses left */
static unsigned char s_ganado[NCH];
static int s_duelo_ch = -1;             /* this duel was played with that deck */

void ygo_perfil_ruta(const char *archivo, char *out, size_t cap);   /* perfil_dir.c */
unsigned ygo_ranura(void);
static unsigned s_ranura;

static void ruta(char *p, size_t cap)
{
    ygo_perfil_ruta("prestados.txt", p, cap);
}

static void cargar(void)
{
    char p[MAX_PATH], l[96];
    FILE *f;
    int i;
    if (s_cargado && s_ranura == ygo_ranura())
        return;
    s_cargado = 1;
    s_ranura = ygo_ranura();            /* 2026-11-25: each game slot its own loans */
    s_activo = -1;
    s_duelo_ch = -1;
    for (i = 0; i < NCH; i++) {
        s_usos[i] = USOS;
        s_ganado[i] = 0;
    }
    ruta(p, sizeof p);
    if ((f = fopen(p, "rb")) == NULL)
        return;
    while (fgets(l, sizeof l, f)) {
        int ch, u, g;
        if (sscanf(l, "activo %d", &ch) == 1)
            s_activo = ch >= 0 && ch < NCH ? ch : -1;
        else if (sscanf(l, "%d %d %d", &ch, &u, &g) == 3 && ch >= 0 && ch < NCH) {
            s_usos[ch] = (signed char)(u < 0 ? 0 : u > USOS ? USOS : u);
            s_ganado[ch] = (unsigned char)(g != 0);
        }
    }
    fclose(f);
    fprintf(stderr, "[PRESTADO] leido %s: activo %d\n", p, s_activo);
}

static void guardar(void)
{
    char p[MAX_PATH], tmp[MAX_PATH + 8];
    FILE *f;
    int i;
    ruta(p, sizeof p);
    snprintf(tmp, sizeof tmp, "%s.tmp", p);
    if ((f = fopen(tmp, "wb")) == NULL) {
        fprintf(stderr, "[PRESTADO] no se pudo escribir %s\n", tmp);
        return;
    }
    fprintf(f, "activo %d\r\n", s_activo);
    for (i = 0; i < NCH; i++)
        if (s_usos[i] != USOS || s_ganado[i])
            fprintf(f, "%d %d %d\r\n", i, s_usos[i], s_ganado[i]);
    fclose(f);
    MoveFileExA(tmp, p, MOVEFILE_REPLACE_EXISTING);
}

/* a tournament is being played (the duel setup 0x66DCDC: [+0] 0 in a Duel) */
static int en_torneo(void) { return MEM32(0x0066DCDCu) == 1u; }   /* 1: premios.c's tournament level too */

/* the duelist d of a character (s8 0x423D60[character]), 0 if it has none */
static int duelista(int ch)
{
    int d;
    if (ch < 0 || ch >= NCH)
        return 0;
    d = (int)(int8_t)MEM8(0x00423D60u + (uint32_t)ch);
    return d > 0 && d < 64 ? d : 0;
}

static int k_de(uint32_t id)
{
    uint32_t k;
    for (k = 1; k < NCARTAS; ++k)
        if (MEM16(T_IMAGEN + 2u * k) == id)
            return (int)k;
    return -1;
}

/* the cards the opponent gives (both lists, any level, as premios.c's sign)
 * and how many of them you own */
static void cartas(int d, int *tengo, int *total)
{
    uint16_t ids[400];
    int n = 0, i;
    uint32_t t;
    *tengo = *total = 0;
    /* 2026-11-25: both story variants (premios.c's sign does the same) */
    for (t = 0; t < 4u; ++t) {
        uint32_t tab = (t & 1u) ? 0x00453630u : 0x00453830u, r = t >> 1;
        uint32_t p = MEM32(tab + 16u * (uint32_t)d + 4u * (uint32_t)r);
        uint32_t c = MEM16(tab + 16u * (uint32_t)d + 8u + 2u * (uint32_t)r), e;
        if (!p || c > 512u)
            continue;
        for (e = 0; e < c; ++e) {
            uint32_t id = MEM16(p + 4u * e), f = MEM16(p + 4u * e + 2u);
            int j, dup = 0;
            if (!(f & 0x1FFFu) || !(f & 0xE000u))
                continue;
            for (j = 0; j < n && !dup; ++j)
                dup = ids[j] == id;
            if (!dup && n < 400)
                ids[n++] = (uint16_t)id;
        }
    }
    for (i = 0; i < n; ++i) {
        int k = k_de(ids[i]);
        ++*total;
        if (k > 0 && MEM8(COLECCION + (uint32_t)k))
            ++*tengo;
    }
}

/* 2026-11-25: the cards it gives in a Tournament / Triple duel (its own list
 * at 0x453FC8 + 16 d, both variants; sub_0008C9B0), as premios.c's sign */
static void cartas_torneo(int d, int *tengo, int *total)
{
    uint16_t ids[400];
    int n = 0, i;
    uint32_t r;
    *tengo = *total = 0;
    if (d < 0 || d >= 32)
        return;
    for (r = 0; r < 2u; ++r) {
        uint32_t p = MEM32(0x00453FC8u + 16u * (uint32_t)d + 4u * r);
        uint32_t c = MEM16(0x00453FD0u + 16u * (uint32_t)d + 2u * r), e;
        if (!p || c > 512u)
            continue;
        for (e = 0; e < c; ++e) {
            uint32_t id = MEM16(p + 4u * e), f = MEM16(p + 4u * e + 2u);
            int j, dup = 0;
            if (!(f & 0x1FFFu) || !(f & 0xE000u))
                continue;
            for (j = 0; j < n && !dup; ++j)
                dup = ids[j] == id;
            if (!dup && n < 400)
                ids[n++] = (uint16_t)id;
        }
    }
    for (i = 0; i < n; ++i) {
        int k = k_de(ids[i]);
        ++*total;
        if (k > 0 && MEM8(COLECCION + (uint32_t)k))
            ++*tengo;
    }
}

/* every card it gives in Duels, or every one in Tournaments (2026-11-25, the
 * user: "al completar cualquiera de las 2"), owned: ganado, for good (saved; a
 * later story variant with new cards does not take it back) */
static void revisar_ganado(int ch, int tengo, int total, int ttengo, int ttotal)
{
    if (!s_ganado[ch] && ((total > 0 && tengo >= total) || (ttotal > 0 && ttengo >= ttotal))) {
        s_ganado[ch] = 1;
        guardar();
        fprintf(stderr, "[PRESTADO] mazo del personaje %d GANADO (duelo %d de %d, torneo %d de %d)\n", ch, tengo,
                total, ttengo, ttotal);
        fflush(stderr);
    }
}

/* ---- for the Perfil's deck panel (the game's thread) --------------------- */

typedef struct {
    int puede;                          /* the character has a deck */
    int usos, ganado, activo;           /* uses left; earned; this one is on */
    int tengo, total;                   /* the cards it gives you own / all */
    int ttengo, ttotal;                 /* the same in Tournaments */
} ygo_prestado_info;

void ygo_prestado_info_de(int ch, ygo_prestado_info *o)
{
    int d = duelista(ch);
    memset(o, 0, sizeof *o);
    cargar();
    if (!d)
        return;
    o->puede = 1;
    cartas(d, &o->tengo, &o->total);
    cartas_torneo(d, &o->ttengo, &o->ttotal);
    revisar_ganado(ch, o->tengo, o->total, o->ttengo, o->ttotal);
    o->usos = s_usos[ch];
    o->ganado = s_ganado[ch];
    o->activo = s_activo == ch;
}

/* the opponents' grid: earned (a green tick) */
int ygo_prestado_ganado(int ch)
{
    cargar();
    return ch >= 0 && ch < NCH && s_ganado[ch];
}

/* every opponent checked (the Perfil opens / turns a page; the game's thread):
 * cards won in ordinary duels earn the deck too */
void ygo_prestado_revisar_todos(void)
{
    int ch, tengo, total, tt, tto, d;
    cargar();
    for (ch = 0; ch < NCH; ch++)
        if (!s_ganado[ch] && (d = duelista(ch)) != 0) {
            cartas(d, &tengo, &total);
            cartas_torneo(d, &tt, &tto);
            revisar_ganado(ch, tengo, total, tt, tto);
        }
}

/* the button: lends the deck (1) or gives it back (0) */
void ygo_prestado_usar(int ch, int si)
{
    cargar();
    if (si) {
        if (!duelista(ch) || (!s_ganado[ch] && s_usos[ch] <= 0))
            return;
        s_activo = ch;
    } else if (s_activo == ch)
        s_activo = -1;
    else
        return;
    guardar();
    fprintf(stderr, "[PRESTADO] %s el mazo del personaje %d (usos %d, ganado %d)\n", si ? "se usa" : "se devuelve",
            ch, s_usos[ch], s_ganado[ch]);
    fflush(stderr);
}

/* ---- the duel ------------------------------------------------------------ */

void sub_0007DC40_pad(void);
void sub_0008C760(void);

void sub_0007DC40(void)
{
    uint32_t p = MEM32(g_esp + 4), slot = MEM32(g_esp + 8);
    cargar();
    s_duelo_ch = -1;
    fprintf(stderr, "[PRESTADO] empieza un duelo: modo %u (1 torneo), rival personaje %u, prestamo %d\n",
            MEM32(0x0066DCDCu), MEM32(0x0066DCDCu + 4u), s_activo);
    /* 2026-11-20 23:2x (the user: "puse usar deck y igual me toco el mio";
     * log: "ranura 0 (modo 0)"): both of sub_0007E070's branches call this
     * for the player's own saved deck (setup 0x66DCDC: [+0] 0 in a normal
     * duel, [+8] 0x1C + the slot); the demo builds the player's with
     * sub_0008C760 and never comes here. So: always, for player 0. */
    /* 2026-11-20 (the user: "solo se puedan usar para los Duelos (no para
     * torneos ... te entra con el deck Comun del jugador y tendra que ... pedirlo
     * de nuevo"; earned: "se libera para todo"): a tournament ([0x66DCDC] == 1,
     * its level at +0x14, read in sub_0008C530) gives the loan back unless it
     * is earned */
    if ((p & 1u) == 0u && s_activo >= 0 && en_torneo() && !s_ganado[s_activo]) {
        fprintf(stderr, "[PRESTADO] torneo: el mazo del personaje %d es solo para Duelos; juegas con el tuyo"
                " y el prestamo se cancela\n", s_activo);
        s_activo = -1;
        guardar();
    }
    if ((p & 1u) == 0u && s_activo >= 0) {
        int ch = s_activo, d = duelista(ch);
        if (d && (s_ganado[ch] || s_usos[ch] > 0)) {
            fprintf(stderr, "[PRESTADO] duelo con el mazo del personaje %d (duelista %d, variante %d; usos %d,"
                    " ganado %d); tu mazo (ranura %u) no se toca\n", ch, d, ygo_premios_variante(d), s_usos[ch],
                    s_ganado[ch], slot);
            fflush(stderr);
            MEM32(g_esp + 8) = (uint32_t)d;     /* sub_0008C760(0, d): same two arguments, same return */
            sub_0008C760();
            fprintf(stderr, "[PRESTADO] mazo armado: %u cartas (fusiones %u)\n", MEM8(0x0065D0A9u), MEM8(0x0065D0ABu));
            fflush(stderr);
            s_duelo_ch = ch;
            return;
        }
        fprintf(stderr, "[PRESTADO] el mazo del personaje %d ya no se puede usar: se devuelve\n", ch);
        s_activo = -1;
        guardar();
    }
    fprintf(stderr, "[PRESTADO] mazo del jugador %u: ranura %u (sin prestamo)\n", p & 1u, slot);
    fflush(stderr);
    sub_0007DC40_pad();
}

/* mascota.c / fundas.c: the character whose deck this duel is played with
 * (its mascot, sleeve and tapete go with it), -1: your own */
int ygo_prestado_en_duelo(void)
{
    cargar();
    if (s_duelo_ch >= 0)
        return s_duelo_ch;
    if (s_activo >= 0 && en_torneo() && !s_ganado[s_activo])
        return -1;                      /* a tournament: yours (the loan is given back) */
    return s_activo;
}

/* estadisticas.c, once per duel when it is left: res 'V' won, 'D' lost,
 * 'E' draw, 0 not finished */
void ygo_prestado_fin(char res)
{
    int ch = s_duelo_ch, tengo, total, tt, tto;
    s_duelo_ch = -1;
    if (ch < 0)
        return;
    cartas(duelista(ch), &tengo, &total);
    cartas_torneo(duelista(ch), &tt, &tto);
    revisar_ganado(ch, tengo, total, tt, tto);
    if (res != 'V' || s_ganado[ch]) {
        fprintf(stderr, "[PRESTADO] fin del duelo (%c) con el mazo del personaje %d: no gasta uso (usos %d, ganado %d)\n",
                res ? res : '-', ch, s_usos[ch], s_ganado[ch]);
        fflush(stderr);
        return;
    }
    if (s_usos[ch] > 0)
        s_usos[ch]--;
    fprintf(stderr, "[PRESTADO] victoria con el mazo del personaje %d: quedan %d usos\n", ch, s_usos[ch]);
    if (s_usos[ch] <= 0 && s_activo == ch) {
        s_activo = -1;
        fprintf(stderr, "[PRESTADO] sin usos: el mazo vuelve al rival, vuelve el tuyo\n");
    }
    fflush(stderr);
    guardar();
}
