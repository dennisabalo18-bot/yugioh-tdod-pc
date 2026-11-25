/* Carta As (idea 3 de Master Duel, 2026-11-19; el usuario eligió la muestra
 * "Carga de guerra" con los Goblins: "si empezemos con los goblins").
 *
 * Cuando tu mascota es una carta y la llamás boca arriba (o el rival llama la
 * de su mascota), el duelo espera mientras corre una animación de ~3,6 s:
 * se oscurece el campo, la carta (la del juego en HD, la de la Biblioteca)
 * cae de golpe al centro: sacudida, polvo, piedras, grietas encendidas; tres
 * golpes de tambor; se encienden los ojos (los Goblins) y su nombre con la
 * letra de títulos del juego; la carta baja a su casilla con un anillo.
 *
 * - Cuándo: en el despacho de las llamadas (sub_00095A30; tipo = (u16
 *   0x66BA8E >> 2) & 7, estado = >> 5 & 0x7F; carta k u16 0x66BA80, jugador
 *   u32 0x66BA7C bit 0, boca arriba bit 0x4000), en el estado de los
 *   disparos "al ser llamada", con la carta ya en su casilla: tipo 1 (Llamada
 *   Normal) estado 2, tipo 2 (por Sacrificio) estado 4. Las demás (Volteo,
 *   Especial) se anotan en el registro para sumarlas después.
 * - El duelo espera: mientras corre, el despacho devuelve 1 ("hay una
 *   llamada en curso", lo que el llamador 0x8404D lee) sin avanzar la máquina.
 * - Qué carta: la del modelo de la mascota de ese lado (mascota.c), por la
 *   tabla carta -> modelo del juego (0x44FBB0, pares u16 id, modelo).
 * - El dibujo: un hilo arma cada cuadro en la CPU (el tiempo en ms reales: a
 *   60 o 120 FPS dura lo mismo), una imagen RGBA premultiplicada del tamaño
 *   de la vista; el kit la pone sobre el cuadro (ONE, INV_SRC_ALPHA: el polvo
 *   y la carta tapan, la luz se suma) y mueve el cuadro para la sacudida.
 * - F2 / Configuración "Carta As": Siempre / Primera vez por duelo / No.
 */
#define COBJMACROS
#include "mods_carpeta.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <wincodec.h>

#include "recomp/gen/recomp_types.h"
#include "fuente.h"

int ft_ancho(const wchar_t *s, int h);
void ft_dibujar(unsigned char *bits, int W, int H, const RECT *rc, const wchar_t *s, int h, unsigned canales);
int ygo_mascota_modelo_lado(int lado);      /* mascota.c */

/* kit (nv2a_pb_d3d11.c) */
extern const unsigned char *volatile g_nv2a_as_px;   /* RGBA premultiplied, w x h; NULL: none */
extern volatile int g_nv2a_as_w, g_nv2a_as_h;
extern volatile unsigned g_nv2a_as_ver;
extern volatile float g_nv2a_as_mueve[2];            /* the frame moved (parts of the view) */
extern volatile int g_nv2a_vista_w, g_nv2a_vista_h;  /* the view's size on screen */
/* 2026-11-19 (the user: "se veia de mala calidad la imagen de la carta de
 * cerca"): the card is the kit's, drawn from its own texture at the screen's
 * size (mipmapped) between two pictures: behind (the dim, the light behind
 * it) and in front (dust, sparks, the eyes, the name) */
extern const unsigned char *volatile g_nv2a_as_fondo_px; /* behind the card, RGBA premultiplied */
extern const unsigned char *volatile g_nv2a_as_carta_px; /* the card, BGRA premultiplied */
extern volatile int g_nv2a_as_carta_w, g_nv2a_as_carta_h;
extern volatile unsigned g_nv2a_as_carta_ver;             /* a new card */
extern volatile float g_nv2a_as_carta[12];  /* canvas px -> card uv (3x3), alpha, canvas w, h */
/* the name's place for the kit: its alpha ([9]) stays 0, no name */
extern volatile float g_nv2a_as_tit[12];

#define T_IMAGEN 0x005FE692u
#define T_DATOS 0x005FEF78u            /* k -> type 20-24, attribute 29-31 (buscador.c) */
#define TXT_NOMBRE 849u
#define CASILLA(p, z) ((uint32_t)(p) * 0x13F8u + (uint32_t)(z) * 0x9Cu + 0x0065D0C8u)
#define DUR 3.6f
#define SUELTA 3.25f                    /* the duel goes on: the card at the game's start */

/* ---- the option ---------------------------------------------------------- */
static volatile LONG s_modo;            /* 0 Siempre, 1 Primera vez por duelo, 2 No */
int ygo_as_get(void) { return (int)s_modo; }
void ygo_as_set(int i) { InterlockedExchange(&s_modo, i >= 0 && i <= 2 ? i : 0); }
static uint32_t s_vistas[2][8];         /* the cards shown this duel, per side */
static int s_nvistas[2];
void ygo_as_duelo_nuevo(void)
{
    s_nvistas[0] = s_nvistas[1] = 0;
}

/* ---- the card's picture --------------------------------------------------- */
typedef struct { unsigned char *p; int w, h; } img;  /* premultiplied BGRA */
static const GUID k_clsid_wic = { 0xcacaf262, 0x9370, 0x4615, { 0xa1, 0x3b, 0x9f, 0x55, 0x39, 0xda, 0x4c, 0x0a } };
static const GUID k_iid_wic = { 0xec5ec8a9, 0xc395, 0x4314, { 0x9c, 0x77, 0x54, 0xd7, 0xa9, 0x35, 0xff, 0x70 } };
static const GUID k_pbgra = { 0x6fddc324, 0x4e03, 0x4bfe, { 0xb1, 0x85, 0x3d, 0x77, 0x76, 0x8d, 0xc9, 0x10 } };

static int leer_jpg(const wchar_t *ruta, img *m)
{
    IWICImagingFactory *wic = NULL;
    IWICBitmapDecoder *dec = NULL;
    IWICBitmapFrameDecode *fr = NULL;
    IWICBitmapSource *cv = NULL;
    UINT w = 0, h = 0;
    int ok = 0;
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(CoCreateInstance(&k_clsid_wic, NULL, CLSCTX_INPROC_SERVER, &k_iid_wic, (void **)&wic))
            || FAILED(IWICImagingFactory_CreateDecoderFromFilename(wic, ruta, NULL, GENERIC_READ,
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
    } else {
        free(m->p);
        m->p = NULL;
    }
fin:
    if (cv) IWICBitmapSource_Release(cv);
    if (fr) IWICBitmapFrameDecode_Release(fr);
    if (dec) IWICBitmapDecoder_Release(dec);
    if (wic) IWICImagingFactory_Release(wic);
    return ok;
}

/* Mods-Extras\perfil\cartas\<language>\g\<ID>.jpg (tools/perfil_cartas.py) */
static int carta_hd(uint32_t id, img *m)
{
    static const wchar_t *const k_dir[6] = { L"en", L"en", L"gr", L"fr", L"it", L"sp" };
    uint32_t lang = MEM32(0x005FC2C0u);
    wchar_t p[MAX_PATH], *barra;
    if (lang >= 6u)
        lang = 5u;
    if (!GetModuleFileNameW(NULL, p, MAX_PATH) || !(barra = wcsrchr(p, L'\\')) || wcslen(p) + 80 >= MAX_PATH)
        return 0;
    _snwprintf(barra, MAX_PATH - (barra - p), L"%ls\\perfil\\cartas\\%ls\\g\\%u.jpg", ygo_mods_w(), k_dir[lang], id);
    p[MAX_PATH - 1] = 0;
    if (leer_jpg(p, m))
        return 1;
    fprintf(stderr, "[AS] sin la carta grande %ls\n", p);
    return 0;
}

/* the card's name in the game's language */
static void nombre_carta(uint32_t k, wchar_t *out, int cap)
{
    uint32_t lang = MEM32(0x005FC2C0u), tab, base, p, n = TXT_NOMBRE + k;
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

/* the game's card -> model table */
static int modelo_carta(uint32_t id)
{
    uint32_t i;
    for (i = 0; i < 2000u; i++) {
        uint16_t a = MEM16(0x0044FBB0u + 4u * i), b = MEM16(0x0044FBB2u + 4u * i);
        if (a < 4000u || a > 5300u)
            return -1;
        if (a == id)
            return (int)b;
    }
    return -1;
}

/* ---- per card: the eyes that light up (card 0..1) -------------------------- */
typedef struct { uint16_t id; int n; float xy[12][2]; } ojos_t;
static const ojos_t k_ojos[] = {
    /* Duende travieso fuerza de ataque (Goblin Attack Force): the front three,
     * the shadows behind (measured on the x4 card, 2026-11-19) */
    { 5145, 10, { { 0.498f, 0.396f }, { 0.543f, 0.396f }, { 0.225f, 0.47f }, { 0.289f, 0.47f }, { 0.674f, 0.565f },
                  { 0.738f, 0.565f }, { 0.40f, 0.365f }, { 0.43f, 0.365f }, { 0.629f, 0.36f }, { 0.658f, 0.36f } } },
};

/* ---- the running one ------------------------------------------------------ */
static volatile LONG s_activo;          /* drawing */
static volatile LONG s_espera;          /* the duel waits */
static volatile LONG s_entregada;       /* the game's own placement took the card */
/* 2026-11-19 (the user: in the Perfil a "Ver Invocación" button): a preview,
 * not a summon: nothing waits for it, no handoff (the card grows and fades) */
static volatile LONG s_vista;
/* 2026-11-19 (the user: "se ve el fondo del menu principal medio segundo"): no gap
 * between the Perfil's windows and the animation, either way: the Perfil hides
 * only once the kit has drawn the first picture (ygo_as_tapa), and at the end
 * the last picture (the Perfil's own, nothing over it) stays until the Perfil
 * says it is back (ygo_as_perfil_vuelto) */
extern volatile unsigned g_nv2a_as_dibujado;
static unsigned s_ver0;
static volatile LONG s_fin, s_vuelto;
static float s_carta_a = 1;            /* the kit's card's alpha */
static unsigned char *s_pf_px;         /* the Perfil's picture (BGRA premultiplied, its window's size) */
static int s_pf_w, s_pf_h;
static unsigned char *s_vfondo;        /* it at the canvas' size: the preview's background */
static ULONGLONG s_t0;
static int s_lado, s_zona;
static uint32_t s_id;
static img s_carta;
static wchar_t s_nombre[128];
static const ojos_t *s_ojos;
static HANDLE s_hilo;

/* the canvas */
static int OW, OH;
static float *s_L;                      /* light, added (rgb) */
static float *s_O;                      /* premultiplied rgba */
static unsigned char *s_px[2], *s_fondo[2];
static int s_pub, s_carta_vis;
static float S;                         /* design (1280 x 720) to canvas */

#define DX(x) (((x) - 640.f) * S + (float)OW * 0.5f)
#define DY(y) (((y) - 360.f) * S + (float)OH * 0.5f)

static float eio(float t) { t = t < 0 ? 0 : t > 1 ? 1 : t; return 0.5f - 0.5f * cosf(3.14159265f * t); }
static float eout(float t, float p) { t = t < 0 ? 0 : t > 1 ? 1 : t; return 1.f - powf(1.f - t, p); }
static float ventana(float t, float a, float b, float fi, float fo) { return eio((t - a) / fi) * (1.f - eio((t - (b - fo)) / fo)); }

static uint32_t s_rnd = 12345u;
static float rnd(void) { s_rnd = s_rnd * 1664525u + 1013904223u; return (float)(s_rnd >> 8) / 16777216.f; }
static float rr(float a, float b) { return a + (b - a) * rnd(); }

/* ---- drawing on the canvas -------------------------------------------------- */
static void luz_punto(float x, float y, float r, float cr, float cg, float cb, float k)
{
    int x0 = (int)(x - 3 * r), x1 = (int)(x + 3 * r) + 1, y0 = (int)(y - 3 * r), y1 = (int)(y + 3 * r) + 1, i, j;
    float inv = 1.f / (2.f * r * r);
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > OW) x1 = OW;
    if (y1 > OH) y1 = OH;
    for (j = y0; j < y1; j++)
        for (i = x0; i < x1; i++) {
            float dx = (float)i - x, dy = (float)j - y, g = expf(-(dx * dx + dy * dy) * inv) * k;
            float *L = s_L + 3 * ((size_t)j * OW + i);
            L[0] += cr * g; L[1] += cg * g; L[2] += cb * g;
        }
}

/* an ellipse of light (rx, ry) */
static void luz_elipse(float x, float y, float rx, float ry, float cr, float cg, float cb, float k)
{
    int x0 = (int)(x - 2.5f * rx), x1 = (int)(x + 2.5f * rx) + 1, y0 = (int)(y - 2.5f * ry), y1 = (int)(y + 2.5f * ry) + 1, i, j;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > OW) x1 = OW;
    if (y1 > OH) y1 = OH;
    for (j = y0; j < y1; j++)
        for (i = x0; i < x1; i++) {
            float dx = ((float)i - x) / rx, dy = ((float)j - y) / ry, g = expf(-(dx * dx + dy * dy)) * k;
            float *L = s_L + 3 * ((size_t)j * OW + i);
            L[0] += cr * g; L[1] += cg * g; L[2] += cb * g;
        }
}

/* a ring (ellipse rx, ry; thickness w) */
static void luz_anillo(float x, float y, float rx, float ry, float w, float cr, float cg, float cb, float k)
{
    int x0 = (int)(x - rx - 3 * w), x1 = (int)(x + rx + 3 * w) + 1, y0 = (int)(y - ry - 3 * w), y1 = (int)(y + ry + 3 * w) + 1, i, j;
    float irx2, iry2;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > OW) x1 = OW;
    if (y1 > OH) y1 = OH;
    if (rx < 1 || ry < 1)
        return;
    irx2 = 1.f / (rx * rx); iry2 = 1.f / (ry * ry);
    for (j = y0; j < y1; j++)
        for (i = x0; i < x1; i++) {
            /* 2026-11-19: the distance to the ellipse in px (its level's value over
             * its gradient), so the ring is as thick all round and fades inside its box */
            float X = (float)i - x, Y = (float)j - y, q = sqrtf(X * X * irx2 + Y * Y * iry2), gx, gy, gr, d, g;
            float *L;
            if (q < 1e-3f)
                continue;
            gx = X * irx2; gy = Y * iry2;
            gr = sqrtf(gx * gx + gy * gy) / q;
            d = (q - 1.f) / (gr > 1e-6f ? gr : 1e-6f) / w;
            if (d * d > 9.f)
                continue;
            g = expf(-d * d) * k;
            if (g < 0.002f)
                continue;
            L = s_L + 3 * ((size_t)j * OW + i);
            L[0] += cr * g; L[1] += cg * g; L[2] += cb * g;
        }
}

/* a segment of light (core w) */
static void luz_linea(float ax, float ay, float bx, float by, float w, float cr, float cg, float cb, float k)
{
    int x0 = (int)(fminf(ax, bx) - 3 * w), x1 = (int)(fmaxf(ax, bx) + 3 * w) + 1;
    int y0 = (int)(fminf(ay, by) - 3 * w), y1 = (int)(fmaxf(ay, by) + 3 * w) + 1, i, j;
    float vx = bx - ax, vy = by - ay, l2 = vx * vx + vy * vy + 1e-6f;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > OW) x1 = OW;
    if (y1 > OH) y1 = OH;
    for (j = y0; j < y1; j++)
        for (i = x0; i < x1; i++) {
            float px = (float)i - ax, py = (float)j - ay, t = (px * vx + py * vy) / l2, dx, dy, g;
            float *L;
            t = t < 0 ? 0 : t > 1 ? 1 : t;
            dx = px - t * vx; dy = py - t * vy;
            if (dx * dx + dy * dy > 9.f * w * w)   /* exp(-9): nothing (a long slanted line's box is mostly empty) */
                continue;
            g = expf(-(dx * dx + dy * dy) / (w * w)) * k;
            if (g < 0.002f)
                continue;
            L = s_L + 3 * ((size_t)j * OW + i);
            L[0] += cr * g; L[1] += cg * g; L[2] += cb * g;
        }
}

/* covering (normal blending): a soft disc */
static void tapa_disco(float x, float y, float r, float cr, float cg, float cb, float a)
{
    int x0 = (int)(x - r), x1 = (int)(x + r) + 1, y0 = (int)(y - r), y1 = (int)(y + r) + 1, i, j;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > OW) x1 = OW;
    if (y1 > OH) y1 = OH;
    for (j = y0; j < y1; j++)
        for (i = x0; i < x1; i++) {
            float dx = ((float)i - x) / r, dy = ((float)j - y) / r, d = dx * dx + dy * dy, al;
            float *o;
            if (d >= 1.f)
                continue;
            al = a * (1.f - d) * (1.f - d);
            o = s_O + 4 * ((size_t)j * OW + i);
            o[0] = cr * al + o[0] * (1 - al); o[1] = cg * al + o[1] * (1 - al);
            o[2] = cb * al + o[2] * (1 - al); o[3] = al + o[3] * (1 - al);
        }
}

/* the card, turned in 3D (degrees) at (cx, cy), h canvas px tall: its
 * corners in q, the homography canvas px -> uv in s_hm; drawn on the CPU only
 * for the test outside the game (s_carta_cpu), else by the kit */
static double s_hm[9];
static int s_carta_cpu;
static void carta_3d(float cx, float cy, float h, float yaw, float pitch, float alfa, float q[4][2])
{
    float w = h * (float)s_carta.w / (float)s_carta.h, f = 1500.f * S;
    float cyw = cosf(yaw * 0.0174533f), syw = sinf(yaw * 0.0174533f);
    float cp = cosf(pitch * 0.0174533f), sp = sinf(pitch * 0.0174533f);
    static const float k_uv[4][2] = { { -0.5f, -0.5f }, { 0.5f, -0.5f }, { 0.5f, 0.5f }, { -0.5f, 0.5f } };
    double A[8][9], hm[9];
    int k, i, j, x0 = 1 << 30, x1 = -1, y0 = 1 << 30, y1 = -1;
    for (k = 0; k < 4; k++) {
        float x = k_uv[k][0] * w, y = k_uv[k][1] * h, z = 0, s;
        z = x * syw; x = x * cyw;
        { float y2 = y * cp - z * sp; z = y * sp + z * cp; y = y2; }
        s = f / (f + z);
        q[k][0] = cx + x * s;
        q[k][1] = cy + y * s;
    }
    /* the homography canvas -> card uv (0..1) */
    for (k = 0; k < 4; k++) {
        double X = q[k][0], Y = q[k][1], u = k_uv[k][0] + 0.5, v = k_uv[k][1] + 0.5;
        double r1[9] = { X, Y, 1, 0, 0, 0, -u * X, -u * Y, u }, r2[9] = { 0, 0, 0, X, Y, 1, -v * X, -v * Y, v };
        memcpy(A[2 * k], r1, sizeof r1);
        memcpy(A[2 * k + 1], r2, sizeof r2);
        if ((int)X < x0) x0 = (int)X;
        if ((int)X + 1 > x1) x1 = (int)X + 1;
        if ((int)Y < y0) y0 = (int)Y;
        if ((int)Y + 1 > y1) y1 = (int)Y + 1;
    }
    for (i = 0; i < 8; i++) {           /* Gauss-Jordan */
        int p = i;
        for (j = i + 1; j < 8; j++)
            if (fabs(A[j][i]) > fabs(A[p][i]))
                p = j;
        if (fabs(A[p][i]) < 1e-12)
            return;
        if (p != i)
            for (k = 0; k < 9; k++) { double t = A[i][k]; A[i][k] = A[p][k]; A[p][k] = t; }
        for (j = 0; j < 8; j++)
            if (j != i) {
                double m = A[j][i] / A[i][i];
                for (k = i; k < 9; k++)
                    A[j][k] -= m * A[i][k];
            }
    }
    for (i = 0; i < 8; i++)
        hm[i] = A[i][8] / A[i][i];
    hm[8] = 1;
    memcpy(s_hm, hm, sizeof s_hm);
    if (!s_carta_cpu)
        return;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > OW) x1 = OW;
    if (y1 > OH) y1 = OH;
    for (j = y0; j < y1; j++)
        for (i = x0; i < x1; i++) {
            double X = i + 0.5, Y = j + 0.5, d = hm[6] * X + hm[7] * Y + 1;
            float u = (float)((hm[0] * X + hm[1] * Y + hm[2]) / d), v = (float)((hm[3] * X + hm[4] * Y + hm[5]) / d);
            float fx, fy, c[4] = { 0, 0, 0, 0 }, cov, al, *o;
            int ix, iy, n;
            if (u < -0.01f || u > 1.01f || v < -0.01f || v > 1.01f)
                continue;
            /* the edges smooth (a px of the canvas) */
            cov = fminf(fminf(u, 1 - u) * (float)(x1 - x0) * 0.9f + 0.5f, fminf(v, 1 - v) * (float)(y1 - y0) * 0.9f + 0.5f);
            cov = cov < 0 ? 0 : cov > 1 ? 1 : cov;
            fx = u * (float)s_carta.w - 0.5f; fy = v * (float)s_carta.h - 0.5f;
            ix = (int)floorf(fx); iy = (int)floorf(fy); fx -= ix; fy -= iy;
            for (n = 0; n < 4; n++) {
                int xx = ix + (n & 1), yy = iy + (n >> 1);
                float wg = ((n & 1) ? fx : 1 - fx) * ((n >> 1) ? fy : 1 - fy);
                const unsigned char *p;
                xx = xx < 0 ? 0 : xx >= s_carta.w ? s_carta.w - 1 : xx;
                yy = yy < 0 ? 0 : yy >= s_carta.h ? s_carta.h - 1 : yy;
                p = s_carta.p + 4 * ((size_t)yy * s_carta.w + xx);
                c[0] += wg * p[2]; c[1] += wg * p[1]; c[2] += wg * p[0]; c[3] += wg * p[3];
            }
            al = c[3] / 255.f * cov * alfa;
            o = s_O + 4 * ((size_t)j * OW + i);
            /* premultiplied source */
            o[0] = c[0] / 255.f * cov * alfa + o[0] * (1 - al);
            o[1] = c[1] / 255.f * cov * alfa + o[1] * (1 - al);
            o[2] = c[2] / 255.f * cov * alfa + o[2] * (1 - al);
            o[3] = al + o[3] * (1 - al);
        }
}

/* ---- particles ---------------------------------------------------------------- */
typedef struct { float x, y, vx, vy, t, vida, tam, g, drag, rot, vr; int tipo; float c[3]; } part_t;
enum { CHISPA, BRASA, POLVO, ROCA };
#define NPART 1400
static part_t s_p[NPART];
static int s_np;

static void emitir(int tipo, float x, float y, float vx, float vy, float vida, float tam, float g, float drag,
                   float r, float gg, float b)
{
    part_t *p;
    if (s_np >= NPART)
        return;
    p = &s_p[s_np++];
    p->tipo = tipo; p->x = x; p->y = y; p->vx = vx; p->vy = vy; p->t = 0; p->vida = vida; p->tam = tam;
    p->g = g; p->drag = drag; p->c[0] = r; p->c[1] = gg; p->c[2] = b; p->rot = rr(0, 6.28f); p->vr = rr(-6, 6);
}

static void paso(float dt)
{
    int i, n = 0;
    for (i = 0; i < s_np; i++) {
        part_t *p = &s_p[i];
        p->vx *= 1 - p->drag * dt;
        p->vy = p->vy * (1 - p->drag * dt) + p->g * dt;
        p->x += p->vx * dt; p->y += p->vy * dt; p->t += dt; p->rot += p->vr * dt;
        if (p->t < p->vida)
            s_p[n++] = *p;
    }
    s_np = n;
}

/* ---- the cracks: made once (design px, from the impact) --------------------------- */
#define NGRIETA 11
#define PGRIETA 33
static float s_gr[NGRIETA][PGRIETA][2];

static void grietas_hacer(void)
{
    int g, n, k, d;
    for (g = 0; g < NGRIETA; g++) {
        float a = rr(0, 6.2832f), ln = rr(180, 420), pts[PGRIETA][2], desv = 30;
        int cnt = 2;
        pts[0][0] = 0; pts[0][1] = 0;
        pts[1][0] = cosf(a) * ln; pts[1][1] = sinf(a) * ln * 0.3f;
        for (d = 0; d < 5; d++) {
            float nv[PGRIETA][2];
            int m = 0;
            for (k = 0; k < cnt - 1; k++) {
                float mx = (pts[k][0] + pts[k + 1][0]) * 0.5f, my = (pts[k][1] + pts[k + 1][1]) * 0.5f;
                float nx = -(pts[k + 1][1] - pts[k][1]), ny = pts[k + 1][0] - pts[k][0], l = sqrtf(nx * nx + ny * ny) + 1e-6f, o = rr(-desv, desv);
                nv[m][0] = pts[k][0]; nv[m][1] = pts[k][1]; m++;
                nv[m][0] = mx + nx / l * o; nv[m][1] = my + ny / l * o; m++;
            }
            nv[m][0] = pts[cnt - 1][0]; nv[m][1] = pts[cnt - 1][1]; m++;
            memcpy(pts, nv, sizeof(float) * 2 * m);
            cnt = m;
            desv *= 0.55f;
        }
        for (n = 0; n < PGRIETA; n++) {
            s_gr[g][n][0] = pts[n < cnt ? n : cnt - 1][0];
            s_gr[g][n][1] = pts[n < cnt ? n : cnt - 1][1];
        }
    }
}

/* 2026-11-21: the name under the card (titulo, titulo_hacer, titulo_gpu)
 * removed: the user asked for no name on 11-19, and it was still made each
 * time, invisible (alpha 0) */

/* ---- bloom: the light, blurred at a quarter, added again -------------------------- */
static void caja(float *a, int w, int h, int r, int horiz)
{
    static float tmp[4096 * 3];
    int n = horiz ? w : h, m = horiz ? h : w, i, j, c;
    if (n > 4096)
        return;
    for (j = 0; j < m; j++) {
        float s[3] = { 0, 0, 0 };
        for (i = -r; i <= r; i++) {
            int ii = i < 0 ? 0 : i >= n ? n - 1 : i;
            float *p = horiz ? a + 3 * ((size_t)j * w + ii) : a + 3 * ((size_t)ii * w + j);
            for (c = 0; c < 3; c++) s[c] += p[c];
        }
        for (i = 0; i < n; i++) {
            int ia = i - r - 1, ib = i + r;
            float *pa, *pb;
            for (c = 0; c < 3; c++) tmp[3 * i + c] = s[c] / (float)(2 * r + 1);
            ia = ia < 0 ? 0 : ia; ib = ib >= n ? n - 1 : ib;
            pa = horiz ? a + 3 * ((size_t)j * w + ia) : a + 3 * ((size_t)ia * w + j);
            pb = horiz ? a + 3 * ((size_t)j * w + ib) : a + 3 * ((size_t)ib * w + j);
            for (c = 0; c < 3; c++) s[c] += pb[c] - pa[c];
        }
        for (i = 0; i < n; i++) {
            float *p = horiz ? a + 3 * ((size_t)j * w + i) : a + 3 * ((size_t)i * w + j);
            for (c = 0; c < 3; c++) p[c] = tmp[3 * i + c];
        }
    }
}

static float *s_bq;
static void bloom(void)
{
    int bw = OW / 4, bh = OH / 4, i, j, c, pase;
    if (!s_bq)
        return;
    for (j = 0; j < bh; j++)
        for (i = 0; i < bw; i++)
            for (c = 0; c < 3; c++) {
                float s = 0;
                int a, b;
                for (b = 0; b < 4; b++)
                    for (a = 0; a < 4; a++)
                        s += s_L[3 * ((size_t)(4 * j + b) * OW + 4 * i + a) + c];
                s = s / 16.f - 0.25f;
                s_bq[3 * ((size_t)j * bw + i) + c] = s > 0 ? s : 0;
            }
    for (pase = 0; pase < 3; pase++) {
        caja(s_bq, bw, bh, 3, 1);
        caja(s_bq, bw, bh, 3, 0);
    }
    for (j = 0; j < OH; j++)
        for (i = 0; i < OW; i++) {
            float fx = (float)i / 4.f - 0.5f, fy = (float)j / 4.f - 0.5f;
            int ix = (int)floorf(fx), iy = (int)floorf(fy);
            float ax = fx - ix, ay = fy - iy;
            int x0 = ix < 0 ? 0 : ix >= bw ? bw - 1 : ix, x1 = ix + 1 >= bw ? bw - 1 : ix + 1 < 0 ? 0 : ix + 1;
            int y0 = iy < 0 ? 0 : iy >= bh ? bh - 1 : iy, y1 = iy + 1 >= bh ? bh - 1 : iy + 1 < 0 ? 0 : iy + 1;
            for (c = 0; c < 3; c++) {
                float v = (s_bq[3 * ((size_t)y0 * bw + x0) + c] * (1 - ax) + s_bq[3 * ((size_t)y0 * bw + x1) + c] * ax) * (1 - ay)
                        + (s_bq[3 * ((size_t)y1 * bw + x0) + c] * (1 - ax) + s_bq[3 * ((size_t)y1 * bw + x1) + c] * ax) * ay;
                s_L[3 * ((size_t)j * OW + i) + c] += v * 1.6f;
            }
        }
}

/* ---- one frame of "Carga de guerra" (t in s) ------------------------------------- */
static const float k_golpes[4] = { 0.62f, 1.25f, 1.55f, 1.85f };
static float s_sim;                     /* the particles' time */
static int s_golpe_hecho;

/* ---- each card's style (2026-11-19, the user: "seguí con las demás, de 10 en
 * 10") -------------------------------------------------------------------------
 * CARGA: "Carga de guerra" (the Goblins: falls, stomps, cracks, rocks).
 * RAYO: "Rayo de destrucción" (the Blue-Eyes sample the user liked): the card
 * comes in fast from the right, a shock ring and speed lines, electricity on
 * its edges; with a mouth (boca u, v on the card, 0..1) it charges and fires
 * a beam that way (dir, degrees, y down); without one, lightning from the sky
 * strikes it. col: the style's light. Cards not listed: CARGA, earth. */
enum { CARGA, RAYO, DESCENSO, RITUAL, MAGO };
typedef struct { uint16_t id; uint8_t estilo; float col[3]; float boca[2]; float dir; } estilo_t;
static const estilo_t k_estilos[] = {
    { 5145, CARGA, { 1.0f, 0.55f, 0.18f }, { -1, -1 }, 0 },       /* Goblin Attack Force */
    /* mouths measured on the x4 cards (2026-11-19) */
    { 4007, RAYO, { 0.55f, 0.8f, 1.0f }, { 0.27f, 0.43f }, 172 }, /* Blue-Eyes: its white lightning, to the left */
    { 4088, RAYO, { 1.0f, 0.35f, 0.15f }, { -1, -1 }, 0 },        /* Red-Eyes: red lightning from the sky (its beam missed the mouth) */
    /* MAGO, the staff's tip measured on the x4 cards */
    { 4041, MAGO, { 0.72f, 0.35f, 1.0f }, { 0.66f, 0.58f }, 0 },   /* Dark Magician */
    { 5165, MAGO, { 0.62f, 0.4f, 1.0f }, { 0.72f, 0.66f }, 0 },    /* Dark Magician (the other art) */
    { 4766, MAGO, { 1.0f, 0.45f, 0.85f }, { 0.5f, 0.68f }, 0 },    /* Dark Magician Girl: pink */
    { 4728, MAGO, { 0.9f, 0.3f, 0.55f }, { 0.5f, 0.45f }, 0 },     /* Magician of Black Chaos: crimson violet */
    { 4386, DESCENSO, { 0.7f, 0.85f, 1.0f }, { -1, -1 }, 0 },     /* Blue-Eyes Ultimate: from heaven (its beam "queda mal") */
    { 4223, RAYO, { 0.85f, 0.4f, 1.0f }, { 0.5f, 0.444f }, 100 },  /* B. Skull Dragon: down at us */
    { 4719, RAYO, { 1.0f, 0.42f, 0.15f }, { 0.225f, 0.41f }, 190 }, /* Meteor B. Dragon */
    { 4392, RAYO, { 0.5f, 1.0f, 0.55f }, { 0.175f, 0.513f }, 180 }, /* Harpie's Pet Dragon */
    { 4075, RAYO, { 1.0f, 0.6f, 0.22f }, { 0.2f, 0.565f }, 190 },  /* Thousand Dragon */
    { 4711, RAYO, { 0.85f, 0.4f, 1.0f }, { 0.51f, 0.496f }, 100 }, /* Tri-Horned Dragon */
    { 4732, RAYO, { 1.0f, 0.42f, 0.15f }, { 0.45f, 0.444f }, 100 }, /* Ryu-Ran */
    { 4103, RITUAL, { 0.85f, 0.5f, 1.0f }, { -1, -1 }, 0 },       /* Dragon Zombie: a violet ritual (its beam "queda mal") */
    /* by the user's choice of style, not their race */
    { 4028, RAYO, { 0.8f, 0.7f, 1.0f }, { -1, -1 }, 0 },          /* Summoned Skull: lightning from the sky */
    { 4758, RAYO, { 1.0f, 0.3f, 0.3f }, { -1, -1 }, 0 },          /* Jinzo: red lightning */
    { 4064, DESCENSO, { 0.8f, 0.88f, 1.0f }, { -1, -1 }, 0 },     /* Kuriboh (a fiend): from heaven */
    { 4044, CARGA, { 1.0f, 0.78f, 0.35f }, { -1, -1 }, 0 },       /* Gaia the Fierce Knight: gold */
    { 4047, CARGA, { 0.55f, 1.0f, 0.45f }, { -1, -1 }, 0 },       /* Celtic Guardian: green */
};
/* the rest by the game's own data (T_DATOS: type 20-24, attribute 29-31):
 * dragons, machines and thunder RAYO (lightning from the sky); spellcasters,
 * fiends and zombies RITUAL (gold); fairies and winged beasts DESCENSO;
 * warriors, beasts, rocks, insects and the rest CARGA. The light by the
 * attribute (1 light, 2 dark, 3 water, 4 fire, 5 earth, 6 wind) */
static const float k_col_atr[8][3] = {
    { 1.0f, 0.55f, 0.18f }, { 0.8f, 0.88f, 1.0f }, { 0.75f, 0.45f, 1.0f }, { 0.35f, 0.7f, 1.0f },
    { 1.0f, 0.42f, 0.15f }, { 1.0f, 0.58f, 0.2f }, { 0.5f, 1.0f, 0.55f }, { 1.0f, 0.55f, 0.18f } };
static estilo_t s_auto;
static const estilo_t *s_est = &k_estilos[0];

static const estilo_t *estilo_auto(uint32_t id, int tipo, int atr)
{
    int i;
    for (i = 0; i < (int)(sizeof k_estilos / sizeof k_estilos[0]); i++)
        if (k_estilos[i].id == id)
            return &k_estilos[i];
    memset(&s_auto, 0, sizeof s_auto);
    s_auto.id = (uint16_t)id;
    s_auto.boca[0] = s_auto.boca[1] = -1;
    s_auto.estilo = (tipo == 1 || tipo == 7 || tipo == 19) ? RAYO
                  : (tipo == 18 || tipo == 3 || tipo == 2) ? RITUAL
                  : (tipo == 17 || tipo == 16) ? DESCENSO : CARGA;
    memcpy(s_auto.col, k_col_atr[atr & 7], sizeof s_auto.col);
    if (s_auto.estilo == RITUAL) {      /* the Millennium gold; the dark ones a little violet */
        s_auto.col[0] = 1.0f; s_auto.col[1] = atr == 2 ? 0.62f : 0.72f; s_auto.col[2] = atr == 2 ? 0.42f : 0.28f;
    }
    return &s_auto;
}

static const estilo_t *estilo_de(uint32_t id, uint32_t k)
{
    uint32_t d = MEM32(T_DATOS + 4u * k);
    return estilo_auto(id, (int)((d >> 20) & 0x1Fu), (int)((d >> 29) & 7u));
}

/* light over all the picture (a flash) */
static void luz_todo(float cr, float cg, float cb, float k)
{
    size_t i, n = (size_t)OW * OH;
    for (i = 0; i < n; i++) {
        s_L[3 * i] += cr * k; s_L[3 * i + 1] += cg * k; s_L[3 * i + 2] += cb * k;
    }
}

/* a lightning bolt from a to b: midpoints pushed aside, halving (its own seed) */
#define RPTS 129
static int rayo_puntos(uint32_t *sem, float ax, float ay, float bx, float by, float desv, int prof, float p[RPTS][2])
{
    int n = 2, d, i;
    p[0][0] = ax; p[0][1] = ay; p[1][0] = bx; p[1][1] = by;
    for (d = 0; d < prof && n * 2 - 1 <= RPTS; d++) {
        float q[RPTS][2];
        int m = 0;
        for (i = 0; i + 1 < n; i++) {
            float mx = (p[i][0] + p[i + 1][0]) / 2, my = (p[i][1] + p[i + 1][1]) / 2;
            float nx = -(p[i + 1][1] - p[i][1]), ny = p[i + 1][0] - p[i][0], ln = sqrtf(nx * nx + ny * ny) + 1e-6f, o;
            *sem = *sem * 1664525u + 1013904223u;
            o = ((float)(*sem >> 8) / 16777216.f * 2 - 1) * desv;
            q[m][0] = p[i][0]; q[m][1] = p[i][1]; m++;
            q[m][0] = mx + nx / ln * o; q[m][1] = my + ny / ln * o; m++;
        }
        q[m][0] = p[n - 1][0]; q[m][1] = p[n - 1][1]; m++;
        memcpy(p, q, sizeof(float) * 2 * m);
        n = m;
        desv *= 0.55f;
    }
    return n;
}

static float sem_f(uint32_t *sem) { *sem = *sem * 1664525u + 1013904223u; return (float)(*sem >> 8) / 16777216.f; }

static void rayo_dibujar(uint32_t *sem, float ax, float ay, float bx, float by, float desv, int prof, float w,
                         const float *c, float k)
{
    float p[RPTS][2];
    int n = rayo_puntos(sem, ax, ay, bx, by, desv, prof, p), i;
    for (i = 0; i + 1 < n; i++) {
        luz_linea(p[i][0], p[i][1], p[i + 1][0], p[i + 1][1], w, 1, 1, 1, 1.6f * k);
        luz_linea(p[i][0], p[i][1], p[i + 1][0], p[i + 1][1], w * 4, c[0], c[1], c[2], 0.45f * k);
    }
    /* a branch, half the time */
    if (n > 4 && sem_f(sem) < 0.5f) {
        int j = (int)(sem_f(sem) * (n - 1));
        float ex = p[j][0] + (sem_f(sem) * 2 - 1) * 90 * S, ey = p[j][1] + (sem_f(sem) * 2 - 1) * 90 * S;
        float q[RPTS][2];
        int m = rayo_puntos(sem, p[j][0], p[j][1], ex, ey, desv * 0.6f, 4, q);
        for (i = 0; i + 1 < m; i++)
            luz_linea(q[i][0], q[i][1], q[i + 1][0], q[i + 1][1], w * 0.6f, c[0], c[1], c[2], 1.0f * k);
    }
}

/* ---- DESCENSO and RITUAL pieces ---------------------------------------------------- */
/* god rays from (ox, oy) downwards (90 +- 38 degrees): n rays of random width and
 * strength (seed), shimmering in t; worked at a quarter and added smoothly */
static float s_rq[(960 / 4 + 2) * (540 / 4 + 2)];
static void rayos_dios(float ox, float oy, float t, const float *c, float k, int n, uint32_t seed, float alcance,
                       float ancho)
{
    enum { NB = 512 };
    static float tabla[NB];
    int qw = OW / 4 + 1, qh = OH / 4 + 1, i, j, b;
    uint32_t sem = seed;
    if (k <= 0.002f || qw * qh > (int)(sizeof s_rq / sizeof s_rq[0]))
        return;
    memset(tabla, 0, sizeof tabla);
    for (i = 0; i < n; i++) {
        float a = (90.f + (sem_f(&sem) * 2 - 1) * 38.f + sinf(t * 0.7f + i) * 2.f) * 0.0174533f;
        float w = (0.25f + 0.75f * sem_f(&sem)) * ancho, v = (60.f + 195.f * sem_f(&sem)) / 255.f;
        v *= 0.75f + 0.25f * sinf(t * 3 + i * 1.7f);
        for (b = 0; b < NB; b++) {
            float th = (float)b / (NB - 1) * 3.14159f, d = (th - a) / w;
            if (d > -3 && d < 3)
                tabla[b] += v * expf(-d * d);
        }
    }
    for (j = 0; j < qh; j++)
        for (i = 0; i < qw; i++) {
            float dx = (float)(i * 4) - ox, dy = (float)(j * 4) - oy, th, d, f;
            if (dy <= 0) { s_rq[j * qw + i] = 0; continue; }
            th = atan2f(dy, dx);
            d = sqrtf(dx * dx + dy * dy);
            f = 1 - d / alcance;
            f = f > 0 ? f * sqrtf(f) : 0;
            b = (int)(th / 3.14159f * (NB - 1));
            s_rq[j * qw + i] = (b >= 0 && b < NB ? tabla[b] : 0) * f * k;
        }
    for (j = oy > 0 ? (int)oy : 0; j < OH; j++)      /* the rays go down */
        for (i = 0; i < OW; i++) {
            float fx = i / 4.f, fy = j / 4.f, v, *L;
            int x0 = (int)fx, y0 = (int)fy;
            fx -= x0; fy -= y0;
            if (x0 + 1 >= qw || y0 + 1 >= qh)
                continue;
            v = (s_rq[y0 * qw + x0] * (1 - fx) + s_rq[y0 * qw + x0 + 1] * fx) * (1 - fy)
              + (s_rq[(y0 + 1) * qw + x0] * (1 - fx) + s_rq[(y0 + 1) * qw + x0 + 1] * fx) * fy;
            if (v < 0.002f)
                continue;
            L = s_L + 3 * ((size_t)j * OW + i);
            L[0] += c[0] * v; L[1] += c[1] * v; L[2] += c[2] * v;
        }
}

/* an arc of light: the ellipse (cx, cy, rx, ry) from a0 to a1 degrees (y down) */
static void luz_arco(float cx, float cy, float rx, float ry, float a0, float a1, float w, const float *c, float k)
{
    int i, n = 14;
    for (i = 0; i < n; i++) {
        float p = (a0 + (a1 - a0) * i / n) * 0.0174533f, q2 = (a0 + (a1 - a0) * (i + 1) / n) * 0.0174533f;
        luz_linea(cx + rx * cosf(p), cy + ry * sinf(p), cx + rx * cosf(q2), cy + ry * sinf(q2), w, c[0], c[1], c[2], k);
    }
}

/* rainbow foil over the card (the front picture, inside its quad): a band across */
static void holo(float tt, float k)
{
    int x0 = 1 << 30, x1 = -1, y0 = 1 << 30, y1 = -1, i, j;
    double *hm = s_hm;
    if (k <= 0)
        return;
    {
        /* the card's box: its quad from the inverse, by sampling the corners */
        float q[4][2];
        int m;
        double A = hm[0], B = hm[1], C = hm[2], D = hm[3], E = hm[4], F = hm[5], G = hm[6], H = hm[7];
        static const float uv[4][2] = { { 0, 0 }, { 1, 0 }, { 1, 1 }, { 0, 1 } };
        for (m = 0; m < 4; m++) {
            /* solve [A-uG B-uH; D-vG E-vH] [X Y] = [u-C; v-F] */
            double u = uv[m][0], v = uv[m][1], a11 = A - u * G, a12 = B - u * H, a21 = D - v * G, a22 = E - v * H;
            double det = a11 * a22 - a12 * a21;
            if (fabs(det) < 1e-12)
                return;
            q[m][0] = (float)(((u - C) * a22 - a12 * (v - F)) / det);
            q[m][1] = (float)((a11 * (v - F) - a21 * (u - C)) / det);
            if ((int)q[m][0] < x0) x0 = (int)q[m][0];
            if ((int)q[m][0] + 1 > x1) x1 = (int)q[m][0] + 1;
            if ((int)q[m][1] < y0) y0 = (int)q[m][1];
            if ((int)q[m][1] + 1 > y1) y1 = (int)q[m][1] + 1;
        }
    }
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > OW) x1 = OW;
    if (y1 > OH) y1 = OH;
    for (j = y0; j < y1; j++)
        for (i = x0; i < x1; i++) {
            double X = i + 0.5, Y = j + 0.5, d = hm[6] * X + hm[7] * Y + 1;
            float u = (float)((hm[0] * X + hm[1] * Y + hm[2]) / d), v = (float)((hm[3] * X + hm[4] * Y + hm[5]) / d);
            float s, band, hue, *L;
            if (u < 0 || u > 1 || v < 0 || v > 1)
                continue;
            s = u + v * 0.6f;
            band = (s - (tt * 2.2f - 0.6f)) / 0.18f;
            band = expf(-band * band) * k;
            if (band < 0.003f)
                continue;
            hue = s * 3 + tt;
            hue -= floorf(hue);
            L = s_L + 3 * ((size_t)j * OW + i);
            L[0] += (0.5f + 0.5f * cosf(6.2832f * hue)) * band;
            L[1] += (0.5f + 0.5f * cosf(6.2832f * (hue + 0.33f))) * band;
            L[2] += (0.5f + 0.5f * cosf(6.2832f * (hue + 0.66f))) * band;
        }
}

/* RITUAL: the card's 6 x 9 pieces fly in out of the vortex (0.8 .. 1.7 s), drawn
 * here (canvas size: they move fast); the kit's card takes over when it is whole */
static float s_tejas[54][4];            /* ox, oy, rot (degrees), delay */
static void tejas_hacer(void)
{
    uint32_t sem = 9;
    int i;
    for (i = 0; i < 54; i++) {
        s_tejas[i][0] = sem_f(&sem) * 2 - 1;
        s_tejas[i][1] = sem_f(&sem) * 2 - 1;
        s_tejas[i][2] = (sem_f(&sem) * 2 - 1) * 180;
        s_tejas[i][3] = sem_f(&sem) * 0.35f;
    }
}

static void tejas(float t, float cx, float cy, float h, const float *c)
{
    float w = h * (float)s_carta.w / (float)s_carta.h, tw = w / 6, th = h / 9;
    int n;
    if (!s_carta.p)
        return;
    for (n = 0; n < 54; n++) {
        int ti = n % 6, tj = n / 6, i, j, x0, x1, y0, y1;
        float q = eout((t - 0.8f - s_tejas[n][3]) / 0.5f, 3), fx, fy, ang, x, y, rot, cr, sr, al, rad;
        if (q <= 0)
            continue;
        fx = cx - w / 2 + (ti + 0.5f) * tw;
        fy = cy - h / 2 + (tj + 0.5f) * th;
        ang = atan2f(fy - cy, fx - cx) + s_tejas[n][0] * 2;
        x = fx + cosf(ang) * 420 * S * (1 - q) + s_tejas[n][0] * 60 * S * (1 - q);
        y = fy + sinf(ang) * 260 * S * (1 - q) + s_tejas[n][1] * 60 * S * (1 - q);
        rot = s_tejas[n][2] * (1 - q) * 0.0174533f;
        cr = cosf(rot); sr = sinf(rot);
        al = fminf(1, q * 1.6f);
        rad = sqrtf(tw * tw + th * th) / 2 + 1;
        x0 = (int)(x - rad); x1 = (int)(x + rad) + 1; y0 = (int)(y - rad); y1 = (int)(y + rad) + 1;
        if (x0 < 0) x0 = 0;
        if (y0 < 0) y0 = 0;
        if (x1 > OW) x1 = OW;
        if (y1 > OH) y1 = OH;
        for (j = y0; j < y1; j++)
            for (i = x0; i < x1; i++) {
                float dx = i + 0.5f - x, dy = j + 0.5f - y, lx = dx * cr + dy * sr, ly = -dx * sr + dy * cr;
                float u, v, sx2, sy2, a, *o;
                int ix, iy;
                const unsigned char *p;
                if (lx < -tw / 2 || lx >= tw / 2 || ly < -th / 2 || ly >= th / 2)
                    continue;
                u = (ti + lx / tw + 0.5f) / 6; v = (tj + ly / th + 0.5f) / 9;
                sx2 = u * s_carta.w; sy2 = v * s_carta.h;
                ix = (int)sx2; iy = (int)sy2;
                ix = ix < 0 ? 0 : ix >= s_carta.w ? s_carta.w - 1 : ix;
                iy = iy < 0 ? 0 : iy >= s_carta.h ? s_carta.h - 1 : iy;
                p = s_carta.p + 4 * ((size_t)iy * s_carta.w + ix);
                a = p[3] / 255.f * al;
                o = s_O + 4 * ((size_t)j * OW + i);
                o[0] = p[2] / 255.f * al + o[0] * (1 - a);
                o[1] = p[1] / 255.f * al + o[1] * (1 - a);
                o[2] = p[0] / 255.f * al + o[2] * (1 - a);
                o[3] = a + o[3] * (1 - a);
            }
        /* its glowing edge while it flies */
        if (q < 0.98f) {
            float k = 1.1f * (1 - q), ex = tw / 2, ey = th / 2;
            float px[4] = { -ex, ex, ex, -ex }, py[4] = { -ey, -ey, ey, ey };
            int m;
            for (m = 0; m < 4; m++) {
                int m2 = (m + 1) & 3;
                luz_linea(x + px[m] * cr - py[m] * sr, y + px[m] * sr + py[m] * cr, x + px[m2] * cr - py[m2] * sr,
                          y + px[m2] * sr + py[m2] * cr, 1.2f * S, c[0], c[1], c[2], k);
            }
        }
    }
}

/* RITUAL: the hieroglyph ring turning in perspective, and the Eye behind the card */
static void ritual_detras(float t, float cx, float cy, const float *c)
{
    float a = ventana(t, 0.2f, 3.0f, 0.4f, 0.4f), rot = t * 25, ry = 0.28f;
    int i;
    if (a <= 0)
        return;
    luz_anillo(cx, cy + 40 * S, 560 * S, 560 * ry * S, 2.4f * S, c[0], c[1], c[2], 1.1f * a);
    luz_anillo(cx, cy + 40 * S, 600 * S, 600 * ry * S, 1.2f * S, c[0], c[1], c[2], 0.8f * a);
    luz_anillo(cx, cy + 40 * S, 520 * S, 520 * ry * S, 1.2f * S, c[0], c[1], c[2], 0.8f * a);
    for (i = 0; i < 36; i++) {
        float g = (i * 10 + rot) * 0.0174533f, x = cx + 560 * S * cosf(g), y = cy + 40 * S + 560 * ry * S * sinf(g);
        float s = (9 + 4 * sinf(g)) * S, k = 1.1f * a, lw = 1.0f * S;
        switch (i % 4) {
        case 0:
            luz_anillo(x, y, s, s * 0.6f, lw, c[0], c[1], c[2], k);
            luz_linea(x, y + s * 0.6f, x, y + s * 1.6f, lw, c[0], c[1], c[2], k);
            break;
        case 1:
            luz_linea(x - s, y, x + s, y, lw, c[0], c[1], c[2], k);
            luz_linea(x, y - s, x, y + s, lw, c[0], c[1], c[2], k);
            break;
        case 2:
            luz_linea(x - s, y + s * 0.7f, x, y - s * 0.7f, lw, c[0], c[1], c[2], k);
            luz_linea(x, y - s * 0.7f, x + s, y + s * 0.7f, lw, c[0], c[1], c[2], k);
            luz_linea(x + s, y + s * 0.7f, x - s, y + s * 0.7f, lw, c[0], c[1], c[2], k);
            break;
        default:
            luz_arco(x, y, s, s, 200, 340, lw, c, k);
            luz_linea(x - s * 0.8f, y, x + s * 0.8f, y, lw, c[0], c[1], c[2], k);
        }
    }
    /* the Eye */
    {
        float ke = ventana(t, 0.35f, 2.0f, 0.4f, 0.5f) * (0.85f + 0.15f * sinf(t * 9)), e = 2.6f * S, ey = cy - 10 * S;
        int pas;
        if (ke > 0)
            for (pas = 0; pas < 2; pas++) {
                float lw = pas ? 9 * S : 2.6f * S, k = pas ? 0.35f * ke : 1.2f * ke;
                luz_arco(cx, ey, 70 * e, 40 * e, 200, 340, lw, c, k);
                luz_arco(cx, ey, 70 * e, 40 * e, 20, 160, lw, c, k);
                luz_arco(cx, ey, 16 * e, 16 * e, 0, 360, lw, c, k);
                luz_linea(cx - 10 * e, ey + 34 * e, cx - 26 * e, ey + 70 * e, lw, c[0], c[1], c[2], k);
                luz_arco(cx + 31 * e, ey + 55 * e, 29 * e, 25 * e, -90, 90, lw, c, k);
                luz_linea(cx - 70 * e, ey - 2 * e, cx - 110 * e, ey - 6 * e, lw, c[0], c[1], c[2], k);
                luz_linea(cx - 55 * e, ey - 52 * e, cx + 60 * e, ey - 58 * e, lw, c[0], c[1], c[2], k);
            }
        if (ke > 0)
            luz_punto(cx, ey, 6 * e, c[0], c[1], c[2], 1.4f * ke);
        rayos_dios(cx, ey, t, c, 0.16f * a, 28, 11, 700 * S, 0.035f);
    }
}

/* ---- MAGO: "Magia oscura" (2026-11-19, the user: "los Magos Oscuros ... su propia
 * animacion ... que use violeta"): a magic circle opens on the floor, the card
 * rises out of it, violet sparks spiral up and lightning runs on its edges, the
 * staff (boca u, v) charges a dark ball and lets it burst ------------------------ */
static void mago_circulo(float t, float cx, float y, const float *c)
{
    float a = ventana(t, 0.0f, 3.0f, 0.45f, 0.5f), g = eout(t / 0.5f, 3), rot = t * 0.6f, ry = 0.3f;
    float r1 = 300 * S * g, r2 = 255 * S * g, rh = 275 * S * g;
    int i;
    if (a <= 0 || g <= 0)
        return;
    luz_anillo(cx, y, r1, r1 * ry, 2.6f * S, c[0], c[1], c[2], 1.2f * a);
    luz_anillo(cx, y, r2, r2 * ry, 1.4f * S, c[0], c[1], c[2], 0.9f * a);
    luz_elipse(cx, y, r1 * 0.9f, r1 * ry * 0.9f, c[0], c[1], c[2], 0.25f * a);
    /* the hexagram: two triangles on the circle, turning */
    for (i = 0; i < 6; i++) {
        float p0 = rot + i * 2.0944f, p1 = rot + (i + 2) * 2.0944f;
        if (i >= 3) { p0 += 1.0472f; p1 += 1.0472f; }
        luz_linea(cx + rh * cosf(p0), y + rh * ry * sinf(p0), cx + rh * cosf(p1), y + rh * ry * sinf(p1), 1.6f * S,
                  c[0], c[1], c[2], 1.0f * a);
    }
    /* runes between the rings */
    for (i = 0; i < 24; i++) {
        float p = -rot * 1.5f + i * 0.2618f, x = cx + (r1 + r2) * 0.5f * cosf(p), yy = y + (r1 + r2) * 0.5f * ry * sinf(p);
        float s = (5 + 2 * sinf(p)) * S * g;
        if (i & 1)
            luz_linea(x - s, yy, x + s, yy, 1.0f * S, c[0], c[1], c[2], 0.9f * a);
        else
            luz_linea(x, yy - s * 0.8f, x, yy + s * 0.8f, 1.0f * S, c[0], c[1], c[2], 0.9f * a);
    }
}

static void zona_xy(float *x, float *y)
{
    static const float near_mon[5] = { 231.f, 276.f, 318.f, 361.f, 405.f };
    static const float far_mon[5] = { 398.f, 359.f, 318.f, 281.f, 243.f };
    int z = s_zona >= 0 && s_zona < 5 ? s_zona : 2;
    float fx = s_lado ? far_mon[z] : near_mon[z], fy = s_lado ? 142.f : 252.f;
    *x = fx / 640.f * (float)OW;
    *y = fy / 480.f * (float)OH;
}

static void cuadro(float t)
{
    const float tr = s_est->col[0], tg = s_est->col[1], tb = s_est->col[2];
    const int rayo = s_est->estilo == RAYO, mago = s_est->estilo == MAGO, con_boca = (rayo || mago) && s_est->boca[0] >= 0;
    float bx = 0, by = 0;
    float dim = eio(t / 0.3f) * (1 - eio((t - 3.2f) / 0.35f));
    s_carta_a = 1;
    float CH = 470.f * S, cx = DX(640), cy, yaw = 0, pitch = 0, h = CH, suelo = DY(315) + CH / 2 + 10 * S;
    float sx = 0, sy = 0, q[4][2];
    int vis = 1, i, j;
    size_t n = (size_t)OW * OH;
    float zx, zy;
    zona_xy(&zx, &zy);
    (void)zx; (void)zy;                 /* the square: the game's placement now */
    /* the dim (and a vignette) */
    for (j = 0; j < OH; j++)
        for (i = 0; i < OW; i++) {
            float vx = ((float)i / OW - 0.5f), vy = ((float)j / OH - 0.5f), v = (vx * vx + vy * vy) * 2.6f;
            float *o = s_O + 4 * ((size_t)j * OW + i);
            o[0] = o[1] = o[2] = 0;
            o[3] = dim * (0.72f + 0.25f * (v > 1 ? 1 : v));
            if (s_vista && s_vfondo) {      /* the Perfil under the dim (its own darkening in it) */
                const unsigned char *f = s_vfondo + 4 * ((size_t)j * OW + i);
                float a = dim * (0.55f + 0.35f * (v > 1 ? 1 : v));
                o[0] = f[2] / 255.f * (1 - a); o[1] = f[1] / 255.f * (1 - a); o[2] = f[0] / 255.f * (1 - a);
                o[3] = a + f[3] / 255.f * (1 - a);
            }
        }
    memset(s_L, 0, n * 3 * sizeof(float));
    /* the card's pose: each style's up to 2.9 s, then the handoff */
    {
        float tt = t < 2.9f ? t : 2.9f;
        if (rayo) {
            /* in fast from the right, turned, hits at 0.55 */
            if (tt < 0.55f) {
                float k = eout((tt - 0.2f) / 0.35f, 4);
                cx = DX(640) + (float)OW * 0.85f * (1 - k);
                cy = DY(315) - 40 * S * (1 - k);
                yaw = -55 * (1 - k);
                vis = tt >= 0.2f;
            } else {
                cy = DY(315) + sinf(tt * 2.5f) * 5 * S;
                yaw = sinf(tt * 1.5f) * 5;
            }
        } else if (s_est->estilo == DESCENSO) {
            /* down from heaven, swaying, slowing; lands at 1.35 */
            if (tt < 1.35f) {
                float k = eout((tt - 0.3f) / 1.05f, 3);
                cy = DY(-330) + (DY(315) - DY(-330)) * k;
                yaw = 70 * (1 - k) * sinf((tt - 0.3f) * 7);
                pitch = 14 * (1 - k);
                h = CH * (0.8f + 0.2f * k);
                vis = tt > 0.3f;
            } else {
                cy = DY(315) + sinf((tt - 1.35f) * 2.2f) * 7 * S;
                yaw = sinf((tt - 1.35f) * 1.6f) * 7;
                pitch = sinf((tt - 1.35f) * 1.1f) * 4;
            }
        } else if (s_est->estilo == MAGO) {
            /* out of the circle on the floor, rising and standing up, 0.35 .. 1.2 */
            if (tt < 1.2f) {
                float k = eout((tt - 0.35f) / 0.85f, 3);
                cy = suelo - CH * 0.1f + (DY(315) - suelo + CH * 0.1f) * k;
                h = CH * (0.25f + 0.75f * k);
                pitch = 70 * (1 - k);
                vis = tt > 0.35f;
            } else {
                cy = DY(315) + sinf((tt - 1.2f) * 2.0f) * 6 * S;
                yaw = sinf((tt - 1.2f) * 1.3f) * 6;
            }
        } else if (s_est->estilo == RITUAL) {
            /* put together out of the vortex (tejas), whole at 1.7 */
            cy = DY(315) + (tt > 1.7f ? sinf((tt - 1.7f) * 2.0f) * 6 * S : 0);
            yaw = tt > 1.7f ? sinf((tt - 1.7f) * 1.4f) * 9 : 0;
            pitch = tt > 1.7f ? sinf(tt - 1.7f) * 3 : 0;
            vis = tt >= 1.7f;
        } else if (tt < 0.62f) {
            float k = tt < 0.25f ? 0 : powf((tt - 0.25f) / 0.37f, 2.2f);
            cy = DY(-330) + (DY(315) - DY(-330)) * k;
            pitch = -25 * (1 - k);
            vis = tt >= 0.25f;
        } else {
            float rb = expf(-(tt - 0.62f) * 9) * sinf((tt - 0.62f) * 40) * 10 * S;
            cy = DY(315) - fabsf(rb);
            yaw = sinf(tt * 1.3f) * 4;
        }
        if (t >= 2.9f) {
            /* 2026-11-19 (the user: "se pone 2 veces"; skipping the game's slide
             * left its card stuck at the top): a handoff. The card goes to where
             * the game's own placement starts (its card, flat, at the top centre:
             * 0.292 of the height, 0.347 of it tall, from the user's capture) and
             * stays there until the game starts it (sub_00046B30), which takes it
             * down to its square: one placement, the game's */
            float k = eio((t - 2.9f) / 0.35f), hz = 0.347f * (float)OH, yz = 0.292f * (float)OH;
            if (s_vista) {
                h *= 1 + 0.2f * k;
                s_carta_a = 1 - k;
                vis = t < 3.25f;
                hz = h; yz = cy;
            }
            cx = cx + (DX(640) - cx) * k;
            cy = cy + (yz - cy) * k;
            h = h + (hz - h) * k;
            yaw *= 1 - k;
            pitch *= 1 - k;
            if (!s_vista)
                vis = !s_entregada;
        }
    }
    /* the mouth on the screen (the card nearly flat while it fires) */
    if (con_boca) {
        bx = cx + (s_est->boca[0] - 0.5f) * h * (512.f / 740.f);
        by = cy + (s_est->boca[1] - 0.5f) * h;
    }
    if (s_est->estilo == CARGA) {
        /* the stomps: the particles they throw, made in the simulation below */
        for (i = 0; i < 4; i++) {
            float g = k_golpes[i], f = i == 0 ? 1.f : 0.45f;
            if (t >= g && t < g + 0.5f) {
                float k = (t - g) / 0.5f, r = (20 + (i == 0 ? 600.f : 380.f) * eout(k, 2.4f)) * S;
                float e = expf(-(t - g) * (i == 0 ? 8.f : 14.f));
                luz_anillo(cx, suelo, r, r * 0.22f, (18 * (1 - k) + 3) * S, tr, tg, tb, 1.3f * f * (1 - k));
                sx += sinf(t * 95) * 26 * e * f * S;
                sy += fabsf(cosf(t * 80)) * 20 * e * f * S;
            }
        }
        /* the cracks, cooling down */
        if (t > 0.62f && t < 2.02f) {
            float k = (t - 0.62f) / 1.4f, largo = eout(k * 5, 2), b = powf(1 - k, 1.5f);
            int g;
            for (g = 0; g < NGRIETA; g++) {
                int m = (int)(largo * (PGRIETA - 1));
                for (j = 0; j < m; j++)
                    luz_linea(cx + s_gr[g][j][0] * S, suelo + s_gr[g][j][1] * S, cx + s_gr[g][j + 1][0] * S,
                              suelo + s_gr[g][j + 1][1] * S, 2.2f * S, tr, tg, tb, 1.8f * b);
            }
        }
    } else if (rayo) {
        /* the sky flickers before it comes */
        if (t < 0.45f) {
            int f = (int)(t * 30);
            if (f == 2 || f == 3 || f == 8 || f == 11) {
                uint32_t sem = 0x51ED27u + (uint32_t)f * 7919u;
                float ax = DX(300 + 680 * sem_f(&sem)), ex2 = DX(300 + 680 * sem_f(&sem)), ey2 = DY(300 + 300 * sem_f(&sem));
                rayo_dibujar(&sem, ax, DY(-20), ex2, ey2, 140 * S, 7, 1.6f * S, s_est->col, 1.4f);
                luz_todo(s_est->col[0], s_est->col[1], s_est->col[2], 0.18f);
            }
        }
        /* the hit: a shock ring, a flash, speed lines out of the centre, a shake */
        if (t >= 0.55f && t < 1.2f) {
            float k = (t - 0.55f) / 0.65f, r = (20 + 700 * eout(k, 2.4f)) * S;
            uint32_t sem = 0xA5A5u + (uint32_t)(t * 30) * 131u;
            luz_anillo(cx, cy, r, r, (18 * (1 - k) + 3) * S, tr, tg, tb, 1.2f * (1 - k));
            if (k < 0.1f)
                luz_todo(0.85f, 0.92f, 1.0f, 0.3f * (1 - k / 0.1f));
            for (i = 0; i < 60; i++) {
                float a = sem_f(&sem) * 6.2832f, r0 = (330 + 170 * sem_f(&sem)) * S, r1 = r0 + (150 + 350 * sem_f(&sem)) * S;
                luz_linea(cx + cosf(a) * r0, cy + sinf(a) * r0, cx + cosf(a) * r1, cy + sinf(a) * r1, 1.3f * S,
                          0.7f + 0.3f * tr, 0.7f + 0.3f * tg, 0.7f + 0.3f * tb, (0.3f + 0.6f * sem_f(&sem)) * (1 - k));
            }
        }
        if (t >= 0.55f && t < 1.1f) {
            float e = expf(-(t - 0.55f) * 7);
            sx += sinf(t * 90) * 22 * e * S;
            sy += cosf(t * 77) * 14 * e * S;
        }
        /* the charge: light into the mouth */
        if (con_boca && t > 1.2f && t < 1.9f) {
            float k = (t - 1.2f) / 0.7f;
            luz_punto(bx, by, (10 + 40 * k) * S, 0.85f, 0.95f, 1.0f, 1.6f * k);
            luz_punto(bx, by, (40 + 80 * k) * S, tr, tg, tb, 0.5f * k);
            sx += sinf(t * 120) * 3 * k * S;
            sy += cosf(t * 110) * 3 * k * S;
        }
    }
    if (s_est->estilo == DESCENSO) {
        float luz = ventana(t, 0.1f, 3.2f, 0.5f, 0.4f);
        rayos_dios(DX(640), DY(-260), t, s_est->col, 0.55f * luz, 46, 3, 1100 * S, 0.05f);
        luz_elipse(DX(640), DY(-60), 520 * S, 160 * S, tr, tg, tb, 0.6f * luz);
        /* landing: a ring on the floor, a flare, a soft flash */
        if (t >= 1.35f && t < 2.2f) {
            float k = (t - 1.35f) / 0.85f, r = (20 + 520 * eout(k, 2.4f)) * S, f = 2.2f * (1 - eout(k, 2));
            luz_anillo(cx, cy + 170 * S, r, r * 0.32f, (18 * (1 - k) + 3) * S, tr, tg, tb, 1.3f * (1 - k));
            luz_linea(cx - 420 * S, cy, cx + 420 * S, cy, 4 * S, tr, tg, tb, 0.7f * f);
            luz_linea(cx - 200 * S, cy, cx + 200 * S, cy, 14 * S, tr, tg, tb, 0.3f * f);
            luz_punto(cx, cy, 40 * S, 1, 1, 1, 0.6f * f);
            if (k < 0.07f)
                luz_todo(tr, tg, tb, 0.25f * (1 - k / 0.07f));
        }
    } else if (mago) {
        mago_circulo(t, DX(640), suelo, s_est->col);
        if (t > 1.2f && t < 1.9f)                       /* a soft pulse when it stands */
            luz_elipse(cx, cy, 280 * S, 360 * S, tr, tg, tb, 0.5f * (1 - (t - 1.2f) / 0.7f));
    } else if (s_est->estilo == RITUAL) {
        ritual_detras(t, cx, cy, s_est->col);
        if (t > 1.65f && t < 2.4f) {        /* the gold pulse when it is whole */
            float k = (t - 1.65f) / 0.75f, r = (20 + 650 * eout(k, 2.4f)) * S;
            luz_anillo(cx, cy + 40 * S, r, r * 0.28f, (18 * (1 - k) + 3) * S, tr, tg, tb, 1.2f * (1 - k));
            luz_elipse(cx, cy, 300 * S, 380 * S, tr, tg, tb, 0.7f * (1 - eout(k, 2)));
            if (k < 0.1f)
                luz_todo(tr, tg, tb, 0.25f * (1 - k / 0.1f));
        }
    }
    /* the particles: up to t, fixed steps (the same at any frame rate) */
    while (s_sim + 1.f / 120.f <= t) {
        float tt = s_sim;
        s_sim += 1.f / 120.f;
        if (s_est->estilo == DESCENSO) {
            int k;
            if (tt > 0.2f && tt < 3.0f && rnd() < 0.75f)      /* falling sparkles */
                emitir(CHISPA, DX(260 + 760 * rnd()), -10, rr(-20, 20) * S, rr(120, 260) * S, 2.5f, rr(1.5f, 3.5f) * S,
                       0, 0.2f, 0.5f + 0.5f * tr, 0.5f + 0.5f * tg, 0.5f + 0.5f * tb);
            if (tt < 1.35f && s_sim >= 1.35f)                 /* landing */
                for (k = 0; k < 160; k++) {
                    float a = rr(0, 6.2832f), v = rr(250, 900) * S;
                    emitir(CHISPA, cx, cy + 150 * S, cosf(a) * v, sinf(a) * v * 0.45f, rr(0.5f, 1.2f), rr(1.5f, 3) * S,
                           380 * S, 1.8f, 0.6f + 0.4f * tr, 0.6f + 0.4f * tg, 0.6f + 0.4f * tb);
                }
        } else if (mago) {
            int k;
            if (tt > 0.3f && tt < 2.6f && rnd() < 0.8f) {      /* violet sparks spiralling up out of the circle */
                float a0 = rr(0, 6.2832f), r0 = rr(120, 290) * S;
                emitir(CHISPA, DX(640) + cosf(a0) * r0, suelo + sinf(a0) * r0 * 0.3f, -sinf(a0) * 160 * S,
                       -rr(250, 520) * S, rr(0.7f, 1.4f), rr(1.2f, 2.8f) * S, 0, 0.4f, tr, tg, tb);
            }
            if (con_boca && tt > 1.5f && tt < 2.0f) {           /* into the staff */
                float a = rr(0, 6.2832f), d0 = rr(200, 380) * S;
                emitir(CHISPA, bx + cosf(a) * d0, by + sinf(a) * d0, -cosf(a) * d0 * 2.4f, -sinf(a) * d0 * 2.4f, 0.4f,
                       2.2f * S, 0, 0, 0.6f + 0.4f * tr, 0.6f + 0.4f * tg, 0.6f + 0.4f * tb);
            }
            if (tt < 2.0f && s_sim >= 2.0f)                     /* the burst */
                for (k = 0; k < 240; k++) {
                    float a = rr(0, 6.2832f), v = rr(300, 1300) * S;
                    emitir(CHISPA, bx, by, cosf(a) * v, sinf(a) * v, rr(0.3f, 1.0f), rr(1.5f, 3.5f) * S, 150 * S, 2.2f,
                           0.5f + 0.5f * tr, 0.5f + 0.5f * tg, 0.5f + 0.5f * tb);
                }
        } else if (s_est->estilo == RITUAL) {
            int k, m = 2 + (rnd() < 0.25f);
            if (tt < 2.6f)                                     /* the sand, spiralling in */
                for (k = 0; k < m; k++) {
                    float a0 = rr(0, 6.2832f), r0 = rr(320, 560) * S;
                    emitir(CHISPA, cx + cosf(a0) * r0, cy + sinf(a0) * r0 * 0.55f,
                           (-sinf(a0) * 520 - cosf(a0) * 140) * S, (cosf(a0) * 280 - sinf(a0) * 70) * S, rr(0.6f, 1.2f),
                           rr(1.0f, 2.4f) * S, 0, 0.6f, tr, tg, tb);
                }
            if (tt < 1.65f && s_sim >= 1.65f)
                for (k = 0; k < 200; k++) {
                    float a = rr(0, 6.2832f), v = rr(150, 650) * S;
                    emitir(CHISPA, cx + cosf(a) * 200 * S, cy + sinf(a) * 260 * S, cosf(a) * v, sinf(a) * v - 100 * S,
                           rr(0.8f, 1.6f), rr(1.0f, 2.5f) * S, 160 * S, 1.4f, tr, tg, tb);
                }
        } else if (!rayo) {
            for (i = 0; i < 4; i++) {
                float g = k_golpes[i], f = i == 0 ? 1.f : 0.45f;
                if (tt < g && s_sim >= g) {
                    int k;
                    for (k = 0; k < (i == 0 ? 60 : 18); k++) {
                        float a = rr(3.14159f * 1.05f, 3.14159f * 1.95f), v = rr(200, 700) * f * S;
                        emitir(POLVO, cx + rr(-120, 120) * S, suelo, cosf(a) * v, sinf(a) * v * 0.5f, rr(0.9f, 1.6f),
                               rr(30, 60) * S, 0, 2.2f, 0.73f, 0.59f, 0.42f);
                    }
                    for (k = 0; k < (int)(26 * f); k++) {
                        float a = rr(3.14159f * 1.1f, 3.14159f * 1.9f), v = rr(300, 800) * f * S;
                        emitir(ROCA, cx + rr(-80, 80) * S, suelo, cosf(a) * v, sinf(a) * v, rr(0.8f, 1.3f), rr(4, 11) * S,
                               1500 * S, 0.5f, 0.31f, 0.23f, 0.15f);
                    }
                    for (k = 0; k < (int)(80 * f); k++) {
                        float a = rr(3.14159f * 1.1f, 3.14159f * 1.9f), v = rr(200, 900) * f * S;
                        emitir(CHISPA, cx + rr(-60, 60) * S, suelo, cosf(a) * v, sinf(a) * v, rr(0.3f, 0.8f), 1.8f * S,
                               900 * S, 1.0f, tr, tg, tb);
                    }
                }
            }
            if (tt > 0.6f && tt < 3.0f && rnd() < 0.2f)
                emitir(BRASA, rr(0, (float)OW), (float)OH + 5, rr(-30, 30) * S, -rr(80, 200) * S, 2.4f, rr(1.2f, 2.4f) * S,
                       -20 * S, 0.1f, tr, tg, tb);
        } else {
            int k;
            if (tt < 0.55f && s_sim >= 0.55f)       /* the hit: sparks all around */
                for (k = 0; k < 220; k++) {
                    float a = rr(0, 6.2832f), v = rr(300, 1200) * S;
                    emitir(CHISPA, cx, cy, cosf(a) * v, sinf(a) * v, rr(0.3f, 0.9f), rr(1.5f, 3.5f) * S, 200 * S, 2.5f,
                           0.6f + 0.4f * tr, 0.6f + 0.4f * tg, 0.6f + 0.4f * tb);
                }
            if (con_boca && tt > 1.2f && tt < 1.9f) {   /* into the mouth */
                float a = rr(0, 6.2832f), d0 = rr(250, 480) * S;
                emitir(CHISPA, bx + cosf(a) * d0, by + sinf(a) * d0, -cosf(a) * d0 * 2.4f, -sinf(a) * d0 * 2.4f, 0.4f,
                       2.5f * S, 0, 0, 0.7f + 0.3f * tr, 0.8f + 0.2f * tg, 0.8f + 0.2f * tb);
            }
            if (tt > 0.6f && tt < 2.9f && rnd() < 0.15f)
                emitir(BRASA, rr(0, (float)OW), (float)OH + 5, rr(-30, 30) * S, -rr(80, 200) * S, 2.4f, rr(1.2f, 2.4f) * S,
                       -20 * S, 0.1f, tr, tg, tb);
        }
        paso(1.f / 120.f);
    }
    /* the card's own light behind it, then the card over it */
    if (vis)
        luz_elipse(cx, cy, 250 * S * h / CH, 320 * S * h / CH, tr, tg, tb, 0.35f);
    /* the light so far goes behind the card: the picture behind it, done */
    {
        unsigned char *px = s_fondo[s_pub ^ 1];
        for (i = 0; i < (int)n; i++) {
            float r = s_O[4 * i] + s_L[3 * i], g = s_O[4 * i + 1] + s_L[3 * i + 1], b = s_O[4 * i + 2] + s_L[3 * i + 2];
            px[4 * i] = (unsigned char)((r > 1 ? 1 : r) * 255.f + 0.5f);
            px[4 * i + 1] = (unsigned char)((g > 1 ? 1 : g) * 255.f + 0.5f);
            px[4 * i + 2] = (unsigned char)((b > 1 ? 1 : b) * 255.f + 0.5f);
            px[4 * i + 3] = (unsigned char)((s_O[4 * i + 3] > 1 ? 1 : s_O[4 * i + 3]) * 255.f + 0.5f);
            s_O[4 * i] = s_O[4 * i + 1] = s_O[4 * i + 2] = s_O[4 * i + 3] = 0;
            s_L[3 * i] = s_L[3 * i + 1] = s_L[3 * i + 2] = 0;
        }
    }
    s_carta_vis = vis;
    if (vis) {
        carta_3d(cx, cy, h, yaw, pitch, 1.f, q);
        /* the eyes light up, one after the other */
        if (s_ojos && t > 1.9f && t < 2.95f)
            for (i = 0; i < s_ojos->n; i++) {
                float k = ventana(t, 1.9f + i * 0.05f, 2.95f, 0.12f, 0.25f) * (0.8f + 0.2f * sinf(t * 20 + i));
                float u = s_ojos->xy[i][0], v = s_ojos->xy[i][1];
                float ex = q[0][0] + (q[1][0] - q[0][0]) * u, ey = q[0][1] + (q[3][1] - q[0][1]) * v;
                luz_punto(ex, ey, 4 * S, 1, 0.85f, 0.4f, 2.2f * k);
                luz_punto(ex, ey, 16 * S, 1, 0.4f, 0.1f, 0.9f * k);
                luz_linea(ex - 45 * S, ey, ex + 45 * S, ey, 2 * S, 1, 0.6f, 0.2f, 0.8f * k);
            }
        /* RAYO: electricity on its edges (a new shape 24 times a second) */
        if (rayo && t > 0.6f && t < 2.9f) {
            float k = ventana(t, 0.6f, 2.9f, 0.2f, 0.3f) * (t < 1.55f ? 1.0f : 1.5f);
            uint32_t sem = (uint32_t)(t * 24) * 7u + 1u;
            for (i = 0; i < 5; i++) {
                int e = (int)(sem_f(&sem) * 4) & 3, e2 = (e + 1) & 3;
                float f0 = sem_f(&sem), f1 = sem_f(&sem), tmp;
                if (f0 > f1) { tmp = f0; f0 = f1; f1 = tmp; }
                rayo_dibujar(&sem, q[e][0] + (q[e2][0] - q[e][0]) * f0, q[e][1] + (q[e2][1] - q[e][1]) * f0,
                             q[e][0] + (q[e2][0] - q[e][0]) * f1 + (sem_f(&sem) * 2 - 1) * 60 * S,
                             q[e][1] + (q[e2][1] - q[e][1]) * f1 + (sem_f(&sem) * 2 - 1) * 60 * S,
                             40 * S, 6, 0.9f * S, s_est->col, k);
            }
        }
    }
    if (s_est->estilo == RITUAL && t > 0.8f && t < 1.7f)
        tejas(t, cx, cy, h, s_est->col);
    if (vis && s_est->estilo == DESCENSO && t > 1.6f && t < 2.4f)
        holo((t - 1.6f) / 0.8f, 0.55f);
    if (vis && s_est->estilo == RITUAL && t > 1.7f && t < 2.5f)
        holo((t - 1.7f) / 0.8f, 0.75f);
    if (mago && vis && t > 1.2f && t < 2.0f) {          /* violet lightning on its edges */
        float k = ventana(t, 1.2f, 2.0f, 0.15f, 0.2f) * 0.8f;
        uint32_t sem = (uint32_t)(t * 24) * 7u + 5u;
        for (i = 0; i < 4; i++) {
            int e = (int)(sem_f(&sem) * 4) & 3, e2 = (e + 1) & 3;
            float f0 = sem_f(&sem), f1 = sem_f(&sem), tmp;
            if (f0 > f1) { tmp = f0; f0 = f1; f1 = tmp; }
            rayo_dibujar(&sem, q[e][0] + (q[e2][0] - q[e][0]) * f0, q[e][1] + (q[e2][1] - q[e][1]) * f0,
                         q[e][0] + (q[e2][0] - q[e][0]) * f1 + (sem_f(&sem) * 2 - 1) * 50 * S,
                         q[e][1] + (q[e2][1] - q[e][1]) * f1 + (sem_f(&sem) * 2 - 1) * 50 * S, 35 * S, 6, 0.9f * S,
                         s_est->col, k);
        }
    }
    if (mago && con_boca && t > 1.5f && t < 2.0f) {     /* the dark ball grows on the staff */
        float k = (t - 1.5f) / 0.5f;
        luz_punto(bx, by, (8 + 22 * k) * S, 1, 1, 1, 1.2f * k);
        luz_punto(bx, by, (30 + 70 * k) * S, tr, tg, tb, 0.8f * k);
        luz_anillo(bx, by, (40 - 25 * k) * S, (40 - 25 * k) * S, 2 * S, tr, tg, tb, 0.8f * k);
        sx += sinf(t * 120) * 2 * k * S;
        sy += cosf(t * 110) * 2 * k * S;
    }
    if (mago && t >= 2.0f && t < 2.7f) {                /* and bursts: rings, a flash, rays out */
        float k = (t - 2.0f) / 0.7f, a = 1 - k, px2 = con_boca ? bx : cx, py2 = con_boca ? by : cy;
        uint32_t sem = 0x3A3Au + (uint32_t)(t * 30) * 151u;
        for (i = 0; i < 3; i++) {
            float kk = k * 1.0f - i * 0.12f, r;
            if (kk <= 0)
                continue;
            r = (20 + 620 * eout(kk, 2.4f)) * S;
            luz_anillo(px2, py2, r, r, (16 * (1 - kk) + 2) * S, tr, tg, tb, 1.1f * (1 - kk));
        }
        luz_punto(px2, py2, (40 + 120 * k) * S, tr, tg, tb, 1.2f * a);
        luz_punto(px2, py2, 24 * S, 1, 1, 1, 1.6f * a);
        for (i = 0; i < 40; i++) {
            float an = sem_f(&sem) * 6.2832f, r0 = (60 + 200 * k) * S, r1 = r0 + (80 + 260 * sem_f(&sem)) * S;
            luz_linea(px2 + cosf(an) * r0, py2 + sinf(an) * r0, px2 + cosf(an) * r1, py2 + sinf(an) * r1, 1.3f * S,
                      0.5f + 0.5f * tr, 0.5f + 0.5f * tg, 0.5f + 0.5f * tb, (0.3f + 0.6f * sem_f(&sem)) * a);
        }
        if (k < 0.12f)
            luz_todo(tr, tg, tb, 0.3f * (1 - k / 0.12f));
        sx += sinf(t * 97) * 14 * a * S;
        sy += cosf(t * 83) * 9 * a * S;
    }
    if (vis && mago && t > 2.1f && t < 2.8f)
        holo((t - 2.1f) / 0.7f, 0.45f);
    /* RAYO: the beam out of the mouth, or lightning from the sky, 1.9..2.65 */
    if (rayo && t > 1.9f && t < 2.65f) {
        float k = (t - 1.9f) / 0.75f, a = k < 0.7f ? sinf(3.14159f * fminf(1, k * 1.4f)) : (1 - k) / 0.3f * 0.9f;
        if (con_boca) {
            float dx = cosf(s_est->dir * 3.14159f / 180), dy = sinf(s_est->dir * 3.14159f / 180);
            float largo = eout(k * 4, 2) * (float)OW * 1.3f, ex = bx + dx * largo, ey = by + dy * largo;
            uint32_t sem = (uint32_t)(t * 60) * 977u + 3u;
            float hc[3], nc[3], mn = fminf(tr, fminf(tg, tb)), mx = fmaxf(tr, fmaxf(tg, tb)), kk;
            int c;
            for (c = 0; c < 3; c++) {
                float v = s_est->col[c];
                if (s_est->id == 4007) {        /* Blue-Eyes: the white lightning */
                    hc[c] = 0.5f + 0.5f * v; nc[c] = 1;
                } else {                        /* the others: their colour, saturated */
                    hc[c] = (v - 0.85f * mn) / (mx - 0.85f * mn + 1e-4f);
                    nc[c] = 0.35f + 0.65f * hc[c];
                }
            }
            kk = s_est->id == 4007 ? 1.f : 0.6f;
            luz_linea(bx, by, ex, ey, (90 + 10 * sinf(t * 50)) * S, hc[0], hc[1], hc[2], 0.55f * a);
            luz_linea(bx, by, ex, ey, 34 * S, hc[0], hc[1], hc[2], 1.3f * a);
            luz_linea(bx, by, ex, ey, (12 + 3 * sinf(t * 60)) * S, nc[0], nc[1], nc[2], 2.6f * a * kk);
            luz_punto(bx, by, 34 * S, nc[0], nc[1], nc[2], 2.2f * a * kk);
            for (i = 0; i < 40; i++) {            /* streaks along it */
                float f = sem_f(&sem), o = (sem_f(&sem) * 2 - 1) * 90 * S, l = (60 + 140 * sem_f(&sem)) * S;
                float x0 = bx + dx * largo * f - dy * o, y0 = by + dy * largo * f + dx * o;
                luz_linea(x0, y0, x0 + dx * l, y0 + dy * l, 1.2f * S, 0.8f + 0.2f * tr, 0.8f + 0.2f * tg,
                          0.8f + 0.2f * tb, (0.3f + 0.6f * sem_f(&sem)) * a);
            }
            /* an anamorphic flare across the mouth */
            luz_linea(bx - 420 * S, by, bx + 420 * S, by, 4 * S, tr, tg, tb, 1.4f * a);
            luz_linea(bx - 200 * S, by, bx + 200 * S, by, 16 * S, tr, tg, tb, 0.5f * a);
        } else {
            /* three bolts from the sky to the card's top and sides, flickering */
            int f = (int)(t * 30);
            uint32_t sem = 0xB01Du + (uint32_t)f * 613u;
            if (vis && (f & 3) != 3)
                for (i = 0; i < 3; i++) {
                    /* each from its own side of the sky to its spot on the card's top edge */
                    float tx = q[0][0] + (q[1][0] - q[0][0]) * (0.15f + 0.35f * i), ty = q[0][1];
                    float ox = tx + ((float)(i - 1) * 380 + (sem_f(&sem) * 2 - 1) * 120) * S;
                    rayo_dibujar(&sem, ox, -10, tx, ty, 90 * S, 7, 1.1f * S, s_est->col, 0.9f * a);
                    luz_punto(tx, ty, 26 * S, tr, tg, tb, 0.9f * a);
                }
            luz_punto(cx, cy - h * 0.45f, 160 * S, tr, tg, tb, 0.3f * a);
        }
        if (k < 0.15f)
            luz_todo(0.9f, 0.95f, 1.0f, 0.35f * (1 - k / 0.15f));
        sx += sinf(t * 97) * 16 * a * S;
        sy += cosf(t * 83) * 10 * a * S;
    }
    /* dust and rocks (cover), sparks and embers (light) */
    for (i = 0; i < s_np; i++) {
        part_t *p = &s_p[i];
        float a = 1 - p->t / p->vida;
        if (p->tipo == POLVO) {
            float r = p->tam * (1 + p->t / p->vida * 1.8f);
            tapa_disco(p->x, p->y, r, p->c[0] * 0.5f, p->c[1] * 0.5f, p->c[2] * 0.5f, 0.32f * a);
        } else if (p->tipo == ROCA) {
            tapa_disco(p->x, p->y, p->tam, p->c[0], p->c[1], p->c[2], fminf(1, a * 3));
            tapa_disco(p->x - p->tam * 0.3f, p->y - p->tam * 0.3f, p->tam * 0.4f, 0.55f, 0.45f, 0.32f, fminf(1, a * 3) * 0.6f);
        } else {
            float r = p->tam * (0.5f + 0.5f * a);
            luz_linea(p->x - p->vx * 0.012f, p->y - p->vy * 0.012f, p->x, p->y, r, p->c[0], p->c[1], p->c[2], 1.2f * a);
        }
    }
    bloom();
    /* to the kit: rgb + light, premultiplied */
    {
        unsigned char *px = s_px[s_pub ^ 1];
        for (i = 0; i < (int)n; i++) {
            float r = s_O[4 * i] + s_L[3 * i], g = s_O[4 * i + 1] + s_L[3 * i + 1], b = s_O[4 * i + 2] + s_L[3 * i + 2], a = s_O[4 * i + 3];
            /* a soft shoulder over 0.85 */
            r = r < 0.85f ? r : 0.85f + 0.15f * (1 - expf(-(r - 0.85f) * 4));
            g = g < 0.85f ? g : 0.85f + 0.15f * (1 - expf(-(g - 0.85f) * 4));
            b = b < 0.85f ? b : 0.85f + 0.15f * (1 - expf(-(b - 0.85f) * 4));
            px[4 * i] = (unsigned char)(r * 255.f + 0.5f);
            px[4 * i + 1] = (unsigned char)(g * 255.f + 0.5f);
            px[4 * i + 2] = (unsigned char)(b * 255.f + 0.5f);
            px[4 * i + 3] = (unsigned char)((a > 1 ? 1 : a) * 255.f + 0.5f);
        }
        s_pub ^= 1;
        g_nv2a_as_w = OW;
        g_nv2a_as_h = OH;
        for (i = 0; i < 9; i++)
            g_nv2a_as_carta[i] = (float)s_hm[i];
        g_nv2a_as_carta[9] = s_carta_vis ? s_carta_a : 0.f;
        g_nv2a_as_carta[10] = (float)OW;
        g_nv2a_as_carta[11] = (float)OH;
        g_nv2a_as_tit[9] = 0;            /* no name (the kit draws none) */
        g_nv2a_as_fondo_px = s_fondo[s_pub];
        g_nv2a_as_px = s_px[s_pub];
        g_nv2a_as_mueve[0] = sx / (float)OW;
        g_nv2a_as_mueve[1] = sy / (float)OH;
        InterlockedIncrement((volatile LONG *)&g_nv2a_as_ver);
    }
}

static DWORD WINAPI as_hilo(LPVOID unused)
{
    ULONGLONG ultimo = 0;
    (void)unused;
    for (;;) {
        float t = (float)(GetTickCount64() - s_t0) / 1000.f;
        if (t >= DUR)
            break;
        if (t >= SUELTA)
            InterlockedExchange(&s_espera, 0);
        cuadro(t);
        {
            ULONGLONG ahora = GetTickCount64();
            if (ahora - ultimo < 8u)
                Sleep((DWORD)(8u - (ahora - ultimo)));
            ultimo = GetTickCount64();
        }
    }
    if (s_vista && s_vfondo) {
        /* the last picture: the Perfil's alone (no dim, no card), until it is back */
        ULONGLONG t0 = GetTickCount64();
        s_np = 0;
        InterlockedExchange(&s_vuelto, 0);
        cuadro(DUR + 0.5f);
        g_nv2a_as_carta[9] = 0;
        g_nv2a_as_tit[9] = 0;
        InterlockedExchange(&s_fin, 1);
        while (!s_vuelto && GetTickCount64() - t0 < 1500u)
            Sleep(5);
        Sleep(40);                      /* a frame or two with both */
    }
    g_nv2a_as_px = NULL;
    g_nv2a_as_fondo_px = NULL;
    g_nv2a_as_carta[9] = 0;
    g_nv2a_as_tit[9] = 0;
    g_nv2a_as_mueve[0] = g_nv2a_as_mueve[1] = 0;
    InterlockedIncrement((volatile LONG *)&g_nv2a_as_ver);
    InterlockedExchange(&s_espera, 0);
    InterlockedExchange(&s_fin, 0);
    InterlockedExchange(&s_activo, 0);
    fprintf(stderr, "[AS] terminada\n");
    fflush(stderr);
    return 0;
}

static int empezar(uint32_t k, int lado, int zona)
{
    uint32_t id = MEM16(T_IMAGEN + 2u * k);
    int vw = g_nv2a_vista_w, vh = g_nv2a_vista_h, i;
    if (s_activo)
        return 0;
    if (vw < 64 || vh < 64) {
        vw = 1280;
        vh = 720;
    }
    /* the canvas: the view's shape, 960 wide at most (the CPU's work) */
    OW = vw > 960 ? 960 : vw;
    OH = (int)((float)OW * (float)vh / (float)vw + 0.5f) & ~3;
    OW &= ~3;
    S = (float)OH / 720.f;
    if ((float)OW / (float)OH < 1280.f / 720.f)
        S = (float)OW / 1280.f;
    free(s_carta.p);
    memset(&s_carta, 0, sizeof s_carta);
    if (!carta_hd(id, &s_carta))
        return 0;
    free(s_L); free(s_O); free(s_px[0]); free(s_px[1]); free(s_fondo[0]); free(s_fondo[1]); free(s_bq);
    s_fondo[0] = (unsigned char *)malloc((size_t)OW * OH * 4);
    s_fondo[1] = (unsigned char *)malloc((size_t)OW * OH * 4);
    s_L = (float *)malloc((size_t)OW * OH * 3 * sizeof(float));
    s_O = (float *)malloc((size_t)OW * OH * 4 * sizeof(float));
    s_px[0] = (unsigned char *)malloc((size_t)OW * OH * 4);
    s_px[1] = (unsigned char *)malloc((size_t)OW * OH * 4);
    s_bq = (float *)malloc((size_t)(OW / 4) * (OH / 4) * 3 * sizeof(float));
    if (!s_L || !s_O || !s_px[0] || !s_px[1] || !s_fondo[0] || !s_fondo[1] || !s_bq)
        return 0;
    /* the card to the kit (it makes its texture once per card) */
    g_nv2a_as_carta_w = s_carta.w;
    g_nv2a_as_carta_h = s_carta.h;
    g_nv2a_as_carta_px = s_carta.p;
    InterlockedIncrement((volatile LONG *)&g_nv2a_as_carta_ver);
    s_id = id;
    s_lado = lado;
    s_zona = zona;
    s_ojos = NULL;
    s_est = estilo_de(id, k);
    free(s_vfondo);
    s_vfondo = NULL;
    if (s_vista && s_pf_px && s_pf_w > 0 && s_pf_h > 0) {
        s_vfondo = (unsigned char *)malloc((size_t)OW * OH * 4);
        if (s_vfondo) {
            int x, y;
            for (y = 0; y < OH; y++)
                for (x = 0; x < OW; x++) {       /* a box of the source px under each */
                    int sx0 = x * s_pf_w / OW, sx1 = (x + 1) * s_pf_w / OW, sy0 = y * s_pf_h / OH, sy1 = (y + 1) * s_pf_h / OH;
                    unsigned acc[4] = { 0, 0, 0, 0 }, cnt = 0, c;
                    int a, b;
                    if (sx1 <= sx0) sx1 = sx0 + 1;
                    if (sy1 <= sy0) sy1 = sy0 + 1;
                    for (b = sy0; b < sy1 && b < s_pf_h; b++)
                        for (a = sx0; a < sx1 && a < s_pf_w; a++, cnt++)
                            for (c = 0; c < 4; c++)
                                acc[c] += s_pf_px[4 * ((size_t)b * s_pf_w + a) + c];
                    for (c = 0; c < 4; c++)
                        s_vfondo[4 * ((size_t)y * OW + x) + c] = (unsigned char)(cnt ? acc[c] / cnt : 0);
                }
        }
    }
    free(s_pf_px);
    s_pf_px = NULL;
    for (i = 0; i < (int)(sizeof k_ojos / sizeof k_ojos[0]); i++)
        if (k_ojos[i].id == id)
            s_ojos = &k_ojos[i];
    nombre_carta(k, s_nombre, 128);
    s_rnd = 0x9E3779B9u ^ id;
    s_np = 0;
    s_sim = 0;
    s_golpe_hecho = 0;
    grietas_hacer();
    tejas_hacer();
    (void)vh;
    s_ver0 = g_nv2a_as_ver;
    InterlockedExchange(&s_fin, 0);
    s_t0 = GetTickCount64();
    InterlockedExchange(&s_activo, 1);
    InterlockedExchange(&s_espera, 1);
    InterlockedExchange(&s_entregada, 0);
    if (s_hilo)
        CloseHandle(s_hilo);
    s_hilo = CreateThread(NULL, 0, as_hilo, NULL, 0, NULL);
    if (!s_hilo) {
        InterlockedExchange(&s_activo, 0);
        InterlockedExchange(&s_espera, 0);
        return 0;
    }
    fprintf(stderr, "[AS] carta %u (k %u) del %s en la casilla %d: lienzo %dx%d, \"%ls\"%s, estilo %s\n", id, k,
            lado ? "rival" : "jugador", zona, OW, OH, s_nombre, s_ojos ? ", con ojos" : "",
            s_est->estilo == RAYO ? (s_est->boca[0] >= 0 ? "rayo (boca)" : "rayo (cielo)")
            : s_est->estilo == DESCENSO ? "descenso" : s_est->estilo == RITUAL ? "ritual"
            : s_est->estilo == MAGO ? "magia oscura" : "carga");
    fflush(stderr);
    return 1;
}

/* ---- the handoff to the game's own placement -------------------------------------- */
/* order 0x90 (sub_000CDE60) step 1 starts the game's placement with
 * sub_00046B30: from then on the card is the game's */
void sub_000CDE60_pad(void);
void sub_00046B30_pad(void);

void sub_000CDE60(void) { sub_000CDE60_pad(); }

void sub_00046B30(void)
{
    if (s_activo && !s_entregada) {
        InterlockedExchange(&s_entregada, 1);
        fprintf(stderr, "[AS] la carta, al juego (su colocación)\n");
        fflush(stderr);
    }
    sub_00046B30_pad();
}

/* ---- the summon dispatcher ------------------------------------------------------- */
void sub_00095A30_pad(void);
static uint16_t s_antes[2][5];          /* the squares before this summon (its card's square) */
static int s_llamada_vista;             /* this summon already looked at */

static void ret16(uint32_t v)
{
    g_eax = (g_eax & 0xFFFF0000u) | (v & 0xFFFFu);
    g_esp += 4;
}

void sub_00095A30(void)
{
    uint16_t f = MEM16(0x0066BA8Eu);
    uint32_t tipo = (f >> 2) & 7u, est = (f >> 5) & 0x7Fu;
    if (s_espera) {
        ret16(1);                       /* a summon in progress: the duel waits */
        return;
    }
    if (!(f & 1u)) {
        s_llamada_vista = 0;
    } else if (!s_llamada_vista) {
        int p = (int)(MEM32(0x0066BA7Cu) & 1u), z;
        uint32_t k = MEM16(0x0066BA80u) & 0x7FFFu;
        /* 2026-11-19 (the user: "primero tocó donde va y luego apareció la
         * animación"): before the card is placed, where the machine moves
         * the cursor to its square (sub_00086F50(p, 0, square)) and plays the
         * game's own order 0x4C: Normal state 1, by Tribute state 3 */
        /* 2026-11-19 again ("se vio luego de apoyarse la carta"): the
         * square is written one step before (order 0x90, sub_00081150):
         * Normal state 0, by Tribute state 2 (after its tributes' question) */
        int disparo = (tipo == 1u && est == 0u) || (tipo == 2u && est == 2u);
        if (!disparo) {
            /* before it is placed: what the squares hold */
            for (z = 0; z < 5; z++)
                s_antes[p][z] = MEM16(CASILLA(p, z)) & 0x7FFFu;
            if (tipo >= 3u && est == 1u) {
                static uint32_t dicho;
                if (dicho != (tipo << 16 | k)) {
                    dicho = tipo << 16 | k;
                    fprintf(stderr, "[AS] llamada tipo %u (carta k %u, jugador %d): todavía sin animación\n", tipo, k, p);
                }
            }
        } else if (MEM32(0x0066BA7Cu) & 0x4000u) {
            uint32_t id = MEM16(T_IMAGEN + 2u * k);
            int m = modelo_carta(id), quiere = ygo_mascota_modelo_lado(p), i, ya = 0;
            int zona = (int)((MEM32(0x0066BA7Cu) >> 1) & 0x1Fu);      /* the square it goes to */
            s_llamada_vista = 1;
            if (zona > 4)
                zona = -1;
            if (s_modo != 2 && m >= 0 && m == quiere) {
                for (i = 0; i < s_nvistas[p]; i++)
                    ya |= s_vistas[p][i] == id;
                if (s_modo == 1 && ya) {
                    fprintf(stderr, "[AS] carta %u ya mostrada en este duelo\n", id);
                } else if ((InterlockedExchange(&s_vista, 0), empezar(k, p, zona))) {
                    if (!ya && s_nvistas[p] < 8)
                        s_vistas[p][s_nvistas[p]++] = id;
                    ret16(1);
                    return;
                }
            }
        }
    }
    sub_00095A30_pad();
}

/* ---- the Perfil's "Ver Invocación" (perfil.c) ---------------------------------- */
int ygo_mascota_modelo_de(int i);       /* mascota.c */

int ygo_as_activa(void) { return s_activo != 0; }

/* plays mascot i's ace card over whatever is on screen; 1 when it started */
int ygo_as_ver(int masc)
{
    int m = ygo_mascota_modelo_de(masc);
    uint32_t i, id = 0, k;
    if (m < 0 || s_activo)
        return 0;
    for (i = 0; i < 2000u; i++) {       /* the model's card (the pair table) */
        uint16_t a = MEM16(0x0044FBB0u + 4u * i), b = MEM16(0x0044FBB2u + 4u * i);
        if (a < 4000u || a > 5300u)
            break;
        if ((int)b == m) {
            id = a;
            break;
        }
    }
    for (k = 1; id && k < 1400u; k++)   /* its k */
        if (MEM16(T_IMAGEN + 2u * k) == id)
            break;
    if (!id || k >= 1400u) {
        fprintf(stderr, "[AS] Ver Invocación: sin carta para la mascota %d (modelo %d)\n", masc, m);
        fflush(stderr);
        return 0;
    }
    InterlockedExchange(&s_vista, 1);
    if (!empezar(k, 0, -1)) {
        InterlockedExchange(&s_vista, 0);
        return 0;
    }
    InterlockedExchange(&s_espera, 0);  /* nothing waits for a preview */
    fprintf(stderr, "[AS] Ver Invocación desde el Perfil (mascota %d)\n", masc);
    fflush(stderr);
    return 1;
}

/* the same with the Perfil's picture under it (perfil.c; px is ours from here on) */
int ygo_as_ver_con(int masc, unsigned char *px, int w, int h)
{
    if (s_activo)
        return 0;
    free(s_pf_px);
    s_pf_px = px; s_pf_w = w; s_pf_h = h;
    if (!ygo_as_ver(masc)) {
        if (s_pf_px != px)              /* empezar took it and let it go: nothing left to free */
            return 1;
        s_pf_px = NULL;                 /* not taken: the caller frees it */
        return 0;
    }
    return 1;
}

/* perfil.c: hide its windows now (the animation's first picture is on screen,
 * and its end has not come) */
int ygo_as_tapa(void)
{
    return s_activo && !s_fin && (int)(g_nv2a_as_dibujado - s_ver0) > 0;
}

/* perfil.c: its windows are shown again */
void ygo_as_perfil_vuelto(void) { InterlockedExchange(&s_vuelto, 1); }

