/* png_nativo.c — 2026-11-21 (the user: "segui con lo de los png"; the 05:05
 * log: a PNG of the loads takes 8-15 ms, 14.6 of 15.4 inside png_read_row
 * with zlib, already native, 3-4 of it). libpng 1.0.8's
 * png_read_filter_row (sub_00294C80: the row's PNG filter undone - Sub, Up,
 * Average, Paeth - byte by byte in recompiled code) native here, as the
 * PNG specification and libpng 1.0.8 define it:
 *   png_read_filter_row(png_ptr, row_info, row, prev_row, filter), cdecl;
 *   row_info: +4 rowbytes, +0xB pixel_depth; bpp = (pixel_depth + 7) >> 3.
 * The first 64 rows of each filter go through both: the game's on the real
 * row, this one on a copy, every byte compared. All equal: native from then
 * on ("[PNG] filtro N: 64 filas iguales, ahora nativo"); one difference: the
 * game's for that filter for good, said. An unknown filter (libpng warns and
 * clears the row) stays the game's. Ajustes\png_original.txt: the game's
 * for all.
 * Also timed for [CARGA] (diag_carga.c): the filter and the row's colour
 * transformations (png_do_read_transformations, sub_0028C280), to see what
 * of a PNG's rows is left. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include "recomp/gen/recomp_types.h"

void sub_00294C80_pad(void);            /* the game's png_read_filter_row */
void sub_0028C280_pad(void);            /* the game's png_do_read_transformations */
void diag_carga_png(int que, int64_t ticks);   /* diag_carga.c: 0 filter, 1 transformations */

#define PRUEBA 64

static int s_modo = -1;                 /* 1 native, 0 the game's (png_original.txt) */
static volatile LONG s_prueba[5] = {0, PRUEBA, PRUEBA, PRUEBA, PRUEBA};   /* rows left to compare */
static volatile LONG s_malo[5];         /* a difference seen: the game's */
static SRWLOCK s_cand = SRWLOCK_INIT;   /* the comparison's scratch row */
static uint8_t *s_copia;
static size_t s_copia_n;

static int64_t ahora(void)
{
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return t.QuadPart;
}

static int modo(void)
{
    if (s_modo < 0) {
        char p[MAX_PATH];
        const char *dir = getenv("YGO_AJUSTES");
        snprintf(p, sizeof p, "%s%spng_original.txt", dir && *dir ? dir : "", dir && *dir ? "\\" : "");
        s_modo = GetFileAttributesA(p) == INVALID_FILE_ATTRIBUTES;
        fprintf(stderr, "[PNG] %s\n", s_modo ? "filtro de filas nativo (se compara al principio)"
                                             : "filtro de filas del juego (png_original.txt)");
    }
    return s_modo;
}

static void filtrar(uint8_t *row, const uint8_t *prev, uint32_t n, uint32_t bpp, uint32_t filtro)
{
    uint32_t i;
    switch (filtro) {
    case 1:                             /* Sub */
        for (i = bpp; i < n; i++)
            row[i] = (uint8_t)(row[i] + row[i - bpp]);
        break;
    case 2:                             /* Up */
        for (i = 0; i < n; i++)
            row[i] = (uint8_t)(row[i] + prev[i]);
        break;
    case 3:                             /* Average */
        for (i = 0; i < bpp && i < n; i++)
            row[i] = (uint8_t)(row[i] + (prev[i] >> 1));
        for (; i < n; i++)
            row[i] = (uint8_t)(row[i] + (((unsigned)row[i - bpp] + prev[i]) >> 1));
        break;
    case 4:                             /* Paeth */
        for (i = 0; i < bpp && i < n; i++)
            row[i] = (uint8_t)(row[i] + prev[i]);
        for (; i < n; i++) {
            int a = row[i - bpp], b = prev[i], c = prev[i - bpp];
            int p = b - c, pc = a - c;
            int pa = p < 0 ? -p : p, pb = pc < 0 ? -pc : pc;
            int px;
            pc = p + pc;
            pc = pc < 0 ? -pc : pc;
            px = (pa <= pb && pa <= pc) ? a : (pb <= pc) ? b : c;
            row[i] = (uint8_t)(row[i] + px);
        }
        break;
    }
}

void sub_00294C80(void)
{
    uint32_t info = MEM32(g_esp + 8), row = MEM32(g_esp + 12), prev = MEM32(g_esp + 16);
    uint32_t filtro = MEM32(g_esp + 20), n, bpp;
    int64_t t0 = ahora();
    if (!modo() || filtro < 1 || filtro > 4 || s_malo[filtro] || !info || !row || !prev) {
        sub_00294C80_pad();
        diag_carga_png(0, ahora() - t0);
        return;
    }
    n = MEM32(info + 4);
    bpp = ((uint32_t)MEM8(info + 0xB) + 7u) >> 3;
    if (s_prueba[filtro] > 0 && InterlockedDecrement(&s_prueba[filtro]) >= 0) {
        int igual;
        AcquireSRWLockExclusive(&s_cand);
        if (s_copia_n < n) {
            uint8_t *c = (uint8_t *)realloc(s_copia, n);
            if (c) {
                s_copia = c;
                s_copia_n = n;
            }
        }
        if (s_copia_n < n) {
            ReleaseSRWLockExclusive(&s_cand);
            sub_00294C80_pad();
            return;
        }
        memcpy(s_copia, (const uint8_t *)XBOX_PTR(row), n);
        filtrar(s_copia, (const uint8_t *)XBOX_PTR(prev), n, bpp, filtro);
        sub_00294C80_pad();             /* the game's on the real row (pops its ret) */
        igual = !memcmp(s_copia, (const uint8_t *)XBOX_PTR(row), n);
        ReleaseSRWLockExclusive(&s_cand);
        if (!igual) {
            if (!InterlockedExchange(&s_malo[filtro], 1))
                fprintf(stderr, "[PNG] filtro %u: fila distinta (%u bytes, bpp %u): queda el del juego\n",
                        filtro, n, bpp);
        } else if (s_prueba[filtro] == 0) {
            fprintf(stderr, "[PNG] filtro %u: %d filas iguales, ahora nativo\n", filtro, PRUEBA);
        }
        diag_carga_png(0, ahora() - t0);
        return;
    }
    filtrar((uint8_t *)XBOX_PTR(row), (const uint8_t *)XBOX_PTR(prev), n, bpp, filtro);
    g_esp += 4;                         /* ret (cdecl) */
    diag_carga_png(0, ahora() - t0);
}

/* the row's transformations: only timed */
void sub_0028C280(void)
{
    int64_t t0 = ahora();
    sub_0028C280_pad();
    diag_carga_png(1, ahora() - t0);
}
