/* La letra del juego para nuestros carteles (2026-11-15, el usuario: "estas
 * letras y fuentes que usaste podés usarlas para todo lo que hicimos de cartas
 * nuevas y los sobres").
 *
 * La página de letras del juego (tools/fuente_juego.py -> fuente_juego.h:
 * 0x21..0x7E y 0x80..0xFF de Windows-1252 en celdas de 26 px, 15 por fila, a
 * 2x desde la versión mejorada), cada letra con su ancho y su altura en la
 * línea (base en la fila 14). Se escala por h (el tamaño de CreateFont(-h):
 * mayúsculas de 0,7 h, como Segoe UI) y se muestrea bilineal.
 */
#include <math.h>
#include <string.h>
#include <wchar.h>

#include "fuente.h"
#include "fuente_juego.h"

#define BASE 20.f                       /* 1x page rows per h */
#define ALTO_LINEA 24.f                 /* the line: 4 rows over the cell for the capitals' accents */
#define ESPACIO 6.f
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
    if (u < FJ_PRIMERO || u == 0x7Fu)
        return -1;                      /* a space (or a control) */
    return u <= 0x7Eu ? (int)(u - FJ_PRIMERO) : (int)(u - FJ_PRIMERO - 1u);
}

int fj_linea(int h)
{
    return (int)(ALTO_LINEA * (float)h / BASE + 0.5f);
}

static float ancho_f(const wchar_t *s, int n, float e)
{
    float x = 0.f;
    int k;
    for (k = 0; k < n && s[k]; k++) {
        int i = indice(s[k]);
        x += i < 0 ? ESPACIO * e : ((float)k_fj_ancho[i] + SEP) * e;
    }
    return x;
}

int fj_ancho(const wchar_t *s, int h)
{
    return (int)ceilf(ancho_f(s, (int)wcslen(s), (float)h / BASE)) + 1;
}

void fj_medir(const wchar_t *s, int h, SIZE *sz)
{
    sz->cx = fj_ancho(s, h);
    sz->cy = fj_linea(h);
}

static float muestra(float u, float v, int cu, int cv)
{
    /* bilinear in the 2x page, inside the glyph's cell only */
    int u0 = (int)floorf(u), v0 = (int)floorf(v), k;
    float fu = u - (float)u0, fv = v - (float)v0, r = 0.f;
    for (k = 0; k < 4; k++) {
        int x = u0 + (k & 1), y = v0 + (k >> 1);
        float w = ((k & 1) ? fu : 1.f - fu) * ((k >> 1) ? fv : 1.f - fv);
        if (x < cu || y < cv || x >= cu + 2 * FJ_CELDA || y >= cv + 2 * FJ_CELDA || x >= FJ_LADO || y >= FJ_LADO)
            continue;
        r += w * (float)k_fj_px[(size_t)y * FJ_LADO + (size_t)x];
    }
    return r;
}

void fj_dibujar(unsigned char *bits, int W, int H, const RECT *rc, const wchar_t *s0, int h,
                unsigned canales, unsigned flags)
{
    float e = (float)h / BASE, x, top, total;
    wchar_t s[512];
    int n, k;
    if (!bits || !s0 || h <= 0)
        return;
    wcsncpy(s, s0, 511);
    s[511] = 0;
    n = (int)wcslen(s);
    total = ancho_f(s, n, e);
    if ((flags & FJ_ELIPSIS) && total > (float)(rc->right - rc->left)) {
        /* cut, with "..." (the page's own dots) */
        float puntos = ancho_f(L"...", 3, e);
        while (n > 0 && ancho_f(s, n, e) + puntos > (float)(rc->right - rc->left))
            n--;
        if (n > 507)
            n = 507;
        wcscpy(s + n, L"...");
        n += 3;
        total = ancho_f(s, n, e);
    }
    x = (float)rc->left;
    if (flags & FJ_CENTRO)
        x += ((float)(rc->right - rc->left) - total) * 0.5f;
    top = (float)rc->top;
    if (flags & FJ_VCENTRO)
        top += ((float)(rc->bottom - rc->top) - ALTO_LINEA * e) * 0.5f;
    for (k = 0; k < n; k++) {
        int i = indice(s[k]), cu, cv, y0, y1, x0, x1, yy, xx;
        float gtop;
        if (i < 0) {
            x += ESPACIO * e;
            continue;
        }
        cu = (i % FJ_COLS) * FJ_CELDA * 2;
        cv = (i / FJ_COLS) * FJ_CELDA * 2;
        gtop = top + (4.f + (float)k_fj_y[i]) * e;
        x0 = (int)floorf(x);
        x1 = (int)ceilf(x + (float)(k_fj_ancho[i] + 1) * e);
        y0 = (int)floorf(gtop);
        y1 = (int)ceilf(gtop + (float)FJ_CELDA * e);
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
                float u = ((float)xx + 0.5f - x) / e * 2.f + (float)cu - 0.5f;
                float v = ((float)yy + 0.5f - gtop) / e * 2.f + (float)cv - 0.5f;
                float a = muestra(u, v, cu, cv);
                unsigned char c, *p;
                if (a < 1.f)
                    continue;
                c = (unsigned char)(a > 254.5f ? 255 : (int)(a + 0.5f));
                p = bits + 4u * ((size_t)yy * (size_t)W + (size_t)xx);
                if ((canales & FJ_B) && c > p[0]) p[0] = c;
                if ((canales & FJ_G) && c > p[1]) p[1] = c;
                if ((canales & FJ_R) && c > p[2]) p[2] = c;
            }
        x += ((float)k_fj_ancho[i] + SEP) * e;
    }
}
