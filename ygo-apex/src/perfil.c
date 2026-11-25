/*
 * perfil.c - the main menu's Perfil (2026-11-16, the user: "un Perfil en el
 * menu principal ... que se sienta como que haces click dentro del juego y se
 * abre un menu"; mock-up 3 approved: one panel, the photo with the name under
 * it, the mascot one at a time with its 3D model moving, the sleeve one deck
 * at a time).
 *
 * Nothing of the game is replaced: like cartel.c's signs, everything is drawn
 * in layered click-through windows of our own over the game's window:
 * - the button: a medallion of the menu's wheel with a duelist's bust, in the
 *   middle of the wheel (raton.c tells where, ygo_perfil_rueda, each frame
 *   the wheel waits for keys); lit under the pointer;
 * - the panel: the whole window darkened, the panel (fondo.png: the menu's
 *   own banner, plates and round arrows, the walls' carvings), the photo,
 *   the texts in the game's letters (fuente.h), the sleeve's deck;
 * - the mascot: a small window over the panel's well, its idle animation
 *   (32 frames recorded from the game's own model by tools/visor_modelos/
 *   grabar.html, tools/perfil.py), about 11 a second.
 * The pictures: Mods-Extras\perfil (tools/perfil_recursos.py, tools/perfil.py).
 * The layout is in the pixels of the user's capture of the menu (1920 x 1080,
 * the game 1008 rows high from row 23), scaled to the window.
 *
 * The mouse: raton.c's wheel part calls ygo_perfil_raton first; while the
 * panel is open every pad button and key the game would see is taken off
 * (the pad state zeroed), a left click works the panel, a right click (or
 * Esc) closes it. The choices: the mascot and the sleeve are F2's own
 * ("Tu mascota", "Funda", "Marco de la funda": the same setters, saved with
 * xbox_MenuSaveSoon); the name and the photo in YGO_PERFIL\perfil.ini.
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
#include "estadisticas.h"

#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "ole32.lib")

int xbox_MenuIsOpen(void);
void xbox_MenuSaveSoon(void);
int nv2a_d3d11_window_to_guest(int px, int py, int cw, int ch, float *gx, float *gy, float *dev);
int ygo_mascota_tuya_get(void);
void ygo_mascota_tuya_set(int i);
int ygo_funda_get(void);
void ygo_funda_set(int i);
int ygo_funda_marco_get(void);
void ygo_funda_marco_set(int i);
const char *ygo_perfil_mascota_nombre(int i);   /* main.c: F2's names, translated */
const char *ygo_perfil_funda_nombre(int i);
const char *f2_traducir(const char *es);          /* f2_idiomas.c */
extern const char *g_ygo_game_dir;

#define NMASCOTAS 100
#define NFUNDAS ygo_arte_n()           /* 2026-11-19: was 32; fundas.c's list */
#define NTAPETES ygo_arte_n()          /* fundas.c k_arte, 0 Clasico */
const char *ygo_arte_archivo(int i);
int ygo_arte_n(void);
#define NMARCOS 5
#define NFOTOS 28
#define LADO 320
/* 2026-11-16 (the user: "hacela como se ve en el juego"): each mascot at
 * its model's own speed, 60 frames a second (tools/perfil.py writes the ms
 * of each frame beside its strip, mascotas\II.txt) */
#define FRAME_MS 16.667f
/* the player's name is the game's own (2026-11-16, the user: "busca donde
 * guarda la partida el jugador"): sub_0002D990, the new game, copies the
 * name screen's (+0x158) with strncpy(0x6518A9, name, 13); empty, "NO NAME"
 * (0x4238E8). It is in the saved block (0x6518A8.., savedata.dat from byte
 * 0x1C: "Dennis" at 0x1D), Windows-1252 (the name screen's letters, table
 * 0x456460), 12 at most. Shown from there, and written there when changed
 * here (the game keeps it with its next save). */
#define NOMBRE_MAX 12
#define NOMBRE_JUEGO 0x006518A9u

/* the collection (premios.c): your copies of card k, k = 1 .. NCARTAS-1 */
#define NCARTAS 1139u
#define COLECCION (0x00651E70u + 0xCC1u)
#define T_IMAGEN 0x005FE692u            /* the card list: k -> ID (0: no card) */

/* ---- the layout, in the capture's pixels (tools/perfil_recursos.py) ---- */
#define REF_H 1008.f
#define REF_W 1500.f                    /* the panel's width with a margin */
#define REF_CX 960.f
#define REF_CY 527.f                    /* row 23 + 1008 / 2 */
#define FONDO_X 238.f
#define FONDO_Y 36.f
#define FOTO_X 305.f
#define FOTO_Y 420.f                    /* 2026-11-19: as high as the mascot's box (was 360) */
#define FOTO_W 330.f
#define FOTO_H 240.f
#define NOMBRE_CX 960.f                 /* 2026-11-19: the name on the panel's top edge */
#define NOMBRE_Y 220.f
#define NOMBRE_W 330.f
#define NOMBRE_H 60.f
#define MASC_X 800.f
#define MASC_Y 380.f
#define MASC_L 320.f
#define MCX 960.f                       /* the mascot's column */
#define FCX 1450.f                      /* the sleeve's column */
#define PLACA_Y 750.f
#define CARTA_W 200.f
#define CARTA_H 292.f
#define CARTA_X 1340.f
#define CARTA_Y 400.f
#define PUNTO_X 1330.f
#define PUNTO_Y 721.f                   /* 2026-11-19: between the sleeve and its name (was 840) */
#define PUNTO_R 12.f                    /* 2026-11-19: a little smaller (was 16) */
/* 2026-11-16: 4, 5 the tapete's, on the sides of its box (smaller: TAP_K) */
/* 2026-11-19 (the user: "los botones son muy grandes y no tienen animación",
 * option D): small medallions (flecha_d_*.png, strips of FD_CUADROS frames),
 * each in a window of its own, by its plate: mascot, sleeve, tapete */
static const float k_flecha[6][2] = { { 766.f, 778.f }, { 1154.f, 778.f }, { 1296.f, 778.f }, { 1604.f, 778.f },
                                      { 316.f, 778.f }, { 624.f, 778.f } };
#define TCX 470.f                       /* the tapete's column */
#define FD_LADO 128                     /* each frame of the strips, px */
#define FD_CUADROS 12
#define FD_VER 64.f                     /* shown, ref px */
#define FD_VENT 100.f                   /* their windows, ref px (room to move and grow) */
#define TAP_K 0.7f
#define FLECHA_W 105.f
#define FLECHA_H 86.f

/* what the pointer is on */
enum { NADA, FOTO, NOMBRE, MASC_ANT, MASC_SIG, FUNDA_ANT, FUNDA_SIG, TAP_ANT, TAP_SIG, PAG_ANT, PAG_SIG, PUNTO0 };
/* the statistics page: the rivals' boxes, and the rival's deck panel */
/* 2026-11-19 (the user): the mascot's box and the sleeves glow under the pointer, as the tapete's */
enum { MASC_FOTO = 30, FUNDA_FOTO = 31, TAPFX0 = 33 };
/* 2026-11-20 (the user: "quiero las 3 y que la gente elija como con los marcos de
 * las cartas. pone abajo de tapete"): the tapete's light (tapete_fx.c), four
 * short pills under its box; its box shows the chosen one live (a window of
 * its own over the picture). The deck shines while the pointer is on it. */
#define FX_Y 676.f
#define FX_H 30.f
#define FX_W 70.f
#define FX_GAP 8.f
int ygo_tapete_fx_get(void);
void ygo_tapete_fx_set(int i);
void ygo_tapete_fx_pintar(const unsigned char *base, int w, int h, unsigned char *out, float t);
/* 2026-11-19 (the user: "al hacerle click a la mascota que brilla salga la
 * animacion"; the "Ver Invocación" button of an hour before taken out): a
 * click on the mascot plays its ace card (carta_as.c ygo_as_ver); meanwhile
 * the Perfil's windows are all hidden as if it were closed (the animation is
 * the kit's, in the game's frame below) and come back by themselves */
int ygo_as_ver(int masc);
int ygo_as_activa(void);
int ygo_as_ver_con(int masc, unsigned char *px, int w, int h);
int ygo_as_tapa(void);
void ygo_as_perfil_vuelto(void);
/* 2026-11-19 (the user: "se ve de fondo el menu principal quiero que se vea
 * perfil"): the click asks the Perfil's thread, which owns the windows, for a
 * picture of the panel as it is (with the model and the arrows); the animation
 * plays over it */
static volatile LONG s_pedir_as = -1;
enum { RIVAL0 = 40, RIVAL_DIF = 46, MAZO_X = 47, MAZO_PANEL = 48, MAZO_BOTON = 49, CONF_SI = 50, CONF_NO = 51,
       MAZO_CARTA = 100, RETRATO0 = 300 };
/* 2026-11-19 (the user: "Decks de los Oponentes" between Perfil and the
 * statistics): pages 0 Perfil, 1 the opponents, 2 the statistics */
#define PAG_RIVALES 1
#define PAG_ESTAD 2
#define NPAGINAS 3
/* 2026-11-19 (the user: "el deck de Duel Computer no se ve ... hay 3 Duel
 * Computer"): character 27 is duelist 27, with no lists; the Duel Computer
 * one meets is character 31, 32 or 33 (duelists 29, 30, 31, 0x423D60), and
 * 28..30 are 27 again. The page: 0..26 and the three Duel Computer */
static const int k_rivales[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22,
                                 23, 24, 25, 26, 31, 32, 33 };
#define NRIVALES ((int)(sizeof k_rivales / sizeof k_rivales[0]))
#define RG_COLS 8
#define RG_W 150.f
#define RG_H 100.f
#define RG_GX 166.f
#define RG_GY 160.f
#define RG_X0 (REF_CX - (RG_GX * (RG_COLS - 1) + RG_W) * 0.5f)
#define RG_Y0 345.f

/* 2026-11-16 (the user: "Estadísticas en el Perfil", mock-up approved): a
 * second page; the banner's two arrows (fondo.png's own, measured at
 * 466/980, 80) turn it */
static const float k_pag[2][2] = { { 704.f, 116.f }, { 1218.f, 116.f } };
/* 2026-11-16 (the user: "se ven confusos esos botones para cambiar"):
 * fondo.png's triangles and medallion icons pointed inwards; they are
 * turned to point out (flechas_afuera: left previous, right next) and the
 * medallions are clicked as well (ref = fondo + 238, 36) */
static const float k_pag_med[2][2] = { { 614.f, 136.f }, { 1304.f, 136.f } };
#define PAG_R 40.f

/* ---- shared state (the game's thread writes, ours draws) ---------------- */
static CRITICAL_SECTION s_cs;
static volatile LONG s_ok;
static ULONGLONG s_rueda_at;            /* the wheel waited for keys then */
static float s_rueda_x, s_rueda_y;      /* its middle, game frame px */
/* 2026-11-17 (the user: "que quede atras de la puerta" as the game's doors
 * close going into a menu): the doors' gap, half of it over half the window
 * (1 open, 0 shut), raton.c each frame; the button is cut to it */
static float s_rendija = 1.f;
static int s_abierto, s_sobre_boton, s_sobre;
static int s_pagina, s_p_pagina;        /* 0 the Perfil, 1 the statistics */
static ULONGLONG s_pag_clic[2];          /* when each page arrow was clicked */
static int s_editando, s_cursor_vis;
static wchar_t s_nombre[NOMBRE_MAX + 1] = L"Duelista", s_edit[NOMBRE_MAX + 1];
static int s_foto;
static LONG s_version = 1;              /* bumped on every change the panel shows */
/* the mapping game frame -> window px (ours: the thread reads it) */
static float s_ax, s_ay, s_sx = 1.f, s_sy = 1.f;
static int s_cw, s_ch;

static void cambio(void) { InterlockedIncrement(&s_version); }

static void nombre_del_juego(wchar_t *w)
{
    char a[NOMBRE_MAX + 1];
    int i;
    for (i = 0; i < NOMBRE_MAX; i++)
        if (!(a[i] = (char)MEM8(NOMBRE_JUEGO + (uint32_t)i)))
            break;
    a[i] = 0;
    if (!MultiByteToWideChar(1252, 0, a, -1, w, NOMBRE_MAX + 1))
        w[0] = 0;
}

static void nombre_al_juego(const wchar_t *w)
{
    char a[NOMBRE_MAX + 2];
    BOOL usado = FALSE;
    int i, n;
    memset(a, 0, sizeof a);
    if (WideCharToMultiByte(1252, WC_NO_BEST_FIT_CHARS, w, -1, a, NOMBRE_MAX + 1, "?", &usado) <= 0)
        return;
    a[NOMBRE_MAX] = 0;
    n = (int)strlen(a);
    for (i = 0; i <= NOMBRE_MAX; i++)
        MEM8(NOMBRE_JUEGO + (uint32_t)i) = (uint8_t)(i < n ? a[i] : 0);
    fprintf(stderr, "[PERFIL] nombre del juego: %s\n", a);
    fflush(stderr);
}

/* a letter the game can show (Windows-1252, as its name screen) */
static int letra_del_juego(wchar_t c)
{
    char b = 0;
    BOOL usado = FALSE;
    return c >= 32 && WideCharToMultiByte(1252, WC_NO_BEST_FIT_CHARS, &c, 1, &b, 1, "?", &usado) == 1 && !usado;
}

/* ---- perfil.ini --------------------------------------------------------- */
void ygo_perfil_wruta(const wchar_t *archivo, wchar_t *out, size_t cap);   /* perfil_dir.c */
unsigned ygo_ranura(void);

/* YGO_PERFIL\Ranura N\perfil.ini (2026-11-25: per profile and game slot) */
static void ini_ruta(wchar_t *p)
{
    ygo_perfil_wruta(L"perfil.ini", p, MAX_PATH);
}

static void guardar(void)
{
    wchar_t p[MAX_PATH];
    char u8[64];
    FILE *f;
    ini_ruta(p);
    if (!(f = _wfopen(p, L"wb")))
        return;
    WideCharToMultiByte(CP_UTF8, 0, s_nombre, -1, u8, sizeof u8, NULL, NULL);
    fprintf(f, "nombre=%s\r\nfoto=%d\r\n", u8, s_foto);
    fclose(f);
}

static void cargar(void)
{
    wchar_t p[MAX_PATH];
    char l[128];
    FILE *f;
    ini_ruta(p);
    if (!(f = _wfopen(p, L"rb")))
        return;
    while (fgets(l, sizeof l, f)) {
        char *e = l + strlen(l);
        while (e > l && (e[-1] == '\n' || e[-1] == '\r'))
            *--e = 0;
        if (!strncmp(l, "nombre=", 7) && l[7]) {
            wchar_t w[64];
            if (MultiByteToWideChar(CP_UTF8, 0, l + 7, -1, w, 64)) {
                w[NOMBRE_MAX] = 0;
                wcscpy(s_nombre, w);
            }
        } else if (!strncmp(l, "foto=", 5)) {
            int v = atoi(l + 5);
            s_foto = v >= 0 && v < NFOTOS ? v : 0;
        }
    }
    fclose(f);
}

/* ---- pictures ------------------------------------------------------------- */
typedef struct { int w, h; unsigned char *p; } img;   /* premultiplied BGRA */

static const GUID k_clsid_wic = { 0xcacaf262, 0x9370, 0x4615, { 0xa1, 0x3b, 0x9f, 0x55, 0x39, 0xda, 0x4c, 0x0a } };
static const GUID k_iid_wic = { 0xec5ec8a9, 0xc395, 0x4314, { 0x9c, 0x77, 0x54, 0xd7, 0xa9, 0x35, 0xff, 0x70 } };
static const GUID k_pbgra = { 0x6fddc324, 0x4e03, 0x4bfe, { 0xb1, 0x85, 0x3d, 0x77, 0x76, 0x8d, 0xc9, 0x10 } };
static IWICImagingFactory *s_wic;

static void liberar(img *m)
{
    free(m->p);
    m->p = NULL;
    m->w = m->h = 0;
}

static int leer(const wchar_t *ruta, img *m)
{
    IWICBitmapDecoder *dec = NULL;
    IWICBitmapFrameDecode *fr = NULL;
    IWICBitmapSource *cv = NULL;
    UINT w = 0, h = 0;
    int ok = 0;
    liberar(m);
    if (!s_wic) {
        CoInitializeEx(NULL, COINIT_MULTITHREADED);
        if (FAILED(CoCreateInstance(&k_clsid_wic, NULL, CLSCTX_INPROC_SERVER, &k_iid_wic, (void **)&s_wic)))
            s_wic = NULL;
    }
    if (!s_wic
            || FAILED(IWICImagingFactory_CreateDecoderFromFilename(s_wic, ruta, NULL, GENERIC_READ,
                                                                   WICDecodeMetadataCacheOnDemand, &dec))
            || FAILED(IWICBitmapDecoder_GetFrame(dec, 0, &fr))
            || FAILED(WICConvertBitmapSource(&k_pbgra, (IWICBitmapSource *)fr, &cv))
            || FAILED(IWICBitmapSource_GetSize(cv, &w, &h)) || !w || !h)
        goto fin;
    m->p = (unsigned char *)malloc((size_t)w * h * 4);
    if (m->p && SUCCEEDED(IWICBitmapSource_CopyPixels(cv, NULL, w * 4, w * h * 4, m->p))) {
        m->w = (int)w;
        m->h = (int)h;
        ok = 1;
    } else
        liberar(m);
fin:
    if (cv)
        IWICBitmapSource_Release(cv);
    if (fr)
        IWICBitmapFrameDecode_Release(fr);
    if (dec)
        IWICBitmapDecoder_Release(dec);
    if (!ok)
        fprintf(stderr, "[PERFIL] no se pudo leer %ls\n", ruta);
    return ok;
}

/* Mods-Extras\perfil\<nombre>, beside the exe */
static int leer_recurso(const char *nombre, img *m)
{
    wchar_t p[MAX_PATH], *barra;
    wchar_t w[96];
    DWORD n = GetModuleFileNameW(NULL, p, MAX_PATH);
    if (!n || n >= MAX_PATH || !(barra = wcsrchr(p, L'\\')))
        return 0;
    *barra = 0;
    MultiByteToWideChar(CP_UTF8, 0, nombre, -1, w, 96);
    if (wcslen(p) + wcslen(w) + 40 >= MAX_PATH)
        return 0;
    wcscat(p, ygo_mods_w());
    wcscat(p, L"\\perfil\\");
    wcscat(p, w);
    return leer(p, m);
}

/* the portraits (cartel.c's): the first face, the top 256 x 128 of the sheet */
static int leer_foto(int ch, img *m)
{
    char a[MAX_PATH];
    wchar_t p[MAX_PATH];
    snprintf(a, sizeof a, "%s\\cf_anim\\small\\png\\cfs%02d.png", g_ygo_game_dir ? g_ygo_game_dir : ".", ch);
    if (!MultiByteToWideChar(CP_ACP, 0, a, -1, p, MAX_PATH))
        return 0;
    return leer(p, m);
}

/* ---- drawing on a premultiplied BGRA canvas ------------------------------ */
typedef struct { unsigned char *p; int w, h; } lienzo;

static float s_s = 1.f;                 /* scale: window px per capture px */
static float X(float x) { return (float)s_cw * 0.5f + (x - REF_CX) * s_s; }
static float Y(float y) { return (float)s_ch * 0.5f + (y - REF_CY) * s_s; }

static void sobre(unsigned char *d, unsigned b, unsigned g, unsigned r, unsigned a)
{
    /* premultiplied source over */
    unsigned k = 255 - a;
    d[0] = (unsigned char)(b + d[0] * k / 255);
    d[1] = (unsigned char)(g + d[1] * k / 255);
    d[2] = (unsigned char)(r + d[2] * k / 255);
    d[3] = (unsigned char)(a + d[3] * k / 255);
}

/* src (a part of it: sx, sy, sw, sh) into dst's box, bilinear (an area
 * average when it is more than halved) */
static void pegar(lienzo *L, const img *m, int sx, int sy, int sw, int sh, float dx, float dy, float dw, float dh,
                  unsigned alfa)
{
    int x0 = (int)floorf(dx), y0 = (int)floorf(dy), x1 = (int)ceilf(dx + dw), y1 = (int)ceilf(dy + dh), x, y;
    float kx = (float)sw / dw, ky = (float)sh / dh;
    int caja = kx > 2.f || ky > 2.f;
    if (!m->p || dw < 1.f || dh < 1.f)
        return;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > L->w) x1 = L->w;
    if (y1 > L->h) y1 = L->h;
    for (y = y0; y < y1; y++) {
        float v = ((float)y + 0.5f - dy) * ky - 0.5f;
        for (x = x0; x < x1; x++) {
            float u = ((float)x + 0.5f - dx) * kx - 0.5f, c[4] = { 0, 0, 0, 0 };
            int k;
            if (u < -0.5f || v < -0.5f || u > (float)sw - 0.5f || v > (float)sh - 0.5f)
                continue;
            if (caja) {
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
            if (alfa != 255)
                for (k = 0; k < 4; k++)
                    c[k] = c[k] * (float)alfa / 255.f;
            if (c[3] >= 0.5f)
                sobre(L->p + 4 * ((size_t)y * L->w + x), (unsigned)(c[0] + 0.5f), (unsigned)(c[1] + 0.5f),
                      (unsigned)(c[2] + 0.5f), (unsigned)(c[3] + 0.5f));
        }
    }
}

static void pegar_todo(lienzo *L, const img *m, float x, float y, float w, float h)
{
    pegar(L, m, 0, 0, m->w, m->h, x, y, w, h, 255);
}

static void wide(const char *u8, wchar_t *w, int n)
{
    if (!MultiByteToWideChar(CP_UTF8, 0, u8, -1, w, n))
        w[0] = 0;
}

/* a line of the game's letters, centred on cx (capture px), its top at y */
/* maxw (capture px, 0: any): smaller until it fits, kept on the same middle
 * (2026-11-16, the user: "los nombres se salen de sus recuadros") */
static void texto_w(lienzo *L, float cx, float y, const wchar_t *w, float tam, COLORREF col, int borde, float maxw)
{
    int h = (int)(tam * s_s + 0.5f), h0, tw, lh, W, H, x0, y0, i, j, b = borde ? (int)(2.f * s_s + 1.f) : 0;
    unsigned char *t;
    if (h < 8)
        h = 8;
    h0 = h;
    while (maxw > 0.f && h > 8 && (float)ft_ancho(w, h) > maxw * s_s)
        h--;
    y += (float)(ft_linea(h0) - ft_linea(h)) * 0.5f / s_s;
    tw = ft_ancho(w, h);
    lh = ft_linea(h);
    W = tw + 2 * b + 4;
    H = lh + 2 * b + 4;
    x0 = (int)(X(cx) - (float)W * 0.5f);
    y0 = (int)Y(y) - b - 2;
    t = (unsigned char *)calloc((size_t)W * H, 4);
    if (!t)
        return;
    {
        RECT rc = { b + 2, b + 2, b + 2 + tw, b + 2 + lh };
        ft_dibujar(t, W, H, &rc, w, h, FJ_G);
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

static void texto(lienzo *L, float cx, float y, const char *u8, float tam, COLORREF col, int borde, float maxw)
{
    wchar_t w[160];
    wide(u8, w, 160);
    texto_w(L, cx, y, w, tam, col, borde, maxw);
}

/* a filled circle with an edge (capture px) */
static void circulo(lienzo *L, float cx, float cy, float r, COLORREF col, COLORREF borde, float grueso)
{
    float X0 = X(cx), Y0 = Y(cy), R = r * s_s, G = grueso * s_s;
    int x, y;
    for (y = (int)(Y0 - R - 2); y <= (int)(Y0 + R + 2); y++)
        for (x = (int)(X0 - R - 2); x <= (int)(X0 + R + 2); x++) {
            float d = sqrtf(((float)x + 0.5f - X0) * ((float)x + 0.5f - X0) + ((float)y + 0.5f - Y0) * ((float)y + 0.5f - Y0));
            float a = R + 0.5f - d, e = d - (R - G) + 0.5f;
            COLORREF c;
            unsigned A;
            if (x < 0 || y < 0 || x >= L->w || y >= L->h || a <= 0.f)
                continue;
            a = a > 1.f ? 1.f : a;
            e = e < 0.f ? 0.f : e > 1.f ? 1.f : e;
            c = RGB(GetRValue(col) + (int)((GetRValue(borde) - GetRValue(col)) * e),
                    GetGValue(col) + (int)((GetGValue(borde) - GetGValue(col)) * e),
                    GetBValue(col) + (int)((GetBValue(borde) - GetBValue(col)) * e));
            A = (unsigned)(a * 255.f + 0.5f);
            sobre(L->p + 4 * ((size_t)y * L->w + x), GetBValue(c) * A / 255, GetGValue(c) * A / 255,
                  GetRValue(c) * A / 255, A);
        }
}

/* the collection's bar (2026-11-16, the user: "la barrita de cuantas cartas
 * tenes esta buena", then "tiene que ser verde"): a dark track with a gold
 * edge, filled in green up to how much you have (capture px) */
static void barra(lienzo *L, float x, float y, float w, float h, float lleno)
{
    int X0 = (int)X(x), Y0 = (int)Y(y), X1 = (int)X(x + w), Y1 = (int)Y(y + h), i, j;
    int borde = (int)(2.f * s_s + 0.5f), hasta;
    if (borde < 1)
        borde = 1;
    lleno = lleno < 0.f ? 0.f : lleno > 1.f ? 1.f : lleno;
    hasta = X0 + borde + (int)((float)(X1 - X0 - 2 * borde) * lleno + 0.5f);
    for (j = Y0; j < Y1; j++)
        for (i = X0; i < X1; i++) {
            unsigned char *p;
            float t;
            if (i < 0 || j < 0 || i >= L->w || j >= L->h)
                continue;
            p = L->p + 4 * ((size_t)j * L->w + i);
            if (i - X0 < borde || X1 - 1 - i < borde || j - Y0 < borde || Y1 - 1 - j < borde)
                sobre(p, 20, 120, 200, 255);                       /* gold edge */
            else if (i < hasta) {
                t = (float)(j - Y0) / (float)(Y1 - Y0);            /* green (the user: "tiene que ser verde"), light top */
                sobre(p, (unsigned)(90 - 60 * t), (unsigned)(235 - 85 * t), (unsigned)(120 - 90 * t), 255);
            } else
                sobre(p, 4, 10, 22, 255);                          /* the track */
        }
}

/* a rectangle's edge (capture px) */
static void marco_rect(lienzo *L, float x, float y, float w, float h, float g, COLORREF col)
{
    int X0 = (int)X(x), Y0 = (int)Y(y), X1 = (int)X(x + w), Y1 = (int)Y(y + h), G = (int)(g * s_s + 0.5f), i, j;
    if (G < 1)
        G = 1;
    for (j = Y0; j < Y1; j++)
        for (i = X0; i < X1; i++) {
            if (i < 0 || j < 0 || i >= L->w || j >= L->h)
                continue;
            if (i - X0 < G || X1 - 1 - i < G || j - Y0 < G || Y1 - 1 - j < G)
                sobre(L->p + 4 * ((size_t)j * L->w + i), GetBValue(col), GetGValue(col), GetRValue(col), 255);
        }
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

static HWND s_arriba;     /* where a window first shown goes: under the doors while they show */

static void ocultar(ventana *v);

static void mostrar(ventana *v, HDC screen, const char *cls, POINT pos, int pintar, BYTE alfa)
{
    POINT src = { 0, 0 };
    SIZE sz;
    BLENDFUNCTION bf = { AC_SRC_OVER, 0, alfa, AC_SRC_ALPHA };
    if (!v->win)
        v->win = CreateWindowExA(WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW
                                 | WS_EX_TOPMOST, cls, "", WS_POPUP, 0, 0, 1, 1, NULL, NULL,
                                 GetModuleHandleA(NULL), NULL);
    if (!v->win)
        return;
    sz.cx = v->l.w;
    sz.cy = v->l.h;
    if (pintar || !v->shown)
        UpdateLayeredWindow(v->win, screen, &pos, &sz, v->mem, &src, 0, &bf, ULW_ALPHA);
    else
        UpdateLayeredWindow(v->win, NULL, &pos, NULL, NULL, NULL, 0, NULL, 0);
    if (!v->shown)
        SetWindowPos(v->win, s_arriba && s_arriba != v->win ? s_arriba : HWND_TOPMOST, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
    v->shown = 1;
}

static void ocultar(ventana *v)
{
    if (v->shown && v->win)
        ShowWindow(v->win, SW_HIDE);
    v->shown = 0;
}

/* ---- the panel ------------------------------------------------------------ */
/* 2026-11-20 (the user: "un Botón para salir a Windows. abajo del ultimo Boton
 * ... el mismo comportamiento que nuestro Boton para perfil"): its medallion
 * (tools/salir_recursos.py: salir*.png, as boton*.png), under the wheel's
 * last icon: SALIR_DX / DY game px from the wheel's centre (the user's
 * capture at 1919 x 1079 of the stretched frame: 498, 145 px), SALIR_K the
 * Perfil button's size */
/* 2026-11-20 (the user: the exit in Duelo vinculado's place): drawn over the
 * wheel's last icon (raton.c g_ygo_sal_*), at its size: the medallion is
 * 1.3 times the wheel's in salir*.png, cut from a 1919 x 1079 capture of
 * the stretched 640 x 480 frame */
extern volatile float g_ygo_sal_x, g_ygo_sal_y;
extern volatile int g_ygo_sal_sel;
extern volatile ULONGLONG g_ygo_sal_at;
#define SALIR_KX (640.f / 1919.f / 1.3f)
#define SALIR_KY (480.f / 1079.f / 1.3f)
static img s_salir, s_salir_luz, s_salir_sol;
/* 2026-11-20 (the user: Perfil and Salir "se sientan como botones reales ...
 * no se puede iluminar 2 Botones a la vez y abajo dice que es"): the kit
 * leaves out the wheel's chosen icon's sun and lit medallion in a box
 * (nv2a_pb_d3d11.c oculta_2d): always on the last icon (our exit medallion
 * is there), everywhere while the pointer is on the Perfil button; and the
 * banner's middle (cartel.png, tools/cartel_recursos.py) says "Perfil"
 * meanwhile, at CARTEL_* game px */
struct nv2a_oculta_kit {                /* nv2a_pb_d3d11.c's, 3 boxes */
    volatile float x[3], y[3], r[3];
    volatile ULONGLONG hasta[3];
    volatile float bx0[3], by0[3], bx1[3], by1[3];
    volatile ULONGLONG medido[3];
};
/* the chosen icon (raton.c) and the unlit look of icons 0..5 (rueda_K.png:
 * the game's 60 x 60 px quad, tools/cartel_recursos.py), laid over the
 * chosen one while the pointer is on the Perfil: it is drawn bright in the
 * row itself */
extern volatile int g_ygo_sel_k;
extern volatile float g_ygo_sel_x, g_ygo_sel_y;
static img s_rueda[6];
extern struct nv2a_oculta_kit g_nv2a_oculta;
#define CARTEL_X0 216.9f
#define CARTEL_Y0 346.5f
#define CARTEL_X1 426.8f
#define CARTEL_Y1 378.0f
static img s_cartel;
/* 2026-11-20 (the user's capture: our "Perfil" plate looked pasted on, "ponelo
 * como los demas"): raton.c has the game itself write "Perfil" in the banner
 * (its own letters, as it does for each icon) while this says the pointer is
 * on the Perfil button; cartel.png is only the fallback if the game's banner
 * was not set within 400 ms */
volatile ULONGLONG g_ygo_cartel_perfil_hasta;   /* set here: on the button until then */
extern volatile ULONGLONG g_ygo_cartel_perfil_ok;   /* raton.c: the game's banner says Perfil */
static img s_fondo, s_fondo_esc, s_boton, s_boton_luz, s_boton_sol, s_flecha_luz[2], s_flecha[2], s_foto_luz;
static img s_placa_nombre, s_masc_luz, s_funda_luz;              /* 2026-11-19: off fondo.png, the Perfil page only */
static img s_fd[2], s_fd_luz[2];        /* 2026-11-19: the arrows (option D), strips */
static ULONGLONG s_fd_clic[6];          /* when each was clicked */
static img s_tapete_img;
static int s_tapete_de = -1;
int ygo_tapete_get(void);
void ygo_tapete_set(int i);
const char *ygo_perfil_tapete_nombre(int i);
static img s_fondo_completo, s_puerta[2], s_puerta_esc[2];
static ULONGLONG s_abrir_t, s_cerrar_t;   /* the doors: when the panel opened / began to close */
/* the doors as the game's menus (the user's videos): they slide shut in
 * ~0.2 s, stay ~0.33 s, slide open in ~0.27 s */
#define CIERRA_MS 200u
#define QUIETAS_MS 330u
#define ABRE_MS 270u
#define PUERTAS_MS (CIERRA_MS + QUIETAS_MS + ABRE_MS)
static float puertas_f(ULONGLONG e)       /* 0 open .. 1 shut */
{
    float u;
    if (e < CIERRA_MS) {
        u = (float)e / CIERRA_MS;
        return u * u * (3.f - 2.f * u);
    }
    if (e < CIERRA_MS + QUIETAS_MS)
        return 1.f;
    if (e < PUERTAS_MS) {
        u = (float)(e - CIERRA_MS - QUIETAS_MS) / ABRE_MS;
        return 1.f - u * u * (3.f - 2.f * u);
    }
    return 0.f;
}
#define SOL_CUADROS 12      /* tools/perfil_recursos.py: one ray step in 12 */
#define ONDA_K 2.2f         /* the button's window, times the button */
#define ONDA_MS 200.f       /* the wave's life */
static img s_funda_img;
/* the mascot's strip kept as it is stored, 256 colours (a 4 s loop at 60 a
 * second would be ~100 MB as BGRA): the indices and the palette, premultiplied */
typedef struct { int w, h; unsigned char *idx; unsigned pal[256]; float ms; } tira_t;
static tira_t s_tira;
static int s_funda_de = -1, s_tira_de = -1;
static float s_fondo_s;
static img s_fondo2, s_fondo2_esc;      /* fondo.png without its boxes (page 2) */
static float s_fondo2_s;
static img s_ret[28];                   /* the opponents' portraits, as needed */

/* what the panel shows of the name and photo (copied under the lock) */
static wchar_t s_p_nombre[NOMBRE_MAX + 1], s_p_edit[NOMBRE_MAX + 1];
static int s_p_foto;

/* the cards you have (one copy or more) and the game's cards (the list's
 * entries with an ID) */
static void contar_cartas(int *tuyas, int *total)
{
    unsigned k;
    *tuyas = *total = 0;
    for (k = 1; k < NCARTAS; k++) {
        if (MEM16(T_IMAGEN + 2u * k))
            ++*total;
        if (MEM8(COLECCION + k))
            ++*tuyas;
    }
}

/* fondo.png's inside covered with its own plain stone (a piece of its
 * empty right side, tiled), its frame kept: the statistics page's panel */
static void flechas_afuera(void)
{
    /* fondo.png px: the two triangles and the two medallion icons */
    static const int k_caja[4][4] = { { 448, 52, 490, 108 }, { 958, 52, 1000, 108 }, { 360, 86, 392, 114 },
                                      { 1050, 86, 1082, 114 } };
    int b, x, y;
    if (!s_fondo.p || s_fondo.w < 1444 || s_fondo.h < 964)
        return;
    for (b = 0; b < 4; b++)
        for (y = k_caja[b][1]; y < k_caja[b][3]; y++)
            for (x = 0; x < (k_caja[b][2] - k_caja[b][0]) / 2; x++) {
                unsigned char *l = s_fondo.p + 4 * ((size_t)y * s_fondo.w + k_caja[b][0] + x);
                unsigned char *r = s_fondo.p + 4 * ((size_t)y * s_fondo.w + k_caja[b][2] - 1 - x), t[4];
                memcpy(t, l, 4);
                memcpy(l, r, 4);
                memcpy(r, t, 4);
            }
}

/* 2026-11-16 (the user: "dejame los verdes nomas y hacele una animacion
 * cuando el mouse pasa por arriba y cuando le hace click"): the two green
 * triangles leave the panel (its plate filled in under them) and are drawn
 * in small windows of their own, so they move without repainting the panel */
static img s_tri[2], s_tri_luz[2];      /* premultiplied; _luz brighter */
static const int k_tri[2][4] = { { 448, 52, 490, 108 }, { 958, 52, 1000, 108 } };   /* fondo.png px */
#define TRI_CX(i) ((float)(k_tri[i][0] + k_tri[i][2]) * 0.5f + FONDO_X)              /* ref */
#define TRI_CY(i) ((float)(k_tri[i][1] + k_tri[i][3]) * 0.5f + FONDO_Y)
#define TRI_VENT 110.f                  /* their windows, ref px */

static void sacar_triangulos(void)
{
    int i, x, y;
    if (!s_fondo.p || s_fondo.w < 1444 || s_fondo.h < 964)
        return;
    for (i = 0; i < 2; i++) {
        int x0 = k_tri[i][0], y0 = k_tri[i][1], w = k_tri[i][2] - x0, h = k_tri[i][3] - y0;
        s_tri[i].p = (unsigned char *)calloc((size_t)w * h, 4);
        s_tri_luz[i].p = (unsigned char *)calloc((size_t)w * h, 4);
        if (!s_tri[i].p || !s_tri_luz[i].p)
            continue;
        s_tri[i].w = s_tri_luz[i].w = w;
        s_tri[i].h = s_tri_luz[i].h = h;
        for (y = 0; y < h; y++) {
            unsigned char *fila = s_fondo.p + 4 * ((size_t)(y0 + y) * s_fondo.w);
            const unsigned char *a = fila + 4 * (x0 - 1), *b = fila + 4 * (x0 + w);
            for (x = 0; x < w; x++) {
                unsigned char *q = fila + 4 * (x0 + x), *d = s_tri[i].p + 4 * ((size_t)y * w + x);
                unsigned char *e = s_tri_luz[i].p + 4 * ((size_t)y * w + x);
                float f = ((float)x + 0.5f) / (float)w, pl[4], dif = 0.f, al;
                int c;
                for (c = 0; c < 4; c++) {   /* the plate there: across from the box's two sides */
                    pl[c] = (float)a[c] * (1.f - f) + (float)b[c] * f;
                    if (c < 3 && fabsf((float)q[c] - pl[c]) > dif)
                        dif = fabsf((float)q[c] - pl[c]);
                }
                al = (dif - 12.f) / 50.f;   /* how much it is not the plate (the triangle and its dark edge) */
                al = al < 0.f ? 0.f : al > 1.f ? 1.f : al;
                if ((int)q[2] > (int)q[1] + 30)
                    al = 0.f;               /* orange: the plate's own bevels */
                if (al > 0.f)
                    for (c = 0; c < 3; c++) {
                        float v = (float)q[c] - (1.f - al) * pl[c], l = v * 1.45f + 18.f * al;
                        v = v < 0.f ? 0.f : v;
                        d[c] = (unsigned char)(v + 0.5f);
                        e[c] = (unsigned char)(l > 255.f * al ? 255.f * al : l < 0.f ? 0.f : l);
                    }
                d[3] = e[3] = (unsigned char)(255.f * al + 0.5f);
                for (c = 0; c < 4; c++)
                    q[c] = (unsigned char)(pl[c] + 0.5f);
            }
        }
    }
}

static void hacer_fondo2(void)
{
    int x, y, X0 = 46, Y0 = 238, X1 = 1398, Y1 = 922, PX = 960, PY = 270, PW = 420, PH = 370;
    if (!s_fondo.p || s_fondo.w < 1444 || s_fondo.h < 964)
        return;
    s_fondo2.w = s_fondo.w;
    s_fondo2.h = s_fondo.h;
    s_fondo2.p = (unsigned char *)malloc((size_t)s_fondo.w * s_fondo.h * 4);
    if (!s_fondo2.p)
        return;
    memcpy(s_fondo2.p, s_fondo.p, (size_t)s_fondo.w * s_fondo.h * 4);
    for (y = Y0; y < Y1; y++)
        for (x = X0; x < X1; x++) {
            int sx = PX + (x - 44) % PW, sy = PY + (y - 236) % PH;
            memcpy(s_fondo2.p + 4 * ((size_t)y * s_fondo.w + x), s_fondo.p + 4 * ((size_t)sy * s_fondo.w + sx), 4);
        }
    /* the sleeve's right arrow pokes over the frame there: frame from higher up */
    for (y = 680; y < 800; y++)
        memcpy(s_fondo2.p + 4 * ((size_t)y * s_fondo.w + 1390), s_fondo.p + 4 * ((size_t)(y - 280) * s_fondo.w + 1390),
               (size_t)(s_fondo.w - 1390) * 4);
}

/* a panel image scaled once per size, at its place */
static void pegar_panel(lienzo *L, const img *src, img *esc, float *de)
{
    int fw, fh;
    if (!src->p)
        return;
    fw = (int)(src->w * s_s + 0.5f);
    fh = (int)(src->h * s_s + 0.5f);
    if (*de != s_s || !esc->p) {
        lienzo t;
        liberar(esc);
        esc->p = (unsigned char *)calloc((size_t)fw * fh, 4);
        if (esc->p) {
            esc->w = fw;
            esc->h = fh;
            t.p = esc->p; t.w = fw; t.h = fh;
            pegar(&t, src, 0, 0, src->w, src->h, 0.f, 0.f, (float)fw, (float)fh, 255);
            *de = s_s;
        }
    }
    if (esc->p)
        pegar(L, esc, 0, 0, fw, fh, (float)(int)X(FONDO_X), (float)(int)Y(FONDO_Y), (float)fw, (float)fh, 255);
}

/* a line starting at x (capture px) */
static void texto_izq(lienzo *L, float x, float y, const char *u8, float tam, COLORREF col, int borde, float maxw)
{
    wchar_t w[160];
    float tw;
    int h = (int)(tam * s_s + 0.5f);
    wide(u8, w, 160);
    tw = (float)ft_ancho(w, h < 8 ? 8 : h) / s_s;
    if (maxw > 0.f && tw > maxw)
        tw = maxw;
    texto_w(L, x + tw * 0.5f, y, w, tam, col, borde, maxw);
}

/* a dark box with a gold edge (capture px) */
static void caja(lienzo *L, float x, float y, float w, float h, unsigned r, unsigned g, unsigned b, unsigned a,
                 COLORREF borde)
{
    int X0 = (int)X(x), Y0 = (int)Y(y), X1 = (int)X(x + w), Y1 = (int)Y(y + h), i, j;
    for (j = Y0; j < Y1; j++)
        for (i = X0; i < X1; i++)
            if (i >= 0 && j >= 0 && i < L->w && j < L->h)
                sobre(L->p + 4 * ((size_t)j * L->w + i), b * a / 255, g * a / 255, r * a / 255, a);
    marco_rect(L, x, y, w, h, 3.f, borde);
}

static void retrato(lienzo *L, int ch, float x, float y, float w, float h)
{
    int k = ygo_estadisticas_retrato(ch);
    if (!s_ret[k].p)
        leer_foto(k, &s_ret[k]);
    if (s_ret[k].p && s_ret[k].w >= 256 && s_ret[k].h >= 128)
        pegar(L, &s_ret[k], 64, 0, 192, 128, X(x), Y(y), w * s_s, h * s_s, 255);   /* the first face */
}

/* ---- a rival's deck (2026-11-17, the user: "en estadisticas ... si
 * presionas en ellos te muestre el Deck que usa"; mock-up approved) --------
 *
 * What the game does (read in the code, sub_0008C760 at a duel's start): an
 * opponent has no fixed deck. sub_0008C6D0 puts in every card of its list B
 * (u32 list at 0x453830 + 16 d + 4 r, u16 count at 0x453838 + 16 d + 2 r;
 * entries u16 card ID | weight), then sub_0008C5E0 fills up to 40 with cards
 * picked at random from list A (the same at 0x453630 / 0x453638). d is the
 * duelist, s8 0x423D60[character]; r the story's variant (sub_0008C530,
 * premios.c's ygo_premios_variante). The panel: list B ("always"), list A
 * ("may add"), each card once with its copies, a tick on the ones you have.
 * Read on the game's thread when clicked (raton.c -> ygo_perfil_raton). */
#define MAZO_MAX 64
#define T_DATOS 0x005FEF78u
typedef struct { int k, n; } mcarta;
/* prestado.c: "Probar este Mazo" (2026-11-20) */
typedef struct {
    int puede;                          /* the character has a deck */
    int usos, ganado, activo;           /* uses left; earned; this one is on */
    int tengo, total;                   /* the cards it gives you own / all */
    int ttengo, ttotal;                 /* the same in Tournaments */
} ygo_prestado_info;
void ygo_prestado_info_de(int ch, ygo_prestado_info *o);
void ygo_prestado_usar(int ch, int si);
int ygo_prestado_ganado(int ch);
void ygo_prestado_revisar_todos(void);
typedef struct {
    int ch;                             /* -1: closed */
    int g, p, totf, nf, na;
    int grande;                         /* the card (k) shown big, 0 none */
    int conf;                           /* the "Probar este Mazo" question is up */
    ygo_prestado_info pi;
    mcarta f[MAZO_MAX], a[MAZO_MAX];
} mazo_t;
static mazo_t s_mazo = { -1 }, s_p_mazo = { -1 };
static int s_p_sobre = 0;               /* NADA */
static img s_minis;                     /* buscador.c's thumbnails: 35 x 51 cells, 40 a row, ID - 4000 */
int ygo_premios_variante(int d);

#define MZ_X0 290.f
#define MZ_Y0 180.f
#define MZ_X1 1630.f
#define MZ_Y1 995.f
#define MZ_GY0 400.f                    /* the thumbnails' top */
#define MZ_GY1 890.f
#define MZ_INFO_Y 910.f
#define MZ_CX (MZ_X1 - 58.f)            /* the close button */
#define MZ_CY (MZ_Y0 + 58.f)
#define MZ_CR 30.f
/* 2026-11-20: "Probar este Mazo" (prestado.c): the button in the header,
 * the cards line under it, the question over the panel */
#define BT_X0 1160.f
#define BT_X1 1490.f
#define BT_Y0 212.f
#define BT_Y1 262.f
#define PR_Y 316.f                      /* the line under the record */
#define CF_X0 520.f
#define CF_X1 1400.f
#define CF_Y0 400.f
#define CF_Y1 730.f
#define CF_BW 260.f
#define CF_BH 60.f
#define CF_BY (CF_Y1 - 96.f)
#define CF_SI_X ((CF_X0 + CF_X1) * 0.5f - CF_BW - 40.f)
#define CF_NO_X ((CF_X0 + CF_X1) * 0.5f + 40.f)

static int mazo_k(uint32_t id)
{
    uint32_t k;
    for (k = 1; k < NCARTAS; ++k)
        if (MEM16(T_IMAGEN + 2u * k) == id)
            return (int)k;
    return 0;
}

/* monsters first (the strongest first), then spells, then traps */
static int mazo_clave(int k)
{
    uint32_t w = MEM32(T_DATOS + 4u * (uint32_t)k), tp = (w >> 20) & 31u;
    if (tp == 22u)
        return 10000;
    if (tp == 21u)
        return 20000;
    return 5000 - (int)((w >> 9) & 511u);
}

static int mazo_cmp(const void *a, const void *b)
{
    const mcarta *x = (const mcarta *)a, *y = (const mcarta *)b;
    int cx = mazo_clave(x->k), cy = mazo_clave(y->k);
    return cx != cy ? cx - cy : x->k - y->k;
}

static int mazo_lista(uint32_t tab, int d, int r, mcarta *out, int *tot)
{
    uint32_t p = MEM32(tab + 16u * (uint32_t)d + 4u * (uint32_t)r);
    uint32_t c = MEM16(tab + 8u + 16u * (uint32_t)d + 2u * (uint32_t)r), i;
    int n = 0, j, k;
    *tot = 0;
    if (!p || c > 400u)
        return 0;
    for (i = 0; i < c; i++) {
        k = mazo_k(MEM32(p + 4u * i) & 0xFFFFu);
        if (k <= 0)
            continue;
        ++*tot;
        for (j = 0; j < n && out[j].k != k; j++)
            ;
        if (j < n)
            out[j].n++;
        else if (n < MAZO_MAX) {
            out[n].k = k;
            out[n].n = 1;
            n++;
        }
    }
    qsort(out, (size_t)n, sizeof *out, mazo_cmp);
    return n;
}

/* 2026-11-25: the other story variant's cards (either list) not shown yet
 * go to the "may add" side, so the panel shows every card the rival gives */
static void mazo_sumar(int d, int r)
{
    mcarta x[MAZO_MAX];
    int t, i, j, nx, tot;
    for (t = 0; t < 2; t++) {
        nx = mazo_lista(t ? 0x00453630u : 0x00453830u, d, r, x, &tot);
        for (i = 0; i < nx; i++) {
            int ya = 0;
            for (j = 0; j < s_mazo.nf && !ya; j++)
                ya = s_mazo.f[j].k == x[i].k;
            for (j = 0; j < s_mazo.na && !ya; j++)
                ya = s_mazo.a[j].k == x[i].k;
            if (!ya && s_mazo.na < MAZO_MAX)
                s_mazo.a[s_mazo.na++] = x[i];
        }
    }
    qsort(s_mazo.a, (size_t)s_mazo.na, sizeof s_mazo.a[0], mazo_cmp);
}

static void mazo_abrir(int ch)          /* the game's thread, s_cs held */
{
    int d, r, tot;
    memset(&s_mazo, 0, sizeof s_mazo);
    s_mazo.ch = ch;
    ygo_estadisticas_rival(ch, &s_mazo.g, &s_mazo.p);
    if (ch < 0 || ch > 39)
        return;
    d = (int)(int8_t)MEM8(0x00423D60u + (uint32_t)ch);
    if (d <= 0 || d >= 64)
        return;
    r = ygo_premios_variante(d);
    s_mazo.nf = mazo_lista(0x00453830u, d, r, s_mazo.f, &s_mazo.totf);
    s_mazo.na = mazo_lista(0x00453630u, d, r, s_mazo.a, &tot);
    mazo_sumar(d, 1 - r);
    ygo_prestado_info_de(ch, &s_mazo.pi);
    if (!s_mazo.nf && !s_mazo.na)
        s_mazo.pi.puede = 0;            /* nothing to play with */
    fprintf(stderr, "[PERFIL] mazo del rival: personaje %d, duelista %d, variante %d: %d fijas (%d distintas), %d al azar\n",
            ch, d, r, s_mazo.totf, s_mazo.nf, s_mazo.na);
    fflush(stderr);
}

/* where the thumbnails go (capture px): as large as fits, both grids side
 * by side, as many rows as the room below the titles takes */
typedef struct { float tw, th, gap, xf, xa; int cf, ca; } mlay;
static void mazo_layout(const mazo_t *m, mlay *o)
{
    float th, wg = MZ_X1 - MZ_X0 - 80.f - 50.f;
    memset(o, 0, sizeof *o);
    for (th = 112.f; th >= 40.f; th -= 2.f) {
        float tw = th * 35.f / 51.f, gap = th * 0.12f;
        int rows = (int)((MZ_GY1 - MZ_GY0 + gap) / (th + gap));
        if (rows < 1)
            continue;
        o->cf = m->nf ? (m->nf + rows - 1) / rows : 0;
        o->ca = m->na ? (m->na + rows - 1) / rows : 0;
        o->tw = tw;
        o->th = th;
        o->gap = gap;
        if ((float)(o->cf + o->ca) * (tw + gap) <= wg)
            break;
    }
    /* the left grid at least as wide as its title, when it has the cards */
    if (o->cf && (float)o->cf * (o->tw + o->gap) < 520.f && m->nf > o->cf) {
        int c = (int)(520.f / (o->tw + o->gap));
        if (c > m->nf)
            c = m->nf;
        if ((float)(c + o->ca) * (o->tw + o->gap) <= wg)
            o->cf = c;
    }
    o->xf = MZ_X0 + 40.f;
    o->xa = o->xf + (o->cf ? (float)o->cf * (o->tw + o->gap) + 50.f : 0.f);
}

static void mazo_celda(const mlay *o, int lado, int i, float *x, float *y)
{
    int c = lado ? o->ca : o->cf;
    if (c < 1)
        c = 1;
    *x = (lado ? o->xa : o->xf) + (float)(i % c) * (o->tw + o->gap);
    *y = MZ_GY0 + (float)(i / c) * (o->th + o->gap);
}

/* 2026-11-17 (the user's capture: "no se ve bien las cartas"): the 35 x 51
 * thumbnails blew up blurry; the panel draws the game's own card picture
 * instead, euro\<language>\card\normal\<NN00>\<ID>.tga (every one: type 1,
 * a 256-colour palette of 24 bits, 128 x 256, bottom row first; the card is
 * the top 185 rows), the card mod's own first (Mods-Extras\cartas\archivos,
 * the same tree). Loaded on the Perfil's thread as drawn, kept (a panel shows
 * at most 2 x MAZO_MAX). The thumbnail stays as the fallback. */
#define CARTA_TGA_H 185
#define NCACHE 160
static struct { uint32_t id, lang; img m; } s_cartas[NCACHE];
static int s_ncartas;

static int leer_tga(const wchar_t *ruta, img *m)
{
    HANDLE f = CreateFileW(ruta, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    unsigned char hd[18], pal[768], *px = NULL;
    DWORD n = 0;
    int x, y, ok = 0;
    if (f == INVALID_HANDLE_VALUE)
        return 0;
    if (ReadFile(f, hd, 18, &n, NULL) && n == 18 && hd[1] == 1 && hd[2] == 1 && hd[7] == 24 && hd[16] == 8
            && (hd[12] | hd[13] << 8) == 128 && (hd[14] | hd[15] << 8) == 256 && (hd[5] | hd[6] << 8) == 256
            && SetFilePointer(f, 18 + hd[0], NULL, FILE_BEGIN) != INVALID_SET_FILE_POINTER
            && ReadFile(f, pal, 768, &n, NULL) && n == 768 && (px = (unsigned char *)malloc(128 * 256))
            && ReadFile(f, px, 128 * 256, &n, NULL) && n == 128 * 256
            && (m->p = (unsigned char *)malloc((size_t)128 * CARTA_TGA_H * 4))) {
        m->w = 128;
        m->h = CARTA_TGA_H;
        for (y = 0; y < CARTA_TGA_H; y++) {
            const unsigned char *s = px + (size_t)(255 - y) * 128;     /* bottom row first */
            unsigned char *d = m->p + (size_t)y * 128 * 4;
            for (x = 0; x < 128; x++) {
                const unsigned char *c = pal + 3 * s[x];
                d[4 * x] = c[0];
                d[4 * x + 1] = c[1];
                d[4 * x + 2] = c[2];
                d[4 * x + 3] = 255;
            }
        }
        ok = 1;
    }
    free(px);
    CloseHandle(f);
    return ok;
}

static const img *carta_img(uint32_t id)
{
    static const wchar_t *const k_dir[6] = { L"en", L"en", L"gr", L"fr", L"it", L"sp" };
    uint32_t lang = MEM32(0x005FC2C0u);
    wchar_t p[MAX_PATH], *barra;
    int i;
    if (lang >= 6u)
        lang = 5u;
    for (i = 0; i < s_ncartas; i++)
        if (s_cartas[i].id == id && s_cartas[i].lang == lang)
            return s_cartas[i].m.p ? &s_cartas[i].m : NULL;
    if (s_ncartas == NCACHE) {                  /* full: start over */
        for (i = 0; i < NCACHE; i++)
            liberar(&s_cartas[i].m);
        s_ncartas = 0;
    }
    i = s_ncartas++;
    s_cartas[i].id = id;
    s_cartas[i].lang = lang;
    memset(&s_cartas[i].m, 0, sizeof s_cartas[i].m);
    /* 2026-11-17 (the user: "hay imagenes mejores como las de la
     * Biblioteca"): first the game's own enhanced card (the texture pack's
     * x4, what the Biblioteca shows), made for this panel by
     * tools/perfil_cartas.py: Mods-Extras\perfil\cartas\<language>\<ID>.jpg;
     * then the card mod's .tga, then the game's */
    if (GetModuleFileNameW(NULL, p, MAX_PATH) && (barra = wcsrchr(p, L'\\'))) {
        *barra = 0;
        if (wcslen(p) + 80 < MAX_PATH) {
            _snwprintf(barra, MAX_PATH - (barra - p), L"%ls\\perfil\\cartas\\%ls\\%u.jpg", ygo_mods_w(), k_dir[lang], id);
            p[MAX_PATH - 1] = 0;
            if (GetFileAttributesW(p) != INVALID_FILE_ATTRIBUTES && leer(p, &s_cartas[i].m))
                return &s_cartas[i].m;
        }
    }
    /* the card mod's, beside the exe */
    if (GetModuleFileNameW(NULL, p, MAX_PATH) && (barra = wcsrchr(p, L'\\'))) {
        *barra = 0;
        if (wcslen(p) + 80 < MAX_PATH) {
            _snwprintf(barra, MAX_PATH - (barra - p), L"%ls\\cartas\\archivos\\euro\\%ls\\card\\normal\\%u\\%u.tga",
                       ygo_mods_w(), k_dir[lang], id / 100u * 100u, id);
            p[MAX_PATH - 1] = 0;
            if (leer_tga(p, &s_cartas[i].m))
                return &s_cartas[i].m;
        }
    }
    {
        char a[MAX_PATH];
        snprintf(a, sizeof a, "%s\\euro\\%ls\\card\\normal\\%u\\%u.tga", g_ygo_game_dir ? g_ygo_game_dir : ".",
                 k_dir[lang], id / 100u * 100u, id);
        if (MultiByteToWideChar(CP_ACP, 0, a, -1, p, MAX_PATH) && leer_tga(p, &s_cartas[i].m))
            return &s_cartas[i].m;
    }
    return NULL;
}

/* 2026-11-19 (the user's capture: the big card blurry, 144 x 208 blown up):
 * the big card's own picture, 512 x 740, Mods-Extras\perfil\cartas\<language>\g\<ID>.jpg
 * (tools/perfil_cartas.py, the opponents' cards only); else the small one */
static img s_grande_img;
static uint32_t s_grande_id = 0xFFFFFFFFu, s_grande_lang;

static const img *carta_grande_img(uint32_t id)
{
    static const wchar_t *const k_dir[6] = { L"en", L"en", L"gr", L"fr", L"it", L"sp" };
    uint32_t lang = MEM32(0x005FC2C0u);
    wchar_t p[MAX_PATH], *barra;
    if (lang >= 6u)
        lang = 5u;
    if (s_grande_id != id || s_grande_lang != lang) {
        liberar(&s_grande_img);
        s_grande_id = id;
        s_grande_lang = lang;
        if (GetModuleFileNameW(NULL, p, MAX_PATH) && (barra = wcsrchr(p, L'\\'))) {
            *barra = 0;
            if (wcslen(p) + 80 < MAX_PATH) {
                _snwprintf(barra, MAX_PATH - (barra - p), L"%ls\\perfil\\cartas\\%ls\\g\\%u.jpg", ygo_mods_w(), k_dir[lang], id);
                p[MAX_PATH - 1] = 0;
                if (GetFileAttributesW(p) != INVALID_FILE_ATTRIBUTES)
                    leer(p, &s_grande_img);
            }
        }
    }
    return s_grande_img.p ? &s_grande_img : carta_img(id);
}

/* the panel's part under the pointer (capture px) */
static int mazo_que(const mazo_t *m, float x, float y)
{
    mlay o;
    int lado, i;
    float dx = x - MZ_CX, dy = y - MZ_CY;
    if (m->conf) {                      /* the question: its two buttons, the rest is "no" */
        if (y >= CF_BY && y <= CF_BY + CF_BH && x >= CF_SI_X && x <= CF_SI_X + CF_BW)
            return CONF_SI;
        if (y >= CF_BY && y <= CF_BY + CF_BH && x >= CF_NO_X && x <= CF_NO_X + CF_BW)
            return CONF_NO;
        return MAZO_PANEL;
    }
    if (dx * dx + dy * dy <= (MZ_CR + 6.f) * (MZ_CR + 6.f))
        return MAZO_X;
    if (x < MZ_X0 || x > MZ_X1 || y < MZ_Y0 || y > MZ_Y1)
        return NADA;
    if (m->pi.puede && x >= BT_X0 && x <= BT_X1 && y >= BT_Y0 && y <= BT_Y1)
        return MAZO_BOTON;
    mazo_layout(m, &o);
    for (lado = 0; lado < 2; lado++)
        for (i = 0; i < (lado ? m->na : m->nf); i++) {
            float cx, cy;
            mazo_celda(&o, lado, i, &cx, &cy);
            if (x >= cx - o.gap * 0.5f && x < cx + o.tw + o.gap * 0.5f && y >= cy - o.gap * 0.5f
                    && y < cy + o.th + o.gap * 0.5f)
                return MAZO_CARTA + lado * MAZO_MAX + i;
        }
    return MAZO_PANEL;
}

/* a thick line (capture px), for the tick */
static void linea(lienzo *L, float x0, float y0, float x1, float y1, float g, COLORREF col)
{
    int k, n = (int)(fmaxf(fabsf(x1 - x0), fabsf(y1 - y0)) * s_s * 2.f) + 1;
    float r = g * s_s * 0.5f;
    for (k = 0; k <= n; k++) {
        float t = (float)k / (float)n, cx = X(x0 + (x1 - x0) * t), cy = Y(y0 + (y1 - y0) * t);
        int i, j;
        for (j = (int)(cy - r); j <= (int)(cy + r); j++)
            for (i = (int)(cx - r); i <= (int)(cx + r); i++)
                if (i >= 0 && j >= 0 && i < L->w && j < L->h)
                    sobre(L->p + 4 * ((size_t)j * L->w + i), GetBValue(col), GetGValue(col), GetRValue(col), 255);
    }
}

/* 2026-11-19: the limited card's circle, as tools/cartas_mod.py's insignia
 * (Master Duel: red ring, black middle, yellow number), here drawn since
 * the panel's pictures are not the texture pack's. The save's limit:
 * u8 at 0x665896 + k (sub_0007EF00), 1 or 2 */
unsigned ygo_limite_carta(uint32_t id);     /* cartas.c */
static int limite_de(int k)
{
    /* 2026-11-19: that byte is 0 for every card here (buscador.c): the card's own limit */
    uint32_t t = k > 0 && (uint32_t)k < NCARTAS && MEM16(T_IMAGEN + 2u * (uint32_t)k) ? ygo_limite_carta(MEM16(T_IMAGEN + 2u * (uint32_t)k)) : 0u;
    return t >= 1u && t <= 2u ? (int)t : 0;
}

static void insignia(lienzo *L, float cx, float cy, float r, int n)
{
    char t[4];
    float tam = r * (1.f - 1.5f / 9.f) * 1.7f;
    circulo(L, cx, cy, r, RGB(8, 3, 3), RGB(250, 5, 5), r * 1.5f / 9.f);
    snprintf(t, sizeof t, "%d", n);
    texto(L, cx, cy - tam * 0.62f, t, tam, RGB(250, 205, 15), 0, 0.f);
}

/* the name, the Duel Computer with its number (characters 31..33: 1..3) */
static void nombre_rival(int ch, char *out, int cap)
{
    ygo_estadisticas_nombre(ch, out, cap);
    if (ch >= 31 && ch <= 33) {
        size_t l = strlen(out);
        snprintf(out + l, (size_t)cap - l, " %d", ch - 30);
    }
}

/* 2026-11-20 (the user: "me gusto el Ganado hacelo mas chico que se vea pero
 * que cubra poco al personaje"): a small gold seal on the portrait's bottom
 * right corner, scalloped edge, the word in the game's letters */
static void sello_ganado(lienzo *L, float cx, float cy, float r)
{
    /* 2026-11-20 (the user: "saca el ganado y agrega un tick verde"): a green tick in a dark disc */
    static const COLORREF VERDE = RGB(110, 230, 120);
    circulo(L, cx, cy, r, RGB(20, 60, 25), VERDE, r * 0.12f);
    linea(L, cx - r * 0.5f, cy, cx - r * 0.1f, cy + r * 0.4f, r * 0.24f, VERDE);
    linea(L, cx - r * 0.1f, cy + r * 0.4f, cx + r * 0.55f, cy - r * 0.45f, r * 0.24f, VERDE);
}

/* the header's button, the cards line and the seal (prestado.c) */
static void pintar_prestado(lienzo *L, const mazo_t *m, int sobre_que)
{
    static const COLORREF ORO = RGB(255, 225, 130), TAN = RGB(230, 195, 120), BORDE = RGB(232, 176, 74),
                          VERDE = RGB(110, 230, 120);
    const ygo_prestado_info *pi = &m->pi;
    char buf[160], nom[64];
    int sob = sobre_que == MAZO_BOTON, apagado = !pi->activo && !pi->ganado && pi->usos <= 0;
    float bw = BT_X1 - BT_X0, bh = BT_Y1 - BT_Y0;
    /* the button: "Probar este Mazo 2/3" (uses left), "Jugar con este mazo"
     * once earned, "Devolver el mazo" while it is on */
    if (pi->activo) {
        caja(L, BT_X0, BT_Y0, bw, bh, sob ? 40 : 24, sob ? 90 : 60, sob ? 44 : 28, 255, VERDE);
        if (pi->ganado)
            snprintf(buf, sizeof buf, "%s", f2_traducir("Devolver el mazo"));
        else
            snprintf(buf, sizeof buf, "%s %d/3", f2_traducir("Devolver el mazo"), pi->usos);
        texto(L, (BT_X0 + BT_X1) * 0.5f, BT_Y0 + 9.f, buf, 28.f, RGB(200, 255, 200), 0, bw - 24.f);
    } else if (apagado) {
        caja(L, BT_X0, BT_Y0, bw, bh, 40, 32, 22, 255, RGB(100, 80, 50));
        snprintf(buf, sizeof buf, "%s 0/3", f2_traducir("Probar este Mazo"));
        texto(L, (BT_X0 + BT_X1) * 0.5f, BT_Y0 + 9.f, buf, 28.f, RGB(150, 130, 100), 0, bw - 24.f);
    } else {
        caja(L, BT_X0, BT_Y0, bw, bh, sob ? 96 : 60, sob ? 66 : 40, sob ? 22 : 14, 255, sob ? RGB(255, 245, 200) : BORDE);
        if (pi->ganado)
            snprintf(buf, sizeof buf, "%s", f2_traducir("Jugar con este mazo"));
        else
            snprintf(buf, sizeof buf, "%s %d/3", f2_traducir("Probar este Mazo"), pi->usos);
        texto(L, (BT_X0 + BT_X1) * 0.5f, BT_Y0 + 9.f, buf, 28.f, ORO, 0, bw - 24.f);
    }
    /* under the button: what is going on */
    if (pi->activo)
        texto(L, (BT_X0 + BT_X1) * 0.5f, BT_Y1 + 10.f,
              f2_traducir(pi->ganado ? "Tu mazo quedó guardado aparte" : "Solo para Duelos (no Torneos)"), 22.f, VERDE, 0,
              bw + 20.f);
    else if (!apagado && !pi->ganado)
        texto(L, (BT_X0 + BT_X1) * 0.5f, BT_Y1 + 10.f, f2_traducir("Solo para Duelos (no Torneos)"), 22.f, TAN, 0,
              bw + 20.f);
    else if (apagado && pi->total > 0) {
        int falta = pi->total - pi->tengo;      /* the nearer of the two (either earns it) */
        if (pi->ttotal > 0 && pi->ttotal - pi->ttengo < falta)
            falta = pi->ttotal - pi->ttengo;
        snprintf(buf, sizeof buf, f2_traducir("Faltan %d cartas para ganarlo"), falta);
        texto(L, (BT_X0 + BT_X1) * 0.5f, BT_Y1 + 10.f, buf, 22.f, RGB(240, 160, 110), 0, bw + 20.f);
    }
    /* the line under the record: the cards it gives that you own, and a bar;
     * 2026-11-25 (the user: two bars, Duelo first then Torneo, "no quiero que
     * rompas tanto la estetica"): both on this same line, Duelo on the left */
    {
        float x0 = MZ_X0 + 242.f;
        int i;
        for (i = 0; i < 2; i++) {
            wchar_t w[160];
            int h = (int)(24.f * s_s + 0.5f), te = i ? pi->ttengo : pi->tengo, to = i ? pi->ttotal : pi->total;
            float tw;
            if (to <= 0)
                continue;
            snprintf(buf, sizeof buf, f2_traducir(i ? "Torneo: %d de %d" : "Duelo: %d de %d"), te, to);
            wide(buf, w, 160);
            tw = (float)ft_ancho(w, h < 8 ? 8 : h) / s_s;
            if (tw > 300.f)
                tw = 300.f;
            texto_w(L, x0 + tw * 0.5f, PR_Y, w, 24.f, TAN, 0, 300.f);
            barra(L, x0 + tw + 16.f, PR_Y + 8.f, 190.f, 16.f, (float)te / (float)to);
            x0 += tw + 16.f + 190.f + 56.f;
        }
        (void)nom;
    }
    if (pi->ganado)                     /* the portrait box: 324..504 x 210..330 */
        sello_ganado(L, MZ_X0 + 34.f + 180.f - 14.f, MZ_Y0 + 30.f + 120.f - 14.f, 20.f);
}

/* the question before the loan */
static void pintar_pregunta(lienzo *L, const mazo_t *m, int sobre_que)
{
    static const COLORREF ORO = RGB(255, 225, 130), TAN = RGB(230, 195, 120), BORDE = RGB(232, 176, 74);
    char buf[160], nom[64];
    int i, si = sobre_que == CONF_SI, no = sobre_que == CONF_NO;
    float y = CF_Y0 + 116.f;
    const char *lineas[6];
    int n = 0;
    for (i = 0; i < L->w * L->h; i++) {
        unsigned char *p = L->p + 4 * (size_t)i;
        p[0] = (unsigned char)(p[0] * 110 / 255);
        p[1] = (unsigned char)(p[1] * 110 / 255);
        p[2] = (unsigned char)(p[2] * 110 / 255);
    }
    caja(L, CF_X0, CF_Y0, CF_X1 - CF_X0, CF_Y1 - CF_Y0, 28, 20, 10, 255, BORDE);
    marco_rect(L, CF_X0 + 14.f, CF_Y0 + 14.f, CF_X1 - CF_X0 - 28.f, CF_Y1 - CF_Y0 - 28.f, 2.f, RGB(140, 100, 40));
    nombre_rival(m->ch, nom, sizeof nom);
    snprintf(buf, sizeof buf, f2_traducir("¿Jugar con el mazo de %s?"), nom);
    texto(L, (CF_X0 + CF_X1) * 0.5f, CF_Y0 + 36.f, buf, 42.f, ORO, 1, CF_X1 - CF_X0 - 80.f);
    /* 2026-11-20 (the user: "resumi en pocas palabras ... solo lo importante") */
    lineas[n++] = f2_traducir("Solo para Duelos. Cada victoria gasta 1 de sus 3 usos.");
    lineas[n++] = f2_traducir("Con todas sus cartas es tuyo para siempre.");
    for (i = 0; i < n; i++, y += 44.f)
        texto(L, (CF_X0 + CF_X1) * 0.5f, y, lineas[i], 27.f, TAN, 0, CF_X1 - CF_X0 - 80.f);
    caja(L, CF_SI_X, CF_BY, CF_BW, CF_BH, si ? 96 : 60, si ? 66 : 40, si ? 22 : 14, 255, si ? RGB(255, 245, 200) : BORDE);
    texto(L, CF_SI_X + CF_BW * 0.5f, CF_BY + 12.f, f2_traducir("Aceptar"), 30.f, ORO, 0, CF_BW - 20.f);
    caja(L, CF_NO_X, CF_BY, CF_BW, CF_BH, no ? 70 : 40, no ? 48 : 26, no ? 20 : 12, 255,
         no ? RGB(255, 245, 200) : RGB(140, 100, 40));
    texto(L, CF_NO_X + CF_BW * 0.5f, CF_BY + 12.f, f2_traducir("Cancelar"), 30.f, TAN, 0, CF_BW - 20.f);
}

static void pintar_mazo(lienzo *L, const mazo_t *m, int sobre_que)
{
    static const COLORREF ORO = RGB(255, 225, 130), TAN = RGB(230, 195, 120), VERDE = RGB(110, 230, 120),
                          BORDE = RGB(232, 176, 74), APAGADO = RGB(90, 70, 40);
    char buf[240], nom[64];
    mlay o;
    int i, lado, hk = -1, hn = 0, hlado = 0;
    /* the page behind, darker (premultiplied: every channel scaled) */
    for (i = 0; i < L->w * L->h; i++) {
        unsigned char *p = L->p + 4 * (size_t)i;
        p[0] = (unsigned char)(p[0] * 90 / 255);
        p[1] = (unsigned char)(p[1] * 90 / 255);
        p[2] = (unsigned char)(p[2] * 90 / 255);
    }
    caja(L, MZ_X0, MZ_Y0, MZ_X1 - MZ_X0, MZ_Y1 - MZ_Y0, 28, 20, 10, 250, BORDE);
    marco_rect(L, MZ_X0 + 14.f, MZ_Y0 + 14.f, MZ_X1 - MZ_X0 - 28.f, MZ_Y1 - MZ_Y0 - 28.f, 2.f, RGB(140, 100, 40));
    /* the header: portrait, name, record */
    caja(L, MZ_X0 + 34.f, MZ_Y0 + 30.f, 180.f, 120.f, 18, 12, 6, 255, BORDE);
    retrato(L, m->ch, MZ_X0 + 40.f, MZ_Y0 + 36.f, 168.f, 108.f);
    nombre_rival(m->ch, nom, sizeof nom);
    snprintf(buf, sizeof buf, f2_traducir("Mazo de %s"), nom);
    texto_izq(L, MZ_X0 + 240.f, MZ_Y0 + 34.f, buf, 46.f, ORO, 1,
              m->pi.puede ? BT_X0 - 24.f - (MZ_X0 + 240.f) : MZ_X1 - MZ_X0 - 360.f);
    snprintf(buf, sizeof buf, f2_traducir("%d ganados / %d perdidos"), m->g, m->p);
    {
        size_t l = strlen(buf);
        snprintf(buf + l, sizeof buf - l, "   \xc2\xb7   %s", f2_traducir("Arma 40 cartas por duelo"));
    }
    texto_izq(L, MZ_X0 + 242.f, MZ_Y0 + 96.f, buf, 26.f, TAN, 0,
              m->pi.puede ? BT_X0 - 24.f - (MZ_X0 + 242.f) : MZ_X1 - MZ_X0 - 360.f);
    circulo(L, MZ_CX, MZ_CY, MZ_CR, sobre_que == MAZO_X ? RGB(130, 36, 26) : RGB(80, 22, 16), RGB(240, 95, 80), 3.f);
    texto(L, MZ_CX, MZ_CY - 21.f, "X", 34.f, RGB(255, 220, 200), 0, 0.f);
    if (m->pi.puede)
        pintar_prestado(L, m, sobre_que);
    if (!m->nf && !m->na) {
        texto(L, (MZ_X0 + MZ_X1) * 0.5f, 560.f, f2_traducir("Sin datos de su mazo"), 32.f, TAN, 0, 900.f);
        return;
    }
    mazo_layout(m, &o);
    if (m->nf) {
        float w = m->na ? o.xa - o.xf - 40.f : MZ_X1 - 40.f - o.xf;
        snprintf(buf, sizeof buf, f2_traducir("Siempre en su mazo (%d cartas)"), m->totf);
        texto_izq(L, o.xf, MZ_GY0 - 54.f, buf, 30.f, ORO, 1, w);
    }
    if (m->na) {
        texto_izq(L, o.xa, MZ_GY0 - 54.f, f2_traducir("Puede sumar al azar"), 30.f, ORO, 1, MZ_X1 - 40.f - o.xa);
        if (m->nf)
            caja(L, o.xa - 26.f, MZ_GY0 - 50.f, 2.f, MZ_GY1 - MZ_GY0 + 40.f, 140, 100, 40, 255, RGB(140, 100, 40));
    }
    for (lado = 0; lado < 2; lado++)
        for (i = 0; i < (lado ? m->na : m->nf); i++) {
            const mcarta *c = lado ? &m->a[i] : &m->f[i];
            uint32_t id = MEM16(T_IMAGEN + 2u * (uint32_t)c->k);
            int tiene = MEM8(COLECCION + (uint32_t)c->k) != 0, sob = sobre_que == MAZO_CARTA + lado * MAZO_MAX + i;
            float x, y;
            mazo_celda(&o, lado, i, &x, &y);
            const img *ci_img = carta_img(id);
            if (ci_img)
                pegar(L, ci_img, 0, 0, ci_img->w, ci_img->h, X(x), Y(y), o.tw * s_s, o.th * s_s, 255);
            else if (s_minis.p && id >= 4000u && (int)id - 4000 < (s_minis.w / 35) * (s_minis.h / 51)) {
                int ci = (int)id - 4000;
                pegar(L, &s_minis, (ci % 40) * 35, (ci / 40) * 51, 35, 51, X(x), Y(y), o.tw * s_s, o.th * s_s, 255);
            }
            marco_rect(L, x - 2.f, y - 2.f, o.tw + 4.f, o.th + 4.f, sob ? 4.f : 2.f,
                       sob ? RGB(255, 245, 200) : tiene ? BORDE : APAGADO);
            if (c->n > 1) {
                float bw = o.th * 0.36f, bh = o.th * 0.27f;
                caja(L, x + o.tw - bw + 4.f, y + o.th - bh + 4.f, bw, bh, 20, 14, 6, 255, ORO);
                snprintf(buf, sizeof buf, "x%d", c->n);
                texto(L, x + o.tw - bw * 0.5f + 4.f, y + o.th - bh + 4.f + bh * 0.08f, buf, bh * 0.78f, ORO, 0, bw);
            }
            /* 2026-11-19: the tick to the bottom left corner (the top left
             * is the limited circle's, over the name's start) */
            if (tiene) {
                float r = o.th * 0.12f, ty = y + o.th - 4.f;
                circulo(L, x + 4.f, ty, r, RGB(20, 60, 25), VERDE, 2.f);
                linea(L, x + 4.f - r * 0.5f, ty, x + 4.f - r * 0.1f, ty + r * 0.4f, r * 0.28f, VERDE);
                linea(L, x + 4.f - r * 0.1f, ty + r * 0.4f, x + 4.f + r * 0.55f, ty - r * 0.45f, r * 0.28f, VERDE);
            }
            {
                int nl = limite_de(c->k);
                if (nl) {
                    float rr = o.tw * 0.15f;
                    insignia(L, x + 2.f + rr, y + 2.f + rr, rr, nl);
                }
            }
            if (sob) {
                hk = c->k;
                hn = c->n;
                hlado = lado;
            }
        }
    /* the card under the pointer */
    caja(L, MZ_X0 + 34.f, MZ_INFO_Y, MZ_X1 - MZ_X0 - 68.f, 60.f, 18, 12, 6, 255, BORDE);
    if (hk > 0) {
        uint32_t w = MEM32(T_DATOS + 4u * (uint32_t)hk), tp = (w >> 20) & 31u;
        int copias = MEM8(COLECCION + (uint32_t)hk);
        char nombre[96], tipo[64], dueno[64];
        size_t l;
        ygo_texto_juego(849u + (uint32_t)hk, nombre, sizeof nombre);
        if (tp == 22u)
            snprintf(tipo, sizeof tipo, "%s", f2_traducir("Carta mágica"));
        else if (tp == 21u)
            snprintf(tipo, sizeof tipo, "%s", f2_traducir("Carta de trampa"));
        else
            snprintf(tipo, sizeof tipo, "ATK %u  DEF %u", ((w >> 9) & 511u) * 10u, (w & 511u) * 10u);
        if (copias)
            snprintf(dueno, sizeof dueno, f2_traducir("La tenés (%d)"), copias);
        else
            snprintf(dueno, sizeof dueno, "%s", f2_traducir("No la tenés"));
        snprintf(buf, sizeof buf, "%s   \xc2\xb7   %s   \xc2\xb7   ", nombre, tipo);
        l = strlen(buf);
        snprintf(buf + l, sizeof buf - l, f2_traducir(hlado ? "x%d al azar" : "x%d en su mazo"), hn);
        l = strlen(buf);
        snprintf(buf + l, sizeof buf - l, "   \xc2\xb7   %s", dueno);
        texto_izq(L, MZ_X0 + 60.f, MZ_INFO_Y + 12.f, buf, 28.f, copias ? TAN : RGB(200, 170, 120), 0,
                  MZ_X1 - MZ_X0 - 120.f);
    } else
        texto(L, (MZ_X0 + MZ_X1) * 0.5f, MZ_INFO_Y + 12.f, f2_traducir("Hacé clic en una carta para verla en grande"),
              28.f, RGB(200, 170, 120), 0, MZ_X1 - MZ_X0 - 120.f);

    /* 2026-11-19 (the user: "cuando le hagas click a una, que se vea en
     * grande"): the clicked card over everything, the panel darker behind;
     * any click closes it */
    if (m->grande > 0) {
        uint32_t id = MEM16(T_IMAGEN + 2u * (uint32_t)m->grande);
        const img *ci = carta_grande_img(id);
        for (i = 0; i < L->w * L->h; i++) {
            unsigned char *p = L->p + 4 * (size_t)i;
            p[0] = (unsigned char)(p[0] * 110 / 255);
            p[1] = (unsigned char)(p[1] * 110 / 255);
            p[2] = (unsigned char)(p[2] * 110 / 255);
        }
        if (ci && ci->w > 0 && ci->h > 0) {
            float h = 760.f, w = h * (float)ci->w / (float)ci->h, x = REF_CX - w * 0.5f, y = 110.f;
            int nl = limite_de(m->grande);
            caja(L, x - 8.f, y - 8.f, w + 16.f, h + 16.f, 20, 14, 6, 255, BORDE);
            pegar(L, ci, 0, 0, ci->w, ci->h, X(x), Y(y), w * s_s, h * s_s, 255);
            if (nl) {
                float r = 9.f * w / 128.f;
                insignia(L, x + r, y + r, r, nl);
            }
        }
        texto(L, REF_CX, 900.f, f2_traducir("Clic para cerrar"), 30.f, TAN, 1, 600.f);
    }
    if (m->conf && m->grande <= 0)
        pintar_pregunta(L, m, sobre_que);
}

/* 2026-11-19: the page of the opponents, the game's 28 characters (their
 * portraits and names, as the statistics' rivals); a click opens its deck */
/* 2026-11-20 (the user: "aparezcan iluminados los que ya desbloqueaste para
 * duelos ... los que te falten saldran oscuros (no tanto) y no podras ver sus
 * decks ni pedirlos prestados"): the game's opponent select (sub_000FC370)
 * lists character ch (0..26, 31..33) when u32 0x651958 + 4 ch != 0 */
static int desbloqueado(int ch)
{
    if ((ch >= 0 && ch <= 26) || (ch >= 31 && ch <= 33))
        return MEM32(0x00651958u + 4u * (uint32_t)ch) != 0u;
    return 0;
}

/* a box of the canvas darker (ref px; premultiplied: every channel scaled) */
static void oscurecer(lienzo *L, float x, float y, float w, float h, unsigned f)
{
    int X0 = (int)X(x), Y0 = (int)Y(y), X1 = (int)X(x + w), Y1 = (int)Y(y + h), i, j;
    for (j = Y0; j < Y1; j++)
        for (i = X0; i < X1; i++)
            if (i >= 0 && j >= 0 && i < L->w && j < L->h) {
                unsigned char *p = L->p + 4 * ((size_t)j * L->w + i);
                p[0] = (unsigned char)(p[0] * f / 255);
                p[1] = (unsigned char)(p[1] * f / 255);
                p[2] = (unsigned char)(p[2] * f / 255);
            }
}

static void pintar_rivales(lienzo *L)
{
    static const COLORREF ORO = RGB(255, 225, 130), TAN = RGB(230, 195, 120), BORDE = RGB(232, 176, 74);
    char nom[64];
    int ch, n;
    texto(L, REF_CX, 82.f, f2_traducir("Decks de los Oponentes"), 40.f, RGB(60, 25, 0), 0, 420.f);
    texto(L, REF_CX, 290.f, f2_traducir("Elegí un rival para ver su Deck (los oscuros todavía no los desbloqueaste en Duelo)"),
          30.f, TAN, 0, 1300.f);
    for (n = 0; n < NRIVALES; n++) {
        float x = RG_X0 + (float)(n % RG_COLS) * RG_GX, y = RG_Y0 + (float)(n / RG_COLS) * RG_GY;
        int so, ok;
        ch = k_rivales[n];
        ok = desbloqueado(ch);
        so = ok && s_p_mazo.ch < 0 && s_p_sobre == RETRATO0 + n;
        caja(L, x - 6.f, y - 6.f, RG_W + 12.f, RG_H + 12.f, 18, 12, 6, 255,
             so ? RGB(255, 245, 200) : ok ? BORDE : RGB(110, 85, 45));
        if (so)
            marco_rect(L, x - 6.f, y - 6.f, RG_W + 12.f, RG_H + 12.f, 4.f, RGB(255, 245, 200));
        retrato(L, ch, x, y, RG_W, RG_H);
        if (!ok)
            oscurecer(L, x, y, RG_W, RG_H, 105);   /* darker, still recognisable */
        else if (ygo_prestado_ganado(ch))
            sello_ganado(L, x + RG_W - 14.f, y + RG_H - 14.f, 18.f);
        nombre_rival(ch, nom, sizeof nom);
        texto(L, x + RG_W * 0.5f, y + RG_H + 12.f, nom, 24.f, so ? ORO : ok ? TAN : RGB(125, 105, 75), 1, RG_GX - 6.f);
    }
    if (s_p_mazo.ch >= 0)
        pintar_mazo(L, &s_p_mazo, s_p_sobre);
}

static void pintar_estadisticas(lienzo *L)
{
    static const COLORREF ORO = RGB(255, 225, 130), TAN = RGB(230, 195, 120), VERDE = RGB(110, 230, 120),
                          ROJO = RGB(240, 95, 80), BORDE = RGB(232, 176, 74);
    ygo_stats st;
    char buf[160], nom[64];
    int i;
    ygo_estadisticas(&st);
    texto(L, REF_CX, 82.f, f2_traducir("Estadísticas"), 40.f, RGB(60, 25, 0), 0, 420.f);

    /* left: the record */
    texto(L, 608.f, 300.f, f2_traducir("Récord"), 30.f, TAN, 0, 400.f);
    for (i = 0; i < 3; i++) {
        static const char *const k_lab[3] = { "Duelos", "Victorias", "Derrotas" };
        int n = i == 0 ? st.duelos : i == 1 ? st.ganados : st.perdidos;
        float x = 318.f + (float)i * 200.f;
        caja(L, x, 341.f, 180.f, 150.f, 18, 12, 6, 215, BORDE);
        texto(L, x + 90.f, 356.f, f2_traducir(k_lab[i]), 26.f, TAN, 0, 170.f);
        snprintf(buf, sizeof buf, "%d", n);
        texto(L, x + 90.f, 401.f, buf, 60.f, i == 0 ? ORO : i == 1 ? VERDE : ROJO, 1, 170.f);
    }
    snprintf(buf, sizeof buf, f2_traducir("Ganadas: %d %%"), st.duelos ? (st.ganados * 100 + st.duelos / 2) / st.duelos : 0);
    texto(L, 608.f, 526.f, buf, 32.f, ORO, 1, 520.f);
    barra(L, 348.f, 574.f, 520.f, 22.f, st.duelos ? (float)st.ganados / (float)st.duelos : 0.f);
    snprintf(buf, sizeof buf, f2_traducir("Racha actual: %d"), st.racha);
    texto(L, 608.f, 648.f, buf, 30.f, TAN, 0, 520.f);
    snprintf(buf, sizeof buf, f2_traducir("Mejor racha: %d"), st.mejor_racha);
    texto(L, 608.f, 703.f, buf, 30.f, TAN, 0, 520.f);
    /* 2026-11-16 (the user): no "Victorias con Exodia" line */

    /* left bottom: the hardest opponent */
    texto(L, 608.f, 786.f, f2_traducir("Rival más difícil"), 30.f, TAN, 0, 520.f);
    caja(L, 348.f, 831.f, 520.f, 110.f, 18, 12, 6, 215, s_p_sobre == RIVAL_DIF ? RGB(255, 245, 200) : BORDE);
    if (st.dificil_ch >= 0) {
        retrato(L, st.dificil_ch, 356.f, 838.f, 144.f, 96.f);
        ygo_estadisticas_nombre(st.dificil_ch, nom, sizeof nom);
        texto_izq(L, 518.f, 848.f, nom, 34.f, ORO, 1, 340.f);
        snprintf(buf, sizeof buf, f2_traducir("%d ganados / %d perdidos"), st.dificil_g, st.dificil_p);
        texto_izq(L, 518.f, 893.f, buf, 26.f, TAN, 0, 340.f);
    } else
        texto(L, 608.f, 867.f, f2_traducir("Todavía ninguno"), 30.f, TAN, 0, 500.f);

    /* right: the last duels, newest first */
    texto(L, 1298.f, 300.f, f2_traducir("Últimos duelos"), 30.f, TAN, 0, 520.f);
    if (!st.n_ult)
        texto(L, 1298.f, 600.f, f2_traducir("Todavía no jugaste duelos"), 30.f, TAN, 0, 600.f);
    for (i = 0; i < st.n_ult; i++) {
        const ygo_duelo_vis *u = &st.ult[i];
        float y = 341.f + (float)i * 120.f;
        COLORREF c = u->res == 'V' ? VERDE : u->res == 'D' ? ROJO : TAN;
        caja(L, 978.f, y, 640.f, 108.f, 18, 12, 6, 215, s_p_sobre == RIVAL0 + i ? RGB(255, 245, 200) : BORDE);
        retrato(L, u->ch, 986.f, y + 6.f, 144.f, 96.f);
        ygo_estadisticas_nombre(u->ch, nom, sizeof nom);
        texto_izq(L, 1148.f, y + 14.f, nom, 34.f, ORO, 1, 290.f);
        snprintf(buf, sizeof buf, "%02d/%02d", u->dia, u->mes);
        texto_izq(L, 1148.f, y + 60.f, buf, 24.f, TAN, 0, 0.f);
        if (u->res == 'V')
            caja(L, 1448.f, y + 30.f, 155.f, 48.f, 20, 60, 25, 255, c);
        else if (u->res == 'D')
            caja(L, 1448.f, y + 30.f, 155.f, 48.f, 80, 22, 16, 255, c);
        else
            caja(L, 1448.f, y + 30.f, 155.f, 48.f, 50, 40, 20, 255, c);
        texto(L, 1525.f, y + 40.f, f2_traducir(u->res == 'V' ? "Victoria" : u->res == 'D' ? "Derrota" : "Empate"),
              26.f, c, 0, 140.f);
    }
    if (s_p_mazo.ch >= 0)
        pintar_mazo(L, &s_p_mazo, s_p_sobre);
}

/* the banner's arrows, lit under the pointer (both pages) */
static void flechas_pagina(lienzo *L, int sobre_que)
{
    int i;
    (void)L;
    (void)sobre_que;
    (void)i;
    return;                             /* the triangles' own windows show the hover now */
    for (i = 0; i < 2; i++)
        if (sobre_que == PAG_ANT + i) {
            const img *f = &s_flecha_luz[i];          /* left: previous, right: next */
            float k = 0.6f;
            if (f->p)
                pegar_todo(L, f, X(k_pag[i][0] - FLECHA_W * k / 2.f), Y(k_pag[i][1] - FLECHA_H * k / 2.f),
                           FLECHA_W * k * s_s, FLECHA_H * k * s_s);
        }
}

static void pintar_panel(lienzo *L, int sobre_que, int editando, int cursor)
{
    int i, masc = ygo_mascota_tuya_get(), fu = ygo_funda_get(), mr = ygo_funda_marco_get();
    char buf[128];
    /* 2026-11-16 (the user: "un fondo completo que tape todo"): the game's
     * menu background (fondo_completo.jpg, 640 x 480) covering the window,
     * its middle rows kept; without it, the game darkened as before */
    if (s_fondo_completo.p) {
        float sh = (float)s_fondo_completo.w * (float)L->h / (float)L->w;
        int sy;
        if (sh > (float)s_fondo_completo.h)
            sh = (float)s_fondo_completo.h;
        sy = (int)(((float)s_fondo_completo.h - sh) * 0.5f);
        pegar(L, &s_fondo_completo, 0, sy, s_fondo_completo.w, (int)sh, 0.f, 0.f, (float)L->w, (float)L->h, 255);
    } else
        for (i = 0; i < L->w * L->h; i++) {
            unsigned char *p = L->p + 4 * (size_t)i;
            p[0] = 0; p[1] = 3; p[2] = 7; p[3] = 175;
        }
    /* the panel, scaled once per size; page 2: the statistics */
    if (s_p_pagina != 0 && s_fondo2.p) {
        pegar_panel(L, &s_fondo2, &s_fondo2_esc, &s_fondo2_s);
        if (s_p_pagina == PAG_RIVALES)
            pintar_rivales(L);
        else
            pintar_estadisticas(L);
        flechas_pagina(L, sobre_que);
        return;
    }
    pegar_panel(L, &s_fondo, &s_fondo_esc, &s_fondo_s);
    /* 2026-11-19 (the user: on the other pages "se sigue viendo donde va el
     * nombre del jugador pero vacio"): its plate is drawn here, not in fondo.png */
    if (s_placa_nombre.p)
        pegar_todo(L, &s_placa_nombre, X(NOMBRE_CX - NOMBRE_W / 2.f - 4.f), Y(NOMBRE_Y - 4.f),
                   (float)s_placa_nombre.w * s_s, (float)s_placa_nombre.h * s_s);
    /* the title in the banner's plate (2026-11-16, the user: "Perfil se sale
     * un poco de su recuadro"): smaller, on the plate's middle (row 100) */
    texto(L, REF_CX, 82.f, f2_traducir("Perfil"), 40.f, RGB(60, 25, 0), 0, 420.f);
    texto(L, TCX, 300.f, f2_traducir("Tapetes"), 30.f, RGB(230, 195, 120), 0, 400.f);
    texto(L, MCX, 300.f, f2_traducir("Mascota"), 30.f, RGB(230, 195, 120), 0, 400.f);
    texto(L, FCX, 300.f, f2_traducir("Funda"), 30.f, RGB(230, 195, 120), 0, 400.f);

    /* 2026-11-16 (the user: "la foto no nos sirve ... capaz podriamos poner
     * Tapetes"): the box shows your tapete (Mods-Extras\tapetes\<name>_vista.jpg,
     * tools/tapetes.py), its name at the bottom; arrows on its sides */
    {
        int tp = ygo_tapete_get();
        if (s_tapete_de != tp) {
            char n[80];
            /* 2026-11-19: fundas.c's list (it grew to 70) */
            s_tapete_de = tp;
            liberar(&s_tapete_img);
            snprintf(n, sizeof n, "..\\tapetes\\%s_vista.jpg", ygo_arte_archivo(tp));
            leer_recurso(n, &s_tapete_img);
        }
        if (s_tapete_img.p)
            pegar_todo(L, &s_tapete_img, X(FOTO_X), Y(FOTO_Y), FOTO_W * s_s, FOTO_H * s_s);
        /* 2026-11-19: its name in its plate and its number, as the others */
        texto(L, TCX, PLACA_Y + 4.f, ygo_perfil_tapete_nombre(tp), 36.f, RGB(60, 25, 0), 0, 220.f);
        snprintf(buf, sizeof buf, "%d / %d", tp, NTAPETES - 1);
        texto(L, TCX, 820.f, buf, 30.f, RGB(230, 195, 120), 0, 0.f);
    }
    if (sobre_que == FOTO && s_foto_luz.p)
        pegar_todo(L, &s_foto_luz, X(FOTO_X - 30.f), Y(FOTO_Y - 30.f), (FOTO_W + 60.f) * s_s, (FOTO_H + 60.f) * s_s);
    if (sobre_que == MASC_FOTO && s_masc_luz.p)
        pegar_todo(L, &s_masc_luz, X(MASC_X - 30.f), Y(MASC_Y - 30.f), (MASC_L + 60.f) * s_s, (MASC_L + 60.f) * s_s);
    /* the sleeves': under the deck (drawn below, over it) */
    if (sobre_que == FUNDA_FOTO && s_funda_luz.p)
        pegar_todo(L, &s_funda_luz, X(CARTA_X - 30.f), Y(CARTA_Y - 24.f - 30.f), 280.f * s_s, 376.f * s_s);

    /* the name */
    if (sobre_que == NOMBRE || editando)
        marco_rect(L, NOMBRE_CX - NOMBRE_W / 2.f - 2.f, NOMBRE_Y - 2.f, NOMBRE_W + 4.f, NOMBRE_H + 4.f, 3.f,
                   RGB(255, 240, 160));
    {
        wchar_t w[NOMBRE_MAX + 3];
        if (editando)
            _snwprintf(w, NOMBRE_MAX + 3, L"%ls%ls", s_p_edit, cursor ? L"|" : L" ");
        else
            wcscpy(w, s_p_nombre);
        w[NOMBRE_MAX + 2] = 0;
        texto_w(L, NOMBRE_CX, NOMBRE_Y + 5.f, w, 40.f, RGB(60, 25, 0), 0, NOMBRE_W - 24.f);
    }
    if (editando) {
        texto(L, NOMBRE_CX, 720.f, f2_traducir("Escribí tu nombre"), 30.f, RGB(255, 225, 130), 0, 400.f);
        texto(L, NOMBRE_CX, 755.f, f2_traducir("y apretá Enter"), 30.f, RGB(255, 225, 130), 0, 400.f);
    }
    {
        /* 2026-11-16 (the user: "pone cuantas cartas se tiene y cuantas en
         * total nada mas") */
        int d, total;
        contar_cartas(&d, &total);
        snprintf(buf, sizeof buf, f2_traducir("Cartas: %d / %d"), d, total);
        /* 2026-11-19: at the panel's bottom, in the middle */
        texto(L, REF_CX, 868.f, buf, 34.f, RGB(255, 220, 110), 1, 400.f);
        barra(L, REF_CX - 220.f, 922.f, 440.f, 18.f, total > 0 ? (float)d / (float)total : 0.f);
    }

    /* the mascot's name (the model: its own window) */
    texto(L, MCX, PLACA_Y + 4.f, ygo_perfil_mascota_nombre(masc), 36.f, RGB(60, 25, 0), 0, 300.f);
    snprintf(buf, sizeof buf, "%d / %d", masc + 1, NMASCOTAS);
    texto(L, MCX, 820.f, buf, 30.f, RGB(230, 195, 120), 0, 0.f);

    /* the sleeve: a deck, its card a little up and right each */
    if (s_funda_de != fu * 8 + mr) {
        char n[32];
        s_funda_de = fu * 8 + mr;
        if (fu <= 0)
            snprintf(n, sizeof n, "fundas\\00_0.jpg");
        else
            snprintf(n, sizeof n, "fundas\\%02d_%d.jpg", fu, mr);
        leer_recurso(n, &s_funda_img);
    }
    if (s_funda_img.p)
        for (i = 4; i >= 0; i--) {
            float x = CARTA_X + (float)i * 5.f, y = CARTA_Y - (float)i * 6.f;
            int X0 = (int)X(x) - 1, Y0 = (int)Y(y) - 1, X1 = (int)X(x + CARTA_W) + 1, Y1 = (int)Y(y + CARTA_H) + 1, a, b;
            for (b = Y0; b < Y1; b++)
                for (a = X0; a < X1; a++)
                    if (a >= 0 && b >= 0 && a < L->w && b < L->h)
                        sobre(L->p + 4 * ((size_t)b * L->w + a), 0, 4, 10, 255);
            pegar_todo(L, &s_funda_img, X(x), Y(y), CARTA_W * s_s, CARTA_H * s_s);
        }
    texto(L, FCX, PLACA_Y + 4.f, ygo_perfil_funda_nombre(fu), 36.f, RGB(60, 25, 0), 0, 220.f);
    {
        static const COLORREF k_col[NMARCOS] = { RGB(236, 175, 30), RGB(200, 200, 210), RGB(40, 40, 50),
                                                 RGB(190, 120, 30), RGB(60, 230, 230) };
        (void)k_col;                    /* 2026-11-20: no dots; a click on the deck changes the frame */
        /* 2026-11-19 (the user): no "Marco" word; the number as the mascot's,
         * the Clásica being 0 */
        snprintf(buf, sizeof buf, "%d / %d", fu, NFUNDAS - 1);
        texto(L, FCX, 820.f, buf, 30.f, RGB(230, 195, 120), 0, 0.f);
    }

    /* (2026-11-19: the six arrows are windows of their own, perfil_thread) */
    /* 2026-11-16 (the user: "elimina el tutorial del click"): no line about the clicks */
    flechas_pagina(L, sobre_que);
}

/* the sleeve's deck (5 cards, each a little up and right) in a lienzo of
 * (CARTA_W + 24) x (CARTA_H + 28) ref px, its corner at CARTA_X - 2, CARTA_Y - 26;
 * the top card's rainbow foil at t (s) */
#define MZ_W (CARTA_W + 24.f)
#define MZ_H (CARTA_H + 28.f)
static void pintar_baraja(lienzo *L, float t)
{
    int i;
    memset(L->p, 0, (size_t)L->w * L->h * 4);
    if (!s_funda_img.p)
        return;
    for (i = 4; i >= 0; i--) {
        float x = (2.f + (float)i * 5.f) * s_s, y = (26.f - (float)i * 6.f) * s_s;
        int X0 = (int)x - 1, Y0 = (int)y - 1, X1 = (int)(x + CARTA_W * s_s) + 1, Y1 = (int)(y + CARTA_H * s_s) + 1, a, b;
        for (b = Y0; b < Y1; b++)
            for (a = X0; a < X1; a++)
                if (a >= 0 && b >= 0 && a < L->w && b < L->h) {
                    unsigned char *q = L->p + 4 * ((size_t)b * L->w + a);
                    q[0] = 0; q[1] = 4; q[2] = 10; q[3] = 255;
                }
        pegar_todo(L, &s_funda_img, x, y, CARTA_W * s_s, CARTA_H * s_s);
    }
    {
        /* the foil: a band of rainbow across the top card, every 2 s */
        float x0 = 2.f * s_s, y0 = 26.f * s_s, cw = CARTA_W * s_s, ch = CARTA_H * s_s;
        float pos = fmodf(t, 2.0f) / 2.0f * 2.2f - 0.5f;
        int a, b;
        for (b = (int)y0; b < (int)(y0 + ch) && b < L->h; b++)
            for (a = (int)x0; a < (int)(x0 + cw) && a < L->w; a++) {
                float u = ((float)a - x0) / cw + ((float)b - y0) / ch * 0.6f, d = (u - pos) / 0.16f, band, hue;
                unsigned char *q;
                int c;
                if (d > 3.f || d < -3.f)
                    continue;
                band = expf(-d * d) * 0.55f * 255.f;
                hue = u * 3.f + t;
                hue -= floorf(hue);
                q = L->p + 4 * ((size_t)b * L->w + a);
                for (c = 0; c < 3; c++) {
                    float v = q[2 - c] + (0.5f + 0.5f * cosf(6.2832f * (hue + 0.33f * c))) * band;
                    q[2 - c] = (unsigned char)(v > 255.f ? 255.f : v);
                }
            }
    }
}

static const GUID k_8bpp = { 0x6fddc324, 0x4e03, 0x4bfe, { 0xb1, 0x85, 0x3d, 0x77, 0x76, 0x8d, 0xc9, 0x04 } };

static void soltar_tira(tira_t *t)
{
    free(t->idx);
    t->idx = NULL;
    t->w = t->h = 0;
}

/* Mods-Extras\perfil\mascotas\II.png (8 bits with its palette) and II.txt */
static int leer_tira(int i, tira_t *t)
{
    wchar_t p[MAX_PATH], *barra;
    IWICBitmapDecoder *dec = NULL;
    IWICBitmapFrameDecode *fr = NULL;
    IWICPalette *pal = NULL;
    WICPixelFormatGUID fmt;
    WICColor col[256];
    UINT w = 0, h = 0, n = 0, k;
    int ok = 0;
    DWORD len = GetModuleFileNameW(NULL, p, MAX_PATH);
    soltar_tira(t);
    t->ms = FRAME_MS;
    if (!len || len >= MAX_PATH - 56 || !(barra = wcsrchr(p, L'\\')))
        return 0;
    _snwprintf(barra, MAX_PATH - (barra - p), L"%ls\\perfil\\mascotas\\%02d.txt", ygo_mods_w(), i);
    {
        FILE *f = _wfopen(p, L"rb");
        if (f) {
            float v = 0.f;
            if (fscanf(f, "%f", &v) == 1 && v >= 5.f && v <= 200.f)
                t->ms = v;
            fclose(f);
        }
    }
    _snwprintf(barra, MAX_PATH - (barra - p), L"%ls\\perfil\\mascotas\\%02d.png", ygo_mods_w(), i);
    if (!s_wic) {
        if (FAILED(CoCreateInstance(&k_clsid_wic, NULL, CLSCTX_INPROC_SERVER, &k_iid_wic, (void **)&s_wic)))
            s_wic = NULL;
    }
    if (!s_wic
            || FAILED(IWICImagingFactory_CreateDecoderFromFilename(s_wic, p, NULL, GENERIC_READ,
                                                                   WICDecodeMetadataCacheOnDemand, &dec))
            || FAILED(IWICBitmapDecoder_GetFrame(dec, 0, &fr))
            || FAILED(IWICBitmapFrameDecode_GetPixelFormat(fr, &fmt)) || memcmp(&fmt, &k_8bpp, sizeof fmt)
            || FAILED(IWICBitmapFrameDecode_GetSize(fr, &w, &h)) || w != LADO || h < LADO
            || FAILED(IWICImagingFactory_CreatePalette(s_wic, &pal))
            || FAILED(IWICBitmapFrameDecode_CopyPalette(fr, pal))
            || FAILED(IWICPalette_GetColors(pal, 256, col, &n)))
        goto fin;
    memset(t->pal, 0, sizeof t->pal);
    for (k = 0; k < n && k < 256; k++) {
        unsigned a = col[k] >> 24, r = (col[k] >> 16) & 255, g = (col[k] >> 8) & 255, b = col[k] & 255;
        t->pal[k] = (a << 24) | ((r * a / 255) << 16) | ((g * a / 255) << 8) | (b * a / 255);
    }
    t->idx = (unsigned char *)malloc((size_t)w * h);
    if (t->idx && SUCCEEDED(IWICBitmapFrameDecode_CopyPixels(fr, NULL, w, w * h, t->idx))) {
        t->w = (int)w;
        t->h = (int)h;
        ok = 1;
    } else
        soltar_tira(t);
fin:
    if (pal)
        IWICPalette_Release(pal);
    if (fr)
        IWICBitmapFrameDecode_Release(fr);
    if (dec)
        IWICBitmapDecoder_Release(dec);
    if (!ok)
        fprintf(stderr, "[PERFIL] no se pudo leer %ls\n", p);
    return ok;
}

/* the mascot's frame f into its window's canvas, bilinear */
static void pintar_mascota(lienzo *L, int f)
{
    int x, y;
    float k = (float)LADO / (float)L->w;
    memset(L->p, 0, (size_t)L->w * L->h * 4);
    if (!s_tira.idx)
        return;
    for (y = 0; y < L->h; y++) {
        float v = ((float)y + 0.5f) * k - 0.5f;
        int vi = (int)floorf(v);
        float fv = v - (float)vi;
        for (x = 0; x < L->w; x++) {
            float u = ((float)x + 0.5f) * k - 0.5f, c[4] = { 0, 0, 0, 0 };
            int ui = (int)floorf(u), q;
            float fu = u - (float)ui;
            unsigned char *d;
            for (q = 0; q < 4; q++) {
                int xx = ui + (q & 1), yy = vi + (q >> 1);
                float wgt = ((q & 1) ? fu : 1.f - fu) * ((q >> 1) ? fv : 1.f - fv);
                unsigned px;
                if (xx < 0) xx = 0;
                if (yy < 0) yy = 0;
                if (xx >= LADO) xx = LADO - 1;
                if (yy >= LADO) yy = LADO - 1;
                px = s_tira.pal[s_tira.idx[(size_t)(f * LADO + yy) * LADO + xx]];
                c[0] += wgt * (float)(px & 255);
                c[1] += wgt * (float)((px >> 8) & 255);
                c[2] += wgt * (float)((px >> 16) & 255);
                c[3] += wgt * (float)(px >> 24);
            }
            d = L->p + 4 * ((size_t)y * L->w + x);
            d[0] = (unsigned char)(c[0] + 0.5f);
            d[1] = (unsigned char)(c[1] + 0.5f);
            d[2] = (unsigned char)(c[2] + 0.5f);
            d[3] = (unsigned char)(c[3] + 0.5f);
        }
    }
}

/* ---- the thread ----------------------------------------------------------- */
static DWORD WINAPI perfil_thread(LPVOID unused)
{
    WNDCLASSA wc;
    HDC screen;
    static ventana vs;                  /* the exit button (2026-11-20) */
    static ventana vc;                  /* the banner saying Perfil (2026-11-20) */
    static ventana vu;                  /* the chosen icon unlit, meanwhile */
    static ventana vb, vp, vm, vd, vf[2], vfd[6], vt, vk;   /* vt: the tapete with its light; vk: the deck shining */
    static unsigned char *tfx_base, *tfx_out;
    static int tfx_de = -1, tfx_w, tfx_h;
    int fd_de[6] = { -1, -1, -1, -1, -1, -1 };
    ULONGLONG pag_clic[2] = { 0, 0 };
    int flecha_de[2] = { -1, -1 };
    float puertas_de = 0.f;
    int puertas_w = 0, puertas_h = 0;
    LONG pintado = -1;
    int boton_luz_de = -1, frame = -1;
    ULONGLONG boton_desde = 0;
    BYTE boton_alfa = 255, boton_alfa_de = 255;
    ULONGLONG onda_desde = 0;
    int sol_cuadro_de = -1;
    float onda_de = 2.f, rendija_de = 1.f, rendija = 1.f;
    float boton_s = 0.f;
    ULONGLONG desde = 0, parpadeo = 0;

    (void)unused;
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    memset(&wc, 0, sizeof wc);
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(NULL);
    wc.lpszClassName = "YgoPerfil";
    RegisterClassA(&wc);
    screen = GetDC(NULL);
    vb.mem = CreateCompatibleDC(screen);
    vs.mem = CreateCompatibleDC(screen);
    vc.mem = CreateCompatibleDC(screen);
    vu.mem = CreateCompatibleDC(screen);
    vp.mem = CreateCompatibleDC(screen);
    vm.mem = CreateCompatibleDC(screen);
    vt.mem = CreateCompatibleDC(screen);         /* 2026-11-20: the tapete with its light, the deck shining (without a DC they drew nothing) */
    vk.mem = CreateCompatibleDC(screen);
    leer_recurso("fondo.png", &s_fondo);
    flechas_afuera();
    sacar_triangulos();
    hacer_fondo2();
    leer_recurso("boton.png", &s_boton);
    leer_recurso("boton_luz.png", &s_boton_luz);
    leer_recurso("boton_sol.png", &s_boton_sol);
    leer_recurso("salir.png", &s_salir);
    leer_recurso("salir_luz.png", &s_salir_luz);
    leer_recurso("salir_sol.png", &s_salir_sol);
    leer_recurso("cartel.png", &s_cartel);
    {
        int k;
        for (k = 0; k < 6; k++) {
            char n[24];
            snprintf(n, sizeof n, "rueda_%d.png", k);
            leer_recurso(n, &s_rueda[k]);
        }
    }
    leer_recurso("fondo_completo.jpg", &s_fondo_completo);
    leer_recurso("puerta_izq.jpg", &s_puerta[0]);
    leer_recurso("puerta_der.jpg", &s_puerta[1]);
    vd.mem = CreateCompatibleDC(screen);
    vf[0].mem = CreateCompatibleDC(screen);     /* the page arrows (without a DC they drew nothing) */
    vf[1].mem = CreateCompatibleDC(screen);
    {
        int k;
        for (k = 0; k < 6; k++)                 /* 2026-11-19: the six arrows (option D) */
            vfd[k].mem = CreateCompatibleDC(screen);
    }
    leer_recurso("placa_nombre.png", &s_placa_nombre);
    leer_recurso("masc_luz.png", &s_masc_luz);
    leer_recurso("funda_luz.png", &s_funda_luz);
    leer_recurso("flecha_d_izq.png", &s_fd[0]);
    leer_recurso("flecha_d_der.png", &s_fd[1]);
    leer_recurso("flecha_d_izq_luz.png", &s_fd_luz[0]);
    leer_recurso("flecha_d_der_luz.png", &s_fd_luz[1]);
    leer_recurso("flecha_izq_luz.png", &s_flecha_luz[0]);
    leer_recurso("flecha_der_luz.png", &s_flecha_luz[1]);
    leer_recurso("flecha_izq.png", &s_flecha[0]);
    leer_recurso("flecha_der.png", &s_flecha[1]);
    leer_recurso("foto_luz.png", &s_foto_luz);
    leer_recurso("../buscador/minis.png", &s_minis);
    fprintf(stderr, "[PERFIL] listo (fondo %dx%d, boton %dx%d)\n", s_fondo.w, s_fondo.h, s_boton.w, s_boton.h);

    for (;;) {
        MSG m;
        HWND g = FindWindowA("RecompNv2aD3D11", NULL);
        RECT cr;
        POINT tl = { 0, 0 };
        ULONGLONG t = GetTickCount64();
        /* 2026-11-25: another game slot loaded: its own name and photo
         * (perfil_dir.c, YGO_PERFIL\Ranura N\perfil.ini) */
        {
            static unsigned ran = 99u;
            if (ran != ygo_ranura()) {
                ran = ygo_ranura();
                EnterCriticalSection(&s_cs);
                wcscpy(s_nombre, L"Duelista");
                s_foto = 0;
                cargar();
                cambio();
                LeaveCriticalSection(&s_cs);
            }
        }
        int base = g && GetForegroundWindow() == g && !xbox_MenuIsOpen() && !IsIconic(g)
                   && GetClientRect(g, &cr) && cr.bottom > 100 && cr.right > 100;
        float ax = 0, ay = 0, bx = 0, by = 0;
        int abierto, sobre_boton, sobre_que, editando, rueda, panel_vis;
        float rx, ry, puertas;
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
            s_ax = ax; s_ay = ay;
            s_sx = (float)cr.right / (bx - ax);
            s_sy = (float)cr.bottom / (by - ay);
            if (s_cw != cr.right || s_ch != cr.bottom) {
                s_cw = cr.right;
                s_ch = cr.bottom;
                cambio();
            }
        }
        rueda = t - s_rueda_at < 200u;
        if (s_abierto && s_cerrar_t && t - s_cerrar_t >= PUERTAS_MS) {   /* the doors are open again */
            s_abierto = 0;
            s_cerrar_t = 0;
            cambio();
        }
        abierto = s_abierto && rueda;
        /* the doors: shutting over the menu, then open on the panel; or back */
        if (abierto) {
            ULONGLONG e = s_cerrar_t ? t - s_cerrar_t : t - s_abrir_t;
            puertas = puertas_f(e);
            panel_vis = s_cerrar_t ? e < CIERRA_MS : e >= CIERRA_MS;
        } else {
            puertas = 0.f;
            panel_vis = 0;
        }
        sobre_boton = s_sobre_boton;
        sobre_que = s_sobre;
        pag_clic[0] = s_pag_clic[0];
        pag_clic[1] = s_pag_clic[1];
        editando = s_editando;
        rendija = s_rendija;
        rx = (s_rueda_x - s_ax) * s_sx;
        ry = (s_rueda_y - s_ay) * s_sy;
        if (editando && t >= parpadeo) {
            parpadeo = t + 500u;
            s_cursor_vis ^= 1;
            cambio();
        }
        ver = s_version + (LONG)(ygo_estadisticas_version() << 20);
        /* 2026-11-19 (the user: the mascot changed in F2 shows in the Perfil,
         * "esto no pasa con las fundas ni los tapetes"): those are drawn in the
         * panel, so a change of them repaints it too */
        ver += (LONG)((ygo_funda_get() * 8 + ygo_funda_marco_get()) * 131 + ygo_tapete_get() * 7919 + ygo_tapete_fx_get() * 104729);
        nombre_del_juego(s_p_nombre);
        if (!s_p_nombre[0])
            wcscpy(s_p_nombre, s_nombre);
        wcscpy(s_p_edit, s_edit);
        s_p_foto = s_foto;
        s_p_pagina = s_pagina;
        s_p_sobre = s_sobre;
        s_p_mazo = s_mazo;
        LeaveCriticalSection(&s_cs);

        s_s = (float)s_ch / REF_H;
        if ((float)s_cw / REF_W < s_s)
            s_s = (float)s_cw / REF_W;

        /* the button */
        /* 2026-11-17 (the user's capture: our doors half shut, the button
         * already gone): it stays while the panel is not showing, i.e. under
         * the doors as they shut (and back as they open); the doors' window
         * is TOPMOST each frame, over it */
        if (base && rueda && (!abierto || !panel_vis) && s_boton.p) {
            /* 2026-11-16 (the user: "que gire asi como los otros? ... una onda
             * expansiva"): with the mouse over it, the wheel's chosen icon's
             * look: the gold sun, its rays turning (boton_sol.png, SOL_CUADROS
             * steps of one ray, ~110 deg/s as in the video), and when it
             * lights a wave: a see-through gold ellipse growing from 1.25 to
             * 2.5 times the medallion and fading (video: ~4 frames at 30/s).
             * The window is ONDA_K times the button, for the wave's room. */
            int sol = sobre_boton && s_boton_sol.p && s_boton_sol.h >= s_boton.h * SOL_CUADROS;
            const img *b = sobre_boton && s_boton_luz.p ? &s_boton_luz : &s_boton;
            int bw = (int)(s_boton.w * s_s + 0.5f), bh = (int)(s_boton.h * s_s + 0.5f);
            int w = (int)(s_boton.w * ONDA_K * s_s + 0.5f), h = (int)(s_boton.h * ONDA_K * s_s + 0.5f), nuevo = 0;
            int cuadro = 0;
            float onda = 2.f;
            POINT pos;
            img fr;
            if (sobre_boton && boton_luz_de != 1)
                onda_desde = t;
            if (sol) {
                cuadro = (int)((t - onda_desde) * (110.0 / 22.5) * SOL_CUADROS / 1000.0) % SOL_CUADROS;
                fr.w = s_boton.w;
                fr.h = s_boton.h;
                fr.p = s_boton_sol.p + (size_t)cuadro * s_boton.w * s_boton.h * 4;
                b = &fr;
                onda = (float)(t - onda_desde) / ONDA_MS;
            }
            if (boton_luz_de != sobre_boton || boton_s != s_s || !vb.bmp || cuadro != sol_cuadro_de || onda < 1.f
                || onda_de < 1.f || rendija != rendija_de) {
                if (lienzo_de(&vb, screen, w, h)) {
                    memset(vb.l.p, 0, (size_t)w * h * 4);
                    if (onda < 1.f) {
                        /* the ellipse of the medallion (the picture's box less
                         * the 30 px margins), grown */
                        float k = 1.25f + 1.25f * onda, a = 0.85f * (1.f - onda);   /* 2026-11-20: brighter, as the game's ("menos intensidad") */
                        float erx = (s_boton.w - 60) * 0.5f * s_s * k, ery = (s_boton.h - 60) * 0.5f * s_s * k;
                        float cx = w * 0.5f, cy = h * 0.5f;
                        int i, j;
                        for (j = 0; j < h; j++)
                            for (i = 0; i < w; i++) {
                                float dx = (i + 0.5f - cx) / erx, dy = (j + 0.5f - cy) / ery, d = dx * dx + dy * dy;
                                float e, al;
                                unsigned A;
                                if (d > 1.f)
                                    continue;
                                e = sqrtf(d);
                                al = a * (0.55f + 0.45f * e * e) * (e > 0.96f ? (1.f - e) / 0.04f : 1.f);
                                A = (unsigned)(al * 255.f + 0.5f);
                                sobre(vb.l.p + ((size_t)j * w + i) * 4, 120u * A / 255u, 215u * A / 255u,
                                      255u * A / 255u, A);
                            }
                    }
                    pegar_todo(&vb.l, b, (w - bw) * 0.5f, (h - bh) * 0.5f, (float)bw, (float)bh);
                    if (rendija < 1.f) {
                        /* the game's doors cover all but their gap, centred */
                        float m = (float)s_cw * 0.5f, a = rendija * m, x0 = rx - (float)(w / 2);
                        int i, j;
                        for (i = 0; i < w; i++) {
                            float x = x0 + i + 0.5f;
                            if (fabsf(x - m) < a)
                                continue;
                            for (j = 0; j < h; j++)
                                memset(vb.l.p + ((size_t)j * w + i) * 4, 0, 4);
                        }
                    }
                    nuevo = 1;
                }
                boton_luz_de = sobre_boton;
                boton_s = s_s;
                sol_cuadro_de = cuadro;
                onda_de = onda;
                rendija_de = rendija;
            }
            pos.x = tl.x + (int)rx - w / 2;
            pos.y = tl.y + (int)ry - h / 2;
            if (vb.bmp) {
                /* comes in over 220 ms, with the doors sliding open */
                ULONGLONG f;
                if (!vb.shown) {
                    fprintf(stderr, "[PERFIL] boton visible\n");
                    fflush(stderr);
                    boton_desde = t;
                }
                f = (t - boton_desde) * 255u / 220u;
                boton_alfa = f > 255u ? 255 : (BYTE)f;
                mostrar(&vb, screen, wc.lpszClassName, pos, nuevo || boton_alfa < 255 || boton_alfa_de < 255,
                        boton_alfa);
                boton_alfa_de = boton_alfa;
            }
        } else {
            if (vb.shown) {
                fprintf(stderr, "[PERFIL] boton oculto (%s) %llu ms\n", !base ? "ventana" : !rueda ? "sin rueda" : "panel",
                        (unsigned long long)GetTickCount64());
                fflush(stderr);
            }
            ocultar(&vb);
        }

        /* the exit button: the Perfil button's ways (shown with it, behind the
         * game's doors as they shut, lit and turning with the mouse on it) */
        if (base && rueda && (!abierto || !panel_vis) && s_salir.p && t - g_ygo_sal_at < 200u) {
            g_nv2a_oculta.x[0] = g_ygo_sal_x;
            g_nv2a_oculta.y[0] = g_ygo_sal_y;
            g_nv2a_oculta.r[0] = 20.f;
            g_nv2a_oculta.hasta[0] = t + 150u;
            static int luz_de = -1, cuadro_de = -1;
            static float s_de, onda_de2 = 2.f, rend_de = -1.f;
            static ULONGLONG desde_onda, desde;
            static BYTE alfa_de;
            int encima = g_ygo_sal_sel && !sobre_boton;
            int sol = encima && s_salir_sol.p && s_salir_sol.h >= s_salir.h * SOL_CUADROS;
            const img *b = encima && s_salir_luz.p ? &s_salir_luz : &s_salir;
            float kx = s_sx * SALIR_KX * 1.12f, ky = s_sy * SALIR_KY * 1.12f;
            float mx = g_ygo_sal_x, my = g_ygo_sal_y;
            if (t - g_nv2a_oculta.medido[0] < 500u) {
                float qw = g_nv2a_oculta.bx1[0] - g_nv2a_oculta.bx0[0], qh = g_nv2a_oculta.by1[0] - g_nv2a_oculta.by0[0];
                kx = qw * s_sx * 0.96f / (float)(s_salir.w - 60);
                ky = qh * s_sy * 0.96f / (float)(s_salir.h - 60);
                mx = (g_nv2a_oculta.bx0[0] + g_nv2a_oculta.bx1[0]) * 0.5f;
                my = (g_nv2a_oculta.by0[0] + g_nv2a_oculta.by1[0]) * 0.5f;
            }
            int bw = (int)(s_salir.w * kx + 0.5f), bh = (int)(s_salir.h * ky + 0.5f);
            int w = (int)(s_salir.w * ONDA_K * kx + 0.5f), h = (int)(s_salir.h * ONDA_K * ky + 0.5f), nuevo = 0;
            int cuadro = 0;
            float onda = 2.f, sx = (mx - s_ax) * s_sx, sy = (my - s_ay) * s_sy;
            POINT pos;
            img fr;
            BYTE alfa;
            if (encima && luz_de != 1)
                desde_onda = t;
            if (sol) {
                cuadro = (int)((t - desde_onda) * (110.0 / 22.5) * SOL_CUADROS / 1000.0) % SOL_CUADROS;
                fr.w = s_salir.w;
                fr.h = s_salir.h;
                fr.p = s_salir_sol.p + (size_t)cuadro * s_salir.w * s_salir.h * 4;
                b = &fr;
                onda = (float)(t - desde_onda) / ONDA_MS;
            }
            if (luz_de != encima || s_de != kx * 1000.f + ky || !vs.bmp || cuadro != cuadro_de || onda < 1.f || onda_de2 < 1.f
                    || rendija != rend_de) {
                if (lienzo_de(&vs, screen, w, h)) {
                    memset(vs.l.p, 0, (size_t)w * h * 4);
                    if (onda < 1.f) {
                        float kk = 1.25f + 1.25f * onda, a = 0.85f * (1.f - onda);
                        float erx = (s_salir.w - 60) * 0.5f * kx * kk, ery = (s_salir.h - 60) * 0.5f * ky * kk;
                        float cx = w * 0.5f, cy = h * 0.5f;
                        int i, j;
                        for (j = 0; j < h; j++)
                            for (i = 0; i < w; i++) {
                                float dx = (i + 0.5f - cx) / erx, dy = (j + 0.5f - cy) / ery, d = dx * dx + dy * dy;
                                float e, al;
                                unsigned A;
                                if (d > 1.f)
                                    continue;
                                e = sqrtf(d);
                                al = a * (0.55f + 0.45f * e * e) * (e > 0.96f ? (1.f - e) / 0.04f : 1.f);
                                A = (unsigned)(al * 255.f + 0.5f);
                                sobre(vs.l.p + ((size_t)j * w + i) * 4, 120u * A / 255u, 215u * A / 255u,
                                      255u * A / 255u, A);
                            }
                    }
                    pegar_todo(&vs.l, b, (w - bw) * 0.5f, (h - bh) * 0.5f, (float)bw, (float)bh);
                    if (rendija < 1.f) {
                        float m = (float)s_cw * 0.5f, a = rendija * m, x0 = sx - (float)(w / 2);
                        int i, j;
                        for (i = 0; i < w; i++) {
                            float x = x0 + i + 0.5f;
                            if (fabsf(x - m) < a)
                                continue;
                            for (j = 0; j < h; j++)
                                memset(vs.l.p + ((size_t)j * w + i) * 4, 0, 4);
                        }
                    }
                    nuevo = 1;
                }
                luz_de = encima;
                s_de = kx * 1000.f + ky;
                cuadro_de = cuadro;
                onda_de2 = onda;
                rend_de = rendija;
            }
            pos.x = tl.x + (int)sx - w / 2;
            pos.y = tl.y + (int)sy - h / 2;
            if (vs.bmp) {
                ULONGLONG f;
                (void)f; (void)desde;
                alfa = 255;             /* at once: the game's icon is under it */
                mostrar(&vs, screen, wc.lpszClassName, pos, nuevo || alfa < 255 || alfa_de < 255, alfa);
                alfa_de = alfa;
            }
        } else
            ocultar(&vs);
        static ULONGLONG cartel_desde;
        if (!(base && rueda && !abierto && sobre_boton))
            cartel_desde = 0;
        else if (!cartel_desde)
            cartel_desde = t;
        if (base && rueda && !abierto && sobre_boton) {
            g_ygo_cartel_perfil_hasta = t + 150u;
            g_nv2a_oculta.x[1] = 320.f;         /* the chosen icon's light off (as before) */
            g_nv2a_oculta.y[1] = 240.f;
            g_nv2a_oculta.r[1] = 1000.f;
            g_nv2a_oculta.hasta[1] = t + 150u;
            g_nv2a_oculta.x[2] = g_ygo_sel_x;
            g_nv2a_oculta.y[2] = g_ygo_sel_y;
            g_nv2a_oculta.r[2] = 20.f;
            g_nv2a_oculta.hasta[2] = t + 150u;
        }
        if (base && rueda && !abierto && sobre_boton && s_cartel.p
                && t - g_ygo_cartel_perfil_ok > 300u && t - cartel_desde > 400u) {
            static float de_w = -1.f;
            int x0 = (int)((CARTEL_X0 - s_ax) * s_sx), y0 = (int)((CARTEL_Y0 - s_ay) * s_sy);
            int w = (int)((CARTEL_X1 - CARTEL_X0) * s_sx + 0.5f), h = (int)((CARTEL_Y1 - CARTEL_Y0) * s_sy + 0.5f);
            POINT pos;
            g_nv2a_oculta.x[1] = 320.f;
            g_nv2a_oculta.y[1] = 240.f;
            g_nv2a_oculta.r[1] = 1000.f;
            g_nv2a_oculta.hasta[1] = t + 150u;
            g_nv2a_oculta.x[2] = g_ygo_sel_x;
            g_nv2a_oculta.y[2] = g_ygo_sel_y;
            g_nv2a_oculta.r[2] = 20.f;
            g_nv2a_oculta.hasta[2] = t + 150u;
            if (w > 8 && h > 8 && (de_w != (float)(w * 10000 + h) || !vc.shown) && lienzo_de(&vc, screen, w, h)) {
                wchar_t txt[40];
                int fh = (int)(h * 0.80f), tw, lh, W, H, i, j;
                unsigned char *tb;
                de_w = (float)(w * 10000 + h);
                memset(vc.l.p, 0, (size_t)w * h * 4);
                pegar_todo(&vc.l, &s_cartel, 0.f, 0.f, (float)w, (float)h);
                wide(f2_traducir("Perfil"), txt, 40);
                if (fh < 8)
                    fh = 8;
                tw = ft_ancho(txt, fh);
                lh = ft_linea(fh);
                W = tw + 4;
                H = lh + 4;
                tb = (unsigned char *)calloc((size_t)W * H, 4);
                if (tb) {
                    RECT rc = { 2, 2, 2 + tw, 2 + lh };
                    int ox = (w - W) / 2, oy = (h - H) / 2;
                    ft_dibujar(tb, W, H, &rc, txt, fh, FJ_G);
                    for (j = 0; j < H; j++)
                        for (i = 0; i < W; i++) {
                            unsigned c = tb[4 * (j * W + i) + 1];
                            int xx = ox + i, yy = oy + j;
                            if (c && xx >= 0 && yy >= 0 && xx < w && yy < h)
                                sobre(vc.l.p + 4 * ((size_t)yy * w + xx), 0, 4 * c / 255, 10 * c / 255, c);
                        }
                    free(tb);
                }
                pos.x = tl.x + x0;
                pos.y = tl.y + y0;
                mostrar(&vc, screen, wc.lpszClassName, pos, 1, 255);
            }
        } else
            ocultar(&vc);
        {
            int k = g_ygo_sel_k;
            if (base && rueda && !abierto && sobre_boton && k >= 0 && k < 6 && s_rueda[k].p
                    && t - g_nv2a_oculta.medido[2] < 500u) {
                static int de_k = -1, de_x, de_y, de_w, de_h;
                int x0 = (int)((g_nv2a_oculta.bx0[2] - s_ax) * s_sx), y0 = (int)((g_nv2a_oculta.by0[2] - s_ay) * s_sy);
                int w = (int)((g_nv2a_oculta.bx1[2] - g_nv2a_oculta.bx0[2]) * s_sx + 0.5f);
                int h = (int)((g_nv2a_oculta.by1[2] - g_nv2a_oculta.by0[2]) * s_sy + 0.5f);
                if (w > 8 && h > 8 && (k != de_k || x0 != de_x || y0 != de_y || w != de_w || h != de_h || !vu.shown)
                        && lienzo_de(&vu, screen, w, h)) {
                    POINT pos;
                    memset(vu.l.p, 0, (size_t)w * h * 4);
                    pegar_todo(&vu.l, &s_rueda[k], 0.f, 0.f, (float)w, (float)h);
                    pos.x = tl.x + x0;
                    pos.y = tl.y + y0;
                    mostrar(&vu, screen, wc.lpszClassName, pos, 1, 255);
                    de_k = k; de_x = x0; de_y = y0; de_w = w; de_h = h;
                }
            } else
                ocultar(&vu);
        }

        /* the doors, over everything: each half scaled to the window once,
         * then slid in rows */
        if (base && puertas > 0.f && s_puerta[0].p && s_puerta[1].p && s_cw > 0 && s_ch > 0) {
            int k, cw = s_cw, ch = s_ch;
            if (puertas_w != cw || puertas_h != ch || !s_puerta_esc[0].p || !s_puerta_esc[1].p) {
                for (k = 0; k < 2; k++) {
                    int pw = (int)((float)s_puerta[k].w * cw / (float)(s_puerta[0].w + s_puerta[1].w) + 0.5f);
                    lienzo t;
                    liberar(&s_puerta_esc[k]);
                    if (k == 1)
                        pw = cw - s_puerta_esc[0].w;
                    if (pw < 1)
                        continue;
                    s_puerta_esc[k].p = (unsigned char *)calloc((size_t)pw * ch, 4);
                    if (!s_puerta_esc[k].p)
                        continue;
                    s_puerta_esc[k].w = pw;
                    s_puerta_esc[k].h = ch;
                    t.p = s_puerta_esc[k].p; t.w = pw; t.h = ch;
                    pegar(&t, &s_puerta[k], 0, 0, s_puerta[k].w, s_puerta[k].h, 0.f, 0.f, (float)pw, (float)ch, 255);
                }
                puertas_w = cw;
                puertas_h = ch;
                puertas_de = -1.f;
            }
            if (s_puerta_esc[0].p && s_puerta_esc[1].p) {
                int nuevo = 0;
                if (puertas != puertas_de || !vd.bmp || vd.l.w != cw || vd.l.h != ch) {
                    if (lienzo_de(&vd, screen, cw, ch)) {
                        /* the left half's right edge at its place times f, the right one's left edge likewise */
                        int lw = s_puerta_esc[0].w, rw = s_puerta_esc[1].w, j;
                        int lx = (int)((float)lw * puertas + 0.5f) - lw;          /* where its left column goes */
                        int rx0 = cw - (int)((float)rw * puertas + 0.5f);
                        memset(vd.l.p, 0, (size_t)cw * ch * 4);
                        for (j = 0; j < ch; j++) {
                            unsigned char *d = vd.l.p + (size_t)j * cw * 4;
                            int n = lw + lx;                                     /* its visible columns */
                            if (n > 0)
                                memcpy(d, s_puerta_esc[0].p + ((size_t)j * lw - lx) * 4, (size_t)n * 4);
                            n = cw - rx0;
                            if (n > 0)
                                memcpy(d + (size_t)rx0 * 4, s_puerta_esc[1].p + (size_t)j * rw * 4, (size_t)n * 4);
                        }
                        nuevo = 1;
                    }
                    puertas_de = puertas;
                }
                if (vd.bmp) {
                    mostrar(&vd, screen, wc.lpszClassName, tl, nuevo, 255);
                    SetWindowPos(vd.win, HWND_TOPMOST, 0, 0, 0, 0,
                                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
                }
            }
        } else if (vd.shown) {
            ocultar(&vd);
            puertas_de = 0.f;
        }
        /* 2026-11-16 (the user: "El Perfil aparece antes"): the panel and
         * the mascot came up TOPMOST over the shut doors on their first
         * showing; now the doors are drawn first and those go just under them */
        s_arriba = vd.shown ? vd.win : HWND_TOPMOST;
        /* the panel and its mascot */
        if (base && abierto && panel_vis && !ygo_as_tapa()) {
            int masc = ygo_mascota_tuya_get(), nuevo = 0;
            if (pintado != ver || !vp.bmp || vp.l.w != s_cw || vp.l.h != s_ch) {
                if (lienzo_de(&vp, screen, s_cw, s_ch)) {
                    pintar_panel(&vp.l, sobre_que, editando, s_cursor_vis);
                    nuevo = 1;
                }
                pintado = ver;
            }
            if (vp.bmp)
                mostrar(&vp, screen, wc.lpszClassName, tl, nuevo, 255);
            if (s_p_pagina != 0)                /* the opponents, the statistics: no mascot */
                ocultar(&vm);
            else if (s_tira_de != masc) {
                s_tira_de = masc;
                leer_tira(masc, &s_tira);
                desde = GetTickCount64();
                frame = -1;
            }
            /* the page arrows: still, a slow nod outwards and brighter
             * under the pointer, a push out when clicked */
            {
                int k;
                ULONGLONG t = GetTickCount64();
                int l = (int)(TRI_VENT * s_s + 0.5f);
                for (k = 0; k < 2; k++) {
                    int sobre = sobre_que == PAG_ANT + k;
                    if (s_p_mazo.ch >= 0) {
                        /* 2026-11-19 (the user's capture): not over the big card;
                         * 2026-11-21 (the user: "oculta las flechas arriba cuando se
                         * abra este menu de los decks"): not while a deck is open */
                        ocultar(&vf[k]);
                        flecha_de[k] = -1;
                        continue;
                    }
                    float c = pag_clic[k] && t - pag_clic[k] < 220u ? (float)(t - pag_clic[k]) / 220.f : 1.f;
                    float empuje = c < 1.f ? sinf(3.14159f * c) : 0.f;
                    float vaiven = sobre ? 0.5f - 0.5f * cosf((float)(t % 700u) * (6.28318f / 700.f)) : 0.f;
                    float dx = (vaiven * 5.f + empuje * 12.f) * (k == 0 ? -1.f : 1.f), esc = 1.f + 0.25f * empuje + (sobre ? 0.08f : 0.f);
                    int estado = sobre || c < 1.f ? -2 : 0;  /* -2: moving, redraw every turn */
                    const img *m = sobre || c < 1.f ? &s_tri_luz[k] : &s_tri[k];
                    if (!m->p || l < 4)
                        continue;
                    if ((estado != flecha_de[k] || estado == -2 || nuevo || !vf[k].shown) && lienzo_de(&vf[k], screen, l, l)) {
                        POINT pos;
                        float w = (float)m->w * esc * s_s, h = (float)m->h * esc * s_s;
                        int primera = !vf[k].shown;
                        memset(vf[k].l.p, 0, (size_t)l * l * 4);
                        pegar(&vf[k].l, m, 0, 0, m->w, m->h, (float)l * 0.5f - w * 0.5f + dx * s_s,
                              (float)l * 0.5f - h * 0.5f, w, h, 255);
                        pos.x = tl.x + (int)X(TRI_CX(k) - TRI_VENT * 0.5f);
                        pos.y = tl.y + (int)Y(TRI_CY(k) - TRI_VENT * 0.5f);
                        mostrar(&vf[k], screen, wc.lpszClassName, pos, 1, 255);
                        if (primera || nuevo)
                            SetWindowPos(vf[k].win, s_arriba, 0, 0, 0, 0,
                                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
                        flecha_de[k] = estado;
                    }
                }
            }
            /* 2026-11-19: the six arrows (option D). Still: frame 0. Under the
             * pointer: lit, the rim turning (a notch step every ~0.5 s) and a
             * slow nod outwards. Clicked: a push out and bigger for 220 ms. */
            {
                int k;
                ULONGLONG t = GetTickCount64();
                int l = (int)(FD_VENT * s_s + 0.5f);
                for (k = 0; k < 6; k++) {
                    int sobre = sobre_que == MASC_ANT + k, der = k % 2;
                    ULONGLONG ck = s_fd_clic[k];
                    float c = ck && t - ck < 220u ? (float)(t - ck) / 220.f : 1.f;
                    float empuje = c < 1.f ? sinf(3.14159f * c) : 0.f;
                    float vaiven = sobre ? 0.5f - 0.5f * cosf((float)(t % 800u) * (6.28318f / 800.f)) : 0.f;
                    float dx = (vaiven * 4.f + empuje * 8.f) * (der ? 1.f : -1.f);
                    float esc = 1.f + 0.18f * empuje + (sobre ? 0.10f : 0.f);
                    int cuadro = sobre || c < 1.f ? (int)((t / 40u) % FD_CUADROS) : 0;
                    int estado = sobre || c < 1.f ? -2 : 0;
                    const img *m = sobre || c < 1.f ? &s_fd_luz[der] : &s_fd[der];
                    if (s_p_pagina != 0 || !m->p || l < 4) {
                        ocultar(&vfd[k]);
                        fd_de[k] = -1;
                        continue;
                    }
                    if ((estado != fd_de[k] || estado == -2 || nuevo || !vfd[k].shown) && lienzo_de(&vfd[k], screen, l, l)) {
                        POINT pos;
                        float w = FD_VER * esc * s_s;
                        int primera = !vfd[k].shown;
                        memset(vfd[k].l.p, 0, (size_t)l * l * 4);
                        pegar(&vfd[k].l, m, 0, cuadro * FD_LADO, FD_LADO, FD_LADO,
                              (float)l * 0.5f - w * 0.5f + dx * s_s, (float)l * 0.5f - w * 0.5f, w, w, 255);
                        pos.x = tl.x + (int)X(k_flecha[k][0] - FD_VENT * 0.5f);
                        pos.y = tl.y + (int)Y(k_flecha[k][1] - FD_VENT * 0.5f);
                        mostrar(&vfd[k], screen, wc.lpszClassName, pos, 1, 255);
                        if (primera || nuevo)
                            SetWindowPos(vfd[k].win, s_arriba, 0, 0, 0, 0,
                                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
                        fd_de[k] = estado;
                    }
                }
            }
            if (s_p_pagina == 0) {
                /* the frame of now: the model's own time */
                int nf = s_tira.idx ? s_tira.h / LADO : 1;
                int f = (int)((double)(GetTickCount64() - desde) / (double)(s_tira.ms > 1.f ? s_tira.ms : FRAME_MS)) % (nf > 0 ? nf : 1);
                int l = (int)(MASC_L * s_s + 0.5f);
                POINT pos;
                if ((f != frame || nuevo || !vm.shown) && lienzo_de(&vm, screen, l, l)) {
                    frame = f;
                    int primera = !vm.shown;
                    pintar_mascota(&vm.l, frame);
                    pos.x = tl.x + (int)X(MASC_X);
                    pos.y = tl.y + (int)Y(MASC_Y);
                    mostrar(&vm, screen, wc.lpszClassName, pos, 1, 255);
                    if (primera || nuevo)     /* over the panel, under the doors */
                        SetWindowPos(vm.win, s_arriba, 0, 0, 0, 0,
                                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
                }
            }
            if (s_p_pagina == 0) {
                float tt = (float)((double)GetTickCount64() / 1000.0);
                int tp = ygo_tapete_get();
                POINT pt, pk;
                pt.x = tl.x + (int)X(FOTO_X); pt.y = tl.y + (int)Y(FOTO_Y);
                pk.x = tl.x + (int)X(CARTA_X - 2.f); pk.y = tl.y + (int)Y(CARTA_Y - 26.f);
                /* the tapete's box with its light, live */
                if (tp >= 0 && ygo_tapete_fx_get() > 0 && s_tapete_img.p) {
                    int w = (int)(FOTO_W * s_s + 0.5f), h = (int)(FOTO_H * s_s + 0.5f);
                    /* 2026-11-20 (the user: "al cambiar de tapete no cambia queda el
                     * otro"): keyed on the tapete the picture holds (s_tapete_de,
                     * set when the panel reads it), not on the choice: the choice
                     * changes at the click, the picture at the panel's next paint,
                     * and a frame in between kept the old one for good */
                    if (tfx_de != s_tapete_de || tfx_w != w || tfx_h != h || !tfx_base) {
                        lienzo t;
                        free(tfx_base); free(tfx_out);
                        tfx_base = (unsigned char *)calloc((size_t)w * h, 4);
                        tfx_out = (unsigned char *)calloc((size_t)w * h, 4);
                        if (tfx_base) {
                            t.p = tfx_base; t.w = w; t.h = h;
                            pegar_todo(&t, &s_tapete_img, 0.f, 0.f, (float)w, (float)h);
                        }
                        tfx_de = s_tapete_de; tfx_w = w; tfx_h = h;
                    }
                    if (tfx_base && tfx_out && lienzo_de(&vt, screen, w, h)) {
                        int primera = !vt.shown;
                        ygo_tapete_fx_pintar(tfx_base, w, h, tfx_out, tt);
                        memcpy(vt.l.p, tfx_out, (size_t)w * h * 4);
                        mostrar(&vt, screen, wc.lpszClassName, pt, 1, 255);
                        if (primera || nuevo)
                            SetWindowPos(vt.win, s_arriba, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
                    }
                } else
                    ocultar(&vt);
                /* the deck shining while the pointer is on it */
                if (sobre_que == FUNDA_FOTO && s_funda_img.p
                        && lienzo_de(&vk, screen, (int)(MZ_W * s_s + 0.5f), (int)(MZ_H * s_s + 0.5f))) {
                    int primera = !vk.shown;
                    pintar_baraja(&vk.l, tt);
                    mostrar(&vk, screen, wc.lpszClassName, pk, 1, 255);
                    if (primera || nuevo)
                        SetWindowPos(vk.win, s_arriba, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
                } else
                    ocultar(&vk);
            } else {
                ocultar(&vt);
                ocultar(&vk);
            }
            if (ygo_as_activa() && vp.shown && (s_p_pagina != 0 || vm.shown))
                ygo_as_perfil_vuelto();     /* back over the animation's last picture */
            /* the click on the mascot: the panel's picture, then the animation over it */
            if (s_pedir_as >= 0 && vp.l.p && vp.l.w > 0 && vp.l.h > 0) {
                int m = (int)InterlockedExchange(&s_pedir_as, -1), k;
                size_t n = (size_t)vp.l.w * vp.l.h * 4;
                unsigned char *buf = (unsigned char *)malloc(n);
                if (buf) {
                    lienzo t;
                    memcpy(buf, vp.l.p, n);
                    t.p = buf; t.w = vp.l.w; t.h = vp.l.h;
                    if (vm.shown && vm.l.p) {
                        img mi = { vm.l.w, vm.l.h, vm.l.p };
                        pegar(&t, &mi, 0, 0, mi.w, mi.h, (float)(int)X(MASC_X), (float)(int)Y(MASC_Y),
                              (float)mi.w, (float)mi.h, 255);
                    }
                    if (vt.shown && vt.l.p) {
                        img ti = { vt.l.w, vt.l.h, vt.l.p };
                        pegar(&t, &ti, 0, 0, ti.w, ti.h, (float)(int)X(FOTO_X), (float)(int)Y(FOTO_Y), (float)ti.w, (float)ti.h, 255);
                    }
                    for (k = 0; k < 6; k++)
                        if (vfd[k].shown && vfd[k].l.p) {
                            img fi = { vfd[k].l.w, vfd[k].l.h, vfd[k].l.p };
                            pegar(&t, &fi, 0, 0, fi.w, fi.h, (float)(int)X(k_flecha[k][0] - FD_VENT * 0.5f),
                                  (float)(int)Y(k_flecha[k][1] - FD_VENT * 0.5f), (float)fi.w, (float)fi.h, 255);
                        }
                    if (!ygo_as_ver_con(m, buf, t.w, t.h))
                        free(buf);
                }
            }
        } else {
            ocultar(&vp);
            ocultar(&vm);
            ocultar(&vt);
            ocultar(&vk);
            ocultar(&vf[0]);
            ocultar(&vf[1]);
            {
                int k;
                for (k = 0; k < 6; k++) {
                    ocultar(&vfd[k]);
                    fd_de[k] = -1;
                }
            }
            if (!abierto && s_tira.idx) {    /* the strip is big: let it go */
                soltar_tira(&s_tira);
                s_tira_de = -1;
            }
        }
        ygo_estadisticas_revisar();     /* the game saved: the pending duels go in */
        Sleep(vd.shown || vp.shown ? 4 : (vb.shown ? (boton_alfa < 255 || sobre_boton ? 8 : 25) : 50));
    }
}

/* ---- the game's thread: raton.c -------------------------------------------- */

/* each frame the main menu's wheel waits for keys: its middle (frame px) */
void ygo_perfil_rueda(float x, float y)
{
    if (!s_ok)
        return;
    EnterCriticalSection(&s_cs);
    /* back on the menu after a while away: the panel starts closed */
    if (GetTickCount64() - s_rueda_at > 1000u && s_abierto) {
        s_abierto = 0;
        s_cerrar_t = 0;
        cambio();
    }
    s_rueda_at = GetTickCount64();
    s_rueda_x = x;
    s_rueda_y = y;
    if (!s_abierto && s_editando)
        s_editando = 0;
    LeaveCriticalSection(&s_cs);
}

/* 2026-11-16 (the user: the button showed over the "¿Seguro?" box a moment):
 * the wheel left 0/1 (raton.c, the frame it changes): the button goes now,
 * not after rueda's 200 ms */
void ygo_perfil_rendija(float f)
{
    if (!s_ok)
        return;
    EnterCriticalSection(&s_cs);
    s_rendija = f;
    LeaveCriticalSection(&s_cs);
}

void ygo_perfil_fuera(void)
{
    if (!s_ok)
        return;
    EnterCriticalSection(&s_cs);
    s_rueda_at = 0;
    LeaveCriticalSection(&s_cs);
}

/* window px of the pointer (game frame px gx, gy) -> capture px */
static void a_ref(float gx, float gy, float *x, float *y)
{
    float px = (gx - s_ax) * s_sx, py = (gy - s_ay) * s_sy;
    *x = (px - (float)s_cw * 0.5f) / s_s + REF_CX;
    *y = (py - (float)s_ch * 0.5f) / s_s + REF_CY;
}

static int que_hay(float x, float y)
{
    int i;
    /* 2026-11-21: a deck open hides the page arrows: no page change under it */
    for (i = 0; i < 2 && !(s_mazo.ch >= 0 && s_pagina != 0); i++) {   /* the banner's arrows and medallions: the page */
        float dx = x - k_pag[i][0], dy = y - k_pag[i][1], mx = x - k_pag_med[i][0], my = y - k_pag_med[i][1];
        if (dx * dx + dy * dy <= PAG_R * PAG_R || mx * mx + my * my <= PAG_R * PAG_R)
            return PAG_ANT + i;
    }
    if (s_mazo.ch >= 0 && s_pagina != 0)      /* a deck open (its big card: the panel) */
        return s_mazo.grande > 0 ? MAZO_PANEL : mazo_que(&s_mazo, x, y);
    if (s_pagina == PAG_RIVALES) {            /* the opponents' portraits */
        for (i = 0; i < NRIVALES; i++) {
            float rx = RG_X0 + (float)(i % RG_COLS) * RG_GX, ry = RG_Y0 + (float)(i / RG_COLS) * RG_GY;
            if (x >= rx - 8.f && x <= rx + RG_W + 8.f && y >= ry - 8.f && y <= ry + RG_H + 44.f)
                return RETRATO0 + i;
        }
        return NADA;
    }
    if (s_pagina == PAG_ESTAD) {              /* the statistics: the rivals */
        ygo_stats st;
        ygo_estadisticas(&st);
        if (st.dificil_ch >= 0 && x >= 348.f && x <= 868.f && y >= 831.f && y <= 941.f)
            return RIVAL_DIF;
        for (i = 0; i < st.n_ult; i++)
            if (x >= 978.f && x <= 1618.f && y >= 341.f + (float)i * 120.f && y <= 449.f + (float)i * 120.f)
                return RIVAL0 + i;
        return NADA;
    }
    for (i = 0; i < 6; i++) {                 /* first: the tapete's are on its box's sides */
        float dx = x - k_flecha[i][0], dy = y - k_flecha[i][1], r = FD_VER * 0.5f;
        if (dx * dx + dy * dy <= r * r)
            return MASC_ANT + i;
    }
    if (x >= FOTO_X - 10.f && x <= FOTO_X + FOTO_W + 10.f && y >= FOTO_Y - 10.f && y <= FOTO_Y + FOTO_H + 10.f)
        return FOTO;
    if (x >= MASC_X - 10.f && x <= MASC_X + MASC_L + 10.f && y >= MASC_Y - 10.f && y <= MASC_Y + MASC_L + 10.f)
        return MASC_FOTO;
    if (x >= CARTA_X - 10.f && x <= CARTA_X + CARTA_W + 30.f && y >= CARTA_Y - 34.f && y <= CARTA_Y + CARTA_H + 10.f)
        return FUNDA_FOTO;
    /* 2026-11-19 (the user: "hace que no se pueda editar el nombre en
     * Perfil"): the name is only shown, never a target (no frame, no edit) */
    return NADA;
}

/* the name being typed: keys pressed since the last frame */
static void teclear(void)
{
    static unsigned char antes[256];
    BYTE ks[256];
    int vk;
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
                || vk == VK_LBUTTON || vk == VK_RBUTTON || vk == VK_MBUTTON) {
            antes[vk] = (unsigned char)down;
            continue;
        }
        if (down && !antes[vk]) {
            size_t n = wcslen(s_edit);
            if (vk == VK_RETURN) {
                if (n) {
                    wcscpy(s_nombre, s_edit);
                    nombre_al_juego(s_edit);
                    guardar();
                }
                s_editando = 0;
            } else if (vk == VK_ESCAPE)
                s_editando = 0;
            else if (vk == VK_BACK) {
                if (n)
                    s_edit[n - 1] = 0;
            } else {
                wchar_t c[4];
                int r = ToUnicodeEx((UINT)vk, MapVirtualKeyA((UINT)vk, 0), ks, c, 4, 4, GetKeyboardLayout(0));
                if (r == 1 && letra_del_juego(c[0]) && n < NOMBRE_MAX) {
                    s_edit[n] = c[0];
                    s_edit[n + 1] = 0;
                }
            }
            cambio();
        }
        antes[vk] = (unsigned char)down;
    }
}

/* raton.c's wheel part, first: 0 nothing of ours (the wheel goes on), 1 the
 * pointer is on our button (no wheel hover; a click opens), 2 the panel is
 * open or just closed (the pad state is taken off) */
int ygo_perfil_raton(int over, float gx, float gy, int clic, int derecho)
{
    static int soltar;                  /* closed: until both buttons are up */
    static ULONGLONG soltado;           /* and then a quarter second more (the 13:5x log: a B got through) */
    static int esc_antes;
    int r = 0, esc, editaba;
    float x, y;
    if (!s_ok)
        return 0;
    EnterCriticalSection(&s_cs);
    if (soltar) {
        if (!(GetAsyncKeyState(VK_LBUTTON) & 0x8000) && !(GetAsyncKeyState(VK_RBUTTON) & 0x8000)
                && !(GetAsyncKeyState(VK_ESCAPE) & 0x8000)) {
            soltar = 0;
            soltado = GetTickCount64();
        }
        LeaveCriticalSection(&s_cs);
        return 2;
    }
    if (soltado && GetTickCount64() - soltado < 250u) {
        LeaveCriticalSection(&s_cs);
        return 2;
    }
    soltado = 0;
    if (s_abierto && ygo_as_activa()) {   /* the ace card's preview: the panel is hidden, nothing to touch */
        LeaveCriticalSection(&s_cs);
        return 2;
    }
    if (!s_abierto) {
        float px = (gx - s_ax) * s_sx, py = (gy - s_ay) * s_sy;
        float bx = (s_rueda_x - s_ax) * s_sx, by = (s_rueda_y - s_ay) * s_sy;
        float rx = (float)(s_boton.w - 60) * 0.5f * s_s, ry = (float)(s_boton.h - 60) * 0.5f * s_s;
        int en = over && s_boton.p && rx > 1.f && ry > 1.f
                 && (px - bx) * (px - bx) / (rx * rx) + (py - by) * (py - by) / (ry * ry) <= 1.f;
        if (en != s_sobre_boton) {
            s_sobre_boton = en;
        }
        if (en) {
            r = 1;
            if (clic) {
                s_abierto = 1;
                s_pagina = 0;
                ygo_prestado_revisar_todos();
                s_mazo.ch = -1;
                s_abrir_t = GetTickCount64();
                s_cerrar_t = 0;
                s_sobre_boton = 0;
                s_sobre = NADA;
                s_editando = 0;
                cambio();
                fprintf(stderr, "[PERFIL] abierto\n");
                fflush(stderr);
                r = 2;
            }
        }
        LeaveCriticalSection(&s_cs);
        return r;
    }
    /* open; while the doors move, nothing of the panel answers */
    if (s_cerrar_t || GetTickCount64() - s_abrir_t < PUERTAS_MS) {
        LeaveCriticalSection(&s_cs);
        return 2;
    }
    esc = (GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0;
    editaba = s_editando;
    if (s_editando)
        teclear();                      /* Esc there only leaves the name as it was */
    if (s_mazo.ch >= 0 && (derecho || (esc && !esc_antes))) {
        esc_antes = esc;
        if (s_mazo.conf)
            s_mazo.conf = 0;                /* only the question closes */
        else if (s_mazo.grande > 0)
            s_mazo.grande = 0;              /* only the big card closes */
        else
            s_mazo.ch = -1;                 /* only the deck panel closes */
        s_sobre = NADA;
        soltar = 1;
        cambio();
        LeaveCriticalSection(&s_cs);
        return 2;
    }
    if (derecho || (!editaba && esc && !esc_antes)) {
        esc_antes = esc;
        if (s_editando)
            s_editando = 0;
        s_cerrar_t = GetTickCount64();      /* the thread closes it when the doors are open again */
        soltar = 1;
        cambio();
        fprintf(stderr, "[PERFIL] cerrado\n");
        fflush(stderr);
        LeaveCriticalSection(&s_cs);
        return 2;
    }
    esc_antes = esc;
    a_ref(gx, gy, &x, &y);
    {
        int q = over ? que_hay(x, y) : NADA;
        if (q != s_sobre) {
            s_sobre = q;
            cambio();
        }
        /* 2026-11-19 (the user: "si se deja el clic apretado en una flecha
         * que se siga moviendo"): the left button held on the same arrow
         * (mascot, sleeve, tapete) steps again after 400 ms, every 110 ms,
         * every 60 ms after 1.5 s; a single click still steps once */
        {
            static int rep_q;
            static ULONGLONG rep_t0, rep_ult;
            ULONGLONG ahora = GetTickCount64();
            int apretado = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
            if (clic) {
                rep_q = q >= MASC_ANT && q <= TAP_SIG ? q : 0;
                rep_t0 = rep_ult = ahora;
            } else if (rep_q && (!apretado || q != rep_q)) {
                rep_q = 0;
            } else if (rep_q && ahora - rep_t0 >= 400u
                       && ahora - rep_ult >= (ahora - rep_t0 >= 1500u ? 60u : 110u)) {
                rep_ult = ahora;
                clic = 1;
            }
        }
        if (clic) {
            int n;
            if (s_editando && q != NOMBRE) {      /* a click elsewhere keeps the name */
                if (wcslen(s_edit)) {
                    wcscpy(s_nombre, s_edit);
                    nombre_al_juego(s_edit);
                    guardar();
                }
                s_editando = 0;
            }
            if (s_mazo.ch >= 0 && s_mazo.grande > 0) {
                s_mazo.grande = 0;              /* the big card: any click closes it */
                s_sobre = NADA;
                cambio();
                LeaveCriticalSection(&s_cs);
                return 2;
            }
            if (s_mazo.ch >= 0 && s_mazo.conf) {
                /* the question: Aceptar lends the deck, anything else is no */
                if (q == CONF_SI) {
                    ygo_prestado_usar(s_mazo.ch, 1);
                    ygo_prestado_info_de(s_mazo.ch, &s_mazo.pi);
                }
                s_mazo.conf = 0;
                s_sobre = NADA;
                cambio();
                LeaveCriticalSection(&s_cs);
                return 2;
            }
            if (s_mazo.ch >= 0 && q == MAZO_BOTON) {
                /* "Probar este Mazo": the question first; "Devolver el mazo": at once */
                ygo_prestado_info_de(s_mazo.ch, &s_mazo.pi);
                if (s_mazo.pi.activo) {
                    ygo_prestado_usar(s_mazo.ch, 0);
                    ygo_prestado_info_de(s_mazo.ch, &s_mazo.pi);
                } else if (s_mazo.pi.ganado) {      /* yours: at once (the user: "simplemente apretando") */
                    ygo_prestado_usar(s_mazo.ch, 1);
                    ygo_prestado_info_de(s_mazo.ch, &s_mazo.pi);
                } else if (s_mazo.pi.usos > 0)
                    s_mazo.conf = 1;
                s_sobre = NADA;
                cambio();
                LeaveCriticalSection(&s_cs);
                return 2;
            }
            if (s_mazo.ch >= 0 && q != PAG_ANT && q != PAG_SIG) {
                /* the deck panel: a card shows big; the X or a click off it closes it */
                if (q >= MAZO_CARTA && q < MAZO_CARTA + 2 * MAZO_MAX) {
                    int lado = (q - MAZO_CARTA) / MAZO_MAX, i = (q - MAZO_CARTA) % MAZO_MAX;
                    if (i < (lado ? s_mazo.na : s_mazo.nf))
                        s_mazo.grande = lado ? s_mazo.a[i].k : s_mazo.f[i].k;
                    s_sobre = NADA;
                } else if (q == MAZO_X || q == NADA) {
                    s_mazo.ch = -1;
                    s_sobre = NADA;
                }
                cambio();
                LeaveCriticalSection(&s_cs);
                return 2;
            }
            if (q >= RETRATO0 && q < RETRATO0 + NRIVALES) {
                if (desbloqueado(k_rivales[q - RETRATO0]))
                    mazo_abrir(k_rivales[q - RETRATO0]);
                s_sobre = NADA;
                cambio();
                LeaveCriticalSection(&s_cs);
                return 2;
            }
            if (q == RIVAL_DIF || (q >= RIVAL0 && q < RIVAL0 + YGO_ULTIMOS)) {
                ygo_stats st;
                ygo_estadisticas(&st);
                if ((q == RIVAL_DIF ? st.dificil_ch >= 0 : q - RIVAL0 < st.n_ult)
                        && desbloqueado(q == RIVAL_DIF ? st.dificil_ch : st.ult[q - RIVAL0].ch))
                    mazo_abrir(q == RIVAL_DIF ? st.dificil_ch : st.ult[q - RIVAL0].ch);
                s_sobre = NADA;
                cambio();
                LeaveCriticalSection(&s_cs);
                return 2;
            }
            switch (q) {
            /* 2026-11-20 (the user: "eliminar los Botones ... ahora sera haciendo
             * click en el tapete y se cambia entre las 4 opciones y el deck lo
             * mismo"): the tapete cycles its light (not the Clásico, the game's),
             * the deck its frame (not the Clásica) */
            case FOTO:
                if (ygo_tapete_get() >= 0) {      /* 2026-11-20: the Clásico too */
                    ygo_tapete_fx_set((ygo_tapete_fx_get() + 1) % 4);
                    xbox_MenuSaveSoon();
                }
                break;
            case FUNDA_FOTO:
                if (ygo_funda_get() > 0) {
                    ygo_funda_marco_set((ygo_funda_marco_get() + 1) % NMARCOS);
                    xbox_MenuSaveSoon();
                }
                break;
            case MASC_FOTO:
                InterlockedExchange(&s_pedir_as, ygo_mascota_tuya_get());
                s_sobre = NADA;
                break;
            case PAG_ANT:
            case PAG_SIG:
                s_mazo.ch = -1;
                s_pag_clic[q - PAG_ANT] = GetTickCount64();
                s_pagina = (s_pagina + (q == PAG_SIG ? 1 : NPAGINAS - 1)) % NPAGINAS;
                ygo_prestado_revisar_todos();
                s_editando = 0;
                s_sobre = NADA;
                break;
            /* 2026-11-19 (the user: "si se presiona encima de la foto de los
             * tapetes no se pueda cambiar (solo con las flechas)"): FOTO only glows */
            case TAP_ANT:
            case TAP_SIG:
                s_fd_clic[q - MASC_ANT] = GetTickCount64();
                n = ygo_tapete_get() + (q == TAP_ANT ? NTAPETES - 1 : 1);
                ygo_tapete_set(n % NTAPETES);
                xbox_MenuSaveSoon();
                break;
            case NOMBRE:
                if (!s_editando) {
                    s_editando = 1;
                    nombre_del_juego(s_edit);
                    if (!s_edit[0])
                        wcscpy(s_edit, s_nombre);
                }
                break;
            case MASC_ANT:
            case MASC_SIG:
                s_fd_clic[q - MASC_ANT] = GetTickCount64();
                n = ygo_mascota_tuya_get() + (q == MASC_SIG ? 1 : NMASCOTAS - 1);
                ygo_mascota_tuya_set(n % NMASCOTAS);
                xbox_MenuSaveSoon();
                break;
            case FUNDA_ANT:
            case FUNDA_SIG:
                s_fd_clic[q - MASC_ANT] = GetTickCount64();
                n = ygo_funda_get() + (q == FUNDA_SIG ? 1 : NFUNDAS - 1);
                ygo_funda_set(n % NFUNDAS);
                xbox_MenuSaveSoon();
                break;
            default:
                if (q >= PUNTO0 && q < PUNTO0 + NMARCOS) {
                    ygo_funda_marco_set(q - PUNTO0);
                    xbox_MenuSaveSoon();
                }
                break;
            }
            cambio();
        }
    }
    LeaveCriticalSection(&s_cs);
    return 2;
}

void ygo_perfil_start(void)
{
    static int started;
    if (started)
        return;
    started = 1;
    InitializeCriticalSection(&s_cs);
    cargar();
    s_ok = 1;
    CloseHandle(CreateThread(NULL, 0, perfil_thread, NULL, 0, NULL));
}
