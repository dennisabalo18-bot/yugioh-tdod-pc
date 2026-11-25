/*
 * cartel.c - small signs over the game (2026-11-12): the Biblioteca's "who
 * gives this card" (with the opponents' portraits, or their names only) and
 * the "new card" notice of the rewards (premios_extra.c).
 *
 * The same look as premios.c's opponent sign: a rounded panel, dark brown
 * with a gold edge, smooth (by its distance to the edge), the first line in
 * gold and the rest in white, 92 % opaque, in a layered click-through window
 * of its own over the game (the game's picture is not touched). Each slot
 * stays up for the time its producer gave (a screen calling every frame
 * keeps it there), while the game is in front and F2 is closed.
 *
 * The portraits are the disc's own (cf_anim/small/png/cfsNN.png, one per
 * character in the order of texts 86-113; the first face is the top 256 x
 * 128 of the sheet), read with WIC from the game files' folder.
 */
#define COBJMACROS
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include "capas.h"   /* the recording mode: panels into the picture */
#include <wincodec.h>
#include "cartel.h"
#include "fuente.h"                 /* the game's own letters (2026-11-15) */

#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "ole32.lib")

int xbox_MenuIsOpen(void);
extern const char *g_ygo_game_dir;      /* main.c: the game files' folder */

#define FOTO_W 256
#define FOTO_H 128
#define NFOTOS 28

static const GUID k_clsid_wic = { 0xcacaf262, 0x9370, 0x4615, { 0xa1, 0x3b, 0x9f, 0x55, 0x39, 0xda, 0x4c, 0x0a } };
static const GUID k_iid_wic = { 0xec5ec8a9, 0xc395, 0x4314, { 0x9c, 0x77, 0x54, 0xd7, 0xa9, 0x35, 0xff, 0x70 } };
static const GUID k_pbgra = { 0x6fddc324, 0x4e03, 0x4bfe, { 0xb1, 0x85, 0x3d, 0x77, 0x76, 0x8d, 0xc9, 0x10 } };

static CRITICAL_SECTION s_cs;
static int s_cs_ok;
static ygo_cartel s_c[CARTEL_N];
static ULONGLONG s_hasta[CARTEL_N];

void ygo_cartel_poner(int slot, const ygo_cartel *c, unsigned ms)
{
    if (!s_cs_ok || slot < 0 || slot >= CARTEL_N)
        return;
    EnterCriticalSection(&s_cs);
    s_c[slot] = *c;
    s_hasta[slot] = GetTickCount64() + ms;
    LeaveCriticalSection(&s_cs);
}

/* the badges (premios_extra.c: the Biblioteca's new cards) */
static float s_mx[CARTEL_MARCAS], s_my[CARTEL_MARCAS], s_mancho;
static int s_mn;
static char s_mtxt[32];
static ULONGLONG s_mhasta;

void ygo_cartel_marcas(const float *gx, const float *gy, int n, const char *texto, float ancho, unsigned ms)
{
    if (!s_cs_ok)
        return;
    if (n > CARTEL_MARCAS)
        n = CARTEL_MARCAS;
    EnterCriticalSection(&s_cs);
    memcpy(s_mx, gx, sizeof(float) * (size_t)n);
    memcpy(s_my, gy, sizeof(float) * (size_t)n);
    s_mn = n;
    s_mancho = ancho;
    snprintf(s_mtxt, sizeof s_mtxt, "%s", texto);
    s_mhasta = GetTickCount64() + ms;
    LeaveCriticalSection(&s_cs);
}

/* at once (2026-11-16: they stayed 250 ms over the card's open detail) */
void ygo_cartel_marcas_quitar(void)
{
    if (!s_cs_ok)
        return;
    EnterCriticalSection(&s_cs);
    s_mhasta = 0;
    LeaveCriticalSection(&s_cs);
}

/* ---- the portraits ------------------------------------------------------ */

static unsigned char *s_foto[NFOTOS];   /* FOTO_W x FOTO_H, premultiplied BGRA */
static int s_foto_tried[NFOTOS];
static float s_foto_c[NFOTOS];         /* the centre of what shows, 0..1 of the width (the faces sit right, 0.60..0.69) */
static IWICImagingFactory *s_wic;

static unsigned char *foto(int ch)
{
    char a[MAX_PATH];
    wchar_t p[MAX_PATH];
    IWICBitmapDecoder *dec = NULL;
    IWICBitmapFrameDecode *fr = NULL;
    IWICBitmapSource *cv = NULL;
    UINT w = 0, h = 0;
    unsigned char *buf = NULL;
    if (ch < 0 || ch >= NFOTOS)
        return NULL;
    if (s_foto_tried[ch])
        return s_foto[ch];
    s_foto_tried[ch] = 1;
    if (!s_wic) {
        CoInitializeEx(NULL, COINIT_MULTITHREADED);
        if (FAILED(CoCreateInstance(&k_clsid_wic, NULL, CLSCTX_INPROC_SERVER, &k_iid_wic, (void **)&s_wic)))
            s_wic = NULL;
    }
    snprintf(a, sizeof a, "%s\\cf_anim\\small\\png\\cfs%02d.png", g_ygo_game_dir ? g_ygo_game_dir : ".", ch);
    if (!s_wic || !MultiByteToWideChar(CP_ACP, 0, a, -1, p, MAX_PATH))
        goto fin;
    if (FAILED(IWICImagingFactory_CreateDecoderFromFilename(s_wic, p, NULL, GENERIC_READ,
                                                            WICDecodeMetadataCacheOnDemand, &dec)))
        goto fin;
    if (FAILED(IWICBitmapDecoder_GetFrame(dec, 0, &fr)))
        goto fin;
    if (FAILED(WICConvertBitmapSource(&k_pbgra, (IWICBitmapSource *)fr, &cv)))
        goto fin;
    if (FAILED(IWICBitmapSource_GetSize(cv, &w, &h)) || w < FOTO_W || h < FOTO_H)
        goto fin;
    buf = (unsigned char *)malloc(FOTO_W * FOTO_H * 4);
    if (buf) {
        WICRect r = { 0, 0, FOTO_W, FOTO_H };
        if (FAILED(IWICBitmapSource_CopyPixels(cv, &r, FOTO_W * 4, FOTO_W * FOTO_H * 4, buf))) {
            free(buf);
            buf = NULL;
        }
    }
fin:
    if (cv)
        IWICBitmapSource_Release(cv);
    if (fr)
        IWICBitmapFrameDecode_Release(fr);
    if (dec)
        IWICBitmapDecoder_Release(dec);
    s_foto_c[ch] = 0.5f;
    if (buf) {
        int x, y, x0 = FOTO_W, x1 = -1;
        for (x = 0; x < FOTO_W; ++x)
            for (y = 0; y < FOTO_H; ++y)
                if (buf[4 * (y * FOTO_W + x) + 3] > 40) {
                    if (x < x0) x0 = x;
                    x1 = x;
                    break;
                }
        if (x1 >= x0)
            s_foto_c[ch] = (float)(x0 + x1 + 1) * 0.5f / (float)FOTO_W;
    }
    fprintf(stderr, "[CARTEL] retrato %d: %s (%s)\n", ch, buf ? "cargado" : "no se pudo leer", a);
    s_foto[ch] = buf;
    return buf;
}

/* the portrait over the panel at (x, y), w x h, by area (box filter) */
static void pegar_foto(unsigned char *bits, int bw, int bh, int x, int y, int w, int h, const unsigned char *src)
{
    int dx, dy;
    for (dy = 0; dy < h; ++dy) {
        int sy0 = dy * FOTO_H / h, sy1 = (dy + 1) * FOTO_H / h;
        if (y + dy < 0 || y + dy >= bh)
            continue;
        if (sy1 <= sy0)
            sy1 = sy0 + 1;
        for (dx = 0; dx < w; ++dx) {
            int sx0 = dx * FOTO_W / w, sx1 = (dx + 1) * FOTO_W / w, sx, sy, n = 0;
            unsigned s[4] = { 0, 0, 0, 0 };
            unsigned char *d;
            if (x + dx < 0 || x + dx >= bw)
                continue;
            if (sx1 <= sx0)
                sx1 = sx0 + 1;
            for (sy = sy0; sy < sy1; ++sy)
                for (sx = sx0; sx < sx1; ++sx) {
                    const unsigned char *q = src + 4 * (sy * FOTO_W + sx);
                    s[0] += q[0]; s[1] += q[1]; s[2] += q[2]; s[3] += q[3];
                    ++n;
                }
            d = bits + 4 * ((y + dy) * bw + x + dx);
            {
                unsigned a = s[3] / n, k = 255 - a;
                d[0] = (unsigned char)(s[0] / n + d[0] * k / 255);
                d[1] = (unsigned char)(s[1] / n + d[1] * k / 255);
                d[2] = (unsigned char)(s[2] / n + d[2] * k / 255);
                d[3] = (unsigned char)(a + d[3] * k / 255);
            }
        }
    }
}

/* ---- the windows ---------------------------------------------------------- */

typedef struct {
    HWND win;
    HDC mem;
    HBITMAP bmp, old;
    unsigned char *bits;
    int bw, bh, shown;
    ygo_cartel last;
    LONG last_w, last_h;
    int valid;
    POINT puesto;                       /* where it was last put (UpdateLayeredWindow) */
    int pintado;                        /* repainted since */
} ventana;

static void wide(const char *u8, wchar_t *w, int n)
{
    if (!MultiByteToWideChar(CP_UTF8, 0, u8, -1, w, n))
        w[0] = 0;
}

#define MAXWRAP 12

/* draws c into v's bitmap for a game of cr's size; 0 if it could not */
static int pintar(ventana *v, HDC screen, const ygo_cartel *c, const RECT *cr)
{
    static HFONT f_grande, f_chica;
    static int fh;
    int h = cr->bottom / 34, hs, pad, gap, lh, ls, maxw, W, H, y, i, x_grid;
    int tile = 0, ph = 0, cols = 0, rows = 0, gridw = 0, gridh = 0, nwrap = 0;
    wchar_t wt[160], wl[CARTEL_LINEAS][200], wrap[MAXWRAP][400];
    SIZE sz;
    HFONT of;
    BITMAPINFO bi;
    RECT rc;

    if (h < 14)
        h = 14;
    hs = h * 4 / 5;
    if (h != fh) {
        if (f_grande)
            DeleteObject(f_grande);
        if (f_chica)
            DeleteObject(f_chica);
        f_grande = CreateFontA(-h, 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                          CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH, "Segoe UI");
        f_chica = CreateFontA(-hs, 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                            CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH, "Segoe UI");
        fh = h;
    }
    pad = h / 2;
    gap = pad / 2 > 2 ? pad / 2 : 2;
    maxw = cr->right * 45 / 100;

    /* measure */
    (void)of;
    wide(c->titulo, wt, 160);
    lh = fj_linea(h);
    W = fj_ancho(wt, h);
    for (i = 0; i < c->nlin && i < CARTEL_LINEAS; ++i) {
        wide(c->lineas[i], wl[i], 200);
        sz.cx = fj_ancho(wl[i], h);
        if (sz.cx > W)
            W = sz.cx;
    }
    if (c->nitems > 0 && c->fotos) {
        ls = fj_linea(hs);
        tile = h * 4;
        ph = tile / 2;
        cols = (maxw + gap) / (tile + gap);
        if (cols < 1)
            cols = 1;
        if (cols > 6)
            cols = 6;
        if (cols > c->nitems)
            cols = c->nitems;
        rows = (c->nitems + cols - 1) / cols;
        gridw = cols * tile + (cols - 1) * gap;
        gridh = rows * (ph + ls) + (rows - 1) * gap;
        if (gridw > W)
            W = gridw;
    } else if (c->nitems > 0) {
        /* the names joined with ", ", wrapped to maxw */
        int wmax = maxw > W ? maxw : W;
        wrap[0][0] = 0;
        nwrap = 1;
        for (i = 0; i < c->nitems && i < CARTEL_ITEMS; ++i) {
            wchar_t nm[48], probe[400];
            wide(c->item[i], nm, 48);
            _snwprintf(probe, 400, L"%ls%ls%ls%ls", wrap[nwrap - 1], wrap[nwrap - 1][0] ? L", " : L"",
                       nm, i + 1 < c->nitems ? L"," : L"");
            probe[399] = 0;
            sz.cx = fj_ancho(probe, h);
            if (sz.cx > wmax && wrap[nwrap - 1][0] && nwrap < MAXWRAP) {
                wcscat(wrap[nwrap - 1], L",");
                wrap[nwrap][0] = 0;
                ++nwrap;
            }
            if (wrap[nwrap - 1][0])
                wcsncat(wrap[nwrap - 1], L", ", 400 - wcslen(wrap[nwrap - 1]) - 1);
            wcsncat(wrap[nwrap - 1], nm, 400 - wcslen(wrap[nwrap - 1]) - 1);
        }
        for (i = 0; i < nwrap; ++i) {
            sz.cx = fj_ancho(wrap[i], h);
            if (sz.cx > W)
                W = sz.cx;
        }
    }
    W += 2 * pad;
    x_grid = pad;
    H = (pad - pad / 6) + lh * (1 + (c->nlin < CARTEL_LINEAS ? c->nlin : CARTEL_LINEAS));
    if (gridh)
        H += gap + gridh;
    H += nwrap * lh;
    H += pad - pad / 6;

    if (W != v->bw || H != v->bh || !v->bmp) {
        if (v->bmp) {
            SelectObject(v->mem, v->old);
            DeleteObject(v->bmp);
            v->bmp = NULL;
        }
        memset(&bi, 0, sizeof bi);
        bi.bmiHeader.biSize = sizeof bi.bmiHeader;
        bi.bmiHeader.biWidth = W;
        bi.bmiHeader.biHeight = -H;
        bi.bmiHeader.biPlanes = 1;
        bi.bmiHeader.biBitCount = 32;
        bi.bmiHeader.biCompression = BI_RGB;
        v->bmp = CreateDIBSection(screen, &bi, DIB_RGB_COLORS, (void **)&v->bits, NULL, 0);
        if (!v->bmp) {
            v->bw = v->bh = 0;
            return 0;
        }
        v->old = (HBITMAP)SelectObject(v->mem, v->bmp);
        v->bw = W;
        v->bh = H;
    }

    /* the text alone first, as coverage: the title in pure red, the rest in
     * pure green on black, grayscale anti-aliasing (premios.c's way) */
    memset(v->bits, 0, (size_t)W * (size_t)H * 4u);
    y = pad - pad / 6;
    rc.left = pad; rc.top = y; rc.right = W - pad; rc.bottom = y + lh;
    fj_dibujar(v->bits, W, H, &rc, wt, h, FJ_R, c->lado == 2 ? FJ_CENTRO : 0);
    y += lh;
    for (i = 0; i < c->nlin && i < CARTEL_LINEAS; ++i) {
        rc.top = y; rc.bottom = y + lh;
        fj_dibujar(v->bits, W, H, &rc, wl[i], h, FJ_G, c->lado == 2 ? FJ_CENTRO : 0);
        y += lh;
    }
    for (i = 0; i < nwrap; ++i) {
        rc.top = y; rc.bottom = y + lh;
        fj_dibujar(v->bits, W, H, &rc, wrap[i], h, FJ_G, 0);
        y += lh;
    }
    if (gridh) {
        /* one portrait: in the middle of the sign; two or more: from the left */
        if (c->nitems == 1)
            x_grid = (W - gridw) / 2;
        y += gap;
        for (i = 0; i < c->nitems && i < CARTEL_ITEMS; ++i) {
            wchar_t nm[48];
            int cx = x_grid + (i % cols) * (tile + gap), cy = y + (i / cols) * (ph + ls + gap);
            wide(c->item[i], nm, 48);
            rc.left = cx - gap / 2; rc.right = cx + tile + gap / 2;
            rc.top = cy + ph; rc.bottom = cy + ph + ls;
            fj_dibujar(v->bits, W, H, &rc, nm, hs, FJ_G, FJ_CENTRO | FJ_ELIPSIS);
        }
    }
    /* the panel under it (premios.c's colours and edge) */
    {
        double rad = pad, edge = h / 16.0 < 1.6 ? 1.6 : h / 16.0;
        double hw = W / 2.0, hh = H / 2.0;
        int yy, xx;
        for (yy = 0; yy < H; ++yy)
            for (xx = 0; xx < W; ++xx) {
                unsigned char *p = v->bits + 4 * (yy * W + xx);
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
                r += (255 - r) * t1; g += (226 - g) * t1; b += (140 - b) * t1;
                r += (245 - r) * t2; g += (245 - g) * t2; b += (245 - b) * t2;
                a *= 0.92;
                p[0] = (unsigned char)(b * a + 0.5);
                p[1] = (unsigned char)(g * a + 0.5);
                p[2] = (unsigned char)(r * a + 0.5);
                p[3] = (unsigned char)(255 * a + 0.5);
            }
    }

    /* the portraits, over the panel */
    if (gridh) {
        for (i = 0; i < c->nitems && i < CARTEL_ITEMS; ++i) {
            const unsigned char *f = foto(c->foto[i]);
            int cx = x_grid + (i % cols) * (tile + gap), cy = y + (i / cols) * (ph + ls + gap);
            /* what shows of the portrait centred on its tile, so the name is under it */
            if (f)
                pegar_foto(v->bits, W, H, cx + (int)((0.5f - s_foto_c[c->foto[i]]) * (float)tile + 0.5f),
                           cy, tile, ph, f);
        }
    }
    return 1;
}

int nv2a_d3d11_window_to_guest(int px, int py, int cw, int ch, float *gx, float *gy, float *dev);

/* The badges' window: the game's whole client area, clear but for the
 * badges (a red pill with a gold edge and the word in white), each centred
 * on its game point; repainted only when they or the size change. The game
 * point -> window pixel map is the kit's own (window_to_guest, affine: read
 * at two corners and inverted). */
typedef struct {
    HWND win;
    HDC mem;
    HBITMAP bmp, old;
    unsigned char *bits;
    int bw, bh, shown, n;
    float x[CARTEL_MARCAS], y[CARTEL_MARCAS], ancho, ax, ay, sx, sy;
    char txt[32];
} marcas_v;

static void marcas_pintar(marcas_v *v, HDC screen, int cw, int ch)
{
    static HFONT f;
    static int fh;
    wchar_t wt[32];
    SIZE sz;
    int i, h, W, H, edge;
    if (cw != v->bw || ch != v->bh || !v->bmp) {
        BITMAPINFO bi;
        if (v->bmp) {
            SelectObject(v->mem, v->old);
            DeleteObject(v->bmp);
            v->bmp = NULL;
        }
        memset(&bi, 0, sizeof bi);
        bi.bmiHeader.biSize = sizeof bi.bmiHeader;
        bi.bmiHeader.biWidth = cw;
        bi.bmiHeader.biHeight = -ch;
        bi.bmiHeader.biPlanes = 1;
        bi.bmiHeader.biBitCount = 32;
        bi.bmiHeader.biCompression = BI_RGB;
        v->bmp = CreateDIBSection(screen, &bi, DIB_RGB_COLORS, (void **)&v->bits, NULL, 0);
        if (!v->bmp) {
            v->bw = v->bh = 0;
            return;
        }
        v->old = (HBITMAP)SelectObject(v->mem, v->bmp);
        v->bw = cw;
        v->bh = ch;
    }
    memset(v->bits, 0, (size_t)cw * (size_t)ch * 4u);
    wide(v->txt, wt, 32);
    /* the word 9 frame px high, smaller while it is wider than the badge */
    h = (int)(9.f * v->sy + 0.5f);
    if (h < 8)
        h = 8;
    for (;;) {
        if (h != fh) {
            if (f)
                DeleteObject(f);
            f = CreateFontA(-h, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                            CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH, "Segoe UI");
            fh = h;
        }
        fj_medir(wt, h, &sz);
        if (sz.cx + h / 2 <= (int)(v->ancho * v->sx) || h <= 8)
            break;
        --h;
    }
    W = sz.cx + h * 2 / 3;
    H = sz.cy + h / 6;
    edge = h / 8 > 1 ? h / 8 : 1;
    SetBkMode(v->mem, TRANSPARENT);
    SetTextColor(v->mem, RGB(255, 255, 255));
    for (i = 0; i < v->n; ++i) {
        int cx = (int)((v->x[i] - v->ax) * v->sx), cy = (int)((v->y[i] - v->ay) * v->sy);
        int x0 = cx - W / 2, y0 = cy - H / 2, xx, yy;
        RECT rc;
        if (x0 < 0 || y0 < 0 || x0 + W > cw || y0 + H > ch)
            continue;
        rc.left = x0; rc.top = y0; rc.right = x0 + W; rc.bottom = y0 + H;
        fj_dibujar(v->bits, cw, ch, &rc, wt, h, FJ_BLANCO, FJ_CENTRO | FJ_VCENTRO);
        {
            double rad = H / 2.0, hw = W / 2.0, hh = H / 2.0;
            for (yy = 0; yy < H; ++yy)
                for (xx = 0; xx < W; ++xx) {
                    unsigned char *p = v->bits + 4 * ((size_t)(y0 + yy) * (size_t)cw + (size_t)(x0 + xx));
                    double qx = fabs(xx + 0.5 - hw) - (hw - rad - 0.5);
                    double qy = fabs(yy + 0.5 - hh) - (hh - rad - 0.5);
                    double ox = qx > 0 ? qx : 0, oy = qy > 0 ? qy : 0;
                    double dist = sqrt(ox * ox + oy * oy) + (qx > qy ? (qx < 0 ? qx : 0) : (qy < 0 ? qy : 0)) - rad;
                    double a = 0.5 - dist, gold = dist + edge + 0.5, t = p[1] / 255.0, r, g, b;
                    a = a < 0 ? 0 : a > 1 ? 1 : a;
                    gold = gold < 0 ? 0 : gold > 1 ? 1 : gold;
                    r = 190 + (230 - 190) * gold;
                    g = 20 + (180 - 20) * gold;
                    b = 30 + (60 - 30) * gold;
                    r += (255 - r) * t; g += (255 - g) * t; b += (255 - b) * t;
                    p[0] = (unsigned char)(b * a + 0.5);
                    p[1] = (unsigned char)(g * a + 0.5);
                    p[2] = (unsigned char)(r * a + 0.5);
                    p[3] = (unsigned char)(255 * a + 0.5);
                }
        }
    }
}

static void marcas_tick(marcas_v *v, HDC screen, int base, const RECT *cr, POINT tl, const char *cls)
{
    float ax = 0, ay = 0, bx = 0, by = 0, sx, sy, x[CARTEL_MARCAS], y[CARTEL_MARCAS], ancho;
    int n, changed;
    ULONGLONG hasta;
    char txt[32];
    EnterCriticalSection(&s_cs);
    n = s_mn;
    hasta = s_mhasta;
    memcpy(x, s_mx, sizeof x);
    memcpy(y, s_my, sizeof y);
    ancho = s_mancho;
    memcpy(txt, s_mtxt, sizeof txt);
    LeaveCriticalSection(&s_cs);
    if (!base || GetTickCount64() >= hasta || n <= 0
            || nv2a_d3d11_window_to_guest(0, 0, cr->right, cr->bottom, &ax, &ay, NULL) < 0
            || nv2a_d3d11_window_to_guest(cr->right, cr->bottom, cr->right, cr->bottom, &bx, &by, NULL) < 0
            || bx - ax < 1.f || by - ay < 1.f) {
        if (v->shown && v->win)
            ShowWindow(v->win, SW_HIDE);
        v->shown = 0;
        return;
    }
    sx = (float)cr->right / (bx - ax);
    sy = (float)cr->bottom / (by - ay);
    changed = !v->bmp || cr->right != v->bw || cr->bottom != v->bh || n != v->n || ancho != v->ancho
              || ax != v->ax || ay != v->ay || sx != v->sx || sy != v->sy
              || memcmp(x, v->x, sizeof(float) * (size_t)n) || memcmp(y, v->y, sizeof(float) * (size_t)n)
              || strcmp(txt, v->txt);
    if (changed) {
        v->n = n;
        memcpy(v->x, x, sizeof(float) * (size_t)n);
        memcpy(v->y, y, sizeof(float) * (size_t)n);
        v->ancho = ancho;
        v->ax = ax; v->ay = ay; v->sx = sx; v->sy = sy;
        memcpy(v->txt, txt, sizeof txt);
        marcas_pintar(v, screen, cr->right, cr->bottom);
    }
    if (!v->bmp)
        return;
    if (!v->win)
        v->win = CreateWindowExA(WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW
                                 | WS_EX_TOPMOST, cls, "", WS_POPUP, 0, 0, 1, 1,
                                 NULL, NULL, GetModuleHandleA(NULL), NULL);
    if (v->win) {
        POINT src = { 0, 0 };
        SIZE sz;
        BLENDFUNCTION bf = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
        sz.cx = v->bw;
        sz.cy = v->bh;
        if (changed || !v->shown)
            UpdateLayeredWindow(v->win, screen, &tl, &sz, v->mem, &src, 0, &bf, ULW_ALPHA);
        else
            UpdateLayeredWindow(v->win, NULL, &tl, NULL, NULL, NULL, 0, NULL, 0);
        if (!v->shown)
            SetWindowPos(v->win, HWND_TOPMOST, 0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
        v->shown = 1;
    }
}

static DWORD WINAPI cartel_thread(LPVOID unused)
{
    WNDCLASSA wc;
    HDC screen;
    static ventana v[CARTEL_N];
    static marcas_v mv;
    int s;

    (void)unused;
    memset(&wc, 0, sizeof wc);
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(NULL);
    wc.lpszClassName = "YgoCartel";
    RegisterClassA(&wc);
    screen = GetDC(NULL);
    for (s = 0; s < CARTEL_N; ++s)
        v[s].mem = CreateCompatibleDC(screen);
    mv.mem = CreateCompatibleDC(screen);

    for (;;) {
        MSG m;
        HWND g = FindWindowA("RecompNv2aD3D11", NULL);
        RECT cr;
        POINT tl = { 0, 0 };
        int base = g && GetForegroundWindow() == g && !xbox_MenuIsOpen() && !IsIconic(g)
                   && GetClientRect(g, &cr) && cr.bottom > 100;
        int any = 0;

        while (PeekMessageA(&m, NULL, 0, 0, PM_REMOVE))
            DispatchMessageA(&m);
        if (base)
            ClientToScreen(g, &tl);
        for (s = 0; s < CARTEL_N; ++s) {
            static ygo_cartel c;
            ULONGLONG hasta;
            EnterCriticalSection(&s_cs);
            c = s_c[s];
            hasta = s_hasta[s];
            LeaveCriticalSection(&s_cs);
            if (!base || GetTickCount64() >= hasta) {
                if (v[s].shown && v[s].win)
                    ShowWindow(v[s].win, SW_HIDE);
                v[s].shown = 0;
                continue;
            }
            any = 1;
            if (!v[s].valid || memcmp(&c, &v[s].last, sizeof c) || cr.right != v[s].last_w
                    || cr.bottom != v[s].last_h) {
                v[s].valid = pintar(&v[s], screen, &c, &cr);
                v[s].pintado = 1;
                v[s].last = c;
                v[s].last_w = cr.right;
                v[s].last_h = cr.bottom;
            }
            if (!v[s].valid)
                continue;
            if (!v[s].win)
                v[s].win = CreateWindowExA(WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW
                                           | WS_EX_TOPMOST, wc.lpszClassName, "", WS_POPUP, 0, 0, 1, 1,
                                           NULL, NULL, wc.hInstance, NULL);
            if (v[s].win) {
                POINT dst, src = { 0, 0 };
                SIZE sz;
                BLENDFUNCTION bf = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
                int m40 = cr.bottom / 40;
                dst.x = c.lado == 1 ? tl.x + m40
                      : c.lado == 2 ? tl.x + (cr.right - v[s].bw) / 2
                      : tl.x + cr.right - v[s].bw - m40;
                dst.y = tl.y + (int)((long long)cr.bottom * c.y_mil / 1000);
                sz.cx = v[s].bw;
                sz.cy = v[s].bh;
                /* 2026-11-15: the picture only when it changed (it was sent
                 * every 30 ms while shown), else nothing or just the place */
                if (v[s].pintado || !v[s].shown)
                    UpdateLayeredWindow(v[s].win, screen, &dst, &sz, v[s].mem, &src, 0, &bf, ULW_ALPHA);
                else if (dst.x != v[s].puesto.x || dst.y != v[s].puesto.y)
                    UpdateLayeredWindow(v[s].win, NULL, &dst, NULL, NULL, NULL, 0, NULL, 0);
                v[s].pintado = 0;
                v[s].puesto = dst;
                if (!v[s].shown)
                    SetWindowPos(v[s].win, HWND_TOPMOST, 0, 0, 0, 0,
                                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
                v[s].shown = 1;
            }
        }
        marcas_tick(&mv, screen, base, &cr, tl, wc.lpszClassName);
        any |= mv.shown;
        Sleep(any ? 30 : 50);
    }
}

void ygo_cartel_start(void)
{
    static int started;
    if (started)
        return;
    started = 1;
    InitializeCriticalSection(&s_cs);
    s_cs_ok = 1;
    CloseHandle(CreateThread(NULL, 0, cartel_thread, NULL, 0, NULL));
}
