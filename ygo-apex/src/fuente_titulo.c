/* La letra de los títulos del juego (la de "Duelo (Individual)") para el
 * Perfil (2026-11-16, el usuario: "usa la fuente del juego para los numeros
 * y las cosas de este menu"). Como fuente.c, con la página FED7C5881DA64D4A
 * (tools/fuente_titulo.py -> fuente_titulo.h: celdas de 30 x 26, cada letra
 * con su ancho y su altura en la línea; la base en la fila 18 de la página).
 * h como en fuente.c: mayúsculas de 0,7 h. */
#include <math.h>
#include <string.h>
#include <wchar.h>

#include "fuente.h"
#include "fuente_titulo.h"
#include "fuente_titulo4.h"   /* the 4x page: letters over 2 px a 1x px (the ace card's name) */

#define PAD 3.f                         /* rows over the capitals (their accents) */
#define BASE ((float)FT_MAYUS / 0.7f)   /* 1x page rows per h */
#define ALTO_LINEA (PAD + (float)FT_ABAJO + 1.f)
#define ESPACIO 7.f
#define SEP 1.f

static int indice(wchar_t wc)
{
    char b = 0;
    BOOL usado = FALSE;
    unsigned u;
    if (wc < 0x80)
        u = (unsigned)wc;
    else if (WideCharToMultiByte(1252, WC_NO_BEST_FIT_CHARS, &wc, 1, &b, 1, "?", &usado) != 1 || usado)
        u = '?';
    else
        u = (unsigned char)b;
    if (u < 0x21u || u == 0x7Fu)
        return -1;
    return u <= 0x7Eu ? (int)(u - 0x21u) : (int)(u - 0x22u);
}

int ft_linea(int h)
{
    return (int)(ALTO_LINEA * (float)h / BASE + 0.5f);
}

static float ancho_f(const wchar_t *s, int n, float e)
{
    float x = 0.f;
    int k;
    for (k = 0; k < n && s[k]; k++) {
        int i = indice(s[k]);
        x += i < 0 ? ESPACIO * e : ((float)k_ft_ancho[i] + SEP) * e;
    }
    return x;
}

int ft_ancho(const wchar_t *s, int h)
{
    return (int)ceilf(ancho_f(s, (int)wcslen(s), (float)h / BASE)) + 1;
}

typedef struct { const unsigned char *px; int w, h, mul; } pagina_t;

static float muestra(const pagina_t *pg, float u, float v, int cu, int cv)
{
    int u0 = (int)floorf(u), v0 = (int)floorf(v), k;
    float fu = u - (float)u0, fv = v - (float)v0, r = 0.f;
    for (k = 0; k < 4; k++) {
        int x = u0 + (k & 1), y = v0 + (k >> 1);
        float w = ((k & 1) ? fu : 1.f - fu) * ((k >> 1) ? fv : 1.f - fv);
        if (x < cu || y < cv || x >= cu + pg->mul * FT_CW || y >= cv + pg->mul * FT_CH || x >= pg->w || y >= pg->h)
            continue;
        r += w * (float)pg->px[(size_t)y * (size_t)pg->w + (size_t)x];
    }
    return r;
}

void ft_dibujar(unsigned char *bits, int W, int H, const RECT *rc, const wchar_t *s0, int h, unsigned canales)
{
    float e = (float)h / BASE, x, top;
    wchar_t s[512];
    int n, k;
    if (!bits || !s0 || h <= 0)
        return;
    /* 2026-11-19 (the user: the ace card's name "no se ve nítido"): over 2
     * px a 1x px the 2x page would be stretched: the 4x one */
    pagina_t pg = { k_ft_px, FT_W, FT_H, 2 };
    if (e > 2.f) {
        pg.px = k_ft_px4; pg.w = FT4_W; pg.h = FT4_H; pg.mul = 4;
    }
    wcsncpy(s, s0, 511);
    s[511] = 0;
    n = (int)wcslen(s);
    x = (float)rc->left;
    top = (float)rc->top;
    for (k = 0; k < n; k++) {
        int i = indice(s[k]), cu, cv, y0, y1, x0, x1, yy, xx;
        float gtop;
        if (i < 0) {
            x += ESPACIO * e;
            continue;
        }
        cu = (i % FT_COLS) * FT_CW * pg.mul;
        cv = (i / FT_COLS) * FT_CH * pg.mul;
        gtop = top + (PAD + (float)k_ft_y[i]) * e;
        x0 = (int)floorf(x);
        x1 = (int)ceilf(x + (float)(k_ft_ancho[i] + 1) * e);
        y0 = (int)floorf(gtop);
        y1 = (int)ceilf(gtop + (float)FT_CH * e);
        if (x0 < rc->left) x0 = rc->left;
        if (y0 < rc->top) y0 = rc->top;
        if (x1 > rc->right) x1 = rc->right;
        if (y1 > rc->bottom) y1 = rc->bottom;
        if (x0 < 0) x0 = 0;
        if (y0 < 0) y0 = 0;
        if (x1 > W) x1 = W;
        if (y1 > H) y1 = H;
        for (yy = y0; yy < y1; yy++)
            for (xx = x0; xx < x1; xx++) {
                float u = ((float)xx + 0.5f - x) / e * (float)pg.mul + (float)cu - 0.5f;
                float v = ((float)yy + 0.5f - gtop) / e * (float)pg.mul + (float)cv - 0.5f;
                float a = muestra(&pg, u, v, cu, cv);
                unsigned char c, *p;
                if (a < 1.f)
                    continue;
                c = (unsigned char)(a > 254.5f ? 255 : (int)(a + 0.5f));
                p = bits + 4u * ((size_t)yy * (size_t)W + (size_t)xx);
                if ((canales & FJ_B) && c > p[0]) p[0] = c;
                if ((canales & FJ_G) && c > p[1]) p[1] = c;
                if ((canales & FJ_R) && c > p[2]) p[2] = c;
            }
        x += ((float)k_ft_ancho[i] + SEP) * e;
    }
}
