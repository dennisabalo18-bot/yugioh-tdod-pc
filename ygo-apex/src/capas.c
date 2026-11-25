/*
 * capas.c - the recording mode for the panels (2026-11-23, the user: "quiero
 * que todo salga en obs ... es para grabar para youtube").
 *
 * Perfil, buscador, carteles, premios and the click ripple are layered
 * windows over the game window; OBS's game and window captures record the
 * game's picture only. With the mode on (F2 "Modo grabación (OBS)", the
 * same switch as the glove in raton.c), every UpdateLayeredWindow of those
 * files (capas.h) hands the picture and place to the kit
 * (nv2a_d3d11_layer), which draws it into the frame in the windows'
 * z-order, and the window itself is updated clear (constant alpha 0): one
 * copy on screen, inside the picture. Mode off: the windows get their last
 * picture back.
 */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

void nv2a_d3d11_layer(void *key, const uint32_t *px, unsigned w, unsigned h, int sx, int sy);
void nv2a_d3d11_layers_clear(void);

#define CAPA_MAX 32
typedef struct {
    HWND h;
    uint32_t *px;                 /* as the window had it (premultiplied) */
    unsigned w, hh, cap;
    POINT pos;
    BYTE alpha;
} Capa;

static Capa  s_c[CAPA_MAX];
static SRWLOCK s_lock = SRWLOCK_INIT;
static volatile LONG s_on;
static uint32_t *s_tmp;           /* the picture with the constant alpha in */
static unsigned s_tmp_cap;

static Capa *capa_de(HWND h)
{
    int i, libre = -1;
    for (i = 0; i < CAPA_MAX; i++) {
        if (s_c[i].h == h)
            return &s_c[i];
        if (libre < 0 && (!s_c[i].h || !IsWindow(s_c[i].h)))
            libre = i;
    }
    if (libre < 0)
        return NULL;
    free(s_c[libre].px);
    memset(&s_c[libre], 0, sizeof s_c[libre]);
    s_c[libre].h = h;
    return &s_c[libre];
}

/* The DIB selected in src, the part at sp of size sz, top row first. */
static int copiar(Capa *c, HDC src, const POINT *sp, const SIZE *sz, const BLENDFUNCTION *bf)
{
    DIBSECTION ds;
    HGDIOBJ b = GetCurrentObject(src, OBJ_BITMAP);
    int x0 = sp ? sp->x : 0, y0 = sp ? sp->y : 0, y, abajo;
    unsigned w = (unsigned)sz->cx, h = (unsigned)sz->cy;
    if (!b || GetObject(b, sizeof ds, &ds) != sizeof ds || !ds.dsBm.bmBits
            || ds.dsBm.bmBitsPixel != 32 || !w || !h)
        return 0;
    if (x0 < 0 || y0 < 0 || x0 + (int)w > ds.dsBm.bmWidth || y0 + (int)h > ds.dsBm.bmHeight)
        return 0;
    if (w * h > c->cap) {
        uint32_t *n = (uint32_t *)realloc(c->px, (size_t)w * h * 4u);
        if (!n)
            return 0;
        c->px = n;
        c->cap = w * h;
    }
    /* 2026-11-23 (the user: "todo lo que agregamos está dado vuelta"):
     * GetObject reports biHeight positive even for a top-down DIB, so it
     * cannot tell. Every panel here makes its DIB top-down (biHeight -h:
     * buscador, cartel, onda, perfil, premios). */
    abajo = 0;
    for (y = 0; y < (int)h; y++) {
        int fy = abajo ? ds.dsBm.bmHeight - 1 - (y0 + y) : y0 + y;
        const uint8_t *fila = (const uint8_t *)ds.dsBm.bmBits + (size_t)fy * ds.dsBm.bmWidthBytes;
        memcpy(c->px + (size_t)y * w, fila + (size_t)x0 * 4u, (size_t)w * 4u);
    }
    if (!bf || bf->AlphaFormat != AC_SRC_ALPHA) {
        unsigned i;
        for (i = 0; i < w * h; i++)
            c->px[i] |= 0xFF000000u;
    }
    c->w = w;
    c->hh = h;
    c->alpha = bf ? bf->SourceConstantAlpha : 255;
    return 1;
}

/* To the kit, with the constant alpha multiplied in. */
static void al_kit(Capa *c, int con_imagen)
{
    const uint32_t *px = NULL;
    if (con_imagen && c->px) {
        px = c->px;
        if (c->alpha != 255) {
            unsigned i, n = c->w * c->hh, a = c->alpha;
            if (n > s_tmp_cap) {
                uint32_t *t = (uint32_t *)realloc(s_tmp, (size_t)n * 4u);
                if (!t)
                    return;
                s_tmp = t;
                s_tmp_cap = n;
            }
            for (i = 0; i < n; i++) {
                uint32_t v = c->px[i];
                s_tmp[i] = ((((v >> 24) & 255u) * a / 255u) << 24) | ((((v >> 16) & 255u) * a / 255u) << 16)
                         | ((((v >> 8) & 255u) * a / 255u) << 8) | ((v & 255u) * a / 255u);
            }
            px = s_tmp;
        }
    }
    nv2a_d3d11_layer(c->h, px, c->w, c->hh, c->pos.x, c->pos.y);
}

#undef UpdateLayeredWindow
BOOL ygo_capa_ulw(HWND hwnd, HDC dst, POINT *pos, SIZE *size, HDC src, POINT *src_pos,
                  COLORREF key, BLENDFUNCTION *blend, DWORD flags)
{
    BLENDFUNCTION clara;
    Capa *c;
    int imagen = 0;
    if (!s_on)
        return UpdateLayeredWindow(hwnd, dst, pos, size, src, src_pos, key, blend, flags);
    AcquireSRWLockExclusive(&s_lock);
    c = capa_de(hwnd);
    if (c) {
        if (pos)
            c->pos = *pos;
        else if (!c->px) {
            RECT r;
            if (GetWindowRect(hwnd, &r)) {
                c->pos.x = r.left;
                c->pos.y = r.top;
            }
        }
        if (src && size)
            imagen = copiar(c, src, src_pos, size, blend);
        if (c->px)
            al_kit(c, imagen);
    }
    ReleaseSRWLockExclusive(&s_lock);
    if (blend && c) {
        clara = *blend;
        clara.SourceConstantAlpha = 0;   /* the window clear: the frame has it */
        blend = &clara;
    }
    return UpdateLayeredWindow(hwnd, dst, pos, size, src, src_pos, key, blend, flags);
}

/* Mode off: each window gets its last picture back. */
static void devolver(void)
{
    int i;
    HDC pant = GetDC(NULL), mem = CreateCompatibleDC(pant);
    for (i = 0; i < CAPA_MAX; i++) {
        Capa *c = &s_c[i];
        BITMAPINFO bi;
        void *bits = NULL;
        HBITMAP b;
        if (!c->h || !c->px || !IsWindow(c->h))
            continue;
        memset(&bi, 0, sizeof bi);
        bi.bmiHeader.biSize = sizeof bi.bmiHeader;
        bi.bmiHeader.biWidth = (LONG)c->w;
        bi.bmiHeader.biHeight = -(LONG)c->hh;
        bi.bmiHeader.biPlanes = 1;
        bi.bmiHeader.biBitCount = 32;
        bi.bmiHeader.biCompression = BI_RGB;
        b = CreateDIBSection(mem, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
        if (b && bits) {
            HGDIOBJ viejo = SelectObject(mem, b);
            POINT cero = {0, 0};
            SIZE sz;
            BLENDFUNCTION bf = { AC_SRC_OVER, 0, c->alpha, AC_SRC_ALPHA };
            sz.cx = (LONG)c->w;
            sz.cy = (LONG)c->hh;
            memcpy(bits, c->px, (size_t)c->w * c->hh * 4u);
            UpdateLayeredWindow(c->h, pant, &c->pos, &sz, mem, &cero, 0, &bf, ULW_ALPHA);
            SelectObject(mem, viejo);
        }
        if (b)
            DeleteObject(b);
    }
    DeleteDC(mem);
    ReleaseDC(NULL, pant);
}

void ygo_capas_set(int on)
{
    on = on ? 1 : 0;
    AcquireSRWLockExclusive(&s_lock);
    if (InterlockedExchange(&s_on, on) != on && !on) {
        devolver();
        nv2a_d3d11_layers_clear();
    }
    ReleaseSRWLockExclusive(&s_lock);
}

int ygo_capas_on(void) { return s_on != 0; }
