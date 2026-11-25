/*
 * estadisticas.c - your duels, for the Perfil's second page (pedido del
 * usuario, 2026-11-16: "Estadísticas en el Perfil", mock-up approved).
 *
 * What the game does (read in the code): the win check (sub_00083AD0 /
 * sub_00083F50) ends the duel when a player's LP (u32 0x65D0A4 + p*0x13F8)
 * is 0 or its "lost" flag (u8 0x65D0AD + p*0x13F8, bit 0: Exodia, no cards
 * ...) is set, and leaves the result in u16 0x65F8DA: bit 0x40 player 0
 * (you) won, 0x80 player 1 won, both a draw (equal LP / both flags); u8
 * 0x65F894 bit 0x10 = the duel is over. The duel scene's state [0x657148]
 * then goes 6 -> 7 (mascota.c watches it and calls ygo_estadisticas_fin).
 * The opponent: character [0x66DCDC + 4] (what the select hands the duel);
 * its name is the game's text 86 + character (0..27), its portrait
 * cf_anim/small/png/cfsNN.png.
 *
 * Kept in YGO_PERFIL (Partidas\Perfil)\duelos.txt, one line per duel:
 *   YYYY-MM-DD HH:MM <character> <V|D|E> <exodia 0/1>
 * The totals are worked out from it (they start the day this went in).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include "recomp/gen/recomp_types.h"
#include "estadisticas.h"

#define MAXD 4096                       /* kept in memory (the file keeps all) */

typedef struct {
    unsigned short anio; unsigned char mes, dia, hora, min; signed char ch; char res; char exodia;
    ULONGLONG base;                     /* savedata.dat's time when it ended (pending ones) */
} duelo;

/* 2026-11-16 (the user: "se guarda siempre o solo si yo guardo la partida?
 * ... podria desfasarse"; chose: with the save; closing mid-duel: not
 * counted): a finished duel waits in memory until the game writes its save
 * (savedata.dat newer than when the duel ended, looked at every half second
 * from the Perfil's thread); closing without saving drops it, as the cards. */
#define MAXP 64
static duelo s_pend[MAXP];              /* finished, not saved yet */
static int s_npend;
BOOL xbox_translate_path(const char *xbox_path, WCHAR *host_path_buf, DWORD buf_size);   /* the kit */

/* the save's last write (0: none) */
static ULONGLONG hora_partida(void)
{
    static WCHAR p[MAX_PATH];
    static int ok = -1;
    WIN32_FILE_ATTRIBUTE_DATA a;
    if (ok < 0)
        ok = xbox_translate_path("\\Device\\Harddisk0\\partition1\\UDATA\\4b4e001e\\0061006D1822\\savedata.dat", p,
                                 MAX_PATH) ? 1 : 0;
    if (!ok || !GetFileAttributesExW(p, GetFileExInfoStandard, &a))
        return 0;
    return ((ULONGLONG)a.ftLastWriteTime.dwHighDateTime << 32) | a.ftLastWriteTime.dwLowDateTime;
}

static CRITICAL_SECTION s_cs;
static int s_cs_ok, s_cargado;
static duelo *s_d;
static int s_n;
static volatile LONG s_version;

void ygo_perfil_ruta(const char *archivo, char *out, size_t cap);   /* perfil_dir.c */
unsigned ygo_ranura(void);
static unsigned s_ranura;               /* the game slot whose duels are loaded */

/* YGO_PERFIL\Ranura N\duelos.txt (2026-11-25: per profile and game slot) */
static void ruta(char *p, size_t cap)
{
    ygo_perfil_ruta("duelos.txt", p, cap);
}

static void agregar(const duelo *d)
{
    if (!s_d && !(s_d = (duelo *)calloc(MAXD, sizeof *s_d)))
        return;
    if (s_n == MAXD) {                  /* the oldest out */
        memmove(s_d, s_d + 1, (MAXD - 1) * sizeof *s_d);
        s_n--;
    }
    s_d[s_n++] = *d;
}

static void iniciar(void)
{
    if (!s_cs_ok) {
        static volatile LONG once;
        if (InterlockedCompareExchange(&once, 1, 0) == 0) {
            InitializeCriticalSection(&s_cs);
            s_cs_ok = 1;
        } else
            while (!s_cs_ok)
                Sleep(1);
    }
    EnterCriticalSection(&s_cs);
    if (s_cargado && s_ranura != ygo_ranura()) {   /* another game slot: its own history */
        s_cargado = 0;
        s_n = 0;
        InterlockedIncrement(&s_version);
    }
    if (!s_cargado) {
        char p[MAX_PATH], l[128];
        FILE *f;
        s_cargado = 1;
        s_ranura = ygo_ranura();
        ruta(p, sizeof p);
        if ((f = fopen(p, "rb")) != NULL) {
            while (fgets(l, sizeof l, f)) {
                int a, m, dd, hh, mi, ch, ex;
                char r;
                duelo d;
                if (sscanf(l, "%d-%d-%d %d:%d %d %c %d", &a, &m, &dd, &hh, &mi, &ch, &r, &ex) != 8
                        || (r != 'V' && r != 'D' && r != 'E') || ch < 0 || ch > 127)
                    continue;
                d.anio = (unsigned short)a;
                d.mes = (unsigned char)m;
                d.dia = (unsigned char)dd;
                d.hora = (unsigned char)hh;
                d.min = (unsigned char)mi;
                d.base = 0;
                d.ch = (signed char)ch;
                d.res = r;
                d.exodia = (char)(ex != 0);
                agregar(&d);
            }
            fclose(f);
        }
        fprintf(stderr, "[ESTADISTICAS] %d duelos leidos de %s\n", s_n, p);
    }
    LeaveCriticalSection(&s_cs);
}

/* the game's thread, each frame of the duel (scene state 6): the win
 * check's result as soon as it says the duel is over */
static unsigned s_visto;                /* 0x100 | the bits, 0: not over yet */
void ygo_estadisticas_mirar(void)
{
    unsigned fin = MEM8(0x0065F894u), flags = MEM16(0x0065F8DAu) & 0xC0u;
    if ((fin & 0x10u) && flags) {
        if (!s_visto)
            fprintf(stderr, "[ESTADISTICAS] el duelo terminó: resultado 0x%02X\n", flags);
        s_visto = 0x100u | flags;
    } else if (!(fin & 0x10u))
        s_visto = 0;                    /* still going (a new duel) */
}

/* the game's thread, once per duel: the scene left 6 */
void ygo_estadisticas_fin(int exodia)
{
    unsigned flags = MEM16(0x0065F8DAu) & 0xC0u, fin = MEM8(0x0065F894u);
    if (s_visto) {                      /* seen while the duel still held it */
        flags = s_visto & 0xC0u;
        fin |= 0x10u;
        s_visto = 0;
    }
    uint32_t ch = MEM32(0x0066DCDCu + 4u);
    char res = flags == 0x40u ? 'V' : flags == 0x80u ? 'D' : 'E';
    SYSTEMTIME t;
    duelo d;
    fprintf(stderr, "[ESTADISTICAS] fin del duelo: resultado 0x%02X (terminado 0x%02X), PV %u / %u, personaje %u,"
            " exodia %d -> %c\n", flags, fin, MEM32(0x0065D0A4u), MEM32(0x0065D0A4u + 0x13F8u), ch, exodia, res);
    fflush(stderr);
    {
        extern void ygo_prestado_fin(char r);
        ygo_prestado_fin((fin & 0x10u) && flags ? res : 0);   /* prestado.c: a win uses one of the loaned deck */
    }
    if (!(fin & 0x10u) || !flags) {     /* not a finished duel (left from a menu ...): nothing */
        fprintf(stderr, "[ESTADISTICAS] no terminado: no se cuenta\n");
        return;
    }
    {
        extern void ygo_guardado_terminado(int gano);
        ygo_guardado_terminado(res == 'V');   /* guardado.c: any result is saved */
    }
    iniciar();
    GetLocalTime(&t);
    d.anio = t.wYear;
    d.mes = (unsigned char)t.wMonth;
    d.dia = (unsigned char)t.wDay;
    d.hora = (unsigned char)t.wHour;
    d.min = (unsigned char)t.wMinute;
    d.base = hora_partida();
    d.ch = (signed char)(ch <= 127u ? ch : 127u);
    d.res = res;
    d.exodia = (char)(exodia && res == 'V');
    EnterCriticalSection(&s_cs);
    if (s_npend == MAXP) {
        memmove(s_pend, s_pend + 1, (MAXP - 1) * sizeof *s_pend);
        s_npend--;
    }
    s_pend[s_npend++] = d;
    InterlockedIncrement(&s_version);
    LeaveCriticalSection(&s_cs);
    fprintf(stderr, "[ESTADISTICAS] pendiente hasta que se guarde la partida (%d)\n", s_npend);
}

/* the Perfil's thread, often: the game saved -> the pending duels go in */
void ygo_estadisticas_revisar(void)
{
    static ULONGLONG antes;
    ULONGLONG ahora = GetTickCount64(), h;
    int i, n = 0;
    if (!s_npend || ahora - antes < 500u)
        return;
    antes = ahora;
    h = hora_partida();
    iniciar();
    EnterCriticalSection(&s_cs);
    for (i = 0; i < s_npend; i++)
        if (h && h > s_pend[i].base)
            n = i + 1;                  /* this one and the ones before it were saved */
    if (n) {
        char p[MAX_PATH];
        FILE *f;
        ruta(p, sizeof p);
        f = fopen(p, "ab");
        for (i = 0; i < n; i++) {
            const duelo *d = &s_pend[i];
            agregar(d);
            if (f)
                fprintf(f, "%04u-%02u-%02u %02u:%02u %d %c %d\r\n", d->anio, d->mes, d->dia, d->hora, d->min,
                        (int)d->ch, d->res, (int)d->exodia);
        }
        if (f)
            fclose(f);
        memmove(s_pend, s_pend + n, (size_t)(s_npend - n) * sizeof *s_pend);
        s_npend -= n;
        InterlockedIncrement(&s_version);
    }
    LeaveCriticalSection(&s_cs);
    if (n)
        fprintf(stderr, "[ESTADISTICAS] partida guardada: %d duelo(s) anotados\n", n);
}

unsigned ygo_estadisticas_version(void)
{
    return (unsigned)s_version;
}

void ygo_estadisticas(ygo_stats *s)
{
    int i, gan[128], per[128];
    memset(s, 0, sizeof *s);
    s->dificil_ch = -1;
    iniciar();
    memset(gan, 0, sizeof gan);
    memset(per, 0, sizeof per);
    EnterCriticalSection(&s_cs);
    for (i = 0; i < s_n + s_npend; i++) {             /* the saved ones, then the pending */
        const duelo *d = i < s_n ? &s_d[i] : &s_pend[i - s_n];
        int c = d->ch >= 0 ? d->ch : 0;
        s->duelos++;
        if (d->res == 'V') {
            s->ganados++;
            gan[c]++;
            s->racha++;
            if (s->racha > s->mejor_racha)
                s->mejor_racha = s->racha;
            if (d->exodia)
                s->exodia++;
        } else {
            if (d->res == 'D') {
                s->perdidos++;
                per[c]++;
            }
            s->racha = 0;
        }
    }
    /* the hardest: the most losses to it, then the fewest wins */
    for (i = 0; i < 128; i++)
        if (per[i] && (s->dificil_ch < 0 || per[i] > s->dificil_p
                       || (per[i] == s->dificil_p && gan[i] < s->dificil_g))) {
            s->dificil_ch = i;
            s->dificil_p = per[i];
            s->dificil_g = gan[i];
        }
    for (i = s_n + s_npend - 1; i >= 0 && s->n_ult < YGO_ULTIMOS; i--) {
        const duelo *d = i < s_n ? &s_d[i] : &s_pend[i - s_n];
        ygo_duelo_vis *u = &s->ult[s->n_ult++];
        u->ch = d->ch;
        u->res = d->res;
        u->dia = d->dia;
        u->mes = d->mes;
    }
    LeaveCriticalSection(&s_cs);
}

/* text n of the game in its current language, as UTF-8 (premios_extra.c's) */
static void texto_juego(uint32_t n, char *out, int cap)
{
    uint32_t lang = MEM32(0x005FC2C0u), tab, base, p;
    wchar_t w[256];
    char a[256];
    int i;
    out[0] = 0;
    if (lang >= 6u)
        return;
    tab = MEM32(0x005FC2DCu + 4u * lang);
    base = MEM32(0x005FC2C4u + 4u * lang);
    if (!tab || n >= MEM32(tab))
        return;
    p = MEM32(tab + 4u + 4u * n) + base;
    for (i = 0; i < 255 && MEM8(p + (uint32_t)i); ++i)
        a[i] = (char)MEM8(p + (uint32_t)i);
    a[i] = 0;
    if (!MultiByteToWideChar(1252, 0, a, -1, w, 256))
        return;
    WideCharToMultiByte(CP_UTF8, 0, w, -1, out, cap, NULL, NULL);
}

/* the record against one opponent (the Perfil's rival deck panel) */
void ygo_estadisticas_rival(int ch, int *g, int *p)
{
    int i;
    *g = *p = 0;
    iniciar();
    EnterCriticalSection(&s_cs);
    for (i = 0; i < s_n + s_npend; i++) {
        const duelo *d = i < s_n ? &s_d[i] : &s_pend[i - s_n];
        if ((d->ch >= 0 ? d->ch : 0) != ch)
            continue;
        if (d->res == 'V')
            ++*g;
        else if (d->res == 'D')
            ++*p;
    }
    LeaveCriticalSection(&s_cs);
}

void ygo_texto_juego(uint32_t n, char *out, int cap) { texto_juego(n, out, cap); }

int ygo_estadisticas_retrato(int ch)
{
    return ch < 0 ? 0 : ch > 27 ? 27 : ch;
}

void ygo_estadisticas_nombre(int ch, char *out, int cap)
{
    texto_juego(86u + (uint32_t)ygo_estadisticas_retrato(ch), out, cap);
    if (!out[0])
        snprintf(out, cap, "?");
}
