/*
 * tapete_fx.c - the tapete's light in the duel (2026-11-20, the user, after the
 * samples modding/muestras/tapete_baraja_propuestas.mp4: "quiero las 3 y que la
 * gente elija como con los marcos de las cartas"; of C "los brillos pero no que
 * se mueva"). F2 / Configuración "Efecto del tapete", and under the tapete in
 * the Perfil: No / Luz (a band of light crossing now and then, the frames
 * shining as it passes) / Pulso (light running along the frames in the tapete's
 * colour, breathing) / Brillo (motes of light rising, the art still).
 *
 * The tapete is a texture of the board (fundas.c TABLERO; the kit draws your
 * half with ours, nv2a_pb_d3d11.c s_tap_srv), so the light is under the cards.
 * fundas.c hands each tapete it reads to ygo_tapete_fx_base(); while a duel is
 * played and an effect is on, a thread of ours makes a frame of it ~30 times a
 * second into one of two buffers and hands it to the kit (nv2a_tapete_cuadro:
 * no copy, the texture updated in place). The same effect is drawn by perfil.c
 * on its box's picture (ygo_tapete_fx_pintar) so the choice is seen there.
 */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include "recomp/gen/recomp_types.h"

void nv2a_tapete_cuadro(const uint32_t *texels, uint32_t w, uint32_t h);

static volatile LONG s_fx;              /* 0 No, 1 Luz, 2 Pulso, 3 Brillo */
int ygo_tapete_fx_get(void) { return (int)s_fx; }
void ygo_tapete_fx_set(int i) { InterlockedExchange(&s_fx, i >= 0 && i <= 3 ? i : 0); }

/* ---- an effect on any BGRA picture (straight or opaque): its masks once --------- */
typedef struct {
    int w, h;
    unsigned char *linea;               /* the gold frames */
    unsigned char *halo;                /* them blurred */
    float col[3];                       /* the tapete's own colour (its saturated px), rgb 0..1 */
} fx_mascaras;

static void mascaras_hacer(fx_mascaras *m, const unsigned char *px, int w, int h)
{
    int x, y, r = w > 1000 ? 6 : 3, i;
    double acc[3] = { 0, 0, 0 }, n = 0;
    unsigned char *tmp;
    m->w = w; m->h = h;
    m->linea = (unsigned char *)calloc((size_t)w * h, 1);
    m->halo = (unsigned char *)calloc((size_t)w * h, 1);
    tmp = (unsigned char *)calloc((size_t)w * h, 2);
    if (!m->linea || !m->halo || !tmp) {
        free(tmp);
        return;
    }
    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++) {
            const unsigned char *p = px + 4 * ((size_t)y * w + x);
            int b = p[0], g = p[1], rr = p[2], a = p[3], mx, mn;
            if (a < 40)
                continue;
            if (rr > 158 && g > 90 && g < 204 && b < 97 && rr - b > 90)
                m->linea[(size_t)y * w + x] = 255;
            mx = rr > g ? (rr > b ? rr : b) : (g > b ? g : b);
            mn = rr < g ? (rr < b ? rr : b) : (g < b ? g : b);
            if (mx > 90 && mx - mn > 70 && !(rr > 158 && g > 90 && g < 204 && b < 97)) {   /* the art's colour, not the frames' */
                acc[0] += rr; acc[1] += g; acc[2] += b; n++;
            }
        }
    if (n > 0) {
        double mx = acc[0] > acc[1] ? (acc[0] > acc[2] ? acc[0] : acc[2]) : (acc[1] > acc[2] ? acc[1] : acc[2]);
        for (i = 0; i < 3; i++)
            m->col[i] = (float)(acc[i] / mx);
    } else {
        m->col[0] = 1.f; m->col[1] = 0.7f; m->col[2] = 0.35f;
    }
    /* the halo: a box blur of the lines, two passes */
    {
        unsigned short *t2 = (unsigned short *)tmp;
        for (y = 0; y < h; y++) {
            int s = 0;
            for (x = -r; x < w + r; x++) {
                if (x + r < w) s += m->linea[(size_t)y * w + x + r];
                if (x - r - 1 >= 0) s -= m->linea[(size_t)y * w + x - r - 1];
                if (x >= 0 && x < w) t2[(size_t)y * w + x] = (unsigned short)(s / (2 * r + 1));
            }
        }
        for (x = 0; x < w; x++) {
            int s = 0;
            for (y = -r; y < h + r; y++) {
                if (y + r < h) s += t2[(size_t)(y + r) * w + x];
                if (y - r - 1 >= 0) s -= t2[(size_t)(y - r - 1) * w + x];
                if (y >= 0 && y < h) {
                    int v = s / (2 * r + 1) * 2;
                    m->halo[(size_t)y * w + x] = (unsigned char)(v > 255 ? 255 : v);
                }
            }
        }
    }
    free(tmp);
}

static void mascaras_soltar(fx_mascaras *m)
{
    free(m->linea);
    free(m->halo);
    memset(m, 0, sizeof *m);
}

static float s_pulso_t[256];

static unsigned char sumar8(unsigned char a, float v)
{
    int r = (int)(a + v);
    return (unsigned char)(r > 255 ? 255 : r < 0 ? 0 : r);
}

/* frame t (s) of effect fx of base into out (both w x h BGRA); flip: the picture's
 * rows are upside down (the board texture: up the screen is down the rows) */
static void componer(const fx_mascaras *m, const unsigned char *base, unsigned char *out, int fx, float t, int flip)
{
    int w = m->w, h = m->h, x, y, i;
    float k = (float)w / 2048.f;
    memcpy(out, base, (size_t)w * h * 4);
    if (!m->linea)
        return;
    if (fx == 1) {                      /* Luz: a band crossing every 3 s, 0.9 s long (2026-11-20: "muy lentas") */
        float c = fmodf(t, 3.0f), pos;
        if (c > 0.9f)
            return;
        pos = c / 0.9f * 2.0f - 0.4f;
        for (y = 0; y < h; y++) {
            float vy = (float)(flip ? h - 1 - y : y) / h * 0.5f;
            for (x = 0; x < w; x++) {
                float u = (float)x / w * 0.8f + vy, d = (u - pos) / 0.07f, b;
                unsigned char *o;
                if (d > 3.f || d < -3.f)
                    continue;
                o = out + 4 * ((size_t)y * w + x);
                if (o[3] < 8)
                    continue;
                b = expf(-d * d) * (0.3f + 0.75f * m->linea[(size_t)y * w + x] / 255.f) * 255.f * (o[3] / 255.f);
                o[0] = sumar8(o[0], b * 0.85f); o[1] = sumar8(o[1], b * 0.95f); o[2] = sumar8(o[2], b);
            }
        }
    } else if (fx == 2) {               /* Pulso: light along the frames, breathing */
        float resp = 0.22f + 0.12f * sinf(t * 2.4f);
        for (i = 0; i < 256; i++) {
            float f = ((float)i / 256.f - 0.5f) / 0.09f;
            s_pulso_t[i] = expf(-f * f);
        }
        for (y = 0; y < h; y++) {
            float vy = (float)(flip ? h - 1 - y : y) / h * 0.5f;
            for (x = 0; x < w; x++) {
                size_t j = (size_t)y * w + x;
                float ln = m->linea[j] / 255.f, hl = m->halo[j] / 255.f, fase, p, v;
                unsigned char *o;
                if (ln <= 0 && hl <= 0)
                    continue;
                o = out + 4 * j;
                fase = (float)x / w * 1.6f + vy - t * 1.2f;
                fase -= floorf(fase);
                p = s_pulso_t[(int)(fase * 255.f)];
                v = (ln * p * 1.4f + hl * (p * 0.8f + resp)) * 150.f * (o[3] / 255.f);
                o[0] = sumar8(o[0], v * m->col[2]); o[1] = sumar8(o[1], v * m->col[1]); o[2] = sumar8(o[2], v * m->col[0]);
            }
        }
    } else if (fx == 3) {               /* Brillo: motes rising (seeded: the same everywhere) */
        uint32_t s = 7;
        for (i = 0; i < 90; i++) {
            float bx, by, vel, rad, fz, px, py, br;
            int x0, x1, y0, y1;
            s = s * 1664525u + 1013904223u; bx = (s >> 8) / 16777216.f;
            s = s * 1664525u + 1013904223u; by = (s >> 8) / 16777216.f;
            s = s * 1664525u + 1013904223u; vel = 0.05f + 0.09f * (s >> 8) / 16777216.f;
            s = s * 1664525u + 1013904223u; rad = (2.5f + 4.5f * (s >> 8) / 16777216.f) * k * 2.4f;
            s = s * 1664525u + 1013904223u; fz = 6.28f * (s >> 8) / 16777216.f;
            py = by + t * vel;
            py -= floorf(py);
            px = bx + sinf(t * 0.8f + fz) * 0.006f;
            br = (0.55f + 0.45f * sinf(t * 3.f + fz)) * 255.f;
            px *= w;
            py = (flip ? py : 1.f - py) * h;      /* up the screen */
            x0 = (int)(px - rad * 3); x1 = (int)(px + rad * 3) + 1; y0 = (int)(py - rad * 3); y1 = (int)(py + rad * 3) + 1;
            if (x0 < 0) x0 = 0;
            if (y0 < 0) y0 = 0;
            if (x1 > w) x1 = w;
            if (y1 > h) y1 = h;
            for (y = y0; y < y1; y++)
                for (x = x0; x < x1; x++) {
                    float dx = x - px, dy = y - py, g = expf(-(dx * dx + dy * dy) / (rad * rad)) * br;
                    unsigned char *o = out + 4 * ((size_t)y * w + x);
                    if (g < 1.f || o[3] < 8)
                        continue;
                    g *= o[3] / 255.f;
                    o[0] = sumar8(o[0], g * (0.4f + 0.6f * m->col[2]));
                    o[1] = sumar8(o[1], g * (0.4f + 0.6f * m->col[1]));
                    o[2] = sumar8(o[2], g * (0.4f + 0.6f * m->col[0]));
                }
        }
    }
}

/* ---- the duel ------------------------------------------------------------------- */
static CRITICAL_SECTION s_cs;
static unsigned char *s_base;           /* the tapete read (straight BGRA), ours */
static int s_bw, s_bh, s_base_ver, s_base_hecha = -1;
static fx_mascaras s_m;
static unsigned char *s_cuadro[2];
static volatile LONG s_hilo;

static int en_duelo(void)
{
    return MEM32(0x00656F88u) != 0u && MEM32(0x00657148u) == 6u;
}

static DWORD WINAPI fx_hilo(LPVOID unused)
{
    int cual = 0, puesto = 0;           /* puesto: a frame of ours is in the kit */
    ULONGLONG t0 = GetTickCount64();
    (void)unused;
    for (;;) {
        int fx = (int)s_fx, ok;
        EnterCriticalSection(&s_cs);
        ok = s_base && fx && en_duelo();
        if (s_base && s_base_hecha != s_base_ver) {        /* a new tapete: its masks */
            mascaras_soltar(&s_m);
            mascaras_hacer(&s_m, s_base, s_bw, s_bh);
            free(s_cuadro[0]); free(s_cuadro[1]);
            s_cuadro[0] = (unsigned char *)malloc((size_t)s_bw * s_bh * 4);
            s_cuadro[1] = (unsigned char *)malloc((size_t)s_bw * s_bh * 4);
            s_base_hecha = s_base_ver;
            fprintf(stderr, "[TAPETE-FX] máscaras %dx%d, color %.2f %.2f %.2f\n", s_bw, s_bh, s_m.col[0], s_m.col[1], s_m.col[2]);
            fflush(stderr);
        }
        if (ok && s_cuadro[0] && s_cuadro[1]) {
            componer(&s_m, s_base, s_cuadro[cual], fx, (float)(GetTickCount64() - t0) / 1000.f, 1);
            nv2a_tapete_cuadro((const uint32_t *)s_cuadro[cual], (uint32_t)s_bw, (uint32_t)s_bh);
            cual ^= 1;
            puesto = 1;
        } else if (puesto && s_base) {  /* off (or out of the duel): the tapete as it is */
            memcpy(s_cuadro[cual], s_base, (size_t)s_bw * s_bh * 4);
            nv2a_tapete_cuadro((const uint32_t *)s_cuadro[cual], (uint32_t)s_bw, (uint32_t)s_bh);
            cual ^= 1;
            puesto = 0;
        }
        LeaveCriticalSection(&s_cs);
        Sleep(ok ? 6 : 100);            /* ~100 frames a second (the user plays at 120: 30 looked slow) */
    }
}

/* fundas.c: the tapete it read (NULL: the classic one, nothing of ours) */
void ygo_tapete_fx_base(const uint32_t *px, uint32_t w, uint32_t h)
{
    static int iniciado;
    if (!iniciado) {
        iniciado = 1;
        InitializeCriticalSection(&s_cs);
    }
    EnterCriticalSection(&s_cs);
    free(s_base);
    s_base = NULL;
    if (px && w && h && (s_base = (unsigned char *)malloc((size_t)w * h * 4)) != NULL) {
        memcpy(s_base, px, (size_t)w * h * 4);
        s_bw = (int)w; s_bh = (int)h;
    }
    s_base_ver++;
    LeaveCriticalSection(&s_cs);
    if (!InterlockedExchange(&s_hilo, 1)) {
        HANDLE th = CreateThread(NULL, 0, fx_hilo, NULL, 0, NULL);
        if (th)
            CloseHandle(th);
    }
}

/* ---- the Perfil: the effect on its box's picture -------------------------------- */
static fx_mascaras s_pm;
static const unsigned char *s_pm_de;

/* base: the box's picture (opaque BGRA, w x h, the right way up); out: the same size */
void ygo_tapete_fx_pintar(const unsigned char *base, int w, int h, unsigned char *out, float t)
{
    if (s_pm_de != base || s_pm.w != w || s_pm.h != h) {
        mascaras_soltar(&s_pm);
        mascaras_hacer(&s_pm, base, w, h);
        s_pm_de = base;
    }
    componer(&s_pm, base, out, (int)s_fx, t, 0);
}
