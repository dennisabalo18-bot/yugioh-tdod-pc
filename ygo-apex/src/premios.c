/*
 * premios.c - a small sign on "Seleccionar oponente": how many cards the
 * marked opponent can give you when you win (at any of its levels), how
 * many of them you still don't have and how many you already have (pedido
 * del usuario, 2026-11-12; at first "con menos de 3", changed to "Ya tienes").
 *
 * What the game does (checked in the code, sub_0008C7E0, the reward after a
 * win): two lists per duelist d (16 bytes each, at 0x453830 and 0x453630:
 * u32 list[2] at +0, u16 count[2] at +8; the index r = sub_0008C530(d), 0 or
 * 1, from the story progress), entries u32 = u16 card ID | u16 (bits 0-12
 * weight, bit 15 - level: the card can come at that level of the opponent).
 * A card comes with chance max(5, weight x factor[copies]) %. Your copies of
 * a card: u8 at 0x651E70 + 0xCC1 + k (sub_000D95C0; k = the card's index,
 * 0x5FE692[k] = ID).
 *
 * The opponent select screen is sub_00106CE0 (thiscall, the screen object):
 * marked slot +0x3F0 (+0x3EC once the turn ends), character +0x424 + 4*slot,
 * level +0x4DC + 4*[+0x3F4] (what sub_001037D0 hands to the duel on confirm:
 * +4 the character, +8 the level); duelist d = s8 0x423D60[character]
 * (sub_00025BC0 uses that table for the reward).
 *
 * raton.c's wrapper of sub_00106CE0 calls ygo_premios_tick each frame of that
 * screen (the game's thread); the sign is drawn in a layered window of its own
 * (as onda.c's ring: the game's picture is not touched), at the top right of
 * the game, while that screen keeps calling. F2 / Configuración "Cartel de
 * premios" (cartel_premios): Sí (default) / No.
 */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include "capas.h"   /* the recording mode: panels into the picture */
#include "recomp/gen/recomp_types.h"
#include "fuente.h"                 /* the game's own letters (2026-11-15) */

int xbox_MenuIsOpen(void);
int f2_idioma(void);                    /* f2_idiomas.c: -1 Spanish, 0 en, 1 pt, 2 fr, 3 de, 4 it */

#define T_IMAGEN 0x005FE692u
#define NCARTAS 1139u
#define COLECCION (0x00651E70u + 0xCC1u)

static volatile LONG s_on = 1;
int ygo_premios_get(void) { return s_on ? 0 : 1; }
void ygo_premios_set(int i) { InterlockedExchange(&s_on, i == 0); }
/* 2026-11-25 (the user: one "Ayudas" switch for every sign): the signs here
 * follow F2 / Configuración "Ayudas" (premios_extra.c's quien_da row, 2 = No);
 * the old hidden cartel_premios row no longer hides them */
int ygo_quien_get(void);
static int ayudas(void) { (void)s_on; return ygo_quien_get() != 2; }

/* a guest pointer the port may read: a itself when its bytes [a, a + n) are
 * mapped and readable on this PC, else 0. 2026-11-25 (the user: no gods'
 * sign after a tournament, three times): fixed ranges were wrong; the
 * tournament's "Cual quieres?" screen lives at 0x74B3EB70 ([COFRE] dios
 * elegido, 16:4x), past the 64 MB of RAM and its mirrors, and the game reads
 * it there. So the host's own page table says what can be read. */
static int host_legible(uintptr_t p)
{
    MEMORY_BASIC_INFORMATION mi;
    if (!VirtualQuery((LPCVOID)p, &mi, sizeof mi) || mi.State != MEM_COMMIT)
        return 0;
    if (mi.Protect & (PAGE_NOACCESS | PAGE_GUARD))
        return 0;
    return (mi.Protect & (PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READ
                          | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)) != 0;
}

uint32_t ygo_ram_n(uint32_t a, uint32_t n)
{
    if (a < 0x10000u || a > 0xFFFFFFFFu - n)
        return 0;
    if (!host_legible((uintptr_t)XBOX_PTR(a)) || !host_legible((uintptr_t)XBOX_PTR(a + n - 1u)))
        return 0;
    return a;
}

uint32_t ygo_ram(uint32_t a) { return ygo_ram_n(a, 4u); }

static CRITICAL_SECTION s_cs;
static int s_cs_ok;
static ULONGLONG s_at;                  /* the last frame of the screen */
static int s_total, s_falta, s_menos3, s_nivel;

static int k_de(uint32_t id)
{
    uint32_t k;
    for (k = 1; k < NCARTAS; ++k)
        if (MEM16(T_IMAGEN + 2u * k) == id)
            return (int)k;
    return -1;
}

/* sub_0008C530(d): how many of the progress rows name d, minus 1, in 0..1 */
static int variante(int d)
{
    uint32_t n = MEM32(0x0066DCDCu) == 1u ? MEM32(0x0066DCDCu + 0x14u) : MEM32(0x00651958u + 0xACu);
    uint32_t i, j;
    int c = 0;
    if ((int32_t)n <= 0)
        return 0;
    if (n > 256u)
        n = 256u;
    for (i = 0; i < n; ++i)
        for (j = 0; j < 3u; ++j) {
            uint32_t v = MEM32(0x004541F0u + 0x18u * i + 8u * j);
            if ((int)(int8_t)MEM8(v + 0x004541C8u) == d) {
                ++c;
                break;
            }
        }
    c -= 1;
    return c < 0 ? 0 : c > 1 ? 1 : c;
}

int ygo_premios_variante(int d) { return variante(d); }   /* premios_extra.c */

void ygo_premios_tick(uint32_t scr)
{
    static int last_d = -99, last_lvl = -99, last_total = -1, last_falta = -1;
    uint32_t slot, ch, lvl, mask, t, i;
    uint16_t ids[400];
    int n = 0, d, r, total = 0, falta = 0, menos3 = 0;
    if (!ayudas() || !scr)
        return;
    slot = MEM32(scr + 0x3F0u);
    if (slot >= 40u)
        slot = MEM32(scr + 0x3ECu);
    if (slot >= 40u)
        return;
    ch = MEM32(scr + 0x424u + 4u * slot);
    if (ch >= 40u)
        return;
    d = (int)(int8_t)MEM8(0x00423D60u + ch);
    if (d < 0 || d > 40)
        return;
    /* every level of the opponent (+0x3F4 / +0x4DC turned out to be another
     * thing, not the level: always out of 0..2 in the user's log) */
    lvl = 99u;
    mask = 0xE000u;
    r = variante(d);
    /* 2026-11-25 (the user: Yami Yugi had 42 de 42, after the credits 35 and
     * "faltan 3"; the game went back to variant 0): both variants' cards
     * count, everywhere (this sign, the Perfil, the loan's "ganado") */
    for (t = 0; t < 4u; ++t) {
        uint32_t tab = (t & 1u) ? 0x00453630u : 0x00453830u;
        uint32_t r = t >> 1;
        uint32_t p = MEM32(tab + 16u * (uint32_t)d + 4u * (uint32_t)r);
        uint32_t c = MEM16(tab + 16u * (uint32_t)d + 8u + 2u * (uint32_t)r), e;
        if (!p || c > 512u)
            continue;
        for (e = 0; e < c; ++e) {
            uint32_t id = MEM16(p + 4u * e), f = MEM16(p + 4u * e + 2u);
            int j, dup = 0;
            if (!(f & 0x1FFFu) || !(f & mask))
                continue;
            for (j = 0; j < n && !dup; ++j)
                dup = ids[j] == id;
            if (!dup && n < 400)
                ids[n++] = (uint16_t)id;
        }
    }
    for (i = 0; i < (uint32_t)n; ++i) {
        int k = k_de(ids[i]);
        uint32_t c = k > 0 ? MEM8(COLECCION + (uint32_t)k) : 0u;
        ++total;
        if (!c)
            ++falta;
        if (c < 3u)
            ++menos3;
    }
    if (d != last_d || (int)lvl != last_lvl || total != last_total || falta != last_falta) {
        fprintf(stderr, "[PREMIOS] slot %u personaje %u rival %d nivel %u variante %d: %d cartas, "
                "faltan %d, con menos de 3 %d\n", slot, ch, d, lvl, r, total, falta, menos3);
        last_d = d;
        last_lvl = (int)lvl;
        last_total = total;
        last_falta = falta;
    }
    if (!s_cs_ok)
        return;
    EnterCriticalSection(&s_cs);
    s_at = GetTickCount64();
    s_total = total;
    s_falta = falta;
    s_menos3 = menos3;
    s_nivel = lvl <= 2u ? (int)lvl + 1 : 0;
    LeaveCriticalSection(&s_cs);
}

/* ---- the Triple duel (2026-11-25, the user: the cards each of the three
 * still has to give you in Tournaments, "con el mismo estilo que los premios")
 * The screen is sub_001057D0 (raton.c's wrapper calls this each frame): the
 * level +0x3E8 (0..14) and, per level, the battle order at +0x41C + 4 *
 * (3 * level + i) (the swap at 0x105D74 exchanges two; sub_001056D0 copies
 * it to the save), each an index into the level's three characters. A Triple duel's
 * rewards come from sub_0008C9B0 (the setup [0x66DCDC] != 0): the rival's own
 * list at 0x453FC8 + 16 d (both variants counted) and a list common to every
 * rival by level, not counted here (it is not that rival's). ---------------- */

static ULONGLONG s_tri_at;
static char s_tri_nom[3][64];
static int s_tri_tot[3], s_tri_fal[3];

void ygo_nombre_personaje(int ch, char *out, int cap);   /* premios_extra.c */

void ygo_premios_triple_tick(uint32_t scr)
{
    static int last[3] = { -1, -1, -1 }, last_f[3];
    uint32_t lvl, i;
    int chs[3], tot[3], fal[3], cambio = 0;
    char nom[3][64];
    if (!ayudas() || !scr)
        return;
    lvl = MEM32(scr + 0x3E8u);
    if (lvl > 14u)
        return;
    for (i = 0; i < 3u; ++i) {
        /* 2026-11-25 03:4x (the user's captures: the names did not match):
         * +0x41C holds the battle order (0..2, swapped by the player); the
         * character is the level's table at 0x455D50 + 8 * (3 * level + that)
         * (u32 character, u32 LP; read in sub_00103B70 / sub_00104EE0, which
         * name it with text 0x56 + character; 0x1B = a random one) */
        uint32_t o = MEM32(scr + 0x41Cu + 4u * (3u * lvl + i)), ch, r;
        int d, n = 0, j, e;
        uint16_t ids[400];
        if (o > 2u)
            return;
        ch = MEM32(0x00455D50u + 8u * (3u * lvl + o));
        if (ch >= 40u)
            return;
        chs[i] = (int)ch;
        d = (int)(int8_t)MEM8(0x00423D60u + ch);
        tot[i] = fal[i] = 0;
        ygo_nombre_personaje((int)ch, nom[i], sizeof nom[i]);
        if (ch == 0x1Bu)                /* a random rival (the game shows "???") */
            snprintf(nom[i], sizeof nom[i], "???");
        if (d < 0 || d >= 32)
            continue;
        for (r = 0; r < 2u; ++r) {
            uint32_t p = MEM32(0x00453FC8u + 16u * (uint32_t)d + 4u * r);
            uint32_t c = MEM16(0x00453FD0u + 16u * (uint32_t)d + 2u * r);
            if (!p || c > 512u)
                continue;
            for (e = 0; e < (int)c; ++e) {
                uint32_t id = MEM16(p + 4u * (uint32_t)e), f = MEM16(p + 4u * (uint32_t)e + 2u);
                int dup = 0;
                if (!(f & 0x1FFFu) || !(f & 0xE000u))
                    continue;
                for (j = 0; j < n && !dup; ++j)
                    dup = ids[j] == id;
                if (!dup && n < 400)
                    ids[n++] = (uint16_t)id;
            }
        }
        for (j = 0; j < n; ++j) {
            int k = k_de(ids[j]);
            ++tot[i];
            if (k <= 0 || !MEM8(COLECCION + (uint32_t)k))
                ++fal[i];
        }
    }
    for (i = 0; i < 3u; ++i)
        if (chs[i] != last[i] || fal[i] != last_f[i])
            cambio = 1;
    if (cambio) {
        fprintf(stderr, "[PREMIOS] triple nivel %u: personajes %d %d %d, torneo %d/%d %d/%d %d/%d (faltan/total)\n", lvl,
                chs[0], chs[1], chs[2], fal[0], tot[0], fal[1], tot[1], fal[2], tot[2]);
        for (i = 0; i < 3u; ++i) {
            last[i] = chs[i];
            last_f[i] = fal[i];
        }
    }
    if (!s_cs_ok)
        return;
    EnterCriticalSection(&s_cs);
    s_tri_at = GetTickCount64();
    for (i = 0; i < 3u; ++i) {
        snprintf(s_tri_nom[i], sizeof s_tri_nom[i], "%s", nom[i][0] ? nom[i] : "?");
        s_tri_tot[i] = tot[i];
        s_tri_fal[i] = fal[i];
    }
    LeaveCriticalSection(&s_cs);
}

/* ---- the gods' choice after a win (2026-11-25, the user: "podes hacer el
 * cartel de las cartas?", mock-up A) -----------------------------------------
 * "Cual quieres?" is sub_00101FD0 (raton.c's wrapper calls this each frame).
 * Its reward object [+0x1C] already holds the three groups drawn by
 * sub_00025BC0: [rew + 0x3C] cards each, u16 at rew + 0x5C + 10 * group + 2 i;
 * the marked god is MEM32(MEM32(+0x10) + 0x74); sub_00101A00 puts that group
 * in the chest. Group 0 = Obelisco (middle), 1 = Slifer (left), 2 = Ra
 * (right): confirmed with the user's picks and the log. Shown until the pick
 * (cartas.c's chest hook calls ygo_premios_dioses_hecho). */

static ULONGLONG s_dios_at;
static int s_dios_nue[3], s_dios_tot[3], s_dios_sel;
static volatile uint32_t s_dios_hecho;  /* the reward object already given */

void ygo_premios_dioses_hecho(uint32_t scr)
{
    if (scr)
        s_dios_hecho = ygo_ram_n(scr, 0x20u) ? MEM32(scr + 0x1Cu) : 0;
}

void ygo_premios_dioses_tick(uint32_t scr)
{
    static int last_sel = -1, last_n[3] = { -1, -1, -1 };
    uint32_t rew, o, n, g, i;
    int nue[3], sel;
    static ULONGLONG visto;
    ULONGLONG now = GetTickCount64();
    if (now - visto > 1500u)
        s_dios_hecho = 0;              /* a new reward screen (its object may be at the same address) */
    visto = now;
    if (!ayudas() || !scr)
        return;
    if (!ygo_ram_n(scr, 0x20u))
        return;
    rew = ygo_ram_n(MEM32(scr + 0x1Cu), 0x5Cu + 30u);
    o = ygo_ram_n(MEM32(scr + 0x10u), 0x78u);
    /* (review) only pointers into the guest's RAM are read. 2026-11-25 (the
     * user: no sign at the tournament's gods): the low RAM alone rejected
     * the contiguous memory at 0x80000000 (the kit's XBOX_CONTIG_BASE) */
    if (!rew || !o || rew == s_dios_hecho) {
        static uint32_t dicho;
        if (rew != s_dios_hecho && dicho != rew) {
            dicho = rew;
            fprintf(stderr, "[PREMIOS] dioses: sin cartel, pantalla %08X premio %08X objeto %08X\n", scr, rew, o);
        }
        return;
    }
    n = MEM32(rew + 0x3Cu);
    sel = (int)MEM32(o + 0x74u);
    if (n < 1u || n > 5u || sel < 0 || sel > 2) {
        static uint32_t dicho2;
        if (dicho2 != rew) {
            dicho2 = rew;
            fprintf(stderr, "[PREMIOS] dioses: sin cartel, %u cartas, marcado %d\n", n, sel);
        }
        return;
    }
    for (g = 0; g < 3u; ++g) {
        uint16_t vistos[5];
        int nv = 0, j, dup;
        nue[g] = 0;
        for (i = 0; i < n; ++i) {
            uint32_t id = MEM16(rew + 0x5Cu + 10u * g + 2u * i);
            int k = k_de(id);
            for (j = 0, dup = 0; j < nv && !dup; ++j)
                dup = vistos[j] == id;
            if (dup)
                continue;
            vistos[nv++] = (uint16_t)id;
            if (k > 0 && !MEM8(COLECCION + (uint32_t)k))
                ++nue[g];
        }
    }
    if (sel != last_sel || nue[0] != last_n[0] || nue[1] != last_n[1] || nue[2] != last_n[2]) {
        fprintf(stderr, "[PREMIOS] dioses: %u cartas cada uno, nuevas Obelisco %d Slifer %d Ra %d, marcado %d\n", n,
                nue[0], nue[1], nue[2], sel);
        last_sel = sel;
        for (g = 0; g < 3u; ++g)
            last_n[g] = nue[g];
    }
    if (!s_cs_ok)
        return;
    EnterCriticalSection(&s_cs);
    s_dios_at = GetTickCount64();
    s_dios_sel = sel;
    for (g = 0; g < 3u; ++g) {
        s_dios_nue[g] = nue[g];
        s_dios_tot[g] = (int)n;
    }
    LeaveCriticalSection(&s_cs);
}

/* ---- the sign ------------------------------------------------------------ */

static const char *const k_l1[6] = {
    "Premios de este rival: %d cartas", "Rewards from this opponent: %d cards",
    "Prêmios deste rival: %d cartas", "Récompenses de cet adversaire : %d cartes",
    "Belohnungen dieses Gegners: %d Karten", "Premi di questo avversario: %d carte" };
static const char *const k_l2[6] = {
    "Te faltan: %d    Ya tienes: %d", "Missing: %d    You have: %d",
    "Faltam: %d    Você já tem: %d", "Manquantes : %d    Déjà obtenues : %d",
    "Fehlen: %d    Du hast: %d", "Mancano: %d    Ne hai già: %d" };
static const char *const k_tri[6] = {
    "Premios de torneo de estos rivales", "Tournament rewards from these opponents",
    "Prêmios de torneio destes rivais", "Récompenses de tournoi de ces adversaires",
    "Turnierbelohnungen dieser Gegner", "Premi del torneo di questi avversari" };
static const char *const k_tri_nada[6] = {
    "No da cartas", "Gives no cards", "Não dá cartas", "Ne donne pas de cartes", "Gibt keine Karten",
    "Non dà carte" };
static const char *const k_dios_t[6] = {
    "Cartas nuevas de cada dios", "New cards from each god", "Cartas novas de cada deus",
    "Nouvelles cartes de chaque dieu", "Neue Karten jedes Gottes", "Carte nuove di ogni dio" };
static const char *const k_dios_n[6] = {
    "Nuevas: %d de %d", "New: %d of %d", "Novas: %d de %d", "Nouvelles : %d sur %d", "Neu: %d von %d",
    "Nuove: %d di %d" };
static const char *const k_obelisco[6] = { "Obelisco", "Obelisk", "Obelisco", "Obélisque", "Obelisk", "Obelisco" };
static const char *const k_nada[6] = {
    "Este rival no da cartas", "This opponent gives no cards", "Este rival não dá cartas",
    "Cet adversaire ne donne pas de cartes", "Dieser Gegner gibt keine Karten",
    "Questo avversario non dà carte" };

static void wide(const char *u8, wchar_t *w, int n)
{
    if (!MultiByteToWideChar(CP_UTF8, 0, u8, -1, w, n))
        w[0] = 0;
}

static DWORD WINAPI premios_thread(LPVOID unused)
{
    WNDCLASSA wc;
    HWND win = NULL;
    HDC screen, mem;
    HBITMAP bmp = NULL, old = NULL;
    HFONT font = NULL;
    unsigned char *bits = NULL;
    int bw = 0, bh = 0, fh = 0, shown = 0;
    char last[256] = "";

    (void)unused;
    memset(&wc, 0, sizeof wc);
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(NULL);
    wc.lpszClassName = "YgoCartelPremios";
    RegisterClassA(&wc);
    screen = GetDC(NULL);
    mem = CreateCompatibleDC(screen);

    for (;;) {
        MSG m;
        HWND g = FindWindowA("RecompNv2aD3D11", NULL);
        ULONGLONG at, tat, dat;
        int total, falta, want, tri, tt[3], tf[3], i3, dios, dn[3], dt[3], dsel;
        char tn[3][64];
        RECT cr;
        POINT tl = { 0, 0 };

        while (PeekMessageA(&m, NULL, 0, 0, PM_REMOVE))
            DispatchMessageA(&m);
        EnterCriticalSection(&s_cs);
        at = s_at;
        total = s_total;
        falta = s_falta;
        tat = s_tri_at;
        dat = s_dios_at;
        dsel = s_dios_sel;
        for (i3 = 0; i3 < 3; ++i3) {
            dn[i3] = s_dios_nue[i3];
            dt[i3] = s_dios_tot[i3];
        }
        for (i3 = 0; i3 < 3; ++i3) {
            snprintf(tn[i3], sizeof tn[i3], "%s", s_tri_nom[i3]);
            tt[i3] = s_tri_tot[i3];
            tf[i3] = s_tri_fal[i3];
        }
        LeaveCriticalSection(&s_cs);
        dios = GetTickCount64() - dat < 300u;                /* the gods' choice */
        tri = !dios && GetTickCount64() - tat < 300u && tat > at;     /* the Triple screen (the newer of the two) */
        want = ayudas() && g && GetForegroundWindow() == g && !xbox_MenuIsOpen() && !IsIconic(g)
               && (GetTickCount64() - at < 300u || tri || dios) && GetClientRect(g, &cr) && cr.bottom > 100;

        if (!want) {
            if (shown && win)
                ShowWindow(win, SW_HIDE);
            shown = 0;
            last[0] = 0;
            Sleep(50);
            continue;
        }

        {
            int lang = f2_idioma() + 1, h, w, pad, x, y, rad, nl, j, colw = 0, numw = 0;
            char l1[128], fil[3][2][128], key[600];
            int oro[3];
            wchar_t w1[128], wf[3][2][128];
            SIZE s1, sf[3][2];
            RECT rc;
            if (lang < 0 || lang > 5)
                lang = 1;
            memset(fil, 0, sizeof fil);
            memset(oro, 0, sizeof oro);
            /* the lines: the first in gold; under it the rows (white), each a
             * left part and, in the Triple, the numbers in a column of their own */
            if (dios) {
                /* as on screen: Slifer (group 1) left, Obelisco (0) middle, Ra (2) right;
                 * the marked one in gold */
                static const int grupo[3] = { 1, 0, 2 };
                snprintf(l1, sizeof l1, "%s", k_dios_t[lang]);
                for (j = 0; j < 3; ++j) {
                    int gq = grupo[j];
                    snprintf(fil[j][0], sizeof fil[j][0], "%s", gq == 1 ? "Slifer" : gq == 2 ? "Ra" : k_obelisco[lang]);
                    snprintf(fil[j][1], sizeof fil[j][1], k_dios_n[lang], dn[gq], dt[gq]);
                    oro[j] = gq == dsel;
                }
                nl = 4;
            } else if (tri) {
                snprintf(l1, sizeof l1, "%s", k_tri[lang]);
                for (j = 0; j < 3; ++j) {
                    snprintf(fil[j][0], sizeof fil[j][0], "%s", tn[j]);
                    if (tt[j])
                        snprintf(fil[j][1], sizeof fil[j][1], k_l2[lang], tf[j], tt[j] - tf[j]);
                    else
                        snprintf(fil[j][1], sizeof fil[j][1], "%s", k_tri_nada[lang]);
                }
                nl = 4;
            } else if (total) {
                snprintf(l1, sizeof l1, k_l1[lang], total);
                snprintf(fil[0][0], sizeof fil[0][0], k_l2[lang], falta, total - falta);
                nl = 2;
            } else {
                snprintf(l1, sizeof l1, "%s", k_nada[lang]);
                nl = 1;
            }
            snprintf(key, sizeof key, "%s|%s%s|%s%s|%s%s|%d%d%d|%ld|%ld", l1, fil[0][0], fil[0][1], fil[1][0],
                     fil[1][1], fil[2][0], fil[2][1], oro[0], oro[1], oro[2], cr.right, cr.bottom);
            ClientToScreen(g, &tl);
            h = cr.bottom / 34;
            if (h < 14)
                h = 14;
            if (h != fh) {
                if (font)
                    DeleteObject(font);
                font = CreateFontA(-h, 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                   CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH, "Segoe UI");
                fh = h;
            }
            pad = h / 2;
            if (strcmp(key, last)) {
                BITMAPINFO bi;
                HFONT of;
                int yy, xx, gap = fh;
                wide(l1, w1, 128);
                fj_medir(w1, fh, &s1);      /* the game's own letters (fuente.c) */
                for (j = 0; j < 3; ++j) {
                    int c;
                    for (c = 0; c < 2; ++c) {
                        wide(fil[j][c], wf[j][c], 128);
                        if (wf[j][c][0])
                            fj_medir(wf[j][c], fh, &sf[j][c]);
                        else
                            sf[j][c].cx = sf[j][c].cy = 0;
                    }
                    if (sf[j][0].cx > colw)
                        colw = sf[j][0].cx;
                    if (sf[j][1].cx > numw)
                        numw = sf[j][1].cx;
                }
                w = colw + (numw ? gap + numw : 0);
                if (s1.cx > w)
                    w = s1.cx;
                w += 2 * pad;
                h = nl * s1.cy + 2 * pad - pad / 3;
                if (w != bw || h != bh) {
                    if (bmp) {
                        SelectObject(mem, old);
                        DeleteObject(bmp);
                    }
                    memset(&bi, 0, sizeof bi);
                    bi.bmiHeader.biSize = sizeof bi.bmiHeader;
                    bi.bmiHeader.biWidth = w;
                    bi.bmiHeader.biHeight = -h;
                    bi.bmiHeader.biPlanes = 1;
                    bi.bmiHeader.biBitCount = 32;
                    bi.bmiHeader.biCompression = BI_RGB;
                    bmp = CreateDIBSection(screen, &bi, DIB_RGB_COLORS, (void **)&bits, NULL, 0);
                    if (!bmp) {
                        bw = bh = 0;
                        Sleep(50);
                        continue;
                    }
                    old = (HBITMAP)SelectObject(mem, bmp);
                    bw = w;
                    bh = h;
                }
                /* the text alone first, as coverage: line 1 in pure red and line
                 * 2 in pure green on black, grayscale anti-aliasing (no
                 * ClearType colours), so each channel is that line's coverage */
                memset(bits, 0, (size_t)bw * (size_t)bh * 4u);
                rc.left = pad; rc.top = pad - pad / 6; rc.right = bw; rc.bottom = bh;
                fj_dibujar(bits, bw, bh, &rc, w1, fh, FJ_R, 0);
                for (j = 0; j < nl - 1; ++j) {
                    rc.top += s1.cy;
                    rc.left = pad;
                    if (wf[j][0][0])
                        fj_dibujar(bits, bw, bh, &rc, wf[j][0], fh, oro[j] ? FJ_R : FJ_G, 0);
                    if (wf[j][1][0]) {
                        rc.left = pad + colw + gap;
                        fj_dibujar(bits, bw, bh, &rc, wf[j][1], fh, oro[j] ? FJ_R : FJ_G, 0);
                    }
                }
                rc.left = pad;
                (void)of;
                /* then the panel under it, smooth: a rounded rectangle by its
                 * distance to the edge (anti-aliased outline and corners), dark
                 * brown with a gold edge (the game's colours), 92 % opaque,
                 * premultiplied */
                rad = pad;
                {
                    double edge = fh / 16.0 < 1.6 ? 1.6 : fh / 16.0;      /* gold edge width */
                    double hw = bw / 2.0, hh = bh / 2.0;
                    for (yy = 0; yy < bh; ++yy)
                        for (xx = 0; xx < bw; ++xx) {
                            unsigned char *p = bits + 4 * (yy * bw + xx);
                            double qx = fabs(xx + 0.5 - hw) - (hw - rad - 0.5);
                            double qy = fabs(yy + 0.5 - hh) - (hh - rad - 0.5);
                            double ox = qx > 0 ? qx : 0, oy = qy > 0 ? qy : 0;
                            double dist = sqrt(ox * ox + oy * oy) + (qx > qy ? (qx < 0 ? qx : 0) : (qy < 0 ? qy : 0)) - rad;
                            double a = 0.5 - dist, gold = dist + edge + 0.5;
                            double t1 = p[2] / 255.0, t2 = p[1] / 255.0, r, g, b;
                            a = a < 0 ? 0 : a > 1 ? 1 : a;
                            gold = gold < 0 ? 0 : gold > 1 ? 1 : gold;
                            r = 34 + (215 - 34) * gold;
                            g = 22 + (165 - 22) * gold;
                            b = 12 + (60 - 12) * gold;
                            r += (255 - r) * t1; g += (226 - g) * t1; b += (140 - b) * t1;   /* line 1, gold */
                            r += (245 - r) * t2; g += (245 - g) * t2; b += (245 - b) * t2;   /* line 2, white */
                            a *= 0.92;
                            p[0] = (unsigned char)(b * a + 0.5);
                            p[1] = (unsigned char)(g * a + 0.5);
                            p[2] = (unsigned char)(r * a + 0.5);
                            p[3] = (unsigned char)(255 * a + 0.5);
                        }
                }
                snprintf(last, sizeof last, "%s", key);
            }
            if (!win)
                win = CreateWindowExA(WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW
                                      | WS_EX_TOPMOST, wc.lpszClassName, "", WS_POPUP, 0, 0, 1, 1,
                                      NULL, NULL, wc.hInstance, NULL);
            if (win && bmp) {
                POINT dst, src = { 0, 0 };
                SIZE sz;
                BLENDFUNCTION bf = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
                x = tl.x + cr.right - bw - cr.bottom / 40;
                y = tl.y + cr.bottom / 40;
                dst.x = x;
                dst.y = y;
                sz.cx = bw;
                sz.cy = bh;
                UpdateLayeredWindow(win, screen, &dst, &sz, mem, &src, 0, &bf, ULW_ALPHA);
                if (!shown)
                    SetWindowPos(win, HWND_TOPMOST, 0, 0, 0, 0,
                                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
                shown = 1;
            }
        }
        Sleep(30);
    }
}

void ygo_premios_start(void)
{
    static int started;
    if (started)
        return;
    started = 1;
    InitializeCriticalSection(&s_cs);
    s_cs_ok = 1;
    CloseHandle(CreateThread(NULL, 0, premios_thread, NULL, 0, NULL));
}
