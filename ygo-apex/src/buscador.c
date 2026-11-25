/*
 * buscador.c - the Biblioteca's search (2026-11-16, the user: "si hacemos un
 * buscador estilo master duel mostrame como quedaria", mock-up approved:
 * "hacela asi pero usa la fuente de texto del juego y que todo se parezca lo
 * mas parecido al juego para no desentonar").
 *
 * Nothing of the game is replaced: as perfil.c / cartel.c, everything is
 * drawn in layered click-through windows of our own over the game's window:
 * - the button "Buscar (F3)" on the Biblioteca's right, while its grid shows;
 * - the panel (F3 or the button; F3, Esc or a right click on it close it):
 *   the Perfil's gold frame and stone (Mods-Extras\buscador, made by
 *   tools/buscador_recursos.py from the game's own pictures), the field on
 *   the Perfil's gold plate, the attributes' icons (the cards' own), chips
 *   for the kind, the level and "only new", a minimum ATK bar, and the list
 *   of the cards found with the game's own thumbnails; all text in the
 *   game's letters (fuente.h);
 * - over the grid: the cards that do not match are darkened, the ones that
 *   do get a gold edge (where the "Nueva" badges go: cells 35 x 51 at
 *   94 + 40 col, 120 + 57 row, game px).
 * A result clicked: the game's cursor goes to that card (raton.c,
 * ygo_biblioteca_ir: the scroll bar's top row and the pointer's cursor).
 * Only the cards you have are searched (the others show face down with no
 * name). The grid itself is the game's and is not filtered.
 *
 * raton.c's Biblioteca part calls ygo_buscador_raton first, on the game's
 * thread; this file's thread draws.
 */
#define COBJMACROS
#include "mods_carpeta.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include "capas.h"   /* the recording mode: panels into the picture */
#include <wincodec.h>
#include "recomp/gen/recomp_types.h"
#include "fuente.h"

#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "ole32.lib")

int xbox_MenuIsOpen(void);
int nv2a_d3d11_window_to_guest(int px, int py, int cw, int ch, float *gx, float *gy, float *dev);
int f2_idioma(void);
int ygo_carta_nueva(int k);                     /* premios_extra.c */
int ygo_carta_agregada(int k);                  /* cartas.c */
int ygo_cartas_hay_agregadas(void);
unsigned ygo_limite_carta(uint32_t id);     /* cartas.c */
void ygo_biblioteca_ir(int idx);                /* raton.c */

#define NCARTAS 1139u
#define T_IMAGEN 0x005FE692u                    /* k -> image ID */
#define T_DATOS 0x005FEF78u                     /* k -> DEF/10 0-8, ATK/10 9-17, class 18-19, type 20-24, level 25-28, attribute 29-31 */
#define LISTA 0x0066DD20u                       /* the grid: place -> k */
#define COPIAS (0x00651E70u + 0xCC1u)           /* your copies of k */
#define TXT_NOMBRE 849u
#define TXT_DESC 1988u

/* the panel, game frame px (its height; its width is the room right of the grid) */
/* 2026-11-16 (the user's capture: it covered the header and the grid's
 * edge): under the header (its lower edge ~ y 87), right of the grid's
 * frame (~ x 477) */
#define P_X0 478.f
#define P_X1 634.f
#define P_Y0 92.f
#define P_H 312.f
/* the button (before: a gold plate "Buscar (F3)", then a box like the
 * game's; the user: "pasa desapercibido", "se sigue viendo mal") */
/* the button (2026-11-17, the user: "Mete una Lupa bien Hecha en el medio al
 * lado de donde esta lo redondo entre los numeros"): a medallion like the
 * game's round one between the card number boxes (centre 538, 155.5, r 14.6
 * game px, measured on the capture), left of it */
#define L_CX 504.f
#define L_CY 155.5f
#define L_R 14.6f

/* ---- the texts (the game's language: es en pt fr de it) ---- */
static const wchar_t *const k_buscar[6] = { L"Buscar", L"Search", L"Buscar", L"Rechercher", L"Suchen", L"Cerca" };
static const wchar_t *const k_campo[6] = { L"Nombre o texto", L"Name or text", L"Nome ou texto", L"Nom ou texte",
                                           L"Name oder Text", L"Nome o testo" };
static const wchar_t *const k_tipo[6][6] = {
    { L"Normal", L"Efecto", L"Fusión", L"Ritual", L"Mágica", L"Trampa" },
    { L"Normal", L"Effect", L"Fusion", L"Ritual", L"Spell", L"Trap" },
    { L"Normal", L"Efeito", L"Fusão", L"Ritual", L"Magia", L"Armadilha" },
    { L"Normal", L"Effet", L"Fusion", L"Rituel", L"Magie", L"Piège" },
    { L"Normal", L"Effekt", L"Fusion", L"Ritual", L"Zauber", L"Falle" },
    { L"Normale", L"Effetto", L"Fusione", L"Rituale", L"Magia", L"Trappola" } };
static const wchar_t *const k_nv[6] = { L"Nv", L"Lv", L"Nv", L"Niv", L"St", L"Liv" };
static const wchar_t *const k_nuevas[6] = { L"Solo nuevas", L"New only", L"Só novas", L"Nouvelles", L"Nur neue", L"Solo nuove" };
static const wchar_t *const k_limitadas[6] = { L"Limitadas", L"Limited", L"Limitadas", L"Limitées", L"Limitiert",
                                               L"Limitate" };
static const wchar_t *const k_agregadas[6] = { L"Agregadas", L"Added", L"Adicionadas", L"Ajoutées", L"Hinzugefügt",
                                               L"Aggiunte" };
static const wchar_t *const k_borrar[6] = { L"Borrar", L"Clear", L"Limpar", L"Effacer", L"Löschen", L"Azzera" };
static const wchar_t *const k_res[6] = { L"%d resultados", L"%d results", L"%d resultados", L"%d résultats",
                                         L"%d Treffer", L"%d risultati" };
static const wchar_t *const k_uno[6] = { L"1 resultado", L"1 result", L"1 resultado", L"1 résultat", L"1 Treffer",
                                         L"1 risultato" };
static const wchar_t *const k_nada[6] = { L"Sin resultados", L"No results", L"Sem resultados", L"Aucun résultat",
                                          L"Keine Treffer", L"Nessun risultato" };

/* 2026-11-17 (the user: "que funcione en los 5 idiomas"): the panel's words
 * in the game's own language, the one its card names are read in
 * (0x5FC2C0: 0 jp, 1 en, 2 de, 3 fr, 4 it, 5 es); F2's is only taken by
 * the game at the next start, so the two could differ for a while */
static int lang6(void)
{
    static const int k_de_juego[6] = { 1, 1, 4, 3, 5, 0 };     /* -> es en pt fr de it */
    uint32_t g = MEM32(0x005FC2C0u);
    if (g < 6u)
        return k_de_juego[g];
    {
        int l = f2_idioma() + 1;
        return l < 0 || l > 5 ? 0 : l;
    }
}

/* ---- shared state (the game's thread writes, ours draws) ---------------- */
static CRITICAL_SECTION s_cs;
static volatile LONG s_ok;
static LONG s_version = 1;
static ULONGLONG s_lib_at;              /* the grid was up then (raton.c each frame) */
static int s_abierto, s_editando, s_cursor_vis;
static wchar_t s_q[33];                 /* what is typed */
static unsigned s_atrib;                /* bit a (1..6) */
static unsigned s_tipos;                /* bits 0..5: normal effect fusion ritual spell trap */
static unsigned s_niveles;              /* bits 0..2: 1-4 5-6 7+ */
static int s_nuevas;
static int s_limitadas, s_agregadas;    /* 2026-11-19: "Limitadas", "Agregadas" (the card mod's) */
static int s_atk;                       /* minimum ATK, 0..4000 */
static int s_arrastre;                  /* the ATK bar held */
static int s_sobre = -1;                /* what the pointer is on (Z_*) */
static int s_sobre_fila = -1;
static int s_lista0;                    /* the first result shown */
static int s_boton_sobre;
/* the results: grid places, in the grid's order (names first) */
#define NRES 1139
static int s_res[NRES], s_nres;
static unsigned char s_coincide[NRES];  /* by place */
static int s_activo;                    /* some filter is on */
/* the mapping game frame -> window px */
static float s_ax, s_ay, s_sx = 1.f, s_sy = 1.f;
static int s_cw, s_ch;

static void cambio(void) { InterlockedIncrement(&s_version); }

/* ---- the panel's layout: units of u window px (u = sy), wu units wide ---- */
enum { Z_NADA = -1, Z_LUPA = -2, Z_CAMPO = 0, Z_ATRIB = 1, Z_TIPO = 10, Z_NIVEL = 20, Z_NUEVAS = 30, Z_BORRAR, Z_ATK, Z_LISTA, Z_PANEL, Z_LIMITADAS, Z_AGREGADAS };
#define PAD 10.f
#define Y_TITULO 7.f
#define Y_CAMPO 28.f
#define H_CAMPO 20.f
#define Y_ATRIB 62.f
#define R_ATRIB 10.f
#define Y_TIPO 80.f
#define H_CHIP 16.f
#define Y_NIVEL 120.f
#define Y_ATK 141.f
#define Y_EXTRA 162.f
#define Y_EXTRA2 182.f
#define Y_RES 203.f
#define Y_LISTA 220.f
#define H_FILA 20.f
#define NFILAS 4
#define Y_PIE 326.f

typedef struct { float x, y, w, h; } caja;

static float panel_wu(void)
{
    float w = (P_X1 - P_X0) * s_sx, u = s_sy;
    return u > 0.f ? w / u : 172.f;
}

static caja z_campo(float wu) { caja c = { PAD, Y_CAMPO, wu - 2 * PAD, H_CAMPO }; return c; }
static caja z_atrib(float wu, int i)
{
    float paso = (wu - 2 * PAD) / 6.f;
    caja c = { PAD + paso * ((float)i + 0.5f) - R_ATRIB, Y_ATRIB - R_ATRIB, 2 * R_ATRIB, 2 * R_ATRIB };
    return c;
}
static caja z_chip(float wu, float y, int i, int n)
{
    float g = 4.f, w = (wu - 2 * PAD - g * (float)(n - 1)) / (float)n;
    caja c = { PAD + (w + g) * (float)i, y, w, H_CHIP };
    return c;
}
static caja z_tipo(float wu, int i) { return z_chip(wu, Y_TIPO + (float)(i / 3) * (H_CHIP + 4.f), i % 3, 3); }
static caja z_nivel(float wu, int i) { return z_chip(wu, Y_NIVEL, i, 3); }
static caja z_extra(float wu, int i) { return z_chip(wu, Y_EXTRA, i, 2); }
/* the second row: Agregadas | Borrar, or Borrar alone without the card mod */
static caja z_extra2(float wu, int i)
{
    if (!ygo_cartas_hay_agregadas())
        return z_chip(wu, Y_EXTRA2, 0, 1);
    return z_chip(wu, Y_EXTRA2, i, 2);
}
static caja z_atk(float wu) { caja c = { PAD + 62.f, Y_ATK + 3.f, wu - 2 * PAD - 62.f, 10.f }; return c; }
static caja z_fila(float wu, int i) { caja c = { PAD - 2.f, Y_LISTA + H_FILA * (float)i, wu - 2 * PAD + 4.f, H_FILA }; return c; }
static int dentro(caja c, float x, float y) { return x >= c.x && x < c.x + c.w && y >= c.y && y < c.y + c.h; }

/* game px -> panel units */
static void a_panel(float gx, float gy, float *x, float *y)
{
    *x = (gx - P_X0) * s_sx / s_sy;
    *y = (gy - P_Y0);
}

static int que_hay(float x, float y, int *fila)
{
    float wu = panel_wu();
    int i;
    *fila = -1;
    if (x < 0 || y < 0 || x >= wu || y >= P_H)
        return Z_NADA;
    {
        float dx = x - (PAD + 9.f), dy = y - (Y_TITULO + 8.f);
        if (dx * dx + dy * dy <= 11.f * 11.f)
            return Z_LUPA;
    }
    if (dentro(z_campo(wu), x, y))
        return Z_CAMPO;
    for (i = 0; i < 6; i++) {
        caja c = z_atrib(wu, i);
        float dx = x - c.x - R_ATRIB, dy = y - c.y - R_ATRIB;
        if (dx * dx + dy * dy <= (R_ATRIB + 2.f) * (R_ATRIB + 2.f))
            return Z_ATRIB + i;
    }
    for (i = 0; i < 6; i++)
        if (dentro(z_tipo(wu, i), x, y))
            return Z_TIPO + i;
    for (i = 0; i < 3; i++)
        if (dentro(z_nivel(wu, i), x, y))
            return Z_NIVEL + i;
    if (dentro(z_extra(wu, 0), x, y))
        return Z_NUEVAS;
    if (dentro(z_extra(wu, 1), x, y))
        return Z_LIMITADAS;
    if (ygo_cartas_hay_agregadas()) {
        if (dentro(z_extra2(wu, 0), x, y))
            return Z_AGREGADAS;
        if (dentro(z_extra2(wu, 1), x, y))
            return Z_BORRAR;
    } else if (dentro(z_extra2(wu, 0), x, y))
        return Z_BORRAR;
    {
        caja a = z_atk(wu);
        a.y -= 5.f;
        a.h += 10.f;
        a.x -= 6.f;
        a.w += 12.f;
        if (dentro(a, x, y))
            return Z_ATK;
    }
    for (i = 0; i < NFILAS; i++)
        if (dentro(z_fila(wu, i), x, y)) {
            *fila = i;
            return Z_LISTA;
        }
    return Z_PANEL;
}

/* ---- the search ------------------------------------------------------------ */
/* a cp1252 letter, lower case and without its accent */
static unsigned char plano(unsigned char c)
{
    static const char *const de = "\xC0\xC1\xC2\xC3\xC4\xC5\xE0\xE1\xE2\xE3\xE4\xE5\xC8\xC9\xCA\xCB\xE8\xE9\xEA\xEB"
                                  "\xCC\xCD\xCE\xCF\xEC\xED\xEE\xEF\xD2\xD3\xD4\xD5\xD6\xF2\xF3\xF4\xF5\xF6"
                                  "\xD9\xDA\xDB\xDC\xF9\xFA\xFB\xFC\xC7\xE7\xD1\xF1";
    static const char *const a = "aaaaaaaaaaaaeeeeeeeeiiiiiiiiooooooooooouuuuuuuuccnn";
    const char *p;
    if (c >= 'A' && c <= 'Z')
        return (unsigned char)(c + 32);
    if (c >= 0xC0 && (p = strchr(de, (char)c)))
        return (unsigned char)a[p - de];
    return c;
}

/* game text n (its current language), flattened, into out */
static void texto_plano(uint32_t n, char *out, int cap)
{
    uint32_t lang = MEM32(0x005FC2C0u), tab, base, p;
    int i;
    out[0] = 0;
    if (lang >= 6u)
        return;
    tab = MEM32(0x005FC2DCu + 4u * lang);
    base = MEM32(0x005FC2C4u + 4u * lang);
    if (!tab || n >= MEM32(tab))
        return;
    p = MEM32(tab + 4u + 4u * n) + base;
    for (i = 0; i < cap - 1 && MEM8(p + (uint32_t)i); ++i)
        out[i] = (char)plano(MEM8(p + (uint32_t)i));
    out[i] = 0;
}

/* game text n as it is (wide) */
static void texto_ancho(uint32_t n, wchar_t *out, int cap)
{
    uint32_t lang = MEM32(0x005FC2C0u), tab, base, p;
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
    if (!MultiByteToWideChar(1252, 0, a, -1, out, cap))
        out[0] = 0;
}

static int filtros_activos(void)
{
    return s_q[0] || s_atrib || s_tipos || s_niveles || s_nuevas || s_limitadas || s_agregadas || s_atk > 0;
}

/* the card k by the chips (not the text) */
static int pasa(uint32_t k)
{
    uint32_t d = MEM32(T_DATOS + 4u * k), tipo = (d >> 20) & 0x1Fu, clase = (d >> 18) & 3u;
    uint32_t nivel = (d >> 25) & 0xFu, atr = (d >> 29) & 7u, atk = ((d >> 9) & 0x1FFu) * 10u;
    int monstruo = tipo < 21u;
    if (s_atrib && (!monstruo || !(s_atrib & (1u << atr))))
        return 0;
    if (s_tipos) {
        unsigned b = tipo == 22u ? 16u : tipo == 21u ? 32u : 1u << clase;
        if (!(s_tipos & b))
            return 0;
    }
    if (s_niveles) {
        unsigned b = !monstruo ? 0u : nivel <= 4u ? 1u : nivel <= 6u ? 2u : 4u;
        if (!(s_niveles & b))
            return 0;
    }
    if (s_atk > 0 && (!monstruo || (int)atk < s_atk))
        return 0;
    if (s_nuevas && !ygo_carta_nueva((int)k))
        return 0;
    if (s_limitadas) {
        /* 2026-11-19 (the user: "cuando pones limitadas no te marca ninguna
         * carta"): the save's byte (u8 at 0x665896 + k) is 0 for every card
         * outside the deck screen; the card's own limit (cartas.c: the mod's,
         * then the game's list) is 1 or 2 */
        uint32_t t = ygo_limite_carta(MEM16(T_IMAGEN + 2u * k));
        if (t < 1u || t > 2u)
            return 0;
    }
    if (s_agregadas && !ygo_carta_agregada((int)k))
        return 0;
    return 1;
}

/* the game's thread, with s_cs held: the results again */
static void buscar(void)
{
    char q[64], t[1024];
    int idx, pase, n = 0;
    s_activo = filtros_activos();
    memset(s_coincide, 0, sizeof s_coincide);
    s_nres = 0;
    if (!s_activo)
        return;
    {
        char a[64];
        BOOL usado = FALSE;
        int i;
        if (WideCharToMultiByte(1252, 0, s_q, -1, a, sizeof a, "?", &usado) <= 0)
            a[0] = 0;
        for (i = 0; a[i]; i++)
            q[i] = (char)plano((unsigned char)a[i]);
        q[i] = 0;
    }
    /* first the names, then the texts */
    for (pase = 0; pase < 2; pase++)
        for (idx = 0; idx < NRES; idx++) {
            uint32_t k = MEM16(LISTA + 2u * (uint32_t)idx);
            if (!k || k >= NCARTAS || !MEM8(COPIAS + k) || s_coincide[idx] || !pasa(k))
                continue;
            if (q[0]) {
                texto_plano(TXT_NOMBRE + k, t, sizeof t);
                if (!strstr(t, q)) {
                    if (pase == 0)
                        continue;
                    texto_plano(TXT_DESC + k, t, sizeof t);
                    if (!strstr(t, q))
                        continue;
                }
            } else if (pase == 1)
                continue;
            s_coincide[idx] = 1;
            if (n < NRES)
                s_res[n++] = idx;
        }
    s_nres = n;
    if (s_lista0 > n - NFILAS)
        s_lista0 = n - NFILAS;
    if (s_lista0 < 0)
        s_lista0 = 0;
}

/* ---- pictures ------------------------------------------------------------- */
typedef struct { int w, h; unsigned char *p; } img;   /* premultiplied BGRA */
static const GUID k_clsid_wic = { 0xcacaf262, 0x9370, 0x4615, { 0xa1, 0x3b, 0x9f, 0x55, 0x39, 0xda, 0x4c, 0x0a } };
static const GUID k_iid_wic = { 0xec5ec8a9, 0xc395, 0x4314, { 0x9c, 0x77, 0x54, 0xd7, 0xa9, 0x35, 0xff, 0x70 } };
static const GUID k_pbgra = { 0x6fddc324, 0x4e03, 0x4bfe, { 0xb1, 0x85, 0x3d, 0x77, 0x76, 0x8d, 0xc9, 0x10 } };
static IWICImagingFactory *s_wic;
static img s_panel, s_placa, s_iconos, s_minis, s_lupa, s_lupa_luz;

static int leer_recurso(const char *nombre, img *m)
{
    wchar_t p[MAX_PATH], w[64], *barra;
    IWICBitmapDecoder *dec = NULL;
    IWICBitmapFrameDecode *fr = NULL;
    IWICBitmapSource *cv = NULL;
    UINT W = 0, H = 0;
    int ok = 0;
    DWORD n = GetModuleFileNameW(NULL, p, MAX_PATH);
    if (!n || n >= MAX_PATH || !(barra = wcsrchr(p, L'\\')))
        return 0;
    *barra = 0;
    MultiByteToWideChar(CP_UTF8, 0, nombre, -1, w, 64);
    if (wcslen(p) + wcslen(w) + 40 >= MAX_PATH)
        return 0;
    wcscat(p, ygo_mods_w());
    wcscat(p, L"\\buscador\\");
    wcscat(p, w);
    if (!s_wic && FAILED(CoCreateInstance(&k_clsid_wic, NULL, CLSCTX_INPROC_SERVER, &k_iid_wic, (void **)&s_wic)))
        s_wic = NULL;
    if (!s_wic
            || FAILED(IWICImagingFactory_CreateDecoderFromFilename(s_wic, p, NULL, GENERIC_READ,
                                                                   WICDecodeMetadataCacheOnDemand, &dec))
            || FAILED(IWICBitmapDecoder_GetFrame(dec, 0, &fr))
            || FAILED(WICConvertBitmapSource(&k_pbgra, (IWICBitmapSource *)fr, &cv))
            || FAILED(IWICBitmapSource_GetSize(cv, &W, &H)) || !W || !H)
        goto fin;
    m->p = (unsigned char *)malloc((size_t)W * H * 4);
    if (m->p && SUCCEEDED(IWICBitmapSource_CopyPixels(cv, NULL, W * 4, W * H * 4, m->p))) {
        m->w = (int)W;
        m->h = (int)H;
        ok = 1;
    } else {
        free(m->p);
        m->p = NULL;
    }
fin:
    if (cv)
        IWICBitmapSource_Release(cv);
    if (fr)
        IWICBitmapFrameDecode_Release(fr);
    if (dec)
        IWICBitmapDecoder_Release(dec);
    if (!ok)
        fprintf(stderr, "[BUSCADOR] no se pudo leer %ls\n", p);
    return ok;
}

/* ---- drawing on a premultiplied BGRA canvas (window px) ------------------ */
typedef struct { unsigned char *p; int w, h; } lienzo;

static void sobre(unsigned char *d, unsigned b, unsigned g, unsigned r, unsigned a)
{
    unsigned k = 255 - a;
    d[0] = (unsigned char)(b + d[0] * k / 255);
    d[1] = (unsigned char)(g + d[1] * k / 255);
    d[2] = (unsigned char)(r + d[2] * k / 255);
    d[3] = (unsigned char)(a + d[3] * k / 255);
}

/* src part into the box, bilinear (area average when much smaller); luz:
 * its colour times luz/255 (the unlit chips), alfa its opacity */
static void pegar(lienzo *L, const img *m, int sx, int sy, int sw, int sh, float dx, float dy, float dw, float dh,
                  unsigned alfa, unsigned luz)
{
    int x0 = (int)floorf(dx), y0 = (int)floorf(dy), x1 = (int)ceilf(dx + dw), y1 = (int)ceilf(dy + dh), x, y;
    float kx, ky;
    int caja_;
    if (!m->p || dw < 0.5f || dh < 0.5f || sw < 1 || sh < 1)
        return;
    kx = (float)sw / dw;
    ky = (float)sh / dh;
    caja_ = kx > 2.f || ky > 2.f;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > L->w) x1 = L->w;
    if (y1 > L->h) y1 = L->h;
    for (y = y0; y < y1; y++) {
        float v = ((float)y + 0.5f - dy) * ky - 0.5f;
        for (x = x0; x < x1; x++) {
            float u = ((float)x + 0.5f - dx) * kx - 0.5f, c[4] = { 0, 0, 0, 0 }, cob;
            int k;
            if (u < -0.5f || v < -0.5f || u > (float)sw - 0.5f || v > (float)sh - 0.5f)
                continue;
            if (caja_) {
                int ua = (int)floorf(u - kx * 0.5f + 0.5f), ub = (int)ceilf(u + kx * 0.5f + 0.5f);
                int va = (int)floorf(v - ky * 0.5f + 0.5f), vb = (int)ceilf(v + ky * 0.5f + 0.5f), n = 0, i, j;
                if (ua < 0) ua = 0;
                if (va < 0) va = 0;
                if (ub > sw) ub = sw;
                if (vb > sh) vb = sh;
                for (j = va; j < vb; j++)
                    for (i = ua; i < ub; i++) {
                        const unsigned char *q = m->p + 4 * ((size_t)(sy + j) * m->w + sx + i);
                        c[0] += q[0]; c[1] += q[1]; c[2] += q[2]; c[3] += q[3];
                        n++;
                    }
                if (!n)
                    continue;
                for (k = 0; k < 4; k++)
                    c[k] /= (float)n;
            } else {
                int ui = (int)floorf(u), vi = (int)floorf(v);
                float fu = u - (float)ui, fv = v - (float)vi;
                for (k = 0; k < 4; k++) {
                    int xx = ui + (k & 1), yy = vi + (k >> 1);
                    float wgt = ((k & 1) ? fu : 1.f - fu) * ((k >> 1) ? fv : 1.f - fv);
                    const unsigned char *q;
                    if (xx < 0) xx = 0;
                    if (yy < 0) yy = 0;
                    if (xx >= sw) xx = sw - 1;
                    if (yy >= sh) yy = sh - 1;
                    q = m->p + 4 * ((size_t)(sy + yy) * m->w + sx + xx);
                    c[0] += wgt * q[0]; c[1] += wgt * q[1]; c[2] += wgt * q[2]; c[3] += wgt * q[3];
                }
            }
            /* partial coverage at the box's edges (smooth when not on whole px) */
            cob = 1.f;
            if ((float)x < dx) cob *= 1.f - (dx - (float)x);
            if ((float)x + 1.f > dx + dw) cob *= dx + dw - (float)x;
            if ((float)y < dy) cob *= 1.f - (dy - (float)y);
            if ((float)y + 1.f > dy + dh) cob *= dy + dh - (float)y;
            if (cob <= 0.f)
                continue;
            for (k = 0; k < 3; k++)
                c[k] = c[k] * (float)luz / 255.f;
            for (k = 0; k < 4; k++)
                c[k] = c[k] * (float)alfa / 255.f * cob;
            if (c[3] >= 0.5f)
                sobre(L->p + 4 * ((size_t)y * L->w + x), (unsigned)(c[0] + 0.5f), (unsigned)(c[1] + 0.5f),
                      (unsigned)(c[2] + 0.5f), (unsigned)(c[3] + 0.5f));
        }
    }
}

/* a picture in 9 parts: its corners (cs source px, shown cd px) kept */
static void pegar9(lienzo *L, const img *m, int cs, float cd, float x, float y, float w, float h, unsigned alfa,
                   unsigned luz)
{
    int sw = m->w, sh = m->h, csy = cs > sh / 2 ? sh / 2 : cs, i, j;
    float cdy = cd > h / 2 ? h / 2 : cd, cdx = cd > w / 2 ? w / 2 : cd;
    int sxs[4] = { 0, cs, sw - cs, sw }, sys[4] = { 0, csy, sh - csy, sh };
    float dxs[4] = { x, x + cdx, x + w - cdx, x + w }, dys[4] = { y, y + cdy, y + h - cdy, y + h };
    if (!m->p)
        return;
    for (i = 0; i < 4; i++) {
        dxs[i] = floorf(dxs[i] + 0.5f);
        dys[i] = floorf(dys[i] + 0.5f);
    }
    for (j = 0; j < 3; j++)
        for (i = 0; i < 3; i++)
            pegar(L, m, sxs[i], sys[j], sxs[i + 1] - sxs[i], sys[j + 1] - sys[j], dxs[i], dys[j],
                  dxs[i + 1] - dxs[i], dys[j + 1] - dys[j], alfa, luz);
}

static void rect(lienzo *L, float x, float y, float w, float h, unsigned r, unsigned g, unsigned b, unsigned a)
{
    int X0 = (int)floorf(x), Y0 = (int)floorf(y), X1 = (int)ceilf(x + w), Y1 = (int)ceilf(y + h), i, j;
    for (j = Y0; j < Y1; j++)
        for (i = X0; i < X1; i++)
            if (i >= 0 && j >= 0 && i < L->w && j < L->h)
                sobre(L->p + 4 * ((size_t)j * L->w + i), b * a / 255, g * a / 255, r * a / 255, a);
}

static void marco(lienzo *L, float x, float y, float w, float h, float g, unsigned r, unsigned gg, unsigned b, unsigned a)
{
    rect(L, x, y, w, g, r, gg, b, a);
    rect(L, x, y + h - g, w, g, r, gg, b, a);
    rect(L, x, y + g, g, h - 2 * g, r, gg, b, a);
    rect(L, x + w - g, y + g, g, h - 2 * g, r, gg, b, a);
}

/* a ring (cx, cy, radius, thickness) */
static void aro(lienzo *L, float cx, float cy, float R, float G, unsigned r, unsigned g, unsigned b, unsigned a)
{
    int x, y;
    for (y = (int)(cy - R - 2); y <= (int)(cy + R + 2); y++)
        for (x = (int)(cx - R - 2); x <= (int)(cx + R + 2); x++) {
            float d = sqrtf(((float)x + 0.5f - cx) * ((float)x + 0.5f - cx) + ((float)y + 0.5f - cy) * ((float)y + 0.5f - cy));
            float k = 1.f - fabsf(d - (R - G * 0.5f)) / (G * 0.5f + 0.5f);
            unsigned A;
            if (x < 0 || y < 0 || x >= L->w || y >= L->h || k <= 0.f)
                continue;
            if (k > 1.f)
                k = 1.f;
            A = (unsigned)(k * (float)a);
            sobre(L->p + 4 * ((size_t)y * L->w + x), b * A / 255, g * A / 255, r * A / 255, A);
        }
}

/* a line of the game's letters: x, y its box's top left (window px), h the
 * size, maxw (0: any) cut with "...", al 0 left 1 centre 2 right of x,
 * borde: a dark edge */
static void letras(lienzo *L, float x, float y, const wchar_t *s, int h, COLORREF col, int al, float maxw, int borde)
{
    int tw = fj_ancho(s, h), lh = fj_linea(h), b = borde ? (h / 9 + 1) : 0, W, H, x0, y0, i, j;
    unsigned char *t;
    if (maxw > 0.f && (float)tw > maxw)
        tw = (int)maxw;
    W = tw + 2 * b + 4;
    H = lh + 2 * b + 4;
    x0 = (int)(al == 1 ? x - (float)tw * 0.5f : al == 2 ? x - (float)tw : x) - b - 2;
    y0 = (int)y - b - 2;
    t = (unsigned char *)calloc((size_t)W * H, 4);
    if (!t)
        return;
    {
        RECT rc = { b + 2, b + 2, b + 2 + tw, b + 2 + lh };
        fj_dibujar(t, W, H, &rc, s, h, FJ_G, FJ_ELIPSIS);
    }
    for (j = 0; j < H; j++)
        for (i = 0; i < W; i++) {
            int xx = x0 + i, yy = y0 + j;
            unsigned c = t[4 * (j * W + i) + 1], o = 0;
            if (xx < 0 || yy < 0 || xx >= L->w || yy >= L->h)
                continue;
            if (b) {
                int di, dj;
                for (dj = -b; dj <= b; dj++)
                    for (di = -b; di <= b; di++) {
                        int u = i + di, v = j + dj;
                        if (u >= 0 && v >= 0 && u < W && v < H && t[4 * (v * W + u) + 1] > o)
                            o = t[4 * (v * W + u) + 1];
                    }
                if (o)
                    sobre(L->p + 4 * ((size_t)yy * L->w + xx), 0, 8 * o / 255, 24 * o / 255, o);
            }
            if (c)
                sobre(L->p + 4 * ((size_t)yy * L->w + xx), GetBValue(col) * c / 255, GetGValue(col) * c / 255,
                      GetRValue(col) * c / 255, c);
        }
    free(t);
}

/* the title letters (the panel's name) */
static void titulo(lienzo *L, float cx, float y, const wchar_t *s, int h, COLORREF col)
{
    int tw = ft_ancho(s, h), lh = ft_linea(h), b = h / 10 + 1, W = tw + 2 * b + 4, H = lh + 2 * b + 4, x0, y0, i, j;
    unsigned char *t = (unsigned char *)calloc((size_t)W * H, 4);
    if (!t)
        return;
    {
        RECT rc = { b + 2, b + 2, b + 2 + tw, b + 2 + lh };
        ft_dibujar(t, W, H, &rc, s, h, FJ_G);
    }
    x0 = (int)(cx - (float)W * 0.5f);
    y0 = (int)y - b - 2;
    for (j = 0; j < H; j++)
        for (i = 0; i < W; i++) {
            int xx = x0 + i, yy = y0 + j, di, dj;
            unsigned c = t[4 * (j * W + i) + 1], o = 0;
            if (xx < 0 || yy < 0 || xx >= L->w || yy >= L->h)
                continue;
            for (dj = -b; dj <= b; dj++)
                for (di = -b; di <= b; di++) {
                    int u = i + di, v = j + dj;
                    if (u >= 0 && v >= 0 && u < W && v < H && t[4 * (v * W + u) + 1] > o)
                        o = t[4 * (v * W + u) + 1];
                }
            if (o)
                sobre(L->p + 4 * ((size_t)yy * L->w + xx), 0, 8 * o / 255, 24 * o / 255, o);
            if (c)
                sobre(L->p + 4 * ((size_t)yy * L->w + xx), GetBValue(col) * c / 255, GetGValue(col) * c / 255,
                      GetRValue(col) * c / 255, c);
        }
    free(t);
}

/* ---- the windows ---------------------------------------------------------- */
typedef struct {
    HWND win;
    HDC mem;
    HBITMAP bmp, old;
    lienzo l;
    int shown;
} ventana;

static int lienzo_de(ventana *v, HDC screen, int w, int h)
{
    BITMAPINFO bi;
    if (v->bmp && v->l.w == w && v->l.h == h)
        return 1;
    if (v->bmp) {
        SelectObject(v->mem, v->old);
        DeleteObject(v->bmp);
        v->bmp = NULL;
    }
    memset(&bi, 0, sizeof bi);
    bi.bmiHeader.biSize = sizeof bi.bmiHeader;
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -h;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    v->bmp = CreateDIBSection(screen, &bi, DIB_RGB_COLORS, (void **)&v->l.p, NULL, 0);
    if (!v->bmp) {
        v->l.w = v->l.h = 0;
        return 0;
    }
    v->old = (HBITMAP)SelectObject(v->mem, v->bmp);
    v->l.w = w;
    v->l.h = h;
    return 1;
}

static void mostrar(ventana *v, HDC screen, POINT pos)
{
    POINT src = { 0, 0 };
    SIZE sz;
    BLENDFUNCTION bf = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
    if (!v->win)
        v->win = CreateWindowExA(WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW
                                 | WS_EX_TOPMOST, "YgoBuscador", "", WS_POPUP, 0, 0, 1, 1, NULL, NULL,
                                 GetModuleHandleA(NULL), NULL);
    if (!v->win)
        return;
    sz.cx = v->l.w;
    sz.cy = v->l.h;
    UpdateLayeredWindow(v->win, screen, &pos, &sz, v->mem, &src, 0, &bf, ULW_ALPHA);
    if (!v->shown)
        SetWindowPos(v->win, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
    v->shown = 1;
}

static void ocultar(ventana *v)
{
    if (v->shown && v->win)
        ShowWindow(v->win, SW_HIDE);
    v->shown = 0;
}

/* ---- painting ------------------------------------------------------------- */
#define ORO RGB(250, 226, 150)
#define TINTA RGB(64, 34, 6)
#define APAGADO RGB(196, 170, 110)

/* the thread's copy of the state */
typedef struct {
    wchar_t q[33];
    unsigned atrib, tipos, niveles;
    int nuevas, limitadas, agregadas, atk, editando, cursor, sobre, fila, lista0, nres, activo, lang;
    int res[NFILAS];
} vista;

static void chip(lienzo *L, float u, caja c, const wchar_t *s, int on, int sobre_)
{
    float x = c.x * u, y = c.y * u, w = c.w * u, h = c.h * u;
    int th = (int)(11.f * u);
    pegar9(L, &s_placa, 30, h * 0.5f, x, y, w, h, 255, on ? 255 : sobre_ ? 150 : 95);
    /* smaller until it fits (the longer words: "Armadilha", "Trappola",
     * "Nouvelles"; a 4:3 picture leaves the chips narrower) */
    while (th > (int)(7.f * u) && (float)fj_ancho(s, th) > w - 8.f * u)
        th--;
    letras(L, x + w * 0.5f, y + (h - (float)fj_linea(th)) * 0.5f, s, th,
           on ? TINTA : sobre_ ? RGB(255, 236, 170) : APAGADO, 1, w - 4.f, !on);
}

static void pintar_panel(lienzo *L, const vista *v, float u, float wu)
{
    int lg = v->lang, i;
    wchar_t w[300];
    memset(L->p, 0, (size_t)L->w * L->h * 4);
    pegar9(L, &s_panel, 70, 26.f * u, 0, 0, (float)L->w, (float)L->h, 245, 255);
    titulo(L, wu * u * 0.5f, Y_TITULO * u - 1.f * u, k_buscar[lg], (int)(15.f * u), RGB(255, 220, 110));
    {
        const img *im = v->sobre == Z_LUPA && s_lupa_luz.p ? &s_lupa_luz : &s_lupa;
        float r = 10.f * u;
        pegar(L, im, 0, 0, im->w, im->h, (PAD + 9.f) * u - r, (Y_TITULO + 8.f) * u - r, 2.f * r, 2.f * r, 255, 255);
    }
    letras(L, (wu - PAD - 2.f) * u, (Y_TITULO + 2.f) * u, L"F3", (int)(9.f * u), APAGADO, 2, 0.f, 1);

    /* the field */
    {
        caja c = z_campo(wu);
        float x = c.x * u, y = c.y * u, cw = c.w * u, ch = c.h * u;
        int h = (int)(12.f * u);
        pegar9(L, &s_placa, 30, ch * 0.5f, x, y, cw, ch, 255, v->editando ? 255 : v->sobre == Z_CAMPO ? 235 : 215);
        if (v->q[0] || v->editando) {
            swprintf(w, 64, L"%ls%ls", v->q, v->editando && v->cursor ? L"|" : L"");
            letras(L, x + 9.f * u, y + (ch - (float)fj_linea(h)) * 0.5f, w, h, TINTA, 0, cw - 18.f * u, 0);
        } else
            letras(L, x + 9.f * u, y + (ch - (float)fj_linea(h)) * 0.5f, k_campo[lg], h, RGB(120, 76, 20), 0,
                   cw - 18.f * u, 0);
    }
    /* the attributes */
    for (i = 0; i < 6; i++) {
        caja c = z_atrib(wu, i);
        int on = (v->atrib >> (i + 1)) & 1u, so = v->sobre == Z_ATRIB + i;
        float r = R_ATRIB * u * (on ? 1.f : 0.9f), cx = (c.x + R_ATRIB) * u, cy = (c.y + R_ATRIB) * u;
        pegar(L, &s_iconos, 64 * i, 0, 64, 64, cx - r * 64.f / 59.4f, cy - r * 64.f / 59.4f, 2.f * r * 64.f / 59.4f,
              2.f * r * 64.f / 59.4f, 255, on || so ? 255 : 120);
        if (on)
            aro(L, cx, cy, r + 2.5f * u, 2.f * u, 255, 214, 90, 255);
    }
    /* the kinds, the levels */
    for (i = 0; i < 6; i++)
        chip(L, u, z_tipo(wu, i), k_tipo[lg][i], (v->tipos >> i) & 1u, v->sobre == Z_TIPO + i);
    for (i = 0; i < 3; i++) {
        swprintf(w, 32, i == 0 ? L"%ls 1-4" : i == 1 ? L"%ls 5-6" : L"%ls 7+", k_nv[lg]);
        chip(L, u, z_nivel(wu, i), w, (v->niveles >> i) & 1u, v->sobre == Z_NIVEL + i);
    }
    /* the minimum ATK */
    {
        caja a = z_atk(wu);
        float x = a.x * u, y = a.y * u, aw = a.w * u, ah = a.h * u, f = (float)v->atk / 4000.f, kx;
        swprintf(w, 32, L"ATK %d+", v->atk);
        letras(L, PAD * u, (Y_ATK + 8.f) * u - (float)fj_linea((int)(11.f * u)) * 0.5f, w, (int)(11.f * u),
               v->atk ? ORO : APAGADO, 0, 60.f * u, 1);
        rect(L, x, y, aw, ah, 26, 14, 4, 255);
        rect(L, x + u, y + u, (aw - 2.f * u) * f, ah - 2.f * u, 238, 168, 40, 255);
        marco(L, x, y, aw, ah, u, 220, 160, 50, 255);
        kx = x + aw * f;
        {
            float r = 7.f * u;
            int yy, xx;
            for (yy = (int)(y + ah * 0.5f - r - 1); yy <= (int)(y + ah * 0.5f + r + 1); yy++)
                for (xx = (int)(kx - r - 1); xx <= (int)(kx + r + 1); xx++) {
                    float dx = (float)xx + 0.5f - kx, dy = (float)yy + 0.5f - (y + ah * 0.5f), d = sqrtf(dx * dx + dy * dy);
                    float k = r + 0.5f - d, t = (dy + r) / (2.f * r);
                    unsigned A;
                    if (xx < 0 || yy < 0 || xx >= L->w || yy >= L->h || k <= 0.f)
                        continue;
                    A = (unsigned)((k > 1.f ? 1.f : k) * 255.f);
                    sobre(L->p + 4 * ((size_t)yy * L->w + xx), (unsigned)(60 - 40 * t) * A / 255,
                          (unsigned)(226 - 90 * t) * A / 255, (unsigned)(255 - 20 * t) * A / 255, A);
                }
            aro(L, kx, y + ah * 0.5f, r, 1.5f * u, 120, 60, 10, 255);
        }
    }
    chip(L, u, z_extra(wu, 0), k_nuevas[lg], v->nuevas, v->sobre == Z_NUEVAS);
    chip(L, u, z_extra(wu, 1), k_limitadas[lg], v->limitadas, v->sobre == Z_LIMITADAS);
    if (ygo_cartas_hay_agregadas()) {
        chip(L, u, z_extra2(wu, 0), k_agregadas[lg], v->agregadas, v->sobre == Z_AGREGADAS);
        chip(L, u, z_extra2(wu, 1), k_borrar[lg], 0, v->sobre == Z_BORRAR);
    } else
        chip(L, u, z_extra2(wu, 0), k_borrar[lg], 0, v->sobre == Z_BORRAR);

    /* the results */
    if (v->activo) {
        if (v->nres == 1)
            wcscpy(w, k_uno[lg]);
        else if (v->nres)
            swprintf(w, 64, k_res[lg], v->nres);
        else
            wcscpy(w, k_nada[lg]);
        letras(L, PAD * u, Y_RES * u, w, (int)(12.f * u), ORO, 0, 0.f, 1);
    }
    rect(L, (PAD - 2.f) * u, (Y_LISTA - 2.f) * u, (wu - 2 * PAD + 4.f) * u, (H_FILA * NFILAS + 4.f) * u, 10, 5, 0, 120);
    marco(L, (PAD - 2.f) * u, (Y_LISTA - 2.f) * u, (wu - 2 * PAD + 4.f) * u, (H_FILA * NFILAS + 4.f) * u, u, 160, 112, 36, 255);
    for (i = 0; i < NFILAS; i++) {
        caja c = z_fila(wu, i);
        int idx = v->res[i];
        uint32_t k, d, tipo, id;
        float x = c.x * u, y = c.y * u, fw = c.w * u, fh = c.h * u;
        if (idx < 0)
            continue;
        k = MEM16(LISTA + 2u * (uint32_t)idx);
        if (!k || k >= NCARTAS)
            continue;
        if (v->fila == i)
            pegar9(L, &s_placa, 30, fh * 0.3f, x + u, y + u, fw - 2.f * u, fh - 2.f * u, 110, 255);
        id = MEM16(T_IMAGEN + 2u * k);
        /* the image IDs go 4007..5695 (minis.png: 1700 cells) */
        if (id >= 4000u && (int)id - 4000 < (s_minis.w / 35) * (s_minis.h / 51)) {
            int ci = (int)id - 4000;
            pegar(L, &s_minis, (ci % 40) * 35, (ci / 40) * 51, 35, 51, x + 4.f * u, y + 1.5f * u, 11.8f * u, 17.f * u,
                  255, 255);
        }
        texto_ancho(TXT_NOMBRE + k, w, 300);
        letras(L, x + 20.f * u, y + 1.f * u, w, (int)(10.5f * u), v->fila == i ? RGB(255, 244, 200) : ORO, 0,
               fw - 24.f * u, 1);
        d = MEM32(T_DATOS + 4u * k);
        tipo = (d >> 20) & 0x1Fu;
        if (tipo == 22u || tipo == 21u)
            wcscpy(w, k_tipo[lg][tipo == 22u ? 4 : 5]);
        else
            swprintf(w, 80, L"%ls %u   ATK %u   DEF %u", k_nv[lg], (d >> 25) & 0xFu, ((d >> 9) & 0x1FFu) * 10u,
                     (d & 0x1FFu) * 10u);
        letras(L, x + 20.f * u, y + 10.5f * u, w, (int)(8.5f * u), APAGADO, 0, fw - 24.f * u, 0);
    }
    /* how far down the list (a gold sliver on the right) */
    if (v->nres > NFILAS) {
        float y0 = Y_LISTA * u, hh = H_FILA * NFILAS * u, x = (wu - PAD - 1.f) * u;
        float a = (float)v->lista0 / (float)v->nres, b = (float)(v->lista0 + NFILAS) / (float)v->nres;
        rect(L, x, y0 + hh * a, 2.f * u, hh * (b - a), 240, 190, 70, 255);
    }
}

/* the grid: what does not match darkened, what does with a gold edge */
static void pintar_rejilla(lienzo *L, float ox, float oy)
{
    uint32_t top = MEM16(0x0066E690u);
    int c, r;
    memset(L->p, 0, (size_t)L->w * L->h * 4);
    for (r = 0; r < 5; r++)
        for (c = 0; c < 10; c++) {
            uint32_t idx = (top + (uint32_t)r) * 10u + (uint32_t)c;
            /* 2026-11-19 (the user: "el marco amarillo esta un poco corrido"):
             * measured on the capture (3.0 x 2.25 px per guest px), the card
             * shown is 34 x 49.5 from 0.5 right of the cell's corner; the frame
             * was 1.5 wider on the right and 1.5 lower at the bottom */
            float gx = 94.f + 40.f * (float)c - 17.5f + 0.5f, gy = 120.f + 57.f * (float)r - 25.5f;
            float x = (gx - s_ax) * s_sx - ox, y = (gy - s_ay) * s_sy - oy, w = 34.f * s_sx, h = 49.5f * s_sy;
            if (idx >= NRES || !MEM16(LISTA + 2u * idx))
                continue;
            if (s_coincide[idx]) {
                marco(L, x - 1.f * s_sx, y - 1.f * s_sy, w + 2.f * s_sx, h + 2.f * s_sy, 1.4f * s_sy, 255, 214, 90, 255);
            } else
                /* 2026-11-19 (the user's capture: a yellow "L" right and
                 * below the darkened ones): the card's gold edge reaches the
                 * whole 35 x 51 cell, so the shade covers it all (and half a
                 * px round) */
                rect(L, x - 1.f * s_sx, y - 0.5f * s_sy, 36.f * s_sx, 52.f * s_sy, 8, 4, 0, 185);
        }
}

static DWORD WINAPI buscador_thread(LPVOID unused)
{
    WNDCLASSA wc;
    HDC screen;
    static ventana vp, vr, vb;
    LONG pintado_p = -1, pintado_r = -1;
    uint32_t top_de = 0xFFFFFFFFu;
    int boton_de = -1;
    ULONGLONG parpadeo = 0;
    float u_de = 0.f;
    (void)unused;
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    memset(&wc, 0, sizeof wc);
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(NULL);
    wc.lpszClassName = "YgoBuscador";
    RegisterClassA(&wc);
    screen = GetDC(NULL);
    vp.mem = CreateCompatibleDC(screen);
    vr.mem = CreateCompatibleDC(screen);
    vb.mem = CreateCompatibleDC(screen);
    leer_recurso("panel.png", &s_panel);
    leer_recurso("placa.png", &s_placa);
    leer_recurso("lupa.png", &s_lupa);
    leer_recurso("lupa_luz.png", &s_lupa_luz);
    leer_recurso("iconos.png", &s_iconos);
    leer_recurso("minis.png", &s_minis);
    fprintf(stderr, "[BUSCADOR] listo (panel %dx%d, miniaturas %dx%d)\n", s_panel.w, s_panel.h, s_minis.w, s_minis.h);
    fflush(stderr);
    for (;;) {
        MSG m;
        HWND g = FindWindowA("RecompNv2aD3D11", NULL);
        RECT cr;
        POINT tl = { 0, 0 };
        ULONGLONG t = GetTickCount64();
        int base = g && GetForegroundWindow() == g && !xbox_MenuIsOpen() && !IsIconic(g)
                   && GetClientRect(g, &cr) && cr.bottom > 100 && cr.right > 100;
        float ax = 0, ay = 0, bx = 0, by = 0, u, wu;
        int en_rejilla, abierto, activo, i;
        vista v;
        LONG ver;
        while (PeekMessageA(&m, NULL, 0, 0, PM_REMOVE))
            DispatchMessageA(&m);
        if (base && (nv2a_d3d11_window_to_guest(0, 0, cr.right, cr.bottom, &ax, &ay, NULL) < 0
                     || nv2a_d3d11_window_to_guest(cr.right, cr.bottom, cr.right, cr.bottom, &bx, &by, NULL) < 0
                     || bx - ax < 1.f || by - ay < 1.f))
            base = 0;
        if (base)
            ClientToScreen(g, &tl);

        EnterCriticalSection(&s_cs);
        if (base) {
            float sx = (float)cr.right / (bx - ax), sy = (float)cr.bottom / (by - ay);
            if (sx != s_sx || sy != s_sy || ax != s_ax || ay != s_ay || s_cw != cr.right || s_ch != cr.bottom) {
                s_ax = ax; s_ay = ay; s_sx = sx; s_sy = sy;
                s_cw = cr.right; s_ch = cr.bottom;
                cambio();
            }
        }
        en_rejilla = base && t - s_lib_at < 300u && MEM16(0x0066E684u) == 0u;
        abierto = s_abierto;
        activo = s_activo;
        if (s_editando && t >= parpadeo) {
            parpadeo = t + 500u;
            s_cursor_vis ^= 1;
            cambio();
        }
        memset(&v, 0, sizeof v);
        wcscpy(v.q, s_q);
        v.atrib = s_atrib; v.tipos = s_tipos; v.niveles = s_niveles;
        v.nuevas = s_nuevas; v.limitadas = s_limitadas; v.agregadas = s_agregadas; v.atk = s_atk; v.editando = s_editando; v.cursor = s_cursor_vis;
        v.sobre = s_sobre; v.fila = s_sobre_fila; v.lista0 = s_lista0; v.nres = s_nres; v.activo = s_activo;
        for (i = 0; i < NFILAS; i++)
            v.res[i] = s_lista0 + i < s_nres ? s_res[s_lista0 + i] : -1;
        v.lang = lang6();
        ver = s_version;
        u = s_sy;
        wu = panel_wu();
        LeaveCriticalSection(&s_cs);

        /* the button */
        if (en_rejilla && !abierto && s_lupa.p) {
            /* stretched as the game's own (its circle is round in game px) */
            int w = (int)(2.f * L_R * s_sx + 0.5f), h = (int)(2.f * L_R * s_sy + 0.5f);
            POINT pos = { tl.x + (LONG)((L_CX - L_R - s_ax) * s_sx), tl.y + (LONG)((L_CY - L_R - s_ay) * s_sy) };
            if (lienzo_de(&vb, screen, w, h)) {
                if (boton_de != s_boton_sobre || !vb.shown || u != u_de) {
                    const img *im = s_boton_sobre && s_lupa_luz.p ? &s_lupa_luz : &s_lupa;
                    boton_de = s_boton_sobre;
                    memset(vb.l.p, 0, (size_t)w * h * 4);
                    pegar(&vb.l, im, 0, 0, im->w, im->h, 0.f, 0.f, (float)w, (float)h, 255, 255);
                }
                mostrar(&vb, screen, pos);
            }
        } else {
            ocultar(&vb);
            boton_de = -1;
        }
        /* the panel */
        if (en_rejilla && abierto && s_panel.p) {
            int w = (int)(wu * u + 0.5f), h = (int)(P_H * u + 0.5f);
            POINT pos = { tl.x + (LONG)((P_X0 - s_ax) * s_sx), tl.y + (LONG)((P_Y0 - s_ay) * s_sy) };
            if (lienzo_de(&vp, screen, w, h)) {
                if (pintado_p != ver || !vp.shown || u != u_de)
                    pintar_panel(&vp.l, &v, u, wu);
                pintado_p = ver;
                mostrar(&vp, screen, pos);
            }
        } else {
            ocultar(&vp);
            pintado_p = -1;
        }
        /* the grid */
        if (en_rejilla && abierto && activo) {
            float x0 = (70.f - s_ax) * s_sx, y0 = (88.f - s_ay) * s_sy;
            int w = (int)(410.f * s_sx), h = (int)(292.f * s_sy);
            POINT pos = { tl.x + (LONG)x0, tl.y + (LONG)y0 };
            uint32_t top = MEM16(0x0066E690u);
            if (lienzo_de(&vr, screen, w, h)) {
                if (pintado_r != ver || top != top_de || !vr.shown || u != u_de) {
                    EnterCriticalSection(&s_cs);
                    pintar_rejilla(&vr.l, x0, y0);
                    LeaveCriticalSection(&s_cs);
                }
                pintado_r = ver;
                top_de = top;
                {
                    /* under cartel.c's windows: the "Nueva" badges stay over it */
                    int nuevo = !vr.shown;
                    mostrar(&vr, screen, pos);
                    if (nuevo || t % 500u < 20u) {
                        HWND c = NULL, ult = NULL;
                        while ((c = FindWindowExA(NULL, c, "YgoCartel", NULL)) != NULL)
                            ult = c;
                        if (ult)
                            SetWindowPos(vr.win, ult, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
                    }
                }
            }
        } else {
            ocultar(&vr);
            pintado_r = -1;
        }
        u_de = u;
        Sleep(en_rejilla ? 16 : 100);
    }
}

/* ---- the game's thread ------------------------------------------------------ */
static void teclear(void)
{
    static unsigned char antes[256];
    BYTE ks[256];
    int vk, cambiado = 0;
    memset(ks, 0, sizeof ks);
    if (GetAsyncKeyState(VK_SHIFT) & 0x8000)
        ks[VK_SHIFT] = 0x80;
    if (GetAsyncKeyState(VK_CONTROL) & 0x8000)
        ks[VK_CONTROL] = 0x80;
    if (GetAsyncKeyState(VK_MENU) & 0x8000)
        ks[VK_MENU] = 0x80;
    if (GetKeyState(VK_CAPITAL) & 1)
        ks[VK_CAPITAL] = 1;
    for (vk = 8; vk < 256; vk++) {
        int down = (GetAsyncKeyState(vk) & 0x8000) != 0;
        if (vk == VK_SHIFT || vk == VK_CONTROL || vk == VK_MENU || (vk >= VK_LSHIFT && vk <= VK_RMENU)
                || vk == VK_LBUTTON || vk == VK_RBUTTON || vk == VK_MBUTTON || vk == VK_F3) {
            antes[vk] = (unsigned char)down;
            continue;
        }
        if (down && !antes[vk]) {
            size_t n = wcslen(s_q);
            if (vk == VK_RETURN || vk == VK_ESCAPE || vk == VK_TAB)
                s_editando = 0;
            else if (vk == VK_BACK) {
                if (n) {
                    s_q[n - 1] = 0;
                    cambiado = 1;
                }
            } else {
                wchar_t c[4];
                int r = ToUnicodeEx((UINT)vk, MapVirtualKeyA((UINT)vk, 0), ks, c, 4, 4, GetKeyboardLayout(0));
                char b = 0;
                BOOL usado = FALSE;
                if (r == 1 && c[0] >= 32 && n < 32
                        && WideCharToMultiByte(1252, WC_NO_BEST_FIT_CHARS, c, 1, &b, 1, "?", &usado) == 1 && !usado) {
                    s_q[n] = c[0];
                    s_q[n + 1] = 0;
                    cambiado = 1;
                }
            }
            cambio();
        }
        antes[vk] = (unsigned char)down;
    }
    if (cambiado) {
        s_lista0 = 0;
        buscar();
    }
}

static void cerrar(void)
{
    s_abierto = 0;
    s_editando = 0;
    s_arrastre = 0;
    s_sobre = Z_NADA;
    cambio();
    fprintf(stderr, "[BUSCADOR] cerrado\n");
    fflush(stderr);
}

static void abrir(void)
{
    s_abierto = 1;
    s_editando = 1;                     /* ready to type */
    s_sobre = Z_NADA;
    buscar();
    cambio();
    fprintf(stderr, "[BUSCADOR] abierto\n");
    fflush(stderr);
}

/* raton.c's Biblioteca part, first, each frame of that screen: 0 nothing of
 * ours (the grid goes on), 1 the pointer is on our button or panel (no grid
 * hover, its clicks and wheel are ours), 2 everything is ours this frame
 * (typing, or a key of ours: the pad state is taken off) */
int ygo_buscador_raton(int over, float gx, float gy, int clic, LONG rueda)
{
    static int f3_antes, esc_antes, der_antes, soltar;
    static ULONGLONG busco_at;
    int f3, esc, der, r = 0, fila, q;
    float x, y;
    ULONGLONG t = GetTickCount64();
    if (!s_ok)
        return 0;
    EnterCriticalSection(&s_cs);
    if (t - s_lib_at > 2000u && s_abierto)
        s_abierto = s_editando = 0;     /* the Biblioteca was left: closed (the filters stay) */
    s_lib_at = t;
    if (MEM16(0x0066E684u) != 0u) {     /* a card's detail or a box: we hide */
        s_editando = 0;
        s_arrastre = 0;
        LeaveCriticalSection(&s_cs);
        return 0;
    }
    {
        HWND g = FindWindowA("RecompNv2aD3D11", NULL);
        int frente = g && GetForegroundWindow() == g;
        f3 = frente && (GetAsyncKeyState(VK_F3) & 0x8000) != 0;
        esc = frente && (GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0;
        der = frente && (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
    }
    if (f3 && !f3_antes) {
        if (s_abierto)
            cerrar();
        else
            abrir();
        r = 2;
    }
    f3_antes = f3;
    /* the collection or the new marks may change while it is open */
    if (s_abierto && t - busco_at > 1000u) {
        busco_at = t;
        if (s_nuevas || s_limitadas || s_activo)
            buscar();
    }
    if (!s_abierto) {
        /* the button */
        float dx = (gx - L_CX) / L_R, dy = (gy - L_CY) / L_R;
        int en = over && dx * dx + dy * dy <= 1.f;
        if (en != s_boton_sobre) {
            s_boton_sobre = en;
            cambio();
        }
        if (en) {
            r = r ? r : 1;
            if (clic)
                abrir();
        }
        esc_antes = esc;
        der_antes = der;
        LeaveCriticalSection(&s_cs);
        return r;
    }
    /* open */
    if (s_editando) {
        teclear();
        r = 2;
        if (!s_editando)
            soltar = 1;                 /* Enter / Esc: out of the field, not to the game */
    } else if (esc && !esc_antes) {
        cerrar();
        soltar = 1;
        r = 2;
    }
    if (soltar) {
        if (!esc && !der && !(GetAsyncKeyState(VK_RETURN) & 0x8000))
            soltar = 0;
        r = 2;
    }
    esc_antes = esc;
    a_panel(gx, gy, &x, &y);
    q = over && s_abierto ? que_hay(x, y, &fila) : Z_NADA;
    if (s_arrastre) {
        if (!(GetAsyncKeyState(VK_LBUTTON) & 0x8000))
            s_arrastre = 0;
        else {
            caja a = z_atk(panel_wu());
            float f = (x - a.x) / a.w;
            int atk = (int)(f < 0.f ? 0.f : f > 1.f ? 4000.f : floorf(f * 40.f + 0.5f) * 100.f);
            if (atk != s_atk) {
                s_atk = atk;
                s_lista0 = 0;
                buscar();
                cambio();
            }
        }
        q = Z_ATK;
    }
    if (q != s_sobre || (q == Z_LISTA ? fila : -1) != s_sobre_fila) {
        s_sobre = q;
        s_sobre_fila = q == Z_LISTA ? fila : -1;
        cambio();
    }
    if (q != Z_NADA && r < 1)
        r = 1;
    if (clic && q == Z_LUPA) {
        cerrar();
        LeaveCriticalSection(&s_cs);
        return 2;
    }
    /* 2026-11-17 (the user: "si presiono segundo click me quiere sacar del
     * menu... que sea como apretar F3"): the right button anywhere closes
     * the open panel, and that press is ours to the end (raton.c keeps its
     * B out); with the panel closed it is the game's (the exit box) */
    if (der && !der_antes) {
        cerrar();
        soltar = 1;
        r = 2;
    }
    der_antes = der;
    if (rueda && q == Z_LISTA) {
        int a = s_lista0;
        s_lista0 -= (int)rueda;
        if (s_lista0 > s_nres - NFILAS)
            s_lista0 = s_nres - NFILAS;
        if (s_lista0 < 0)
            s_lista0 = 0;
        if (a != s_lista0)
            cambio();
    }
    if (clic && s_abierto) {
        int cambia = 1;
        if (s_editando && q != Z_CAMPO)
            s_editando = 0;
        if (q == Z_CAMPO)
            s_editando = 1;
        else if (q >= Z_ATRIB && q < Z_ATRIB + 6)
            s_atrib ^= 1u << (q - Z_ATRIB + 1);
        else if (q >= Z_TIPO && q < Z_TIPO + 6)
            s_tipos ^= 1u << (q - Z_TIPO);
        else if (q >= Z_NIVEL && q < Z_NIVEL + 3)
            s_niveles ^= 1u << (q - Z_NIVEL);
        else if (q == Z_NUEVAS)
            s_nuevas ^= 1;
        else if (q == Z_LIMITADAS)
            s_limitadas ^= 1;
        else if (q == Z_AGREGADAS)
            s_agregadas ^= 1;
        else if (q == Z_BORRAR) {
            s_q[0] = 0;
            s_atrib = s_tipos = s_niveles = 0;
            s_nuevas = s_limitadas = s_agregadas = 0;
            s_atk = 0;
        } else if (q == Z_ATK) {
            s_arrastre = 1;
            cambia = 0;
        } else if (q == Z_LISTA) {
            int i = s_lista0 + fila;
            cambia = 0;
            if (fila >= 0 && i < s_nres) {
                fprintf(stderr, "[BUSCADOR] ir al lugar %d (carta k %u)\n", s_res[i],
                        MEM16(LISTA + 2u * (uint32_t)s_res[i]));
                fflush(stderr);
                ygo_biblioteca_ir(s_res[i]);
            }
        } else
            cambia = 0;
        if (cambia) {
            s_lista0 = 0;
            buscar();
        }
        cambio();
    }
    LeaveCriticalSection(&s_cs);
    return r;
}

void ygo_buscador_start(void)
{
    static int started;
    if (started)
        return;
    started = 1;
    InitializeCriticalSection(&s_cs);
    s_ok = 1;
    CloseHandle(CreateThread(NULL, 0, buscador_thread, NULL, 0, NULL));
}
