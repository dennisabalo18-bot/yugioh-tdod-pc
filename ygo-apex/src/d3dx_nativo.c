/* d3dx_nativo.c — 2026-11-21 (the user: "se tranca al cambiar de turno y
 * recibir una nueva carta ... segui con lo que falta y dame resultados").
 *
 * The 03:01 and 03:25 logs (diag_carga.c, real call chains): every slow
 * duel frame (25-73 ms) spends 45-60 % in four functions of the XDK's D3DX
 * (analysis/disasm/asm/D3DX.asm), its software pixel converters, called by
 * the game through sub_003EEB15 <- sub_003F0C1B <- sub_003EC418 <-
 * sub_0017E910 when it makes a texture at run time (the drawn card's):
 *   sub_003F3218  float RGBA row -> X8R8G8B8, 4x4 dither, x87 RC=truncate
 *   sub_003F3B9D  float RGBA row -> L8 (luminance), dither, truncate
 *   sub_003F43B6  A8R8G8B8 row -> float RGBA (x 1/255)
 *   sub_003F4B5C  A8L8 row -> float RGBA
 * All thiscall (ecx = the converter), args x, y, the float row; ret 0xC.
 * Recompiled, each x87 op goes through the emulated stack; here the same
 * operations natively, in the same order and with the same rounding: the
 * guest's precision control (RECOMP_FP_PC), the float stores, and FIST
 * under RC=truncate (what sub_003F2D7F sets), recomp_fist itself.
 *
 * Proof in the game: the first 64 calls of each run the game's function,
 * then this one into a scratch row, and compare every byte; one difference
 * and that function stays the game's for the run (logged). After 64 equal,
 * only this one. Logs: "[D3DX] ...". The decoders' colour-key branch
 * ([this+0x10] set: sub_003F300D after the row) stays the game's. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include "recomp/gen/recomp_types.h"

void sub_003F3218_pad(void);
void sub_003F3B9D_pad(void);
void sub_003F43B6_pad(void);
void sub_003F4B5C_pad(void);

enum { F_RGB, F_L8, F_ARGB, F_AL, NF };
static const char *k_nom[NF] = { "sub_003F3218 (a X8R8G8B8)", "sub_003F3B9D (a L8)",
                                 "sub_003F43B6 (de A8R8G8B8)", "sub_003F4B5C (de A8L8)" };
static volatile LONG s_prueba[NF] = { 64, 64, 64, 64 };
static volatile LONG s_mal[NF];
static int s_dicho[NF];

/* the guest's precision control on a result (RECOMP_FP_PC) */
static double pc(double x)
{
    return (g_fp_control_word & 0x300u) ? x : recomp_fp_round24(x);
}

static uint32_t destino(uint32_t self, uint32_t x, uint32_t y)
{
    return MEM32(self + 0x18) + x * MEM32(self + 0x1048) + y * MEM32(self + 0x104C);
}

/* the encoders, into out (host); returns the bytes */
static uint32_t a_rgb(uint32_t self, uint32_t x, uint32_t y, uint32_t src, uint8_t *out)
{
    uint32_t n = MEM32(self + 0x1050), i;
    uint32_t tab = MEM32(self + 0x2C) + (((y & 3u) + (x & 3u) * 8u) * 4u);
    uint16_t cwt = (uint16_t)(g_fp_control_word | 0xC00u);
    double k = (double)MEMF(0x455934u);
    for (i = 0; i < n; i++) {
        uint32_t p = src + i * 16u, px;
        double d = (double)MEMF(tab + (i & 3u) * 4u);
        int32_t a, b, c;
        a = (int32_t)recomp_fist((double)(float)pc(pc((double)MEMF(p) * k) + d), cwt, 32);
        b = (int32_t)recomp_fist((double)(float)pc(pc((double)MEMF(p + 4) * k) + d), cwt, 32);
        c = (int32_t)recomp_fist((double)(float)pc(pc((double)MEMF(p + 8) * k) + d), cwt, 32);
        px = (((uint32_t)a << 8 | (uint32_t)b) << 8) | (uint32_t)c;
        memcpy(out + i * 4u, &px, 4);
    }
    return n * 4u;
}

static uint32_t a_l8(uint32_t self, uint32_t x, uint32_t y, uint32_t src, uint8_t *out)
{
    uint32_t n = MEM32(self + 0x1050), i;
    uint32_t tab = MEM32(self + 0x2C) + (((y & 3u) + (x & 3u) * 8u) * 4u);
    uint16_t cwt = (uint16_t)(g_fp_control_word | 0xC00u);
    double k = (double)MEMF(0x455934u), kr = (double)MEMF(0x5F415Cu),
           kg = (double)MEMF(0x5F4154u), kb = (double)MEMF(0x5F4158u);
    for (i = 0; i < n; i++) {
        uint32_t p = src + i * 16u;
        double s = pc(pc((double)MEMF(p) * kr) + pc((double)MEMF(p + 8) * kb));
        s = pc(s + pc((double)MEMF(p + 4) * kg));
        s = pc(s * k);
        s = pc(s + (double)MEMF(tab + (i & 3u) * 4u));
        out[i] = (uint8_t)recomp_fist((double)(float)s, cwt, 32);
    }
    return n;
}

/* the decoders, into out (host floats); returns the bytes */
static uint32_t de_argb(uint32_t self, uint32_t x, uint32_t y, uint8_t *out)
{
    uint32_t n = MEM32(self + 0x1050), src = destino(self, x, y), i;
    double s = (double)MEMF(0x423A44u);
    for (i = 0; i < n; i++) {
        uint32_t q = src + i * 4u;
        float f[4];
        f[0] = (float)pc((double)MEM8(q + 2) * s);
        f[1] = (float)pc((double)MEM8(q + 1) * s);
        f[2] = (float)pc((double)MEM8(q) * s);
        f[3] = (float)pc((double)MEM8(q + 3) * s);
        memcpy(out + i * 16u, f, 16);
    }
    return n * 16u;
}

static uint32_t de_al(uint32_t self, uint32_t x, uint32_t y, uint8_t *out)
{
    uint32_t n = MEM32(self + 0x1050), src = destino(self, x, y), i;
    double s = (double)MEMF(0x423A44u);
    for (i = 0; i < n; i++) {
        uint32_t q = src + i * 2u;
        float f[4];
        f[0] = f[1] = f[2] = (float)pc((double)MEM8(q) * s);
        f[3] = (float)pc((double)MEM8(q + 1) * s);
        memcpy(out + i * 16u, f, 16);
    }
    return n * 16u;
}

static uint8_t s_fila[4096 * 16];       /* a row: up to 4096 pixels x 16 bytes */
static SRWLOCK s_cand = SRWLOCK_INIT;  /* s_fila: the loader worker may convert too */

static void convertir(int fn, void (*juego)(void))
{
    uint32_t self = g_ecx, x = MEM32(g_esp + 4), y = MEM32(g_esp + 8), buf = MEM32(g_esp + 12);
    uint32_t n = MEM32(self + 0x1050), dst, len;
    uint16_t cw = g_fp_control_word;
    int probar;
    if (s_mal[fn] || n > 4096u || ((fn == F_ARGB || fn == F_AL) && MEM32(self + 0x10))) {
        juego();
        return;
    }
    probar = s_prueba[fn] > 0;
    if (probar) {
        juego();                        /* the game's, into the guest row (it returns) */
        AcquireSRWLockExclusive(&s_cand);
        dst = fn <= F_L8 ? destino(self, x, y) : buf;
        len = fn == F_RGB ? a_rgb(self, x, y, buf, s_fila) : fn == F_L8 ? a_l8(self, x, y, buf, s_fila)
            : fn == F_ARGB ? de_argb(self, x, y, s_fila) : de_al(self, x, y, s_fila);
        if (memcmp((const void *)XBOX_PTR(dst), s_fila, len)) {
            uint32_t i;
            InterlockedExchange(&s_mal[fn], 1);
            for (i = 0; i < len && MEM8(dst + i) == s_fila[i]; i++)
                ;
            fprintf(stderr, "[D3DX] %s: distinto en el byte %u de %u (juego %02X, nativo %02X): queda el del juego\n",
                    k_nom[fn], i, len, MEM8(dst + i), s_fila[i]);
        } else if (InterlockedDecrement(&s_prueba[fn]) == 0 && !s_dicho[fn]) {
            s_dicho[fn] = 1;
            fprintf(stderr, "[D3DX] %s: 64 filas iguales al juego, ahora nativo\n", k_nom[fn]);
        }
        ReleaseSRWLockExclusive(&s_cand);
        return;
    }
    /* native only */
    AcquireSRWLockExclusive(&s_cand);
    if (fn <= F_L8) {
        uint32_t guardado = (MEM32(self + 0x2C) & 0xFFFF0000u) | cw;   /* sub_003F2D7F's store */
        dst = destino(self, x, y);
        len = fn == F_RGB ? a_rgb(self, x, y, buf, s_fila) : a_l8(self, x, y, buf, s_fila);
        memcpy((void *)XBOX_PTR(dst), s_fila, len);
        MEM32(0x00677250u) = guardado;
        g_eax = guardado;
    } else {
        len = fn == F_ARGB ? de_argb(self, x, y, s_fila) : de_al(self, x, y, s_fila);
        memcpy((void *)XBOX_PTR(buf), s_fila, len);
    }
    ReleaseSRWLockExclusive(&s_cand);
    g_esp += 16;                        /* ret 0xC */
}

void sub_003F3218(void) { convertir(F_RGB, sub_003F3218_pad); }
void sub_003F3B9D(void) { convertir(F_L8, sub_003F3B9D_pad); }
void sub_003F43B6(void) { convertir(F_ARGB, sub_003F43B6_pad); }
void sub_003F4B5C(void) { convertir(F_AL, sub_003F4B5C_pad); }
