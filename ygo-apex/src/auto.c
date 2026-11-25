/*
 * auto.c - "AUTO" in the duel (2026-11-18, the user's list: "boton de auto
 * on/off para responder las de juego rapido"; mock-up approved: a smaller
 * copy of the player's name plate under it, "AUTO" in the game's letters and
 * a lamp, green when on).
 *
 * On: the game never asks the player "chain?" (sub_000A4EC0(activation, p),
 * the check sub_00082890 makes before sub_00082280 shows the Yes/No box,
 * answers 0 for the player, cartas.c). Off: as the game.
 * A click on the plate switches it; so does F2 / Configuracion ("auto_cadena",
 * saved with the other rows).
 *
 * Drawn as buscador.c / perfil.c: a layered click-through window of our own
 * over the game's, placed in game px (the name plate is the game's own:
 * Mods-Extras\auto\auto_on.png / auto_off.png, tools/auto_recursos.py), only
 * while the duel is being played (scene [0x656F88], state u32 0x657148 = 6).
 */
#define COBJMACROS
#include "mods_carpeta.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <wincodec.h>
#include "recomp/gen/recomp_types.h"
#include "kernel/nv2a_tex_pack.h"

#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "ole32.lib")

int xbox_MenuIsOpen(void);
void xbox_MenuSaveSoon(void);
int nv2a_d3d11_window_to_guest(int px, int py, int cw, int ch, float *gx, float *gy, float *dev);

/* the plate in game px (640 x 480): under the player's name plate */
#define A_X 28.0f
#define A_Y 275.0f
#define A_W 74.7f
#define A_H 24.9f

static volatile LONG s_on;              /* 1: AUTO on */
static volatile LONG s_version = 1;
static volatile LONG s_visible;

int ygo_auto_get(void) { return s_on ? 1 : 0; }           /* F2: "No", "Sí" */
void ygo_auto_set(int i) { InterlockedExchange(&s_on, i == 1); InterlockedIncrement(&s_version); }
/* 2026-11-20 (the user's pick, Master Duel "Switch + Hold"): while the middle
 * mouse button is held, AUTO lets the game ask "chain?" as if it were off. */
int ygo_auto_on(void)
{
    static ULONGLONG s_dijo;
    if (!s_on)
        return 0;
    if (GetAsyncKeyState(VK_MBUTTON) & 0x8000) {
        ULONGLONG t = GetTickCount64();
        if (t - s_dijo > 2000u) {
            fprintf(stderr, "[AUTO] boton del medio apretado: el juego pregunta encadenar\n");
            fflush(stderr);
        }
        s_dijo = t;
        return 0;
    }
    return 1;
}

/* 2026-11-19 (the user: "el boton de auto tiene que quedar igual que el
 * nombre cuando atacan las cartas y se ven los modelos 3D, tambien si se abre
 * el cementerio"): the name plates are the duel's two HUD widgets (scene
 * +0x34/+0x38, vtable 0x4294A8); sub_0004F440(show, animate) slides one in
 * (x 92, player's: [hud+0x48] == 0) or out (x -158). AUTO follows the
 * player's: shown only while it is in. */
void sub_0004F440_pad(void);
static volatile LONG s_placa = 1;       /* the player's name plate is in */
static volatile LONG s_escena_vista;

void sub_0004F440(void)
{
    if (MEM32(g_ecx + 0x48u) == 0u) {
        LONG on = (MEM32(g_esp + 4) & 0xFFu) != 0u;
        if (InterlockedExchange(&s_placa, on) != on) {
            fprintf(stderr, "[AUTO] la placa del nombre %s\n", on ? "entra" : "sale");
            fflush(stderr);
        }
    }
    sub_0004F440_pad();
}

/* 2026-11-19 07:2x (the user, capture: AUTO on and still "Tu adversario
 * Coloco un monstruo. ¿Deseas activar una Carta Magica de Juego Rapido o de
 * Trampa?"; 15 boxes in the 07:14 log after switching it on): that box is
 * sub_00082280(act, p) itself (text 0xC3E/0xC3F, sub_000880B0), its own state
 * machine in u8 0x66C078 (the caller sets it to 0 first). AUTO on, the
 * player asked (p 0) and the machine at its start: answered at once as the
 * game's own "nothing to chain" exit (0x8275F: ax 1; bit 7 of 0x66C079, "a
 * card chosen", clear), so the duel goes on without the box. */
void sub_00082280_pad(void);
void sub_00082280(void)
{
    uint32_t p = MEM32(g_esp + 8) & 1u;
    if (p == 0u && s_on && MEM8(0x0066C078u) == 0u) {
        MEM8(0x0066C079u) &= 0x7Fu;
        g_eax = (g_eax & 0xFFFF0000u) | 1u;
        g_esp += 4;                     /* ret */
        fprintf(stderr, "[AUTO] pregunta de cadena salteada (desde %08X)\n", MEM32(g_esp - 4));
        fflush(stderr);
        return;
    }
    sub_00082280_pad();
}

/* 2026-11-19 14:2x (the user, 14:15 log: AUTO on, a Trap set, "igual me
 * pedia activar la trampa como 4 veces por turno"; no "pregunta de cadena
 * salteada" in the whole duel): the box was not sub_00082280's. The game's
 * texts (default.xbe, tools/dialogos.py's table) give the two others:
 *  - sub_000BFE50 (from sub_000C0770, its machine in u8 0x66C078, the
 *    player in bit 12): "Tu adversario Coloco un monstruo." etc. (sub_000BFAD0)
 *    + 0xD01 "Deseas activar una Carta Magica de Juego Rapido o de Trampa?".
 *    Its step 0 asks sub_000BF9C0(p) whether p has a card that can answer
 *    (the Spell/Trap zones and the hand); 0 is its own "nothing" exit
 *    (0xC039E: machine 0xF0, no box). AUTO on, p 0: answered 0.
 *  - sub_000C07F0 (sub_000A1BF0, flag 8 of 0x66C494), its machine in u8
 *    0x66C07A >> 1: step 0 shows the same question or 0xC3E/0xC3F "El efecto
 *    %s esta activado. Resolverlo como parte de una Cadena?" (the card that
 *    was played, u16 0x66C0B8). AUTO on, at its step 0: its own "No" exit
 *    (0xC093F, also taken when the chain is full): sub_00085720(0xF055, 0, 0,
 *    0), ax 1. Its own wait at step 0 (0x65F8F8 bit 2 with summon step
 *    0x1E/0x1F/0x28/0x29/0x2A: ax 0, nothing shown) is left to it. */
void sub_000BF9C0_pad(void);
void sub_000BF9C0(void)
{
    if (s_on && (MEM32(g_esp + 4) & 1u) == 0u) {
        g_eax &= 0xFFFF0000u;
        g_esp += 4;                     /* ret */
        fprintf(stderr, "[AUTO] pregunta de Juego Rapido o Trampa salteada (desde %08X)\n", MEM32(g_esp - 4));
        fflush(stderr);
        return;
    }
    sub_000BF9C0_pad();
}

void sub_000C07F0_pad(void);
void sub_00085720(void);
void sub_000C07F0(void)
{
    uint8_t fd = MEM8(0x0065F8FDu);
    int espera = (MEM8(0x0065F8F8u) & 2u)
                 && (fd == 0x1Eu || fd == 0x1Fu || fd == 0x28u || fd == 0x29u || fd == 0x2Au);
    if (s_on && ((MEM8(0x0066C07Au) >> 1) & 0x7Fu) == 0u && !espera) {
        uint32_t sp = g_esp, sc = g_ecx, sd = g_edx;
        PUSH32(g_esp, 0u);
        PUSH32(g_esp, 0u);
        PUSH32(g_esp, 0u);
        PUSH32(g_esp, 0xF055u);
        PUSH32(g_esp, 0x000C094Fu);     /* where the game's own call returns */
        sub_00085720();
        g_esp = sp;
        g_ecx = sc;
        g_edx = sd;
        g_eax = (g_eax & 0xFFFF0000u) | 1u;
        g_esp += 4;                     /* ret */
        fprintf(stderr, "[AUTO] pregunta de activar salteada (%s, desde %08X)\n",
                (MEM8(0x0066C07Bu) & 1u) ? "efecto encadenable" : "Juego Rapido o Trampa", MEM32(g_esp - 4));
        fflush(stderr);
        return;
    }
    sub_000C07F0_pad();
}

/* 2026-11-19 07:2x (the user: "cuando un monstruo ataca o lo ves de cerca
 * sigue apareciendo en pantalla"; the 07:14 log has no "la placa del nombre
 * sale": the plates are not slid out there): mascota.c stamps the time each
 * frame a battle viewer is on screen (sub_00056FC0); AUTO hides meanwhile */
extern volatile ULONGLONG g_ygo_batalla_t;
extern volatile ULONGLONG g_ygo_botones_t;     /* raton.c: the round buttons / the card's info */

static int en_duelo(void)
{
    uint32_t esc = MEM32(0x00656F88u);
    if ((LONG)esc != s_escena_vista) {          /* a new duel: the plate starts in */
        InterlockedExchange(&s_escena_vista, (LONG)esc);
        InterlockedExchange(&s_placa, 1);
    }
    return esc != 0u && MEM32(0x00657148u) == 6u;
}

/* 2026-11-19 16:5x (the user: AUTO over the attacking monsters, and darker
 * than the name plate after the brightness guess: "mas te vale que sea la
 * definitiva"): AUTO is no longer a window over the game. The kit draws it
 * inside the game's frame with the player's name plate's own batch
 * (nv2a_pb_d3d11.c auto_placa: same tint, same slide, covered by whatever the
 * game draws later); here only its two pictures, on/off and "a duel is
 * being played" are handed over, and the kit says where it drew it. */
struct nv2a_auto_kit {
    const unsigned char *volatile px[2];
    volatile int w, h;
    volatile int activa;
    volatile int on;
    volatile float x, y, ancho, alto;
    volatile ULONGLONG visto;
    /* 2026-11-19 (the user: "al lado de auto podemos hacer un pequeno boton que
     * diga Off y si lo presionas On ... para activar o desactivar las
     * animaciones"): the Carta As switch right of AUTO (as_off.png / as_on.png;
     * on = carta_as.c's mode is not "No") */
    const unsigned char *volatile px2[2];
    volatile int w2, h2;
    volatile int on2;
    volatile float x2, y2, ancho2, alto2;
};
extern struct nv2a_auto_kit g_nv2a_auto;
int ygo_as_get(void);
void ygo_as_set(int i);

/* raton.c, a left click at game px (gx, gy): 1 when it was on AUTO as the
 * kit drew it in the last frames (switched; the game does not see it) */
int ygo_auto_raton(float gx, float gy)
{
    float x = g_nv2a_auto.x, y = g_nv2a_auto.y, w = g_nv2a_auto.ancho, h = g_nv2a_auto.alto;
    if (!en_duelo() || GetTickCount64() - g_nv2a_auto.visto > 300u)
        return 0;
    {
        /* the Carta As button: On (Siempre / Primera vez) <-> Off (No) */
        float x2 = g_nv2a_auto.x2, y2 = g_nv2a_auto.y2, w2 = g_nv2a_auto.ancho2, h2 = g_nv2a_auto.alto2;
        if (w2 > 0.f && gx >= x2 && gx < x2 + w2 && gy >= y2 && gy < y2 + h2) {
            int on = ygo_as_get() != 2;
            ygo_as_set(on ? 2 : 0);
            g_nv2a_auto.on2 = on ? 0 : 1;
            xbox_MenuSaveSoon();
            fprintf(stderr, "[AS] animaciones %s (clic en el botón)\n", on ? "apagadas" : "encendidas");
            fflush(stderr);
            return 1;
        }
    }
    if (w <= 0.f || gx < x || gx >= x + w || gy < y || gy >= y + h)
        return 0;
    ygo_auto_set(s_on ? 0 : 1);
    g_nv2a_auto.on = s_on ? 1 : 0;
    xbox_MenuSaveSoon();
    fprintf(stderr, "[AUTO] %s (clic en la placa)\n", s_on ? "encendido" : "apagado");
    fflush(stderr);
    return 1;
}

/* ---- the pictures ---------------------------------------------------------- */
typedef struct { unsigned char *p; int w, h; } img;
static const GUID k_clsid_wic = { 0xcacaf262, 0x9370, 0x4615, { 0xa1, 0x3b, 0x9f, 0x55, 0x39, 0xda, 0x4c, 0x0a } };
static const GUID k_iid_wic = { 0xec5ec8a9, 0xc395, 0x4314, { 0x9c, 0x77, 0x54, 0xd7, 0xa9, 0x35, 0xff, 0x70 } };
static const GUID k_pbgra = { 0x6fddc324, 0x4e03, 0x4bfe, { 0xb1, 0x85, 0x3d, 0x77, 0x76, 0x8d, 0xc9, 0x10 } };
static IWICImagingFactory *s_wic;
static img s_img[2];                    /* off, on */
static img s_boton[2];                  /* the Carta As button: off, on */

static int leer(const wchar_t *nombre, img *m)
{
    wchar_t p[MAX_PATH], *barra;
    IWICBitmapDecoder *dec = NULL;
    IWICBitmapFrameDecode *fr = NULL;
    IWICBitmapSource *cv = NULL;
    UINT W = 0, H = 0;
    int ok = 0;
    DWORD n = GetModuleFileNameW(NULL, p, MAX_PATH);
    if (!n || n >= MAX_PATH || !(barra = wcsrchr(p, L'\\')))
        return 0;
    *barra = 0;
    if (wcslen(p) + wcslen(nombre) + 40 >= MAX_PATH)
        return 0;
    wcscat(p, ygo_mods_w());
    wcscat(p, L"\\auto\\");
    wcscat(p, nombre);
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
        fprintf(stderr, "[AUTO] no se pudo leer %ls\n", p);
    return ok;
}

/* the WIC pictures are premultiplied BGRA; the kit wants straight RGBA */
static unsigned char *a_rgba(const img *m)
{
    size_t i, n = (size_t)m->w * (size_t)m->h;
    unsigned char *o = (unsigned char *)malloc(n * 4u);
    if (!o)
        return NULL;
    for (i = 0; i < n; i++) {
        const unsigned char *q = m->p + 4u * i;
        unsigned a = q[3];
        o[4u * i + 0] = (unsigned char)(a ? (q[2] * 255u + a / 2u) / a : 0u);
        o[4u * i + 1] = (unsigned char)(a ? (q[1] * 255u + a / 2u) / a : 0u);
        o[4u * i + 2] = (unsigned char)(a ? (q[0] * 255u + a / 2u) / a : 0u);
        o[4u * i + 3] = (unsigned char)a;
    }
    return o;
}

static DWORD WINAPI auto_thread(LPVOID unused)
{
    int dicho = -1;
    (void)unused;
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    leer(L"auto_off.png", &s_img[0]);
    leer(L"auto_on.png", &s_img[1]);
    if (s_img[0].p && s_img[1].p && s_img[0].w == s_img[1].w && s_img[0].h == s_img[1].h) {
        g_nv2a_auto.w = s_img[0].w;
        g_nv2a_auto.h = s_img[0].h;
        g_nv2a_auto.px[0] = a_rgba(&s_img[0]);
        g_nv2a_auto.px[1] = a_rgba(&s_img[1]);
    }
    leer(L"as_off.png", &s_boton[0]);
    leer(L"as_on.png", &s_boton[1]);
    if (s_boton[0].p && s_boton[1].p && s_boton[0].w == s_boton[1].w && s_boton[0].h == s_boton[1].h) {
        g_nv2a_auto.w2 = s_boton[0].w;
        g_nv2a_auto.h2 = s_boton[0].h;
        g_nv2a_auto.px2[0] = a_rgba(&s_boton[0]);
        g_nv2a_auto.px2[1] = a_rgba(&s_boton[1]);
    }
    fprintf(stderr, "[AUTO] listo (%s), dibujado por el kit dentro del cuadro\n", s_on ? "encendido" : "apagado");
    fflush(stderr);
    for (;;) {
        int duelo = en_duelo();
        /* the limited cards' Master Duel circle (cartas_mod.py: the pack's
         * variant entries) outside the duel only (2026-11-18) */
        nv2a_tex_pack_set_variant(MEM32(0x00656F88u) == 0u);
        g_nv2a_auto.on = s_on ? 1 : 0;
        g_nv2a_auto.on2 = ygo_as_get() != 2;
        g_nv2a_auto.activa = duelo;
        if (duelo != dicho) {
            dicho = duelo;
            InterlockedExchange(&s_visible, duelo);
        }
        Sleep(50);
    }
}

void ygo_auto_start(void)
{
    static int started;
    if (started)
        return;
    started = 1;
    CloseHandle(CreateThread(NULL, 0, auto_thread, NULL, 0, NULL));
}
