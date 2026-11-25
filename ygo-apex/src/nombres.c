/* nombres.c -- the game's shared-name table guarded and watched.
 *
 * 2026-11-22 (registro 18:17:19, the user: "se me acaba de cerrar el juego"):
 * mid-duel, a battle swapping its monsters (m924/m500), the old scene torn
 * down (sub_0019F5E0 -> sub_00187050 ...) reached sub_001B4A00, which lets a
 * shared name go: sub_00030BD0 drops its use count ([e+4]) and at 0 calls
 * sub_001AD030(pool; e), which takes it out of the sorted table
 * ([pool]: +4 count, +0x10 array of entries; an entry is [pool, uses,
 * chars...] and holders keep e+8) by binary search (sub_00188430) and frees
 * it (sub_00188700). The name was NOT in the table: the search gave
 * index == count, the shift size came out -4 bytes (ecx 0x3FFFFFFF) and the
 * copy ran off the memory: access violation, game closed. A name in use
 * that is not in the table = it was let go once too many before (its entry
 * freed and maybe reused).
 *
 * Here:
 * - sub_00030BD0: a name whose count is already 0 (or wrapped) is not
 *   dropped again: "[NOMBRES] referencia de mas" with the name and callers.
 * - sub_001AD030: the entry is looked for in the table first (the pointer,
 *   linear); not there -> not removed nor freed (the original either crashed
 *   or took out a wrong neighbour): "[NOMBRES] NO esta en la tabla" with the
 *   name, its count, the table, the callers, other entries with the same
 *   name, and every earlier removal of the same entry or name from the last
 *   8192 (so the first, extra, let-go shows up with its callers).
 * - every duel-scene end a one-line count ("[NOMBRES] ... quitados").
 */
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "recomp/gen/recomp_types.h"

extern uint32_t g_xbox_code_lo, g_xbox_code_hi;

void sub_00030BD0_pad(void);
void sub_001AD030_pad(void);

#define NRING 8192u
#define NRET 6

static struct quitado {
    uint32_t e, uses_tabla;
    ULONGLONG t;
    uint32_t ret[NRET];
    char name[48];
} s_ring[NRING];
static unsigned s_prox;
static SRWLOCK s_lock = SRWLOCK_INIT;
static volatile LONG s_quitados, s_salvados, s_de_mas;

static int legible(uint32_t a, uint32_t len)
{
    MEMORY_BASIC_INFORMATION mi;
    uintptr_t p;
    if (a < 0x10000u || a > 0xFCFFFFFFu)
        return 0;
    p = (uintptr_t)XBOX_PTR(a);
    if (!VirtualQuery((void *)p, &mi, sizeof mi) || mi.State != MEM_COMMIT
            || (mi.Protect & (PAGE_NOACCESS | PAGE_GUARD)))
        return 0;
    return p + len <= (uintptr_t)mi.BaseAddress + mi.RegionSize;
}

/* the name's chars (e+8), printable only, at most cap-1 */
static void leer_nombre(uint32_t e, char *out, size_t cap, int con_chequeo)
{
    size_t i;
    out[0] = 0;
    if (con_chequeo && !legible(e + 8, 1))
        return;
    for (i = 0; i + 1 < cap; i++) {
        uint8_t c;
        if (con_chequeo && ((e + 8 + i) & 0xFFFu) == 0 && !legible(e + 8 + (uint32_t)i, 1))
            break;
        c = MEM8(e + 8 + (uint32_t)i);
        if (!c)
            break;
        out[i] = (c >= 0x20 && c < 0x7F) ? (char)c : '?';
    }
    out[i] = 0;
}

/* the guest return addresses above esp, innermost first */
static int pila(uint32_t sp, uint32_t *ret, int max)
{
    int i, n = 0;
    for (i = 0; i < 256 && n < max; i++) {
        uint32_t v = MEM32(sp + (uint32_t)i * 4u);
        if (v > g_xbox_code_lo && v < g_xbox_code_hi)
            ret[n++] = v;
    }
    return n;
}

static void imprimir_pila(const char *pre, const uint32_t *r, int n)
{
    char b[16 * 12 + 8];
    int i, k = 0;
    b[0] = 0;
    for (i = 0; i < n; i++)
        k += snprintf(b + k, sizeof b - (size_t)k, " %08X", r[i]);
    fprintf(stderr, "%s%s\n", pre, b);
}

/* 1: e is in the table, 0: it is not, -1: the table looks wrong. First the
 * game's own way (binary search by the chars, unsigned bytes like its
 * compare), then, if that does not land on e, every slot. Only what the
 * original reads anyway is read (no system calls on this path). */
static int en_tabla(uint32_t t, uint32_t e, uint32_t *n_out)
{
    uint32_t n = MEM32(t + 4), arr = MEM32(t + 0x10), i, lo = 0, hi;
    const char *k = (const char *)XBOX_PTR(e + 8);
    *n_out = n;
    if (n > 0x100000u || (n && arr < 0x10000u))
        return -1;
    hi = n;
    while (lo < hi) {
        uint32_t m = lo + (hi - lo) / 2u, o = MEM32(arr + m * 4u);
        int c;
        if (o == e)
            return 1;
        if (o < 0x10000u)
            break;
        c = strcmp(k, (const char *)XBOX_PTR(o + 8));
        if (c == 0)
            break;
        if (c < 0)
            hi = m;
        else
            lo = m + 1u;
    }
    for (i = 0; i < n; i++)
        if (MEM32(arr + i * 4u) == e)
            return 1;
    return 0;
}

static void anotar(uint32_t e, uint32_t n, const uint32_t *r)
{
    struct quitado *q;
    AcquireSRWLockExclusive(&s_lock);
    q = &s_ring[s_prox++ % NRING];
    q->e = e;
    q->uses_tabla = n;
    q->t = GetTickCount64();
    memcpy(q->ret, r, sizeof q->ret);
    leer_nombre(e, q->name, sizeof q->name, 0);
    ReleaseSRWLockExclusive(&s_lock);
}

static void historia(uint32_t e, const char *name)
{
    ULONGLONG now = GetTickCount64();
    unsigned i, hay = 0, total;
    AcquireSRWLockShared(&s_lock);
    total = s_prox < NRING ? s_prox : NRING;
    for (i = 0; i < total; i++) {
        const struct quitado *q = &s_ring[(s_prox - 1u - i) % NRING];
        if (q->e != e && (!name[0] || strcmp(q->name, name)))
            continue;
        fprintf(stderr, "[NOMBRES]   antes: hace %llu ms se quito %08X \"%s\" (%s; tabla %u)",
                (unsigned long long)(now - q->t), q->e, q->name,
                q->e == e ? "misma entrada" : "mismo nombre", q->uses_tabla);
        imprimir_pila(", llamado desde", q->ret, NRET);
        if (++hay >= 12)
            break;
    }
    ReleaseSRWLockShared(&s_lock);
    if (!hay)
        fprintf(stderr, "[NOMBRES]   no se quito antes (en los ultimos %u)\n", total);
}

static void mismos_nombres(uint32_t t, const char *name, uint32_t n)
{
    uint32_t arr = MEM32(t + 0x10), i;
    char otro[48];
    int k = 0;
    if (!name[0] || n > 0x100000u || (n && !legible(arr, n * 4u)))
        return;
    for (i = 0; i < n && k < 4; i++) {
        uint32_t o = MEM32(arr + i * 4u);
        if (!legible(o, 16))
            continue;
        leer_nombre(o, otro, sizeof otro, 1);
        if (!strcmp(otro, name)) {
            fprintf(stderr, "[NOMBRES]   en la tabla hay otra entrada con ese nombre: %08X (usos %u), posicion %u\n",
                    o, MEM32(o + 4), i);
            k++;
        }
    }
}

/* ecx = entry; no args; ret */
void sub_00030BD0(void)
{
    uint32_t e = g_ecx, u;
    if (e >= 0x10000u) {                 /* the original reads it anyway */
        u = MEM32(e + 4);
        if (u == 0 || u >= 0x80000000u) {
            uint32_t r[16];
            char name[64];
            int n = pila(g_esp, r, 16);
            leer_nombre(e, name, sizeof name, 1);
            InterlockedIncrement(&s_de_mas);
            fprintf(stderr, "[NOMBRES] referencia de mas: %08X \"%s\" ya tenia %u usos; no se baja (vez %ld)\n",
                    e, name, u, s_de_mas);
            imprimir_pila("[NOMBRES]   pila:", r, n);
            historia(e, name);
            g_esp += 4;                  /* ret */
            return;
        }
    }
    sub_00030BD0_pad();
}

/* ecx = pool; [esp+4] = entry; ret 4 */
void sub_001AD030(void)
{
    uint32_t pool = g_ecx, e = MEM32(g_esp + 4), t = 0, n = 0;
    uint32_t r[NRET];
    int st = -1;
    if (pool >= 0x10000u && (t = MEM32(pool)) >= 0x10000u && e >= 0x10000u) {
        st = en_tabla(t, e, &n);
        if (st == 0)
            st = en_tabla(t, e, &n);    /* once more: another thread may be shifting it */
    }
    if (st == 0) {
        uint32_t rr[16];
        char name[64];
        int k = pila(g_esp, rr, 16);
        leer_nombre(e, name, sizeof name, 1);
        InterlockedIncrement(&s_salvados);
        fprintf(stderr, "[NOMBRES] NO esta en la tabla: %08X \"%s\" (usos %u), tabla %08X con %u; "
                "no se quita ni se libera (antes: cierre del juego) (vez %ld)\n",
                e, name, legible(e, 8) ? MEM32(e + 4) : 0xFFFFFFFFu, t, n, s_salvados);
        imprimir_pila("[NOMBRES]   pila:", rr, k);
        mismos_nombres(t, name, n);
        historia(e, name);
        g_esp += 8;                      /* ret 4 */
        return;
    }
    memset(r, 0, sizeof r);
    pila(g_esp, r, NRET);
    anotar(e, n, r);
    InterlockedIncrement(&s_quitados);
    sub_001AD030_pad();
}

/* src/mascota.c calls it at each duel scene's end */
void nombres_resumen(void)
{
    fprintf(stderr, "[NOMBRES] %ld nombres quitados, %ld salvados (no estaban), %ld referencias de mas\n",
            s_quitados, s_salvados, s_de_mas);
}
