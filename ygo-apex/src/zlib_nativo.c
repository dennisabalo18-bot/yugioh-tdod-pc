/* zlib_nativo.c — 2026-11-21 (the user: "los fps son muy cambiantes ...
 * sobre todo cuando se cierra la puerta"; chose to bring miniz in).
 *
 * The 02:31 log, every thread sampled: in each load the game's main thread
 * waits 55-82 % on the game's lock 0x00631A64, held by its worker thread
 * while that one inflates PNGs with the game's own zlib 1.1.3, recompiled
 * (inflate sub_00297940 -> inflate_blocks 0x2998B0 -> inflate_codes
 * 0x29C150 -> inflate_fast 0x29D120, plus crc32 0x297650 for every chunk).
 * The decompression sits on the load's critical path.
 *
 * Here inflate() and crc32() run natively with miniz 3.0.2 (third_party,
 * MIT): the guest z_stream (zlib 1.1.3, 32-bit) is read and written in
 * place; each stream has a native mz_stream, found by the guest address and
 * started again when the guest's total_in and total_out are both 0
 * (inflateInit/inflateReset). The window (zlib header or raw) is read from
 * the guest state: nowrap at +12. Every other zlib call (inflateInit,
 * inflateEnd, ...) stays the game's.
 *
 * crc32: the first 16 calls run both and compare; any difference goes back
 * to the game's for the rest of the run, logged.
 * Ajustes\zlib_original.txt: both stay the game's (a way back without a
 * build). Logs: "[ZLIB] ...". */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include "recomp/gen/recomp_types.h"
#define MINIZ_NO_ZLIB_COMPATIBLE_NAMES
#define MINIZ_NO_ARCHIVE_APIS
#define MINIZ_NO_STDIO
#define MINIZ_NO_TIME
#define MINIZ_NO_DEFLATE_APIS
#include "../../third_party/miniz-3.0.2/miniz.h"
#include "zlib_inflar.h"            /* zlib 1.1.3's behaviour on mz_inflate (checked offline on all 334 PNGs) */

void sub_00297940_pad(void);            /* the game's inflate */
void sub_00297650_pad(void);            /* the game's crc32 */
void diag_carga_zlib(int64_t ticks);    /* diag_carga.c: the time, for its lines */

/* the guest z_stream, zlib 1.1.3 */
#define ZS_NEXT_IN   0
#define ZS_AVAIL_IN  4
#define ZS_TOTAL_IN  8
#define ZS_NEXT_OUT  12
#define ZS_AVAIL_OUT 16
#define ZS_TOTAL_OUT 20
#define ZS_STATE     28
#define ZS_ADLER     48

#define FLUJOS 32
static struct { uint32_t g; uint64_t uso; int vivo; unsigned pend; mz_stream s; } s_fl[FLUJOS];
static SRWLOCK s_cand = SRWLOCK_INIT;
static uint64_t s_reloj;
static int s_modo = -1;                 /* 1 native, 0 the game's (zlib_original.txt) */
static volatile LONG s_crc_ok = 1, s_crc_prueba = 16;
static LONG s_flujos, s_errores;

static int modo(void)
{
    if (s_modo < 0) {
        char p[MAX_PATH];
        const char *dir = getenv("YGO_AJUSTES");
        snprintf(p, sizeof p, "%s%szlib_original.txt", dir && *dir ? dir : "", dir && *dir ? "\\" : "");
        s_modo = GetFileAttributesA(p) == INVALID_FILE_ATTRIBUTES;
        fprintf(stderr, "[ZLIB] %s\n", s_modo ? "inflate y crc32 nativos (miniz 3.0.2)"
                                               : "zlib del juego (zlib_original.txt)");
    }
    return s_modo;
}

static int64_t ahora(void)
{
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return t.QuadPart;
}

/* the native stream of guest stream g; NULL when it is not known and not
 * at its start */
static mz_stream *flujo(uint32_t g, unsigned **pend)
{
    int i, libre = -1, viejo = 0;
    int nuevo = MEM32(g + ZS_TOTAL_IN) == 0 && MEM32(g + ZS_TOTAL_OUT) == 0;
    for (i = 0; i < FLUJOS; i++) {
        if (s_fl[i].vivo && s_fl[i].g == g)
            break;
        if (!s_fl[i].vivo && libre < 0)
            libre = i;
        if (s_fl[i].uso < s_fl[viejo].uso)
            viejo = i;
    }
    if (i == FLUJOS) {
        if (!nuevo)
            return NULL;
        i = libre >= 0 ? libre : viejo;
        if (s_fl[i].vivo)
            mz_inflateEnd(&s_fl[i].s);
        s_fl[i].vivo = 0;
    }
    if (nuevo || !s_fl[i].vivo) {
        uint32_t st = MEM32(g + ZS_STATE);
        int nowrap = st ? (int)MEM32(st + 12) : 0;
        if (s_fl[i].vivo)
            mz_inflateEnd(&s_fl[i].s);
        memset(&s_fl[i].s, 0, sizeof s_fl[i].s);
        if (mz_inflateInit2(&s_fl[i].s, nowrap ? -MZ_DEFAULT_WINDOW_BITS : MZ_DEFAULT_WINDOW_BITS) != MZ_OK)
            return NULL;
        s_fl[i].g = g;
        s_fl[i].vivo = 1;
        s_fl[i].pend = 0;
        if (InterlockedIncrement(&s_flujos) <= 3)
            fprintf(stderr, "[ZLIB] flujo %08X %s\n", g, nowrap ? "crudo" : "con cabecera zlib");
    }
    s_fl[i].uso = ++s_reloj;
    *pend = &s_fl[i].pend;
    return &s_fl[i].s;
}

/* int inflate(z_streamp strm, int flush), cdecl */
void sub_00297940(void)
{
    uint32_t g = MEM32(g_esp + 4), flush = MEM32(g_esp + 8);
    uint32_t in, out, ain, aout;
    const unsigned char *hin;
    unsigned char *hout;
    mz_stream *s;
    unsigned *pend = NULL;
    int64_t t0;
    int r;
    if (!modo() || !g) {
        sub_00297940_pad();
        return;
    }
    t0 = ahora();
    AcquireSRWLockExclusive(&s_cand);
    s = flujo(g, &pend);
    if (!s) {
        ReleaseSRWLockExclusive(&s_cand);
        if (InterlockedIncrement(&s_errores) <= 8)
            fprintf(stderr, "[ZLIB] flujo %08X desconocido a mitad (total_in %u): error\n",
                    g, MEM32(g + ZS_TOTAL_IN));
        g_eax = (uint32_t)MZ_DATA_ERROR;
        g_esp += 4;
        return;
    }
    in = MEM32(g + ZS_NEXT_IN);
    ain = MEM32(g + ZS_AVAIL_IN);
    out = MEM32(g + ZS_NEXT_OUT);
    aout = MEM32(g + ZS_AVAIL_OUT);
    hin = (const unsigned char *)XBOX_PTR(in);
    hout = (unsigned char *)XBOX_PTR(out);
    (void)flush;                        /* zlib 1.1.3 decodes the same whatever the flush */
    {
        const unsigned char *ni = hin;
        unsigned char *no = hout;
        unsigned ai = ain, ao = aout;
        uint32_t usado, hecho;
        r = zinflar(s, pend, &ni, &ai, &no, &ao);
        usado = (uint32_t)(ni - hin);
        hecho = (uint32_t)(no - hout);
        MEM32(g + ZS_NEXT_IN) = in + usado;
        MEM32(g + ZS_AVAIL_IN) = ain - usado;
        MEM32(g + ZS_TOTAL_IN) = MEM32(g + ZS_TOTAL_IN) + usado;
        MEM32(g + ZS_NEXT_OUT) = out + hecho;
        MEM32(g + ZS_AVAIL_OUT) = aout - hecho;
        MEM32(g + ZS_TOTAL_OUT) = MEM32(g + ZS_TOTAL_OUT) + hecho;
        MEM32(g + ZS_ADLER) = (uint32_t)s->adler;
    }
    ReleaseSRWLockExclusive(&s_cand);
    if (r < 0 && r != MZ_BUF_ERROR && InterlockedIncrement(&s_errores) <= 8)
        fprintf(stderr, "[ZLIB] flujo %08X: error %d (total_in %u, total_out %u)\n",
                g, r, MEM32(g + ZS_TOTAL_IN), MEM32(g + ZS_TOTAL_OUT));
    g_eax = (uint32_t)r;                /* MZ_* are zlib's own values */
    g_esp += 4;                         /* ret (cdecl) */
    diag_carga_zlib(ahora() - t0);
}

/* uLong crc32(uLong crc, const Bytef *buf, uInt len), cdecl */
void sub_00297650(void)
{
    uint32_t crc = MEM32(g_esp + 4), buf = MEM32(g_esp + 8), len = MEM32(g_esp + 12), mio;
    if (!modo() || !s_crc_ok) {
        sub_00297650_pad();
        return;
    }
    mio = buf ? (uint32_t)mz_crc32(crc, (const unsigned char *)XBOX_PTR(buf), len) : 0;
    if (s_crc_prueba > 0 && InterlockedDecrement(&s_crc_prueba) >= 0) {
        sub_00297650_pad();             /* the game's, to compare (it returns) */
        if (g_eax != mio) {
            s_crc_ok = 0;
            fprintf(stderr, "[ZLIB] crc32 distinto (juego %08X, miniz %08X, largo %u): queda el del juego\n",
                    g_eax, mio, len);
        } else if (s_crc_prueba == 0)
            fprintf(stderr, "[ZLIB] crc32 nativo: 16 iguales al del juego\n");
        return;                         /* the game's result, its own ret */
    }
    g_eax = mio;
    g_esp += 4;
}
